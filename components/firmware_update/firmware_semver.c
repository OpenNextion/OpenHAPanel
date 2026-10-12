#include "firmware_semver.h"

#include <string.h>

typedef struct {
    const char *value;
    size_t length;
} firmware_semver_identifier_t;

typedef struct {
    firmware_semver_identifier_t major;
    firmware_semver_identifier_t minor;
    firmware_semver_identifier_t patch;
    const char *prerelease;
    size_t prerelease_len;
} firmware_semver_t;

static bool semver_is_digit(char value)
{
    return value >= '0' && value <= '9';
}

static bool semver_is_identifier_char(char value)
{
    return semver_is_digit(value) ||
           (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') ||
           value == '-';
}

static bool semver_parse_number(const char **cursor, firmware_semver_identifier_t *number)
{
    const char *start = *cursor;
    if (!semver_is_digit(*start)) {
        return false;
    }
    if (*start == '0' && semver_is_digit(start[1])) {
        return false;
    }

    const char *p = start;
    while (semver_is_digit(*p)) {
        p++;
    }

    number->value = start;
    number->length = (size_t)(p - start);
    *cursor = p;
    return true;
}

static bool semver_validate_identifiers(const char *start,
                                        const char *end,
                                        bool reject_numeric_leading_zero)
{
    if (start == end) {
        return false;
    }

    const char *identifier = start;
    for (const char *p = start; p <= end; p++) {
        if (p != end && *p != '.') {
            if (!semver_is_identifier_char(*p)) {
                return false;
            }
            continue;
        }

        size_t length = (size_t)(p - identifier);
        if (length == 0) {
            return false;
        }

        bool numeric = true;
        for (const char *q = identifier; q < p; q++) {
            if (!semver_is_digit(*q)) {
                numeric = false;
                break;
            }
        }
        if (reject_numeric_leading_zero && numeric && length > 1 && identifier[0] == '0') {
            return false;
        }
        identifier = p + 1;
    }
    return true;
}

static bool semver_parse(const char *version, firmware_semver_t *parsed)
{
    if (!version || !parsed || version[0] == '\0') {
        return false;
    }

    memset(parsed, 0, sizeof(*parsed));
    const char *p = version;
    if (!semver_parse_number(&p, &parsed->major) || *p++ != '.' ||
        !semver_parse_number(&p, &parsed->minor) || *p++ != '.' ||
        !semver_parse_number(&p, &parsed->patch)) {
        return false;
    }

    if (*p == '-') {
        const char *start = ++p;
        while (*p != '\0' && *p != '+') {
            p++;
        }
        if (!semver_validate_identifiers(start, p, true)) {
            return false;
        }
        parsed->prerelease = start;
        parsed->prerelease_len = (size_t)(p - start);
    }

    if (*p == '+') {
        const char *start = ++p;
        while (*p != '\0') {
            p++;
        }
        if (!semver_validate_identifiers(start, p, false)) {
            return false;
        }
    }

    return *p == '\0';
}

static int semver_compare_numeric(const firmware_semver_identifier_t *left,
                                  const firmware_semver_identifier_t *right)
{
    if (left->length != right->length) {
        return left->length < right->length ? -1 : 1;
    }
    int lexical = memcmp(left->value, right->value, left->length);
    if (lexical == 0) {
        return 0;
    }
    return lexical < 0 ? -1 : 1;
}

static bool semver_identifier_is_numeric(const char *identifier, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        if (!semver_is_digit(identifier[i])) {
            return false;
        }
    }
    return true;
}

static int semver_compare_identifier(const char *left,
                                     size_t left_len,
                                     const char *right,
                                     size_t right_len)
{
    bool left_numeric = semver_identifier_is_numeric(left, left_len);
    bool right_numeric = semver_identifier_is_numeric(right, right_len);

    if (left_numeric && right_numeric) {
        if (left_len != right_len) {
            return left_len < right_len ? -1 : 1;
        }
    } else if (left_numeric != right_numeric) {
        return left_numeric ? -1 : 1;
    }

    size_t common_len = left_len < right_len ? left_len : right_len;
    int lexical = memcmp(left, right, common_len);
    if (lexical != 0) {
        return lexical < 0 ? -1 : 1;
    }
    if (left_len == right_len) {
        return 0;
    }
    return left_len < right_len ? -1 : 1;
}

static int semver_compare_prerelease(const firmware_semver_t *left,
                                     const firmware_semver_t *right)
{
    if (!left->prerelease && !right->prerelease) {
        return 0;
    }
    if (!left->prerelease) {
        return 1;
    }
    if (!right->prerelease) {
        return -1;
    }

    size_t left_offset = 0;
    size_t right_offset = 0;
    while (left_offset < left->prerelease_len && right_offset < right->prerelease_len) {
        size_t left_end = left_offset;
        size_t right_end = right_offset;
        while (left_end < left->prerelease_len && left->prerelease[left_end] != '.') {
            left_end++;
        }
        while (right_end < right->prerelease_len && right->prerelease[right_end] != '.') {
            right_end++;
        }

        int result = semver_compare_identifier(left->prerelease + left_offset,
                                               left_end - left_offset,
                                               right->prerelease + right_offset,
                                               right_end - right_offset);
        if (result != 0) {
            return result;
        }

        left_offset = left_end < left->prerelease_len ? left_end + 1 : left_end;
        right_offset = right_end < right->prerelease_len ? right_end + 1 : right_end;
    }

    if (left_offset == left->prerelease_len && right_offset == right->prerelease_len) {
        return 0;
    }
    return left_offset == left->prerelease_len ? -1 : 1;
}

bool firmware_semver_is_valid(const char *version)
{
    firmware_semver_t parsed;
    return semver_parse(version, &parsed);
}

esp_err_t firmware_semver_compare(const char *left, const char *right, int *result)
{
    if (!left || !right || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    firmware_semver_t parsed_left;
    firmware_semver_t parsed_right;
    if (!semver_parse(left, &parsed_left) || !semver_parse(right, &parsed_right)) {
        return ESP_ERR_INVALID_ARG;
    }

    int comparison = semver_compare_numeric(&parsed_left.major, &parsed_right.major);
    if (comparison == 0) {
        comparison = semver_compare_numeric(&parsed_left.minor, &parsed_right.minor);
    }
    if (comparison == 0) {
        comparison = semver_compare_numeric(&parsed_left.patch, &parsed_right.patch);
    }
    if (comparison == 0) {
        comparison = semver_compare_prerelease(&parsed_left, &parsed_right);
    }

    *result = comparison;
    return ESP_OK;
}
