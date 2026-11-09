#include "ui_i18n.h"
#include "ha_ui_font.h"
#include "i18n.h"
#include "ha_provisioning_view.h"

#include <string.h>

#include "../../assets/ha_ui_assets.h"
#include "screen_theme.h"
#include "system_status_bar.h"

#define HA_OVERLAY_DELAY_MS 3000

typedef struct {
    lv_obj_t *root;
    lv_obj_t *ha_icon;
    lv_obj_t *title;
    lv_obj_t *instructions;
    lv_obj_t *overlay;
    lv_obj_t *overlay_image;
    lv_obj_t *overlay_message;
    system_status_bar_t *status_bar;
    lv_timer_t *timer;
    ha_provisioning_hint_dismissed_cb_t hint_dismissed_cb;
    void *hint_dismissed_user_data;
    bool hint_dismissed;
} ha_provisioning_view_ctx_t;

static void ha_view_apply_clear_style(lv_obj_t *obj);
static void ha_view_apply_layout(ha_provisioning_view_ctx_t *ctx);
static void ha_view_apply_theme(ha_provisioning_view_ctx_t *ctx);
static void ha_view_dismiss_hint(ha_provisioning_view_ctx_t *ctx);
static void ha_view_timer_cb(lv_timer_t *timer);
static void ha_view_overlay_click_cb(lv_event_t *event);
static void ha_view_root_size_changed_cb(lv_event_t *event);
static void ha_view_root_delete_cb(lv_event_t *event);

lv_obj_t *ha_provisioning_view_create(
    lv_obj_t *parent,
    const ha_provisioning_view_config_t *config)
{
    if (!parent) {
        return NULL;
    }

    ha_provisioning_view_ctx_t *ctx = lv_malloc(sizeof(*ctx));
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
    ha_view_apply_clear_style(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_align(root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x010203), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_user_data(root, ctx);
    lv_obj_add_event_cb(root,
                        ha_view_root_size_changed_cb,
                        LV_EVENT_SIZE_CHANGED,
                        ctx);
    lv_obj_add_event_cb(root, ha_view_root_delete_cb, LV_EVENT_DELETE, ctx);

    ctx->status_bar = system_status_bar_create(root);

    lv_obj_t *ha_icon = lv_image_create(root);
    ctx->ha_icon = ha_icon;
    lv_image_set_src(ha_icon, &hass_60x60);
    lv_obj_remove_flag(ha_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(root);
    ctx->title = title;
    ui_i18n_label_set_text(title, i18n_get(I18N_PROVISION_HA_TITLE));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, ha_ui_font_medium(20), 0);

    lv_obj_t *instructions = lv_label_create(root);
    ctx->instructions = instructions;
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    ui_i18n_label_set_text(instructions,
                      i18n_get(I18N_PROVISION_HA_HELP));
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(instructions, ha_ui_font_regular(16), 0);
    lv_obj_set_style_text_line_space(instructions, 2, 0);

    if (config) {
        ctx->hint_dismissed_cb = config->hint_dismissed_cb;
        ctx->hint_dismissed_user_data = config->hint_dismissed_user_data;
        if (config->show_device_info_hint) {
            ctx->timer = lv_timer_create(ha_view_timer_cb,
                                         HA_OVERLAY_DELAY_MS,
                                         ctx);
        }
        else {
            ctx->hint_dismissed = true;
        }
    }
    else {
        ctx->hint_dismissed = true;
    }

    ha_view_apply_theme(ctx);
    lv_obj_update_layout(root);
    ha_view_apply_layout(ctx);

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

void ha_provisioning_view_apply_orientation(lv_obj_t *root)
{
    ha_provisioning_view_ctx_t *ctx = root ? lv_obj_get_user_data(root) : NULL;
    if (ctx && ctx->root == root) {
        lv_obj_update_layout(root);
        ha_view_apply_layout(ctx);
    }
}

void ha_provisioning_view_apply_theme(lv_obj_t *root)
{
    ha_provisioning_view_ctx_t *ctx = root ? lv_obj_get_user_data(root) : NULL;
    if (ctx && ctx->root == root) {
        ha_view_apply_theme(ctx);
    }
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
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_add_event_cb(overlay, ha_view_overlay_click_cb, LV_EVENT_CLICKED, ctx);


    lv_obj_t *img = lv_img_create(overlay);
    ctx->overlay_image = img;
    lv_img_set_src(img, &scroll_down_108);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *message = lv_label_create(overlay);
    ctx->overlay_message = message;
    ui_i18n_label_set_text(message, i18n_get(I18N_PROVISION_SWIPE_HINT));
    lv_obj_add_flag(message, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_text_font(message, ha_ui_font_regular(16), 0);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(message, 2, 0);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);

    ha_view_apply_theme(ctx);
    ha_view_apply_layout(ctx);
    lv_obj_move_foreground(overlay);
}

static void ha_view_apply_theme(ha_provisioning_view_ctx_t *ctx)
{
    if (!ctx || !ctx->root) {
        return;
    }

    const screen_theme_palette_t *palette = screen_theme_get_palette();

    lv_obj_set_style_bg_color(ctx->root, lv_color_hex(palette->page_bg), 0);

    if (ctx->title) {
        lv_obj_set_style_text_color(ctx->title,
                                    lv_color_hex(palette->text_primary),
                                    0);
    }
    if (ctx->instructions) {
        lv_obj_set_style_text_color(ctx->instructions,
                                    lv_color_hex(palette->text_secondary),
                                    0);
    }
    if (ctx->status_bar) {
        system_status_bar_apply_theme(ctx->status_bar);
    }
    if (ctx->overlay) {
        lv_obj_set_style_bg_color(ctx->overlay,
                                  lv_color_hex(palette->overlay),
                                  0);
    }
    if (ctx->overlay_message) {
        lv_obj_set_style_text_color(ctx->overlay_message,
                                    lv_color_hex(palette->text_on_overlay),
                                    0);
    }

    lv_obj_invalidate(ctx->root);
}

static void ha_view_apply_layout(ha_provisioning_view_ctx_t *ctx)
{
    if (!ctx || !ctx->root || !ctx->ha_icon || !ctx->title ||
        !ctx->instructions) {
        return;
    }

    lv_display_t *display = lv_obj_get_display(ctx->root);
    const bool landscape = lv_display_get_horizontal_resolution(display) >
                           lv_display_get_vertical_resolution(display);
    if (landscape) {
        lv_obj_align(ctx->ha_icon, LV_ALIGN_TOP_MID, 0, 70);

        lv_obj_set_size(ctx->title, 440, LV_SIZE_CONTENT);
        lv_obj_align(ctx->title, LV_ALIGN_TOP_MID, 0, 150);
        lv_obj_set_size(ctx->instructions, 320, LV_SIZE_CONTENT);
    }
    else {
        lv_obj_align(ctx->ha_icon, LV_ALIGN_TOP_MID, 0, 130);

        lv_obj_set_size(ctx->title, 300, LV_SIZE_CONTENT);
        lv_obj_align(ctx->title, LV_ALIGN_TOP_MID, 0, 210);
        lv_obj_set_size(ctx->instructions, 280, LV_SIZE_CONTENT);
    }
    lv_obj_update_layout(ctx->title);
    lv_obj_align_to(ctx->instructions, ctx->title, LV_ALIGN_OUT_BOTTOM_MID,
                    0, 10);

    if (ctx->overlay) {
        lv_obj_set_size(ctx->overlay, LV_PCT(100), LV_PCT(100));
        lv_obj_update_layout(ctx->overlay);
    }
    if (ctx->overlay_image) {
        lv_obj_align(ctx->overlay_image, LV_ALIGN_TOP_MID, 0, 0);
    }
    if (ctx->overlay_message) {
        lv_obj_set_size(ctx->overlay_message, landscape ? 440 : 280,
                        LV_SIZE_CONTENT);
        lv_obj_align(ctx->overlay_message, LV_ALIGN_TOP_MID, 0, 176);
    }

    lv_obj_invalidate(ctx->root);
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
        ctx->overlay_image = NULL;
        ctx->overlay_message = NULL;
        lv_obj_delete_async(overlay);
    }

    if (ctx->hint_dismissed_cb) {
        ctx->hint_dismissed_cb(ctx->hint_dismissed_user_data);
    }
}

static void ha_view_root_size_changed_cb(lv_event_t *event)
{
    ha_provisioning_view_ctx_t *ctx = lv_event_get_user_data(event);
    if (!ctx || lv_event_get_target_obj(event) != ctx->root) {
        return;
    }
    ha_view_apply_layout(ctx);
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
    ctx->overlay_image = NULL;
    ctx->overlay_message = NULL;
    ctx->status_bar = NULL;
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
