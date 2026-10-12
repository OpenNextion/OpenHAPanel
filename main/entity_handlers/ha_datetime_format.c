#include "ha_datetime_format.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sys_time_api.h"

#define HA_DATETIME_MIN_VALID_EPOCH 1577836800

typedef struct {
    int year;
    unsigned month;
    unsigned day;
    unsigned hour;
    unsigned minute;
    unsigned second;
} ha_datetime_parts_t;

static bool ha_datetime_is_missing(const char *value, size_t length);
static bool ha_datetime_parse_parts(const char *raw_value,
                                    time_t *epoch_out,
                                    ha_datetime_parts_t *parts_out);
static bool ha_datetime_parse_local_epoch(time_t epoch,
                                          ha_datetime_parts_t *parts_out);
static bool ha_datetime_parse_digits(const char *text,
                                     size_t count,
                                     unsigned *value_out);
static unsigned ha_datetime_days_in_month(int year, unsigned month);
static int64_t ha_datetime_civil_day_number(int year,
                                            unsigned month,
                                            unsigned day);
static int64_t ha_datetime_floor_mod(int64_t value, int64_t divisor);
static int ha_datetime_js_round(double value);
static void ha_datetime_format_english(int value,
                                       const char *unit,
                                       char *output,
                                       size_t output_size);
static esp_err_t ha_datetime_write_error(const char *text,
                                         esp_err_t error,
                                         char *output,
                                         size_t output_size);

esp_err_t ha_datetime_format_relative(const char *raw_value,
                                      time_t now_epoch,
                                      char *output,
                                      size_t output_size)
{
    if (!raw_value || !output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    output[0] = '\0';
    const char *value = raw_value;
    size_t value_length = strlen(value);
    if (value_length >= 2 && value[0] == '"' &&
        value[value_length - 1] == '"') {
        value++;
        value_length -= 2;
    }
    if (ha_datetime_is_missing(value, value_length)) {
        snprintf(output, output_size, "Never");
        return ESP_OK;
    }
    if (now_epoch < HA_DATETIME_MIN_VALID_EPOCH) {
        return ha_datetime_write_error("Invalid date",
                                       ESP_ERR_INVALID_STATE,
                                       output,
                                       output_size);
    }

    time_t from_epoch = 0;
    if (ha_datetime_parse_epoch(raw_value, &from_epoch) != ESP_OK) {
        return ha_datetime_write_error("Invalid date",
                                       ESP_ERR_INVALID_ARG,
                                       output,
                                       output_size);
    }

    return ha_datetime_format_relative_epoch(from_epoch,
                                             now_epoch,
                                             output,
                                             output_size);
}

esp_err_t ha_datetime_format_relative_epoch(time_t from_epoch,
                                            time_t now_epoch,
                                            char *output,
                                            size_t output_size)
{
    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    output[0] = '\0';
    if (now_epoch < HA_DATETIME_MIN_VALID_EPOCH) {
        return ha_datetime_write_error("Invalid date",
                                       ESP_ERR_INVALID_STATE,
                                       output,
                                       output_size);
    }

    ha_datetime_parts_t from = {0};
    ha_datetime_parts_t to = {0};
    if (!ha_datetime_parse_local_epoch(from_epoch, &from) ||
        !ha_datetime_parse_local_epoch(now_epoch, &to)) {
        return ha_datetime_write_error("Invalid date",
                                       ESP_FAIL,
                                       output,
                                       output_size);
    }

    const double seconds = difftime(from_epoch, now_epoch);
    if (fabs(seconds) < 59.0) {
        ha_datetime_format_english(ha_datetime_js_round(seconds),
                                   "second",
                                   output,
                                   output_size);
        return ESP_OK;
    }

    const double minutes = seconds / 60.0;
    if (fabs(minutes) < 59.0) {
        ha_datetime_format_english(ha_datetime_js_round(minutes),
                                   "minute",
                                   output,
                                   output_size);
        return ESP_OK;
    }

    const double hours = minutes / 60.0;
    if (fabs(hours) < 22.0) {
        ha_datetime_format_english(ha_datetime_js_round(hours),
                                   "hour",
                                   output,
                                   output_size);
        return ESP_OK;
    }

    const int64_t from_day = ha_datetime_civil_day_number(from.year,
                                                           from.month,
                                                           from.day);
    const int64_t to_day = ha_datetime_civil_day_number(to.year,
                                                         to.month,
                                                         to.day);
    const int days = (int)(from_day - to_day);
    if (days == 0) {
        ha_datetime_format_english(ha_datetime_js_round(hours),
                                   "hour",
                                   output,
                                   output_size);
        return ESP_OK;
    }
    if (days > -5 && days < 5) {
        ha_datetime_format_english(days, "day", output, output_size);
        return ESP_OK;
    }

    const int64_t from_week = from_day -
                              ha_datetime_floor_mod(from_day + 4, 7);
    const int64_t to_week = to_day -
                            ha_datetime_floor_mod(to_day + 4, 7);
    const int weeks = (int)((from_week - to_week) / 7);
    if (weeks == 0) {
        ha_datetime_format_english(days, "day", output, output_size);
        return ESP_OK;
    }
    if (weeks > -4 && weeks < 4) {
        ha_datetime_format_english(weeks, "week", output, output_size);
        return ESP_OK;
    }

    const int years = from.year - to.year;
    const int months = years * 12 + (int)from.month - (int)to.month;
    if (months == 0) {
        ha_datetime_format_english(weeks, "week", output, output_size);
    }
    else if ((months > -11 && months < 11) || years == 0) {
        ha_datetime_format_english(months, "month", output, output_size);
    }
    else {
        ha_datetime_format_english(years, "year", output, output_size);
    }
    return ESP_OK;
}

esp_err_t ha_datetime_format_relative_now(const char *raw_value,
                                          char *output,
                                          size_t output_size)
{
    return ha_datetime_format_relative(raw_value,
                                       time(NULL),
                                       output,
                                       output_size);
}

esp_err_t ha_datetime_parse_epoch(const char *raw_value,
                                  time_t *epoch_out)
{
    ha_datetime_parts_t parts = {0};
    return ha_datetime_parse_parts(raw_value, epoch_out, &parts)
               ? ESP_OK
               : ESP_ERR_INVALID_ARG;
}

static bool ha_datetime_is_missing(const char *value, size_t length)
{
    static const char *const missing_values[] = {
        "None",
        "none",
        "null",
        "unknown",
        "unavailable",
    };

    if (!value || length == 0) {
        return true;
    }
    for (size_t i = 0;
         i < sizeof(missing_values) / sizeof(missing_values[0]);
         i++) {
        if (strlen(missing_values[i]) == length &&
            strncmp(value, missing_values[i], length) == 0) {
            return true;
        }
    }
    return false;
}

static bool ha_datetime_parse_parts(const char *raw_value,
                                    time_t *epoch_out,
                                    ha_datetime_parts_t *parts_out)
{
    if (!raw_value || !epoch_out || !parts_out) {
        return false;
    }

    const char *value = raw_value;
    size_t length = strlen(value);
    if (length >= 2 && value[0] == '"' && value[length - 1] == '"') {
        value++;
        length -= 2;
    }
    if (length < 19 || value[4] != '-' || value[7] != '-' ||
        (value[10] != 'T' && value[10] != ' ') ||
        value[13] != ':' || value[16] != ':') {
        return false;
    }

    unsigned year = 0;
    if (!ha_datetime_parse_digits(value, 4, &year) ||
        !ha_datetime_parse_digits(value + 5, 2, &parts_out->month) ||
        !ha_datetime_parse_digits(value + 8, 2, &parts_out->day) ||
        !ha_datetime_parse_digits(value + 11, 2, &parts_out->hour) ||
        !ha_datetime_parse_digits(value + 14, 2, &parts_out->minute) ||
        !ha_datetime_parse_digits(value + 17, 2, &parts_out->second)) {
        return false;
    }
    parts_out->year = (int)year;
    if (parts_out->month < 1 || parts_out->month > 12 ||
        parts_out->day < 1 ||
        parts_out->day > ha_datetime_days_in_month(parts_out->year,
                                                    parts_out->month) ||
        parts_out->hour > 23 || parts_out->minute > 59 ||
        parts_out->second > 59) {
        return false;
    }

    size_t suffix = 19;
    if (suffix < length && value[suffix] == '.') {
        suffix++;
        while (suffix < length && value[suffix] >= '0' &&
               value[suffix] <= '9') {
            suffix++;
        }
    }

    int32_t offset_seconds = 0;
    if (suffix < length && (value[suffix] == '+' || value[suffix] == '-')) {
        if (suffix + 6 != length || value[suffix + 3] != ':') {
            return false;
        }
        unsigned offset_hours = 0;
        unsigned offset_minutes = 0;
        if (!ha_datetime_parse_digits(value + suffix + 1,
                                      2,
                                      &offset_hours) ||
            !ha_datetime_parse_digits(value + suffix + 4,
                                      2,
                                      &offset_minutes) ||
            offset_hours > 23 || offset_minutes > 59) {
            return false;
        }
        const int sign = value[suffix] == '+' ? 1 : -1;
        offset_seconds = sign *
                         (int32_t)(offset_hours * 3600 +
                                   offset_minutes * 60);
    }
    else if (suffix < length &&
             !((value[suffix] == 'Z' || value[suffix] == 'z') &&
               suffix + 1 == length)) {
        return false;
    }

    const int64_t epoch =
        ha_datetime_civil_day_number(parts_out->year,
                                     parts_out->month,
                                     parts_out->day) * 86400 +
        (int64_t)parts_out->hour * 3600 +
        (int64_t)parts_out->minute * 60 + parts_out->second -
        offset_seconds;
    if (epoch < 0) {
        return false;
    }
    *epoch_out = (time_t)epoch;
    return true;
}

static bool ha_datetime_parse_local_epoch(time_t epoch,
                                          ha_datetime_parts_t *parts_out)
{
    if (!parts_out || epoch < 0) {
        return false;
    }

    char local_iso[40] = {0};
    if (sys_time_format_iso8601_time_by_utc_ms((uint64_t)epoch * 1000,
                                               local_iso,
                                               sizeof(local_iso)) != 0) {
        return false;
    }

    time_t ignored_epoch = 0;
    return ha_datetime_parse_parts(local_iso, &ignored_epoch, parts_out);
}

static bool ha_datetime_parse_digits(const char *text,
                                     size_t count,
                                     unsigned *value_out)
{
    if (!text || !value_out) {
        return false;
    }

    unsigned value = 0;
    for (size_t i = 0; i < count; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        value = value * 10 + (unsigned)(text[i] - '0');
    }
    *value_out = value;
    return true;
}

static unsigned ha_datetime_days_in_month(int year, unsigned month)
{
    static const unsigned days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (month < 1 || month > 12) {
        return 0;
    }

    unsigned result = days[month - 1];
    if (month == 2 &&
        ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        result++;
    }
    return result;
}

static int64_t ha_datetime_civil_day_number(int year,
                                            unsigned month,
                                            unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = (unsigned)(year - era * 400);
    const unsigned adjusted_month = month > 2 ? month - 3 : month + 9;
    const unsigned day_of_year =
        (153 * adjusted_month + 2) / 5 + day - 1;
    const unsigned day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 +
        day_of_year;
    return (int64_t)era * 146097 + (int64_t)day_of_era - 719468;
}

static int64_t ha_datetime_floor_mod(int64_t value, int64_t divisor)
{
    const int64_t remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

static int ha_datetime_js_round(double value)
{
    return (int)floor(value + 0.5);
}

static void ha_datetime_format_english(int value,
                                       const char *unit,
                                       char *output,
                                       size_t output_size)
{
    if (value == 0) {
        if (strcmp(unit, "second") == 0) {
            snprintf(output, output_size, "now");
            return;
        }
        if (strcmp(unit, "day") == 0) {
            snprintf(output, output_size, "today");
            return;
        }
        if (strcmp(unit, "week") == 0) {
            snprintf(output, output_size, "this week");
            return;
        }
        if (strcmp(unit, "month") == 0) {
            snprintf(output, output_size, "this month");
            return;
        }
        if (strcmp(unit, "year") == 0) {
            snprintf(output, output_size, "this year");
            return;
        }
    }
    if (value == -1) {
        if (strcmp(unit, "day") == 0) {
            snprintf(output, output_size, "yesterday");
            return;
        }
        if (strcmp(unit, "week") == 0) {
            snprintf(output, output_size, "last week");
            return;
        }
        if (strcmp(unit, "month") == 0) {
            snprintf(output, output_size, "last month");
            return;
        }
        if (strcmp(unit, "year") == 0) {
            snprintf(output, output_size, "last year");
            return;
        }
    }
    if (value == 1) {
        if (strcmp(unit, "day") == 0) {
            snprintf(output, output_size, "tomorrow");
            return;
        }
        if (strcmp(unit, "week") == 0) {
            snprintf(output, output_size, "next week");
            return;
        }
        if (strcmp(unit, "month") == 0) {
            snprintf(output, output_size, "next month");
            return;
        }
        if (strcmp(unit, "year") == 0) {
            snprintf(output, output_size, "next year");
            return;
        }
    }

    const int magnitude = value < 0 ? -value : value;
    if (value < 0) {
        snprintf(output,
                 output_size,
                 "%d %s%s ago",
                 magnitude,
                 unit,
                 magnitude == 1 ? "" : "s");
    }
    else {
        snprintf(output,
                 output_size,
                 "in %d %s%s",
                 magnitude,
                 unit,
                 magnitude == 1 ? "" : "s");
    }
}

static esp_err_t ha_datetime_write_error(const char *text,
                                         esp_err_t error,
                                         char *output,
                                         size_t output_size)
{
    snprintf(output, output_size, "%s", text);
    return error;
}
