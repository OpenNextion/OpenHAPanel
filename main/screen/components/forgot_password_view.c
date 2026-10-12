#include "forgot_password_view.h"

#include "../../assets/ha_ui_assets.h"
#include "system_status_bar.h"

#define FORGOT_PASSWORD_VIEW_WIDTH  320
#define FORGOT_PASSWORD_VIEW_HEIGHT 480
#define FORGOT_PASSWORD_TIMEOUT_SECONDS 10
#define FORGOT_PASSWORD_TIMER_PERIOD_MS 1000

typedef struct {
    lv_obj_t *instructions_label;
    lv_timer_t *timer;
    uint32_t started_at_ms;
} forgot_password_view_ctx_t;

static void forgot_password_view_update_countdown(forgot_password_view_ctx_t *ctx);
static void forgot_password_view_timer_cb(lv_timer_t *timer);
static void forgot_password_view_delete_cb(lv_event_t *event);

lv_obj_t *forgot_password_view_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, FORGOT_PASSWORD_VIEW_WIDTH, FORGOT_PASSWORD_VIEW_HEIGHT);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x010101), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    system_status_bar_create(root);

    lv_obj_t *lock_icon = lv_image_create(root);
    lv_image_set_src(lock_icon, &lock_60x60);
    lv_obj_set_pos(lock_icon, 130, 130);
    lv_obj_remove_flag(lock_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *forget_label = lv_label_create(root);
    lv_obj_set_size(forget_label, 280, 34);
    lv_obj_set_pos(forget_label, 20, 210);
    lv_label_set_text(forget_label, "Forgot Password");
    lv_obj_set_style_text_color(forget_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(forget_label, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_align(forget_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *instructions_label = lv_label_create(root);
    lv_label_set_long_mode(instructions_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_size(instructions_label, 290, 54);
    lv_obj_set_pos(instructions_label, 20, 247);
    lv_label_set_text_fmt(instructions_label,
                          "Press the device button twice within %ds",
                          FORGOT_PASSWORD_TIMEOUT_SECONDS);
    lv_obj_set_style_text_color(instructions_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(instructions_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_align(instructions_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(instructions_label, 2, 0);

    forgot_password_view_ctx_t *ctx = lv_malloc(sizeof(*ctx));
    if (!ctx) {
        return root;
    }

    ctx->instructions_label = instructions_label;
    ctx->started_at_ms = lv_tick_get();
    ctx->timer = lv_timer_create(forgot_password_view_timer_cb,
                                 FORGOT_PASSWORD_TIMER_PERIOD_MS,
                                 ctx);
    lv_obj_add_event_cb(root, forgot_password_view_delete_cb, LV_EVENT_DELETE, ctx);

    return root;
}

static void forgot_password_view_update_countdown(forgot_password_view_ctx_t *ctx)
{
    if (!ctx || !ctx->instructions_label) {
        return;
    }

    const uint32_t timeout_ms = FORGOT_PASSWORD_TIMEOUT_SECONDS * 1000U;
    uint32_t elapsed_ms = lv_tick_elaps(ctx->started_at_ms);
    uint32_t remaining_ms = elapsed_ms >= timeout_ms ? 0 : timeout_ms - elapsed_ms;
    uint32_t remaining_seconds = (remaining_ms + 999U) / 1000U;

    lv_label_set_text_fmt(ctx->instructions_label,
                          "Press the device button twice within %lus",
                          (unsigned long)remaining_seconds);
}

static void forgot_password_view_timer_cb(lv_timer_t *timer)
{
    forgot_password_view_ctx_t *ctx = lv_timer_get_user_data(timer);
    if (!ctx) {
        return;
    }

    forgot_password_view_update_countdown(ctx);
    if (lv_tick_elaps(ctx->started_at_ms) >= FORGOT_PASSWORD_TIMEOUT_SECONDS * 1000U) {
        ctx->timer = NULL;
        lv_timer_delete(timer);
    }
}

static void forgot_password_view_delete_cb(lv_event_t *event)
{
    forgot_password_view_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx) {
        return;
    }

    if (ctx->timer) {
        lv_timer_delete(ctx->timer);
        ctx->timer = NULL;
    }
    ctx->instructions_label = NULL;
    lv_free(ctx);
}
