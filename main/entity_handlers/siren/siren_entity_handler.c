#include "siren_entity_handler.h"

#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"
#include "entity_handlers/ha_datetime_format.h"
#include "esp_log.h"
#include "screen/components/ha_relative_time_label.h"

static const char *TAG = "siren_entity_handler";

#define SIREN_TILE_BG              0x1E2228
#define SIREN_TILE_ICON_OFF        0x999999
#define SIREN_TILE_ICON_ON         0xF44336
#define SIREN_TILE_TITLE           0xFFFFFF
#define SIREN_TILE_STATE           0x666666
#define SIREN_DETAIL_CONTENT_WIDTH 280
/* track: rgba(153,153,153,.2) off / rgba(244,66,54,.2) on */
#define SIREN_TRACK_COLOR_OFF 0x999999
#define SIREN_TRACK_COLOR_ON  0xF44236
#define SIREN_TRACK_OPA       51

/* knob: #888888 off / #F44236 on */
#define SIREN_KNOB_COLOR_OFF 0x888888
#define SIREN_KNOB_COLOR_ON  0xF44236

#define SIREN_TRACK_RADIUS  20

/* Unavailable is the Off palette with the whole control faded to 50%.
 * The track and knob colours are pre-blended against the page background
 * instead of stacked opacities: the knob stays opaque so the ring mask painted
 * on top of it does not show up as a patch, and the white graphic alone keeps a
 * real 50% opacity.
 *   track: #999999 @ 10% over #010203 -> #101112
 *   knob:  #888888 @ 50% over #010203 -> #454545 */
#define SIREN_DISABLED_TRACK_COLOR 0x101112
#define SIREN_DISABLED_KNOB_COLOR  0x454545
#define SIREN_DISABLED_GLYPH_OPA   LV_OPA_50

/* Vertical switch: 88x230 track, 76x109 knob, both rounded 20. */
#define SIREN_TRACK_WIDTH   88
#define SIREN_TRACK_HEIGHT  230
#define SIREN_KNOB_WIDTH    76
#define SIREN_KNOB_HEIGHT   109
#define SIREN_KNOB_RADIUS   20

/* On = knob parked at the top end. Flip to 0 to mirror the direction. */
#define SIREN_KNOB_ON_TOP 1

#define SIREN_TRACK_INSET   6

/* Knob graphic: 14px, white, 2px stroke, never filled. */
#define SIREN_GLYPH_SIZE   14
#define SIREN_GLYPH_STROKE 2

/* Press feedback.
 *
 * Home Assistant never flips a switch before the device confirms, so the press
 * itself is the only immediate signal the user gets; its toggle paints the
 * track brighter while the finger is down and settles back on release. This
 * control does the same, and for the same reason: on a slow link the round trip
 * to HA can take seconds, and without a pressed state nothing at all moves
 * between the tap and the new state arriving.
 *
 * The track therefore thickens on LV_STATE_PRESSED. The knob is left alone,
 * exactly as in HA, and it carries no pressed style of its own because LVGL
 * does not propagate the state to children - the knob is not the press target
 * (it has no LV_OBJ_FLAG_CLICKABLE), the track is.
 *
 * The unavailable palette is already opaque, so opacity cannot brighten it;
 * it steps to a lighter grey instead. */
#define SIREN_TRACK_OPA_PRESSED            102
#define SIREN_DISABLED_TRACK_COLOR_PRESSED 0x1A1C1E

/* Knob y-position range computed from the track/knob geometry. */
#define SIREN_KNOB_Y_ON  SIREN_TRACK_INSET
#define SIREN_KNOB_Y_OFF (SIREN_TRACK_HEIGHT - SIREN_KNOB_HEIGHT - SIREN_TRACK_INSET)

/* Shared style-transition descriptors so every rebuild reuses the same animation
 * config without heap churn. One-off-init is safe: the detail page is always
 * built on the LVGL task. */
static lv_style_transition_dsc_t s_siren_track_trans;
static lv_style_transition_dsc_t s_siren_knob_trans;
static lv_style_t s_siren_track_trans_style;
static lv_style_t s_siren_knob_trans_style;
static bool s_siren_trans_inited = false;

static void siren_more_info_close_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (context && context->close) {
        context->close(context->close_user_data);
    }
}

static void siren_call_reverse(const char *entity_id, const char *source)
{
    if (!entity_id || !entity_id[0]) {
        ESP_LOGW(TAG, "%s reverse ignored: empty entity_id", source);
        return;
    }

    ha_entity_store_t *store = app_get_ha_entity_store();
    if (!store) {
        ESP_LOGW(TAG, "%s reverse for %s failed: no entity store",
                 source, entity_id);
        return;
    }

    ha_entity_snapshot_t snapshot;
    const char *service = "turn_on";
    if (ha_entity_store_get(store, entity_id, &snapshot) == ESP_OK
        && strcmp(snapshot.state, "on") == 0) {
        service = "turn_off";
    }

    esp_err_t err = app_ha_call_entity_service("siren", service, entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "%s siren.%s for %s failed to send: %s",
                 source,
                 service,
                 entity_id,
                 esp_err_to_name(err));
    }
    else {
        ESP_LOGI(TAG, "%s siren.%s sent for %s", source, service, entity_id);
    }
}

static bool siren_handle_action(const entity_action_context_t *context)
{
    if (entity_handler_handle_more_info_action(context)) {
        return true;
    }

    if (!context || !context->entity_id) {
        return false;
    }
    const screen_action_t *action = entity_handler_get_action(context);
    if (!action || action->action != SCREEN_ACTION_TOGGLE) {
        return false;
    }
    siren_call_reverse(context->entity_id, "tile icon:");
    return true;
}

static esp_err_t siren_render_tile(ha_entity_store_t *store,
                        const ha_entity_snapshot_t *snapshot,
                        entity_display_model_t *model)
{
    (void)store;
    if (!snapshot || !model) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *icon_default = MDI_SYMBOL_BULLHORN;
    ESP_LOGI(TAG,
             "tile icon probe: entity=%s state='%s' name='%s' "
             "device_class='%s'",
             snapshot->entity_id,
             snapshot->state,
             snapshot->friendly_name,
             snapshot->device_class);
    entity_handler_apply_friendly_name(snapshot, model);
    model->icon = icon_default;
    const bool is_on = strcmp(snapshot->state, "on") == 0;
    model->background_color = lv_color_hex(SIREN_TILE_BG);
    model->icon_color =
        lv_color_hex(is_on ? SIREN_TILE_ICON_ON : SIREN_TILE_ICON_OFF);
    // model->icon_circle_background_color =
    //     lv_color_hex(is_on ? SIREN_TILE_CIRCLE_ON : SIREN_TILE_CIRCLE_OFF);
    model->title_color = lv_color_hex(SIREN_TILE_TITLE);
    model->state_color = lv_color_hex(SIREN_TILE_STATE);
    if (entity_handler_snapshot_is_service_unavailable(snapshot)) {
        entity_handler_copy_snapshot_state(model->state_text,
                                           sizeof(model->state_text),
                                           snapshot);
        model->icon_color = lv_color_hex(0x670425);
        // model->icon_circle_background_color = lv_color_hex();
        return ESP_OK;
    }
    entity_handler_copy_snapshot_state(model->state_text,
                                       sizeof(model->state_text),
                                       snapshot);
    return ESP_OK;
}

static lv_obj_t *siren_more_info_build_page(
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
    lv_obj_set_flex_align(screen,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(screen, 0, 0);

    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 20, 0);
    lv_obj_set_style_pad_ver(header, 12, 0);
    lv_obj_set_style_pad_gap(header, 10, 0);

    lv_obj_t *close_button = lv_image_create(header);
    lv_img_set_src(close_button, &close_50);
    lv_obj_add_flag(close_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_button,
                        siren_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_set_flex_grow(title, 1);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_label_set_text(title,
                      context->snapshot.friendly_name[0]
                          ? context->snapshot.friendly_name
                          : context->snapshot.entity_id);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    /* Pin the label to exactly one line. LV_LABEL_LONG_DOT only ellipsises
     * when the text is taller than the widget, and LV_SIZE_CONTENT would let
     * the height grow with the wrapped text instead - a long friendly name
     * then takes two rows and pushes the whole page down. */
    lv_obj_set_height(title, lv_font_get_line_height(&ha_font_misan_medium_20));

    lv_obj_t *close_hitbox = lv_obj_create(context->overlay);
    lv_obj_remove_style_all(close_hitbox);
    lv_obj_remove_flag(close_hitbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(close_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(close_hitbox, 50, 50);
    lv_obj_align(close_hitbox, LV_ALIGN_TOP_LEFT, 20, 12);
    lv_obj_add_event_cb(close_hitbox,
                        siren_more_info_close_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_remove_style_all(content);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(content, SIREN_DETAIL_CONTENT_WIDTH);
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_top(content, 4, 0);
    lv_obj_set_style_pad_bottom(content, 0, 0);
    lv_obj_set_style_pad_hor(content, 0, 0);
    /* Gap between the state block and the track, matching the design. */
    lv_obj_set_style_pad_gap(content, 20, 0);
    return content;
}

static void siren_build_state_block(lv_obj_t *parent,
                                     const entity_more_info_context_t *context)
{
    lv_obj_t *block = lv_obj_create(parent);
    lv_obj_remove_style_all(block);
    lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(block, LV_PCT(100));
    lv_obj_set_height(block, LV_SIZE_CONTENT);
    lv_obj_set_layout(block, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(block,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(block, 0, 0);
    /* The design keeps the state line and the relative time tight together,
     * the breathing room lives below the block instead (see content gap). */
    lv_obj_set_style_pad_gap(block, 2, 0);

    char state_text[ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
    entity_handler_copy_snapshot_state(state_text,
                                       sizeof(state_text),
                                       &context->snapshot);

    lv_obj_t *state = lv_label_create(block);
    lv_label_set_text(state, state_text);
    lv_obj_set_style_text_font(state, &ha_font_misan_medium_20, 0);
    lv_obj_set_style_text_color(state, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(state, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(state,
                    LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_medium_20));

    /* "24 minutes ago" comes from HA last_changed, not from the local receive
     * time. The label is bound to the shared relative-time component so it keeps
     * ticking while the page stays open, matching scene/button.
     *
     * ha_datetime_format_relative_now() writes its diagnostic into the buffer
     * even when it fails ("Invalid date" when the clock has not been set yet,
     * i.e. before SNTP lands), so the return value decides whether the line is
     * shown at all. Dropping it beats printing an error string where the design
     * expects a timestamp. Binding is skipped in that case as well: the
     * component would re-publish the same diagnostic text every second. */
    char time_text[48] = {0};
    time_t changed_epoch = 0;
    const bool has_time =
        context->snapshot.last_changed[0] &&
        ha_datetime_format_relative_now(context->snapshot.last_changed,
                                        time_text,
                                        sizeof(time_text)) == ESP_OK &&
        ha_datetime_parse_epoch(context->snapshot.last_changed,
                                &changed_epoch) == ESP_OK;
    if (!has_time) {
        time_text[0] = '\0';
    }
    
    lv_obj_t *time = lv_label_create(block);
    lv_label_set_text(time, time_text);
    lv_obj_set_style_text_font(time, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(time, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(time,
                    LV_PCT(100),
                    lv_font_get_line_height(&ha_font_misan_regular_16));
    if (time_text[0]) {
        /* Auto-unbinds on LV_EVENT_DELETE, so the more-info rebuild below is
         * safe without an explicit teardown. */
        ha_relative_time_label_bind(time, changed_epoch);
    }
    else {
        lv_obj_add_flag(time, LV_OBJ_FLAG_HIDDEN);
    }
}

/* The cached snapshot is a hint for how a control is *painted*, never an
 * authority on whether it may be *used*. Home Assistant owns that decision and
 * answers on the service call itself.
 *
 * Gating the call on the cache used to strand the switch: an entity that had
 * not been synced yet, or whose last known state was "unknown"/"unavailable",
 * left the control permanently inert. A reboot only cured it because it forced
 * a full get_states sync - so the same staleness will come back on any later
 * reconnect. Ask for a fresh sync so the page can correct itself, then send the
 * call regardless. */
static void siren_request_store_sync(void)
{
    ha_entity_store_t *store = app_get_ha_entity_store();
    if (store) {
        ha_entity_store_sync(store);
    }
}

static bool siren_entity_snapshot_is_on(const ha_entity_snapshot_t *snapshot)
{
    return snapshot && strcmp(snapshot->state, "on") == 0;
}

static void siren_more_info_control_event_cb(lv_event_t *event)
{
    const entity_more_info_context_t *context = lv_event_get_user_data(event);
    if (!context || !context->snapshot.entity_id[0]) {
        ESP_LOGW(TAG, "detail control tapped but context/entity_id is empty");
        return;
    }
    if (entity_handler_snapshot_is_service_unavailable(&context->snapshot)) {
        ESP_LOGW(TAG,
                 "detail control: cached state '%s' (sync_status=%d) looks "
                 "unavailable for %s, refreshing the cache and sending anyway",
                 context->snapshot.state,
                 (int)context->snapshot.sync_status,
                 context->snapshot.entity_id);
        siren_request_store_sync();
    }
    siren_call_reverse(context->snapshot.entity_id, "detail control:");
}

/* Knob y-position animation: mirrors HA's .switch .button { transition: transform
 * 180ms ease-in-out } translateY. The knob is always aligned TOP_MID so the
 * animated int32_t is the y inset from the top of the track. */
static void siren_knob_y_anim_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    lv_obj_set_y(obj, v);
}

static void siren_build_control(lv_obj_t *parent,
                                 const entity_more_info_context_t *context)
{
    const bool unavailable = entity_handler_snapshot_is_service_unavailable(
        &context->snapshot);
    const bool is_on = !unavailable && siren_entity_snapshot_is_on(&context->snapshot);

    lv_color_t track_color = lv_color_hex(SIREN_TRACK_COLOR_OFF);
    lv_opa_t track_opa = SIREN_TRACK_OPA;
    lv_color_t knob_color = lv_color_hex(SIREN_KNOB_COLOR_OFF);
    lv_opa_t glyph_opa = LV_OPA_COVER;
    
    if (unavailable) {
        track_color = lv_color_hex(SIREN_DISABLED_TRACK_COLOR);
        track_opa = LV_OPA_COVER;
        knob_color = lv_color_hex(SIREN_DISABLED_KNOB_COLOR);
        glyph_opa = SIREN_DISABLED_GLYPH_OPA;
    }
    else if (is_on) {
        track_color = lv_color_hex(SIREN_TRACK_COLOR_ON);
        knob_color = lv_color_hex(SIREN_KNOB_COLOR_ON);
    }

    /* One-off init of the track & knob colour-transition descriptors.
     * Mirrors HA .switch .background { transition: background-color 180ms }
     *        and .switch .button     { transition: background-color 180ms }. */
    if (!s_siren_trans_inited) {
        static const lv_style_prop_t track_props[] = {
            LV_STYLE_BG_COLOR,
            LV_STYLE_BG_OPA,
            LV_STYLE_PROP_INV,
        };
        static const lv_style_prop_t knob_props[] = {
            LV_STYLE_BG_COLOR,
            LV_STYLE_PROP_INV,
        };
        lv_style_init(&s_siren_track_trans_style);
        lv_style_transition_dsc_init(&s_siren_track_trans, track_props,
                                      lv_anim_path_ease_in_out, 180, 0, NULL);
        lv_style_set_transition(&s_siren_track_trans_style, &s_siren_track_trans);

        lv_style_init(&s_siren_knob_trans_style);
        lv_style_transition_dsc_init(&s_siren_knob_trans, knob_props,
                                      lv_anim_path_ease_in_out, 180, 0, NULL);
        lv_style_set_transition(&s_siren_knob_trans_style, &s_siren_knob_trans);
        s_siren_trans_inited = true;
    }

    lv_obj_t *track = lv_obj_create(parent);
    lv_obj_remove_style_all(track);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_set_size(track, SIREN_TRACK_WIDTH, SIREN_TRACK_HEIGHT);
    lv_obj_set_style_radius(track, SIREN_TRACK_RADIUS, 0);
    lv_obj_set_style_bg_color(track, track_color, 0);
    lv_obj_set_style_bg_opa(track, track_opa, 0);
    lv_obj_add_style(track, &s_siren_track_trans_style, 0);
    if (unavailable) {
        lv_obj_set_style_bg_color(track,
                                  lv_color_hex(SIREN_DISABLED_TRACK_COLOR_PRESSED),
                                  LV_STATE_PRESSED);
    }
    else {
        lv_obj_set_style_bg_opa(track, SIREN_TRACK_OPA_PRESSED, LV_STATE_PRESSED);
    }
    lv_obj_add_event_cb(track,
                        siren_more_info_control_event_cb,
                        LV_EVENT_CLICKED,
                        (void *)context);
    lv_obj_t *knob = lv_obj_create(track);
    lv_obj_remove_style_all(knob);
    lv_obj_remove_flag(knob, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(knob, SIREN_KNOB_WIDTH, SIREN_KNOB_HEIGHT);
    lv_obj_set_style_radius(knob, SIREN_KNOB_RADIUS, 0);
    lv_obj_set_style_border_width(knob, 0, 0);
    lv_obj_set_style_bg_color(knob, knob_color, 0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_add_style(knob, &s_siren_knob_trans_style, 0);

    /* Knob position: first place at the *opposite* of the target, then animate
     * to target — mirrors HA .switch .button { transition: transform 180ms }.
     *
     * Always align TOP_MID so y = inset from track top, giving a single-axis
     * range [SIREN_KNOB_Y_ON, SIREN_KNOB_Y_OFF] for the animator. */
    const int32_t target_y = is_on ? SIREN_KNOB_Y_ON : SIREN_KNOB_Y_OFF;
    const int32_t start_y  = is_on ? SIREN_KNOB_Y_OFF : SIREN_KNOB_Y_ON;
    lv_obj_align(knob, LV_ALIGN_TOP_MID, 0, start_y);

    lv_anim_t knob_anim;
    lv_anim_init(&knob_anim);
    lv_anim_set_var(&knob_anim, knob);
    lv_anim_set_exec_cb(&knob_anim, siren_knob_y_anim_cb);
    lv_anim_set_values(&knob_anim, start_y, target_y);
    lv_anim_set_time(&knob_anim, 180);
    lv_anim_set_path_cb(&knob_anim, lv_anim_path_ease_in_out);
    lv_anim_start(&knob_anim);
    lv_obj_t *icon_label = lv_label_create(knob);
    lv_obj_set_style_text_font(icon_label, &ha_font_mdi_20, 0);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(icon_label, glyph_opa, 0);
    lv_obj_center(icon_label);
    lv_label_set_text(icon_label,
                      is_on ? MDI_SYMBOL_VOLUME_HIGH : MDI_SYMBOL_VOLUME_OFF);
}

static esp_err_t siren_render_more_info(
    const entity_more_info_context_t *context)
{
    if (!context || !context->screen || !context->overlay ||
        !context->close) {
        return ESP_ERR_INVALID_ARG;
    }
    lv_obj_t *content = siren_more_info_build_page(context);  
    siren_build_state_block(content, context);
    siren_build_control(content, context);
    return ESP_OK;
}

const entity_handler_t siren_entity_handler = {
    .domain = "siren",
    .required_fields = HA_ENTITY_FIELD_ALL,
    .refresh_changes = ENTITY_HANDLER_REFRESH_CHANGES,
    .initial_icon = MDI_SYMBOL_BELL,
    .default_action_config = {
        .has_tap_action = true,
        .tap_action = { .action = SCREEN_ACTION_MORE_INFO },
        .has_hold_action = true,
        .hold_action = { .action = SCREEN_ACTION_NOTHING },
        .has_icon_tap_action = true,
        .icon_tap_action = { .action = SCREEN_ACTION_TOGGLE },
    },
    .handle_action = siren_handle_action,
    .render_tile = siren_render_tile,
    .render_more_info = siren_render_more_info,
};