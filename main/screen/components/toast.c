#include "toast.h"

#include <string.h>

#include "../../assets/ha_ui_assets.h"

#define TOAST_DEFAULT_WIDTH             280
#define TOAST_DEFAULT_HEIGHT            60
#define TOAST_DEFAULT_RADIUS            12
#define TOAST_DEFAULT_BACKGROUND_COLOR  0x202328
#define TOAST_ICON_SIZE                 32
#define TOAST_HORIZONTAL_PADDING        20
#define TOAST_CONTENT_GAP               8

struct toast {
    lv_obj_t *obj;
    lv_timer_t *timer;
    bool closing;
    toast_closed_cb_t closed_cb;
    void *closed_user_data;
};

static void toast_timer_cb(lv_timer_t *timer);
static void toast_delete_event_cb(lv_event_t *event);

toast_t *toast_show(lv_obj_t *parent, const toast_config_t *config)
{
    if (!parent || !config || !config->title) {
        return NULL;
    }

    toast_t *toast = lv_malloc(sizeof(*toast));
    if (!toast) {
        return NULL;
    }
    memset(toast, 0, sizeof(*toast));
    toast->closed_cb = config->closed_cb;
    toast->closed_user_data = config->closed_user_data;

    int32_t width = config->width > 0 ? config->width : TOAST_DEFAULT_WIDTH;
    int32_t height = config->height > 0 ? config->height : TOAST_DEFAULT_HEIGHT;
    int32_t radius = config->radius > 0 ? config->radius : TOAST_DEFAULT_RADIUS;
    uint32_t background_color = config->background_color
                                    ? config->background_color
                                    : TOAST_DEFAULT_BACKGROUND_COLOR;

    toast->obj = lv_obj_create(parent);
    if (!toast->obj) {
        lv_free(toast);
        return NULL;
    }

    lv_obj_remove_flag(toast->obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(toast->obj, width, height);
    lv_obj_align(toast->obj, LV_ALIGN_TOP_MID, config->x, config->y);
    lv_obj_set_style_bg_color(toast->obj, lv_color_hex(background_color), 0);
    lv_obj_set_style_bg_opa(toast->obj, config->background_opa, 0);
    lv_obj_set_style_border_width(toast->obj, 0, 0);
    lv_obj_set_style_radius(toast->obj, radius, 0);
    lv_obj_set_style_shadow_width(toast->obj, 0, 0);
    lv_obj_set_style_pad_hor(toast->obj, TOAST_HORIZONTAL_PADDING, 0);
    lv_obj_set_style_pad_ver(toast->obj, 0, 0);
    lv_obj_set_style_pad_column(toast->obj, TOAST_CONTENT_GAP, 0);
    lv_obj_set_layout(toast->obj, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(toast->obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(toast->obj,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(toast->obj,
                        toast_delete_event_cb,
                        LV_EVENT_DELETE,
                        toast);

    if (config->icon_src) {
        lv_obj_t *icon = lv_image_create(toast->obj);
        lv_image_set_src(icon, config->icon_src);
        lv_obj_set_size(icon, TOAST_ICON_SIZE, TOAST_ICON_SIZE);
        lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *title = lv_label_create(toast->obj);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_set_height(title, lv_font_get_line_height(&ha_font_misan_medium_20));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title, config->title);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_20, 0);

    if (config->behind_siblings) {
        lv_obj_move_to_index(toast->obj, 0);
    }

    if (config->duration_ms > 0) {
        toast->timer = lv_timer_create(toast_timer_cb,
                                       config->duration_ms,
                                       toast);
        if (!toast->timer) {
            lv_obj_delete(toast->obj);
            return NULL;
        }
        lv_timer_set_repeat_count(toast->timer, 1);
    }

    return toast;
}

void toast_close(toast_t *toast)
{
    if (!toast || toast->closing) {
        return;
    }
    toast->closing = true;

    if (toast->timer) {
        lv_timer_delete(toast->timer);
        toast->timer = NULL;
    }
    if (toast->obj) {
        lv_obj_delete_async(toast->obj);
    }
}

static void toast_timer_cb(lv_timer_t *timer)
{
    toast_t *toast = lv_timer_get_user_data(timer);
    if (!toast) {
        return;
    }

    toast->timer = NULL;
    toast->closing = true;
    if (toast->obj) {
        lv_obj_delete_async(toast->obj);
    }
}

static void toast_delete_event_cb(lv_event_t *event)
{
    toast_t *toast = lv_event_get_user_data(event);
    if (!toast) {
        return;
    }

    if (toast->timer) {
        lv_timer_delete(toast->timer);
        toast->timer = NULL;
    }
    toast->obj = NULL;

    if (toast->closed_cb) {
        toast->closed_cb(toast, toast->closed_user_data);
    }
    lv_free(toast);
}
