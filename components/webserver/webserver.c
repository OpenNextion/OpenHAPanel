#include "webserver.h"
#include "esp_log.h"
#include "webserver_auth.h"
#include "webserver_message_parse.h"
#include "webserver_config_api.h"
#include "webserver_config_forget_password.h"
#include "webserver_firmware_update.h"
#include "screen_api.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <lwip/sockets.h>

#include "network_api.h"

static const char *TAG = "webserver";

static esp_netif_ip_info_t webserver_client_ip_info = {0};

/**
 * websocket mutex
 */
SemaphoreHandle_t webserver_send_message_mutex = NULL;

/**
 * Client parameters that currently require asynchronous responses
 */
async_resp_arg webserver_cur_resp_arg = {0};

/**
 * websocket message queue
 */
#define WEB_MSG_ITEM_NUM (10 * 10)
#define WEB_MSG_ITEM_LEN (sizeof(websocketMsg_t))
static EXT_RAM_BSS_ATTR uint8_t webserver_message_queue_storage[WEB_MSG_ITEM_LEN * WEB_MSG_ITEM_NUM];
static QueueHandle_t webserver_message_queue_handle;
static StaticQueue_t webserver_message_queue_buffer;
static TaskHandle_t webserver_message_task_handle;
static esp_event_handler_instance_t webserver_ip_event_instance;
static esp_event_handler_instance_t webserver_event_instance;

/**
 * websocket loop handle
 */
httpd_handle_t websocket_server_handle = NULL;

/**
 * websocket esp event define base
 */
ESP_EVENT_DEFINE_BASE(WEB_SERVER_EVENT);

/**
 * @brief Event handler for when the device obtains an IP address
 *
 * This function is triggered when either the Wi-Fi station (STA) or Ethernet
 * interface obtains an IP address. It logs the IP configuration and stores
 * the obtained IP info in a global variable.
 *
 * @param arg         User-provided argument (unused).
 * @param event_base  Base of the event (e.g., IP_EVENT).
 * @param event_id    Specific event ID (e.g., IP_EVENT_STA_GOT_IP).
 * @param event_data  Pointer to event-specific data (contains IP info).
 */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  ip_event_got_ip_t *event = NULL;
  const esp_netif_ip_info_t *ip_info = NULL;

  if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    event = (ip_event_got_ip_t *)event_data;
    ip_info = &event->ip_info;
  }

  if (ip_info) {
    memcpy(&webserver_client_ip_info, ip_info, sizeof(esp_netif_ip_info_t));
  }
}

/**
 * @brief Webserver internal event handler
 *
 * This function handles internal events posted to the webserver event loop.
 * Depending on the event ID, it performs corresponding actions such as
 * cleaning up client resources, updating the frontend with status, or handling
 * authentication completion.
 *
 * @param arg         User-provided argument (unused).
 * @param event_base  The base of the event (should be WEB_SERVER_EVENT).
 * @param event_id    The ID of the event (e.g., connect, disconnect, auth ok).
 * @param event_data  Pointer to any associated event data.
 */
static void webserver_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  if (event_base == WEB_SERVER_EVENT) {
    switch (event_id) {
    case WEB_SERVER_EVENT_CONNECT: {
      break;
    }
    case WEB_SERVER_EVENT_DISCONNECT: {
      ESP_LOGI(TAG, "webserver disconnect");
      break;
    }
    case WEB_SERVER_EVENT_RECEIVE: {
      break;
    }
    case WEB_SERVER_EVENT_SKIP_AUTH:
    case WEB_SERVER_EVENT_AUTH_OK: {
      ESP_LOGI(TAG, "webserver auth ok or no need auth");
        webserver_network_info_notify();
        // webserver_vpn_status_notify();
        // webserver_wireless_controller_notify();
        webserver_device_info_notify(UPDATE_DEVICE_ALL);
        webserver_theme_mode_notify();
        screen_config_notify();
        webserver_firmware_update_status_notify();
        // webserver_indicator_notify(UPDATE_INDICATOR_ALL);
        webserver_whitelist_notify();
        // webserver_webhook_task_list_notify();
        // webserver_mqtt_api_notify();
        // webserver_light_smart_api_notify();
        // webserver_firmware_info_notify();
      break;
    }
    default: {
      ESP_LOGE(TAG, "webserver event error");
      break;
    }
    }
  }
}

/**
 * @brief Callback for WebSocket client disconnection
 *
 * This function is triggered when a WebSocket client disconnects.
 * It posts a disconnect event to the webserver event loop, removes
 * the client from the internal client list, and closes the socket.
 *
 * @param hd      The HTTP server handle.
 * @param sock_fd The file descriptor of the disconnected client socket.
 */
static void websocket_client_disconnect_cb(httpd_handle_t hd, int sock_fd)
{
  ESP_LOGI(TAG, "Client disconnected sock_fd=%d", sock_fd);
    // webserver_logger_close_socket(sock_fd);
    check_and_clear_forget_password_socket(sock_fd);
    // check_and_clear_wireless_controller_web_ota_socket(sock_fd);
    // clear_esp32_web_ota_client_socket_fd(sock_fd);
  webserver_client_remove(sock_fd);
  close(sock_fd);
}

static void check_and_register_new_ws_client(httpd_handle_t hd, int sock_fd) {
  webserver_client_node_t client_info;
  if (!webserver_client_find_copy(sock_fd, &client_info)) {
    in_addr_t ip_addr = 0;
    bool is_ap_subnet = webserver_client_ip_is_ap_subnet(sock_fd, &ip_addr);
    /* webserver_login_authentication() judges over-limit on the count that already
     * includes this new client, so the equivalent predicate has to be captured here
     * before webserver_client_add() runs. */
    bool over_limit = (webserver_client_get_count() >= MAX_WEBSOCKET_CLIENT);
    webserver_client_add(sock_fd, is_ap_subnet);
    async_resp_arg resp_arg = {
        .hd = hd,
        .fd = sock_fd,
        .seq = 0,
    };
    websocket_set_async_rsp_arg(&resp_arg);
    ESP_LOGI(TAG, "New WS client registered: fd=%d, is_ap=%d", sock_fd, is_ap_subnet);
    webserver_login_authentication(&resp_arg, is_ap_subnet, ip_addr);
    if (!over_limit) {
      esp_err_t notify_err = webserver_config_forget_password_state_notify_to_client(sock_fd);
      if (notify_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send forgot-password state to fd=%d: %s",
                 sock_fd, esp_err_to_name(notify_err));
      }
    }
    if (over_limit) {
      /* The notice above is written to the socket synchronously (httpd_ws_send_frame_async
       * sends in the caller's context), so the session can be released immediately.
       * Leaving it open would hold a socket and keep a node in the client list, which
       * would pin the count above the limit and refuse every later console even after
       * an admitted one has been closed. */
      ESP_LOGW(TAG, "Console limit %d reached, releasing fd=%d", MAX_WEBSOCKET_CLIENT, sock_fd);
      httpd_sess_trigger_close(hd, sock_fd);
    }
  }
}

/**
 * @brief Handle incoming WebSocket requests
 *
 * This function processes WebSocket HTTP requests. If the method is HTTP GET,
 * it completes the WebSocket handshake and enqueues a connect message. If it
 * is a WebSocket data frame (non-GET), it reads the frame content and pushes
 * a data message into the web message queue.
 *
 * - For HTTP GET: initializes a connect event and sends it to `webserver_message_queue_handle`.
 * - For WebSocket frames: receives the payload, wraps it in a message, and
 *   sends it to `webserver_message_queue_handle`.
 *
 * Queue access is protected using `webMsgQueueWriteMutex`.
 *
 * @param req Pointer to the HTTP request context.
 * @return ESP_OK on success, or error code on failure.
 */
static esp_err_t websocket_request_receive(httpd_req_t *req)
{
  esp_err_t ret;
  int sock_fd = httpd_req_to_sockfd(req);

  if (req->method == HTTP_GET) {
    websocketMsg_t msg = {0};
    msg.msgType = WEB_MSG_TYPE_CONNECT;
    msg.resp_arg.hd = req->handle;
    msg.resp_arg.fd = sock_fd;
    msg.data = NULL;
    msg.len = 0;
    if (webserver_message_queue_handle) {
      if (xQueueSend(webserver_message_queue_handle, (void *)&msg,
                     (TickType_t)0) != pdPASS) {
        ESP_LOGE(TAG, "webserver_message_queue_handle is full!");
        return ESP_ERR_NO_MEM;
      }
    }
    return ESP_OK;
  } else {
    check_and_register_new_ws_client(req->handle, sock_fd);
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
      return ret;
    }

    if (ws_pkt.len) {
      uint8_t *buf = NULL;
      buf = calloc(1, ws_pkt.len + 1);
      if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to call oc memory for buf");
        return ESP_ERR_NO_MEM;
      }
      ws_pkt.payload = buf;
      ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        free(buf);
        return ret;
      }
      websocketMsg_t msg = {0};
      msg.msgType = WEB_MSG_TYPE_DATA;
      msg.resp_arg.hd = req->handle;
      msg.resp_arg.fd = sock_fd;
      msg.data = (char *)ws_pkt.payload;
      msg.len = ws_pkt.len;

      bool queued = false;
      if (webserver_message_queue_handle) {
                if (xQueueSend(webserver_message_queue_handle, (void *)&msg, (TickType_t)0) == pdPASS) {
          queued = true;
                }
                else {
                    ESP_LOGE(TAG, "webserver_message_queue_handle is full, drop DATA len=%d fd=%d", msg.len,
                             msg.resp_arg.fd);
        }
            }
            else {
                ESP_LOGE(TAG, "webserver_message_queue_handle is NULL, drop DATA len=%d fd=%d", msg.len,
                         msg.resp_arg.fd);
      }

      if (!queued) {
        free(buf);
      }
    }
    return ret;
  }
}

/**
 * @brief Custom 404 error handler for HTTP server.
 *
 * Redirects captive-portal probes and unknown paths to an absolute root URL
 * on the same device interface used by the current HTTP connection.
 *
 * It responds with:
 * - Status: 302 Temporary Redirect
 * - Header: Location: http://<device-ip>/
 * - Body: "Redirect to the captive portal"
 *
 * @param req The HTTP request context.
 * @param err The error code (typically HTTPD_404_NOT_FOUND).
 * @return ESP_OK on success.
 */
static esp_err_t http_captive_portal_redirect_handler(httpd_req_t *req)
{
  char location[64] = {0};
  struct sockaddr_storage local_addr = {0};
  socklen_t local_addr_len = sizeof(local_addr);
  int sock_fd = httpd_req_to_sockfd(req);

  if (getsockname(sock_fd, (struct sockaddr *)&local_addr, &local_addr_len) == 0 &&
      local_addr.ss_family == AF_INET) {
    const struct sockaddr_in *local_ipv4 = (const struct sockaddr_in *)&local_addr;
    char address[INET_ADDRSTRLEN] = {0};
    if (local_ipv4->sin_addr.s_addr != INADDR_ANY &&
        inet_ntop(AF_INET, &local_ipv4->sin_addr, address, sizeof(address))) {
      snprintf(location, sizeof(location), "http://%s/", address);
    }
  }

  if (location[0] == '\0') {
    esp_netif_ip_info_t ap_ip = {0};
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif && esp_netif_get_ip_info(ap_netif, &ap_ip) == ESP_OK && ap_ip.ip.addr != 0) {
      snprintf(location, sizeof(location), "http://" IPSTR "/", IP2STR(&ap_ip.ip));
    }
    else {
      strlcpy(location, "http://192.168.4.1/", sizeof(location));
    }
  }

  // ESP_LOGI(TAG, "Captive portal redirect: uri=%s fd=%d location=%s", req->uri, sock_fd, location);
  httpd_resp_set_status(req, "302 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", location);
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
  return http_captive_portal_redirect_handler(req);
}

/**
 * @brief Handles HTTP GET requests for network connectivity probes.
 *
 * Serves a lightweight "204 No Content" response for resources that must not
 * trigger captive-portal navigation, currently the browser favicon request.
 *
 * @param req Pointer to the HTTP request object.
 * @return ESP_OK on success.
 */
static esp_err_t http_no_content_handler(httpd_req_t *req)
{
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

/**
 * @brief Builds an RFC 1123 formatted Date string for HTTP responses.
 *
 * Safari and other strict browsers use the Date header for cache freshness
 * evaluation; responses without it may not be cached properly on iOS.
 *
 * @param[out] buf      Output buffer for the formatted date string.
 * @param[in]  buf_len  Size of the output buffer (recommend 32+).
 */
static void http_resp_date(char *buf, size_t buf_len)
{
  time_t now = time(NULL);
  struct tm tm_gmt;
  gmtime_r(&now, &tm_gmt);
  strftime(buf, buf_len, "%a, %d %b %Y %H:%M:%S GMT", &tm_gmt);
}

/**
 * @brief Handles HTTP GET requests for the root path ("/").
 *
 * This handler serves a pre-compressed gzip HTML file (`index.html.gz`)
 * embedded into the binary. It sets the `Content-Encoding: gzip` header and
 * sends the file with an explicit Content-Length (single httpd_resp_send)
 * instead of chunked encoding. An ETag derived from the payload size allows
 * browsers to revalidate with If-None-Match and receive a cheap 304 instead
 * of re-downloading the whole page.
 *
 * @param req Pointer to the HTTP request object.
 * @return ESP_OK on success.
 */
static esp_err_t html_get_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "html_get_handler: socket fd=%d", httpd_req_to_sockfd(req));

  extern const unsigned char index_start[] asm("_binary_index_html_gz_start");
  extern const unsigned char index_end[] asm("_binary_index_html_gz_end");
  const size_t index_size = (index_end - index_start);

  /* ETag 由构建产物大小派生：固件更新后产物变化，ETag 随之失效 */
  char etag[32];
  snprintf(etag, sizeof(etag), "\"gz-%u\"", (unsigned int)index_size);

  /* RFC 1123 Date 头：iOS Safari 对缺 Date 的响应缓存行为不可靠 */
  char date_str[40];
  http_resp_date(date_str, sizeof(date_str));
  httpd_resp_set_hdr(req, "Date", date_str);

  /* 支持 If-None-Match 协商缓存：命中则只回 304，不再重复传输整页 */
  size_t inm_len = httpd_req_get_hdr_value_len(req, "If-None-Match");
  if (inm_len > 0 && inm_len < 64) {
    char inm_buf[64];
    if (httpd_req_get_hdr_value_str(req, "If-None-Match", inm_buf, sizeof(inm_buf)) == ESP_OK &&
        strcmp(inm_buf, etag) == 0) {
      httpd_resp_set_status(req, "304 Not Modified");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }

  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(req, "ETag", etag);
  /* no-cache：允许缓存但每次需 ETag 协商，保证固件更新后页面即时生效 */
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  /* 单次发送，自动携带 Content-Length，浏览器可显示进度并复用连接 */
  return httpd_resp_send(req, (const char *)index_start, index_size);
}

/**
 * @brief Start the HTTP/WebSocket server and register URI handlers.
 *
 * Initializes the HTTP server with default configuration, sets maximum
 * open sockets, and registers URI handlers including the WebSocket endpoint,
 * root HTML handler, and captive-portal probe endpoints.
 *
 * @return httpd_handle_t The server handle if successful, NULL otherwise.
 */
static httpd_handle_t start_webserver(void)
{
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  /* 跑在 core 1：core 0 被 WiFi 协议栈/lwIP 占据且优先级更高，同核会被饿死，
   * 大文件传输吞吐骤降；core 1 上与 webMsgDistTask（同优先级）分时即可 */
  config.core_id = 1;
  // config.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  config.max_open_sockets = MAX_OPEN_SOCKETS;
  config.close_fn = websocket_client_disconnect_cb;
  config.keep_alive_enable = true;
  config.keep_alive_idle = 10;
  config.keep_alive_interval = 5;
  config.keep_alive_count = 3;
  ESP_LOGI(TAG, "Starting websocket server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    ESP_LOGI(TAG, "Registering URI handlers");
    static httpd_uri_t html_file = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = html_get_handler,
    };

    static const httpd_uri_t ws = {.uri = "/api/ws",
        .method = HTTP_GET,
        .handler = websocket_request_receive,
        .user_ctx = NULL,
        .is_websocket = true};

    static const httpd_uri_t uri_generate_204 = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = http_captive_portal_redirect_handler,
    };

    /* Compatibility with vendor probes that omit the underscore. */
    static const httpd_uri_t uri_generate204 = {
        .uri = "/generate204",
        .method = HTTP_GET,
        .handler = http_captive_portal_redirect_handler,
    };

    /* 浏览器默认图标请求：必须拦截，避免落入 404->302->"/" 触发整页重下 */
    static const httpd_uri_t uri_favicon = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = http_no_content_handler,
    };
    esp_err_t err = httpd_register_uri_handler(server, &html_file);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &ws);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &uri_generate_204);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &uri_generate204);
    if (err == ESP_OK) err = httpd_register_uri_handler(server, &uri_favicon);
    if (err == ESP_OK) err = httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,
                                                         http_404_error_handler);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register HTTP handler: %s", esp_err_to_name(err));
      httpd_stop(server);
      return NULL;
    }
    return server;
  }

  ESP_LOGE(TAG, "Error starting websocket server!");
  return NULL;
}

/**
 * @brief Webserver message queue handling task.
 *
 * Continuously waits for messages on the `webserver_message_queue_handle`. Depending on the message type:
 * - On `WEB_MSG_TYPE_CONNECT`, it sets up websocket async response,
 *   registers the client, performs login flow, and sends UI update notifications.
 * - On `WEB_MSG_TYPE_DATA`, it routes the data to the webserver's message handler.
 *
 * The task also logs the current stack high watermark for debugging memory usage.
 *
 * @param pvParameters Unused parameter.
 */
static void webserver_message_handle_task(void *pvParameters)
{
  websocketMsg_t msg = {0};

  while (true) {
    if (xQueueReceive(webserver_message_queue_handle, &(msg), pdMS_TO_TICKS(50))) {
      if (msg.msgType == WEB_MSG_TYPE_CONNECT) {
        ESP_LOGI(TAG, "connect from client sock_fd=%d", msg.resp_arg.fd);
        check_and_register_new_ws_client(msg.resp_arg.hd, msg.resp_arg.fd);
      } else if (msg.msgType == WEB_MSG_TYPE_DATA) {
        /* Set response context and handle received data */
        check_and_register_new_ws_client(msg.resp_arg.hd, msg.resp_arg.fd);
        websocket_set_async_rsp_arg(&msg.resp_arg);
        webserver_data_message_handler((uint8_t *)msg.data, msg.len);
      } else {
        ESP_LOGE(TAG, "WEB_MSG_TYPE error");
      }
    } else {
      /* Timeout: check active WebSocket clients on server */
      if (websocket_server_handle) {
        size_t fds_num = MAX_OPEN_SOCKETS;
        int fds[MAX_OPEN_SOCKETS];
        if (httpd_get_client_list(websocket_server_handle, &fds_num, fds) == ESP_OK) {
          for (size_t i = 0; i < fds_num; i++) {
            if (httpd_ws_get_fd_info(websocket_server_handle, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
              check_and_register_new_ws_client(websocket_server_handle, fds[i]);
            }
          }
        }
      }
    }
  }
}

/**
 * @brief Setup and start the web server components.
 *
 * This includes:
 *  - Registering IP-related event handlers for Wi-Fi and Ethernet connections.
 *  - Creating a mutex to synchronize websocket message sending.
 *  - Initializing the client management list.
 *  - Creating a static message queue for web messages and spawning a handler task.
 *  - Creating a dedicated event loop for web server events and registering event handlers.
 *  - Starting the web server instance.
 *
 * The function logs errors if resources cannot be allocated or tasks fail to start.
 * It does not return an error code; failures are logged for debugging.
 */
void webserver_init(void)
{
  esp_err_t err = esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       got_ip_event_handler,
                                                       &websocket_server_handle,
                                                       &webserver_ip_event_instance);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(err));
    return;
  }

  /* create webserver mutex */
  webserver_send_message_mutex = xSemaphoreCreateMutex();
  if (webserver_send_message_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create webserver_send_message_mutex Mutex!");
    goto cleanup;
  }

  /* create web msg queue and create webserver message handle task */
    memset(webserver_message_queue_storage, 0x0, sizeof(webserver_message_queue_storage));
    webserver_message_queue_handle = xQueueCreateStatic(WEB_MSG_ITEM_NUM, WEB_MSG_ITEM_LEN,
                                                        &(webserver_message_queue_storage[0]),
      &webserver_message_queue_buffer);
  if (webserver_message_queue_handle == NULL) {
    ESP_LOGE(TAG, "Failed to create web msg queue!");
    goto cleanup;
  }

  BaseType_t task_created =
      xTaskCreatePinnedToCore(webserver_message_handle_task, "webMsgDistTask", 4096 * 2,
                              NULL, 5, &webserver_message_task_handle, 1);
  if (task_created != pdPASS) {
    ESP_LOGE(TAG, "webserver_message_handle_task create failed");
    goto cleanup;
  }

  /* register webserver event handler */
  err = esp_event_handler_instance_register(WEB_SERVER_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             webserver_event_handler,
                                             NULL,
                                             &webserver_event_instance);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register webserver event handler: %s", esp_err_to_name(err));
    goto cleanup;
  }

  /* create webserver */
  websocket_server_handle = start_webserver();
  if (!websocket_server_handle) {
    goto cleanup;
  }
  return;

cleanup:
  if (webserver_event_instance) {
    esp_event_handler_instance_unregister(WEB_SERVER_EVENT,
                                           ESP_EVENT_ANY_ID,
                                           webserver_event_instance);
    webserver_event_instance = NULL;
  }
  if (webserver_message_task_handle) {
    vTaskDelete(webserver_message_task_handle);
    webserver_message_task_handle = NULL;
  }
  if (webserver_message_queue_handle) {
    vQueueDelete(webserver_message_queue_handle);
    webserver_message_queue_handle = NULL;
  }
  if (webserver_send_message_mutex) {
    vSemaphoreDelete(webserver_send_message_mutex);
    webserver_send_message_mutex = NULL;
  }
  if (webserver_ip_event_instance) {
    esp_event_handler_instance_unregister(IP_EVENT,
                                           IP_EVENT_STA_GOT_IP,
                                           webserver_ip_event_instance);
    webserver_ip_event_instance = NULL;
  }
}
