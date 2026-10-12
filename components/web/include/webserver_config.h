#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_H
#define OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_H

#include "webserver_method_api.h"

/** @brief Configuration file name */
#define CONFIG_FILE_NAME "config.json"

/** @brief Device info file name */
#define DEVICE_INFO_FILE_NAME "device_info.json"

/** @brief Prefix used when generating the default device display name */
#define DEFAULT_DEVICE_NAME_PREFIX "Nextion"

/** @brief Maximum number of whitelisted IP addresses */
#define MAX_IP_COUNT 5

/** @brief Maximum length of a single IP address string (including null-terminator) */
#define MAX_IP_LEN 16

/** @brief Maximum length for passwords */
#define MAX_PASSWORD_LEN 64

/** @brief Buffer size for Base64-encoded data */
#define BASE64_BUFFER_SIZE (((MAX_PASSWORD_LEN + 2) / 3 * 4) + 1)

/** @brief AES key length in bytes */
#define AES_KEY_SIZE 16

/** @brief AES IV length in bytes */
#define AES_IV_SIZE 16

/** @brief AES key length in hexadecimal string format (including null terminator) */
#define AES_KEY_HEX_SIZE 33

/** @brief AES IV length in hexadecimal string format (including null terminator) */
#define AES_IV_HEX_SIZE 33

/** @brief Error code: duplicate IP found in whitelist */
#define WEB_API_ERROR_WHITE_LIST_IP_ALREADY_EXIST (907)

/**
 * @brief Theme mode configuration.
 */
typedef struct
{
    char current_mode[7]; /* Current theme mode (e.g., "light", "dark") */
    bool follow_system;   /* Whether to follow system theme settings */
} theme_mode_t;

/**
 * @brief Device identity and theme configuration.
 */
typedef struct
{
    theme_mode_t theme_mode; /* UI theme configuration */
    char product_name[31];   /* Product name (max 30 characters) */
    char language[5];        /* Language code (e.g., \"en\", \"zh\") */
    uint8_t brightness;       /* Current screen brightness (0-100) */
    uint32_t idle_time;     /* Screen lock idle time (seconds) */
} device_info_t;

typedef struct
{
    bool enable;
    uint8_t brightness;
    char from[10]; /* HH:MM format */
    char to[10];   /* HH:MM format */
    bool suppressed_this_period;     /* Whether to suppress this period */
} night_mode_config_t;

/**
 * @brief Configuration structure including security and whitelist info.
 */
typedef struct
{
    bool enabled;                                 /* Whether the whitelist is enabled */
    char ip_White_list[MAX_IP_COUNT][MAX_IP_LEN]; /* Array of whitelisted IP addresses */
    uint8_t ip_count;                             /* Number of whitelisted IPs */
    char web_password[MAX_PASSWORD_LEN];          /* Web login password */
    char aes_key[AES_KEY_SIZE];                   /* AES encryption key */
    char aes_iv[AES_IV_SIZE];                     /* AES initialization vector */
    char aes_key_hex[AES_KEY_HEX_SIZE];           /* Hex string of AES key */
    char aes_iv_hex[AES_IV_HEX_SIZE];             /* Hex string of AES IV */
} webserver_config_t;

/**
 * @brief Get the pointer to the product ID.
 *
 * @return Pointer to the DeviceInfo structure holding device information.
 */
char *webserver_config_get_product_id(void);

void webserver_config_get_default_product_name(char *out, size_t len);

/**
 * @brief Save device information to a JSON file.
 *
 * Serializes the given DeviceInfo structure into a JSON format and writes it to a file.
 *
 * @param info Pointer to the DeviceInfo structure containing device information to save.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL if the input is NULL, JSON creation fails, or file writing fails.
 */
int webserver_config_save_device_info_to_file(device_info_t *info);

/**
 * @brief Load device information from a JSON file.
 *
 * Parses the device information from a JSON file into the provided DeviceInfo structure.
 *
 * @param info Pointer to the DeviceInfo structure where the loaded data will be stored.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_FAIL if the input is NULL, file reading/parsing fails.
 */
int webserver_config_load_device_info_from_file(device_info_t *info);

/**
 * @brief Save the configuration information to a JSON file.
 *
 * Validates the IP whitelist to ensure no duplicates exist, converts AES key and IV
 * to hex strings, encrypts and base64-encodes the passwords, then serializes the
 * entire configuration to JSON and writes it to persistent storage.
 *
 * @param config Pointer to the configuration data to be saved.
 *
 * @return
 *   - ESP_OK on success.
 *   - ESP_FAIL on failure (invalid input, duplicate IP addresses, JSON errors, or write failure).
 */
int webserver_config_save_config_info_to_file(webserver_config_t *config);

/**
 * @brief Load configuration information from the JSON file.
 *
 * Reads and parses the configuration JSON file and populates the given Config structure.
 * Handles IP whitelist, AES keys, and password decryption.
 *
 * @param config Pointer to the Config structure to populate.
 *
 * @return
 *   - ESP_OK on success.
 *   - ESP_FAIL on failure to read file, parse JSON, or decode critical fields.
 */
int webserver_config_load_config_info_from_file(webserver_config_t *config);

/**
 * @brief Initialize the webserver configuration system.
 *
 * Attempts to load the security configuration from persistent storage.
 * If loading fails, applies default configuration values.
 * Also initializes the device information.
 */
void webserver_config_init();

#endif // OPEN_NEXTION_COMPONENTS_WEB_INCLUDE_WEBSERVER_CONFIG_H

#ifdef __cplusplus
}
#endif
