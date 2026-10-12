#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "littlefs.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "sys_time_sync.h"
#include "cjson_helpers.h"
#include "webserver_config.h"

#define WAIT_TIMEOUT_MS 10000
#define AES_BLOCK_SIZE 16
#define AES_KEY_HEX_LEN 32
#define BASE64_ENC_BUF_LEN BASE64_BUFFER_SIZE

/**
 * @brief Safely copies a string to a fixed-size buffer
 *
 * Copies content from `src` to `dst` with truncation safety based on `sizeof(dst)`.
 * Note: Only safe when `dst` is a fixed-size array (not a pointer).
 */
#define COPY_FIELD(dst, src) strncpy((dst), (src), sizeof(dst))

/**
 * @brief Global configuration data stored in external RAM
 */
EXT_RAM_BSS_ATTR webserver_config_t config_info = {0};

/**
 * @brief Global device information stored in external RAM
 */
EXT_RAM_BSS_ATTR device_info_t device_info = {0};

/**
 * @brief Global night mode configuration stored in external RAM
 */
EXT_RAM_BSS_ATTR night_mode_config_t s_night_mode_config = {0};

/**
 * @brief Logger tag
 */
static const char *TAG = "webserver_config";

static char product_id[16];

/**
 * @brief Get the pointer to the product ID.
 *
 * @return Pointer to the DeviceInfo structure holding device information.
 */
char *webserver_config_get_product_id(void)
{
    return product_id;
}

/**
 * @brief Encrypt input data using AES-CBC with PKCS#7 padding.
 *
 * This function encrypts the given input buffer using AES encryption in CBC mode.
 * It applies PKCS#7 padding to the input to align with the AES block size.
 *
 * @param input  Pointer to the plaintext input data.
 * @param length Length of the input data in bytes.
 * @param output Pointer to the buffer where the encrypted output will be stored.
 *               Must be at least ((length / AES_KEY_SIZE) + 1) * AES_KEY_SIZE bytes.
 * @param key    Pointer to the AES key (16 bytes).
 * @param iv     Pointer to the AES initialization vector (16 bytes).
 */
static void encryptAes(const unsigned char *input, size_t length, unsigned char *output, const unsigned char *key,
                       const unsigned char *iv)
{
    mbedtls_aes_context aes;
    unsigned char iv_copy[AES_IV_SIZE];
    memcpy(iv_copy, iv, AES_IV_SIZE);

    size_t padded_length = ((length / AES_KEY_SIZE) + 1) * AES_KEY_SIZE;
    unsigned char padded_input[padded_length];

    memcpy(padded_input, input, length);
    unsigned char pad = AES_KEY_SIZE - (length % AES_KEY_SIZE);
    memset(padded_input + length, pad, pad);

    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padded_length, iv_copy, padded_input, output);
    mbedtls_aes_free(&aes);
}

/**
 * @brief Decrypt input data using AES-CBC and remove PKCS#7 padding.
 *
 * This function decrypts the given input buffer using AES decryption in CBC mode.
 * After decryption, it removes the PKCS#7 padding if valid and appends a null terminator.
 *
 * @param input  Pointer to the encrypted input data.
 * @param length Length of the encrypted data in bytes (must be multiple of AES block size).
 * @param output Pointer to the buffer where the decrypted output will be stored.
 *               Should be large enough to hold the decrypted data plus a null terminator.
 * @param key    Pointer to the AES key (16 bytes).
 * @param iv     Pointer to the AES initialization vector (16 bytes).
 */
static void decryptAes(const unsigned char *input, size_t length, unsigned char *output, const unsigned char *key,
                       const unsigned char *iv)
{
    mbedtls_aes_context aes;
    unsigned char iv_copy[AES_IV_SIZE];
    memcpy(iv_copy, iv, AES_IV_SIZE);

    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 128);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, length, iv_copy, input, output);
    mbedtls_aes_free(&aes);

    unsigned char pad = output[length - 1];
    if (pad > 0 && pad <= AES_KEY_SIZE) {
        int valid = 1;
        for (int i = 0; i < pad; i++) {
            if (output[length - 1 - i] != pad) {
                valid = 0;
                break;
            }
        }

        if (valid) {
            length -= pad;
            output[length] = '\0';
        }
    }
}

/**
 * @brief Save device information to a JSON file.
 *
 * This function serializes the device information pointed to by `info` into a JSON object,
 * then writes the JSON string to the device info file. It includes theme mode, product name,
 * brightness, language, and product ID fields.
 *
 * @param info Pointer to the DeviceInfo structure to be saved.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL if input pointer is NULL, JSON creation fails, or file writing fails.
 */
int webserver_config_save_device_info_to_file(device_info_t *info)
{
    if (!info) {
        ESP_LOGE(TAG, "The device information pointer is empty");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    cJSON *theme_mode = cJSON_CreateObject();
    cJSON_AddStringToObject(theme_mode, "currentMode", info->theme_mode.current_mode);
    cJSON_AddBoolToObject(theme_mode, "followSystem", info->theme_mode.follow_system);
    cJSON_AddItemToObject(root, "theme_mode", theme_mode);
    cJSON_AddStringToObject(root, "product_name", info->product_name);
    cJSON_AddStringToObject(root, "language", info->language);
    cJSON_AddStringToObject(root, "product_id", product_id);
    cJSON_AddNumberToObject(root, "brightness", info->brightness);
    cJSON_AddNumberToObject(root,"idle_time",info->idle_time);

    if (!little_file_system_write_json_to_file(root, DEVICE_INFO_FILE_NAME)) {
        return ESP_FAIL;
    }
    memcpy(&device_info, info, sizeof(device_info_t));
    return ESP_OK;
}

/**
 * @brief Load device information from a JSON file.
 *
 * This function reads and parses the JSON file containing device information.
 * It populates the DeviceInfo structure pointed to by `info` with the parsed data,
 * including theme mode, product name, brightness, language, and product ID.
 *
 * @param info Pointer to the DeviceInfo structure to store loaded data.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL if input pointer is NULL, file reading fails, or JSON parsing fails.
 */
int webserver_config_load_device_info_from_file(device_info_t *info)
{
    if (!info) {
        ESP_LOGE(TAG, "The device information pointer is empty");
        return ESP_FAIL;
    }

    cJSON *root = little_file_system_read_file_to_json(DEVICE_INFO_FILE_NAME);
    if (!root) {
        ESP_LOGE(TAG, "Device information file does not exist or read failed");
        return ESP_FAIL;
    }

    char *string;
    if (cjson_parse_string_field(root, "product_name", &string)) {
        strlcpy(info->product_name, string, sizeof(info->product_name));
    }
    cJSON *theme_mode = cJSON_GetObjectItem(root, "theme_mode");
    if (theme_mode) {
        if (cjson_parse_string_field(theme_mode, "currentMode", &string)) {
            strlcpy(info->theme_mode.current_mode, string, sizeof(info->theme_mode.current_mode));
        }
        cjson_parse_bool_field(theme_mode, "followSystem", &info->theme_mode.follow_system);
    }
    if (cjson_parse_string_field(root, "language", &string)) {
        strlcpy(info->language, string, sizeof(info->language));
    }
    cJSON *json_brightness = cJSON_GetObjectItem(root, "brightness");
    if (json_brightness && cJSON_IsNumber(json_brightness)) {
        int br = json_brightness->valueint;
        info->brightness = (br >= 0 && br <= 100) ? (uint8_t)br : 100;
    }
    cJSON *json_idle_time = cJSON_GetObjectItem(root, "idle_time");
    if (json_idle_time && cJSON_IsNumber(json_idle_time)) {
        int idle = json_idle_time->valueint;
        ESP_LOGI(TAG, "idle_time: %d", idle);
        info->idle_time = (idle >= 0) ? (uint32_t)idle : 0;
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Device information read successfully");

    return ESP_OK;
}

/**
 * @brief Check for duplicate IP addresses in the whitelist.
 *
 * Iterates through the IP whitelist in the provided Config structure and
 * checks if any duplicate IP addresses exist.
 *
 * @param config Pointer to the Config structure containing the IP whitelist.
 *
 * @return
 *      - ESP_OK if no duplicates are found
 *      - ESP_FAIL if a duplicate IP is detected
 */
static esp_err_t check_ip_whitelist_duplicates(const webserver_config_t *config)
{
    for (uint8_t i = 0; i < config->ip_count; i++) {
        for (uint8_t j = i + 1; j < config->ip_count; j++) {
            if (strcmp(config->ip_White_list[i], config->ip_White_list[j]) == 0) {
                ESP_LOGE(TAG, "Duplicate IP found: %s", config->ip_White_list[i]);
                return ESP_FAIL;
            }
        }
    }
    return ESP_OK;
}

/**
 * @brief Initialize AES key and IV in Config and convert them to hex strings.
 *
 * Copies AES key and IV from the global config_info into the provided Config structure,
 * then converts these binary keys to hex string representations.
 *
 * @param config Pointer to the Config structure to initialize.
 * @param aes_key_hex Buffer to store the hex string of the AES key (must be at least AES_KEY_HEX_LEN + 1 bytes).
 * @param aes_iv_hex Buffer to store the hex string of the AES IV (must be at least AES_KEY_HEX_LEN + 1 bytes).
 */
static void init_config_aes_key_iv(webserver_config_t *config)
{
    for (int i = 0; i < AES_BLOCK_SIZE; i++) {
        sprintf(config->aes_key_hex + i * 2, "%02X", (uint8_t)config->aes_key[i]);
        sprintf(config->aes_iv_hex + i * 2, "%02X", (uint8_t)config->aes_iv[i]);
    }
    config->aes_key_hex[AES_KEY_HEX_LEN] = '\0';
    config->aes_iv_hex[AES_KEY_HEX_LEN] = '\0';
}

/**
 * @brief Encrypt input data using AES and encode the result in base64.
 *
 * Encrypts the input string with AES-CBC using the provided key and IV,
 * then base64-encodes the encrypted output.
 *
 * @param input Null-terminated input string to encrypt.
 * @param key AES key for encryption.
 * @param iv AES initialization vector.
 * @param out_encoded Buffer to store the base64-encoded output.
 * @param out_size Size of the output buffer.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL on encryption or base64 encoding failure
 */
static esp_err_t encrypt_and_base64(const char *input, const unsigned char *key, const unsigned char *iv,
                                    char *out_encoded, size_t out_size)
{
    unsigned char encrypted[64] = {0};
    size_t input_len = strlen(input);
    size_t padded_len = ((input_len / AES_BLOCK_SIZE) + 1) * AES_BLOCK_SIZE;
    size_t enc_len = 0;

    encryptAes((const unsigned char *)input, input_len, encrypted, key, iv);

    if (mbedtls_base64_encode((unsigned char *)out_encoded, out_size, &enc_len, encrypted, padded_len) != 0) {
        ESP_LOGE(TAG, "Base64 encoding failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Build a cJSON object representing the Config structure.
 *
 * Constructs a JSON object containing the IP whitelist, AES keys (in hex),
 * enabled flag, and encrypted + base64-encoded web and AP passwords.
 *
 * @param config Pointer to the Config structure to serialize.
 * @param aes_key_hex Hex string representation of the AES key.
 * @param aes_iv_hex Hex string representation of the AES IV.
 *
 * @return
 *      - Pointer to a cJSON object on success (caller must free)
 *      - NULL on failure
 */
static cJSON *build_config_json(const webserver_config_t *config)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON_AddNumberToObject(root, "ip_count", config->ip_count);
    cJSON_AddBoolToObject(root, "enabled", config->enabled);

    cJSON *ip_list = cJSON_CreateArray();
    for (int i = 0; i < config->ip_count; i++) {
        cJSON_AddItemToArray(ip_list, cJSON_CreateString(config->ip_White_list[i]));
    }
    cJSON_AddItemToObject(root, "ip_White_list", ip_list);
    cJSON_AddStringToObject(root, "aes_key", config->aes_key_hex);
    cJSON_AddStringToObject(root, "aes_iv", config->aes_iv_hex);

    char encoded_web[BASE64_ENC_BUF_LEN] = {0};
    char encoded_ap[BASE64_ENC_BUF_LEN] = {0};

    if (encrypt_and_base64(config->web_password, (unsigned char *)config->aes_key, (unsigned char *)config->aes_iv,
                           encoded_web, sizeof(encoded_web)) != ESP_OK ||
        encrypt_and_base64("0", (unsigned char *)config->aes_key, (unsigned char *)config->aes_iv, encoded_ap,
                           sizeof(encoded_ap)) != ESP_OK) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_AddStringToObject(root, "web_password", encoded_web);
    cJSON_AddStringToObject(root, "ap_password", encoded_ap);

    return root;
}

/**
 * @brief Save the configuration information to a JSON file.
 *
 * This function validates the IP whitelist for duplicates, initializes AES key and IV
 * in hexadecimal format, builds the JSON representation of the configuration,
 * and writes it to the configuration file. Upon success, it updates the global
 * config_info with the new configuration.
 *
 * @param config Pointer to the Config structure containing configuration data to save.
 *
 * @return
 *      - ESP_OK on successful save
 *      - ESP_FAIL on any error (null pointer, duplicate IP, JSON creation failure,
 *        file write failure, etc.)
 */
int webserver_config_save_config_info_to_file(webserver_config_t *config)
{
    if (!config)
        return ESP_FAIL;
    if (check_ip_whitelist_duplicates(config) != ESP_OK)
        return ESP_FAIL;
    init_config_aes_key_iv(config);
    cJSON *root = build_config_json(config);
    if (!root)
        return ESP_FAIL;

    if (!little_file_system_write_json_to_file(root, CONFIG_FILE_NAME)) {
        return ESP_FAIL;
    }
    memcpy(&config_info, config, sizeof(webserver_config_t));
    ESP_LOGI(TAG, "Configuration saved successfully");
    return ESP_OK;
}

/**
 * @brief Parse the IP whitelist from a JSON object into the Config structure.
 *
 * Extracts the "ip_White_list" array from the provided cJSON root object and
 * populates the Config's ip_White_list array with the IP addresses. Ensures
 * each IP is a valid string and handles invalid entries by setting them to empty.
 *
 * @param root Pointer to the cJSON object containing the configuration data.
 * @param config Pointer to the Config structure to populate with IP whitelist.
 *
 * @return
 *   - ESP_OK on successful parsing.
 *   - ESP_FAIL if the IP whitelist array is missing or invalid.
 */
static esp_err_t parse_ip_whitelist(cJSON *root, webserver_config_t *config)
{
    cJSON *ip_list = cJSON_GetObjectItemCaseSensitive(root, "ip_White_list");
    if (!ip_list || !cJSON_IsArray(ip_list)) {
        ESP_LOGW(TAG, "Missing or invalid 'ip_White_list' array");
        return ESP_FAIL;
    }

    for (uint8_t i = 0; i < config->ip_count; i++) {
        cJSON *ip_item = cJSON_GetArrayItem(ip_list, i);
        if (ip_item && cJSON_IsString(ip_item) && ip_item->valuestring) {
            snprintf(config->ip_White_list[i], MAX_IP_LEN, "%s", ip_item->valuestring);
        }
        else {
            ESP_LOGW(TAG, "Invalid IP string at index %d", i);
            config->ip_White_list[i][0] = '\0';
        }
    }
    return ESP_OK;
}

/**
 * @brief Parse AES key and IV hexadecimal strings from JSON and convert to binary.
 *
 * Reads "aes_key" and "aes_iv" string fields from the JSON root object, copies
 * them into the Config structure, and converts the hex strings into binary key
 * and IV arrays for cryptographic use.
 *
 * @param root Pointer to the cJSON object containing the configuration data.
 * @param config Pointer to the Config structure to populate with AES keys.
 *
 * @return
 *   - ESP_OK on successful parsing and conversion.
 *   - ESP_FAIL if either "aes_key" or "aes_iv" fields are missing or invalid.
 */
static esp_err_t parse_aes_keys(cJSON *root, webserver_config_t *config)
{
    char *aes_key_hex = NULL;
    char *aes_iv_hex = NULL;
    if (!cjson_parse_string_field(root, "aes_key", &aes_key_hex) || !aes_key_hex) {
        ESP_LOGE(TAG, "Missing or invalid 'aes_key'");
        return ESP_FAIL;
    }
    if (!cjson_parse_string_field(root, "aes_iv", &aes_iv_hex) || !aes_iv_hex) {
        ESP_LOGE(TAG, "Missing or invalid 'aes_iv'");
        return ESP_FAIL;
    }

    strncpy((char *)config->aes_key_hex, aes_key_hex, AES_KEY_HEX_SIZE - 1);
    config->aes_key_hex[AES_KEY_HEX_SIZE - 1] = '\0';

    strncpy((char *)config->aes_iv_hex, aes_iv_hex, AES_IV_HEX_SIZE - 1);
    config->aes_iv_hex[AES_IV_HEX_SIZE - 1] = '\0';

    for (int i = 0; i < 16; i++) {
        sscanf(config->aes_key_hex + 2 * i, "%2hhx", &config->aes_key[i]);
        sscanf(config->aes_iv_hex + 2 * i, "%2hhx", &config->aes_iv[i]);
    }
    return ESP_OK;
}

/**
 * @brief Decode a base64-encoded and AES-encrypted password from JSON and decrypt it.
 *
 * Retrieves the base64-encoded password string from the JSON field specified by key,
 * decodes it to binary, and decrypts it using the provided AES key and IV. The
 * decrypted password is stored in out_password.
 *
 * @param root Pointer to the cJSON object containing the configuration data.
 * @param key The JSON field name of the encoded password (e.g., "web_password").
 * @param out_password Buffer to receive the decrypted password.
 * @param aes_key AES key for decryption.
 * @param aes_iv AES IV for decryption.
 *
 * @return
 *   - ESP_OK on successful decoding and decryption.
 *   - ESP_FAIL on missing field, decoding failure, or other errors.
 */
static esp_err_t decode_and_decrypt_password(cJSON *root, const char *key, unsigned char *out_password,
                                             unsigned char *aes_key, unsigned char *aes_iv)
{
    char *encoded = NULL;
    unsigned char encrypted[64] = {0};
    size_t decoded_len = 0;

    if (!cjson_parse_string_field(root, key, &encoded) || !encoded) {
        ESP_LOGE(TAG, "Missing or invalid '%s'", key);
        return ESP_FAIL;
    }

    if (mbedtls_base64_decode(encrypted, sizeof(encrypted), &decoded_len, (const unsigned char *)encoded,
                              strlen(encoded)) != 0) {
        ESP_LOGE(TAG, "Base64 decode failed for '%s'", key);
        return ESP_FAIL;
    }

    decryptAes(encrypted, decoded_len, out_password, aes_key, aes_iv);
    return ESP_OK;
}

/**
 * @brief Load configuration information from the config JSON file into the Config structure.
 *
 * This function reads the configuration file named CONFIG_FILE_NAME, parses the JSON content,
 * and fills the provided Config structure with data including whitelist IPs, AES keys,
 * and encrypted passwords (which are decrypted in the process).
 * It also validates fields and provides default values for missing or invalid entries.
 *
 * @param config Pointer to the Config structure to be populated with configuration data.
 *
 * @return
 *   - ESP_OK if configuration is successfully loaded and parsed.
 *   - ESP_FAIL if file reading, JSON parsing, or any critical data extraction fails.
 */
int webserver_config_load_config_info_from_file(webserver_config_t *config)
{
    cJSON *root = little_file_system_read_file_to_json(CONFIG_FILE_NAME);
    if (!root) {
        return ESP_FAIL;
    }

    config->enabled = false;
    cjson_parse_bool_field(root, "enabled", &config->enabled);
    int ip_count = 0;
    if (cjson_parse_int_field(root, "ip_count", &ip_count)) {
        config->ip_count = (ip_count > MAX_IP_COUNT) ? MAX_IP_COUNT : ip_count;
    }
    else {
        ESP_LOGW(TAG, "Missing or invalid 'ip_count' field, defaulting to 0");
        config->ip_count = 0;
    }

    if (parse_ip_whitelist(root, config) != ESP_OK) {
        for (int i = 0; i < MAX_IP_COUNT; i++)
            config->ip_White_list[i][0] = '\0';
        config->ip_count = 0;
    }

    if (parse_aes_keys(root, config) != ESP_OK) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    if (decode_and_decrypt_password(root, "web_password", (unsigned char *)config->web_password,
                                    (unsigned char *)config->aes_key, (unsigned char *)config->aes_iv) != ESP_OK) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Configuration information loaded successfully");
    return ESP_OK;
}

/**
 * @brief Initialize the theme mode to default values.
 *
 * Sets the default theme mode to "dark" and disables following the system theme.
 */
static void webserver_config_init_theme_mode(void)
{
    COPY_FIELD(device_info.theme_mode.current_mode, "dark");
    device_info.theme_mode.follow_system = false;
}

/**
 * @brief Generate a default product name based on device MAC address.
 *
 * @param[out] out Buffer to store the generated product name string.
 * @param[in] len Length of the output buffer.
 */
void webserver_config_get_default_product_name(char *out, size_t len)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, len, DEFAULT_DEVICE_NAME_PREFIX "_%02X%02X", mac[4], mac[5]);
}

/**
 * @brief Initialize product-related device information.
 *
 * Generates a default product name and reads the product ID,
 * sets the default language to English.
 */
static void webserver_config_init_product_info(void)
{
    webserver_config_get_default_product_name(device_info.product_name, sizeof(device_info.product_name));
    COPY_FIELD(device_info.language, "en");
    device_info.brightness = 50;
    device_info.idle_time = 300;

}

/**
 * @brief Initialize device information from stored file or defaults.
 *
 * Attempts to load device info from file; if fails, initializes defaults and saves them.
 * Ensures product ID is set after loading.
 */
static void webserver_config_device_info_init(void)
{
    if (webserver_config_load_device_info_from_file(&device_info) != ESP_OK) {
        webserver_config_init_theme_mode();
        webserver_config_init_product_info();

        if (webserver_config_save_device_info_to_file(&device_info) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save default configuration");
            return;
        }
        ESP_LOGI(TAG, "Device information initialization");
    }
    // read_product_id(product_id, sizeof(product_id));
    if (product_id[0] == '\0') {
        ESP_LOGE(TAG, "Product ID is empty");
    }
    else {
        ESP_LOGI(TAG, "Product ID: %s", product_id);
    }
    webserver_config_save_device_info_to_file(&device_info);
}

/**
 * @brief Generate random AES key and initialization vector for the config.
 *
 * Fills the AES key and IV fields in the Config structure with cryptographically
 * secure random bytes.
 *
 * @param[out] config Pointer to the Config structure to populate with random key and IV.
 */
static void webserver_config_generate_random_key_and_iv(webserver_config_t *config)
{
    esp_fill_random(config->aes_key, AES_KEY_SIZE);
    esp_fill_random(config->aes_iv, AES_IV_SIZE);
    config->aes_key[AES_KEY_SIZE - 1] = '\0';
    config->aes_iv[AES_IV_SIZE - 1] = '\0';
    //ESP_LOGW(TAG, "aes_key:[%s] aes_iv:[%s] ", config->aes_key, config->aes_iv);
}

/**
 * @brief Apply default configuration values if loading configuration fails.
 *
 * Sets default flags, clears IP whitelist and passwords,
 * generates random AES key and IV, configures the device as an AP,
 * and saves the default configuration to persistent storage.
 */
static void webserver_config_apply_default_config()
{
    webserver_config_t default_cfg = {0};
    default_cfg.enabled = false;
    default_cfg.ip_count = 0;

    for (int i = 0; i < MAX_IP_COUNT; i++) {
        memset(default_cfg.ip_White_list[i], 0, MAX_IP_LEN);
    }

    memset(default_cfg.web_password, 0, sizeof(default_cfg.web_password));
    webserver_config_generate_random_key_and_iv(&default_cfg);
    ESP_LOGW(TAG, "No configuration file found. Generated random key and IV.");

    if (webserver_config_save_config_info_to_file(&default_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save default configuration");
    }
    memcpy(&config_info, &default_cfg, sizeof(webserver_config_t));
}

/**
 * @brief Initialize the webserver configuration.
 *
 * Tries to load existing configuration from file; if unsuccessful,
 * applies default configuration settings. Then initializes device info.
 */
void webserver_config_init()
{
    if (webserver_config_load_config_info_from_file(&config_info) != ESP_OK) {
        webserver_config_apply_default_config();
    }
    else {
        ESP_LOGI(TAG, "Successfully loaded security configuration");
    }
    webserver_config_device_info_init();

    //ESP_LOGI(TAG, "config_info.aes_key:[%s] config_info.aes_iv:[%s] ", config_info.aes_key_hex, config_info.aes_iv_hex);
    //ESP_LOGI(TAG, "config_info.web_password:[%s]", config_info.web_password);
    ESP_LOGI(TAG, "Security configuration initialized");
}
