#include "login_session.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "nvs_storage.h"

#define LOGIN_SESSION_NAMESPACE "web_session"
#define LOGIN_SESSION_SLOT_COUNT 4
#define LOGIN_SESSION_ID_BYTES 16
#define LOGIN_SESSION_TOKEN_BYTES 32
#define LOGIN_SESSION_SHA256_BYTES 32
#define LOGIN_SESSION_VALID_EPOCH_MIN 1577836800UL

typedef struct {
    bool valid;
    char session_id[LOGIN_SESSION_ID_HEX_LEN + 1];
    char verifier[LOGIN_SESSION_PROOF_HEX_LEN + 1];
    uint32_t created_at;
    uint32_t expires_at;
} login_session_slot_t;

typedef struct {
    SemaphoreHandle_t lock;
    bool factory_resetting;
    login_session_slot_t slots[LOGIN_SESSION_SLOT_COUNT];
} login_session_context_t;

static const char *TAG = "login_session";
static login_session_context_t s_sessions;

void login_session_secure_zero(void *data, size_t length)
{
    volatile uint8_t *p = data;
    while (p && length--) {
        *p++ = 0;
    }
}

static void bytes_to_hex(const uint8_t *input, size_t length, char *output)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < length; i++) {
        output[i * 2] = hex[input[i] >> 4];
        output[i * 2 + 1] = hex[input[i] & 0x0f];
    }
    output[length * 2] = '\0';
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool hex_to_bytes(const char *input, size_t expected_length, uint8_t *output)
{
    if (!input || strnlen(input, expected_length + 1) != expected_length || (expected_length & 1U)) return false;
    for (size_t i = 0; i < expected_length / 2; i++) {
        int high = hex_value(input[i * 2]);
        int low = hex_value(input[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        output[i] = (uint8_t)((high << 4) | low);
    }
    return true;
}

static bool is_lower_hex(const char *value, size_t length)
{
    if (!value || strnlen(value, length + 1) != length) return false;
    for (size_t i = 0; i < length; i++) {
        if (!isdigit((unsigned char)value[i]) && !(value[i] >= 'a' && value[i] <= 'f')) return false;
    }
    return true;
}

static bool constant_time_equal(const uint8_t *a, const uint8_t *b, size_t length)
{
    uint8_t difference = 0;
    for (size_t i = 0; i < length; i++) difference |= a[i] ^ b[i];
    return difference == 0;
}

static esp_err_t sha256_bytes(const uint8_t *input, size_t length, uint8_t output[LOGIN_SESSION_SHA256_BYTES])
{
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    int result = mbedtls_sha256_starts(&context, 0);
    if (result == 0) result = mbedtls_sha256_update(&context, input, length);
    if (result == 0) result = mbedtls_sha256_finish(&context, output);
    mbedtls_sha256_free(&context);
    return result == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t hmac_sha256(const uint8_t *key,
                             size_t key_length,
                             const uint8_t *message,
                             size_t message_length,
                             uint8_t output[LOGIN_SESSION_SHA256_BYTES])
{
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!info) return ESP_FAIL;
    return mbedtls_md_hmac(info, key, key_length, message, message_length, output) == 0 ? ESP_OK : ESP_FAIL;
}

bool login_session_time_is_valid(void)
{
    time_t now = time(NULL);
    return now >= (time_t)LOGIN_SESSION_VALID_EPOCH_MIN &&
           (uint64_t)now <= UINT32_MAX - LOGIN_SESSION_LIFETIME_SECONDS;
}

const char *login_session_reason_to_string(login_session_reason_t reason)
{
    switch (reason) {
    case LOGIN_SESSION_REASON_NONE: return "none";
    case LOGIN_SESSION_REASON_INVALID: return "sessionInvalid";
    case LOGIN_SESSION_REASON_EXPIRED: return "sessionExpired";
    case LOGIN_SESSION_REASON_TIME_UNAVAILABLE: return "timeUnavailable";
    case LOGIN_SESSION_REASON_STORAGE_FAILED: return "storageFailed";
    case LOGIN_SESSION_REASON_FACTORY_RESETTING: return "sessionInvalid";
    default: return "sessionInvalid";
    }
}

static void make_key(char *buffer, size_t size, size_t slot, const char *suffix)
{
    snprintf(buffer, size, "s%u_%s", (unsigned)slot, suffix);
}

static esp_err_t save_slot(size_t index, const login_session_slot_t *slot)
{
    char valid_key[12], id_key[12], verifier_key[12], created_key[12], expires_key[12];
    make_key(valid_key, sizeof(valid_key), index, "valid");
    make_key(id_key, sizeof(id_key), index, "id");
    make_key(verifier_key, sizeof(verifier_key), index, "verify");
    make_key(created_key, sizeof(created_key), index, "created");
    make_key(expires_key, sizeof(expires_key), index, "exp");

    const nvs_storage_write_item_t items[] = {
        {.key = id_key, .type = NVS_STORAGE_TYPE_STRING, .value.string = slot->session_id},
        {.key = verifier_key, .type = NVS_STORAGE_TYPE_STRING, .value.string = slot->verifier},
        {.key = created_key, .type = NVS_STORAGE_TYPE_U32, .value.u32 = slot->created_at},
        {.key = expires_key, .type = NVS_STORAGE_TYPE_U32, .value.u32 = slot->expires_at},
        {.key = valid_key, .type = NVS_STORAGE_TYPE_U8, .value.u8 = slot->valid ? 1 : 0},
    };
    return nvs_storage_write(LOGIN_SESSION_NAMESPACE, items, sizeof(items) / sizeof(items[0]));
}

static esp_err_t invalidate_slot(size_t index)
{
    char valid_key[12];
    make_key(valid_key, sizeof(valid_key), index, "valid");
    esp_err_t err = nvs_storage_set_u8(LOGIN_SESSION_NAMESPACE, valid_key, 0);
    login_session_secure_zero(&s_sessions.slots[index], sizeof(s_sessions.slots[index]));
    return err;
}

static void load_slot(size_t index)
{
    login_session_slot_t slot = {0};
    uint8_t valid = 0;
    char valid_key[12], id_key[12], verifier_key[12], created_key[12], expires_key[12];
    make_key(valid_key, sizeof(valid_key), index, "valid");
    make_key(id_key, sizeof(id_key), index, "id");
    make_key(verifier_key, sizeof(verifier_key), index, "verify");
    make_key(created_key, sizeof(created_key), index, "created");
    make_key(expires_key, sizeof(expires_key), index, "exp");

    nvs_storage_read_item_t items[] = {
        {.key = id_key, .type = NVS_STORAGE_TYPE_STRING, .value = slot.session_id,
         .value_size = sizeof(slot.session_id)},
        {.key = verifier_key, .type = NVS_STORAGE_TYPE_STRING, .value = slot.verifier,
         .value_size = sizeof(slot.verifier)},
        {.key = created_key, .type = NVS_STORAGE_TYPE_U32, .value = &slot.created_at},
        {.key = expires_key, .type = NVS_STORAGE_TYPE_U32, .value = &slot.expires_at},
        {.key = valid_key, .type = NVS_STORAGE_TYPE_U8, .value = &valid},
    };
    esp_err_t err = nvs_storage_read(LOGIN_SESSION_NAMESPACE, items, sizeof(items) / sizeof(items[0]));
    if (err == ESP_OK && valid == 1 &&
        items[0].result == ESP_OK && items[1].result == ESP_OK &&
        items[2].result == ESP_OK && items[3].result == ESP_OK && items[4].result == ESP_OK &&
        is_lower_hex(slot.session_id, LOGIN_SESSION_ID_HEX_LEN) &&
        is_lower_hex(slot.verifier, LOGIN_SESSION_PROOF_HEX_LEN) &&
        slot.created_at >= LOGIN_SESSION_VALID_EPOCH_MIN && slot.expires_at > slot.created_at) {
        slot.valid = true;
        s_sessions.slots[index] = slot;
    }
    else {
        login_session_secure_zero(&s_sessions.slots[index], sizeof(s_sessions.slots[index]));
    }
    login_session_secure_zero(&slot, sizeof(slot));
}

esp_err_t login_session_init(void)
{
    if (s_sessions.lock) return ESP_OK;
    s_sessions.lock = xSemaphoreCreateMutex();
    if (!s_sessions.lock) return ESP_ERR_NO_MEM;

    for (size_t i = 0; i < LOGIN_SESSION_SLOT_COUNT; i++) load_slot(i);
    return ESP_OK;
}

static bool session_id_exists_locked(const char *session_id)
{
    for (size_t i = 0; i < LOGIN_SESSION_SLOT_COUNT; i++) {
        if (s_sessions.slots[i].valid && strcmp(s_sessions.slots[i].session_id, session_id) == 0) return true;
    }
    return false;
}

static size_t choose_slot_locked(uint32_t now)
{
    size_t oldest = 0;
    for (size_t i = 0; i < LOGIN_SESSION_SLOT_COUNT; i++) {
        if (!s_sessions.slots[i].valid || s_sessions.slots[i].expires_at <= now) return i;
        if (s_sessions.slots[i].created_at < s_sessions.slots[oldest].created_at) oldest = i;
    }
    return oldest;
}

esp_err_t login_session_issue(login_session_credential_t *credential,
                              login_session_reason_t *reason)
{
    if (reason) *reason = LOGIN_SESSION_REASON_INVALID;
    if (!credential || !s_sessions.lock) return ESP_ERR_INVALID_ARG;
    memset(credential, 0, sizeof(*credential));
    if (!login_session_time_is_valid()) {
        if (reason) *reason = LOGIN_SESSION_REASON_TIME_UNAVAILABLE;
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t random_id[LOGIN_SESSION_ID_BYTES];
    uint8_t random_token[LOGIN_SESSION_TOKEN_BYTES];
    uint8_t verifier[LOGIN_SESSION_SHA256_BYTES];
    login_session_slot_t new_slot = {0};
    time_t now_time = time(NULL);
    uint32_t now = (uint32_t)now_time;

    xSemaphoreTake(s_sessions.lock, portMAX_DELAY);
    if (s_sessions.factory_resetting) {
        xSemaphoreGive(s_sessions.lock);
        if (reason) *reason = LOGIN_SESSION_REASON_FACTORY_RESETTING;
        return ESP_ERR_INVALID_STATE;
    }

    do {
        esp_fill_random(random_id, sizeof(random_id));
        bytes_to_hex(random_id, sizeof(random_id), new_slot.session_id);
    } while (session_id_exists_locked(new_slot.session_id));
    esp_fill_random(random_token, sizeof(random_token));
    bytes_to_hex(random_token, sizeof(random_token), credential->session_token);
    if (sha256_bytes((const uint8_t *)credential->session_token,
                     strlen(credential->session_token), verifier) != ESP_OK) {
        xSemaphoreGive(s_sessions.lock);
        login_session_secure_zero(credential, sizeof(*credential));
        login_session_secure_zero(random_id, sizeof(random_id));
        login_session_secure_zero(random_token, sizeof(random_token));
        login_session_secure_zero(verifier, sizeof(verifier));
        login_session_secure_zero(&new_slot, sizeof(new_slot));
        return ESP_FAIL;
    }

    bytes_to_hex(verifier, sizeof(verifier), new_slot.verifier);
    new_slot.valid = true;
    new_slot.created_at = now;
    new_slot.expires_at = now + LOGIN_SESSION_LIFETIME_SECONDS;
    strlcpy(credential->session_id, new_slot.session_id, sizeof(credential->session_id));
    credential->expires_at = new_slot.expires_at;

    size_t index = choose_slot_locked(now);
    esp_err_t err = save_slot(index, &new_slot);
    if (err == ESP_OK) {
        s_sessions.slots[index] = new_slot;
        if (reason) *reason = LOGIN_SESSION_REASON_NONE;
    }
    else {
        login_session_secure_zero(credential, sizeof(*credential));
        if (reason) *reason = LOGIN_SESSION_REASON_STORAGE_FAILED;
    }
    xSemaphoreGive(s_sessions.lock);

    login_session_secure_zero(random_id, sizeof(random_id));
    login_session_secure_zero(random_token, sizeof(random_token));
    login_session_secure_zero(verifier, sizeof(verifier));
    login_session_secure_zero(&new_slot, sizeof(new_slot));
    return err;
}

esp_err_t login_session_verify(const char *session_id,
                               const char *server_nonce,
                               const char *client_nonce,
                               const char *proof,
                               uint32_t *expires_at,
                               login_session_reason_t *reason)
{
    if (reason) *reason = LOGIN_SESSION_REASON_INVALID;
    size_t client_nonce_length = client_nonce ? strnlen(client_nonce, LOGIN_SESSION_CNONCE_MAX_LEN + 1) : 0;
    if (!s_sessions.lock || !expires_at ||
        !is_lower_hex(session_id, LOGIN_SESSION_ID_HEX_LEN) ||
        !is_lower_hex(server_nonce, LOGIN_SESSION_PROOF_HEX_LEN) ||
        client_nonce_length < 16 || client_nonce_length > LOGIN_SESSION_CNONCE_MAX_LEN ||
        !is_lower_hex(client_nonce, client_nonce_length) ||
        !is_lower_hex(proof, LOGIN_SESSION_PROOF_HEX_LEN)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!login_session_time_is_valid()) {
        if (reason) *reason = LOGIN_SESSION_REASON_TIME_UNAVAILABLE;
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t verifier[LOGIN_SESSION_SHA256_BYTES] = {0};
    uint8_t expected[LOGIN_SESSION_SHA256_BYTES] = {0};
    uint8_t received[LOGIN_SESSION_SHA256_BYTES] = {0};
    char message[256];
    uint32_t now = (uint32_t)time(NULL);
    esp_err_t result = ESP_ERR_NOT_FOUND;

    xSemaphoreTake(s_sessions.lock, portMAX_DELAY);
    if (s_sessions.factory_resetting) {
        if (reason) *reason = LOGIN_SESSION_REASON_FACTORY_RESETTING;
        result = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    size_t index = LOGIN_SESSION_SLOT_COUNT;
    for (size_t i = 0; i < LOGIN_SESSION_SLOT_COUNT; i++) {
        if (s_sessions.slots[i].valid && strcmp(s_sessions.slots[i].session_id, session_id) == 0) {
            index = i;
            break;
        }
    }
    if (index == LOGIN_SESSION_SLOT_COUNT) goto exit;
    if (s_sessions.slots[index].expires_at <= now) {
        invalidate_slot(index);
        if (reason) *reason = LOGIN_SESSION_REASON_EXPIRED;
        result = ESP_ERR_TIMEOUT;
        goto exit;
    }
    if (!hex_to_bytes(s_sessions.slots[index].verifier, LOGIN_SESSION_PROOF_HEX_LEN, verifier) ||
        !hex_to_bytes(proof, LOGIN_SESSION_PROOF_HEX_LEN, received)) {
        goto exit;
    }

    int length = snprintf(message, sizeof(message), "resume-v1\n%s\n%s\n%s",
                          session_id, server_nonce, client_nonce);
    if (length < 0 || (size_t)length >= sizeof(message) ||
        hmac_sha256(verifier, sizeof(verifier), (const uint8_t *)message, (size_t)length, expected) != ESP_OK) {
        result = ESP_FAIL;
        goto exit;
    }
    if (!constant_time_equal(expected, received, sizeof(expected))) goto exit;

    *expires_at = s_sessions.slots[index].expires_at;
    if (reason) *reason = LOGIN_SESSION_REASON_NONE;
    result = ESP_OK;

exit:
    xSemaphoreGive(s_sessions.lock);
    login_session_secure_zero(verifier, sizeof(verifier));
    login_session_secure_zero(expected, sizeof(expected));
    login_session_secure_zero(received, sizeof(received));
    login_session_secure_zero(message, sizeof(message));
    return result;
}

esp_err_t login_session_revoke(const char *session_id)
{
    if (!s_sessions.lock || !is_lower_hex(session_id, LOGIN_SESSION_ID_HEX_LEN)) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_sessions.lock, portMAX_DELAY);
    esp_err_t result = ESP_ERR_NOT_FOUND;
    for (size_t i = 0; i < LOGIN_SESSION_SLOT_COUNT; i++) {
        if (s_sessions.slots[i].valid && strcmp(s_sessions.slots[i].session_id, session_id) == 0) {
            result = invalidate_slot(i);
            break;
        }
    }
    xSemaphoreGive(s_sessions.lock);
    return result;
}

esp_err_t login_session_revoke_all(void)
{
    if (!s_sessions.lock) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_sessions.lock, portMAX_DELAY);
    esp_err_t err = nvs_storage_erase_namespace(LOGIN_SESSION_NAMESPACE);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        login_session_secure_zero(s_sessions.slots, sizeof(s_sessions.slots));
        err = ESP_OK;
    }
    xSemaphoreGive(s_sessions.lock);
    return err;
}

esp_err_t login_session_prepare_factory_reset(void)
{
    if (!s_sessions.lock) {
        esp_err_t err = nvs_storage_erase_namespace(LOGIN_SESSION_NAMESPACE);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
    }
    xSemaphoreTake(s_sessions.lock, portMAX_DELAY);
    s_sessions.factory_resetting = true;
    esp_err_t err = nvs_storage_erase_namespace(LOGIN_SESSION_NAMESPACE);
    login_session_secure_zero(s_sessions.slots, sizeof(s_sessions.slots));
    xSemaphoreGive(s_sessions.lock);
    return (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : err;
}
