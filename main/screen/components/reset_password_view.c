#include "reset_password_view.h"

#include <string.h>

#include "../../assets/ha_ui_assets.h"
#include "system_status_bar.h"

#define RESET_PASSWORD_TIMEOUT_SECONDS 300
#define RESET_PASSWORD_TIMER_PERIOD_MS 1000

typedef struct {
    lv_obj_t *root;
    lv_obj_t *lock_icon;
    lv_obj_t *title;
    lv_obj_t *instructions;
    lv_obj_t *countdown_card;
    lv_obj_t *countdown_label;
    lv_timer_t *timer;
    uint32_t started_at_ms;
} reset_password_view_ctx_t;

static void reset_password_view_apply_layout(reset_password_view_ctx_t *ctx);
static void reset_password_view_update_countdown(reset_password_view_ctx_t *ctx);
static void reset_password_view_timer_cb(lv_timer_t *timer);
static void reset_password_view_size_changed_cb(lv_event_t *event);
static void reset_password_view_delete_cb(lv_event_t *event);

lv_obj_t *reset_password_view_create(lv_obj_t *parent)
{
    if (!parent) {
        return NULL;
    }

    reset_password_view_ctx_t *ctx = lv_malloc(sizeof(*ctx));
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
                        reset_password_view_size_changed_cb,
                        LV_EVENT_SIZE_CHANGED,
                        ctx);
    lv_obj_add_event_cb(root,
                        reset_password_view_delete_cb,
                        LV_EVENT_DELETE,
                        ctx);

    system_status_bar_create(root);

    lv_obj_t *lock_icon = lv_image_create(root);
    ctx->lock_icon = lock_icon;
    lv_image_set_src(lock_icon, &lock_60x60);
    lv_obj_remove_flag(lock_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(root);
    ctx->title = title;
    lv_label_set_text(title, "Forgot Password");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_20, 0);

    lv_obj_t *instructions = lv_label_create(root);
    ctx->instructions = instructions;
    lv_label_set_text(instructions, "Set a new password within 5 min");
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(instructions, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(instructions, &ha_font_misan_regular_16, 0);

    lv_obj_t *countdown_card = lv_obj_create(root);
    ctx->countdown_card = countdown_card;
    lv_obj_remove_flag(countdown_card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(countdown_card, 248, 48);
    lv_obj_set_style_radius(countdown_card, 12, 0);
    lv_obj_set_style_border_width(countdown_card, 0, 0);
    lv_obj_set_style_pad_all(countdown_card, 0, 0);
    lv_obj_set_style_bg_color(countdown_card, lv_color_hex(0x202328), 0);
    lv_obj_set_style_bg_opa(countdown_card, LV_OPA_COVER, 0);
    lv_obj_set_layout(countdown_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(countdown_card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(countdown_card,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(countdown_card, 8, 0);

    lv_obj_t *hourglass_icon = lv_image_create(countdown_card);
    lv_image_set_src(hourglass_icon, &hourglass_24x24);
    lv_obj_remove_flag(hourglass_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *countdown_label = lv_label_create(countdown_card);
    ctx->countdown_label = countdown_label;
    lv_label_set_text_fmt(countdown_label, "%ds", RESET_PASSWORD_TIMEOUT_SECONDS);
    lv_obj_set_style_text_color(countdown_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(countdown_label, &ha_font_misan_regular_16, 0);

    ctx->started_at_ms = lv_tick_get();
    ctx->timer = lv_timer_create(reset_password_view_timer_cb,
                                 RESET_PASSWORD_TIMER_PERIOD_MS,
                                 ctx);

    lv_obj_update_layout(root);
    reset_password_view_apply_layout(ctx);

    return root;
}

static void reset_password_view_apply_layout(reset_password_view_ctx_t *ctx)
{
    if (!ctx || !ctx->root || !ctx->lock_icon || !ctx->title ||
        !ctx->instructions || !ctx->countdown_card) {
        return;
    }

    const bool landscape = lv_obj_get_width(ctx->root) > lv_obj_get_height(ctx->root);
    if (landscape) {
        lv_obj_align(ctx->lock_icon, LV_ALIGN_TOP_MID, 0, 70);

        lv_obj_set_size(ctx->title, 440, 34);
        lv_obj_set_pos(ctx->title, 20, 150);
        lv_obj_set_size(ctx->instructions, 440, 28);
        lv_obj_set_pos(ctx->instructions, 20, 187);
        lv_obj_align(ctx->countdown_card, LV_ALIGN_TOP_MID, 0, 214);
    }
    else {
        lv_obj_align(ctx->lock_icon, LV_ALIGN_TOP_LEFT, 130, 130);

        lv_obj_set_size(ctx->title, 280, 34);
        lv_obj_set_pos(ctx->title, 20, 210);
        lv_obj_set_size(ctx->instructions, 280, 28);
        lv_obj_set_pos(ctx->instructions, 20, 247);
        lv_obj_align(ctx->countdown_card, LV_ALIGN_TOP_LEFT, 36, 282);
    }

    lv_obj_invalidate(ctx->root);
}

static void reset_password_view_update_countdown(reset_password_view_ctx_t *ctx)
{
    if (!ctx || !ctx->countdown_label) {
        return;
    }

    const uint32_t timeout_ms = RESET_PASSWORD_TIMEOUT_SECONDS * 1000U;
    uint32_t elapsed_ms = lv_tick_elaps(ctx->started_at_ms);
    uint32_t remaining_ms = elapsed_ms >= timeout_ms ? 0 : timeout_ms - elapsed_ms;
    uint32_t remaining_seconds = (remaining_ms + 999U) / 1000U;

    lv_label_set_text_fmt(ctx->countdown_label, "%lus", (unsigned long)remaining_seconds);
}

static void reset_password_view_timer_cb(lv_timer_t *timer)
{
    reset_password_view_ctx_t *ctx = lv_timer_get_user_data(timer);
    if (!ctx) {
        return;
    }

    reset_password_view_update_countdown(ctx);
    if (lv_tick_elaps(ctx->started_at_ms) >= RESET_PASSWORD_TIMEOUT_SECONDS * 1000U) {
        ctx->timer = NULL;
        lv_timer_delete(timer);
    }
}

static void reset_password_view_size_changed_cb(lv_event_t *event)
{
    reset_password_view_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx || lv_event_get_target_obj(event) != ctx->root) {
        return;
    }
    reset_password_view_apply_layout(ctx);
}

static void reset_password_view_delete_cb(lv_event_t *event)
{
    reset_password_view_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx) {
        return;
    }

    if (ctx->timer) {
        lv_timer_delete(ctx->timer);
        ctx->timer = NULL;
    }
    ctx->root = NULL;
    ctx->lock_icon = NULL;
    ctx->title = NULL;
    ctx->instructions = NULL;
    ctx->countdown_card = NULL;
    ctx->countdown_label = NULL;
    lv_free(ctx);
}
