#include <time.h>
#include <inttypes.h>
#include <esp_log.h>
#include <esp_event.h>
#include <sys/param.h>
#include "esp_http_client.h"
#include "cJSON.h"
#include "sys_time_sync.h"
#include "sys_time.h"
#include "esp_mac.h"
#include "webserver_method_api.h"
#include "network.h"
#include "webserver_api.h"
#include "cjson_helpers.h"
#include "sys_time_api.h"
#include "webserver_config_api.h"

#define DST_RE_TYR_TIME_MS (10 * 60 * 1000)
#define QUERY_REC_DATA_LEN (512)
#define HTTP_SUCCESS_CODE (200)
#define MAX_DST_RETRY_COUNT (4)
#define INITIAL_RETRY_DELAY_MS (60 * 1000)
#define NETWORK_CHECK_INTERVAL_MS (2000)
#define NETWORK_LOG_INTERVAL (30)

#define TIME_DST_SYNC_URL "https://timeapi.coolkit.cc/timezone?id=%s&testingId=%s"

static const char *TAG = "sys_time_sync_time";

extern timezone_info_t sys_time_timezone_info;

/**
 * @brief Query buffer to hold JSON response from the time server.
 */
EXT_RAM_BSS_ATTR char query_data[QUERY_REC_DATA_LEN];

/**
 * @brief Mutex to protect timezone query task.
 */
static SemaphoreHandle_t sync_time_mutex = NULL;

/**
 * @brief Task handle for DST info synchronization task.
 */
static TaskHandle_t sync_dst_info_from_network_task_handle = NULL;

/**
 * @brief Task handle for timezone query task.
 */
static TaskHandle_t timezone_query_task_handle = NULL;

/**
 * @brief Indicates whether the system time has been successfully synchronized.
 *
 * This flag is set to true once time synchronization (e.g., via NTP or HTTP)
 * has completed successfully. It is used to ensure that dependent components
 * can safely rely on accurate system time.
 */
static bool sync_time_success_flag = false;

/**
 * @brief Get the system time synchronization status.
 *
 * Returns the status of whether the system time has been successfully
 * synchronized. Useful for modules that depend on accurate time to determine
 * whether it's safe to proceed with time-based operations.
 *
 * @return true if the system time has been synchronized successfully.
 * @return false if the system time has not yet been synchronized.
 */
bool sys_time_get_sync_success_flag(void)
{
    return sync_time_success_flag;
}

/**
 * @brief Parses an ISO 8601 UTC datetime string into a Unix timestamp.
 *
 * This function converts a UTC datetime string in the format `YYYY-MM-DDTHH:MM:SS`
 * to a `time_t` timestamp. The input is assumed to be in UTC, and daylight saving
 * time is explicitly disabled.
 *
 * @param[in]  utc_str        Null-terminated ISO 8601 UTC string (e.g., "2025-06-18T12:34:56").
 * @param[out] out_timestamp  Pointer to a `time_t` variable where the result will be stored.
 *
 * @return
 *         - 0 on success.
 *         - -1 on failure (e.g., null input, parsing error, or mktime failure).
 */
static int sys_time_parse_iso8601_utc_to_timestamp(const char *utc_str, time_t *out_timestamp)
{
    if (!utc_str || !out_timestamp) {
        return -1;
    }

    int year, mon, day, hour, min, sec;
    if (sscanf(utc_str, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &mon, &day, &hour, &min, &sec) != 6) {
        return -1;
    }

    struct tm time_info = {
        .tm_year = year - 1900,
        .tm_mon = mon - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = min,
        .tm_sec = sec,
        .tm_isdst = 0,
    };

    time_t timestamp = mktime(&time_info);
    if (timestamp == -1) {
        return -1;
    }

    *out_timestamp = timestamp;
    return 0;
}

/**
 * @brief Sets the system time in UTC.
 *
 * This function sets the system time using the provided UTC timestamp.
 * It wraps the standard `settimeofday` function to update the system clock.
 *
 * @param[in] t UTC time value as a `time_t` timestamp (seconds since epoch).
 *
 * @return
 *         - 0 on success.
 *         - -1 if setting the time fails (e.g., insufficient permissions or internal error).
 */
static int sys_time_set_sys_utc_time(time_t t)
{
    struct timeval tv;
    tv.tv_sec = t;
    tv.tv_usec = 0;

    if (settimeofday(&tv, (struct timezone *)NULL) < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Synchronizes timezone information from a JSON server response.
 *
 * This function parses a JSON string received from the server to extract
 * timezone configuration details, including timezone ID, UTC time, DST (Daylight Saving Time)
 * flags, UTC offset, and upcoming DST transition time and offset.
 *
 * It updates the system UTC time and sets the global `sys_time_timezone_info` accordingly.
 *
 * The expected JSON structure should resemble:
 *
 * {
 *     "data": {
 *         "id": "Region/City",
 *         "utc": "2025-06-18T12:34:56Z",
 *         "dst": 1,
 *         "offset": 2,
 *         "nextDst": "2025-10-27T01:00:00Z",
 *         "nextOffset": 1
 *     }
 * }
 *
 * @param[in] json_msg A null-terminated JSON string containing timezone information.
 *
 * @return
 *         - ESP_OK on success.
 *         - ESP_ERR_INVALID_ARG if input is NULL.
 *         - ESP_FAIL if JSON parsing fails or mandatory fields are missing.
 */
static esp_err_t sync_timezone_from_server_response(const char *json_msg)
{
    ESP_LOGI(TAG, "Syncing timezone from server response");

    if (!json_msg) {
        ESP_LOGE(TAG, "Null input JSON message");
        return ESP_ERR_INVALID_ARG;
    }

    timezone_info_t tz_param = {0};

    cJSON *root = cJSON_Parse(json_msg);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON *data = NULL;
    if (!cjson_parse_object_field(root, "data", &data)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    char *tz_id = NULL;
    if (cjson_parse_string_field(data, "id", &tz_id)) {
        strncpy(tz_param.timezone, tz_id, TIMEZONE_MAX_LEN - 1);
    }

    char *utc_str = NULL;
    if (cjson_parse_string_field(data, "utc", &utc_str)) {
        time_t t;
        if (sys_time_parse_iso8601_utc_to_timestamp(utc_str, &t) == 0) {
            sys_time_set_sys_utc_time(t);
        }
    }

    int dst_flag = -1;
    if (cjson_parse_int_field(data, "dst", &dst_flag)) {
        tz_param.has_dst = (dst_flag != 0);
        tz_param.is_dst = (dst_flag == 1);
    }

    int offset = 0;
    if (cjson_parse_int_field(data, "offset", &offset)) {
        tz_param.utc_office = offset * 60;
    }

    char *next_dst_str = NULL;
    if (cjson_parse_string_field(data, "nextDst", &next_dst_str)) {
        time_t next_dst_time;
        if (sys_time_parse_iso8601_utc_to_timestamp(next_dst_str, &next_dst_time) == 0) {
            tz_param.next_dst = next_dst_time;
        }
    }

    int next_offset = 0;
    if (cjson_parse_int_field(data, "nextOffset", &next_offset)) {
        tz_param.next_utc_office = next_offset * 60;
    }

    tz_param.sync_time = true;
    sys_time_timezone_info = tz_param;
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief HTTP event handler for synchronizing time.
 *
 * This function handles various HTTP client events during a time synchronization request.
 * It logs event statuses and copies HTTP response data into a buffer (`query_data`) when
 * data is received.
 *
 * The `evt->data` content is truncated if it exceeds the size limit defined by `QUERY_REC_DATA_LEN`.
 *
 * @param[in] evt Pointer to the HTTP client event structure.
 *
 * @return
 *         - ESP_OK Always returns success (ESP_OK).
 */
static esp_err_t handle_http_event_sync_time(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->data_len > 0) {
            int copy_len = MIN(evt->data_len, QUERY_REC_DATA_LEN - 1);
            memcpy(query_data, evt->data, copy_len);
            query_data[copy_len] = '\0';
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    default:
        break;
    }

    return ESP_OK;
}

/**
 * @brief Synchronizes system time and timezone data from a remote server.
 *
 * This function constructs a URL using the provided timezone and device MAC address,
 * then sends an HTTP GET request to fetch timezone and time synchronization data.
 * It handles the HTTP client lifecycle, parses the response to verify its validity,
 * and copies the raw JSON response into the user-provided buffer if successful.
 *
 * The function ensures thread safety using a mutex (`sync_time_mutex`) to prevent
 * concurrent synchronization attempts.
 *
 * @param[in]  timezone  Null-terminated string specifying the timezone (e.g., "Asia/Shanghai").
 * @param[out] get_buf   Buffer to store the JSON response string.
 * @param[in]  buf_len   Length of the `get_buf` buffer.
 *
 * @return
 *         - 0 on success (valid response received and stored in `get_buf`).
 *         - -1 on failure (e.g., HTTP error, JSON parse failure, missing fields).
 */
static int sys_time_sync_time_from_server(char *timezone, char *get_buf, uint16_t buf_len)
{
    int result = -1;
    esp_http_client_handle_t client = NULL;
    char url[128] = {0};
    char mac_addr[20] = {0};
    uint8_t mac[6];

    esp_read_mac(mac, ESP_MAC_ETH);
    snprintf(mac_addr, sizeof(mac_addr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(url, sizeof(url), TIME_DST_SYNC_URL, timezone, mac_addr);

    if (sync_time_mutex == NULL) {
        sync_time_mutex = xSemaphoreCreateMutex();
        if (sync_time_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create sync_time_mutex");
            return -1;
        }
    }

    xSemaphoreTake(sync_time_mutex, portMAX_DELAY);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = handle_http_event_sync_time,
        .timeout_ms = 10000,
        .user_data = query_data,
        .buffer_size = QUERY_REC_DATA_LEN,
    };

    client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP client init failed");
        goto cleanup;
    }

    ESP_LOGI(TAG, "Sending GET request to: %s", url);

    if (esp_http_client_perform(client) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed");
        goto cleanup;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != HTTP_SUCCESS_CODE) {
        ESP_LOGW(TAG, "HTTP status code: %d", status_code);
        goto cleanup;
    }

    ESP_LOGI(TAG, "HTTP Response: %s", query_data);

    if (query_data[0] != '{') {
        ESP_LOGW(TAG, "Response is not JSON format");
        goto cleanup;
    }

    cJSON *root = cJSON_Parse(query_data);
    if (!root) {
        ESP_LOGW(TAG, "Failed to parse JSON");
        goto cleanup;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *id = data ? cJSON_GetObjectItem(data, "id") : NULL;

    if (id == NULL) {
        ESP_LOGW(TAG, "JSON missing 'data.id'");
        cJSON_Delete(root);
        goto cleanup;
    }

    strncpy(get_buf, query_data, buf_len - 1);
    get_buf[buf_len - 1] = '\0';
    result = 0;

    cJSON_Delete(root);

cleanup:
    if (client) {
        esp_http_client_cleanup(client);
    }
    sync_time_success_flag = (result == 0);
    xSemaphoreGive(sync_time_mutex);
    return result;
}

/**
 * @brief FreeRTOS task that queries timezone and time synchronization data from the server.
 *
 * This task is responsible for querying the current timezone information by sending an HTTP
 * request to the time synchronization server using the timezone string provided in the
 * `timezone_query_info_t` structure. The server response is parsed and returned to the web
 * client using a predefined response format.
 *
 * If the response contains valid JSON data under the `"data"` field, it is extracted and
 * passed along; otherwise, an error code is returned to the client.
 *
 * Memory allocated for the input parameter is freed before the task exits.
 * The task cleans up and deletes itself upon completion.
 *
 * @param[in] param Pointer to a dynamically allocated `timezone_query_info_t` structure
 *                  containing the timezone string and request sequence ID.
 */
static void sys_time_timezone_query_task(void *param)
{
    timezone_query_info_t *info = (timezone_query_info_t *)param;
    char response_buf[512] = {0};
    int error_code = sys_time_sync_time_from_server(info->timezone, response_buf, sizeof(response_buf));

    cJSON *response_data = NULL;

    if (strlen(response_buf) > 0) {
        cJSON *root = cJSON_Parse(response_buf);
        if (root == NULL) {
            ESP_LOGW(TAG, "Failed to parse HTTP response JSON");
            error_code = -1;
        }
        else {
            cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
            if (cJSON_IsObject(data)) {
                response_data = cJSON_Duplicate(data, cJSON_True);
            }
            else {
                ESP_LOGW(TAG, "Missing or invalid 'data' object in response");
                error_code = -1;
            }
            cJSON_Delete(root);
        }
    }
    else {
        error_code = -1;
    }

    webserver_data_code_response_to_current_client(response_data, WEB_API_METHOD_GET_TIMEZONE_RSP, info->seq,
                                                   error_code);

    free(info);
    timezone_query_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Starts a FreeRTOS task to query timezone information from the server.
 *
 * This function initiates a background task (`sys_time_timezone_query_task`) that sends an
 * HTTP request to retrieve timezone and UTC time information from the server.
 *
 * It ensures only one such task runs at a time and verifies that network connectivity is available.
 * If successful, it passes the timezone and request sequence to the task and returns 0.
 *
 * @param[in] timezone Null-terminated string representing the timezone (e.g., "Asia/Shanghai").
 * @param[in] seq      Request sequence number used to match responses with requests.
 *
 * @return
 *         -  0 on success (task created).
 *         -  905 if a query task is already running.
 *         -  906 if no network is available.
 *         - -1 on memory allocation or task creation failure.
 */
int sys_time_timezone_query_task_start(char *timezone, uint64_t seq)
{
    if (timezone_query_task_handle != NULL) {
        return SYSTIME_ERR_TIMEZONE_CHECKING;
    }

    NetworkState *net = network_get_state();
    if (!net->sta_connected_flag) {
        ESP_LOGE(TAG, "Network unavailable for timezone query");
        return SYSTIME_ERR_NOT_NETWORK;
    }

    timezone_query_info_t *query_info = (timezone_query_info_t *)calloc(1, sizeof(timezone_query_info_t));
    if (!query_info) {
        ESP_LOGE(TAG, "Failed to allocate memory for timezone query");
        return -1;
    }

    strncpy(query_info->timezone, timezone, sizeof(query_info->timezone) - 1);
    query_info->seq = seq;

    if (xTaskCreatePinnedToCore(sys_time_timezone_query_task, "timezone_query_task", 4096, query_info, 8,
                                &timezone_query_task_handle, 1) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create timezone query task");
        free(query_info);
        timezone_query_task_handle = NULL;
        return -1;
    }

    return 0;
}

/**
 * @brief Sends a JSON notification with the current timezone information to the web client.
 *
 * This function creates a JSON object containing the current timezone details stored in
 * `sys_time_timezone_info`, including timezone ID, UTC offset, and daylight saving time (DST)
 * status. If DST is applicable, the upcoming DST shift time and next UTC offset are included.
 *
 * The JSON data is sent to the current web client via `webserver_data_response_to_current_client`
 * with the event name "Device.InfoNotify".
 */
static void webserver_timezone_info_notify(void)
{
    cJSON *data = cJSON_CreateObject();
    cJSON *timezone_info_cjson = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "tzInfo", timezone_info_cjson);
    cJSON_AddStringToObject(timezone_info_cjson, "timeZone", sys_time_timezone_info.timezone);
    cJSON_AddNumberToObject(timezone_info_cjson, "utcOffset", sys_time_timezone_info.utc_office);
    cJSON_AddBoolToObject(timezone_info_cjson, "isDst", sys_time_timezone_info.is_dst);
    cJSON_AddBoolToObject(timezone_info_cjson, "hasDst", sys_time_timezone_info.has_dst);

    if (sys_time_timezone_info.has_dst) {
        cJSON_AddNumberToObject(timezone_info_cjson, "nextDstShift", sys_time_timezone_info.next_dst * 1000);
        cJSON_AddNumberToObject(timezone_info_cjson, "nextUtcOffset", sys_time_timezone_info.next_utc_office);
    }
    else {
        cJSON_AddNullToObject(timezone_info_cjson, "nextDstShift");
        cJSON_AddNullToObject(timezone_info_cjson, "nextUtcOffset");
    }
    webserver_data_response_to_current_client(data, WEB_API_METHOD_DEVICE_INFO_NOTIFY, 0);
}

/**
 * @brief FreeRTOS task that synchronizes Daylight Saving Time (DST) information from the network.
 *
 * This task waits for a valid internet connection, then repeatedly attempts to fetch
 * timezone and DST information from the time server until successful.
 *
 * If the fetch fails, the task retries with exponential backoff delays, up to a maximum retry count,
 * after which it waits a longer period before retrying again.
 *
 * Upon successful retrieval, the timezone information is updated and notifications are sent
 * to the webserver to inform clients of the updated time settings.
 *
 * The task deletes itself after completion.
 *
 * @param[in] param Unused parameter.
 */
static void sync_dst_info_from_network_task(void *param)
{
    int network_check_count = 0;
    uint32_t retry_count = 0;
    uint32_t retry_delay_ms = INITIAL_RETRY_DELAY_MS;

    while (1) {
        NetworkState *net = network_get_state();
        if (net && net->internet_connected) {
            break;
        }

        if (++network_check_count % NETWORK_LOG_INTERVAL == 0) {
            ESP_LOGI(TAG, "Waiting for network to query DST info...");
        }
        vTaskDelay(NETWORK_CHECK_INTERVAL_MS / portTICK_PERIOD_MS);
    }

    char response_buf[512] = {0};
    while (sys_time_sync_time_from_server(sys_time_timezone_info.timezone, response_buf, sizeof(response_buf)) != 0) {
        retry_count++;

        if (retry_count > MAX_DST_RETRY_COUNT) {
            ESP_LOGW(TAG, "Max retries reached, waiting %d minutes before retrying...",
                     DST_RE_TYR_TIME_MS / (60 * 1000));
            vTaskDelay(DST_RE_TYR_TIME_MS / portTICK_PERIOD_MS);
        }
        else {
            ESP_LOGW(TAG, "Retry %lu: waiting %lu ms before retrying...", retry_count, retry_count * retry_delay_ms);
            vTaskDelay((retry_count * retry_delay_ms) / portTICK_PERIOD_MS);
        }
    }

    ESP_LOGI(TAG, "Successfully fetched DST info for timezone: %s", sys_time_timezone_info.timezone);
    if (sync_timezone_from_server_response(response_buf) == ESP_OK) {
        esp_err_t event_err = esp_event_post(SYS_TIME_EVENT, SYS_TIME_EVENT_SYNCED, NULL, 0, portMAX_DELAY);
        if (event_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post SYS_TIME_EVENT_SYNCED: %s", esp_err_to_name(event_err));
        }
        webserver_timezone_info_notify();
        webserver_device_info_notify(UPDATE_CURRENT_TIME);
    }

    sync_dst_info_from_network_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Starts the DST synchronization task to fetch timezone Daylight Saving Time info.
 *
 * This function creates a FreeRTOS task that synchronizes Daylight Saving Time (DST)
 * information from the network if the task is not already running.
 *
 * @return
 *         - 0 if the task was successfully created.
 *         - -1 if the task is already running or creation failed.
 */
int sync_dst_info_from_network_task_start(void)
{
    if (sync_dst_info_from_network_task_handle != NULL)
        return -1;
    if (xTaskCreatePinnedToCore(sync_dst_info_from_network_task, "DST_QUERY", 4096, NULL, 10,
                                &sync_dst_info_from_network_task_handle, 1) != pdPASS) {
        ESP_LOGE(TAG, "task create fail");
        return -1;
    }
    return 0;
}