#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include "sys_time.h"
#include "sys_time_sync.h"
#include "littlefs.h"
#include "cjson_helpers.h"
#include "webserver_api.h"
#include "sys_time_api.h"
#include "webserver_config_api.h"

static const char *TAG = "sys_time";

ESP_EVENT_DEFINE_BASE(SYS_TIME_EVENT);

#define QUERY_TIME (60 * 1000)
#define QUERY_TZ_PERIOD_CNT (60 * 24 * 10)
#define DEFAULT_NTP_SERVER_MAIN "pool.ntp.org"
#define DEFAULT_NTP_SERVER_SPARE "time.google.com"

static EXT_RAM_BSS_ATTR char ntp_server_main[NTP_DOMAIN_MAX_LEN] = {0};
static EXT_RAM_BSS_ATTR char ntp_server_spare[NTP_DOMAIN_MAX_LEN] = {0};

static TimerHandle_t sync_dst_time_handle = NULL;
static uint32_t query_tz_period_cnt = QUERY_TZ_PERIOD_CNT;
/* Tracks whether esp_sntp_init() has already run, so a later IP event can use
 * esp_sntp_restart() instead of initializing the client a second time. */
static bool s_sntp_initialized = false;

uint32_t sys_time_start_time_s = 0;
timezone_info_t sys_time_timezone_info = {0};

/**
 * @brief Retrieve the configured NTP server names.
 *
 * Copies the main and spare NTP server names into the provided buffers.
 *
 * @param[out] main_name  Buffer to store the main NTP server name.
 * @param[out] spare_name Buffer to store the spare NTP server name.
 *
 * @return Returns 0 on success.
 */
int sys_time_get_ntp_server_name(char *main_name, char *spare_name)
{
    strcpy(main_name, ntp_server_main);
    strcpy(spare_name, ntp_server_spare);
    return 0;
}

/**
 * @brief Set the main and spare NTP server names.
 *
 * This function sets the NTP server hostnames used for time synchronization.
 * It validates the length of the provided server names and falls back to
 * default values if NULL pointers are given.
 *
 * @param[in] main_name   Pointer to the main NTP server name string. If NULL,
 *                       the default main server name is used.
 * @param[in] spare_name  Pointer to the spare NTP server name string. If NULL,
 *                       the default spare server name is used.
 *
 * @return
 *      - 0 on success.
 *      - -1 if the main server name length exceeds the maximum allowed length.
 *      - -2 if the spare server name length exceeds the maximum allowed length.
 */
static int sys_time_set_ntp_server_name(const char *main_name, const char *spare_name)
{
    if (main_name && strlen(main_name) >= NTP_DOMAIN_MAX_LEN) {
        ESP_LOGE(TAG, "main NTP server name too long");
        return -1;
    }

    if (spare_name && strlen(spare_name) >= NTP_DOMAIN_MAX_LEN) {
        ESP_LOGE(TAG, "spare NTP server name too long");
        return -2;
    }

    const char *main_to_set = (main_name != NULL) ? main_name : DEFAULT_NTP_SERVER_MAIN;
    strlcpy(ntp_server_main, main_to_set, NTP_DOMAIN_MAX_LEN);
    ESP_LOGI(TAG, "set ntp server1: %s", ntp_server_main);

    const char *spare_to_set = (spare_name != NULL) ? spare_name : DEFAULT_NTP_SERVER_SPARE;
    strlcpy(ntp_server_spare, spare_to_set, NTP_DOMAIN_MAX_LEN);
    ESP_LOGI(TAG, "set ntp server2: %s", ntp_server_spare);

    return 0;
}

/**
 * @brief Load time configuration information from a file.
 *
 * This function reads the time configuration JSON file, parses it, and updates
 * the system timezone and NTP server names accordingly. If the file cannot be
 * read or parsed, it falls back to setting the default NTP server names.
 *
 * @return
 *      - 0 on successful load and parsing of time configuration.
 *      - -1 if the configuration file cannot be read or JSON parsing fails.
 */
static int sys_time_load_time_info_from_file()
{
    cJSON *time_save_json = little_file_system_read_file_to_json(TIME_CONFIG_FILE_NAME);
    if (time_save_json == NULL) {
        sys_time_set_ntp_server_name(NULL, NULL);
        return -1;
    }

    char *timezone;
    if (cjson_parse_string_field(time_save_json, "timezone", &timezone)) {
        strlcpy(sys_time_timezone_info.timezone, timezone, TIMEZONE_MAX_LEN);
    }

    char *server1_str;
    char *server2_str;
    cjson_parse_string_field(time_save_json, "server1", &server1_str);
    cjson_parse_string_field(time_save_json, "server2", &server2_str);

    sys_time_set_ntp_server_name(server1_str, server2_str);
    cJSON_Delete(time_save_json);
    return 0;
}

/**
 * @brief Save current time configuration information to a file.
 *
 * This function creates a JSON object containing the current timezone and NTP server names,
 * converts it to a string, and writes it to the time configuration file. It manages memory
 * cleanup of the JSON object and string after writing.
 *
 * @return
 *      - 0 on successful write to file.
 *      - -1 if writing to the file fails.
 */
static int sys_time_save_time_info_to_file(void)
{
    int ret = ESP_FAIL;
    cJSON *time_save_json = cJSON_CreateObject();
    if (time_save_json == NULL) {
        return ret;
    }
    cJSON_AddStringToObject(time_save_json, "timezone", sys_time_timezone_info.timezone);
    cJSON_AddStringToObject(time_save_json, "server1", ntp_server_main);
    cJSON_AddStringToObject(time_save_json, "server2", ntp_server_spare);

    if (little_file_system_write_json_to_file(time_save_json, TIME_CONFIG_FILE_NAME)) {
        ret = ESP_OK;
    }

    return ret;
}

/**
 * @brief Handle the API request to set the timezone.
 *
 * This function extracts the "timeZone" field from the given JSON object.
 * It updates the global system timezone information, persists this info to a config file,
 * and starts a task to synchronize DST information from the network.
 * Finally, it sends a response back to the client indicating the result.
 *
 * @param data          Pointer to cJSON object containing the request data.
 * @param now_resp_arg  Pointer to async response argument used to send reply to client.
 *
 * @return
 *      - ESP_OK on success,
 *      - WEB_API_ERROR_FAIL if any step fails.
 */
esp_err_t method_api_handle_set_timezone(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error_code = WEB_API_ERROR_FAIL;
    char *timezone;
    if (!cjson_parse_string_field(data, "timeZone", &timezone)) {
        goto send_response;
    }
    strncpy(sys_time_timezone_info.timezone, timezone, TIMEZONE_MAX_LEN - 1);
    if (sys_time_save_time_info_to_file() != 0) {
        goto send_response;
    }
    error_code = sync_dst_info_from_network_task_start();

send_response:
    webserver_code_response_to_current_client(error_code, WEB_API_METHOD_SET_TIMEZONE_RSP, now_resp_arg->seq);
    return error_code;
}

/**
 * @brief Handle the API request to get timezone information.
 *
 * This function extracts the "timeZone" string from the provided JSON data,
 * then attempts to start an asynchronous task to query timezone information
 * from a server. If starting the task fails or the JSON parsing fails,
 * an error response is sent back to the client immediately.
 *
 * @param data          Pointer to cJSON object containing the request payload.
 * @param now_resp_arg  Pointer to async response argument for client communication.
 *
 * @return
 *      - ESP_OK if the timezone query task started successfully,
 *      - ESP_FAIL otherwise.
 */
esp_err_t method_api_handle_get_timezone(cJSON *data, async_resp_arg *now_resp_arg)
{
    int error_code = ESP_FAIL;
    char *timezone;
    if (!cjson_parse_string_field(data, "timeZone", &timezone)) {
        goto send_response;
    }
    error_code = sys_time_timezone_query_task_start(timezone, (uint64_t)now_resp_arg->seq);

send_response:
    if (error_code != ESP_OK) {
        webserver_code_response_to_current_client(error_code, WEB_API_METHOD_GET_TIMEZONE_RSP, now_resp_arg->seq);
    }
    return error_code;
}

/**
 * @brief Handle the API request to set NTP server names.
 *
 * This function extracts the NTP server names "server1" and "server2" from
 * the JSON data, sets them via sys_time_set_ntp_server_name, and saves the
 * configuration to persistent storage. Upon success, it sends a positive
 * response to the client and notifies the system of the NTP server update.
 * If any step fails, an error response is returned.
 *
 * @param data          Pointer to cJSON object containing the request payload.
 * @param now_resp_arg  Pointer to async response argument for client communication.
 *
 * @return
 *      - ESP_OK if the NTP servers are set and saved successfully,
 *      - ESP_FAIL otherwise.
 */
esp_err_t method_api_handle_set_ntp_server(cJSON *data, async_resp_arg *now_resp_arg)
{
    esp_err_t error_code = ESP_FAIL;
    char *server1_str;
    char *server2_str;
    if (!cjson_parse_string_field(data, "server1", &server1_str)) {
        goto send_response;
    }
    if (!cjson_parse_string_field(data, "server2", &server2_str)) {
        goto send_response;
    }

    int result = sys_time_set_ntp_server_name(server1_str, server2_str);
    if (result == 0) {
        error_code = sys_time_save_time_info_to_file();
    }
    else {
        error_code = WEB_API_ERROR_PARAM_LEN;
    }

send_response:
    webserver_code_response_to_current_client(error_code, WEB_API_METHOD_SET_NTP_SERVER_RSP, now_resp_arg->seq);
    if (error_code == ESP_OK) {
        webserver_device_info_notify(UPDATE_NTP_SERVER);
    }
    return error_code;
}

/**
 * @brief Check if the current time has reached or passed the next DST transition time.
 *
 * This function checks whether Daylight Saving Time (DST) is applicable and
 * compares the current UTC time with the stored next DST transition timestamp.
 *
 * @return
 *      - true if current time is equal or later than the next DST transition time,
 *      - false otherwise or if DST is not enabled.
 */
static bool sys_time_check_is_next_dst(void)
{
    if (sys_time_timezone_info.has_dst == false) {
        return false;
    }
    time_t sec_utc = time(NULL);
    if (sec_utc >= sys_time_timezone_info.next_dst) {
        return true;
    }
    return false;
}

/**
 * @brief Timer callback to periodically check and synchronize DST info.
 *
 * This timer callback increments the query period count and, when the count
 * reaches a threshold, resets it with some randomness and triggers a DST info
 * synchronization task if the next DST transition time has arrived. It also logs
 * the current local time and upcoming query timing.
 *
 * @param xTimer Timer handle provided by the FreeRTOS timer service.
 */
static void sys_time_sync_dst_timer(TimerHandle_t xTimer)
{
    query_tz_period_cnt++;
    if (query_tz_period_cnt >= QUERY_TZ_PERIOD_CNT) {
        query_tz_period_cnt = esp_random() % QUERY_TZ_PERIOD_CNT;
        if (sys_time_check_is_next_dst() == true) {
            ESP_LOGI(TAG, "start http query");
            sync_dst_info_from_network_task_start();
        }

        ESP_LOGI(TAG, "The query time has been reached, and the next query time will be in %ld hours",
                 ((QUERY_TZ_PERIOD_CNT - query_tz_period_cnt) / 60));
    }
    char time_str[30];
    sys_time_get_local_time_format(time_str);
    ESP_LOGI(TAG, "local time :%s", time_str);
}

/**
 * @brief SNTP synchronization notification callback.
 *
 * Called by the SNTP client when time synchronization succeeds. This callback
 * records the system start time relative to the synchronized timestamp, which
 * helps calculate system uptime accurately.
 *
 * @param tv Pointer to timeval structure representing the synchronized time (unused).
 */
static void sys_time_sntp_sync_notification_cb(struct timeval *tv)
{
    static bool sync_flag = false;
    if (sync_flag == false) {
        sync_flag = true;
        uint32_t sys_start_time = esp_log_early_timestamp();
        time_t timestamp;
        time(&timestamp);
        sys_time_start_time_s = timestamp - sys_start_time / 1000;
    }
    ESP_LOGI(TAG, "sntp_sync_notification_cb success");
    esp_err_t event_err = esp_event_post(SYS_TIME_EVENT, SYS_TIME_EVENT_SYNCED, NULL, 0, portMAX_DELAY);
    if (event_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post SYS_TIME_EVENT_SYNCED: %s", esp_err_to_name(event_err));
    }
}

/* The spare server belongs in lwIP's server table, and lwIP silently discards
 * any index >= SNTP_MAX_SERVERS. esp_sntp_setservername() returns void, so a
 * table that is too small cannot be reported at runtime -- the spare just stops
 * being used, with nothing in the log to show for it.
 *
 * Compiled only when this file is built (CONFIG_* is baked in at that point),
 * which makes it a stale-sdkconfig detector too: a tree whose sdkconfig still
 * carries the old value warns here. It is a warning rather than an error on
 * purpose, so a stale sdkconfig does not break anyone else's build. */
#if CONFIG_LWIP_SNTP_MAX_SERVERS < 2
#warning "sys_time configures a spare NTP server, but CONFIG_LWIP_SNTP_MAX_SERVERS < 2 drops it; delete sdkconfig to pick up sdkconfig.defaults"
#endif

/**
 * @brief Initialize the SNTP client with configured NTP servers.
 *
 * Configures the SNTP operating mode, sets the primary and secondary NTP server names,
 * registers the synchronization notification callback, and starts the SNTP service.
 */
static void sys_time_initialize_sntp(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ntp_server_main);
    esp_sntp_setservername(1, ntp_server_spare);
    sntp_set_time_sync_notification_cb(sys_time_sntp_sync_notification_cb);
    esp_sntp_init();
    s_sntp_initialized = true;
    ESP_LOGI(TAG, "sys_time sntp init success");
}

/**
 * @brief Reload time configuration from storage and start DST synchronization task.
 *
 * This function reads time and timezone info from the configuration file,
 * then triggers a synchronization task to update DST info from the network.
 */
void sys_time_reload_time_info(void)
{
    sys_time_load_time_info_from_file();
    sync_dst_info_from_network_task_start();
}

/**
 * @brief Initialize system time service.
 *
 * This function reloads the saved time info, initializes the SNTP client,
 * and creates a periodic timer to check and synchronize DST information.
 * It logs an error if the DST query timer creation fails.
 */
void sys_time_init(void)
{
    sys_time_reload_time_info();
    sys_time_initialize_sntp();
    if (sync_dst_time_handle == NULL) {
        sync_dst_time_handle =
            xTimerCreate("sys_time_sync_dst_timer", pdMS_TO_TICKS(QUERY_TIME), pdTRUE, 0, sys_time_sync_dst_timer);
        if (sync_dst_time_handle == NULL) {
            ESP_LOGE(TAG, "dst query timer create fail");
        }
        else {
            xTimerStart(sync_dst_time_handle, 100);
        }
    }
}

/**
 * @brief Re-issue the SNTP request now that an interface has an IP address.
 *
 * sys_time_init() starts the SNTP client during boot, which is before DHCP
 * completes, so that first poll can never succeed. lwIP then falls back to its
 * retry timer (SNTP_RETRY_TIMEOUT = 15 s, doubling on every further failure) and
 * the status bar clock stays at "--:--" far longer than necessary. Restarting
 * the client as soon as an address is available issues a fresh request instead
 * of waiting out that schedule.
 *
 * lwIP still applies SNTP_STARTUP_DELAY_FUNC before the request actually goes
 * out. That jitter is capped by CONFIG_LWIP_SNTP_MAXIMUM_STARTUP_DELAY, which
 * sdkconfig.defaults lowers from the 5000 ms default, so the remaining "--:--"
 * window is the network bring-up time plus at most half a second.
 *
 * Safe to call on every IP event: a later address change simply re-polls.
 */
void sys_time_on_network_ready(void)
{
    if (!s_sntp_initialized) {
        /* sys_time_init() has not started the client yet. It runs moments later
         * and its very first poll will already see a usable address. */
        return;
    }

    ESP_LOGI(TAG, "network is up, re-polling sntp");
    esp_sntp_restart();
}