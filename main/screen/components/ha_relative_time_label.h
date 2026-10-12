#ifndef OPEN_NEXTION_MAIN_SCREEN_HA_RELATIVE_TIME_LABEL_H
#define OPEN_NEXTION_MAIN_SCREEN_HA_RELATIVE_TIME_LABEL_H

#include <time.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Keep an LVGL label synchronized with a relative event time.
 *
 * All bound labels share one LVGL timer. The binding is removed
 * automatically when the label is deleted.
 */
esp_err_t ha_relative_time_label_bind(lv_obj_t *label, time_t event_epoch);

/** Stop updating a label previously bound to a relative event time. */
void ha_relative_time_label_unbind(lv_obj_t *label);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_HA_RELATIVE_TIME_LABEL_H
