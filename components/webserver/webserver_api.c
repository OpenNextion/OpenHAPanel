#include <esp_log.h>
#include <lwip/sockets.h>
#include "cJSON.h"
#include "webserver.h"
// #include "webserver_logger.h"
#include <mbedtls/sha256.h>
#include <esp_wifi.h>

static const char *TAG = "webserver_api";

/**
 * extern variable
 */
extern SemaphoreHandle_t webserver_clients_mutex;
extern SemaphoreHandle_t webserver_send_message_mutex;
extern webserver_client_node_t *webserver_node_clients;
extern httpd_handle_t websocket_server_handle;
extern async_resp_arg webserver_cur_resp_arg;

/**
 * @brief Internal implementation of SHA-256 calculation using mbedTLS.
 *
 * Allocates and initializes an `mbedtls_sha256_context`, performs the hashing process
 * (starts, updates, finishes), and stores the 32-byte digest in the `out` buffer.
 *
 * Notes:
 * - The output is a raw binary digest (not hex-encoded).
 * - Caller must ensure `out` points to a buffer of at least 32 bytes.
 * - Memory allocation is performed internally and freed before return.
 *
 * @param in     Input buffer containing the data to hash.
 * @param len    Length of the input buffer in bytes.
 * @param out    Output buffer for the raw SHA-256 digest (32 bytes).
 *
 * @return
 *         - ESP_OK on successful hash computation.
 *         - ESP_FAIL on invalid input or any mbedTLS failure.
 */
esp_err_t sha256_get(const uint8_t *in, uint32_t len, char *out)
{
  if ((in == NULL) || (len == 0) || (out == NULL)) {
    return ESP_FAIL;
  }

  mbedtls_sha256_context *ctx = (mbedtls_sha256_context *)malloc(sizeof(mbedtls_sha256_context));
  if (!ctx) {
    return ESP_ERR_NO_MEM;
  }
  mbedtls_sha256_init(ctx);

    if (mbedtls_sha256_starts(ctx, 0) != 0 || mbedtls_sha256_update(ctx, in, len) != 0 ||
      mbedtls_sha256_finish(ctx, (uint8_t *)out) != 0) {
    mbedtls_sha256_free(ctx);
    free(ctx);
    return ESP_FAIL;
  }

  mbedtls_sha256_free(ctx);
  free(ctx);
  return ESP_OK;
}

/**
 * @brief Get the pointer to the current asynchronous response argument.
 *
 * This function returns a pointer to the static/global variable
 * that holds the current response argument used for asynchronous
 * websocket responses.
 *
 * @return Pointer to the current async_resp_arg structure.
 */
async_resp_arg *websocket_get_async_rsp_arg(void)
{
  return &webserver_cur_resp_arg;
}

/**
 * @brief Set the current asynchronous response argument.
 *
 * Copies the provided async_resp_arg data into the internal
 * current response argument structure used for asynchronous
 * websocket responses.
 *
 * @param arg Pointer to the async_resp_arg data to set.
 */
void websocket_set_async_rsp_arg(async_resp_arg *arg)
{
  memcpy(&webserver_cur_resp_arg, arg, sizeof(async_resp_arg));
}

static bool get_netif_ip_info_by_ifkey(const char *ifkey, esp_netif_ip_info_t *ip_info)
{
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey(ifkey);
  if (!netif) {
    ESP_LOGW(TAG, "netif not found: %s", ifkey);
    return false;
  }

  esp_err_t ret = esp_netif_get_ip_info(netif, ip_info);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "esp_netif_get_ip_info(%s) failed, ret=%d", ifkey, ret);
    return false;
  }

  if (ip_info->ip.addr == 0) {
    return false;
  }

  return true;
}

static void log_ipv4_addr(const char *prefix, in_addr_t addr)
{
  char ip_str[INET_ADDRSTRLEN] = {0};

  struct in_addr in = {.s_addr = addr};

  inet_ntoa_r(in, ip_str, sizeof(ip_str));
  ESP_LOGI(TAG, "%s%s", prefix, ip_str);
}

#if CONFIG_LWIP_IPV6
static bool is_ipv4_mapped_ipv6(const struct in6_addr *addr6)
{
  const uint8_t *b = (const uint8_t *)addr6->s6_addr;

    return b[0] == 0x00 && b[1] == 0x00 && b[2] == 0x00 && b[3] == 0x00 && b[4] == 0x00 && b[5] == 0x00 &&
           b[6] == 0x00 && b[7] == 0x00 && b[8] == 0x00 && b[9] == 0x00 && b[10] == 0xff && b[11] == 0xff;
}

static in_addr_t ipv4_from_mapped_ipv6(const struct in6_addr *addr6)
{
  in_addr_t ipv4 = 0;

  /*
   * IPv4-mapped IPv6 format:
   * ::ffff:a.b.c.d
   *
   * Last 4 bytes are IPv4 address in network byte order.
   */
  memcpy(&ipv4, &addr6->s6_addr[12], sizeof(ipv4));

  return ipv4;
}
#endif

static bool sockaddr_to_ipv4_if_possible(const struct sockaddr_storage *addr, in_addr_t *ipv4_out, const char *name)
{
  if (!addr || !ipv4_out) {
    return false;
  }

  if (addr->ss_family == AF_INET) {
    const struct sockaddr_in *addr4 = (const struct sockaddr_in *)addr;
    *ipv4_out = addr4->sin_addr.s_addr;
    return true;
  }

#if CONFIG_LWIP_IPV6
  if (addr->ss_family == AF_INET6) {
    const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)addr;

    if (is_ipv4_mapped_ipv6(&addr6->sin6_addr)) {
      *ipv4_out = ipv4_from_mapped_ipv6(&addr6->sin6_addr);
      return true;
    }

    ESP_LOGW(TAG, "%s is pure IPv6, disable AP bypass", name);
    return false;
  }
#endif

  ESP_LOGW(TAG, "%s unsupported address family=%d", name, addr->ss_family);
  return false;
}

/**
 * @brief Determine whether the current WebSocket connection is coming from the SoftAP interface.
 *
 * Note:
 * 1. It is no longer judged by the CIDR segment where the peer_ip is located.
 * 2. Get the local_ip via getsockname().
 * 3. local_ip == WIFI_AP_DEF IP to allow AP sign-free login.
 * 4. IPv4-mapped IPv6 will be converted to IPv4 before making a decision.
 * 5. Pure IPv6 does not allow sign-in.
 */
bool webserver_client_ip_is_ap_subnet(int socket_fd, in_addr_t *peer_ip_addr)
{
  struct sockaddr_storage peer_addr;
  struct sockaddr_storage local_addr;
  socklen_t peer_len = sizeof(peer_addr);
  socklen_t local_len = sizeof(local_addr);

  memset(&peer_addr, 0, sizeof(peer_addr));
  memset(&local_addr, 0, sizeof(local_addr));

  if (getpeername(socket_fd, (struct sockaddr *)&peer_addr, &peer_len) != 0) {
    ESP_LOGE(TAG, "getpeername failed, fd=%d errno=%d", socket_fd, errno);
    return false;
  }

  if (getsockname(socket_fd, (struct sockaddr *)&local_addr, &local_len) != 0) {
    ESP_LOGE(TAG, "getsockname failed, fd=%d errno=%d", socket_fd, errno);
    return false;
  }

  in_addr_t peer_ipv4 = 0;
  in_addr_t local_ipv4 = 0;

  if (!sockaddr_to_ipv4_if_possible(&peer_addr, &peer_ipv4, "peer_addr")) {
    return false;
  }

  if (!sockaddr_to_ipv4_if_possible(&local_addr, &local_ipv4, "local_addr")) {
    return false;
  }

  if (peer_ip_addr) {
    *peer_ip_addr = peer_ipv4;
  }

  log_ipv4_addr("WS peer IPv4: ", peer_ipv4);
  log_ipv4_addr("WS local IPv4: ", local_ipv4);

  esp_netif_ip_info_t ap_ip_info = {0};
  esp_netif_ip_info_t sta_ip_info = {0};

  bool ap_ok = get_netif_ip_info_by_ifkey("WIFI_AP_DEF", &ap_ip_info);
  bool sta_ok = get_netif_ip_info_by_ifkey("WIFI_STA_DEF", &sta_ip_info);

  if (!ap_ok) {
    ESP_LOGW(TAG, "WIFI_AP_DEF has no valid IPv4, disable AP bypass");
    return false;
  }

  /*
   * Security protection:
   * If the AP IP and STA IP are identical, the source cannot be safely distinguished.
   */
  if (sta_ok && ap_ip_info.ip.addr == sta_ip_info.ip.addr) {
    ESP_LOGW(TAG, "AP IP conflicts with STA IP, disable AP bypass");
    return false;
  }

  if (local_ipv4 == ap_ip_info.ip.addr) {
    return true;
  }

  if (sta_ok && local_ipv4 == sta_ip_info.ip.addr) {
    return false;
  }

  ESP_LOGW(TAG, "websocket source unknown, need password");
  return false;
}

/**
 * @brief Retrieve the nonce for a connected client based on socket descriptor.
 *
 * This function verifies the provided socket descriptor and attempts to locate the corresponding
 * client data in the webserver's active client list. If found, it copies the per-connection
 * random authentication nonce to the provided output buffer.
 *
 * Key Steps:
 * - Validate input parameters (`sock_fd`, `nonce_out`)
 * - Ensure the socket descriptor is within valid lwIP range
 * - Find the client node using `webserver_client_find_copy`
 * - Copy the client's nonce to the output buffer using `strncpy`
 *
 * @param sock_fd   The socket file descriptor of the client.
 * @param nonce_out Pointer to a buffer that is at least WEB_AUTH_NONCE_HEX_LEN + 1 bytes long.
 *
 * @return
 *      - ESP_OK if the nonce was successfully retrieved
 *      - ESP_ERR_INVALID_ARG if inputs are invalid
 *      - ESP_FAIL if the client was not found
 */
esp_err_t webserver_get_client_nonce(int sock_fd, char *nonce_out)
{
  if (!nonce_out) {
    ESP_LOGE(TAG, "%s: nonce_out param invalid", __func__);
    return ESP_ERR_INVALID_ARG;
  }

  if (sock_fd <= 0 || sock_fd > LWIP_SELECT_MAXNFDS) {
    ESP_LOGE(TAG, "%s: socket_fd=%d invalid", __func__, sock_fd);
    return ESP_ERR_INVALID_ARG;
  }

  webserver_client_node_t client;
  if (!webserver_client_find_copy(sock_fd, &client)) {
    ESP_LOGE(TAG, "Client not found: sock_fd=%d", sock_fd);
    return ESP_FAIL;
  }

  strlcpy(nonce_out, client.nonce, WEB_AUTH_NONCE_HEX_LEN + 1);

  // ESP_LOGI(TAG, "client sock_fd=%d nonce retrieved", sock_fd);
  return ESP_OK;
}

/**
 * @brief Update the nonce for a connected client.
 *
 * This function attempts to update the nonce (a unique identifier such as a UUID) associated with
 * a client connection, identified by its socket file descriptor. The nonce is typically used for
 * session tracking, authentication, or challenge-response mechanisms.
 *
 * Steps:
 * 1. Validate the `nonce_in` pointer.
 * 2. Check the validity of the socket descriptor (must be within the lwIP range).
 * 3. Call `webserver_client_update_nonce()` to update the stored nonce for the client.
 * 4. Log success or failure.
 *
 * @param sock_fd   The client's socket file descriptor.
 * @param nonce_in  Pointer to a string containing the new nonce value.
 *
 * @note The function does not return a value. All outcomes are logged.
 */
void webserver_update_client_nonce(int sock_fd, char *nonce_in)
{
  if (!nonce_in) {
    ESP_LOGE(TAG, "%s: nonce_in param invalid", __func__);
    return;
  }

  if (sock_fd <= 0 || sock_fd > LWIP_SELECT_MAXNFDS) {
    ESP_LOGE(TAG, "%s: socket_fd=%d invalid", __func__, sock_fd);
    return;
  }

  if (!webserver_client_update_nonce(sock_fd, nonce_in)) {
    ESP_LOGE(TAG, "Failed to update nonce: sock_fd=%d", sock_fd);
  }
}

/**
 * @brief Determine if the WebSocket client connection limit has been exceeded.
 *
 * This function checks whether the current number of connected WebSocket webserver_node_clients
 * (as returned by `webserver_client_get_count()`) exceeds the allowed maximum,
 * defined by `MAX_WEBSOCKET_CLIENT`.
 *
 * @return
 *      - true  if the current number of webserver_node_clients exceeds the limit.
 *      - false if the current number of webserver_node_clients is within the allowed range.
 */
bool websocket_client_is_limit(void)
{
  return webserver_client_get_count() > MAX_WEBSOCKET_CLIENT;
}

/**
 * @brief Send a WebSocket message asynchronously to a specific client.
 *
 * This static helper function constructs a `httpd_ws_frame_t` frame and sends it
 * to a WebSocket client identified by its socket file descriptor using the
 * ESP-IDF `httpd_ws_send_frame_async()` API.
 *
 * @param sock_fd  The socket file descriptor of the WebSocket client.
 * @param msg      Pointer to the message payload to be sent.
 * @param len      Length of the message payload in bytes.
 *
 * @return
 *      - ESP_OK on successful transmission.
 *      - ESP_FAIL or other esp_err_t code if sending fails, with error logged.
 *
 * @note This function is static and intended for internal use within the source file.
 */
static esp_err_t send_ws_message_to_client(int sock_fd, const char *msg, size_t len)
{
    httpd_ws_frame_t ws_pkt = {.type = HTTPD_WS_TYPE_BINARY, .payload = (uint8_t *)msg, .len = len};

    esp_err_t ret = httpd_ws_send_frame_async(websocket_server_handle, sock_fd, &ws_pkt);
  return ret;
}

/**
 * @brief Send a WebSocket message to a client with semaphore-based synchronization.
 *
 * This static function wraps `send_ws_message_to_client()` with a semaphore (`webserver_send_message_mutex`)
 * to ensure thread-safe access to WebSocket sending operations, preventing concurrent sends.
 * It also performs input validation and optional debug logging.
 *
 * Steps:
 * 1. Validate input parameters (socket descriptor and `webserver_send_message_mutex` semaphore).
 * 2. Acquire `webserver_send_message_mutex` semaphore using `xSemaphoreTake`.
 * 3. Send the WebSocket message via `send_ws_message_to_client()`.
 * 4. Release the semaphore with `xSemaphoreGive`.
 *
 * @param sock_fd  Socket file descriptor of the target WebSocket client.
 * @param msg      Pointer to the message payload to be sent.
 * @param len      Length of the message payload.
 *
 * @return
 *      - ESP_OK on successful send.
 *      - ESP_ERR_INVALID_ARG if the socket descriptor is invalid.
 *      - ESP_FAIL if the semaphore is unavailable or sending fails.
 *
 * @note
 * This function is intended for internal use only. It relies on a globally accessible
 * binary semaphore (`webserver_send_message_mutex`) to serialize WebSocket send operations across tasks.
 */
static esp_err_t send_ws_message_with_semaphore(int sock_fd, const char *msg, size_t len)
{
  if (webserver_send_message_mutex == NULL) {
    ESP_LOGE(TAG, "webserver_send_message_mutex is NULL");
    return ESP_FAIL;
  }
  if (sock_fd <= 0 || sock_fd > LWIP_SELECT_MAXNFDS) {
    ESP_LOGE(TAG, "Invalid socket fd: %d", sock_fd);
    return ESP_ERR_INVALID_ARG;
  }

  if (xSemaphoreTake(webserver_send_message_mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take webserver_send_message_mutex semaphore");
    return ESP_FAIL;
  }

  esp_err_t ret = send_ws_message_to_client(sock_fd, msg, len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "send_ws_message_to_client failed with %d", ret);
    }
    else {
        ESP_LOGI(TAG, "send_ws_message_to_client success with sock_fd=%d message:[%s]", sock_fd, msg);
  }

  xSemaphoreGive(webserver_send_message_mutex);
  return ret;
}

/**
 * @brief Broadcast a WebSocket message to all connected webserver_node_clients.
 *
 * This function iterates over the list of currently connected WebSocket webserver_node_clients and sends the
 * specified message to each one using `send_ws_message_with_semaphore()`. Access to the client
 * list is protected using the `webserver_clients_mutex` to ensure thread-safe traversal.
 *
 * Steps:
 * 1. Validate input parameters and ensure the client mutex is initialized.
 * 2. Acquire `webserver_clients_mutex` to safely access the client list.
 * 3. Loop through all client nodes and send the message using the semaphore-safe function.
 * 4. Release the mutex after broadcasting.
 *
 * @param msg  Pointer to the message payload to broadcast.
 * @param len  Length of the message in bytes.
 *
 * @return
 *      - ESP_OK if all messages are sent successfully.
 *      - ESP_FAIL if the client list mutex cannot be acquired, or if any message fails to send.
 *      - ESP_ERR_INVALID_ARG if the message is NULL or length is zero.
 *
 * @note
 * This function continues broadcasting even if sending to a specific client fails.
 * The last error encountered (if any) is returned.
 */
esp_err_t webserver_send_message_to_all_client(char *msg, size_t len)
{
  if (webserver_clients_mutex == NULL) {
    ESP_LOGE(TAG, "webserver_clients_mutex is NULL");
    return ESP_FAIL;
  }
  if (!msg || len == 0) {
    ESP_LOGE(TAG, "Invalid message parameter");
    return ESP_ERR_INVALID_ARG;
  }

  if (xSemaphoreTake(webserver_clients_mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take webserver_clients_mutex");
    return ESP_FAIL;
  }

  webserver_client_node_t *current = webserver_node_clients;
  esp_err_t ret = ESP_OK;

  while (current) {
        esp_err_t send_ret = send_ws_message_with_semaphore(current->sock_fd, msg, len);
    if (send_ret != ESP_OK) {
      ret = send_ret; // record failure but keep sending to others
    }
    current = current->next;
  }

  xSemaphoreGive(webserver_clients_mutex);
  return ret;
}

esp_err_t webserver_send_message_to_authenticated_clients(char *msg, size_t len)
{
  if (!msg || len == 0 || !webserver_clients_mutex) {
    return ESP_ERR_INVALID_ARG;
  }
  if (xSemaphoreTake(webserver_clients_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_FAIL;
  }

  esp_err_t ret = ESP_OK;
  for (webserver_client_node_t *current = webserver_node_clients; current; current = current->next) {
    if (current->auth) {
      esp_err_t send_ret = send_ws_message_with_semaphore(current->sock_fd, msg, len);
      if (send_ret != ESP_OK) ret = send_ret;
    }
  }
  xSemaphoreGive(webserver_clients_mutex);
  return ret;
}

/**
 * @brief Send a WebSocket message to the currently active client (in the async context).
 *
 * This function retrieves the file descriptor of the client currently being handled in
 * the WebSocket async context (via `websocket_get_async_rsp_arg()`), and sends the specified
 * message to that client using semaphore-safe transmission.
 *
 * Steps:
 * 1. Validate message pointer and length.
 * 2. Retrieve the current async WebSocket response context.
 * 3. Send the message using `send_ws_message_with_semaphore()`.
 *
 * @param msg   Pointer to the message payload.
 * @param len   Length of the message in bytes.
 *
 * @return
 *      - ESP_OK on successful send.
 *      - ESP_ERR_INVALID_ARG if the input parameters are invalid.
 *      - ESP_FAIL if the async context could not be retrieved or if sending fails.
 *
 * @note This function is intended to be used only within a WebSocket async
 * handler context.
 */
esp_err_t webserver_send_message_to_current_client(char *msg, size_t len)
{
  if (!msg || len == 0) {
    ESP_LOGE(TAG, "Invalid parameter: msg is NULL or length is zero");
    return ESP_ERR_INVALID_ARG;
  }

  async_resp_arg *resp_arg = websocket_get_async_rsp_arg();
  if (!resp_arg) {
    ESP_LOGE(TAG, "Failed to get async response argument");
    return ESP_FAIL;
  }

  return send_ws_message_with_semaphore(resp_arg->fd, msg, len);
}

/**
 * @brief Send a WebSocket message to a specified client by socket file descriptor.
 *
 * This function sends the provided WebSocket message to a client identified by its socket FD.
 * It ensures thread-safe transmission using a semaphore-based send helper.
 *
 * @param msg        Pointer to the message payload.
 * @param len        Length of the message in bytes.
 * @param socket_fd  Socket file descriptor of the target WebSocket client.
 *
 * @return
 *      - ESP_OK on successful transmission.
 *      - ESP_ERR_INVALID_ARG if input parameters are invalid.
 *      - ESP_FAIL if sending fails.
 *
 * @note Caller must ensure that the socket_fd refers to a valid connected WebSocket client.
 */
esp_err_t webserver_send_message_to_special_client(char *msg, size_t len, int socket_fd)
{
  if (!msg || len == 0) {
    ESP_LOGE(TAG, "Invalid parameter: msg is NULL or length is zero");
    return ESP_ERR_INVALID_ARG;
  }

  return send_ws_message_with_semaphore(socket_fd, msg, len);
}

/**
 * @brief Send a WebSocket message not log to a specified client by socket file descriptor.
 *
 * This function sends the provided WebSocket message to a client identified by its socket FD.
 * It ensures thread-safe transmission using a semaphore-based send helper.
 *
 * @param msg        Pointer to the message payload.
 * @param len        Length of the message in bytes.
 * @param socket_fd  Socket file descriptor of the target WebSocket client.
 *
 * @return
 *      - ESP_OK on successful transmission.
 *      - ESP_ERR_INVALID_ARG if input parameters are invalid.
 *      - ESP_FAIL if sending fails.
 *
 * @note Caller must ensure that the socket_fd refers to a valid connected WebSocket client.
 */
esp_err_t webserver_send_message_to_special_client_not_log(char *msg, size_t len, int socket_fd)
{
  if (webserver_send_message_mutex == NULL) {
    ESP_LOGE(TAG, "webserver_send_message_mutex is NULL");
    return ESP_FAIL;
  }
  if (socket_fd <= 0 || socket_fd > LWIP_SELECT_MAXNFDS) {
    ESP_LOGE(TAG, "Invalid socket fd: %d", socket_fd);
    return ESP_ERR_INVALID_ARG;
  }

  if (xSemaphoreTake(webserver_send_message_mutex, portMAX_DELAY) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take webserver_send_message_mutex semaphore");
    return ESP_FAIL;
  }

  esp_err_t ret = send_ws_message_to_client(socket_fd, msg, len);
  xSemaphoreGive(webserver_send_message_mutex);
  return ret;
}

esp_err_t webserver_data_code_response_to_special_client_not_log(cJSON *data,
                                                                  char *method,
                                                                  double seq,
                                                                  int error_code,
                                                                  int socket_fd)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (!root_rsp) {
    cJSON_Delete(data);
    return ESP_ERR_NO_MEM;
  }

  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_ERROR, error_code);
  if (seq != 0) {
    cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  }
  if (data) {
    cJSON_AddItemToObject(root_rsp, WEB_API_KEY_DATA, data);
  }

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  esp_err_t ret = ESP_ERR_NO_MEM;
  if (rsp_str) {
    ret = webserver_send_message_to_special_client_not_log(rsp_str, strlen(rsp_str), socket_fd);
    memset(rsp_str, 0, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ret;
}

esp_err_t webserver_data_response_to_special_client_not_log(cJSON *data,
                                                             char *method,
                                                             double seq,
                                                             int socket_fd)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (!root_rsp) {
    cJSON_Delete(data);
    return ESP_ERR_NO_MEM;
  }

  time_t now = time(NULL);
  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_TS, (int64_t)now * 1000);
  if (seq != 0) {
    cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  }
  if (data) {
    cJSON_AddItemToObject(root_rsp, WEB_API_KEY_DATA, data);
  }

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  esp_err_t ret = ESP_ERR_NO_MEM;
  if (rsp_str) {
    ret = webserver_send_message_to_special_client_not_log(rsp_str, strlen(rsp_str), socket_fd);
    memset(rsp_str, 0, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ret;
}

/**
 * @brief Close all Wi-Fi and Ethernet WebSocket webserver_node_clients except the current one.
 *
 * This function iterates through the list of connected WebSocket webserver_node_clients and forcibly closes
 * all webserver_node_clients that are **not** connected via the SoftAP interface and are **not** the current client.
 * It uses `httpd_sess_trigger_close()` to terminate sessions and protects access to the client list
 * using a mutex.
 *
 * @param current_socket_fd  The socket file descriptor of the current client (to be preserved).
 *
 * @note
 * - Clients connected via AP (Access Point mode) are retained.
 * - Thread-safe access ensured via `webserver_clients_mutex`.
 * - Assumes `websocket_server_handle` is a valid HTTP server handle.
 *
 * @warning
 * Invalid `current_socket_fd` or uninitialized server/mutex will cause early return.
 */
void webserver_close_other_wifi_and_eth_clients(int current_socket_fd)
{
  if (webserver_clients_mutex == NULL) {
    ESP_LOGE(TAG, "webserver_clients_mutex not created!");
    return;
  }
  int sock_fd;
  if (websocket_server_handle == NULL) {
    ESP_LOGE(TAG, "webserver is not create");
    return;
  }

  if ((current_socket_fd <= 0) || (current_socket_fd > LWIP_SELECT_MAXNFDS)) {
    ESP_LOGE(TAG, "socket_fd=%d invalid", current_socket_fd);
    return;
  }

  if (xSemaphoreTake(webserver_clients_mutex, portMAX_DELAY) == pdTRUE) {
    webserver_client_node_t **ptr = &webserver_node_clients;
    while (*ptr) {
      if (((*ptr)->sock_fd != current_socket_fd) && (*ptr)->is_ap == false) {
        sock_fd = (*ptr)->sock_fd;
        httpd_sess_trigger_close(websocket_server_handle, sock_fd);
      }
      ptr = &((*ptr)->next);
    }
  }
  xSemaphoreGive(webserver_clients_mutex);
}

/**
 * @brief Close the WebSocket connection for the specified client.
 *
 * This function triggers the closure of the HTTPD session associated with the provided
 * socket file descriptor. It ensures the socket is valid and that the HTTP server is initialized.
 *
 * @param current_socket_fd  The socket file descriptor of the client to be closed.
 *
 * @note
 * - If the socket is invalid or the server is not initialized, the function returns early.
 * - Uses `httpd_sess_trigger_close()` to request the session closure.
 */
void webserver_close_current_client(int current_socket_fd)
{
  if (websocket_server_handle == NULL) {
    ESP_LOGE(TAG, "webserver is not create");
    return;
  }

  if ((current_socket_fd <= 0) || (current_socket_fd > LWIP_SELECT_MAXNFDS)) {
    ESP_LOGE(TAG, "socket_fd=%d invalid", current_socket_fd);
    return;
  }
  httpd_sess_trigger_close(websocket_server_handle, current_socket_fd);
}

/**
 * @brief Send a structured JSON error response to the current WebSocket client.
 *
 * This function builds a JSON message containing a method name, sequence number,
 * and an error code, and sends it to the current WebSocket client using text frame.
 *
 * JSON structure example:
 * {
 *   "method": "<method_name>",
 *   "seq": <sequence_number>,
 *   "error": <error_code>
 * }
 *
 * @param error_code  The error code to include in the response.
 * @param method      The method name associated with the request.
 * @param seq         The sequence number of the request.
 *
 * @return
 *      - ESP_OK on successful send.
 *      - ESP_FAIL if memory allocation fails or sending fails.
 *
 * @note Uses `cJSON` for JSON encoding and sends via `webserver_send_message_to_current_client`.
 */
esp_err_t webserver_code_response_to_current_client(int error_code, char *method, double seq)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (root_rsp == NULL) {
    ESP_LOGE(TAG, "no mem");
    return ESP_FAIL;
  }

  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_ERROR, error_code);

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  if (rsp_str) {
    webserver_send_message_to_current_client(rsp_str, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ESP_OK;
}

/**
 * @brief Send a JSON-formatted error response to a specific WebSocket client.
 *
 * This function constructs a JSON object containing the method name, sequence number,
 * and error code, then sends it as a text WebSocket frame to the client identified
 * by the given socket file descriptor.
 *
 * JSON Format:
 * {
 *   "method": "<method_name>",
 *   "seq": <sequence_number>,
 *   "error": <error_code>
 * }
 *
 * @param error_code   The error code to be returned to the client.
 * @param method       The method name associated with the client request.
 * @param seq          The sequence number used to match the request/response pair.
 * @param socket_fd    The socket file descriptor of the target client.
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_FAIL if memory allocation or message sending fails.
 *
 * @note Uses cJSON for JSON creation and `webserver_send_message_to_special_client`
 *       for WebSocket transmission.
 */
esp_err_t webserver_code_response_to_special_client(int error_code, char *method, double seq, int socket_fd)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (root_rsp == NULL) {
    ESP_LOGE(TAG, "no mem");
    return ESP_FAIL;
  }

  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_ERROR, error_code);

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  if (rsp_str) {
        webserver_send_message_to_special_client(rsp_str, strlen(rsp_str), socket_fd);
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ESP_OK;
}

/**
 * @brief Broadcast a structured JSON error response to all connected WebSocket webserver_node_clients.
 *
 * This function creates a JSON object containing the provided method name, sequence number,
 * and error code, and sends it as a text WebSocket message to **all currently connected webserver_node_clients**.
 *
 * JSON Format Example:
 * {
 *   "method": "<method_name>",
 *   "seq": <sequence_number>,
 *   "error": <error_code>
 * }
 *
 * @param error_code   Integer representing the application-level error code.
 * @param method       Null-terminated string for the method name (e.g., API name).
 * @param seq          Sequence number for tracking request/response pairs.
 *
 * @return
 *      - ESP_OK on successful broadcast to all webserver_node_clients.
 *      - ESP_FAIL if memory allocation or WebSocket sending fails.
 *
 * @note Utilizes `webserver_send_message_to_all_client()` to dispatch the message to each client.
 */
esp_err_t webserver_code_response_to_all_client(int error_code, char *method, double seq)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (root_rsp == NULL) {
    ESP_LOGE(TAG, "no mem");
    return ESP_FAIL;
  }

  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_ERROR, error_code);

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  if (rsp_str) {
    webserver_send_message_to_all_client(rsp_str, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ESP_OK;
}

/**
 * @brief Send a JSON data response to the current WebSocket client.
 *
 * This function constructs a JSON response containing a method name, timestamp,
 * optional sequence number, and a `data` object. It sends the resulting JSON message
 * as a WebSocket frame to the currently active client associated with the ongoing session.
 *
 * JSON Format Example:
 * {
 *   "method": "<method_name>",
 *   "ts": <timestamp_ms>,
 *   "data": {...},          // The actual payload, passed in via `data`
 *   "seq": <optional_sequence_number>
 * }
 *
 * @param data        A pointer to a cJSON object to include under the "data" key.
 *                    Ownership is transferred and will be deleted by cJSON after sending.
 * @param method      Null-terminated string indicating the API method name.
 * @param seq         Optional sequence number (set to 0 if unused).
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_FAIL if memory allocation fails or message sending fails.
 *
 * @note This function transfers the ownership of `data` to the final JSON response.
 *       Do not use `data` after calling this function.
 */
esp_err_t webserver_data_response_to_current_client(cJSON *data, char *method, double seq)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (root_rsp == NULL) {
    ESP_LOGE(TAG, "no mem");
    return ESP_FAIL;
  }

  time_t now = time(NULL);
  int64_t timestamp_ms = now * 1000;
  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_TS, timestamp_ms);
  cJSON_AddItemToObject(root_rsp, WEB_API_KEY_DATA, data);
  if (seq != 0) {
    cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  }

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  if (rsp_str) {
    webserver_send_message_to_current_client(rsp_str, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ESP_OK;
}

/**
 * @brief Send a JSON data response to the special WebSocket client.
 *
 * This function constructs a JSON response containing a method name, timestamp,
 * optional sequence number, and a `data` object. It sends the resulting JSON message
 * as a WebSocket frame to the special client associated with the ongoing session.
 *
 * JSON Format Example:
 * {
 *   "method": "<method_name>",
 *   "ts": <timestamp_ms>,
 *   "data": {...},          // The actual payload, passed in via `data`
 *   "seq": <optional_sequence_number>
 * }
 *
 * @param data        A pointer to a cJSON object to include under the "data" key.
 *                    Ownership is transferred and will be deleted by cJSON after sending.
 * @param method      Null-terminated string indicating the API method name.
 * @param seq         Optional sequence number (set to 0 if unused).
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_FAIL if memory allocation fails or message sending fails.
 *
 * @note This function transfers the ownership of `data` to the final JSON response.
 *       Do not use `data` after calling this function.
 */
esp_err_t webserver_data_response_to_special_client(cJSON *data, char *method, double seq, int socket_fd)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (root_rsp == NULL) {
    ESP_LOGE(TAG, "no mem");
    return ESP_FAIL;
  }

  time_t now = time(NULL);
  int64_t timestamp_ms = now * 1000;
  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_TS, timestamp_ms);
  cJSON_AddItemToObject(root_rsp, WEB_API_KEY_DATA, data);
  if (seq != 0) {
    cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  }

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  if (rsp_str) {
        webserver_send_message_to_special_client(rsp_str, strlen(rsp_str), socket_fd);
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ESP_OK;
}

/**
 * @brief Send a JSON data response to the current WebSocket client.
 *
 * This function constructs a JSON response containing a method name, timestamp,
 * optional sequence number, and a `data` object. It sends the resulting JSON message
 * as a WebSocket frame to the currently active client associated with the ongoing session.
 *
 * JSON Format Example:
 * {
 *   "method": "<method_name>",
 *   "error_code": <error_code>,
 *   "data": {...},          // The actual payload, passed in via `data`
 *   "seq": <optional_sequence_number>
 * }
 *
 * @param data        A pointer to a cJSON object to include under the "data" key.
 *                    Ownership is transferred and will be deleted by cJSON after sending.
 * @param method      Null-terminated string indicating the API method name.
 * @param seq         Optional sequence number (set to 0 if unused).
 * @param error_code  Error_code
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_FAIL if memory allocation fails or message sending fails.
 *
 * @note This function transfers the ownership of `data` to the final JSON response.
 *       Do not use `data` after calling this function.
 */
esp_err_t webserver_data_code_response_to_current_client(cJSON *data, char *method, double seq, int error_code)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (root_rsp == NULL) {
    ESP_LOGE(TAG, "no mem");
    return ESP_FAIL;
  }

  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_ERROR, error_code);
  if (seq != 0) {
    cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  }
  if (data) {
    cJSON_AddItemToObject(root_rsp, WEB_API_KEY_DATA, data);
  }

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  if (rsp_str) {
    webserver_send_message_to_current_client(rsp_str, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ESP_OK;
}

/**
 * @brief Broadcast a structured JSON data notification to all WebSocket webserver_node_clients.
 *
 * This function constructs a JSON message containing a method name, timestamp,
 * optional sequence number, and the data payload. The final JSON is broadcasted
 * to all currently connected WebSocket webserver_node_clients.
 *
 * JSON Format Example:
 * {
 *   "method": "<method_name>",
 *   "ts": <timestamp_ms>,
 *   "data": {...},            // Payload provided via the `data` parameter
 *   "seq": <optional_sequence_number>
 * }
 *
 * @param data     Pointer to a cJSON object that will be included as "data".
 *                 Ownership is transferred to this function; do not use it afterwards.
 * @param method   Null-terminated string representing the method name.
 * @param seq      Optional sequence number (set to 0 if not needed).
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_FAIL if memory allocation fails or sending fails.
 *
 * @note The `data` object will be deleted after the message is sent.
 */
esp_err_t webserver_data_response_to_all_client(cJSON *data, char *method, double seq)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (root_rsp == NULL) {
    ESP_LOGE(TAG, "no mem");
    return ESP_FAIL;
  }
  time_t now = time(NULL);
  int64_t timestamp_ms = now * 1000;
  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_TS, timestamp_ms);
  cJSON_AddItemToObject(root_rsp, WEB_API_KEY_DATA, data);
  if (seq != 0) {
    cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);
  }
  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  if (rsp_str) {
    webserver_send_message_to_all_client(rsp_str, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ESP_OK;
}

esp_err_t webserver_data_response_to_authenticated_clients(cJSON *data, char *method, double seq)
{
  cJSON *root_rsp = cJSON_CreateObject();
  if (!root_rsp) {
    cJSON_Delete(data);
    return ESP_ERR_NO_MEM;
  }
  time_t now = time(NULL);
  cJSON_AddStringToObject(root_rsp, WEB_API_KEY_METHOD, method);
  cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_TS, (int64_t)now * 1000);
  if (data) cJSON_AddItemToObject(root_rsp, WEB_API_KEY_DATA, data);
  if (seq != 0) cJSON_AddNumberToObject(root_rsp, WEB_API_KEY_SEQ, seq);

  char *rsp_str = cJSON_PrintUnformatted(root_rsp);
  esp_err_t ret = ESP_ERR_NO_MEM;
  if (rsp_str) {
    ret = webserver_send_message_to_authenticated_clients(rsp_str, strlen(rsp_str));
    free(rsp_str);
  }
  cJSON_Delete(root_rsp);
  return ret;
}
