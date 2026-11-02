#include "i18n.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Copy whole UTF-8 code points, including on the truncation boundary. */
static bool append(char *out, size_t size, size_t *used, const char *src, size_t len)
{
    size_t pos = 0;
    while (pos < len) {
        unsigned char c = (unsigned char)src[pos];
        size_t width = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        if (pos + width > len || *used + width >= size) return false;
        memcpy(out + *used, src + pos, width);
        *used += width;
        pos += width;
        out[*used] = '\0';
    }
    return true;
}

void i18n_copy(char *output, size_t size, const char *text)
{
    if (!output || !size) return;
    output[0] = '\0';
    size_t used = 0;
    if (text) append(output, size, &used, text, strlen(text));
}

bool i18n_format(char *output, size_t size, const char *text,
                 const i18n_arg_t *args, size_t count)
{
    if (!output || !size) return false;
    output[0] = '\0';
    if (!text || (count && !args)) return false;
    size_t used = 0;
    while (*text) {
        const char *open = strchr(text, '{');
        size_t prefix = open ? (size_t)(open - text) : strlen(text);
        if (!append(output, size, &used, text, prefix)) return false;
        if (!open) return true;
        const char *close = strchr(open + 1, '}');
        if (!close) return false;
        const char *value = NULL;
        for (size_t i = 0; i < count; ++i) {
            if (args[i].name && strlen(args[i].name) == (size_t)(close - open - 1) &&
                strncmp(args[i].name, open + 1, (size_t)(close - open - 1)) == 0) {
                value = args[i].value;
                break;
            }
        }
        if (!value || !append(output, size, &used, value, strlen(value))) return false;
        text = close + 1;
    }
    return true;
}

void i18n_format_relative(char *output, size_t size, const char *unit,
                          int value, bool include_tense)
{
    if (!output || !size) return;
    if (!unit) { output[0] = '\0'; return; }
    char key[80];
    if (include_tense && value >= -1 && value <= 1) {
        snprintf(key, sizeof(key), "time.%s.%s", unit,
                 value == 0 ? "zero" : value < 0 ? "previous" : "next");
        const char *special = i18n_find(key);
        if (special) { i18n_copy(output, size, special); return; }
    }
    unsigned magnitude = value < 0 ? (unsigned)(-(int64_t)value) : (unsigned)value;
    snprintf(key, sizeof(key), "time.%s.%s.%s", unit,
             !include_tense ? "elapsed" : value < 0 ? "past" : "future",
             i18n_plural_category(magnitude));
    char number[24];
    snprintf(number, sizeof(number), "%u", magnitude);
    const i18n_arg_t args[] = {{"count", number}};
    i18n_format(output, size, i18n_find(key), args, 1);
}

bool i18n_format_date(char *output, size_t size, unsigned year,
                      unsigned month, unsigned day, bool today)
{
    if (!output || !size) return false;
    output[0] = '\0';
    if (month < 1 || month > 12 || day < 1 || day > 31) return false;
    char key[32], year_text[16], day_text[16], date[160];
    snprintf(key, sizeof(key), "date.month_%u", month);
    snprintf(year_text, sizeof(year_text), "%u", year);
    snprintf(day_text, sizeof(day_text), "%u", day);
    const i18n_arg_t args[] = {{"month", i18n_find(key)}, {"day", day_text}, {"year", year_text}};
    if (!i18n_format(date, sizeof(date), i18n_get(I18N_DATE_FULL), args, 3)) return false;
    if (today) {
        const i18n_arg_t today_args[] = {{"date", date}};
        return i18n_format(output, size, i18n_get(I18N_DATE_TODAY), today_args, 1);
    }
    return i18n_format(output, size, "{date}", (i18n_arg_t[]){{"date", date}}, 1);
}

bool i18n_format_datetime(char *output, size_t size, unsigned year,
                          unsigned month, unsigned day, unsigned hour,
                          unsigned minute)
{
    if (!output || !size) return false;
    output[0] = '\0';
    char date[160], time[16];
    if (hour > 23 || minute > 59 || !i18n_format_date(date, sizeof(date), year, month, day, false)) return false;
    snprintf(time, sizeof(time), "%02u:%02u", hour, minute);
    const i18n_arg_t args[] = {{"date", date}, {"time", time}};
    return i18n_format(output, size, i18n_get(I18N_DATE_TIME), args, 2);
}
