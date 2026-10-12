#pragma once

#include <stddef.h>

#include "esp_err.h"

/**
 * Download one complete Manifest document over verified HTTPS.
 *
 * output_capacity includes space for the trailing NUL. response_length does
 * not include that terminator. Redirects are handled by esp_http_client and
 * HTTPS-to-HTTP downgrade redirects are rejected by ESP-IDF.
 */
esp_err_t firmware_manifest_download(const char *url,
                                     char *output,
                                     size_t output_capacity,
                                     size_t *response_length);
