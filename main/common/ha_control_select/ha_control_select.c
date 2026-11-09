/**
 * @file ha_control_select.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "ha_control_select_private_gen.h"

#include <string.h>
#include <stdlib.h>

/*********************
 *      DEFINES
 *********************/

#define DEFAULT_HEIGHT       44
#define DEFAULT_RADIUS       12
#define DEFAULT_PAD          0
#define BUTTON_RADIUS        8

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void rebuild_map(ha_control_select_t * st);
static void update_styles(ha_control_select_t * st);
static void clear_options(ha_control_select_t * st);
static void buttonmatrix_value_changed_cb(lv_event_t * e);
static lv_color_t parse_color_str(const char * str, bool * ok);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ha_control_select_constructor_hook(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st != NULL) {
        st->options_data = NULL;
        st->map = NULL;
        st->option_count = 0;
        st->selected_value[0] = '\0';
        st->color = lv_color_hex(0xFF6F22);
        st->text_color = lv_color_hex(0x141414);
        st->height = DEFAULT_HEIGHT;
        st->disabled = false;
        st->vertical = false;
        st->hide_option_label = false;

        /* Set base container styles (LV_PART_MAIN) */
        lv_obj_set_style_radius(obj, DEFAULT_RADIUS, LV_PART_MAIN);
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x727272), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, (255 * 18 / 100), LV_PART_MAIN);
        lv_obj_set_style_pad_all(obj, DEFAULT_PAD, LV_PART_MAIN);
        lv_obj_set_style_pad_column(obj, DEFAULT_PAD, LV_PART_MAIN);
        lv_obj_set_style_pad_row(obj, DEFAULT_PAD, LV_PART_MAIN);
        lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);

        /* Set unselected item styles (LV_PART_ITEMS) */
        lv_obj_set_style_radius(obj, BUTTON_RADIUS, LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(obj, LV_OPA_0, LV_PART_ITEMS);
        lv_obj_set_style_border_width(obj, 0, LV_PART_ITEMS);
        lv_obj_set_style_text_color(obj, st->text_color, LV_PART_ITEMS);

        /* Set selected item styles (LV_PART_ITEMS | LV_STATE_CHECKED) */
        lv_obj_set_style_radius(obj, BUTTON_RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(obj, st->color, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(obj, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(obj, 0, LV_PART_ITEMS | LV_STATE_CHECKED);

        /* Set pressed item styles */
        lv_obj_set_style_radius(obj, BUTTON_RADIUS, LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(obj, st->color, LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(obj, (255 * 25 / 100), LV_PART_ITEMS | LV_STATE_PRESSED);

        lv_buttonmatrix_set_one_checked(obj, true);
        lv_obj_add_event_cb(obj, buttonmatrix_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

void ha_control_select_destructor_hook(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st != NULL) {
        clear_options(st);
    }
}

void ha_control_select_event_hook(lv_event_t * e)
{
    LV_UNUSED(e);
}

void ha_control_select_set_options_array(lv_obj_t * obj, const ha_control_select_option_t * options, uint32_t count)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;

    static const char * empty_map[] = {" ", ""};
    lv_buttonmatrix_set_map(obj, empty_map);
    clear_options(st);
    if(options == NULL || count == 0) return;

    st->options_data = lv_malloc(sizeof(ha_control_select_option_store_t) * count);
    LV_ASSERT_MALLOC(st->options_data);
    if(st->options_data == NULL) return;

    st->option_count = count;

    for(uint32_t i = 0; i < count; i++) {
        lv_strlcpy(st->options_data[i].value, options[i].value ? options[i].value : "", sizeof(st->options_data[i].value));
        lv_strlcpy(st->options_data[i].label, options[i].label ? options[i].label : st->options_data[i].value, sizeof(st->options_data[i].label));
        lv_strlcpy(st->options_data[i].symbol, options[i].symbol ? options[i].symbol : "", sizeof(st->options_data[i].symbol));
        st->options_data[i].color = options[i].color;
        st->options_data[i].has_color = options[i].has_color;
    }

    if(st->selected_value[0] == '\0' && count > 0) {
        lv_strlcpy(st->selected_value, st->options_data[0].value, sizeof(st->selected_value));
        st->value = st->selected_value;
    }

    rebuild_map(st);
}

void ha_control_select_set_options(lv_obj_t * obj, const char * options_str)
{
    if(obj == NULL || options_str == NULL || options_str[0] == '\0') return;

    /* Count items delimited by ';' */
    uint32_t count = 1;
    for(const char * p = options_str; *p; p++) {
        if(*p == ';') count++;
    }

    ha_control_select_option_t * opts = lv_malloc(sizeof(ha_control_select_option_t) * count);
    if(opts == NULL) return;

    char * buf = lv_strdup(options_str);
    if(buf == NULL) {
        lv_free(opts);
        return;
    }

    char * item = buf;
    uint32_t idx = 0;
    while(item != NULL && idx < count) {
        char * next_item = strchr(item, ';');
        if(next_item != NULL) {
            *next_item = '\0';
            next_item++;
        }

        /* Parse item fields: "value,label,symbol,color" or "value,label,symbol" or "value,label" or "value" */
        char * v = item;
        char * l = strchr(v, ',');
        char * s = NULL;
        char * c = NULL;
        if(l != NULL) {
            *l = '\0';
            l++;
            s = strchr(l, ',');
            if(s != NULL) {
                *s = '\0';
                s++;
                c = strchr(s, ',');
                if(c != NULL) {
                    *c = '\0';
                    c++;
                }
            }
        }

        opts[idx].value = v;
        opts[idx].label = l ? l : v;
        opts[idx].symbol = s ? s : "";
        opts[idx].has_color = false;
        opts[idx].color = lv_color_hex(0);
        if(c != NULL && c[0] != '\0') {
            bool ok = false;
            opts[idx].color = parse_color_str(c, &ok);
            opts[idx].has_color = ok;
        }
        idx++;

        item = next_item;
    }

    ha_control_select_set_options_array(obj, opts, idx);

    lv_free(buf);
    lv_free(opts);
}

void ha_control_select_set_value(lv_obj_t * obj, const char * value)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    lv_strlcpy(st->selected_value, value ? value : "", sizeof(st->selected_value));
    st->value = st->selected_value;

    if(st->options_data != NULL) {
        for(uint32_t i = 0; i < st->option_count; i++) {
            if(strcmp(st->options_data[i].value, st->selected_value) == 0) {
                lv_buttonmatrix_set_button_ctrl(obj, i, LV_BUTTONMATRIX_CTRL_CHECKED);
            } else {
                lv_buttonmatrix_clear_button_ctrl(obj, i, LV_BUTTONMATRIX_CTRL_CHECKED);
            }
        }
    }
    update_styles(st);
}

void ha_control_select_set_label(lv_obj_t * obj, const char * label)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    st->label = label;
}

void ha_control_select_set_disabled(lv_obj_t * obj, bool disabled)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    st->disabled = disabled;
    update_styles(st);
}

void ha_control_select_set_vertical(lv_obj_t * obj, bool vertical)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    st->vertical = vertical;
    rebuild_map(st);
}

void ha_control_select_set_hide_option_label(lv_obj_t * obj, bool hide_option_label)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    st->hide_option_label = hide_option_label;
    rebuild_map(st);
}

void ha_control_select_set_color(lv_obj_t * obj, lv_color_t color)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    st->color = color;
    update_styles(st);
}

void ha_control_select_set_text_color(lv_obj_t * obj, lv_color_t color)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    st->text_color = color;
    lv_obj_set_style_text_color(obj, color, LV_PART_ITEMS);
}

void ha_control_select_set_surface_color(lv_obj_t * obj, lv_color_t color)
{
    ha_control_select_set_surface_style(obj, color, LV_OPA_COVER);
}

void ha_control_select_set_surface_style(lv_obj_t * obj, lv_color_t color, lv_opa_t opa)
{
    if(obj == NULL) return;
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, opa, LV_PART_MAIN);
}

void ha_control_select_set_height(lv_obj_t * obj, lv_coord_t height)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL) return;
    st->height = height;
    lv_obj_set_height(obj, height > 0 ? height : DEFAULT_HEIGHT);
}

void ha_control_select_set_text_font(lv_obj_t * ha_control_select, const lv_font_t * font)
{
    if(ha_control_select != NULL && font != NULL) {
        lv_obj_set_style_text_font(ha_control_select, font, LV_PART_ITEMS);
    }
}

void ha_control_select_set_symbol_font(lv_obj_t * ha_control_select, const lv_font_t * font)
{
    if(ha_control_select != NULL && font != NULL) {
        lv_obj_set_style_text_font(ha_control_select, font, LV_PART_ITEMS);
    }
}

const char * ha_control_select_get_value(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    return st ? st->selected_value : "";
}

uint32_t ha_control_select_get_option_count(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    return st ? st->option_count : 0;
}

const char * ha_control_select_get_label(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    return st ? st->label : "";
}

bool ha_control_select_get_disabled(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    return st ? st->disabled : false;
}

bool ha_control_select_get_vertical(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    return st ? st->vertical : false;
}

bool ha_control_select_get_hide_option_label(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    return st ? st->hide_option_label : false;
}

lv_color_t ha_control_select_get_color(lv_obj_t * obj)
{
    ha_control_select_t * st = (ha_control_select_t *)obj;
    return st ? st->color : lv_color_hex(0xFF6F22);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void rebuild_map(ha_control_select_t * st)
{
    if(st == NULL || st->options_data == NULL || st->option_count == 0) return;
    lv_obj_t * obj = (lv_obj_t *)st;

    if(st->map != NULL) {
        lv_free(st->map);
        st->map = NULL;
    }

    uint32_t count = st->option_count;
    uint32_t map_slots = st->vertical ? (count * 2 + 1) : (count + 1);
    st->map = lv_malloc(sizeof(char *) * map_slots);
    LV_ASSERT_MALLOC(st->map);
    if(st->map == NULL) return;

    uint32_t m = 0;
    for(uint32_t i = 0; i < count; i++) {
        if(st->hide_option_label) {
            /* Pure symbol mode (or label if symbol is absent) */
            st->map[m++] = (st->options_data[i].symbol[0] != '\0') ? st->options_data[i].symbol : st->options_data[i].label;
        } else if(st->options_data[i].symbol[0] != '\0' && st->options_data[i].label[0] != '\0') {
            /* Two lines: Symbol on top, Label on bottom */
            lv_snprintf(st->options_data[i].display_text, sizeof(st->options_data[i].display_text),
                        "%s\n%s", st->options_data[i].symbol, st->options_data[i].label);
            st->map[m++] = st->options_data[i].display_text;
        } else if(st->options_data[i].symbol[0] != '\0') {
            st->map[m++] = st->options_data[i].symbol;
        } else {
            st->map[m++] = st->options_data[i].label;
        }

        if(st->vertical && i < count - 1) {
            st->map[m++] = "\n";
        }
    }
    st->map[m] = "";

    lv_buttonmatrix_set_map(obj, st->map);
    lv_buttonmatrix_set_button_ctrl_all(obj, LV_BUTTONMATRIX_CTRL_CHECKABLE | LV_BUTTONMATRIX_CTRL_NO_REPEAT | LV_BUTTONMATRIX_CTRL_CLICK_TRIG);

    /* Highlight current active value */
    for(uint32_t i = 0; i < count; i++) {
        if(strcmp(st->options_data[i].value, st->selected_value) == 0) {
            lv_buttonmatrix_set_button_ctrl(obj, i, LV_BUTTONMATRIX_CTRL_CHECKED);
        } else {
            lv_buttonmatrix_clear_button_ctrl(obj, i, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
    }

    update_styles(st);
}

static void update_styles(ha_control_select_t * st)
{
    if(st == NULL) return;
    lv_obj_t * obj = (lv_obj_t *)st;

    lv_color_t active_color = st->color;
    if(st->options_data != NULL) {
        for(uint32_t i = 0; i < st->option_count; i++) {
            if(strcmp(st->options_data[i].value, st->selected_value) == 0) {
                if(st->options_data[i].has_color) {
                    active_color = st->options_data[i].color;
                }
                break;
            }
        }
    }

    /* Update selected item style */
    lv_obj_set_style_bg_color(obj, active_color, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(obj, active_color, LV_PART_ITEMS | LV_STATE_PRESSED);

    if(st->disabled) {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
        /* Keep the control as the touch target so disabled features cannot tap the Tile. */
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(obj, LV_OPA_40, LV_PART_MAIN);
    } else {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    }
}

static void clear_options(ha_control_select_t * st)
{
    if(st == NULL) return;
    if(st->map != NULL) {
        lv_free(st->map);
        st->map = NULL;
    }
    if(st->options_data != NULL) {
        lv_free(st->options_data);
        st->options_data = NULL;
    }
    st->option_count = 0;
}

static void buttonmatrix_value_changed_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    ha_control_select_t * st = (ha_control_select_t *)obj;
    if(st == NULL || st->disabled || st->options_data == NULL) return;

    uint32_t btn_id = lv_buttonmatrix_get_selected_button(obj);
    if(btn_id >= st->option_count) return;

    if(strcmp(st->selected_value, st->options_data[btn_id].value) == 0) return;

    lv_strlcpy(st->selected_value, st->options_data[btn_id].value, sizeof(st->selected_value));
    st->value = st->selected_value;

    update_styles(st);

}

static lv_color_t parse_color_str(const char * str, bool * ok)
{
    if(str == NULL || str[0] == '\0') {
        if(ok) *ok = false;
        return lv_color_hex(0);
    }
    if(str[0] == '#') str++;
    char * end = NULL;
    unsigned long value = strtoul(str, &end, 16);
    if(end == str || *end != '\0' || value > 0xFFFFFFUL) {
        if(ok) *ok = false;
        return lv_color_hex(0);
    }
    if(ok) *ok = true;
    return lv_color_hex((uint32_t)value);
}
