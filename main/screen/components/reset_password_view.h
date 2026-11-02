#ifndef OPEN_NEXTION_MAIN_SCREEN_RESET_PASSWORD_VIEW_H
#define OPEN_NEXTION_MAIN_SCREEN_RESET_PASSWORD_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create the page shown while the browser is allowed to set a new password. */
lv_obj_t *reset_password_view_create(lv_obj_t *parent);

/** Reapply the password-reset layout after display rotation. */
void reset_password_view_apply_orientation(lv_obj_t *view);

/** Reapply colors from the currently selected screen theme. */
void reset_password_view_apply_theme(lv_obj_t *view);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_RESET_PASSWORD_VIEW_H
