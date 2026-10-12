#include "ha_history_list.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assets/ha_ui_assets.h"
#include "sys_time_api.h"

static lv_obj_t *history_list_create_root(lv_obj_t *parent,
                                          const char *title)
{
    lv_obj_t *root = lv_obj_create(parent);
    if (!root) {
        return NULL;
    }
    lv_obj_remove_style_all(root);
    lv_obj_set_width(root, HA_HISTORY_LIST_DEFAULT_WIDTH);
    lv_obj_set_height(root, LV_SIZE_CONTENT);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x1E2228), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root, 8, 0);

    lv_obj_t *header = lv_obj_create(root);
    lv_obj_remove_style_all(header);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_top(header, 14, 0);
    lv_obj_set_style_pad_bottom(header, 14, 0);
    lv_obj_set_style_pad_left(header, 12, 0);
    lv_obj_set_style_pad_right(header, 84, 0);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *title_label = lv_label_create(header);
    lv_obj_set_style_text_font(title_label, &ha_font_misan_medium_20, 0);
    lv_label_set_text(title_label, title && title[0] ? title : "History");
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    return root;
}

static lv_obj_t *history_list_create_content(lv_obj_t *root)
{
    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_layout(content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_style_pad_gap(content, 30, 0);
    lv_obj_set_style_pad_hor(content, 12, 0);
    return content;
}

static void history_list_render_message(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
}

static lv_obj_t *history_list_create_day_group(lv_obj_t *parent,
                                                const char *date_text)
{
    lv_obj_t *group = lv_obj_create(parent);
    lv_obj_remove_style_all(group);
    lv_obj_set_layout(group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_size(group, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_gap(group, 16, 0);

    lv_obj_t *date = lv_label_create(group);
    lv_label_set_text(date, date_text);
    lv_obj_set_style_text_font(date, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(date, lv_color_white(), 0);

    lv_obj_t *records = lv_obj_create(group);
    lv_obj_remove_style_all(records);
    lv_obj_set_size(records, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(records, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(records, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(records,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(records, 0, 0);
    lv_obj_set_style_pad_row(records, 12, 0);
    lv_obj_set_style_pad_column(records, 0, 0);
    return records;
}

static void history_list_create_record(lv_obj_t *parent,
                                       const char *text,
                                       const char *time_text)
{
    lv_obj_t *event = lv_label_create(parent);
    lv_label_set_text(event, text ? text : "");
    lv_obj_set_width(event, LV_PCT(60));
    lv_label_set_long_mode(event, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(event, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(event, lv_color_hex(0x666666), 0);

    lv_obj_t *time_label = lv_label_create(parent);
    lv_label_set_text(time_label, time_text);
    lv_obj_set_width(time_label, LV_PCT(40));
    lv_obj_set_style_text_font(time_label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_RIGHT, 0);
}

lv_obj_t *ha_history_list_create(
    lv_obj_t *parent,
    const ha_history_list_config_t *config)
{
    if (!parent || !config) {
        return NULL;
    }

    lv_obj_t *root = history_list_create_root(parent, config->title);
    if (!root) {
        return NULL;
    }
    lv_obj_t *content = history_list_create_content(root);
    if (!content) {
        lv_obj_delete(root);
        return NULL;
    }

    if (config->message && config->message[0]) {
        history_list_render_message(content, config->message);
        return root;
    }
    if (!config->items || config->item_count == 0) {
        history_list_render_message(content, "No history");
        return root;
    }

    static const char *const months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December",
    };
    char today_iso[40] = {0};
    const time_t now = time(NULL);
    if (now > 0) {
        sys_time_format_iso8601_time_by_utc_ms((uint64_t)now * 1000,
                                               today_iso,
                                               sizeof(today_iso));
    }

    char previous_date[11] = {0};
    lv_obj_t *record_box = NULL;
    for (size_t i = 0; i < config->item_count; i++) {
        if (config->items[i].epoch <= 0) {
            continue;
        }

        char local_iso[40] = {0};
        if (sys_time_format_iso8601_time_by_utc_ms(
                (uint64_t)config->items[i].epoch * 1000,
                local_iso,
                sizeof(local_iso)) != 0) {
            continue;
        }

        char date_key[11] = {0};
        snprintf(date_key, sizeof(date_key), "%.10s", local_iso);
        if (strcmp(previous_date, date_key) != 0) {
            unsigned year = 0;
            unsigned month = 0;
            unsigned day = 0;
            if (sscanf(date_key, "%4u-%2u-%2u", &year, &month, &day) != 3 ||
                month < 1 || month > 12) {
                continue;
            }

            char date_text[48] = {0};
            snprintf(date_text,
                     sizeof(date_text),
                     "%.10s%s %u, %u",
                     strncmp(today_iso, date_key, 10) == 0 ? "Today · " : "",
                     months[month - 1],
                     day,
                     year);
            record_box = history_list_create_day_group(content, date_text);
            snprintf(previous_date, sizeof(previous_date), "%s", date_key);
        }

        if (record_box) {
            char time_text[9] = {0};
            snprintf(time_text, sizeof(time_text), "%.8s", local_iso + 11);
            history_list_create_record(record_box,
                                       config->items[i].text,
                                       time_text);
        }
    }

    return root;
}
