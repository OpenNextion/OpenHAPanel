#include <esp_log.h>
#include <lwip/dns.h>
#include "esp_netif.h"
#include "network_ping.h"
#include <network_wifi_sta.h>
#include <lwip/inet.h>
#include "ping/ping_sock.h"
#include "network_api.h"
static const char *TAG = "network_ping";

/**
 * @brief Handle for the ESP ping session.
 *
 * Used to manage the ping task, including starting, stopping, and tracking
 * ping operations within the network module.
 */
static esp_ping_handle_t network_ping_handle;

/**
 * @brief Counter for consecutive ping losses.
 *
 * Tracks the number of sequential failed ping attempts to detect
 * connectivity issues.
 */
static int32_t ping_loss_count = 0;

/**
 * @brief Flag indicating whether the ping task is currently running.
 *
 * Non-zero means the ping task is active; zero means it is stopped.
 */
static int32_t ping_task_running = 0;

/**
 * @brief Counter for the number of DNS IP retry attempts.
 *
 * Used to limit or track the frequency of retries to resolve DNS IP
 * addresses when ping timeouts occur.
 */
static int32_t ping_ip_retry_count = 0;

/**
 * @brief Flag to indicate a ping target IP change is required.
 *
 * Declared extern; set externally when the ping IP needs to be updated.
 */
extern bool ping_change_flag;

static esp_netif_t *network_ping_get_uplink_netif(void)
{
    if (net_state.sta_connected_flag) {
        return network_sta_get_netif();
    }

    return NULL;
}

/**
 * @brief Callback invoked on successful ping response.
 *
 * Resets ping loss counters, marks internet connection as up if needed,
 * and periodically logs detailed ping success information.
 *
 * @param hdl Handle to the ping session.
 * @param args User-defined callback arguments (unused).
 */
static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    static uint32_t ping_success_count = 0;
    ping_loss_count = 0;
    ping_ip_retry_count = 0;
    if (!net_state.internet_connected) {
        net_state.internet_connected = 1;
        if (net_state.sta_connected_flag)
            webserver_network_mode_info_notify(NET_MODE_WLAN_ONLY);
        // basic_config_internet_sync(1);
    }

    if (++ping_success_count % 50 == 0) {
        uint8_t ttl = 0;
        uint16_t seqno = 0;
        uint32_t time_ms = 0, recv_len = 0;
        ip_addr_t addr;

        esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
        esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
        esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &addr, sizeof(addr));
        esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
        esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &time_ms, sizeof(time_ms));

        ESP_LOGI(TAG, "Ping success: from %s icmp_seq=%d ttl=%d time=%ld ms  %ld bytes", inet_ntoa(addr.u_addr.ip4),
                 seqno, ttl, time_ms, recv_len);
    }
}

/**
 * @brief Callback invoked on ping timeout event.
 *
 * Increments loss counters, attempts DNS cache clearing and IP retry on
 * specific conditions, and updates internet connectivity state if loss
 * threshold is reached. Logs timeout details and triggers DNS IP parsing retry.
 *
 * @param hdl Handle to the ping session.
 * @param args User-defined callback arguments (unused).
 */
static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    static uint32_t ping_timeout_count = 0;
    uint16_t seqno = 0;
    ip_addr_t addr;

    ping_loss_count++;
    ping_ip_retry_count++;
    if (net_state.internet_connected && ping_loss_count == 1) {
        dns_clear_cache();
    }
    if (ping_ip_retry_count % 60 == 0) {
        dns_clear_cache();
    }
    if (ping_loss_count >= 3) {
        ping_loss_count = 0;
        if (net_state.internet_connected) {
            net_state.internet_connected = 0;

            if (net_state.sta_connected_flag)
                webserver_network_mode_info_notify(NET_MODE_WLAN_ONLY);

            // basic_config_internet_sync(0);
        }
    }

    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &addr, sizeof(addr));
    ESP_LOGI(TAG, "Ping timeout: from %s icmp_seq=%d ping_loss_count:%lu ping_timeout_count:%lu",
             inet_ntoa(addr.u_addr.ip4), seqno, ping_loss_count, ping_timeout_count);

    if (++ping_timeout_count % 30 == 0) {
        dns_clear_cache();
        ping_ip_retry_count = 0;
    }
}

/**
 * @brief Callback invoked when the ping session ends.
 *
 * Logs statistics about the number of ping requests sent and received,
 * and the total duration of the ping session.
 *
 * @param hdl Handle to the ping session.
 * @param args User-defined callback arguments (unused).
 */
static void on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t sent, received, duration_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &sent, sizeof(sent));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &duration_ms, sizeof(duration_ms));

    ESP_LOGI(TAG, "Ping stats: %ld sent, %ld received, time %ldms", sent, received, duration_ms);
}

/**
 * @brief Stop the ongoing network ping task.
 *
 * This function stops the ping task if it is running by stopping and deleting
 * the ESP ping session, resetting the ping loss count, and marking the ping
 * task as not running. It also adds a delay to ensure proper task termination.
 *
 * @note This function assumes the ping task is currently running and
 *       will only attempt to stop if ping_task_running is true.
 *
 * @return void
 */
void network_stop_ping_task(void)
{
    ESP_LOGI(TAG, "Stopping ping task");
    if (network_ping_handle) {
        esp_ping_stop(network_ping_handle);
        esp_ping_delete_session(network_ping_handle);
        network_ping_handle = NULL;
    }

    ping_task_running = 0;
    ping_loss_count = 0;
    ping_ip_retry_count = 0;
}

/**
 * @brief Start the network ping task.
 *
 * This function initializes and starts a continuous ping task targeting
 * Google's public DNS server (8.8.8.8). It resets ping loss counters,
 * sets up the ping configuration including target IP, interval, count,
 * and network interface (if Ethernet is disconnected). It registers
 * callback functions for ping success, timeout, and end events, and
 * starts the ping session.
 *
 * @note The ping count is set to a very large number to enable
 *       effectively continuous pinging until stopped.
 *
 * @return void
 */
void network_start_ping_task(void)
{
    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    esp_ping_callbacks_t cbs = {.on_ping_success = on_ping_success,
                                .on_ping_timeout = on_ping_timeout,
                                .on_ping_end = on_ping_end,
                                .cb_args = NULL};
    esp_netif_t *uplink_netif = network_ping_get_uplink_netif();
    ip_addr_t target_addr;
    esp_err_t err;
    int if_index;
    if (!uplink_netif) {
        ESP_LOGW(TAG, "No uplink netif available, skip ping start");
        return;
    }
    network_stop_ping_task();
    ESP_LOGI(TAG, "Starting ping task");
    memset(&target_addr, 0, sizeof(target_addr));
    if (!ipaddr_aton("8.8.8.8", &target_addr)) {
        ESP_LOGE(TAG, "Failed to parse ping target address");
        return;
    }

    cfg.target_addr = target_addr;
    cfg.interval_ms = (uint32_t)5000;
    cfg.count = ESP_PING_COUNT_INFINITE;
    cfg.task_stack_size = (uint32_t)4096;

    if_index = esp_netif_get_netif_impl_index(uplink_netif);
    if (if_index > 0) {
        cfg.interface = (uint32_t)if_index;
    }

    err = esp_ping_new_session(&cfg, &cbs, &network_ping_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ping_new_session failed: %s", esp_err_to_name(err));
        network_ping_handle = NULL;
        ping_task_running = 0;
        return;
    }

    err = esp_ping_start(network_ping_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ping_start failed: %s", esp_err_to_name(err));
        esp_ping_delete_session(network_ping_handle);
        network_ping_handle = NULL;
        ping_task_running = 0;
        return;
    }

    ping_task_running = 1;
}
