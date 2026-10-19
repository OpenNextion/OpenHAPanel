/**
 * @file climate_big_number.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_big_number_gen.h"
#include "climate_big_number_private_gen.h"
#include <lvgl.h>
#include "src/core/lv_obj_style.h"
#include "src/core/lv_obj_style_gen.h"
#include "src/layouts/flex/lv_flex.h"
#include <string.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
static void climate_big_number_refresh(lv_obj_t * obj);

void climate_big_number_constructor_hook(lv_obj_t * obj)
{
    climate_big_number_refresh(obj);
}

void climate_big_number_destructor_hook(lv_obj_t * obj)
{

}

void climate_big_number_event_hook(lv_event_t * e)
{

}

void climate_big_number_set_value(lv_obj_t * obj, int32_t value)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    widget->value = value;
    climate_big_number_refresh(obj);
}

void climate_big_number_set_decimal_places(lv_obj_t * obj, int32_t decimal_places)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    widget->decimal_places = decimal_places;
    climate_big_number_refresh(obj);
}

void climate_big_number_set_unit(lv_obj_t * obj, const char * unit)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    lv_label_set_text(widget->unit_label, unit ? unit : "");
}

void climate_big_number_set_unit_position(lv_obj_t * obj, climate_big_number_unit_position_t unit_position)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    widget->unit_position = unit_position;
    climate_big_number_refresh(obj);
}

void climate_big_number_set_text_color(lv_obj_t * obj, lv_color_t text_color)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    widget->text_color = text_color;
    climate_big_number_refresh(obj);
}

void climate_big_number_set_integer_label_text_font(lv_obj_t * obj, const lv_font_t * font)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    if(font != NULL) {
        lv_obj_set_style_text_font(widget->integer_label, font, LV_PART_MAIN);
        climate_big_number_refresh(obj);
    }
}

void climate_big_number_set_decimal_label_text_font(lv_obj_t * obj, const lv_font_t * font)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    if(font != NULL) {
        lv_obj_set_style_text_font(widget->decimal_label, font, LV_PART_MAIN);
        climate_big_number_refresh(obj);
    }
}

void climate_big_number_set_unit_label_text_font(lv_obj_t * obj, const lv_font_t * font)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;
    if(font != NULL) {
        lv_obj_set_style_text_font(widget->unit_label, font, LV_PART_MAIN);
        climate_big_number_refresh(obj);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static int32_t pow10_i32(uint8_t n)
{
    int32_t result = 1;
    while(n-- > 0) result *= 10;
    return result;
}

static void format_parts(const climate_big_number_t * st, char * integer, size_t integer_len,
                         char * decimal, size_t decimal_len)
{
    int32_t scale = pow10_i32(st->decimal_places);
    int32_t whole = st->value / scale;
    int32_t frac = st->value % scale;

    if(st->value < 0 && whole == 0) {
        lv_snprintf(integer, integer_len, "-0");
    } else {
        lv_snprintf(integer, integer_len, "%ld", whole);
    }

    if(st->decimal_places == 0) {
        decimal[0] = '\0';
        return;
    }

    if(frac < 0) frac = -frac;

    char frac_buf[16];
    lv_snprintf(frac_buf, sizeof(frac_buf), "%ld", frac);

    size_t frac_len = strlen(frac_buf);
    size_t pad = st->decimal_places > frac_len ? st->decimal_places - frac_len : 0;

    if(pad >= decimal_len) pad = decimal_len - 1;
    decimal[0] = '.';
    for(size_t i = 0; i < pad && i + 1 < decimal_len; i++) {
        decimal[i + 1] = '0';
    }
    decimal[pad + 1] = '\0';
    lv_strncat(decimal, frac_buf, decimal_len - strlen(decimal) - 1);
}

static void climate_big_number_refresh(lv_obj_t * obj)
{
    climate_big_number_t * widget = (climate_big_number_t *)obj;

    char integer_str[24];
    char decimal_str[16];
    format_parts(widget, integer_str, sizeof(integer_str), decimal_str, sizeof(decimal_str));

    lv_label_set_text(widget->integer_label, integer_str);
    lv_label_set_text(widget->decimal_label, decimal_str);

    /* 动态计算字体文本基线差 (Font Baseline Offset) */
    const lv_font_t * font_int = lv_obj_get_style_text_font(widget->integer_label, 0);
    const lv_font_t * font_dec = lv_obj_get_style_text_font(widget->decimal_label, 0);
    const lv_font_t * font_unit = lv_obj_get_style_text_font(widget->unit_label, 0);

    int32_t baseline_offset = 0;
    if (font_int) {
        /*
         * LVGL font->base_line 官方定义：Base line measured from the bottom of the line_height.
         * 即 base_line 本身就是文本基线距离 Label 容器底部的像素高度 (Descent)。
         * LVGL Flex 采用 LV_FLEX_ALIGN_END 框底对齐时，补偿 Base Line 差值即可完美对齐文本基线。
         */
        int32_t target_base_line = 0;
        if (widget->unit_position == UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_BOTTOM) {
            target_base_line = (widget->decimal_places > 0 && font_dec) ? font_dec->base_line : (font_unit ? font_unit->base_line : 0);
        } else {
            target_base_line = font_dec ? font_dec->base_line : 0;
        }
        
        baseline_offset = font_int->base_line - target_base_line;
        if (baseline_offset < 0) baseline_offset = 0;
    }

    if (widget->unit_position == UNIT_POSITION_DEF_CLIMATE_BIG_NUMBER_UNIT_BOTTOM) {
        lv_obj_set_flex_flow(widget->addon, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(widget->addon, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_gap(widget->addon, 2, 0);
        lv_obj_set_style_pad_bottom(widget->addon, baseline_offset, 0);
        lv_obj_set_style_min_height(widget->decimal_label, 0, 0);
    } else {
        lv_obj_set_flex_flow(widget->addon, LV_FLEX_FLOW_COLUMN_REVERSE);
        lv_obj_set_flex_align(widget->addon, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_gap(widget->addon, 0, 0);
        lv_obj_set_style_pad_bottom(widget->addon, baseline_offset, 0);

        /* 对标 TS 源码 .decimal { min-height: 1.33em }：无小数时保留行高占位，防止右上角单位塌陷 */
        if (widget->decimal_places == 0 && font_dec) {
            lv_obj_set_style_min_height(widget->decimal_label, font_dec->line_height, 0);
        } else {
            lv_obj_set_style_min_height(widget->decimal_label, 0, 0);
        }
    }

    lv_obj_set_style_text_color(widget->integer_label, widget->text_color, 0);
    lv_obj_set_style_text_color(widget->decimal_label, widget->text_color, 0);
    lv_obj_set_style_text_color(widget->unit_label, widget->text_color, 0);
}
