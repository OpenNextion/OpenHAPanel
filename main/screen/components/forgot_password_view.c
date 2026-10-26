#include "forgot_password_view.h"

#include <string.h>

#include "../../assets/ha_ui_assets.h"
#include "system_status_bar.h"

#define FORGOT_PASSWORD_TIMEOUT_SECONDS 10
#define FORGOT_PASSWORD_TIMER_PERIOD_MS 1000

typedef struct {
    lv_obj_t *root;
    lv_obj_t *lock_icon;
    lv_obj_t *forget_label;
    lv_obj_t *instructions_label;
    lv_timer_t *timer;
    uint32_t started_at_ms;
} forgot_password_view_ctx_t;

static void forgot_password_view_apply_layout(forgot_password_view_ctx_t *ctx);
static void forgot_password_view_update_countdown(forgot_password_view_ctx_t *ctx);
static void forgot_password_view_timer_cb(lv_timer_t *timer);
static void forgot_password_view_size_changed_cb(lv_event_t *event);
static void forgot_password_view_delete_cb(lv_event_t *event);

lv_obj_t *forgot_password_view_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    forgot_password_view_ctx_t *ctx = lv_malloc(sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }
    memset(ctx, 0, sizeof(*ctx));

    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        lv_free(ctx);
        return NULL;
    }
    ctx->root = root;
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x010101), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(root,
                        forgot_password_view_size_changed_cb,
                        LV_EVENT_SIZE_CHANGED,
                        ctx);
    lv_obj_add_event_cb(root,
                        forgot_password_view_delete_cb,
                        LV_EVENT_DELETE,
                        ctx);

    system_status_bar_create(root);

    lv_obj_t *lock_icon = lv_image_create(root);
    ctx->lock_icon = lock_icon;
    lv_image_set_src(lock_icon, &lock_60x60);
    lv_obj_remove_flag(lock_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *forget_label = lv_label_create(root);
    ctx->forget_label = forget_label;
    lv_label_set_text(forget_label, "Forgot Password");
    lv_obj_set_style_text_color(forget_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(forget_label, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_align(forget_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *instructions_label = lv_label_create(root);
    ctx->instructions_label = instructions_label;
    lv_label_set_long_mode(instructions_label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text_fmt(instructions_label,
                          "Press the device button twice within %ds",
                          FORGOT_PASSWORD_TIMEOUT_SECONDS);
    lv_obj_set_style_text_color(instructions_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(instructions_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_align(instructions_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(instructions_label, 2, 0);

    ctx->started_at_ms = lv_tick_get();
    ctx->timer = lv_timer_create(forgot_password_view_timer_cb,
                                 FORGOT_PASSWORD_TIMER_PERIOD_MS,
                                 ctx);

    lv_obj_update_layout(root);
    forgot_password_view_apply_layout(ctx);

    return root;
}

static void forgot_password_view_apply_layout(forgot_password_view_ctx_t *ctx)
{
    if (!ctx || !ctx->root || !ctx->lock_icon || !ctx->forget_label ||
        !ctx->instructions_label) {
        return;
    }

    const bool landscape = lv_obj_get_width(ctx->root) > lv_obj_get_height(ctx->root);
    if (landscape) {
        lv_obj_align(ctx->lock_icon, LV_ALIGN_TOP_MID, 0, 70);

        lv_obj_set_size(ctx->forget_label, 440, 34);
        lv_obj_set_pos(ctx->forget_label, 20, 150);
        lv_obj_set_size(ctx->instructions_label, 320, 54);
        lv_obj_set_pos(ctx->instructions_label, 80, 187);
    }
    else {
        lv_obj_align(ctx->lock_icon, LV_ALIGN_TOP_LEFT, 130, 130);

        lv_obj_set_size(ctx->forget_label, 280, 34);
        lv_obj_set_pos(ctx->forget_label, 20, 210);
        lv_obj_set_size(ctx->instructions_label, 290, 54);
        lv_obj_set_pos(ctx->instructions_label, 20, 247);
    }

    forgot_password_view_update_countdown(ctx);
    lv_obj_invalidate(ctx->root);
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

    const bool landscape = ctx->root &&
                           lv_obj_get_width(ctx->root) > lv_obj_get_height(ctx->root);
    lv_label_set_text_fmt(ctx->instructions_label,
                          landscape
                              ? "Press the device button twice within\n%lus"
                              : "Press the device button twice within %lus",
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

static void forgot_password_view_size_changed_cb(lv_event_t *event)
{
    forgot_password_view_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx || lv_event_get_target_obj(event) != ctx->root) {
        return;
    }
    forgot_password_view_apply_layout(ctx);
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
    ctx->root = NULL;
    ctx->lock_icon = NULL;
    ctx->forget_label = NULL;
    ctx->instructions_label = NULL;
    lv_free(ctx);
}
