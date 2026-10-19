#include <esp_mac.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mdns_init.h"
#include <esp_wifi_types.h>
#include "esp_netif.h"
#include "freertos/idf_additions.h"
#include "network.h"
#include "network_api.h"
#include "webserver_config_api.h"

static const char *TAG = "mdns_init";

static TaskHandle_t mdns_task_handle = NULL;
static TaskHandle_t s_mdns_hostname_task = NULL;
static bool s_mdns_server_started = false;
static volatile bool s_mdns_scan_paused = false;

EXT_RAM_BSS_ATTR char mdns_current_ip[MDNS_IP_LEN] = {0};
EXT_RAM_BSS_ATTR char mdns_custom_hostname[HOSTNAME_MAX_LEN] = {0};
EXT_RAM_BSS_ATTR mdns_scan_result *mdns_current_scan_results = NULL;
EXT_RAM_BSS_ATTR int16_t mdns_current_count = 0;
EXT_RAM_BSS_ATTR mdns_scan_result *mdns_prev_scan_results = NULL;
EXT_RAM_BSS_ATTR int16_t mdns_prev_count = 0;

/**
 * @brief Retrieves the last two bytes of the device's Wi-Fi MAC address.
 *
 * This function reads the station (STA) MAC address of the device using the
 * esp_read_mac() API and returns a 16-bit value composed of the last two bytes
 * (mac[4] and mac[5]). The returned value is useful for generating unique
 * identifiers for devices on a network.
 *
 * @return A 16-bit unsigned integer combining the 5th and 6th bytes of the MAC address.
 */
static uint16_t get_mac_address(void)
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        ESP_LOGE(TAG, "read Wi-Fi MAC address failed");
        return 0;
    }
    return ((uint16_t)mac[4] << 8) | mac[5];
}

static const char *mdns_instance_prefix_for_mode(wireless_controller_work_mode_t mode)
{
    switch (mode) {
    case WIRELESS_CONTROLLER_WORK_MODE_COOR:
        return INSTANCE_NAME_PREFIX;
    case WIRELESS_CONTROLLER_WORK_MODE_ROUTER:
        return INSTANCE_NAME_ROUTER_PREFIX;
    case WIRELESS_CONTROLLER_WORK_MODE_THREAD:
        return INSTANCE_NAME_THREAD_PREFIX;
    case WIRELESS_CONTROLLER_WORK_MODE_CUSTOM:
    case WIRELESS_CONTROLLER_WORK_MODE_INVALID:
    default:
        return INSTANCE_NAME_CUSTOM_PREFIX;
    }
}

static void mdns_build_instance_name(wireless_controller_work_mode_t mode, char *out, size_t out_len)
{
    snprintf(out, out_len, "%s_%04X", mdns_instance_prefix_for_mode(mode), (unsigned int)get_mac_address());
}

/**
 * @brief Initializes and starts the mDNS server with configured services.
 *
 * This function sets up the mDNS (Multicast DNS) server for the device,
 * creating a unique instance name by appending a portion of the device's MAC
 * address to a mode-specific prefix. It registers the HTTP service here;
 * the ZHA discovery service is then synchronized according to the firmware mode.
 *
 * The Zigbee coordinator service is never registered for Router, OpenThread,
 * Custom, or Invalid modes.
 *
 * This allows the device to be discoverable over the local network using
 * standard mDNS queries without requiring a central DNS server.
 *
 * Dependencies:
 * - Assumes that `get_mac_address()`, `getProductName()`, and `getProductId()` are defined elsewhere.
 * - Uses the ESP-IDF mDNS API.
 */
static esp_err_t mdns_start_server(wireless_controller_work_mode_t mode)
{
    ESP_LOGI(TAG, "start broadcast mdns server");
    char instance_name[64] = {0};
    mdns_build_instance_name(mode, instance_name, sizeof(instance_name));
    esp_err_t err = mdns_instance_name_set(instance_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set initial mDNS instance name failed: %s", esp_err_to_name(err));
        return err;
    }

    device_info_t *device_info = webserver_config_get_device_info();

    char mac_id[8] = {0};
    snprintf(mac_id, sizeof(mac_id), "%04X", (unsigned int)get_mac_address());
    /* txt configuration for the http service */
    mdns_txt_item_t http_txt[] = {
        {"name", device_info->product_name},
        {"id", mac_id},
    };
    err = mdns_service_add(instance_name, "_http", "_tcp", 80, http_txt, 2);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add HTTP mDNS service failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "mDNS HTTP service registered: instance=%s port=80", instance_name);
    }
    return err;
}

esp_err_t mdns_server_sync_device_mode(wireless_controller_work_mode_t mode, uint32_t baud_rate)
{
    if (!s_mdns_server_started) {
        return ESP_ERR_INVALID_STATE;
    }

    char instance_name[64] = {0};
    mdns_build_instance_name(mode, instance_name, sizeof(instance_name));

    esp_err_t err = mdns_instance_name_set(instance_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set mDNS instance name failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_service_instance_name_set("_http", "_tcp", instance_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set HTTP mDNS instance name failed: %s", esp_err_to_name(err));
        return err;
    }

    bool coordinator = mode == WIRELESS_CONTROLLER_WORK_MODE_COOR;
    bool zha_service_exists = mdns_service_exists("_zigbee-coordinator", "_tcp", NULL);
    if (!coordinator) {
        if (zha_service_exists) {
            err = mdns_service_remove("_zigbee-coordinator", "_tcp");
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "remove ZHA discovery service failed: %s", esp_err_to_name(err));
                return err;
            }
        }
        ESP_LOGI(TAG, "mDNS instance set to %s; ZHA discovery disabled", instance_name);
        return ESP_OK;
    }

    if (baud_rate == 0) {
        ESP_LOGE(TAG, "invalid Zigbee coordinator baud rate");
        return ESP_ERR_INVALID_ARG;
    }

    char baud_str[16] = {0};
    snprintf(baud_str, sizeof(baud_str), "%lu", (unsigned long)baud_rate);

    mdns_txt_item_t zigbee_txt[] = {
        {"radio_type", "znp"},
        {"baud_rate", baud_str},
        {"serial_number", webserver_config_get_product_id()},
    };

    if (zha_service_exists) {
        err = mdns_service_instance_name_set("_zigbee-coordinator", "_tcp", instance_name);
        if (err == ESP_OK) {
            err = mdns_service_txt_set("_zigbee-coordinator", "_tcp", zigbee_txt, 3);
        }
    }
    else {
        err = mdns_service_add(instance_name, "_zigbee-coordinator", "_tcp", 6638, zigbee_txt, 3);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "update ZHA discovery service failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "mDNS instance set to %s; ZHA discovery enabled at %lu baud", instance_name,
             (unsigned long)baud_rate);
    return ESP_OK;
}

/**
 * @brief Compares two arrays of mDNS scan results for equality.
 *
 * This function checks whether two arrays of `mdns_scan_result` structures
 * contain identical device information. It compares each corresponding
 * element's `txt_id`, `txt_username`, `hostname`, and `ip_addr` strings.
 *
 * If both arrays are NULL or if the count is zero, the function considers them equal.
 * If either array is NULL while the other is not, or if any device information differs,
 * the function returns 0 (not equal).
 *
 * @param prev    Pointer to the first array of scan results (previous results).
 * @param current Pointer to the second array of scan results (current results).
 * @param count   Number of elements in each array to compare.
 *
 * @return int
 *         1 if both arrays are equal (or both NULL/empty),
 *         0 if they differ in any device entry or one is NULL while the other is not.
 */
static int mdns_compare_scan_results(mdns_scan_result *prev, mdns_scan_result *current, uint8_t count)
{
    if (prev == NULL && current == NULL)
        return 1;
    if (prev == NULL || current == NULL)
        return 0;
    if (count == 0)
        return 1;

    for (int i = 0; i < count; i++) {
        if (strcmp(prev[i].txt_id, current[i].txt_id) != 0 ||
            strcmp(prev[i].txt_username, current[i].txt_username) != 0 ||
            strcmp(prev[i].hostname, current[i].hostname) != 0 || strcmp(prev[i].ip_addr, current[i].ip_addr) != 0) {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief Parses the mDNS TXT records to extract device ID and name.
 *
 * This function iterates through the TXT records in the given `mdns_result_t`
 * structure and copies the values of the keys "id" and "name" into the provided
 * buffers `id_buf` and `name_buf`, respectively.
 *
 * Only non-null values are copied, and the buffers are limited to
 * `MDNS_ID_BUF_LEN - 1` and `MDNS_NAME_BUF_LEN - 1` characters to ensure safety.
 *
 * @param result   Pointer to the mDNS query result containing TXT records.
 * @param id_buf   Buffer to store the extracted device ID string.
 * @param name_buf Buffer to store the extracted device name string.
 *
 * @return bool
 *         true  - if an "id" value was found and copied successfully.
 *         false - if no "id" key was found in the TXT records.
 */
static bool mdns_parse_mdns_txt(mdns_result_t *result, char *id_buf, char *name_buf)
{
    for (int i = 0; i < result->txt_count; i++) {
        mdns_txt_item_t *txt = &result->txt[i];
        if (strcmp(txt->key, "id") == 0 && txt->value) {
            strncpy(id_buf, txt->value, MDNS_ID_BUF_LEN - 1);
        }
        else if (strcmp(txt->key, "name") == 0 && txt->value) {
            strncpy(name_buf, txt->value, MDNS_NAME_BUF_LEN - 1);
        }
    }
    return id_buf[0] != '\0';
}

/**
 * @brief Checks if a given TXT ID already exists in the mDNS scan results.
 *
 * This function scans through an array of `mdns_scan_result` structures to
 * determine if any entry has a `txt_id` matching the provided `txt_id` string.
 *
 * @param results Pointer to an array of mDNS scan results.
 * @param count   Number of elements in the `results` array.
 * @param txt_id  The TXT ID string to search for.
 *
 * @return bool
 *         true  - if a duplicate TXT ID is found in the results.
 *         false - if no matching TXT ID exists in the results.
 */
static bool mdns_is_duplicate_result(const mdns_scan_result *results, uint8_t count, const char *txt_id)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(results[i].txt_id, txt_id) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Extracts the IPv4 address from an mDNS query result.
 *
 * This function iterates through the linked list of IP addresses in the given
 * `mdns_result_t` structure and copies the first found IPv4 address as a string
 * into the provided `ip_buf`. If no IPv4 address is found, it sets `ip_buf`
 * to "0.0.0.0".
 *
 * @param result  Pointer to the mDNS query result containing IP addresses.
 * @param ip_buf  Buffer to store the extracted IPv4 address as a null-terminated string.
 */
static void mdns_extract_ip_address(mdns_result_t *result, char *ip_buf)
{
    mdns_ip_addr_t *addr = result->addr;
    while (addr) {
        if (addr->addr.type == IPADDR_TYPE_V4) {
            esp_ip4addr_ntoa(&(addr->addr.u_addr.ip4), ip_buf, MDNS_IP_LEN);
            return;
        }
        addr = addr->next;
    }
    snprintf(ip_buf, MDNS_IP_LEN, "0.0.0.0");
}

static int mdns_result_cmp_by_username(const void *a, const void *b)
{
    const mdns_scan_result *r1 = (const mdns_scan_result *)a;
    const mdns_scan_result *r2 = (const mdns_scan_result *)b;

    return strcmp(r1->txt_username, r2->txt_username);
}

/**
 * @brief Performs an mDNS scan to discover devices advertising the HTTP service.
 *
 * This function queries the local network for mDNS services of type "_http._tcp"
 * within a specified scan time and collects devices whose instance names start
 * with the defined prefix `INSTANCE_NAME_PREFIX`.
 *
 * It allocates and fills an array of `mdns_scan_result` structures with unique
 * devices found, extracting their TXT records (id and name), hostname, and IPv4 address.
 *
 * Duplicate devices (based on `txt_id`) are ignored. The caller is responsible for
 * freeing the allocated array pointed to by `get_result`.
 *
 * A log message is periodically printed if no devices are found or the query fails.
 *
 * @param get_result Output pointer to the allocated array of scan results.
 *
 * @return uint8_t
 *         Number of valid unique devices found and stored in `get_result`.
 *         Returns 0 if no devices are found, on failure, or memory allocation error.
 */
static uint8_t mdns_open_nextion_scan(mdns_scan_result **get_result)
{
    mdns_result_t *results = NULL;
    static uint8_t log_silence_counter = 0;
    uint8_t valid_count = 0;

    esp_err_t err = mdns_query_ptr("_http", "_tcp", MDNS_SCAN_TIME_MS, 20, &results);
    if (err || !results) {
        if (++log_silence_counter % 60 == 0) {
            ESP_LOGI(TAG, "mDNS scan: No device found or query failed");
        }
        return 0;
    }

    log_silence_counter = 0;

    uint8_t total = 0;
    for (mdns_result_t *r = results; r; r = r->next) {
        if (strncmp(r->instance_name, INSTANCE_NAME_PREFIX, strlen(INSTANCE_NAME_PREFIX)) == 0) {
            total++;
        }
    }

    if (total == 0) {
        mdns_query_results_free(results);
        return 0;
    }

    *get_result = (mdns_scan_result *)heap_caps_calloc(total,
                                                       sizeof(mdns_scan_result),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!(*get_result)) {
        ESP_LOGW(TAG, "PSRAM scan result allocation failed; using internal RAM");
        *get_result = (mdns_scan_result *)calloc(total, sizeof(mdns_scan_result));
    }
    if (!(*get_result)) {
        ESP_LOGE(TAG, "Memory allocation failed");
        mdns_query_results_free(results);
        return 0;
    }

    for (mdns_result_t *r = results; r; r = r->next) {
        char txt_id[MDNS_ID_BUF_LEN] = {0};
        char txt_name[MDNS_NAME_BUF_LEN] = {0};
        if (strncmp(r->instance_name, INSTANCE_NAME_PREFIX, strlen(INSTANCE_NAME_PREFIX)) != 0)
            continue;
        mdns_parse_mdns_txt(r, txt_id, txt_name);
        if (txt_id[0] == '\0') {
            const char *suffix = strrchr(r->instance_name, '_');
            if (suffix) {
                strncpy(txt_id, suffix + 1, MDNS_ID_BUF_LEN - 1);
            } else {
                strncpy(txt_id, r->hostname, MDNS_ID_BUF_LEN - 1);
            }
        }
        if (mdns_is_duplicate_result(*get_result, valid_count, txt_id)) {
            continue;
        }

        snprintf((*get_result)[valid_count].txt_id, MDNS_ID_BUF_LEN, "%s", txt_id);
        snprintf((*get_result)[valid_count].txt_username, MDNS_NAME_BUF_LEN, "%s", txt_name);
        snprintf((*get_result)[valid_count].hostname, MDNS_NAME_BUF_LEN, "%s", r->hostname);
        mdns_extract_ip_address(r, (*get_result)[valid_count].ip_addr);
        valid_count++;
    }

    mdns_query_results_free(results);

    if (valid_count > 1) {
        qsort(*get_result, valid_count, sizeof(mdns_scan_result), mdns_result_cmp_by_username);
    }
    return valid_count;
}

/**
 * @brief Task that periodically performs mDNS scanning and updates scan results.
 *
 * This FreeRTOS task continuously waits for the network connection (Ethernet or Wi-Fi)
 * to be established before initiating an mDNS scan. It calls `mdns_open_nextion_scan()` to
 * retrieve the current list of devices advertising the HTTP service.
 *
 * The task compares the new scan results with the previous results using
 * `mdns_compare_scan_results()`. If changes are detected (difference in count or content),
 * it updates the cached results and notifies the webserver via
 * `webserver_cur_mdns_scan_result_notify()`.
 *
 * If no changes are found, it frees the current scan results without updating.
 *
 * The task repeats this scanning process after a delay defined by `MDNS_SCAN_INTERVAL_MS`.
 * It logs a warning if the network is unavailable for extended periods.
 *
 * @param param Unused parameter.
 */
static void mdns_scan_task(void *param)
{
    int network_cnt = 0;
    NetworkState *net = NULL;

    while (true) {
        if (s_mdns_scan_paused) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        /* wait for the network to connect */
        while (true) {
            net = network_get_state();
            if (net && (net->eth_connected || net->sta_connected_flag)) {
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(2000));
            if (++network_cnt % 30 == 0) {
                ESP_LOGW(TAG, "No network available for mDNS scan");
            }
        }

        if (s_mdns_scan_paused) {
            continue;
        }

        /* scan and compare the results */
        mdns_current_count = mdns_open_nextion_scan(&mdns_current_scan_results);

        bool changed =
            (mdns_current_count != mdns_prev_count) ||
            !mdns_compare_scan_results(mdns_prev_scan_results, mdns_current_scan_results, mdns_current_count);

        if (changed) {
            ESP_LOGI(TAG, "mDNS scan results changed (%d)", mdns_current_count);
            /* replace the old results */
            free(mdns_prev_scan_results);
            mdns_prev_scan_results = mdns_current_scan_results;
            mdns_prev_count = mdns_current_count;
            webserver_cur_mdns_scan_result_notify();
        }
        else {
            free(mdns_current_scan_results);
        }

        mdns_current_scan_results = NULL;
        vTaskDelay(pdMS_TO_TICKS(MDNS_SCAN_INTERVAL_MS));
    }
}

/**
 * @brief Starts the mDNS scan task if it is not already running.
 *
 * This function creates the FreeRTOS task responsible for performing periodic
 * mDNS scans (`mdns_scan_task`). It prevents multiple instances of the task
 * by checking if the task handle is already set.
 *
 * @return int8_t
 *         0  - if the task was successfully created.
 *         -1 - if the task is already running or creation failed.
 */
static int8_t mdns_scan_task_start(void)
{
    if (mdns_task_handle != NULL)
        return -1;

    bool stack_in_psram = true;
    int ret = xTaskCreatePinnedToCoreWithCaps(mdns_scan_task,
                                              "MDNS_SCAN",
                                              3072,
                                              NULL,
                                              10,
                                              &mdns_task_handle,
                                              1,
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (ret != pdPASS) {
        stack_in_psram = false;
        ESP_LOGW(TAG, "PSRAM task stack allocation failed; using internal RAM");
        ret = xTaskCreatePinnedToCore(mdns_scan_task,
                                      "MDNS_SCAN",
                                      3072,
                                      NULL,
                                      10,
                                      &mdns_task_handle,
                                      1);
    }

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "task create fail");
        return -1;
    }
    ESP_LOGI(TAG,
             "mDNS scan task started with %s stack",
             stack_in_psram ? "PSRAM" : "internal");
    return 0;
}

void mdns_scan_set_paused(bool paused)
{
    if (s_mdns_scan_paused == paused) {
        return;
    }
    s_mdns_scan_paused = paused;
    ESP_LOGI(TAG, "Periodic mDNS scanning %s", paused ? "paused" : "resumed");
}

static void mdns_hostname_monitor_task(void *arg)
{
    (void)arg;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (mdns_current_ip[0] != '\0') {
            mdns_verify_hostname();
        }
    }
}

static void mdns_hostname_monitor_start(void)
{
    if (s_mdns_hostname_task != NULL) {
        return;
    }
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(mdns_hostname_monitor_task,
                                                     "MDNS_NAME",
                                                     3072,
                                                     NULL,
                                                     4,
                                                     &s_mdns_hostname_task,
                                                     1,
                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "PSRAM mDNS hostname monitor allocation failed; using internal RAM");
        ret = xTaskCreatePinnedToCore(mdns_hostname_monitor_task,
                                      "MDNS_NAME",
                                      3072,
                                      NULL,
                                      4,
                                      &s_mdns_hostname_task,
                                      1);
    }
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to start mDNS hostname monitor");
    }
}

/**
 * @brief Event handler for network IP address changes.
 *
 * This function handles the event when the device obtains a new IPv4 address.
 * It compares the new IP address with the currently stored one. If the address
 * has changed, it logs the new IP and either starts the mDNS server for the first time
 * or updates the existing mDNS server's hostname configuration.
 *
 * @param arg         User-defined argument (unused).
 * @param event_base  The event base (expected to be IP event).
 * @param event_id    The event ID indicating the type of IP event.
 * @param event_data  Pointer to event-specific data containing the new IP info.
 */
static void mdns_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char tmp_ip[MDNS_IP_LEN] = {0};
        sprintf(tmp_ip, IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "mDNS got-IP: event=%ld ip=%s previous_ip=%s netif=%p up=%d",
                 (long)event_id, tmp_ip, mdns_current_ip, (void *)event->esp_netif,
                 event->esp_netif ? (int)esp_netif_is_netif_up(event->esp_netif) : -1);
        if (tmp_ip[0] != '\0' && strcmp(mdns_current_ip, tmp_ip) != 0) {
            ESP_LOGI(TAG, "IPv4 change address: %s", tmp_ip);
            mdns_select_hostname_for_network();
            sprintf(mdns_current_ip, IPSTR, IP2STR(&event->ip_info.ip));
            char active_hostname[HOSTNAME_MAX_LEN] = {0};
            esp_err_t hostname_err = mdns_hostname_get(active_hostname);
            ESP_LOGI(TAG, "mDNS active identity: hostname=%s.local ip=%s hostname_get=%s",
                     active_hostname, mdns_current_ip, esp_err_to_name(hostname_err));
            /* The network got-IP handler may have already sent its incremental
             * status before conflict avoidance selected the final hostname.
             * Publish again so provisioning clients receive the hostname that
             * is actually being advertised. */
            webserver_network_info_notify();
        }
    }
}

/**
 * @brief Initialize the mDNS server and related services.
 *
 * This function initializes the mDNS service, loads the configuration from
 * a file, registers event handlers to respond to IP address acquisition
 * on both Wi-Fi and Ethernet interfaces, and starts the mDNS scan task.
 *
 * @param mode Current wireless controller firmware mode.
 * @param baud_rate Current wireless controller UART baud rate.
 *
 * @return int
 *         0 on successful initialization, -1 on failure.
 */
int mdns_server_init(wireless_controller_work_mode_t mode, uint32_t baud_rate)
{
    mdns_load_config_from_file();
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialize mDNS failed: %s", esp_err_to_name(err));
        return -1;
    }
    err = mdns_apply_fallback_hostname();
    if (err != ESP_OK) {
        return -1;
    }
    err = mdns_start_server(mode);
    if (err != ESP_OK) {
        return -1;
    }
    s_mdns_server_started = true;
    err = mdns_server_sync_device_mode(mode, baud_rate);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "configure mDNS services failed: %s", esp_err_to_name(err));
        return -1;
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &mdns_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &mdns_event_handler, NULL));
    char startup_hostname[HOSTNAME_MAX_LEN] = {0};
    mdns_hostname_get(startup_hostname);
    ESP_LOGI(TAG, "mDNS initialized: startup=%s.local requested=%s.local; waiting for STA/ETH IP",
             startup_hostname, mdns_custom_hostname[0] ? mdns_custom_hostname : DEFAULTS_HOST_NAME);
    mdns_hostname_monitor_start();
    mdns_scan_task_start();
    return 0;
}
