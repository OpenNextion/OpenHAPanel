#include "ha_provisioning_view.h"

#include "../../assets/ha_ui_assets.h"
#include "system_status_bar.h"

#define HA_VIEW_WIDTH  320
#define HA_VIEW_HEIGHT 480
#define HA_OVERLAY_DELAY_MS 3000

typedef struct {
    lv_obj_t *root;
    lv_obj_t *overlay;
    lv_timer_t *timer;
    ha_provisioning_hint_dismissed_cb_t hint_dismissed_cb;
    void *hint_dismissed_user_data;
    bool hint_dismissed;
} ha_provisioning_view_ctx_t;

static void ha_view_apply_clear_style(lv_obj_t *obj);
static void ha_view_dismiss_hint(ha_provisioning_view_ctx_t *ctx);
static void ha_view_timer_cb(lv_timer_t *timer);
static void ha_view_overlay_click_cb(lv_event_t *event);
static void ha_view_root_delete_cb(lv_event_t *event);

lv_obj_t *ha_provisioning_view_create(
    lv_obj_t *parent,
    const ha_provisioning_view_config_t *config)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *root = lv_obj_create(parent);
    ha_view_apply_clear_style(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, HA_VIEW_WIDTH, HA_VIEW_HEIGHT);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x010101), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    system_status_bar_create(root);

    lv_obj_t *ha_icon = lv_image_create(root);
    lv_image_set_src(ha_icon, &hass_60x60);
    lv_obj_set_pos(ha_icon, 130, 130);
    lv_obj_remove_flag(ha_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(root);
    lv_obj_set_size(title, 300, 34);
    lv_obj_set_pos(title, 10, 210);
    lv_label_set_text(title, "Connect to Home Assistant");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_20, 0);

    lv_obj_t *instructions = lv_label_create(root);
    lv_obj_set_size(instructions, 280, 28);
    lv_obj_set_pos(instructions, 20, 247);
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    lv_label_set_text(instructions,
                      "Set up HA in the Web Console");
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(instructions, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(instructions, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_line_space(instructions, 2, 0);

    if (!config || !config->show_device_info_hint) {
        return root;
    }

    ha_provisioning_view_ctx_t *ctx = lv_malloc(sizeof(*ctx));
    if (!ctx) {
        return root;
    }

    ctx->root = root;
    ctx->overlay = NULL;
    ctx->hint_dismissed_cb = config->hint_dismissed_cb;
    ctx->hint_dismissed_user_data = config->hint_dismissed_user_data;
    ctx->hint_dismissed = false;
    lv_obj_set_user_data(root, ctx);
    ctx->timer = lv_timer_create(ha_view_timer_cb, HA_OVERLAY_DELAY_MS, ctx);
    lv_obj_add_event_cb(root, ha_view_root_delete_cb, LV_EVENT_DELETE, ctx);

    return root;
}

void ha_provisioning_view_dismiss_hint(lv_obj_t *root)
{
    if (!root) {
        return;
    }

    ha_provisioning_view_ctx_t *ctx = lv_obj_get_user_data(root);
    if (!ctx || ctx->root != root) {
        return;
    }

    ha_view_dismiss_hint(ctx);
}

static void ha_view_timer_cb(lv_timer_t *timer)
{
    ha_provisioning_view_ctx_t *ctx = lv_timer_get_user_data(timer);
    if (!ctx || !ctx->root || ctx->overlay) {
        return;
    }

    lv_obj_t *overlay = lv_obj_create(ctx->root);
    if (!overlay) {
        lv_timer_delete(timer);
        ctx->timer = NULL;
        return;
    }

    ctx->overlay = overlay;
    lv_timer_pause(timer);

    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(overlay, HA_VIEW_WIDTH, HA_VIEW_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_add_event_cb(overlay, ha_view_overlay_click_cb, LV_EVENT_CLICKED, ctx);


    lv_obj_t *img = lv_img_create(overlay);
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 118, 0);
    lv_img_set_src(img, &scroll_down_108);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *message = lv_label_create(overlay);
    lv_obj_align(message, LV_ALIGN_TOP_LEFT, 30, 176);
    lv_label_set_text(message, "Swipe down for device info");
    lv_obj_add_flag(message, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_text_color(message, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(message, &ha_font_misan_regular_20, 0);

    lv_obj_move_foreground(overlay);
}

static void ha_view_overlay_click_cb(lv_event_t *event)
{
    ha_provisioning_view_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx) {
        return;
    }

    ha_view_dismiss_hint(ctx);
}

static void ha_view_dismiss_hint(ha_provisioning_view_ctx_t *ctx)
{
    if (!ctx || ctx->hint_dismissed) {
        return;
    }

    ctx->hint_dismissed = true;

    if (ctx->timer) {
        lv_timer_delete(ctx->timer);
        ctx->timer = NULL;
    }
    if (ctx->overlay) {
        lv_obj_t *overlay = ctx->overlay;
        ctx->overlay = NULL;
        lv_obj_delete_async(overlay);
    }

    if (ctx->hint_dismissed_cb) {
        ctx->hint_dismissed_cb(ctx->hint_dismissed_user_data);
    }
}

static void ha_view_root_delete_cb(lv_event_t *event)
{
    ha_provisioning_view_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx) {
        return;
    }

    if (ctx->timer) {
        lv_timer_delete(ctx->timer);
        ctx->timer = NULL;
    }
    ctx->root = NULL;
    ctx->overlay = NULL;
    lv_obj_set_user_data(lv_event_get_target_obj(event), NULL);
    lv_free(ctx);
}

static void ha_view_apply_clear_style(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
}
