#include "ap_provisioning_view.h"

#include "../../assets/ha_ui_assets.h"
#include "network.h"

#define AP_VIEW_WIDTH       320
#define AP_VIEW_HEIGHT      480
#define AP_VIEW_CARD_WIDTH  248
#define AP_VIEW_CARD_HEIGHT 84

static void ap_view_apply_clear_style(lv_obj_t *obj);

lv_obj_t *ap_provisioning_view_create(lv_obj_t *parent,
                                      const ap_provisioning_view_config_t *config)
{
    if (!parent) {
        return NULL;
    }

    const char *ssid = config && config->ssid ? config->ssid : NETWORK_DEFAULT_AP_SSID_PREFIX;
    const char *setup_url = config && config->setup_url ? config->setup_url : NETWORK_DEFAULT_AP_IP;
    const char *credential = config && config->credential_text
                                 ? config->credential_text
                                 : "No password";

    lv_obj_t *root = lv_obj_create(parent);
    ap_view_apply_clear_style(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, AP_VIEW_WIDTH, AP_VIEW_HEIGHT);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x010101), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    lv_obj_t *wifi_icon = lv_image_create(root);
    lv_image_set_src(wifi_icon, &wifi_60x60);
    lv_obj_set_pos(wifi_icon, 130, 130);
    lv_obj_remove_flag(wifi_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *title = lv_label_create(root);
    lv_obj_set_size(title, 280, 34);
    lv_obj_set_pos(title, 20, 210);
    lv_label_set_text(title, "Connect to Hotspot");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &ha_font_misan_medium_20, 0);

    lv_obj_t *instructions = lv_label_create(root);
    lv_obj_set_size(instructions, 280, 54);
    lv_obj_set_pos(instructions, 20, 247);
    lv_label_set_long_mode(instructions, LV_LABEL_LONG_WRAP);
    lv_label_set_text_fmt(instructions,
                          "Join the Wi-Fi hotspot, then open %s",
                          setup_url);
    lv_obj_set_style_text_align(instructions, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(instructions, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(instructions, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_line_space(instructions, 2, 0);

    lv_obj_t *card = lv_obj_create(root);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(card, AP_VIEW_CARD_WIDTH, AP_VIEW_CARD_HEIGHT);
    lv_obj_set_pos(card, 36, 310);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x202328), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);

    lv_obj_t *ssid_row = lv_obj_create(card);
    ap_view_apply_clear_style(ssid_row);
    lv_obj_remove_flag(ssid_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ssid_row, 216, 24);
    lv_obj_set_pos(ssid_row, 16, 12);
    lv_obj_set_layout(ssid_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ssid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ssid_row,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ssid_row, 8, 0);

    lv_obj_t *ssid_icon = lv_image_create(ssid_row);
    lv_image_set_src(ssid_icon, &wifi_24x24);
    lv_obj_remove_flag(ssid_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *ssid_label = lv_label_create(ssid_row);
    lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(ssid_label, 176, 0);
    lv_label_set_text(ssid_label, ssid);
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ssid_label, &ha_font_misan_regular_16, 0);

    lv_obj_t *credential_row = lv_obj_create(card);
    ap_view_apply_clear_style(credential_row);
    lv_obj_remove_flag(credential_row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(credential_row, 216, 24);
    lv_obj_set_pos(credential_row, 16, 48);
    lv_obj_set_layout(credential_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(credential_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(credential_row,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(credential_row, 8, 0);

    lv_obj_t *unlock_icon = lv_image_create(credential_row);
    lv_image_set_src(unlock_icon, &unlock_24x24);
    lv_obj_remove_flag(unlock_icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *credential_label = lv_label_create(credential_row);
    lv_label_set_long_mode(credential_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(credential_label, 176, 0);
    lv_label_set_text(credential_label, credential);
    lv_obj_set_style_text_color(credential_label, lv_color_hex(0x707070), 0);
    lv_obj_set_style_text_font(credential_label, &ha_font_misan_regular_16, 0);

    return root;
}

static void ap_view_apply_clear_style(lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
}
