#include <string.h>

#include "firmware_manifest.h"
#include "firmware_semver.h"
#include "unity.h"

static const char VALID_MANIFEST[] =
    "{"
    "\"schemaVersion\":2,"
    "\"product\":\"open-ha-panel\","
    "\"channel\":\"stable\","
    "\"version\":\"1.2.3\","
    "\"publishedAt\":\"2026-09-17T10:00:00Z\","
    "\"build\":{"
        "\"gitSha\":\"0123456789abcdef0123456789abcdef01234567\","
        "\"idfVersion\":\"5.5.5\""
    "},"
    "\"target\":{"
        "\"model\":\"ONX3248G035\","
        "\"chip\":\"esp32s3\","
        "\"partitionLayout\":\"ota-6m-v1\""
    "},"
    "\"firmware\":{"
        "\"url\":\"https://github.com/OpenNextion/OpenHAPanel/releases/download/v1.2.3/open-ha-panel-onx3248g035.ota.bin\","
        "\"size\":1678901,"
        "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\""
    "},"
    "\"secureVersion\":0,"
    "\"mandatory\":false,"
    "\"releaseUrl\":\"https://github.com/OpenNextion/OpenHAPanel/releases/tag/v1.2.3\""
    "}";

TEST_CASE("firmware semver accepts valid versions", "[firmware_update]")
{
    TEST_ASSERT_TRUE(firmware_semver_is_valid("0.0.1"));
    TEST_ASSERT_TRUE(firmware_semver_is_valid("1.2.3"));
    TEST_ASSERT_TRUE(firmware_semver_is_valid("1.2.3-beta.1"));
    TEST_ASSERT_TRUE(firmware_semver_is_valid("1.2.3-beta.1+build.5"));
    TEST_ASSERT_TRUE(firmware_semver_is_valid("42949672960.0.1"));
}

TEST_CASE("firmware semver rejects invalid versions", "[firmware_update]")
{
    TEST_ASSERT_FALSE(firmware_semver_is_valid("v1.2.3"));
    TEST_ASSERT_FALSE(firmware_semver_is_valid("1.2"));
    TEST_ASSERT_FALSE(firmware_semver_is_valid("01.2.3"));
    TEST_ASSERT_FALSE(firmware_semver_is_valid("1.2.3-01"));
    TEST_ASSERT_FALSE(firmware_semver_is_valid("1.2.3-"));
    TEST_ASSERT_FALSE(firmware_semver_is_valid("1.2.3+"));
}

TEST_CASE("firmware semver follows prerelease precedence", "[firmware_update]")
{
    static const char *versions[] = {
        "1.0.0-alpha",
        "1.0.0-alpha.1",
        "1.0.0-alpha.beta",
        "1.0.0-beta",
        "1.0.0-beta.2",
        "1.0.0-beta.11",
        "1.0.0-rc.1",
        "1.0.0",
    };

    for (size_t i = 0; i + 1 < sizeof(versions) / sizeof(versions[0]); i++) {
        int result = 0;
        TEST_ASSERT_EQUAL(ESP_OK,
                          firmware_semver_compare(versions[i], versions[i + 1], &result));
        TEST_ASSERT_EQUAL_INT(-1, result);
    }

    int result = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      firmware_semver_compare("1.2.3+build.1", "1.2.3+build.2", &result));
    TEST_ASSERT_EQUAL_INT(0, result);
}

TEST_CASE("firmware manifest parses and validates", "[firmware_update]")
{
    firmware_manifest_t manifest;
    TEST_ASSERT_EQUAL(ESP_OK,
                      firmware_manifest_parse(VALID_MANIFEST,
                                              strlen(VALID_MANIFEST),
                                              &manifest));
    TEST_ASSERT_EQUAL_STRING("1.2.3", manifest.version);
    TEST_ASSERT_EQUAL_STRING("stable", manifest.channel);
    TEST_ASSERT_EQUAL_STRING("ONX3248G035", manifest.device_model);
    TEST_ASSERT_EQUAL_UINT32(1678901, manifest.firmware_size);
    TEST_ASSERT_FALSE(manifest.mandatory);
    TEST_ASSERT_EQUAL_UINT8(0x01, manifest.firmware_sha256[0]);
    TEST_ASSERT_EQUAL_UINT8(0xef, manifest.firmware_sha256[31]);

    TEST_ASSERT_EQUAL(ESP_OK,
                      firmware_manifest_validate_for_device(&manifest,
                                                            "stable",
                                                            6 * 1024 * 1024));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE,
                      firmware_manifest_validate_for_device(&manifest,
                                                            "beta",
                                                            6 * 1024 * 1024));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      firmware_manifest_validate_for_device(&manifest,
                                                            "stable",
                                                            1024));

    int result = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      firmware_manifest_compare_running_version(&manifest, "0.0.1", &result));
    TEST_ASSERT_EQUAL_INT(1, result);
}

TEST_CASE("firmware manifest rejects duplicate required fields", "[firmware_update]")
{
    static const char duplicate_schema[] =
        "{\"schemaVersion\":2,\"schemaVersion\":2}";
    firmware_manifest_t manifest;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      firmware_manifest_parse(duplicate_schema,
                                              strlen(duplicate_schema),
                                              &manifest));
}

TEST_CASE("firmware manifest rejects missing device model", "[firmware_update]")
{
    char manifest_without_model[sizeof(VALID_MANIFEST)];
    strlcpy(manifest_without_model, VALID_MANIFEST, sizeof(manifest_without_model));
    char *model = strstr(manifest_without_model, "\"model\":\"ONX3248G035\",");
    TEST_ASSERT_NOT_NULL(model);
    memmove(model,
            model + strlen("\"model\":\"ONX3248G035\","),
            strlen(model + strlen("\"model\":\"ONX3248G035\",")) + 1);

    firmware_manifest_t manifest;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      firmware_manifest_parse(manifest_without_model,
                                              strlen(manifest_without_model),
                                              &manifest));
}

TEST_CASE("firmware manifest rejects another device model", "[firmware_update]")
{
    char manifest_for_other_model[sizeof(VALID_MANIFEST)];
    strlcpy(manifest_for_other_model, VALID_MANIFEST, sizeof(manifest_for_other_model));
    char *model = strstr(manifest_for_other_model, "ONX3248G035");
    TEST_ASSERT_NOT_NULL(model);
    memcpy(model, "ONX3248G036", strlen("ONX3248G036"));

    firmware_manifest_t manifest;
    TEST_ASSERT_EQUAL(ESP_OK,
                      firmware_manifest_parse(manifest_for_other_model,
                                              strlen(manifest_for_other_model),
                                              &manifest));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      firmware_manifest_validate_for_device(&manifest,
                                                            "stable",
                                                            6 * 1024 * 1024));
}

TEST_CASE("firmware manifest rejects trailing content", "[firmware_update]")
{
    char manifest_with_trailing[sizeof(VALID_MANIFEST) + 2];
    memcpy(manifest_with_trailing, VALID_MANIFEST, sizeof(VALID_MANIFEST) - 1);
    memcpy(manifest_with_trailing + sizeof(VALID_MANIFEST) - 1, "{}", 3);

    firmware_manifest_t manifest;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      firmware_manifest_parse(manifest_with_trailing,
                                              strlen(manifest_with_trailing),
                                              &manifest));
}
