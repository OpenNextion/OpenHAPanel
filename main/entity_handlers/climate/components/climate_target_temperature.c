#include "climate_target_temperature.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ha_ui_font.h"
#include "i18n.h"
#include "screen_theme.h"
#include "assets/ha_ui_assets.h"
#include "assets/mdi_symbol_def.h"

#define TARGET_HEIGHT 56
#define TARGET_GAP 4
#define TARGET_BUTTON_WIDTH 44
#define VALUE_FONT_GROW_MARGIN 6
#define TARGET_BUTTON_TOUCH_WIDTH 48

typedef struct climate_temperature_context climate_temperature_context_t;

typedef struct {
    climate_temperature_context_t *context;
    size_t index;
    int direction;
    bool enabled;
} climate_temperature_button_t;

struct climate_temperature_context {
    lv_obj_t *bars[2];
    lv_obj_t *values[2];
    lv_obj_t *buttons[2][2];
    lv_obj_t *symbols[2][2];
    climate_temperature_button_t bindings[2][2];
    climate_temperature_target_t targets[2];
    int32_t value_widths[2];
    const lv_font_t *value_font;
    char unit[12];
    size_t count;
    bool available;
    bool half_width;
    climate_temperature_change_cb_t callback;
    void *user_data;
};

static climate_temperature_context_t *context_of(lv_obj_t *obj)
{
    return obj ? lv_obj_get_user_data(obj) : NULL;
}

static void root_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        lv_free(lv_event_get_user_data(event));
    }
}

static void button_event(lv_event_t *event)
{
    climate_temperature_button_t *binding = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING ||
        code == LV_EVENT_RELEASED || code == LV_EVENT_CLICKED) {
        lv_event_stop_bubbling(event);
    }
    if (code == LV_EVENT_CLICKED && binding->enabled &&
        binding->context->callback) {
        binding->context->callback(binding->index, binding->direction,
                                   binding->context->user_data);
    }
}

static void update_bar_layout(climate_temperature_context_t *context,
                              size_t index, int32_t width)
{
    lv_obj_t *value = context->values[index];
    int32_t button_width;
    if (context->count > 1) {
        button_width = context->half_width
            ? LV_MIN(24, LV_MAX(14, (width - 28) / 2))
            : LV_MIN(36, LV_MAX(20, (width - 50) / 2));
    } else {
        button_width = context->half_width
            ? LV_MIN(36, LV_MAX(22, (width - 52) / 2))
            : TARGET_BUTTON_WIDTH;
    }
    const int32_t button_ext_click = LV_MAX(0, (TARGET_BUTTON_TOUCH_WIDTH -
                                                button_width) / 2);
    for (size_t j = 0; j < 2; j++) {
        lv_obj_set_width(context->buttons[index][j], button_width);
        lv_obj_set_ext_click_area(context->buttons[index][j], button_ext_click);
        lv_obj_set_style_text_font(context->symbols[index][j],
                                   &ha_font_mdi_20, 0);
    }
    const int32_t value_width = LV_MAX(1, width - 2 * button_width);
    context->value_widths[index] = value_width;
    lv_obj_set_width(value, value_width);
}

static bool value_font_fits(const climate_temperature_context_t *context,
                            const lv_font_t *font, int32_t margin)
{
    for (size_t i = 0; i < context->count; i++) {
        lv_point_t text_size;
        lv_text_get_size(&text_size, lv_label_get_text(context->values[i]),
                         font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (text_size.x > context->value_widths[i] - margin) {
            return false;
        }
    }
    return true;
}

static void update_value_fonts(climate_temperature_context_t *context)
{
    if (context->count == 0) return;
    static const lv_font_t *const fonts[] = {
        &ha_font_misan_medium_20,
        &ha_font_misan_medium_16,
        &ha_font_misan_medium_12,
    };
    const size_t font_count = sizeof(fonts) / sizeof(fonts[0]);

    size_t current = font_count - 1;
    for (size_t i = 0; i < font_count; i++) {
        if (context->value_font == fonts[i]) {
            current = i;
            break;
        }
    }

    while (current < font_count - 1 &&
           !value_font_fits(context, fonts[current], 0)) {
        current++;
    }
    while (current > 0 &&
           value_font_fits(context, fonts[current - 1], VALUE_FONT_GROW_MARGIN)) {
        current--;
    }

    context->value_font = fonts[current];
    for (size_t i = 0; i < context->count; i++) {
        lv_obj_t *value = context->values[i];
        lv_obj_set_style_text_font(value, fonts[current], 0);
        lv_obj_set_height(value, lv_font_get_line_height(fonts[current]));
        lv_obj_align(value, LV_ALIGN_CENTER, 0, 0);
    }
}

static void update_bars_layout(climate_temperature_context_t *context,
                               lv_obj_t *root)
{
    const int32_t width = lv_obj_get_width(root);
    const int32_t bar_width = context->count > 1
        ? LV_MAX(1, (width - TARGET_GAP) / 2) : width;
    for (size_t i = 0; i < 2; i++) {
        lv_obj_set_width(context->bars[i], bar_width);
        lv_obj_set_pos(context->bars[i],
                       i == 0 ? 0 : width - bar_width, 0);
        update_bar_layout(context, i, bar_width);
    }
    update_value_fonts(context);
}

static void root_size_changed(lv_event_t *event)
{
    update_bars_layout(lv_event_get_user_data(event),
                       lv_event_get_target_obj(event));
}

static void bar_size_changed(lv_event_t *event)
{
    climate_temperature_context_t *context = lv_event_get_user_data(event);
    lv_obj_t *bar = lv_event_get_target_obj(event);
    update_bar_layout(context, bar == context->bars[0] ? 0 : 1,
                      lv_obj_get_width(bar));
    update_value_fonts(context);
}

static unsigned decimal_places(double step)
{
    for (unsigned places = 0; places < 3; places++) {
        if (fabs(step - round(step)) < 0.00001) return places;
        step *= 10.0;
    }
    return 3;
}

void climate_target_temperature_apply_theme(lv_obj_t *obj)
{
    climate_temperature_context_t *context = context_of(obj);
    if (!context) return;
    const bool light = screen_theme_get() == SCREEN_THEME_LIGHT;
    const bool available = context->available;
    const lv_color_t color = lv_color_hex(available
        ? (light ? 0x141414 : 0xE1E1E1)
        : (light ? 0xBDBDBD : 0x464646));
    const lv_color_t background = lv_color_hex(light ? 0xE3E3E3 : 0x242424);
    for (size_t i = 0; i < 2; i++) {
        lv_obj_set_style_bg_color(context->bars[i], background, 0);
        lv_obj_set_style_bg_opa(context->bars[i], LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(context->values[i], color, 0);
        for (size_t j = 0; j < 2; j++) {
            lv_obj_set_style_text_color(context->buttons[i][j], color, 0);
            lv_obj_set_style_text_color(context->symbols[i][j], color, 0);
            lv_obj_set_style_text_opa(context->symbols[i][j],
                                      context->bindings[i][j].enabled
                                          ? LV_OPA_COVER : LV_OPA_40, 0);
        }
    }
}

lv_obj_t *climate_target_temperature_create(lv_obj_t *parent)
{
    if (!parent) return NULL;
    climate_temperature_context_t *context = lv_calloc(1, sizeof(*context));
    if (!context) return NULL;
    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        lv_free(context);
        return NULL;
    }
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(root, LV_PCT(100), TARGET_HEIGHT);
    lv_obj_set_user_data(root, context);
    lv_obj_add_event_cb(root, root_event, LV_EVENT_DELETE, context);

    for (size_t i = 0; i < 2; i++) {
        lv_obj_t *bar = lv_obj_create(root);
        context->bars[i] = bar;
        lv_obj_remove_style_all(bar);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(bar, LV_PCT(100), TARGET_HEIGHT);
        lv_obj_set_pos(bar, 0, 0);
        lv_obj_set_style_radius(bar, 16, 0);

        lv_obj_t *value = lv_label_create(bar);
        context->values[i] = value;
        lv_obj_set_width(value, LV_PCT(100));
        lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(value, &ha_font_misan_medium_20, 0);
        lv_obj_align(value, LV_ALIGN_CENTER, 0, 0);

        for (size_t j = 0; j < 2; j++) {
            lv_obj_t *button = lv_obj_create(bar);
            context->buttons[i][j] = button;
            lv_obj_remove_style_all(button);
            lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
            lv_obj_set_size(button, TARGET_BUTTON_WIDTH, TARGET_HEIGHT);
            lv_obj_align(button, j == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID,
                         0, 0);
            lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_align(button, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_t *symbol = lv_label_create(button);
            context->symbols[i][j] = symbol;
            lv_label_set_text(symbol, j == 0 ? MDI_SYMBOL_MINUS : MDI_SYMBOL_PLUS);
            lv_obj_center(symbol);
            lv_obj_set_style_text_font(symbol, &ha_font_mdi_20, 0);
            context->bindings[i][j] = (climate_temperature_button_t){
                .context = context, .index = i, .direction = j == 0 ? -1 : 1
            };
            lv_obj_add_event_cb(button, button_event, LV_EVENT_ALL,
                                &context->bindings[i][j]);
        }
        lv_obj_add_event_cb(bar, bar_size_changed, LV_EVENT_SIZE_CHANGED, context);
    }
    context->value_font = &ha_font_misan_medium_20;
    lv_obj_add_event_cb(root, root_size_changed, LV_EVENT_SIZE_CHANGED, context);
    climate_target_temperature_set(root, NULL, 0, "°C", false, false);
    return root;
}

void climate_target_temperature_set(lv_obj_t *obj,
                                    const climate_temperature_target_t *targets,
                                    size_t count, const char *unit,
                                    bool available, bool half_width)
{
    climate_temperature_context_t *context = context_of(obj);
    if (!context) return;
    context->count = targets ? LV_MIN(count, 2) : 0;
    context->available = available && context->count > 0;
    context->half_width = half_width;
    lv_strlcpy(context->unit, unit && unit[0] ? unit : "°C",
               sizeof(context->unit));
    lv_obj_set_height(obj, TARGET_HEIGHT);
    if (lv_obj_get_parent(obj)) {
        lv_obj_set_height(lv_obj_get_parent(obj), TARGET_HEIGHT);
    }
    update_bars_layout(context, obj);
    for (size_t i = 0; i < 2; i++) {
        if (i >= context->count) {
            lv_obj_add_flag(context->bars[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(context->bars[i], LV_OBJ_FLAG_HIDDEN);
        context->targets[i] = targets[i];
        char number[24];
        const i18n_number_options_t options = {
            true, I18N_NUMBER_FRACTION_FIXED,
            (uint8_t)decimal_places(targets[i].step)
        };
        if (!i18n_format_number_double(number, sizeof(number),
                                       targets[i].value, &options)) {
            snprintf(number, sizeof(number), "%.*f",
                     (int)options.fraction_digits, targets[i].value);
        }
        char text[40];
        if (context->count > 1 && context->half_width) {
            lv_strlcpy(text, number, sizeof(text));
        } else {
            const i18n_arg_t args[] = {
                {"value", number},
                {"unit", context->unit},
            };
            if (!i18n_format(text, sizeof(text), "{value} {unit}", args, 2)) {
                snprintf(text, sizeof(text), "%s %s", number, context->unit);
            }
        }
        lv_label_set_text(context->values[i],
                          context->available ? text : context->unit);
        for (size_t j = 0; j < 2; j++) {
            climate_temperature_button_t *binding = &context->bindings[i][j];
            binding->enabled = context->available &&
                (j == 0 ? targets[i].value > targets[i].min + 0.00001
                        : targets[i].value < targets[i].max - 0.00001);
        }
    }
    update_value_fonts(context);
    climate_target_temperature_apply_theme(obj);
}

void climate_target_temperature_set_change_cb(lv_obj_t *obj,
                                              climate_temperature_change_cb_t callback,
                                              void *user_data)
{
    climate_temperature_context_t *context = context_of(obj);
    if (!context) return;
    context->callback = callback;
    context->user_data = user_data;
}

size_t climate_target_temperature_get(lv_obj_t *obj,
                                      climate_temperature_target_t targets[2])
{
    climate_temperature_context_t *context = context_of(obj);
    if (!context || !targets) return 0;
    memcpy(targets, context->targets, context->count * sizeof(*targets));
    return context->count;
}
