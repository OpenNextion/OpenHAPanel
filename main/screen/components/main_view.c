#include "main_view.h"

#include <string.h>

#include "app.h"
#include "assets/ha_ui_assets.h"
#include "badge_display.h"
#include "entity_display.h"
#include "entity_handlers/entity_handler_registry.h"
#include "esp_log.h"
#include "ha_entity_store.h"
#include "ha_badge.h"
#include "ha_tile_card.h"
#include "more_info_screen.h"

static const char *TAG = "main_view";

typedef struct {
    size_t config_index;
    const char *entity_id;
    lv_obj_t *name_label;
    lv_obj_t *state_label;
    lv_obj_t *badge_pill;
    const char *badge_name;
    lv_obj_t *tile_card;
    ha_entity_handle_t *entity_handle;
    bool icon_clickable;
    bool body_clickable;
    bool body_tap_enabled;
    bool body_hold_enabled;
    bool has_action_config;
    const screen_tile_card_data_t *tile_config;
    screen_action_config_t action_config;
} main_view_entity_ref_t;

struct main_view {
    lv_obj_t *container;
    main_view_entity_ref_t *badges;
    size_t badge_count;
    main_view_entity_ref_t *entities;
    size_t entity_count;
};

static void main_view_apply_container_style(lv_obj_t *container);
static void main_view_clear_refs(main_view_t *main_view);
static void main_view_render_badges(main_view_t *main_view, const screen_view_t *view);
static void main_view_render_cards(main_view_t *main_view, const screen_view_t *view);
static const char *main_view_card_name(const screen_card_config_t *card);
static bool main_view_action_is_enabled(bool has_action,
                                        const screen_action_t *action);
static void main_view_apply_action_config(main_view_entity_ref_t *ref,
                                          const screen_tile_card_data_t *tile);
static bool main_view_update_ref(main_view_entity_ref_t *ref, const char *name, const char *state);
static bool main_view_apply_cached_entity(main_view_entity_ref_t *ref,
                                          ha_entity_store_t *store);
static void main_view_apply_badge_model(main_view_entity_ref_t *ref,
                                        entity_display_model_t *model,
                                        ha_entity_store_t *store);
static void main_view_apply_badge_action_config(main_view_entity_ref_t *ref,
                                                const screen_badge_config_t *badge);
static bool main_view_badge_domain_is(const char *entity_id, const char *domain);
static void main_view_dispatch_badge_action(main_view_entity_ref_t *ref,
                                            entity_action_trigger_t trigger);
static void main_view_apply_display_model(main_view_entity_ref_t *ref,
                                          const entity_display_model_t *model);
static void main_view_tile_icon_click_cb(lv_obj_t *tile_card, void *user_data);
static void main_view_badge_body_click_cb(lv_obj_t *badge_pill, void *user_data);
static void main_view_tile_body_click_cb(lv_obj_t *tile_card, void *user_data);
static void main_view_badge_body_hold_cb(lv_obj_t *badge_pill, void *user_data);
static void main_view_tile_body_hold_cb(lv_obj_t *tile_card, void *user_data);
static void main_view_dispatch_action(main_view_entity_ref_t *ref,
                                      entity_action_trigger_t trigger);
static void main_view_open_more_info_cb(const char *entity_id,
                                        void *user_data);

main_view_t *main_view_create(lv_obj_t *parent, const screen_view_t *view)
{
    if (!parent) {
        return NULL;
    }

    main_view_t *main_view = lv_malloc(sizeof(main_view_t));
    if (!main_view) {
        return NULL;
    }
    memset(main_view, 0, sizeof(main_view_t));

    main_view->container = lv_obj_create(parent);
    main_view_apply_container_style(main_view->container);
    main_view_update(main_view, view);

    return main_view;
}

void main_view_destroy(main_view_t *main_view)
{
    if (!main_view) {
        return;
    }

    main_view_clear_refs(main_view);
    lv_free(main_view);
}

lv_obj_t *main_view_get_obj(main_view_t *main_view)
{
    return main_view ? main_view->container : NULL;
}

void main_view_update(main_view_t *main_view, const screen_view_t *view)
{
    if (!main_view || !main_view->container || !view) {
        return;
    }

    main_view_clear_refs(main_view);
    lv_obj_clean(main_view->container);
    main_view_render_badges(main_view, view);
    main_view_render_cards(main_view, view);

    ha_entity_store_t *store = app_get_ha_entity_store();
    if (store && main_view_set_entity_store(main_view, store) > 0) {
        esp_err_t err = ha_entity_store_sync(store);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to request entity state sync: %s", esp_err_to_name(err));
        }
    }
}

bool main_view_update_badge(main_view_t *main_view,
                            size_t badge_index,
                            const char *name,
                            const char *state)
{
    if (!main_view) {
        return false;
    }

    for (size_t i = 0; i < main_view->badge_count; i++) {
        main_view_entity_ref_t *badge = &main_view->badges[i];
        if (badge->config_index == badge_index) {
            return main_view_update_ref(badge, name, state);
        }
    }

    return false;
}

size_t main_view_update_entity(main_view_t *main_view,
                               const char *entity_id,
                               const char *name,
                               const char *state)
{
    if (!main_view || !entity_id) {
        return 0;
    }

    size_t updated = 0;
    for (size_t i = 0; i < main_view->badge_count; i++) {
        main_view_entity_ref_t *badge = &main_view->badges[i];
        if (badge->entity_id && strcmp(badge->entity_id, entity_id) == 0) {
            updated += main_view_update_ref(badge, name, state) ? 1U : 0U;
        }
    }
    for (size_t i = 0; i < main_view->entity_count; i++) {
        main_view_entity_ref_t *entity = &main_view->entities[i];
        if (entity->entity_id && strcmp(entity->entity_id, entity_id) == 0) {
            updated += main_view_update_ref(entity, name, state) ? 1U : 0U;
        }
    }

    return updated;
}

size_t main_view_refresh_entity(main_view_t *main_view,
                                ha_entity_store_t *store,
                                const char *entity_id)
{
    if (!main_view || !store || !entity_id) {
        return 0;
    }

    size_t refreshed = 0;
    for (size_t i = 0; i < main_view->badge_count; i++) {
        main_view_entity_ref_t *badge = &main_view->badges[i];
        if (badge->entity_id && strcmp(badge->entity_id, entity_id) == 0) {
            refreshed += main_view_apply_cached_entity(badge, store) ? 1U : 0U;
        }
    }
    for (size_t i = 0; i < main_view->entity_count; i++) {
        main_view_entity_ref_t *entity = &main_view->entities[i];
        if (entity->entity_id && strcmp(entity->entity_id, entity_id) == 0) {
            refreshed += main_view_apply_cached_entity(entity, store) ? 1U : 0U;
        }
    }

    return refreshed;
}

size_t main_view_set_entity_store(main_view_t *main_view,
                                  ha_entity_store_t *store)
{
    if (!main_view) {
        return 0;
    }

    size_t acquired = 0;
    for (size_t i = 0; i < main_view->badge_count + main_view->entity_count; i++) {
        main_view_entity_ref_t *ref = i < main_view->badge_count
                                          ? &main_view->badges[i]
                                          : &main_view->entities[i - main_view->badge_count];
        ha_entity_store_release(ref->entity_handle);
        ref->entity_handle = NULL;

        if (!ref->tile_card && !ref->badge_pill) {
            continue;
        }

        entity_display_model_t display_model;
        entity_display_model_init(ref->entity_id, &display_model);
        if (ref->badge_pill) {
            main_view_apply_badge_model(ref, &display_model, NULL);
        } else {
            main_view_apply_display_model(ref, &display_model);
        }

        if (!store || !ref->entity_id || !ref->entity_id[0]) {
            continue;
        }

        esp_err_t err = ha_entity_store_acquire(store,
                                                ref->entity_id,
                                                entity_display_required_fields(ref->entity_id),
                                                &ref->entity_handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "Failed to acquire entity %s: %s",
                     ref->entity_id,
                     esp_err_to_name(err));
            continue;
        }

        acquired++;
        main_view_apply_cached_entity(ref, store);
    }

    return acquired;
}

static void main_view_apply_container_style(lv_obj_t *container)
{
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_radius(container, 0, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_pad_gap(container, 8, 0);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_scroll_dir(container, LV_DIR_VER);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
}

static void main_view_clear_refs(main_view_t *main_view)
{
    if (!main_view) {
        return;
    }

    for (size_t i = 0; i < main_view->badge_count; i++) {
        ha_entity_store_release(main_view->badges[i].entity_handle);
        main_view->badges[i].entity_handle = NULL;
        main_view->badges[i].badge_pill = NULL;
    }
    for (size_t i = 0; i < main_view->entity_count; i++) {
        ha_entity_store_release(main_view->entities[i].entity_handle);
        main_view->entities[i].entity_handle = NULL;
        main_view->entities[i].tile_card = NULL;
    }

    lv_free(main_view->badges);
    lv_free(main_view->entities);
    main_view->badges = NULL;
    main_view->badge_count = 0;
    main_view->entities = NULL;
    main_view->entity_count = 0;
}

static void main_view_render_badges(main_view_t *main_view, const screen_view_t *view)
{
    if (!view->badges || view->badge_count == 0) {
        return;
    }

    main_view->badges = lv_malloc(sizeof(main_view_entity_ref_t) * view->badge_count);
    if (!main_view->badges) {
        return;
    }
    memset(main_view->badges, 0, sizeof(main_view_entity_ref_t) * view->badge_count);

    lv_obj_t *badges_row = NULL;

    for (size_t i = 0; i < view->badge_count; i++) {
        const screen_badge_config_t *badge = &view->badges[i];
        if (badge->type != SCREEN_BADGE_TYPE_ENTITY || !badge->has_data ||
            !badge_display_supported(badge->data.entity)) {
            continue;
        }

        if (!badges_row) {
            badges_row = lv_obj_create(main_view->container);
            lv_obj_remove_style_all(badges_row);
            lv_obj_remove_flag(badges_row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_size(badges_row, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_layout(badges_row, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(badges_row, LV_FLEX_FLOW_ROW_WRAP);
            lv_obj_set_flex_align(badges_row,
                                  LV_FLEX_ALIGN_START,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(badges_row, 0, 0);
            lv_obj_set_style_pad_hor(badges_row, 20, 0);
            lv_obj_set_style_pad_top(badges_row, 12, 0);
            lv_obj_set_style_pad_column(badges_row, 4, 0);
            lv_obj_set_style_pad_row(badges_row, 4, 0);
        }

        lv_obj_t *pill = ha_badge_create(badges_row);
        if (!pill) {
            ESP_LOGE(TAG, "Failed to create badge for %s", badge->data.entity);
            continue;
        }

        main_view_entity_ref_t *ref = &main_view->badges[main_view->badge_count++];
        ref->config_index = i;
        ref->entity_id = badge->data.entity;
        ref->badge_name = badge->data.name;
        ref->badge_pill = pill;
        main_view_apply_badge_action_config(ref, badge);
        ha_badge_set_body_click_cb(pill,
                                   ref->body_tap_enabled
                                       ? main_view_badge_body_click_cb : NULL,
                                   ref);
        ha_badge_set_body_hold_cb(pill,
                                  ref->body_hold_enabled
                                      ? main_view_badge_body_hold_cb : NULL,
                                  ref);
        ha_badge_set_body_clickable(pill, ref->body_clickable);

        entity_display_model_t model;
        entity_display_model_init(ref->entity_id, &model);
        main_view_apply_badge_model(ref, &model, NULL);
    }
}

static void main_view_render_cards(main_view_t *main_view, const screen_view_t *view)
{
    if (!view->cards || view->card_count == 0) {
        return;
    }

    main_view->entities = lv_malloc(sizeof(main_view_entity_ref_t) * view->card_count);
    if (!main_view->entities) {
        return;
    }
    memset(main_view->entities, 0, sizeof(main_view_entity_ref_t) * view->card_count);

    lv_obj_t *cards_row = lv_obj_create(main_view->container);
    lv_obj_remove_style_all(cards_row);
    lv_obj_remove_flag(cards_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(cards_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(cards_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cards_row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cards_row,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cards_row, 0, 0);
    lv_obj_set_style_pad_gap(cards_row, 8, 0);

    for (size_t i = 0; i < view->card_count; i++) {
        const screen_card_config_t *card_config = &view->cards[i];

        if (card_config->type == SCREEN_CARD_TYPE_TILE) {
            main_view_entity_ref_t *ref = &main_view->entities[main_view->entity_count++];
            ref->config_index = i;
            ref->entity_id = card_config->data.tile.entity;
            main_view_apply_action_config(ref, &card_config->data.tile);

            ref->tile_card = ha_tile_card_create(cards_row);
            if (!ref->tile_card) {
                ESP_LOGE(TAG, "Failed to create tile card for %s",
                         ref->entity_id ? ref->entity_id : "");
                continue;
            }
            entity_display_model_t display_model;
            entity_display_model_init(ref->entity_id, &display_model);
            main_view_apply_display_model(ref, &display_model);

            continue;
        }

        lv_obj_t *card = lv_obj_create(cards_row);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(card, LV_PCT(100));
        lv_obj_set_height(card, 54);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x242936), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(card, 12, 0);
        lv_obj_set_style_pad_ver(card, 8, 0);

        lv_obj_t *name_label = lv_label_create(card);
        lv_label_set_text(name_label, main_view_card_name(card_config));
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_label, LV_PCT(100));
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);

    }
}

static bool main_view_apply_cached_entity(main_view_entity_ref_t *ref,
                                          ha_entity_store_t *store)
{
    if (!ref || !store || !ref->entity_id ||
        (!ref->tile_card && !ref->badge_pill)) {
        return false;
    }

    entity_display_model_t display_model;
    if (entity_display_build(store, ref->entity_id, &display_model) != ESP_OK) {
        return false;
    }

    if (ref->badge_pill) {
        main_view_apply_badge_model(ref, &display_model, store);
    } else {
        main_view_apply_display_model(ref, &display_model);
    }
    return true;
}

static void main_view_apply_badge_model(main_view_entity_ref_t *ref,
                                        entity_display_model_t *model,
                                        ha_entity_store_t *store)
{
    if (!ref || !ref->badge_pill || !model) {
        return;
    }

    char text[ENTITY_DISPLAY_STATE_TEXT_MAX] = {0};
    badge_display_format(store, ref->entity_id, ref->badge_name,
                          model, text, sizeof(text));
    ha_badge_set_icon(ref->badge_pill, model->icon);
    ha_badge_set_icon_color(ref->badge_pill, model->icon_color);
    ha_badge_set_text(ref->badge_pill, text);
    ha_badge_set_text_color(ref->badge_pill,
                            model->unavailable ? lv_color_hex(0x909090)
                                               : lv_color_hex(0xE4E7EE));
}

static bool main_view_badge_domain_is(const char *entity_id, const char *domain)
{
    const size_t length = strlen(domain);
    return entity_id && strncmp(entity_id, domain, length) == 0 &&
           entity_id[length] == '.';
}

static void main_view_apply_badge_action_config(main_view_entity_ref_t *ref,
                                                const screen_badge_config_t *badge)
{
    if (!ref || !badge) {
        return;
    }

    screen_action_config_t config = {0};
    config.has_tap_action = true;
    if(main_view_badge_domain_is(ref->entity_id, "event"))
    {
        config.tap_action.action = SCREEN_ACTION_MORE_INFO;
        config.has_hold_action = true;
        config.hold_action.action = SCREEN_ACTION_NOTHING;
    }else{
    config.tap_action.action =
        main_view_badge_domain_is(ref->entity_id, "sensor") ||
        main_view_badge_domain_is(ref->entity_id, "binary_sensor")
            ? SCREEN_ACTION_NOTHING : SCREEN_ACTION_TOGGLE;
        config.has_hold_action = true;
        config.hold_action.action = SCREEN_ACTION_MORE_INFO;
    }
    if (badge->data.has_action_config) {
        const screen_action_config_t *override = &badge->data.action_config;
        if (override->has_tap_action) {
            config.tap_action = override->tap_action;
        }
        if (override->has_hold_action) {
            config.hold_action = override->hold_action;
        }
    }

    ref->action_config = config;
    ref->has_action_config = true;
    ref->body_tap_enabled = main_view_action_is_enabled(
        config.has_tap_action, &config.tap_action);
    ref->body_hold_enabled = main_view_action_is_enabled(
        config.has_hold_action, &config.hold_action);
    ref->body_clickable = ref->body_tap_enabled || ref->body_hold_enabled;
}

static void main_view_apply_display_model(main_view_entity_ref_t *ref,
                                          const entity_display_model_t *model)
{
    if (!ref || !ref->tile_card || !model) {
        return;
    }

    lv_obj_t *tile_card = ref->tile_card;
    ha_tile_card_set_title(tile_card, model->title);
    if (model->state_text_type == ENTITY_STATE_TEXT_RELATIVE_TIME &&
        model->state_time > 0) {
        ha_tile_card_set_relative_state_time(tile_card, model->state_time);
    }
    else {
        ha_tile_card_set_state_text(tile_card, model->state_text);
    }
    ha_tile_card_set_icon(tile_card, model->icon);
    ha_tile_card_set_unavailable(tile_card, model->unavailable);
    ha_tile_card_set_state_icon(tile_card, model->state_icon);
    ha_tile_card_set_state_icon_visible(tile_card, model->show_state_icon);
    ha_tile_card_set_background_color(tile_card, model->background_color);
    ha_tile_card_set_icon_color(tile_card, model->icon_color);
    ha_tile_card_set_icon_circle_background_color(tile_card,
                                                  model->icon_circle_background_color);
    ha_tile_card_set_ripple_color(tile_card,
                                  model->has_ripple_color
                                      ? model->ripple_color
                                      : model->icon_circle_background_color);
    ha_tile_card_set_title_color(tile_card, model->title_color);
    ha_tile_card_set_state_color(tile_card, model->state_color);
    ha_tile_card_set_icon_click_cb(tile_card,
                                   ref->icon_clickable
                                       ? main_view_tile_icon_click_cb
                                       : NULL,
                                   ref);
    ha_tile_card_set_icon_clickable(tile_card, ref->icon_clickable);
    ha_tile_card_set_body_click_cb(tile_card,
                                   ref->body_tap_enabled
                                       ? main_view_tile_body_click_cb
                                       : NULL,
                                   ref);
    ha_tile_card_set_body_hold_cb(tile_card,
                                  ref->body_hold_enabled
                                      ? main_view_tile_body_hold_cb
                                      : NULL,
                                  ref);
    ha_tile_card_set_body_clickable(tile_card, ref->body_clickable);
}

static void main_view_tile_icon_click_cb(lv_obj_t *tile_card, void *user_data)
{
    (void)tile_card;
    main_view_dispatch_action(user_data, ENTITY_ACTION_TRIGGER_ICON_TAP);
}

static void main_view_badge_body_click_cb(lv_obj_t *badge_pill, void *user_data)
{
    (void)badge_pill;
    main_view_dispatch_badge_action(user_data, ENTITY_ACTION_TRIGGER_TAP);
}

static void main_view_tile_body_click_cb(lv_obj_t *tile_card, void *user_data)
{
    (void)tile_card;
    main_view_dispatch_action(user_data, ENTITY_ACTION_TRIGGER_TAP);
}

static void main_view_badge_body_hold_cb(lv_obj_t *badge_pill, void *user_data)
{
    (void)badge_pill;
    main_view_dispatch_badge_action(user_data, ENTITY_ACTION_TRIGGER_HOLD);
}

static void main_view_tile_body_hold_cb(lv_obj_t *tile_card, void *user_data)
{
    (void)tile_card;
    main_view_dispatch_action(user_data, ENTITY_ACTION_TRIGGER_HOLD);
}

static bool main_view_update_ref(main_view_entity_ref_t *ref, const char *name, const char *state)
{
    if (!ref || (!name && !state)) {
        return false;
    }

    if (ref->tile_card || ref->badge_pill) {
        /* Keep the icon, text and colors in sync with the cached entity. */
        ha_entity_store_t *store = app_get_ha_entity_store();
        if (store) {
            return main_view_apply_cached_entity(ref, store);
        }
    }

    if (ref->badge_pill) {
        ha_badge_set_text(ref->badge_pill, state ? state : name);
        return true;
    }

    if (name && ref->name_label) {
        lv_label_set_text(ref->name_label, name);
    }
    if (state && ref->state_label) {
        lv_label_set_text(ref->state_label, state);
    }

    return true;
}

static const char *main_view_card_name(const screen_card_config_t *card)
{
    if (!card) {
        return "";
    }

    if (card->type == SCREEN_CARD_TYPE_HEADING) {
        return card->data.heading.heading ? card->data.heading.heading : "";
    }

    if (card->type == SCREEN_CARD_TYPE_TILE) {
        if (card->data.tile.name) {
            return card->data.tile.name;
        }
        return card->data.tile.entity ? card->data.tile.entity : "";
    }

    return "";
}

static bool main_view_action_is_enabled(bool has_action,
                                        const screen_action_t *action)
{
    return has_action && action && action->action != SCREEN_ACTION_NOTHING;
}

static void main_view_apply_action_config(main_view_entity_ref_t *ref,
                                          const screen_tile_card_data_t *tile)
{
    if (!ref || !tile) {
        return;
    }

    /* 1. Resolve domain default action configuration for this entity */
    screen_action_config_t effective_config;
    entity_handler_registry_get_default_action_config(ref->entity_id,
                                                      &effective_config);

    /* 2. Override with explicitly configured actions if present */
    if (tile->has_action_config) {
        if (tile->action_config.has_tap_action) {
            effective_config.has_tap_action = true;
            effective_config.tap_action = tile->action_config.tap_action;
        }
        if (tile->action_config.has_hold_action) {
            effective_config.has_hold_action = true;
            effective_config.hold_action = tile->action_config.hold_action;
        }
        if (tile->action_config.has_icon_tap_action) {
            effective_config.has_icon_tap_action = true;
            effective_config.icon_tap_action = tile->action_config.icon_tap_action;
        }
    }

    ref->action_config = effective_config;
    ref->has_action_config = true;
    ref->tile_config = tile;

    const screen_action_config_t *config = &ref->action_config;
    ref->icon_clickable = main_view_action_is_enabled(
        config->has_icon_tap_action,
        &config->icon_tap_action);
    ref->body_tap_enabled = main_view_action_is_enabled(
        config->has_tap_action,
        &config->tap_action);
    ref->body_hold_enabled = main_view_action_is_enabled(
        config->has_hold_action,
        &config->hold_action);
    ref->body_clickable = ref->body_tap_enabled || ref->body_hold_enabled;
}

static void main_view_dispatch_badge_action(main_view_entity_ref_t *ref,
                                            entity_action_trigger_t trigger)
{
    if (!ref || !ref->entity_id || !ref->has_action_config) {
        return;
    }
    const screen_action_t *action = trigger == ENTITY_ACTION_TRIGGER_HOLD
                                        ? &ref->action_config.hold_action
                                        : &ref->action_config.tap_action;
    if (action->action == SCREEN_ACTION_NOTHING) {
        return;
    }
    if (action->action == SCREEN_ACTION_MORE_INFO) {
        main_view_open_more_info_cb(ref->entity_id, ref);
        return;
    }

    /* Cover cards and badges share the domain handler's toggle semantics. */
    if (main_view_badge_domain_is(ref->entity_id, "cover")) {
        main_view_dispatch_action(ref, trigger);
        return;
    }

    const char *domain = NULL;
    const char *service = NULL;
    if (main_view_badge_domain_is(ref->entity_id, "climate")) {
        ha_entity_store_t *store = app_get_ha_entity_store();
        ha_entity_snapshot_t snapshot = {0};
        if (!store || ha_entity_store_get(store, ref->entity_id, &snapshot) != ESP_OK ||
            strcmp(snapshot.state, "unavailable") == 0 ||
            strcmp(snapshot.state, "unknown") == 0) {
            ESP_LOGW(TAG, "Cannot toggle badge %s without an available state",
                     ref->entity_id);
            return;
        }
        domain = "climate";
        service = strcmp(snapshot.state, "off") == 0
                      ? "turn_on" : "turn_off";
    } 
    // else if (main_view_badge_domain_is(ref->entity_id, "fan")) {
        // ha_entity_store_t *store = app_get_ha_entity_store();
        // ha_entity_snapshot_t snapshot = {0};
        // if (!store ||
            // ha_entity_store_get(store, ref->entity_id, &snapshot) != ESP_OK ||
            // (strcmp(snapshot.state, "on") != 0 &&
            //  strcmp(snapshot.state, "off") != 0)) {
            // ESP_LOGW(TAG, "Cannot toggle badge %s without an available state",
                    //  ref->entity_id);
            // return;
        // }
        // domain = "fan";
        // service = "toggle";
    // } 
    else if (main_view_badge_domain_is(ref->entity_id, "event") ||
               main_view_badge_domain_is(ref->entity_id, "sensor") ||
               main_view_badge_domain_is(ref->entity_id, "binary_sensor")) {
        ESP_LOGW(TAG, "Badge entity %s has no toggle service", ref->entity_id);
        return;
    } else {
        main_view_dispatch_action(ref, trigger);
        return;
    }

    if (!service) {
        ESP_LOGW(TAG, "Cannot toggle badge %s in this state", ref->entity_id);
        return;
    }
    esp_err_t err = app_ha_call_entity_service(domain, service, ref->entity_id);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Badge %s.%s for %s failed: %s",
                 domain, service, ref->entity_id, esp_err_to_name(err));
    }
}

static void main_view_dispatch_action(main_view_entity_ref_t *ref,
                                      entity_action_trigger_t trigger)
{
    if (!ref || !ref->entity_id || !ref->has_action_config) {
        return;
    }

    entity_action_context_t context = {
        .entity_id = ref->entity_id,
        .tile_config = ref->tile_config,
        .action_config = &ref->action_config,
        .trigger = trigger,
        .open_more_info = main_view_open_more_info_cb,
        .user_data = ref,
    };

    entity_handler_registry_dispatch_action(&context);
}

static void main_view_open_more_info_cb(const char *entity_id,
                                        void *user_data)
{
    main_view_entity_ref_t *ref = user_data;
    ha_entity_store_t *store = app_get_ha_entity_store();
    if (ref && store) {
        if (ref->badge_pill) {
            ha_badge_stop_ripple(ref->badge_pill);
        }
        if (ref->tile_card) {
            ha_tile_card_stop_ripple(ref->tile_card);
        }
        more_info_screen_open(store, entity_id, ref->tile_config);
    }
}
