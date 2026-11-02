#!/usr/bin/env python3
"""Validate locale resources and generate immutable C tables (no compiler required)."""
import argparse
import json
import re
import unicodedata
from pathlib import Path

PRINTF = re.compile(r'%(?:[-+ #0]*)(?:\d+)?(?:\.\d+)?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcsp%]')
NAMED = re.compile(r'\{([a-z][a-z0-9_]*)\}')


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'{path}: duplicate key {key}')
            result[key] = value
        return result
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique)


def signature(text):
    # Preserve full specifiers (including precision), argument order and types.
    # Reject unsupported printf syntax, %n, positional args and dynamic widths.
    tokens = PRINTF.findall(text)
    if '%' in PRINTF.sub('', text):
        raise ValueError(f'invalid printf format: {text!r}')
    named = sorted(NAMED.findall(text))
    rest = NAMED.sub('', text)
    if '{' in rest or '}' in rest or (named and tokens):
        raise ValueError(f'invalid or mixed template: {text!r}')
    return tokens, named


def symbol(key):
    return 'I18N_' + key.upper().replace('.', '_')


def generate(source, output=None):
    manifest = load(source / 'manifest.json')
    languages = manifest['languages']
    if manifest['default'] != 'en' or not languages or languages[0]['code'] != 'en':
        raise ValueError('English must be the first and default locale')
    codes = set()
    catalogs = []
    for lang in languages:
        code = lang['code']
        if not re.fullmatch(r'[a-z]{2,3}(?:-[A-Za-z0-9]{2,8})*', code) or len(code) >= 32 or code.lower() in codes:
            raise ValueError(f'invalid/duplicate language code {code}')
        codes.add(code.lower())
        if not isinstance(lang.get('name'), str) or not lang['name'] or '\0' in lang['name']:
            raise ValueError(f'{code}: invalid language name')
        if lang['plural_rule'] not in ('one_other', 'other'):
            raise ValueError(f'unsupported plural rule: {lang["plural_rule"]}')
        if Path(lang['file']).name != lang['file']:
            raise ValueError('locale files must be inside locales/')
        catalog = load(source / lang['file'])
        if not isinstance(catalog, dict):
            raise ValueError('catalog must be a JSON object')
        catalog = {key: unicodedata.normalize('NFC', text) if isinstance(text, str) else text
                   for key, text in catalog.items()}
        for key, text in catalog.items():
            if not re.fullmatch(r'[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)*', key) or not isinstance(text, str) or not text or '\0' in text:
                raise ValueError(f'{code}: invalid key or text {key}')
            signature(text)
        catalogs.append(catalog)
    english = catalogs[0]
    keys = sorted(english)
    if len({symbol(k) for k in keys}) != len(keys):
        raise ValueError('key names collide as C identifiers')
    for lang, catalog in zip(languages, catalogs):
        extra = catalog.keys() - english.keys()
        if extra:
            raise ValueError(f'{lang["code"]}: unknown keys {sorted(extra)}')
        for key, text in catalog.items():
            if signature(text) != signature(english[key]):
                raise ValueError(f'{lang["code"]}: placeholder mismatch for {key}')
        missing = english.keys() - catalog.keys()
        if missing:
            print(f'{lang["code"]}: {len(missing)} keys fall back to English')
    if output:
        output.mkdir(parents=True, exist_ok=True)
        quote = lambda value: json.dumps(value, ensure_ascii=False)
        header = '#pragma once\n\n/* Generated; edit the JSON files in locales instead. */\ntypedef enum {\n'
        header += ''.join(f'    {symbol(k)},\n' for k in keys)
        header += '    I18N_KEY_COUNT\n} i18n_key_t;\n'
        (output / 'i18n_keys.h').write_text(header, encoding='utf-8')
        data = '/* Generated; included by i18n.c. */\n'
        data += 'static const char *const s_key_names[I18N_KEY_COUNT] = {\n' + ''.join(f'    {quote(k)},\n' for k in keys) + '};\n'
        data += 'static const i18n_locale_t s_locales[] = {\n'
        data += ''.join('    {' + quote(l['code']) + ', ' + quote(unicodedata.normalize('NFC', l['name'])) + ', ' + ('true' if l['plural_rule'] == 'one_other' else 'false') + '},\n' for l in languages) + '};\n'
        for lang_index, catalog in enumerate(catalogs):
            for key_index, key in enumerate(keys):
                if key in catalog:
                    data += f'static const char s_text_{lang_index}_{key_index}[] = {quote(catalog[key])};\n'
        data += 'static const char *const s_messages[][I18N_KEY_COUNT] = {\n'
        for lang_index, catalog in enumerate(catalogs):
            data += '    {\n'
            for key_index, key in enumerate(keys):
                reference = f's_text_{lang_index}_{key_index}' if key in catalog else 'NULL'
                data += f'        {reference},\n'
            data += '    },\n'
        data += '};\n'
        (output / 'i18n_data.inc').write_text(data, encoding='utf-8')
    print(f'Validated {len(languages)} locales, {len(keys)} message keys')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1] / 'locales')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    generate(args.source, args.output)
