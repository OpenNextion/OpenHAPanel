"""Resource validation tests only; does not compile firmware or host C code."""
import contextlib
import io
import json
import re
from pathlib import Path
import shutil
import tempfile
import unittest

from generate_i18n import generate, load, signature


class ResourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'locales'
        shutil.copytree(Path(__file__).resolve().parents[1] / 'locales', self.source)

    def write_locale(self, code, messages):
        (self.source / f'{code}.json').write_text(json.dumps(messages, ensure_ascii=False), encoding='utf-8')

    def run_generator(self, output=None):
        with contextlib.redirect_stdout(io.StringIO()):
            generate(self.source, output)

    def test_shipped_catalogs_complete_and_deterministic(self):
        self.assertEqual(set(load(self.source / 'en.json')), set(load(self.source / 'de.json')))
        self.run_generator(self.root / 'first')
        self.run_generator(self.root / 'second')
        for name in ['i18n_keys.h', 'i18n_data.inc']:
            self.assertEqual((self.root / 'first' / name).read_bytes(), (self.root / 'second' / name).read_bytes())
            generated = (self.root / 'first' / name).read_text(encoding='utf-8')
            # GCC -Wcomment rejects nested openers even inside a path glob.
            for comment in re.findall(r'/\*(.*?)\*/', generated, re.S):
                self.assertNotIn('/*', comment, name)
        data = (self.root / 'first' / 'i18n_data.inc').read_text(encoding='utf-8')
        self.assertIn('Geräte-Hotspot', data)
        self.assertIn('static const char s_text_', data)

    def test_decomposed_translation_is_emitted_precomposed(self):
        messages = load(self.source / 'vi.json')
        messages['climate.auto'] = 'o\u0302\u0323'
        self.write_locale('vi', messages)
        self.run_generator(self.root / 'generated')
        generated = (self.root / 'generated' / 'i18n_data.inc').read_text(encoding='utf-8')
        self.assertIn('"\u1ed9"', generated)
        self.assertNotIn('"o\u0302\u0323"', generated)

    def test_missing_translation_emits_fallback_slot(self):
        messages = load(self.source / 'de.json')
        messages.pop('common.cancel')
        self.write_locale('de', messages)
        self.run_generator(self.root / 'generated')
        self.assertIn('        NULL,', (self.root / 'generated' / 'i18n_data.inc').read_text(encoding='utf-8'))

    def test_printf_argument_contract(self):
        messages = load(self.source / 'de.json')
        messages['password.confirm'] = 'Noch %s Sekunden'
        self.write_locale('de', messages)
        with self.assertRaisesRegex(ValueError, 'placeholder mismatch'):
            self.run_generator()

    def test_named_arguments_allow_reordering_but_not_renaming(self):
        self.assertEqual(signature('{month} {day}'), signature('{day}. {month}'))
        messages = load(self.source / 'de.json')
        messages['date.full'] = '{month} {day}, {wrong_year}'
        self.write_locale('de', messages)
        with self.assertRaisesRegex(ValueError, 'placeholder mismatch'):
            self.run_generator()

    def test_unsafe_or_unsupported_formats_rejected(self):
        for text in ['%n', '%*s', '%1$s', '%s {name}', '{broken']:
            with self.subTest(text=text), self.assertRaises(ValueError):
                signature(text)

    def test_duplicate_keys_rejected(self):
        (self.source / 'de.json').write_text('{"common.cancel":"A","common.cancel":"B"}')
        with self.assertRaisesRegex(ValueError, 'duplicate key'):
            self.run_generator()

    def test_unknown_key_rejected(self):
        messages = load(self.source / 'de.json')
        messages['typo.cancel'] = 'Abbrechen'
        self.write_locale('de', messages)
        with self.assertRaisesRegex(ValueError, 'unknown keys'):
            self.run_generator()

    def test_c_identifier_collision_rejected(self):
        messages = load(self.source / 'en.json')
        messages.update({'a.b_c': 'First', 'a_b.c': 'Second'})
        self.write_locale('en', messages)
        with self.assertRaisesRegex(ValueError, 'collide'):
            self.run_generator()

    def test_new_locale_can_reuse_english_fallback(self):
        manifest = load(self.source / 'manifest.json')
        manifest['languages'].append({'code': 'sv', 'name': 'Svenska', 'file': 'sv.json', 'plural_rule': 'one_other'})
        (self.source / 'manifest.json').write_text(json.dumps(manifest))
        self.write_locale('sv', {'common.cancel': 'Avbryt'})
        self.run_generator(self.root / 'generated')
        self.assertIn('"sv", "Svenska"', (self.root / 'generated' / 'i18n_data.inc').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
