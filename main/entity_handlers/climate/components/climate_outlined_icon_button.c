/**
 * @file climate_outlined_icon_button.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "climate_outlined_icon_button_private_gen.h"
#include "climate_ripple.h"
#include "climate_mdi_icon.h"

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

void climate_outlined_icon_button_constructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void climate_outlined_icon_button_destructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void climate_outlined_icon_button_event_hook(lv_event_t * e)
{
    LV_UNUSED(e);
}

void climate_outlined_icon_button_set_size(lv_obj_t * obj, int32_t size)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if (root == NULL) return;
    root->size = size;
    lv_obj_set_size(&root->obj, size, size);
}

void climate_outlined_icon_button_set_icon_size(lv_obj_t * obj, int32_t icon_size)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if (root == NULL) return;
    root->icon_size = icon_size;
    if(root->icon != NULL) {
        climate_mdi_icon_set_size(root->icon, icon_size);
    }
}

void climate_outlined_icon_button_set_text_font(lv_obj_t * obj, const lv_font_t * font)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if(root && root->icon != NULL) {
        climate_mdi_icon_set_text_font(root->icon, font);
    }
}

void climate_outlined_icon_button_set_symbol(lv_obj_t * obj, const char * symbol)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if (root == NULL) return;
    root->symbol = symbol;
    if(root->icon != NULL) {
        climate_mdi_icon_set_symbol(root->icon, symbol);
    }
}

void climate_outlined_icon_button_set_outline_color(lv_obj_t * obj, lv_color_t outline_color)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if (root == NULL) return;
    root->outline_color = outline_color;
    lv_obj_set_style_border_color(&root->obj, outline_color, 0);
    if(root->ripple != NULL) {
        climate_ripple_set_p_color(root->ripple, outline_color);
    }
}

void climate_outlined_icon_button_set_text_color(lv_obj_t * obj, lv_color_t text_color)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if (root == NULL) return;
    root->text_color = text_color;
    if(root->icon != NULL) {
        lv_obj_set_style_text_color(root->icon, text_color, 0);
    }
}

void climate_outlined_icon_button_set_disabled(lv_obj_t * obj, bool disabled)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if (root == NULL) return;
    root->disabled = disabled;
    if(disabled) {
        lv_obj_add_state(&root->obj, LV_STATE_DISABLED);
        lv_obj_clear_flag(&root->obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(&root->obj, LV_OPA_40, 0);
        if(root->ripple != NULL) {
            climate_ripple_set_p_disabled(root->ripple, true);
        }
    } else {
        lv_obj_remove_state(&root->obj, LV_STATE_DISABLED);
        lv_obj_add_flag(&root->obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(&root->obj, LV_OPA_COVER, 0);
        if(root->ripple != NULL) {
            climate_ripple_set_p_disabled(root->ripple, false);
        }
    }
}

void climate_outlined_icon_button_set_no_ripple(lv_obj_t * obj, bool no_ripple)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    if (root == NULL) return;
    root->no_ripple = no_ripple;
    if(root->ripple != NULL) {
        if(no_ripple) {
            lv_obj_add_flag(root->ripple, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(root->ripple, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

int32_t climate_outlined_icon_button_get_size(lv_obj_t * obj)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    return root ? root->size : 48;
}

int32_t climate_outlined_icon_button_get_icon_size(lv_obj_t * obj)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    return root ? root->icon_size : 24;
}

const char * climate_outlined_icon_button_get_symbol(lv_obj_t * obj)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    return root ? root->symbol : "";
}

lv_color_t climate_outlined_icon_button_get_outline_color(lv_obj_t * obj)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    return root ? root->outline_color : lv_color_hex(0x5e5e5e);
}

lv_color_t climate_outlined_icon_button_get_text_color(lv_obj_t * obj)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    return root ? root->text_color : lv_color_hex(0x5e5e5e);
}

bool climate_outlined_icon_button_get_disabled(lv_obj_t * obj)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    return root ? root->disabled : false;
}

bool climate_outlined_icon_button_get_no_ripple(lv_obj_t * obj)
{
    climate_outlined_icon_button_t * root = (climate_outlined_icon_button_t *)obj;
    return root ? root->no_ripple : false;
}
