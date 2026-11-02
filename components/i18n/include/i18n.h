#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "i18n_keys.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I18N_LOCALE_MAX 32

typedef struct {
    const char *code;
    const char *name;
    bool singular_one;
} i18n_locale_t;

typedef struct {
    const char *name;
    const char *value;
} i18n_arg_t;

/* Immutable strings have firmware lifetime. No caller frees the result. */
const char *i18n_get(i18n_key_t key);
/* NULL for an unknown key; a missing translation falls back to English. */
const char *i18n_find(const char *key);
/* Resolve only immutable catalog pointers, never arbitrary equal strings. */
bool i18n_resolve_key(const char *text, i18n_key_t *key);
size_t i18n_locale_count(void);
const i18n_locale_t *i18n_locale_at(size_t index);
/* Case-insensitive exact match, then base language, otherwise English. */
const char *i18n_normalize_locale(const char *code);
/* Thread-safe selection, no LVGL or persistence dependency. */
bool i18n_set_locale(const char *code);
const char *i18n_get_locale(void);
/* Changes even when multiple switches return to the previous language. */
uint32_t i18n_get_revision(void);
const char *i18n_plural_category(unsigned count);

/* Named arguments can be reordered. False means missing arguments/truncation.
 * output is always terminated; a truncated UTF-8 sequence is never emitted. */
bool i18n_format(char *output, size_t size, const char *text,
                 const i18n_arg_t *args, size_t count);
void i18n_copy(char *output, size_t size, const char *text);
/* Normalize supported combining marks for the selected device language. */
void i18n_display_copy(char *output, size_t size, const char *text);
void i18n_format_relative(char *output, size_t size, const char *unit,
                          int value, bool include_tense);
bool i18n_format_date(char *output, size_t size, unsigned year,
                      unsigned month, unsigned day, bool today);
bool i18n_format_datetime(char *output, size_t size, unsigned year,
                          unsigned month, unsigned day, unsigned hour,
                          unsigned minute);

#ifdef __cplusplus
}
#endif
