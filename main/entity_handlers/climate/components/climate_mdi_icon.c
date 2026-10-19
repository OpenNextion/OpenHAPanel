/**
 * @file climate_mdi_icon.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_mdi_icon_private_gen.h"

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

void climate_mdi_icon_constructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void climate_mdi_icon_destructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void climate_mdi_icon_event_hook(lv_event_t * e)
{
    LV_UNUSED(e);
}

void climate_mdi_icon_set_size(lv_obj_t * climate_mdi_icon, int32_t size)
{
    climate_mdi_icon_t * root = (climate_mdi_icon_t *) climate_mdi_icon;
    if (root == NULL) return;
    root->size = size;
}

void climate_mdi_icon_set_text_font(lv_obj_t * climate_mdi_icon, const lv_font_t * font)
{
    if (climate_mdi_icon == NULL) return;
    lv_obj_set_style_text_font(climate_mdi_icon, font, 0);
}

void climate_mdi_icon_set_text_color(lv_obj_t * climate_mdi_icon, lv_color_t text_color)
{
    if (climate_mdi_icon == NULL) return;
    lv_obj_set_style_text_color(climate_mdi_icon, text_color, 0);
}

void climate_mdi_icon_set_symbol(lv_obj_t * climate_mdi_icon, const char * symbol)
{
    climate_mdi_icon_t * root = (climate_mdi_icon_t *) climate_mdi_icon;
    if (root == NULL) return;
    root->symbol = symbol;
    lv_label_set_text((lv_obj_t *)&root->obj, symbol ? symbol : "");
}
