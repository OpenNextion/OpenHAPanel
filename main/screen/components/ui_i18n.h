#pragma once

#include "lvgl.h"

/* Like lv_label_set_text, but retains a translation key when text was returned
 * by i18n_get/find. Ordinary strings are copied without a translation binding.
 * All functions must run in the LVGL task or while holding the LVGL lock. */
void ui_i18n_label_set_text(lv_obj_t *label, const char *text);
void ui_i18n_refresh_labels(void);
