#include "more_info_screen.h"

#include <stdio.h>
#include <string.h>

#include "assets/ha_ui_assets.h"
#include "entity_handlers/entity_handler_registry.h"
#include "esp_log.h"
#include "lvgl.h"

#define MORE_INFO_SCREEN_ANIM_MS 250

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *previous_screen;
    lv_obj_t *overlay;
    ha_entity_store_t *store;
    const screen_tile_card_data_t *tile_config;
    char entity_id[HA_ENTITY_STORE_ENTITY_ID_MAX];
    bool loaded;
    bool refresh_pending;
    bool closing;
    entity_more_info_context_t render_context;
} more_info_screen_context_t;

static more_info_screen_context_t s_more_info;
static const char *TAG = "more_info_screen";

static void more_info_screen_build(lv_obj_t *screen);
static bool more_info_screen_refresh(void);
static void more_info_screen_render_default(
    const entity_more_info_context_t *context);
static void more_info_screen_close_cb(void *user_data);
static void more_info_screen_default_close_event_cb(lv_event_t *event);
static void more_info_screen_lifecycle_event_cb(lv_event_t *event);
static void more_info_screen_overlay_delete_event_cb(lv_event_t *event);
static void more_info_screen_log_memory(const char *stage);

bool more_info_screen_open(ha_entity_store_t *store,
                           const char *entity_id,
                           const screen_tile_card_data_t *tile_config)
{
    if (!store || !entity_id || !entity_id[0] || s_more_info.screen) {
        return false;
    }

    more_info_screen_log_memory("before open");

    lv_obj_t *previous_screen = lv_screen_active();
    lv_obj_t *screen = lv_obj_create(NULL);
    if (!previous_screen || !screen) {
        if (screen) {
            lv_obj_delete(screen);
        }
        return false;
    }

    memset(&s_more_info, 0, sizeof(s_more_info));
    s_more_info.screen = screen;
    s_more_info.previous_screen = previous_screen;
    s_more_info.store = store;
    s_more_info.tile_config = tile_config;
    snprintf(s_more_info.entity_id,
             sizeof(s_more_info.entity_id),
             "%s",
             entity_id);

    more_info_screen_build(screen);
    if (!more_info_screen_refresh()) {
        lv_obj_t *overlay = s_more_info.overlay;
        memset(&s_more_info, 0, sizeof(s_more_info));
        if (overlay && lv_obj_is_valid(overlay)) {
            lv_obj_delete(overlay);
        }
        lv_obj_delete(screen);
        return false;
    }
    more_info_screen_log_memory("after build");

    lv_obj_add_event_cb(screen,
                        more_info_screen_lifecycle_event_cb,
                        LV_EVENT_ALL,
                        NULL);
    lv_screen_load_anim(screen,
                        LV_SCREEN_LOAD_ANIM_MOVE_LEFT,
                        MORE_INFO_SCREEN_ANIM_MS,
                        0,
                        false);
    return true;
}

void more_info_screen_close(void)
{
    if (!s_more_info.screen || !s_more_info.previous_screen ||
        s_more_info.closing ||
        !lv_obj_is_valid(s_more_info.previous_screen)) {
        return;
    }

    s_more_info.closing = true;
    lv_screen_load_anim(s_more_info.previous_screen,
                        LV_SCREEN_LOAD_ANIM_MOVE_RIGHT,
                        MORE_INFO_SCREEN_ANIM_MS,
                        0,
                        false);
}

bool more_info_screen_refresh_entity(ha_entity_store_t *store,
                                     const char *entity_id)
{
    if (!s_more_info.screen || s_more_info.closing || !store ||
        !entity_id || strcmp(s_more_info.entity_id, entity_id) != 0) {
        return false;
    }

    s_more_info.store = store;
    if (!s_more_info.loaded) {
        s_more_info.refresh_pending = true;
        return true;
    }
    return more_info_screen_refresh();
}

bool more_info_screen_is_open(void)
{
    return s_more_info.screen != NULL && !s_more_info.closing;
}

static void more_info_screen_build(lv_obj_t *screen)
{
    (void)screen;

    s_more_info.overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_more_info.overlay);
    lv_obj_remove_flag(s_more_info.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_more_info.overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_more_info.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_more_info.overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(s_more_info.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_more_info.overlay,
                        more_info_screen_overlay_delete_event_cb,
                        LV_EVENT_DELETE,
                        NULL);
    lv_obj_move_to_index(s_more_info.overlay, -1);
}

static bool more_info_screen_refresh(void)
{
    if (!s_more_info.screen || !s_more_info.overlay ||
        !s_more_info.store || !s_more_info.entity_id[0]) {
        return false;
    }

    ha_entity_snapshot_t snapshot = {0};
    const bool has_snapshot = ha_entity_store_get(s_more_info.store,
                                                   s_more_info.entity_id,
                                                   &snapshot) == ESP_OK;
    if (!has_snapshot) {
        snprintf(snapshot.entity_id,
                 sizeof(snapshot.entity_id),
                 "%s",
                 s_more_info.entity_id);
    }

    lv_obj_clean(s_more_info.screen);
    lv_obj_clean(s_more_info.overlay);
    s_more_info.render_context = (entity_more_info_context_t) {
        .screen = s_more_info.screen,
        .overlay = s_more_info.overlay,
        .store = s_more_info.store,
        .snapshot = snapshot,
        .tile_config = s_more_info.tile_config,
        .close = more_info_screen_close_cb,
        .close_user_data = NULL,
    };

    const entity_handler_t *handler = entity_handler_registry_find(
        s_more_info.entity_id);
    if (handler && handler->render_more_info) {
        return handler->render_more_info(&s_more_info.render_context) == ESP_OK;
    }

    more_info_screen_render_default(&s_more_info.render_context);
    return true;
}

static void more_info_screen_render_default(
    const entity_more_info_context_t *context)
{
    lv_obj_t *screen = context->screen;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x010203), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(screen, 0, 0);
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));

    lv_obj_t *header_box = lv_obj_create(screen);
    lv_obj_remove_style_all(header_box);
    lv_obj_set_layout(header_box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(header_box, 10, 0);
    lv_obj_set_style_pad_hor(header_box, 20, 0);
    lv_obj_set_style_pad_ver(header_box, 12, 0);
    lv_obj_set_style_border_width(header_box, 0, 0);
    lv_obj_set_size(header_box, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *close_btn = lv_image_create(header_box);
    lv_img_set_src(close_btn, &close_50);
    lv_obj_set_flag(close_btn, LV_OBJ_FLAG_CHECKABLE, true);
    lv_obj_add_event_cb(close_btn,
                        more_info_screen_default_close_event_cb,
                        LV_EVENT_CLICKED,
                        NULL);

    lv_obj_t *close_hitbox = lv_obj_create(context->overlay);
    lv_obj_remove_style_all(close_hitbox);
    lv_obj_remove_flag(close_hitbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(close_hitbox, 50, 50);
    lv_obj_align(close_hitbox, LV_ALIGN_TOP_LEFT, 20, 12);
    lv_obj_add_event_cb(close_hitbox,
                        more_info_screen_default_close_event_cb,
                        LV_EVENT_CLICKED,
                        NULL);

    lv_obj_t *close_label = lv_label_create(header_box);
    lv_label_set_text(close_label,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);
    lv_obj_set_style_text_font(close_label, &ha_font_misan_medium_24, 0);
    lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_flex_grow(close_label, 1);
    lv_label_set_long_mode(close_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(close_label, lv_font_get_line_height(&ha_font_misan_medium_24));

    lv_obj_t *tip = lv_label_create(screen);
    lv_label_set_text(tip, "Unsupport");
    lv_obj_set_style_text_font(tip, &ha_font_misan_medium_24, 0);
    lv_obj_set_style_text_color(tip, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_translate_y(tip, 20, 0);
}

static void more_info_screen_close_cb(void *user_data)
{
    (void)user_data;
    more_info_screen_close();
}

static void more_info_screen_default_close_event_cb(lv_event_t *event)
{
    (void)event;
    more_info_screen_close();
}

static void more_info_screen_lifecycle_event_cb(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != s_more_info.screen) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_SCREEN_LOADED) {
        s_more_info.loaded = true;
        if (s_more_info.overlay && lv_obj_is_valid(s_more_info.overlay)) {
            lv_obj_remove_flag(s_more_info.overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_to_index(s_more_info.overlay, -1);
        }
        if (s_more_info.refresh_pending) {
            s_more_info.refresh_pending = false;
            more_info_screen_refresh();
        }
        more_info_screen_log_memory("loaded");
    }
    else if (code == LV_EVENT_SCREEN_UNLOAD_START) {
        s_more_info.loaded = false;
        if (s_more_info.overlay && lv_obj_is_valid(s_more_info.overlay)) {
            lv_obj_add_flag(s_more_info.overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else if (code == LV_EVENT_SCREEN_UNLOADED) {
        lv_obj_t *screen = s_more_info.screen;
        lv_obj_t *previous_screen = s_more_info.previous_screen;
        lv_obj_t *overlay = s_more_info.overlay;
        const bool returning = s_more_info.closing;
        memset(&s_more_info, 0, sizeof(s_more_info));
        if (overlay && lv_obj_is_valid(overlay)) {
            lv_obj_delete_async(overlay);
        }
        if (!returning && previous_screen &&
            lv_obj_is_valid(previous_screen)) {
            lv_obj_delete_async(previous_screen);
        }
        lv_obj_delete_async(screen);
    }
    else if (code == LV_EVENT_DELETE) {
        lv_obj_t *overlay = s_more_info.overlay;
        memset(&s_more_info, 0, sizeof(s_more_info));
        if (overlay && lv_obj_is_valid(overlay)) {
            lv_obj_delete_async(overlay);
        }
    }
}

static void more_info_screen_overlay_delete_event_cb(lv_event_t *event)
{
    (void)event;
    if (!s_more_info.screen || s_more_info.closing ||
        lv_screen_active() != s_more_info.screen) {
        return;
    }

    lv_obj_t *previous_screen = s_more_info.previous_screen;
    memset(&s_more_info, 0, sizeof(s_more_info));
    if (previous_screen && lv_obj_is_valid(previous_screen)) {
        lv_obj_delete_async(previous_screen);
    }
}

static void more_info_screen_log_memory(const char *stage)
{
    lv_mem_monitor_t monitor = {0};
    lv_mem_monitor(&monitor);
    ESP_LOGI(TAG,
             "LVGL memory %s: free=%u largest=%u used=%u%% frag=%u%%",
             stage ? stage : "",
             (unsigned)monitor.free_size,
             (unsigned)monitor.free_biggest_size,
             (unsigned)monitor.used_pct,
             (unsigned)monitor.frag_pct);
}
