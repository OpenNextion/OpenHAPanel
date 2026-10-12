#ifndef OPEN_NEXTION_MAIN_ENTITY_HANDLERS_HA_DATETIME_FORMAT_H
#define OPEN_NEXTION_MAIN_ENTITY_HANDLERS_HA_DATETIME_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Format a Home Assistant ISO 8601 datetime as English relative time.
 *
 * Missing HA values produce "Never". Invalid datetimes produce
 * "Invalid date" and return an error while still leaving useful text in the
 * output buffer.
 */
esp_err_t ha_datetime_format_relative(const char *raw_value,
                                      time_t now_epoch,
                                      char *output,
                                      size_t output_size);

/** Format an already parsed event epoch relative to the supplied time. */
esp_err_t ha_datetime_format_relative_epoch(time_t event_epoch,
                                            time_t now_epoch,
                                            char *output,
                                            size_t output_size);

/** Format relative to the current synchronized system time. */
esp_err_t ha_datetime_format_relative_now(const char *raw_value,
                                          char *output,
                                          size_t output_size);

/** Parse a Home Assistant ISO 8601 datetime into a UTC epoch. */
esp_err_t ha_datetime_parse_epoch(const char *raw_value,
                                  time_t *epoch_out);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_ENTITY_HANDLERS_HA_DATETIME_FORMAT_H
