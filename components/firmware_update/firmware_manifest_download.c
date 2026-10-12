#include "firmware_manifest_download.h"

#include <stdbool.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define FIRMWARE_MANIFEST_HTTP_TIMEOUT_MS 15000
#define FIRMWARE_MANIFEST_HTTP_BUFFER_SIZE 1024
#define FIRMWARE_MANIFEST_HTTP_TX_BUFFER_SIZE 2048
#define FIRMWARE_MANIFEST_MAX_REDIRECTS 5

typedef struct {
    char *output;
    size_t capacity;
    size_t length;
    bool overflow;
    bool ignore_body;
    unsigned redirects;
} firmware_manifest_http_context_t;

static const char *TAG = "firmware_manifest_http";

static bool firmware_manifest_url_is_https(const char *url)
{
    return url && strncmp(url, "https://", strlen("https://")) == 0;
}

static bool firmware_manifest_url_is_allowed(const char *url)
{
    if (firmware_manifest_url_is_https(url)) {
        return true;
    }
#if OTA_ALLOW_INSECURE_HTTP
    return url && strncmp(url, "http://", strlen("http://")) == 0;
#else
    return false;
#endif
}

static esp_err_t firmware_manifest_http_event_handler(esp_http_client_event_t *event)
{
    firmware_manifest_http_context_t *context = event ? event->user_data : NULL;
    if (!context) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (event->event_id) {
    case HTTP_EVENT_HEADERS_SENT:
        /* A redirected request starts a new response document. */
        context->length = 0;
        context->overflow = false;
        context->ignore_body = false;
        break;

    case HTTP_EVENT_REDIRECT:
        context->redirects++;
        context->ignore_body = true;
        break;

    case HTTP_EVENT_ON_DATA:
        if (context->ignore_body || !event->data || event->data_len <= 0) {
            break;
        }

        size_t data_length = (size_t)event->data_len;
        size_t maximum_length = context->capacity - 1;
        if (context->length > maximum_length ||
            data_length > maximum_length - context->length) {
            context->overflow = true;
            break;
        }

        memcpy(context->output + context->length, event->data, data_length);
        context->length += data_length;
        break;

    default:
        break;
    }

    return ESP_OK;
}

esp_err_t firmware_manifest_download(const char *url,
                                     char *output,
                                     size_t output_capacity,
                                     size_t *response_length)
{
    if (!firmware_manifest_url_is_allowed(url) ||
        !output || output_capacity < 2 || !response_length) {
        return ESP_ERR_INVALID_ARG;
    }

    output[0] = '\0';
    *response_length = 0;

    firmware_manifest_http_context_t context = {
        .output = output,
        .capacity = output_capacity,
    };
    bool use_tls = firmware_manifest_url_is_https(url);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = firmware_manifest_http_event_handler,
        .user_data = &context,
        .timeout_ms = FIRMWARE_MANIFEST_HTTP_TIMEOUT_MS,
        .disable_auto_redirect = false,
        .max_redirection_count = FIRMWARE_MANIFEST_MAX_REDIRECTS,
        .buffer_size = FIRMWARE_MANIFEST_HTTP_BUFFER_SIZE,
        .buffer_size_tx = FIRMWARE_MANIFEST_HTTP_TX_BUFFER_SIZE,
        .crt_bundle_attach = use_tls ? esp_crt_bundle_attach : NULL,
        .keep_alive_enable = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    if (!use_tls) {
        ESP_LOGW(TAG, "Downloading Manifest over insecure HTTP test transport");
    }
    else {
        ESP_LOGI(TAG,
                 "TLS heap before Manifest request: internal_free=%u internal_largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    ESP_LOGI(TAG, "Downloading OTA Manifest: %s", url);
    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);
    int64_t content_length = esp_http_client_get_content_length(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Manifest request failed: %s", esp_err_to_name(err));
    }
    else if (status_code != HttpStatus_Ok) {
        ESP_LOGE(TAG, "Manifest request returned HTTP status %d", status_code);
        err = ESP_ERR_INVALID_RESPONSE;
    }
    else if (content_length > (int64_t)(output_capacity - 1) || context.overflow) {
        ESP_LOGE(TAG,
                 "Manifest response is too large: content_length=%lld limit=%u",
                 (long long)content_length,
                 (unsigned)(output_capacity - 1));
        err = ESP_ERR_INVALID_SIZE;
    }
    else if (context.length == 0) {
        ESP_LOGE(TAG, "Manifest response is empty");
        err = ESP_ERR_INVALID_RESPONSE;
    }

    if (err == ESP_OK) {
        context.output[context.length] = '\0';
        *response_length = context.length;
        ESP_LOGI(TAG,
                 "Manifest downloaded: bytes=%u redirects=%u",
                 (unsigned)context.length,
                 context.redirects);
    }

    esp_http_client_cleanup(client);
    return err;
}
