#include "climate_dial.h"
#include "assets/ha_ui_assets.h"
#include "climate_big_number.h"
#include "climate_control_circular_slider.h"
#include <math.h>
#include <string.h>

typedef struct {
    lv_obj_t *status_row, *slider, *number_area;
    lv_obj_t *current_temp, *current_humidity, *action, *low_number, *high_number;
    int low, high;
    bool dual, humidity, selected_low, disabled, theme_off;
    climate_dial_change_cb_t callback;
    void *user_data;
    climate_dial_selection_cb_t selection_callback;
    void *selection_user_data;
} climate_dial_t;

static climate_dial_t *ctx(lv_obj_t *obj) { return obj ? lv_obj_get_user_data(obj) : NULL; }

static void set_number(lv_obj_t *obj, int value, bool humidity)
{
    climate_big_number_set_decimal_places(obj, humidity ? 0 : 1);
    climate_big_number_set_value(obj, humidity ? (int)lround(value / 10.0) : value);
}

static void refresh(climate_dial_t *dial)
{
    set_number(dial->low_number, dial->low, dial->humidity);
    if (dial->dual) {
        set_number(dial->high_number, dial->high, dial->humidity);
        lv_obj_remove_flag(dial->high_number, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(dial->low_number);
        lv_obj_update_layout(dial->high_number);
        int low_width = lv_obj_get_width(dial->low_number);
        int high_width = lv_obj_get_width(dial->high_number);
        lv_obj_align(dial->low_number, LV_ALIGN_CENTER, -(high_width + 16) / 2, 0);
        lv_obj_align(dial->high_number, LV_ALIGN_CENTER, (low_width + 16) / 2, 0);
        climate_big_number_set_text_color(dial->low_number, lv_color_hex(dial->selected_low ? 0xE1E1E1 : 0x9B9B9B));
        climate_big_number_set_text_color(dial->high_number, lv_color_hex(dial->selected_low ? 0x9B9B9B : 0xE1E1E1));
    } else {
        lv_obj_add_flag(dial->high_number, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(dial->low_number, LV_ALIGN_CENTER, 0, 0);
        climate_big_number_set_text_color(dial->low_number, lv_color_hex(0xE1E1E1));
    }
}

static void number_clicked(lv_event_t *event)
{
    climate_dial_t *dial = lv_event_get_user_data(event);
    if (!dial || !dial->dual) return;
    dial->selected_low = lv_event_get_target_obj(event) == dial->low_number;
    refresh(dial);
    if (dial->selection_callback)
        dial->selection_callback(!dial->selected_low, dial->selection_user_data);
}

static void slider_event(lv_event_t *event)
{
    climate_dial_t *dial = lv_event_get_user_data(event);
    const climate_control_circular_slider_value_changed_event_t *change = lv_event_get_param(event);
    if (!dial || !change || dial->disabled) return;
    bool selection_changed = dial->dual && dial->selected_low != change->is_low;
    dial->selected_low = change->is_low;
    if (!dial->dual || change->is_low) dial->low = change->value;
    else dial->high = change->value;
    refresh(dial);
    if (selection_changed && dial->selection_callback)
        dial->selection_callback(!dial->selected_low, dial->selection_user_data);
    if (lv_event_get_code(event) == LV_EVENT_RELEASED && dial->callback)
        dial->callback(change->value, change->is_low, dial->user_data);
}

static void deleted(lv_event_t *event)
{
    lv_free(lv_obj_get_user_data(lv_event_get_target_obj(event)));
}

static lv_obj_t *make_number(lv_obj_t *parent)
{
    lv_obj_t *number = climate_big_number_create(parent);
    if (!number) return NULL;
    climate_big_number_set_unit_position(number, UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_TOP);
    climate_big_number_set_integer_label_text_font(number, &ha_font_misan_medium_36);
    climate_big_number_set_decimal_label_text_font(number, &ha_font_misan_medium_16);
    climate_big_number_set_unit_label_text_font(number, &ha_font_misan_medium_12);
    climate_big_number_set_unit(number, "°C");
    lv_obj_add_flag(number, LV_OBJ_FLAG_CLICKABLE);
    return number;
}

lv_obj_t *climate_dial_create(lv_obj_t *parent)
{
    if (!parent) return NULL;
    climate_dial_t *dial = lv_calloc(1, sizeof(*dial));
    if (!dial) return NULL;
    lv_obj_t *root = lv_obj_create(parent);
    if (!root) { lv_free(dial); return NULL; }
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_PCT(100), 238);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_user_data(root, dial);
    lv_obj_add_event_cb(root, deleted, LV_EVENT_DELETE, NULL);

    dial->status_row = lv_obj_create(root);
    if (!dial->status_row) { lv_obj_delete(root); return NULL; }
    lv_obj_remove_style_all(dial->status_row);
    lv_obj_set_size(dial->status_row, LV_PCT(100), 20);
    lv_obj_remove_flag(dial->status_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(dial->status_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dial->status_row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(dial->status_row, 0, 0);
    lv_obj_set_style_pad_column(dial->status_row, 16, 0);
    dial->current_temp = lv_label_create(dial->status_row);
    dial->current_humidity = lv_label_create(dial->status_row);
    if (!dial->current_temp || !dial->current_humidity) {
        lv_obj_delete(root);
        return NULL;
    }
    lv_obj_t *currents[] = {dial->current_temp, dial->current_humidity};
    for (size_t i = 0; i < 2; ++i) {
        lv_obj_set_width(currents[i], 136);
        lv_obj_set_style_text_align(currents[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(currents[i], &ha_font_misan_regular_12, 0);
        lv_obj_set_style_text_color(currents[i], lv_color_white(), 0);
        lv_label_set_text(currents[i], "");
    }
    lv_obj_add_flag(dial->current_humidity, LV_OBJ_FLAG_HIDDEN);

    dial->slider = climate_control_circular_slider_create(root);
    if (!dial->slider) { lv_obj_delete(root); return NULL; }
    climate_control_circular_slider_set_size(dial->slider, 210);
    lv_obj_add_event_cb(dial->slider, slider_event, LV_EVENT_VALUE_CHANGED, dial);
    lv_obj_add_event_cb(dial->slider, slider_event, LV_EVENT_RELEASED, dial);
    dial->action = lv_label_create(dial->slider);
    if (!dial->action) { lv_obj_delete(root); return NULL; }
    lv_obj_set_style_text_font(dial->action, &ha_font_misan_regular_12, 0);
    lv_obj_set_style_text_color(dial->action, lv_color_white(), 0);
    lv_obj_align(dial->action, LV_ALIGN_CENTER, 0, -39);
    lv_label_set_text(dial->action, "");
    dial->number_area = lv_obj_create(dial->slider);
    if (!dial->number_area) { lv_obj_delete(root); return NULL; }
    lv_obj_remove_style_all(dial->number_area);
    lv_obj_set_size(dial->number_area, 190, 74);
    lv_obj_remove_flag(dial->number_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dial->number_area, LV_ALIGN_CENTER, 0, -6);
    dial->low_number = make_number(dial->number_area);
    dial->high_number = make_number(dial->number_area);
    if (!dial->low_number || !dial->high_number) {
        lv_obj_delete(root);
        return NULL;
    }
    lv_obj_add_event_cb(dial->low_number, number_clicked, LV_EVENT_CLICKED, dial);
    lv_obj_add_event_cb(dial->high_number, number_clicked, LV_EVENT_CLICKED, dial);
    dial->selected_low = true;
    refresh(dial);
    return root;
}

void climate_dial_set_range(lv_obj_t *obj, int min, int max, int step)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial) return;
    climate_control_circular_slider_set_range(dial->slider, min, max);
    climate_control_circular_slider_set_step(dial->slider, step);
}

void climate_dial_set_dual(lv_obj_t *obj, bool dual)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial) return;
    dial->dual = dual;
    if (!dual) dial->selected_low = true;
    climate_control_circular_slider_set_dual(dial->slider, dual);
    refresh(dial);
}

void climate_dial_set_value(lv_obj_t *obj, int value)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial) return;
    dial->low = value;
    if (dial->dual) climate_control_circular_slider_set_low(dial->slider, value);
    else climate_control_circular_slider_set_value(dial->slider, value);
    refresh(dial);
}

void climate_dial_set_high(lv_obj_t *obj, int value)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial) return;
    dial->high = value;
    climate_control_circular_slider_set_high(dial->slider, value);
    refresh(dial);
}

int climate_dial_get_value(lv_obj_t *obj)
{
    climate_dial_t *dial = ctx(obj);
    return dial ? (dial->dual && !dial->selected_low ? dial->high : dial->low) : 0;
}

bool climate_dial_selected_low(lv_obj_t *obj)
{
    climate_dial_t *dial = ctx(obj);
    return dial ? dial->selected_low : true;
}

bool climate_dial_is_dragging(lv_obj_t *obj)
{
    climate_dial_t *dial = ctx(obj);
    return dial && (climate_control_circular_slider_is_updating_low(dial->slider) ||
                    climate_control_circular_slider_is_updating_high(dial->slider));
}

void climate_dial_set_selected_high(lv_obj_t *obj, bool selected_high)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial || !dial->dual) return;
    dial->selected_low = !selected_high;
    refresh(dial);
}

void climate_dial_set_measured(lv_obj_t *obj, int value)
{
    climate_dial_t *dial = ctx(obj);
    if (dial) climate_control_circular_slider_set_current(dial->slider, value);
}

void climate_dial_set_currents(lv_obj_t *obj, const char *temperature,
                               const char *humidity)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial) return;
    const bool has_temp = temperature && temperature[0];
    const bool has_humidity = humidity && humidity[0];
    lv_label_set_text(dial->current_temp, has_temp ? temperature : "");
    lv_label_set_text(dial->current_humidity, has_humidity ? humidity : "");
    if (has_temp) lv_obj_remove_flag(dial->current_temp, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(dial->current_temp, LV_OBJ_FLAG_HIDDEN);
    if (has_humidity) lv_obj_remove_flag(dial->current_humidity, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(dial->current_humidity, LV_OBJ_FLAG_HIDDEN);
}

void climate_dial_set_action(lv_obj_t *obj, const char *text)
{
    climate_dial_t *dial = ctx(obj);
    if (dial) lv_label_set_text(dial->action, text ? text : "");
}

void climate_dial_set_unit(lv_obj_t *obj, const char *unit)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial) return;
    dial->humidity = unit && strcmp(unit, "%") == 0;
    climate_big_number_set_unit(dial->low_number, unit);
    climate_big_number_set_unit(dial->high_number, unit);
    climate_big_number_set_unit_position(dial->low_number, dial->humidity
        ? UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_BOTTOM
        : UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_TOP);
    climate_big_number_set_unit_position(dial->high_number, dial->humidity
        ? UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_BOTTOM
        : UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_TOP);
    refresh(dial);
}

void climate_dial_set_theme(lv_obj_t *obj, const char *mode, bool humidity)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial || !mode) return;
    bool off = !mode[0] || strcmp(mode, "off") == 0 ||
               strcmp(mode, "unknown") == 0 || strcmp(mode, "unavailable") == 0;
    climate_circular_slider_mode_t slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_NONE;
    uint32_t color = 0x6E6E6E;
    if (humidity) {
        if (!off) {
            slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_START;
            color = 0x2196F3;
        }
    } else if (strcmp(mode, "heat") == 0) {
        slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_START; color = 0xFF6F22;
    } else if (strcmp(mode, "cool") == 0) {
        slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_END; color = 0x2196F3;
    } else if (strcmp(mode, "heat_cool") == 0) {
        slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_FULL; color = 0xFFC107;
    } else if (strcmp(mode, "auto") == 0) {
        slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_FULL; color = 0x4CAF50;
    } else if (strcmp(mode, "dry") == 0) {
        slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_FULL; color = 0xFF9800;
    } else if (strcmp(mode, "fan_only") == 0) {
        slider_mode = CLIMATE_CIRCULAR_SLIDER_MODE_FULL; color = 0x00BCD4;
    }
    climate_control_circular_slider_set_mode(dial->slider, slider_mode);
    if (!humidity && dial->dual) {
        climate_control_circular_slider_set_low_color(dial->slider, lv_color_hex(0xFF6F22));
        climate_control_circular_slider_set_high_color(dial->slider, lv_color_hex(0x2196F3));
    } else {
        climate_control_circular_slider_set_active_color(dial->slider, lv_color_hex(color));
    }
    dial->theme_off = off;
    climate_control_circular_slider_set_inactive(dial->slider, off || dial->disabled);
}

void climate_dial_set_disabled(lv_obj_t *obj, bool disabled)
{
    climate_dial_t *dial = ctx(obj);
    if (!dial) return;
    dial->disabled = disabled;
    climate_control_circular_slider_set_inactive(dial->slider, disabled || dial->theme_off);
    climate_control_circular_slider_set_disabled(dial->slider, disabled);
}

void climate_dial_set_change_cb(lv_obj_t *obj, climate_dial_change_cb_t callback, void *user_data)
{
    climate_dial_t *dial = ctx(obj);
    if (dial) { dial->callback = callback; dial->user_data = user_data; }
}

void climate_dial_set_selection_cb(lv_obj_t *obj, climate_dial_selection_cb_t callback, void *user_data)
{
    climate_dial_t *dial = ctx(obj);
    if (dial) { dial->selection_callback = callback; dial->selection_user_data = user_data; }
}
