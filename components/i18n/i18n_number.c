#include "i18n.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define I18N_NUMBER_CANONICAL_MAX 192
#define I18N_NUMBER_MAX_FRACTION_DIGITS 100

static bool append_bytes(char *output, size_t size, size_t *used,
                         const char *text, size_t length)
{
    if (!output || !size || !used || !text || length >= size - *used) {
        return false;
    }
    memcpy(output + *used, text, length);
    *used += length;
    output[*used] = '\0';
    return true;
}

static bool append_char(char *output, size_t size, size_t *used, char value)
{
    return append_bytes(output, size, used, &value, 1);
}

/* Expand a finite canonical decimal, including scientific notation, without
 * passing it through binary floating point. This preserves HA state digits. */
static bool expand_canonical_number(const char *number,
                                    char *output,
                                    size_t size)
{
    if (!number || !number[0] || !output || !size) {
        return false;
    }

    const char *cursor = number;
    const bool negative = *cursor == '-';
    if (*cursor == '-' || *cursor == '+') {
        cursor++;
    }

    char digits[I18N_NUMBER_CANONICAL_MAX];
    size_t digit_count = 0;
    size_t integer_digits = 0;
    bool decimal_seen = false;
    while (*cursor && *cursor != 'e' && *cursor != 'E') {
        if (*cursor == '.') {
            if (decimal_seen) {
                return false;
            }
            decimal_seen = true;
            integer_digits = digit_count;
        }
        else if (*cursor >= '0' && *cursor <= '9') {
            if (digit_count + 1 >= sizeof(digits)) {
                return false;
            }
            digits[digit_count++] = *cursor;
        }
        else {
            return false;
        }
        cursor++;
    }
    if (!decimal_seen) {
        integer_digits = digit_count;
    }
    if (digit_count == 0) {
        return false;
    }

    long exponent = 0;
    if (*cursor == 'e' || *cursor == 'E') {
        char *end = NULL;
        exponent = strtol(cursor + 1, &end, 10);
        if (!end || end == cursor + 1 || *end != '\0' ||
            exponent < -1000 || exponent > 1000) {
            return false;
        }
    }
    else if (*cursor != '\0') {
        return false;
    }

    const long point = (long)integer_digits + exponent;
    size_t required = negative ? 1 : 0;
    if (point <= 0) {
        required += 2 + (size_t)(-point) + digit_count;
    }
    else if (point >= (long)digit_count) {
        required += (size_t)point;
    }
    else {
        required += digit_count + 1;
    }
    if (required >= size) {
        return false;
    }

    char *out = output;
    if (negative) {
        *out++ = '-';
    }
    if (point <= 0) {
        *out++ = '0';
        *out++ = '.';
        for (long i = 0; i < -point; i++) {
            *out++ = '0';
        }
        memcpy(out, digits, digit_count);
        out += digit_count;
    }
    else if (point >= (long)digit_count) {
        memcpy(out, digits, digit_count);
        out += digit_count;
        for (long i = (long)digit_count; i < point; i++) {
            *out++ = '0';
        }
    }
    else {
        memcpy(out, digits, (size_t)point);
        out += point;
        *out++ = '.';
        memcpy(out, digits + point, digit_count - (size_t)point);
        out += digit_count - (size_t)point;
    }
    *out = '\0';
    return true;
}

static bool prepare_canonical_number(const char *number,
                                     const i18n_number_options_t *options,
                                     char *output,
                                     size_t size)
{
    if (options->fraction_mode == I18N_NUMBER_FRACTION_PRESERVE) {
        return expand_canonical_number(number, output, size);
    }
    if (options->fraction_mode != I18N_NUMBER_FRACTION_FIXED &&
        options->fraction_mode != I18N_NUMBER_FRACTION_MAXIMUM) {
        return false;
    }
    if (options->fraction_digits > I18N_NUMBER_MAX_FRACTION_DIGITS) {
        return false;
    }

    char *end = NULL;
    const double value = strtod(number, &end);
    if (!end || end == number || *end != '\0' || !isfinite(value)) {
        return false;
    }
    const int written = snprintf(output, size, "%.*f",
                                 options->fraction_digits, value);
    if (written < 0 || (size_t)written >= size) {
        return false;
    }
    if (options->fraction_mode == I18N_NUMBER_FRACTION_MAXIMUM) {
        size_t length = (size_t)written;
        const char *decimal = strchr(output, '.');
        if (decimal) {
            const size_t decimal_index = (size_t)(decimal - output);
            while (length > decimal_index + 1 && output[length - 1] == '0') {
                output[--length] = '\0';
            }
            if (length == decimal_index + 1) {
                output[decimal_index] = '\0';
            }
        }
        if (strcmp(output, "-0") == 0) {
            output[0] = '0';
            output[1] = '\0';
        }
    }
    return true;
}

bool i18n_format_number(char *output, size_t size, const char *number,
                        const i18n_number_options_t *options)
{
    if (!output || !size) {
        return false;
    }
    output[0] = '\0';
    const i18n_number_options_t defaults = I18N_NUMBER_OPTIONS_PRESERVE;
    const i18n_number_options_t *format = options ? options : &defaults;
    const i18n_locale_t *locale = i18n_current_locale();
    if (!locale || !locale->number_group_separator ||
        !locale->number_decimal_separator || locale->number_group_size == 0) {
        return false;
    }

    char canonical[I18N_NUMBER_CANONICAL_MAX];
    if (!prepare_canonical_number(number, format,
                                  canonical, sizeof(canonical))) {
        return false;
    }

    const char *cursor = canonical;
    const bool negative = *cursor == '-';
    if (negative) {
        cursor++;
    }
    const char *decimal = strchr(cursor, '.');
    const char *integer_end = decimal ? decimal : cursor + strlen(cursor);
    const char *integer = cursor;
    while (integer + 1 < integer_end && *integer == '0') {
        integer++;
    }
    const size_t integer_length = (size_t)(integer_end - integer);
    const char *fraction = decimal ? decimal + 1 : NULL;
    const size_t fraction_length = fraction ? strlen(fraction) : 0;

    size_t used = 0;
    if (negative && !append_char(output, size, &used, '-')) {
        output[0] = '\0';
        return false;
    }
    for (size_t i = 0; i < integer_length; i++) {
        if (format->use_grouping && i > 0 &&
            (integer_length - i) % locale->number_group_size == 0 &&
            !append_bytes(output, size, &used,
                          locale->number_group_separator,
                          strlen(locale->number_group_separator))) {
            output[0] = '\0';
            return false;
        }
        if (!append_char(output, size, &used, integer[i])) {
            output[0] = '\0';
            return false;
        }
    }
    if (fraction_length > 0) {
        if (!append_bytes(output, size, &used,
                          locale->number_decimal_separator,
                          strlen(locale->number_decimal_separator)) ||
            !append_bytes(output, size, &used, fraction, fraction_length)) {
            output[0] = '\0';
            return false;
        }
    }
    return true;
}

bool i18n_format_number_double(char *output, size_t size, double number,
                               const i18n_number_options_t *options)
{
    if (!output || !size || !isfinite(number)) {
        if (output && size) {
            output[0] = '\0';
        }
        return false;
    }
    const i18n_number_options_t defaults = {
        true, I18N_NUMBER_FRACTION_MAXIMUM, 2
    };
    const i18n_number_options_t *format = options ? options : &defaults;
    if (format->fraction_mode == I18N_NUMBER_FRACTION_PRESERVE) {
        output[0] = '\0';
        return false;
    }
    char canonical[48];
    const int written = snprintf(canonical, sizeof(canonical), "%.17g", number);
    if (written < 0 || (size_t)written >= sizeof(canonical)) {
        output[0] = '\0';
        return false;
    }
    return i18n_format_number(output, size, canonical, format);
}
