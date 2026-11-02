#include "i18n.h"

#include <stdatomic.h>
#include <string.h>
#include <strings.h>
#include "i18n_data.inc"

static atomic_uint s_locale = 0;
static atomic_uint s_revision = 0;

size_t i18n_locale_count(void)
{
    return sizeof(s_locales) / sizeof(s_locales[0]);
}

const i18n_locale_t *i18n_locale_at(size_t index)
{
    return index < i18n_locale_count() ? &s_locales[index] : NULL;
}

static unsigned locale_index(const char *code)
{
    if (!code || !*code) return 0;
    for (unsigned i = 0; i < i18n_locale_count(); ++i) {
        if (strcasecmp(code, s_locales[i].code) == 0) return i;
    }
    size_t base = strcspn(code, "-_");
    if (code[base]) {
        for (unsigned i = 0; i < i18n_locale_count(); ++i) {
            if (strlen(s_locales[i].code) == base &&
                strncasecmp(code, s_locales[i].code, base) == 0) return i;
        }
    }
    return 0;
}

const char *i18n_normalize_locale(const char *code)
{
    return s_locales[locale_index(code)].code;
}

bool i18n_set_locale(const char *code)
{
    unsigned next = locale_index(code);
    bool changed = atomic_exchange(&s_locale, next) != next;
    if (changed) atomic_fetch_add(&s_revision, 1);
    return changed;
}

const char *i18n_get_locale(void)
{
    return s_locales[atomic_load(&s_locale)].code;
}

const char *i18n_plural_category(unsigned count)
{
    return s_locales[atomic_load(&s_locale)].singular_one && count == 1 ? "one" : "other";
}

const char *i18n_get(i18n_key_t key)
{
    if ((unsigned)key >= I18N_KEY_COUNT) return "";
    const char *value = s_messages[atomic_load(&s_locale)][key];
    return value ? value : s_messages[0][key];
}

const char *i18n_find(const char *key)
{
    if (!key) return NULL;
    size_t lo = 0, hi = I18N_KEY_COUNT;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int order = strcmp(key, s_key_names[mid]);
        if (!order) return i18n_get((i18n_key_t)mid);
        if (order < 0) hi = mid;
        else lo = mid + 1;
    }
    return NULL;
}

bool i18n_resolve_key(const char *text, i18n_key_t *key)
{
    if (!text || !key) return false;
    /* Identity, not contents: user-provided names must never become keys.
     * Generated messages are distinct named arrays, even for equal text. */
    for (size_t locale = 0; locale < i18n_locale_count(); ++locale) {
        for (unsigned i = 0; i < I18N_KEY_COUNT; ++i) {
            if (text == s_messages[locale][i]) {
                *key = (i18n_key_t)i;
                return true;
            }
        }
    }
    return false;
}

uint32_t i18n_get_revision(void)
{
    return atomic_load(&s_revision);
}
