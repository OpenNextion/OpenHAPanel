#include "unity.h"

#include <string.h>

#include "i18n.h"

static void assert_preserved(const char *locale,
                             const char *input,
                             const char *expected)
{
    char output[96];
    const i18n_number_options_t options = I18N_NUMBER_OPTIONS_PRESERVE;
    TEST_ASSERT_TRUE(i18n_set_locale(locale) ||
                     strcmp(i18n_get_locale(), locale) == 0);
    TEST_ASSERT_TRUE(i18n_format_number(output, sizeof(output), input,
                                        &options));
    TEST_ASSERT_EQUAL_STRING(expected, output);
}

TEST_CASE("i18n number formatter follows supported locale separators",
          "[i18n]")
{
    assert_preserved("en", "1234567.89", "1,234,567.89");
    assert_preserved("de", "1234567.89", "1.234.567,89");
    assert_preserved("fr", "1234567.89",
                     "1\xC2\xA0" "234\xC2\xA0" "567,89");
    assert_preserved("ru", "1234567.89",
                     "1\xC2\xA0" "234\xC2\xA0" "567,89");
    assert_preserved("ko", "1234567.89", "1,234,567.89");
}

TEST_CASE("i18n number formatter preserves HA state digits and exponent",
          "[i18n]")
{
    assert_preserved("en", "-001234.50", "-1,234.50");
    assert_preserved("de", "1.20e3", "1.200");
    assert_preserved("de", "1e-3", "0,001");
}

TEST_CASE("i18n number formatter supports fixed and maximum precision",
          "[i18n]")
{
    char output[64];
    TEST_ASSERT_TRUE(i18n_set_locale("de") ||
                     strcmp(i18n_get_locale(), "de") == 0);
    TEST_ASSERT_NOT_NULL(i18n_current_locale());
    TEST_ASSERT_EQUAL_STRING("de", i18n_current_locale()->code);
    TEST_ASSERT_EQUAL_STRING(",",
                             i18n_current_locale()->number_decimal_separator);

    const i18n_number_options_t fixed = {
        true, I18N_NUMBER_FRACTION_FIXED, 2
    };
    TEST_ASSERT_TRUE(i18n_format_number(output, sizeof(output), "1234.5",
                                        &fixed));
    TEST_ASSERT_EQUAL_STRING("1.234,50", output);

    const i18n_number_options_t maximum = {
        true, I18N_NUMBER_FRACTION_MAXIMUM, 2
    };
    TEST_ASSERT_TRUE(i18n_format_number_double(output, sizeof(output), 120.0,
                                               &maximum));
    TEST_ASSERT_EQUAL_STRING("120", output);
    TEST_ASSERT_TRUE(i18n_format_number_double(output, sizeof(output), 1234.5,
                                               &maximum));
    TEST_ASSERT_EQUAL_STRING("1.234,5", output);
}

TEST_CASE("i18n number formatter rejects invalid or truncated output",
          "[i18n]")
{
    char output[8];
    const i18n_number_options_t options = I18N_NUMBER_OPTIONS_PRESERVE;
    TEST_ASSERT_FALSE(i18n_format_number(output, sizeof(output), "not-a-number",
                                         &options));
    TEST_ASSERT_EQUAL_STRING("", output);
    TEST_ASSERT_FALSE(i18n_format_number(output, sizeof(output), "1234567.89",
                                         &options));
    TEST_ASSERT_EQUAL_STRING("", output);
}
