#ifndef OPEN_NEXTION_MAIN_SCREEN_RESET_PASSWORD_VIEW_H
#define OPEN_NEXTION_MAIN_SCREEN_RESET_PASSWORD_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create the page shown while the browser is allowed to set a new password. */
lv_obj_t *reset_password_view_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_RESET_PASSWORD_VIEW_H
