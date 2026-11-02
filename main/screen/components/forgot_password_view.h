#ifndef OPEN_NEXTION_MAIN_SCREEN_FORGOT_PASSWORD_VIEW_H
#define OPEN_NEXTION_MAIN_SCREEN_FORGOT_PASSWORD_VIEW_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create the password-recovery confirmation page as a full-size child of parent. */
lv_obj_t *forgot_password_view_create(lv_obj_t *parent);

/** Reapply the password-recovery layout after display rotation. */
void forgot_password_view_apply_orientation(lv_obj_t *view);

/** Reapply colors from the currently selected screen theme. */
void forgot_password_view_apply_theme(lv_obj_t *view);

#ifdef __cplusplus
}
#endif

#endif // OPEN_NEXTION_MAIN_SCREEN_FORGOT_PASSWORD_VIEW_H
