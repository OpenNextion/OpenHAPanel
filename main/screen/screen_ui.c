#include "screen_ui.h"
#include "app.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "assets/ha_ui_assets.h"
#include "components/badge_display.h"
#include "components/main_view.h"
#include "components/notification.h"
#include "components/ap_provisioning_view.h"
#include "components/forgot_password_view.h"
#include "components/ha_provisioning_view.h"
#include "components/more_info_screen.h"
#include "components/reset_password_view.h"
#include "components/screen_device_info.h"
#include "components/screen_indicator.h"
#include "components/screen_ota_update.h"
#include "components/system_status_bar.h"
#include "components/toast.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_lvgl_port.h"
#include "firmware_auto_check.h"
#include "firmware_update.h"
#include "freertos/FreeRTOS.h"
#include "ha_entity_store.h"
#include "lwip/ip4_addr.h"
#include "network.h"
#include "network_wifi_ap.h"
#include "screen.h"
#include "sys_time_api.h"
#include "ui_preferences.h"
#include "webserver_config_forget_password.h"

#define SCREEN_DEVICE_INFO_GESTURE_HEIGHT 44
#define SCREEN_DEVICE_INFO_REFRESH_MS 1000
#define SCREEN_MANUAL_OTA_START_DELAY_MS 150
#define SCREEN_DEFAULT_CONTENT_WIDTH 280

typedef struct {
    lv_color_t bg_color;
    const screen_config_t *config;
    system_status_bar_t *status_bar;
    screen_device_info_t *device_info;
    screen_ota_update_t *ota_update;
    lv_obj_t *device_info_gesture_area;
    lv_timer_t *device_info_refresh_timer;
    lv_timer_t *manual_ota_start_timer;
    lv_obj_t *ha_provisioning_root;
    lv_obj_t *tileview;
    screen_indicator_t *screen_indicator;
    lv_obj_t **tiles;
    main_view_t **main_views;
    size_t page_count;
    size_t active_page;
    bool manual_ota_active;
    bool manual_ota_retry_check;
    bool manual_ota_activation_requested;
    bool manual_ota_reboot_requested;
    bool manual_ota_retry_activation;
    bool manual_ota_failure_visible;
    bool manual_ota_failed_candidate_visible;
    char manual_ota_failed_candidate_text[64];
    bool background_ota_check_feedback;
} screen_ctx_t;

static screen_ctx_t *s_screen_ctx = NULL;
static screen_config_t s_screen_config = {0};
static screen_config_t s_pending_screen_config = {0};
static bool s_config_update_pending;
static bool s_state_handler_registered;
static uint32_t s_notification_active_mask;
static uint32_t s_notification_dismissed_mask;
static notification_t *s_active_notification;
static screen_notification_type_t s_visible_notification;
static bool s_wifi_connected_toast_pending;
static toast_t *s_wifi_connected_toast;
static bool s_ha_swipe_hint_seen;
static bool s_ha_swipe_hint_state_loaded;
static bool s_device_info_ota_result_dismissed;
static uint32_t s_device_info_ota_result_revision;
static bool s_device_info_manual_ota_check_active;
static bool s_device_info_manual_ota_result_visible;
static uint32_t s_device_info_manual_ota_result_revision;
static const char *TAG = "screen_ui";

static screen_ctx_t *screen_ctx_create(void);
static void screen_ctx_destroy(screen_ctx_t *ctx);
static void screen_ui_destroy_current(void);
static bool screen_ui_create_system_layer(void);
static void screen_ui_apply_pending_config(void);
static void screen_ui_render_ap_provisioning_page(void);
static void screen_ui_render_centered_label(const char *text);
static void screen_ui_sync_notification(void);
static void screen_ui_sync_wifi_connected_toast(void);
static void screen_ui_wifi_connected_toast_closed_cb(toast_t *toast,
                                                     void *user_data);
static void screen_ui_load_ha_swipe_hint_state(void);
static void screen_ui_ha_swipe_hint_dismissed_cb(void *user_data);
static screen_notification_type_t screen_ui_select_notification(void);
static void screen_ui_notification_closed_cb(notification_t *notification,
                                             notification_close_reason_t reason,
                                             void *user_data);
static void screen_ui_notification_swipe_down_cb(notification_t *notification,
                                                 void *user_data);
static void screen_ui_build_wifi_notification_description(char *output, size_t output_size);
static void screen_render(const screen_config_t *config);
static bool screen_ui_can_enter_page(screen_page_t page);
static void screen_ui_enter_page(screen_page_t page);
static void screen_ui_state_event_handler(void *arg,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          void *event_data);
static void screen_create_components(screen_ctx_t *ctx, lv_obj_t *screen, const screen_config_t *config);
static void screen_create_status_bar(screen_ctx_t *ctx, lv_obj_t *screen);
static void screen_create_pages(screen_ctx_t *ctx, lv_obj_t *screen, const screen_config_t *config);
static void screen_create_indicator(screen_ctx_t *ctx, lv_obj_t *screen);
static void screen_create_default_content(lv_obj_t *parent, lv_coord_t y_offset);
static bool screen_view_has_badges_or_entities(const screen_view_t *view);
static void screen_create_device_info(screen_ctx_t *ctx);
static bool screen_device_info_is_available(void);
static void screen_update_device_info(screen_ctx_t *ctx);
static void screen_device_info_refresh_timer_cb(lv_timer_t *timer);
static void screen_device_info_check_update_cb(screen_device_info_t *view,
                                               void *user_data);
static void screen_device_info_upgrade_cb(screen_device_info_t *view,
                                          void *user_data);
static void screen_device_info_hidden_cb(screen_device_info_t *view,
                                         void *user_data);
static void screen_manual_ota_start_timer_cb(lv_timer_t *timer);
static void screen_manual_ota_start_install(screen_ctx_t *ctx);
static void screen_manual_ota_retry_cb(screen_ota_update_t *view,
                                       void *user_data);
static void screen_manual_ota_cancel_cb(screen_ota_update_t *view,
                                        void *user_data);
static void screen_update_manual_ota(screen_ctx_t *ctx,
                                     const firmware_update_status_t *status);
static void screen_manual_ota_show_failure(screen_ctx_t *ctx,
                                           esp_err_t error,
                                           bool retry_activation);
static bool screen_get_netif_ipv4(const char *if_key, char *output, size_t output_size);
static void screen_get_firmware_version(char *output, size_t output_size);
static void screen_get_available_firmware_version(const firmware_update_status_t *status,
                                                  char *output,
                                                  size_t output_size);
static void screen_get_staged_firmware_version(char *output, size_t output_size);
static void screen_format_target_firmware_version(const char *version,
                                                  const char *published_at,
                                                  char *output,
                                                  size_t output_size);
static int screen_month_number(const char *month_name);
static void screen_device_info_open_gesture_cb(lv_event_t *event);
static void screen_device_info_close_gesture_cb(lv_event_t *event);
static lv_dir_t screen_page_scroll_dirs(size_t index, size_t count);
static void screen_tileview_event_cb(lv_event_t *event);
static void screen_update_active_page(screen_ctx_t *ctx, size_t active_page);
static bool screen_find_page_position(const screen_ctx_t *ctx, int32_t screen_index, size_t *position);

void clear_lvgl_screen(void)
{
    lv_obj_clean(lv_screen_active());
    lv_obj_clean(lv_layer_top());
    lv_obj_clean(lv_layer_sys());
    lv_obj_clean(lv_layer_bottom());
}

static void screen_render(const screen_config_t *config)
{
    if (!s_screen_ctx) {
        ESP_LOGE(TAG, "Cannot render split view without system layer");
        return;
    }

    lv_obj_t *screen = lv_screen_active();
    screen_create_components(s_screen_ctx, screen, config);
}

esp_err_t screen_ui_init(void)
{
    screen_ui_load_ha_swipe_hint_state();

    if (s_state_handler_registered) {
        return ESP_OK;
    }

    esp_err_t err = esp_event_handler_register(SCREEN_STATE_EVENT,
                                               SCREEN_STATE_EVENT_TRANSITION_REQUESTED,
                                               screen_ui_state_event_handler,
                                               NULL);
    if (err == ESP_OK) {
        s_state_handler_registered = true;
    }
    return err;
}

esp_err_t screen_ui_reload(void)
{
    cJSON *config_json = screen_load_config();
    if (!config_json) {
        ESP_LOGE(TAG, "Failed to load screen config JSON");
        return ESP_FAIL;
    }

    screen_config_t parsed_config = {0};
    esp_err_t ret = screen_parse_config(config_json, &parsed_config);
    cJSON_Delete(config_json);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse screen config JSON");
        screen_free_config(&parsed_config);
        return ret;
    }

    if (s_config_update_pending) {
        screen_free_config(&s_pending_screen_config);
    }
    s_pending_screen_config = parsed_config;
    s_config_update_pending = true;
    esp_err_t refresh_err = screen_state_request_refresh();
    if (refresh_err != ESP_OK && refresh_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to request screen refresh: %s", esp_err_to_name(refresh_err));
        return refresh_err;
    }
    return ESP_OK;
}

static bool screen_ui_can_enter_page(screen_page_t page)
{
    screen_page_t password_page = webserver_config_forget_password_get_screen_page();
    if (password_page != SCREEN_PAGE_NONE &&
        page != password_page &&
        page != SCREEN_PAGE_RESTARTING) {
        return false;
    }

    switch (page) {
    case SCREEN_PAGE_SPLIT_VIEW:
        return true;
    case SCREEN_PAGE_AP_PROVISIONING:
    case SCREEN_PAGE_HA_PROVISIONING:
        return true;
    case SCREEN_PAGE_FORGOT_PASSWORD:
    case SCREEN_PAGE_RESET_PASSWORD:
        return page == password_page;
    case SCREEN_PAGE_RESTARTING:
        return true;
    default:
        return false;
    }
}

static void screen_ui_enter_page(screen_page_t page)
{
    screen_ui_destroy_current();
    if (page != SCREEN_PAGE_HA_PROVISIONING) {
        s_wifi_connected_toast_pending = false;
    }
    screen_ui_apply_pending_config();

    if (page != SCREEN_PAGE_RESTARTING && !screen_ui_create_system_layer()) {
        ESP_LOGE(TAG,
                 "Failed to create system layer for page: %s",
                 screen_state_to_string(page));
        return;
    }

    switch (page) {
    case SCREEN_PAGE_AP_PROVISIONING:
        screen_ui_render_ap_provisioning_page();
        break;
    case SCREEN_PAGE_HA_PROVISIONING: {
        const ha_provisioning_view_config_t ha_view_config = {
            .show_device_info_hint = !s_ha_swipe_hint_seen,
            .hint_dismissed_cb = screen_ui_ha_swipe_hint_dismissed_cb,
            .hint_dismissed_user_data = NULL,
        };
        s_screen_ctx->ha_provisioning_root =
            ha_provisioning_view_create(lv_screen_active(), &ha_view_config);
        break;
    }
    case SCREEN_PAGE_FORGOT_PASSWORD:
        forgot_password_view_create(lv_screen_active());
        break;
    case SCREEN_PAGE_RESET_PASSWORD:
        reset_password_view_create(lv_screen_active());
        break;
    case SCREEN_PAGE_SPLIT_VIEW:
        screen_render(&s_screen_config);
        break;
    case SCREEN_PAGE_RESTARTING:
        ESP_LOGI(TAG, "Entered screen page without renderer: %s", screen_state_to_string(page));
        break;
    default:
        ESP_LOGW(TAG, "Cannot render unsupported screen page: %d", page);
        break;
    }

    if (page != SCREEN_PAGE_RESTARTING) {
        screen_ui_sync_wifi_connected_toast();
        screen_ui_sync_notification();
    }
}

static void screen_ui_state_event_handler(void *arg,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          void *event_data)
{
    (void)arg;
    (void)event_base;
    if (event_id != SCREEN_STATE_EVENT_TRANSITION_REQUESTED || !event_data) {
        return;
    }

    const screen_state_transition_request_t *request = event_data;
    if (!lvgl_port_lock(0)) {
        ESP_LOGW(TAG, "Rejected screen transition because LVGL is busy");
        screen_state_reject_transition(request);
        return;
    }

    if (!screen_ui_can_enter_page(request->requested)) {
        ESP_LOGW(TAG,
                 "Rejected screen transition: %s -> %s",
                 screen_state_to_string(request->current),
                 screen_state_to_string(request->requested));
        screen_state_reject_transition(request);
        lvgl_port_unlock();
        return;
    }

    esp_err_t err = screen_state_commit_transition(request);
    if (err == ESP_OK) {
        screen_ui_enter_page(request->requested);
    } else if (err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to commit screen transition: %s", esp_err_to_name(err));
    }
    lvgl_port_unlock();
}

void screen_ui_set_status_title(const char *title)
{
    if (!s_screen_ctx) {
        return;
    }

    system_status_bar_set_title(s_screen_ctx->status_bar, title);
}

void screen_ui_set_status_time(const char *time_str)
{
    if (!s_screen_ctx) {
        return;
    }

    system_status_bar_set_time(s_screen_ctx->status_bar, time_str);
}

void screen_ui_set_ota_update_available(bool available)
{
    system_status_bar_set_ota_update_visible(available);
}

void screen_ui_set_ha_connected(bool connected)
{
    system_status_bar_set_hass_connected_visible(connected);
}

void screen_ui_set_wifi_sta_connected(bool connected)
{
    system_status_bar_set_wifi_sta_connected_visible(connected);
}

void screen_ui_set_active_screen(int32_t screen_index)
{
    size_t position = 0;
    if (!s_screen_ctx || !s_screen_ctx->tileview ||
        !screen_find_page_position(s_screen_ctx, screen_index, &position)) {
        return;
    }

    lv_tileview_set_tile_by_index(s_screen_ctx->tileview, (uint32_t)position, 0, LV_ANIM_ON);
    screen_update_active_page(s_screen_ctx, position);
}

void screen_ui_show_device_info(void)
{
    if (!s_screen_ctx || !screen_device_info_is_available()) {
        return;
    }

    if (s_screen_ctx->ha_provisioning_root) {
        ha_provisioning_view_dismiss_hint(s_screen_ctx->ha_provisioning_root);
    }
    screen_update_device_info(s_screen_ctx);
    if (s_screen_ctx->device_info_refresh_timer) {
        lv_timer_resume(s_screen_ctx->device_info_refresh_timer);
    }
    screen_device_info_show(s_screen_ctx->device_info);
}

void screen_ui_hide_device_info(void)
{
    if (!s_screen_ctx) {
        return;
    }

    if (s_screen_ctx->device_info_refresh_timer) {
        lv_timer_pause(s_screen_ctx->device_info_refresh_timer);
    }
    screen_device_info_hide(s_screen_ctx->device_info);
}

void screen_ui_set_notification_active(screen_notification_type_t type, bool active)
{
    if (type <= 0 || type >= SCREEN_NOTIFICATION_COUNT) {
        return;
    }

    uint32_t mask = 1U << (uint32_t)type;
    bool was_active = (s_notification_active_mask & mask) != 0;
    if (active) {
        s_notification_active_mask |= mask;
        if (!was_active) {
            s_notification_dismissed_mask &= ~mask;
        }
    }
    else {
        s_notification_active_mask &= ~mask;
        s_notification_dismissed_mask &= ~mask;
    }

    screen_ui_sync_notification();
}

void screen_ui_set_wifi_connected_toast_pending(bool pending)
{
    s_wifi_connected_toast_pending = pending;
    screen_ui_sync_wifi_connected_toast();
}

bool screen_ui_update_badge(int32_t screen_index,
                            size_t badge_index,
                            const char *name,
                            const char *state)
{
    size_t position = 0;
    if (!s_screen_ctx || !s_screen_ctx->main_views ||
        !screen_find_page_position(s_screen_ctx, screen_index, &position)) {
        return false;
    }

    return main_view_update_badge(s_screen_ctx->main_views[position], badge_index, name, state);
}

size_t screen_ui_update_entity(int32_t screen_index,
                               const char *entity_id,
                               const char *name,
                               const char *state)
{
    size_t position = 0;
    if (!s_screen_ctx || !s_screen_ctx->main_views ||
        !screen_find_page_position(s_screen_ctx, screen_index, &position)) {
        return 0;
    }

    return main_view_update_entity(s_screen_ctx->main_views[position], entity_id, name, state);
}

size_t screen_ui_update_entity_all(const char *entity_id,
                                   const char *name,
                                   const char *state)
{
    if (!s_screen_ctx || !s_screen_ctx->main_views || !entity_id) {
        return 0;
    }

    size_t updated = 0;
    for (size_t i = 0; i < s_screen_ctx->page_count; i++) {
        updated += main_view_update_entity(s_screen_ctx->main_views[i], entity_id, name, state);
    }

    return updated;
}

size_t screen_ui_refresh_entity_all(ha_entity_store_t *store,
                                    const char *entity_id)
{
    if (!s_screen_ctx || !s_screen_ctx->main_views || !store || !entity_id) {
        return 0;
    }

    size_t refreshed = 0;
    for (size_t i = 0; i < s_screen_ctx->page_count; i++) {
        refreshed += main_view_refresh_entity(s_screen_ctx->main_views[i], store, entity_id);
    }

    more_info_screen_refresh_entity(store, entity_id);

    return refreshed;
}

void screen_ui_set_entity_store(ha_entity_store_t *store)
{
    if (!s_screen_ctx || !s_screen_ctx->main_views) {
        return;
    }

    size_t acquired = 0;
    for (size_t i = 0; i < s_screen_ctx->page_count; i++) {
        acquired += main_view_set_entity_store(s_screen_ctx->main_views[i], store);
    }

    if (store && acquired > 0) {
        esp_err_t err = ha_entity_store_sync(store);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to synchronize rebound entities: %s", esp_err_to_name(err));
        }
    }
}

static screen_ctx_t *screen_ctx_create(void)
{
    screen_ctx_t *ctx = lv_malloc(sizeof(screen_ctx_t));
    if (!ctx) {
        return NULL;
    }
    memset(ctx, 0, sizeof(screen_ctx_t));
    ctx->bg_color = lv_color_hex(0x010203);

    return ctx;
}

static void screen_ctx_destroy(screen_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->main_views) {
        for (size_t i = 0; i < ctx->page_count; i++) {
            main_view_destroy(ctx->main_views[i]);
        }
        lv_free(ctx->main_views);
    }
    if (ctx->tiles) {
        lv_free(ctx->tiles);
    }
    if (ctx->device_info_refresh_timer) {
        lv_timer_delete(ctx->device_info_refresh_timer);
        ctx->device_info_refresh_timer = NULL;
    }
    if (ctx->manual_ota_start_timer) {
        lv_timer_delete(ctx->manual_ota_start_timer);
        ctx->manual_ota_start_timer = NULL;
    }
    screen_indicator_destroy(ctx->screen_indicator);
    screen_device_info_destroy(ctx->device_info);
    screen_ota_update_destroy(ctx->ota_update);
    lv_free(ctx);
}

static void screen_ui_destroy_current(void)
{
    clear_lvgl_screen();
    screen_ctx_destroy(s_screen_ctx);
    s_screen_ctx = NULL;
}

static bool screen_ui_create_system_layer(void)
{
    s_screen_ctx = screen_ctx_create();
    if (!s_screen_ctx) {
        return false;
    }

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, s_screen_ctx->bg_color, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    screen_create_device_info(s_screen_ctx);
    if (!s_screen_ctx->device_info || !s_screen_ctx->device_info_gesture_area ||
        !s_screen_ctx->ota_update) {
        screen_ui_destroy_current();
        return false;
    }

    ESP_LOGI(TAG, "System device information overlay initialized");
    return true;
}

static void screen_ui_apply_pending_config(void)
{
    if (!s_config_update_pending) {
        return;
    }

    screen_free_config(&s_screen_config);
    s_screen_config = s_pending_screen_config;
    memset(&s_pending_screen_config, 0, sizeof(s_pending_screen_config));
    s_config_update_pending = false;
}

static void screen_ui_render_ap_provisioning_page(void)
{
    NetworkState *network = network_get_state();
    const char *ssid = NETWORK_DEFAULT_AP_SSID_PREFIX;
    char setup_url[32] = NETWORK_DEFAULT_AP_IP;
    char credential[96] = "No password";

    if (network) {
        if (network->ap_params.ssid[0] != '\0') {
            ssid = (const char *)network->ap_params.ssid;
        }

        if (network->current_ap_ip.gateway != 0) {
            ip4_addr_t gateway = {.addr = network->current_ap_ip.gateway};
            char gateway_text[16] = {0};
            if (ip4addr_ntoa_r(&gateway, gateway_text, sizeof(gateway_text))) {
                strlcpy(setup_url, gateway_text, sizeof(setup_url));
            }
        }

        if (network->ap_params.password[0] != '\0') {
            snprintf(credential,
                     sizeof(credential),
                     "Password: %.64s",
                     (const char *)network->ap_params.password);
        }
    }

    const ap_provisioning_view_config_t config = {
        .ssid = ssid,
        .setup_url = setup_url,
        .credential_text = credential,
    };
    ap_provisioning_view_create(lv_screen_active(), &config);
}

static void screen_ui_render_centered_label(const char *text)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x010203), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);
}


static screen_notification_type_t screen_ui_select_notification(void)
{
    static const screen_notification_type_t priorities[] = {
        SCREEN_NOTIFICATION_WIFI_DISCONNECTED,
        SCREEN_NOTIFICATION_FIRMWARE_UPDATE,
        SCREEN_NOTIFICATION_HA_AUTH_EXPIRED,
        SCREEN_NOTIFICATION_HA_AUTH_CANCELED,
        SCREEN_NOTIFICATION_HA_DISCONNECTED,
    };
    const screen_page_t current_page = screen_state_get_current();

    for (size_t i = 0; i < sizeof(priorities) / sizeof(priorities[0]); i++) {
        if (current_page == SCREEN_PAGE_AP_PROVISIONING &&
            priorities[i] == SCREEN_NOTIFICATION_FIRMWARE_UPDATE) {
            continue;
        }

        uint32_t mask = 1U << (uint32_t)priorities[i];
        if ((s_notification_active_mask & mask) &&
            !(s_notification_dismissed_mask & mask)) {
            return priorities[i];
        }
    }
    return 0;
}

static void screen_ui_sync_wifi_connected_toast(void)
{
    bool can_show = s_wifi_connected_toast_pending &&
                    screen_state_get_current() == SCREEN_PAGE_HA_PROVISIONING &&
                    s_screen_ctx && s_screen_ctx->ha_provisioning_root;

    if (!can_show) {
        if (s_wifi_connected_toast) {
            toast_t *toast = s_wifi_connected_toast;
            s_wifi_connected_toast = NULL;
            toast_close(toast);
        }
        return;
    }
    if (s_wifi_connected_toast) {
        return;
    }

    const toast_config_t config = {
        .title = "Wi-Fi Connected",
        .icon_src = &ok_icon_green,
        .duration_ms = 2000,
        .background_color = 0x272B32,
        .background_opa = LV_OPA_80,
        .width = 280,
        .height = 60,
        .x = 0,
        .y = 20,
        .radius = 12,
        .behind_siblings = false,
        .closed_cb = screen_ui_wifi_connected_toast_closed_cb,
        .closed_user_data = NULL,
    };
    s_wifi_connected_toast = toast_show(s_screen_ctx->ha_provisioning_root,
                                        &config);
    if (!s_wifi_connected_toast) {
        s_wifi_connected_toast_pending = false;
        ESP_LOGE(TAG, "Failed to show Wi-Fi-connected Toast");
    }
}

static void screen_ui_wifi_connected_toast_closed_cb(toast_t *toast,
                                                     void *user_data)
{
    (void)user_data;
    if (toast != s_wifi_connected_toast) {
        return;
    }

    s_wifi_connected_toast = NULL;
    s_wifi_connected_toast_pending = false;
}

static void screen_ui_load_ha_swipe_hint_state(void)
{
    if (s_ha_swipe_hint_state_loaded) {
        return;
    }

    bool seen = false;
    esp_err_t err = ui_preferences_get_ha_swipe_hint_seen(&seen);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to load HA swipe-hint preference: %s",
                 esp_err_to_name(err));
    }

    s_ha_swipe_hint_seen = err == ESP_OK && seen;
    s_ha_swipe_hint_state_loaded = true;
}

static void screen_ui_ha_swipe_hint_dismissed_cb(void *user_data)
{
    (void)user_data;
    if (s_ha_swipe_hint_seen) {
        return;
    }

    s_ha_swipe_hint_seen = true;
    esp_err_t err = ui_preferences_set_ha_swipe_hint_seen();
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "Failed to save HA swipe-hint preference: %s",
                 esp_err_to_name(err));
    }
}

static void screen_ui_sync_notification(void)
{
    screen_notification_type_t selected = screen_ui_select_notification();
    if (s_active_notification && s_visible_notification == selected) {
        return;
    }

    if (s_active_notification) {
        notification_t *previous = s_active_notification;
        s_active_notification = NULL;
        s_visible_notification = 0;
        notification_close(previous);
    }

    if (!selected || !s_screen_ctx) {
        return;
    }

    char wifi_description[256] = {0};
    char firmware_status[96] = {0};
    notification_config_t config = {
        .durations = 0,
        .tips = NULL,
        .closeable = false,
        .swipe_down_cb = screen_ui_notification_swipe_down_cb,
        .swipe_down_user_data = NULL,
        .closed_cb = screen_ui_notification_closed_cb,
        .closed_user_data = NULL,
    };

    switch (selected) {
    case SCREEN_NOTIFICATION_HA_AUTH_CANCELED:
        config.title = "Disconnected from Home Assistant";
        config.icon_src = &hass_red_48x48;
        config.status_icon_src = &warning_red;
        config.status_text = "Not connected to Home Assistant";
        config.description = "Reconnect to Home Assistant in the Web Console.";
        config.icon_includes_background = true;
        config.icon_size = 48;
        config.status_in_header = false;
        config.dialog_background_color = 0x3B1919;
        config.accent_color = 0xFF5C62;
        config.description_color = 0x777277;
        config.level = NOTIFICATION_LEVEL_ERROR;
        break;
    case SCREEN_NOTIFICATION_HA_DISCONNECTED:
        config.title = "Home Assistant Connection Lost";
        config.icon_src = &hass_yellow_48x48;
        config.status_icon_src = &warning_yellow;
        config.status_text = "Not connected to Home Assistant";
        config.description = "Check that Home Assistant is running and reachable.";
        config.icon_includes_background = true;
        config.icon_size = 48;
        config.status_in_header = false;
        config.dialog_background_color = 0x332B17;
        config.accent_color = 0xFF9800;
        config.description_color = 0x777277;
        config.level = NOTIFICATION_LEVEL_WARNING;
        break;
    case SCREEN_NOTIFICATION_HA_AUTH_EXPIRED:
        config.title = "Home Assistant Authorization Invalid";
        config.icon_src = &hass_grey_48x48;
        config.status_icon_src = &warning_grey;
        config.status_text = "Not connected to Home Assistant";
        config.description = "Reauthorize Home Assistant in the Web Console.";
        config.icon_includes_background = true;
        config.icon_size = 48;
        config.status_in_header = false;
        config.dialog_background_color = 0x17263C;
        config.accent_color = 0x777277;
        config.description_color = 0x777277;
        config.level = NOTIFICATION_LEVEL_INACTIVE;
        break;
    case SCREEN_NOTIFICATION_WIFI_DISCONNECTED:
        screen_ui_build_wifi_notification_description(wifi_description,
                                                      sizeof(wifi_description));
        config.title = "Wi-Fi Disconnected";
        config.icon_src = &wifi_disconnected_48x48;
        config.status_icon_src = &warning_red;
        config.status_text = "Connection lost";
        config.description = wifi_description;
        config.icon_includes_background = true;
        config.icon_size = 48;
        config.status_in_header = false;
        config.dialog_background_color = 0x3B1919;
        config.accent_color = 0xFF5C62;
        config.description_color = 0x777277;
        config.level = NOTIFICATION_LEVEL_ERROR;
        break;
    case SCREEN_NOTIFICATION_FIRMWARE_UPDATE: {
        char staged_version[64] = {0};
        screen_get_staged_firmware_version(staged_version,
                                           sizeof(staged_version));
        snprintf(firmware_status,
                 sizeof(firmware_status),
                 "Update available%s%s",
                 staged_version[0] ? " " : "",
                 staged_version);
        config.title = "Firmware Update";
        config.icon_src = &cloud_update_32;
        config.status_text = firmware_status;
        config.description = "Swipe down to update";
        config.closeable = true;
        config.icon_includes_background = true;
        config.dialog_background_color = 0x17263C;
        config.accent_color = 0x0D84FF;
        config.description_color = 0x666A70;
        config.level = NOTIFICATION_LEVEL_INFO;
        break;
    }
    default:
        return;
    }

    s_active_notification = notification_show(&config);
    if (!s_active_notification) {
        ESP_LOGE(TAG, "Failed to show system notification: %d", selected);
        return;
    }
    s_visible_notification = selected;
    if (screen_ota_update_is_visible(s_screen_ctx->ota_update)) {
        screen_ota_update_bring_to_front(s_screen_ctx->ota_update);
    }
    else if (screen_device_info_is_visible(s_screen_ctx->device_info)) {
        lv_obj_t *device_info_obj =
            screen_device_info_get_obj(s_screen_ctx->device_info);
        if (device_info_obj) {
            lv_obj_move_foreground(device_info_obj);
        }
    }
}

static void screen_ui_notification_swipe_down_cb(notification_t *notification,
                                                 void *user_data)
{
    (void)user_data;
    if (notification != s_active_notification) {
        return;
    }

    ESP_LOGI(TAG, "Opening device information page from notification swipe-down gesture");
    screen_ui_show_device_info();
}

static void screen_ui_notification_closed_cb(notification_t *notification,
                                             notification_close_reason_t reason,
                                             void *user_data)
{
    (void)user_data;
    if (notification != s_active_notification) {
        return;
    }

    screen_notification_type_t closed_type = s_visible_notification;
    s_active_notification = NULL;
    s_visible_notification = 0;
    if ((reason == NOTIFICATION_CLOSE_USER || reason == NOTIFICATION_CLOSE_TIMEOUT) &&
        closed_type > 0 && closed_type < SCREEN_NOTIFICATION_COUNT) {
        s_notification_dismissed_mask |= 1U << (uint32_t)closed_type;
    }
}

static void screen_ui_build_wifi_notification_description(char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }

    NetworkState *network = network_get_state();
    const char *ssid = network && network->ap_params.ssid[0]
                           ? (const char *)network->ap_params.ssid
                           : NETWORK_DEFAULT_AP_SSID_PREFIX;
    const char *password = network && network->ap_params.password[0]
                               ? (const char *)network->ap_params.password
                               : NULL;
    char ap_ip[16] = {0};
    if (!screen_get_netif_ipv4("WIFI_AP_DEF", ap_ip, sizeof(ap_ip))) {
        if (network && network->current_ap_ip.gateway != 0) {
            esp_ip4_addr_t gateway = {.addr = network->current_ap_ip.gateway};
            snprintf(ap_ip, sizeof(ap_ip), IPSTR, IP2STR(&gateway));
        }
        else {
            strlcpy(ap_ip, NETWORK_DEFAULT_AP_IP, sizeof(ap_ip));
        }
    }

    snprintf(output,
             output_size,
             "Join hotspot: %s, Open http://%s and select a working 2.4GHz Wi-Fi",
             ssid,
             ap_ip);

    /*
    if (password) {
        snprintf(output,
                 output_size,
                 "Join hotspot: %s,%s\nhttp://%s and select a working 2.4GHz Wi-Fi",
                 ssid,
                 password,
                 ap_ip);
    }
    else {
        snprintf(output,
                 output_size,
                 "Join hotspot: %s\nhttp://%s and select a working 2.4GHz Wi-Fi",
                 ssid,
                 ap_ip);
    }
    */
}

static void screen_create_components(screen_ctx_t *ctx, lv_obj_t *screen, const screen_config_t *config)
{
    if (!ctx || !screen) {
        return;
    }

    ctx->config = config;
    ctx->page_count = config && config->screens ? config->screen_count : 0;
    ctx->active_page = 0;

    screen_create_status_bar(ctx, screen);
    if (ctx->page_count == 0) {
        system_status_bar_set_title(ctx->status_bar, "default screen");
        screen_create_default_content(screen, 0);
        return;
    }

    screen_create_pages(ctx, screen, config);
    screen_create_indicator(ctx, screen);
    screen_update_active_page(ctx, 0);
}

static void screen_create_device_info(screen_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    lv_obj_t *top_layer = lv_layer_top();
    lv_obj_t *system_layer = lv_layer_sys();
    ctx->device_info = screen_device_info_create(system_layer);
    if (!ctx->device_info) {
        ESP_LOGE(TAG, "Failed to create device information page");
        return;
    }

    lv_obj_t *device_info_obj = screen_device_info_get_obj(ctx->device_info);
    lv_obj_add_event_cb(device_info_obj,
                        screen_device_info_close_gesture_cb,
                        LV_EVENT_GESTURE,
                        ctx);
    screen_device_info_set_check_update_cb(ctx->device_info,
                                           screen_device_info_check_update_cb,
                                           ctx);
    screen_device_info_set_upgrade_cb(ctx->device_info,
                                      screen_device_info_upgrade_cb,
                                      ctx);
    screen_device_info_set_hidden_cb(ctx->device_info,
                                     screen_device_info_hidden_cb,
                                     ctx);

    ctx->device_info_gesture_area = lv_obj_create(top_layer);
    if (!ctx->device_info_gesture_area) {
        ESP_LOGE(TAG, "Failed to create device information gesture area");
        return;
    }

    lv_obj_remove_style_all(ctx->device_info_gesture_area);
    lv_obj_remove_flag(ctx->device_info_gesture_area,
                       LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(ctx->device_info_gesture_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ctx->device_info_gesture_area,
                    LV_PCT(100),
                    SCREEN_DEVICE_INFO_GESTURE_HEIGHT);
    lv_obj_align(ctx->device_info_gesture_area, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(ctx->device_info_gesture_area, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(ctx->device_info_gesture_area,
                        screen_device_info_open_gesture_cb,
                        LV_EVENT_GESTURE,
                        ctx);

    if (!screen_device_info_is_available()) {
        lv_obj_add_flag(ctx->device_info_gesture_area, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Device information gesture disabled on AP provisioning page");
    }

    ctx->device_info_refresh_timer = lv_timer_create(screen_device_info_refresh_timer_cb,
                                                      SCREEN_DEVICE_INFO_REFRESH_MS,
                                                      ctx);
    if (ctx->device_info_refresh_timer) {
        lv_timer_pause(ctx->device_info_refresh_timer);
    }
    else {
        ESP_LOGW(TAG, "Failed to create device information refresh timer");
    }

    ctx->ota_update = screen_ota_update_create(system_layer);
    if (!ctx->ota_update) {
        ESP_LOGE(TAG, "Failed to create firmware update overlay");
        return;
    }
    screen_ota_update_set_callbacks(ctx->ota_update,
                                    screen_manual_ota_retry_cb,
                                    screen_manual_ota_cancel_cb,
                                    ctx);
}

static bool screen_device_info_is_available(void)
{
    return screen_state_get_current() != SCREEN_PAGE_AP_PROVISIONING;
}

static void screen_update_device_info(screen_ctx_t *ctx)
{
    if (!ctx || !ctx->device_info) {
        return;
    }

    screen_device_info_data_t data = {0};

    wifi_ap_record_t station = {0};
    if (esp_wifi_sta_get_ap_info(&station) == ESP_OK) {
        data.wifi_connected = true;
        size_t ssid_length = strnlen((const char *)station.ssid, sizeof(station.ssid));
        snprintf(data.wifi_ssid,
                 sizeof(data.wifi_ssid),
                 "%.*s",
                 (int)ssid_length,
                 (const char *)station.ssid);
        screen_get_netif_ipv4("WIFI_STA_DEF", data.wifi_ip, sizeof(data.wifi_ip));
    }

    data.ap_enabled = network_wifi_get_ap_enabled();
    if (data.ap_enabled) {
        wifi_config_t ap_config = {0};
        if (esp_wifi_get_config(WIFI_IF_AP, &ap_config) == ESP_OK) {
            size_t ssid_length = ap_config.ap.ssid_len;
            if (ssid_length == 0 || ssid_length > sizeof(ap_config.ap.ssid)) {
                ssid_length = strnlen((const char *)ap_config.ap.ssid,
                                      sizeof(ap_config.ap.ssid));
            }
            snprintf(data.ap_ssid,
                     sizeof(data.ap_ssid),
                     "%.*s",
                     (int)ssid_length,
                     (const char *)ap_config.ap.ssid);
        }

        if (!data.ap_ssid[0]) {
            NetworkState *network = network_get_state();
            if (network) {
                strlcpy(data.ap_ssid,
                        (const char *)network->ap_params.ssid,
                        sizeof(data.ap_ssid));
            }
        }
        screen_get_netif_ipv4("WIFI_AP_DEF", data.ap_ip, sizeof(data.ap_ip));
    }

    screen_get_firmware_version(data.firmware_version,
                                sizeof(data.firmware_version));

    time_t now = 0;
    time(&now);
    if (now >= 1577836800) {
        struct tm *local_time = sys_time_get_local_time();
        if (local_time) {
            strftime(data.time_text,
                     sizeof(data.time_text),
                     "%H:%M",
                     local_time);
        }
    }

    screen_device_info_set_data(ctx->device_info, &data);

    firmware_update_status_t update_status = {0};
    if (firmware_update_get_status(&update_status) == ESP_OK) {
        screen_device_info_update_state_t view_state = SCREEN_DEVICE_INFO_UPDATE_IDLE;
        if (s_device_info_manual_ota_result_visible &&
            update_status.revision != s_device_info_manual_ota_result_revision) {
            s_device_info_manual_ota_result_visible = false;
        }
        bool manual_check_result =
            update_status.state == FIRMWARE_UPDATE_STATE_UP_TO_DATE ||
            update_status.state == FIRMWARE_UPDATE_STATE_AVAILABLE ||
            update_status.state == FIRMWARE_UPDATE_STATE_STAGED ||
            update_status.state == FIRMWARE_UPDATE_STATE_FAILED;
        if (s_device_info_manual_ota_check_active && manual_check_result) {
            s_device_info_manual_ota_check_active = false;
            s_device_info_manual_ota_result_visible = true;
            s_device_info_manual_ota_result_revision = update_status.revision;
        }
        bool show_manual_ota_state =
            s_device_info_manual_ota_check_active ||
            (s_device_info_manual_ota_result_visible &&
             update_status.revision == s_device_info_manual_ota_result_revision);
        if (s_device_info_ota_result_dismissed &&
            update_status.revision != s_device_info_ota_result_revision) {
            s_device_info_ota_result_dismissed = false;
        }
        bool ota_result_dismissed =
            s_device_info_ota_result_dismissed &&
            update_status.revision == s_device_info_ota_result_revision;
        bool background_download_active =
            update_status.state == FIRMWARE_UPDATE_STATE_DOWNLOADING ||
            update_status.state == FIRMWARE_UPDATE_STATE_VERIFYING;
        if (!background_download_active) {
            ctx->background_ota_check_feedback = false;
        }

        switch (update_status.state) {
        case FIRMWARE_UPDATE_STATE_CHECKING:
            if (show_manual_ota_state) {
                view_state = SCREEN_DEVICE_INFO_UPDATE_CHECKING;
            }
            break;
        case FIRMWARE_UPDATE_STATE_DOWNLOADING:
        case FIRMWARE_UPDATE_STATE_VERIFYING:
            if (ctx->background_ota_check_feedback) {
                view_state = SCREEN_DEVICE_INFO_UPDATE_CHECKING;
            }
            break;
        case FIRMWARE_UPDATE_STATE_UP_TO_DATE:
            if (show_manual_ota_state && !ota_result_dismissed) {
                view_state = SCREEN_DEVICE_INFO_UPDATE_UP_TO_DATE;
            }
            break;
        case FIRMWARE_UPDATE_STATE_AVAILABLE:
            if (show_manual_ota_state ||
                firmware_auto_check_has_wifi_candidate()) {
                view_state = SCREEN_DEVICE_INFO_UPDATE_AVAILABLE;
            }
            break;
        case FIRMWARE_UPDATE_STATE_STAGED:
            ctx->manual_ota_failed_candidate_visible = false;
            ctx->manual_ota_failed_candidate_text[0] = '\0';
            if (show_manual_ota_state && update_status.last_error != ESP_OK &&
                !ota_result_dismissed) {
                view_state = SCREEN_DEVICE_INFO_UPDATE_CHECK_FAILED;
            }
            else {
                view_state = SCREEN_DEVICE_INFO_UPDATE_STAGED;
            }
            break;
        case FIRMWARE_UPDATE_STATE_FAILED:
            if (ctx->manual_ota_failed_candidate_visible) {
                view_state = SCREEN_DEVICE_INFO_UPDATE_AVAILABLE;
            }
            else if (show_manual_ota_state && !ota_result_dismissed) {
                view_state = SCREEN_DEVICE_INFO_UPDATE_CHECK_FAILED;
            }
            break;
        default:
            break;
        }
        char available_version[64] = {0};
        if (view_state == SCREEN_DEVICE_INFO_UPDATE_AVAILABLE) {
            if (update_status.state == FIRMWARE_UPDATE_STATE_FAILED &&
                ctx->manual_ota_failed_candidate_visible) {
                strlcpy(available_version,
                        ctx->manual_ota_failed_candidate_text,
                        sizeof(available_version));
            }
            else {
                screen_get_available_firmware_version(&update_status,
                                                      available_version,
                                                      sizeof(available_version));
            }
        }
        else if (view_state == SCREEN_DEVICE_INFO_UPDATE_STAGED) {
            screen_get_staged_firmware_version(available_version,
                                               sizeof(available_version));
        }
        screen_device_info_set_update_state(
            ctx->device_info,
            view_state,
            available_version[0] ? available_version : update_status.target_version);
        screen_update_manual_ota(ctx, &update_status);
    }
}

static void screen_device_info_refresh_timer_cb(lv_timer_t *timer)
{
    screen_ctx_t *ctx = lv_timer_get_user_data(timer);
    screen_update_device_info(ctx);
}

static void screen_device_info_check_update_cb(screen_device_info_t *view,
                                               void *user_data)
{
    screen_ctx_t *ctx = user_data;
    firmware_update_status_t status = {0};
    if (ctx && firmware_update_get_status(&status) == ESP_OK &&
        (status.state == FIRMWARE_UPDATE_STATE_DOWNLOADING ||
         status.state == FIRMWARE_UPDATE_STATE_VERIFYING)) {
        ctx->background_ota_check_feedback = true;
        screen_device_info_set_checking(view, true);
        ESP_LOGI(TAG,
                 "Firmware is already downloading in background; showing checking feedback only");
        return;
    }

    esp_err_t err = firmware_update_check_async();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Cannot start firmware update check: %s", esp_err_to_name(err));
        return;
    }

    s_device_info_manual_ota_check_active = true;
    s_device_info_manual_ota_result_visible = false;
    s_device_info_ota_result_dismissed = false;
    if (ctx) {
        ctx->manual_ota_failed_candidate_visible = false;
        ctx->manual_ota_failed_candidate_text[0] = '\0';
    }
    ESP_LOGI(TAG, "Firmware update check requested from device information page");
    screen_device_info_set_checking(view, true);
}

static void screen_device_info_upgrade_cb(screen_device_info_t *view,
                                          void *user_data)
{
    (void)view;
    screen_ctx_t *ctx = user_data;
    if (!ctx || !ctx->ota_update) {
        return;
    }

    firmware_update_status_t status = {0};
    bool status_valid = firmware_update_get_status(&status) == ESP_OK;
    bool staged = status_valid && status.state == FIRMWARE_UPDATE_STATE_STAGED;
    bool retry_failed_download = status_valid &&
                                 status.state == FIRMWARE_UPDATE_STATE_FAILED &&
                                 ctx->manual_ota_failed_candidate_visible;
    if (status_valid && status.state == FIRMWARE_UPDATE_STATE_AVAILABLE) {
        screen_get_available_firmware_version(
            &status,
            ctx->manual_ota_failed_candidate_text,
            sizeof(ctx->manual_ota_failed_candidate_text));
        ctx->manual_ota_failed_candidate_visible = false;
    }

    app_set_manual_ota_web_lock(true);
    ctx->manual_ota_active = true;
    ctx->manual_ota_retry_check = retry_failed_download;
    ctx->manual_ota_activation_requested = staged;
    ctx->manual_ota_reboot_requested = false;
    ctx->manual_ota_retry_activation = false;
    ctx->manual_ota_failure_visible = false;
    screen_ota_update_show_updating(ctx->ota_update, 0);

    ctx->manual_ota_start_timer = lv_timer_create(screen_manual_ota_start_timer_cb,
                                                   SCREEN_MANUAL_OTA_START_DELAY_MS,
                                                   ctx);
    if (ctx->manual_ota_start_timer) {
        lv_timer_set_repeat_count(ctx->manual_ota_start_timer, 1);
        lv_timer_set_auto_delete(ctx->manual_ota_start_timer, true);
        return;
    }

    ESP_LOGW(TAG, "Cannot defer OTA installation; starting immediately");
    screen_manual_ota_start_install(ctx);
}

static void screen_device_info_hidden_cb(screen_device_info_t *view,
                                         void *user_data)
{
    screen_ctx_t *ctx = user_data;
    if (!ctx || view != ctx->device_info || ctx != s_screen_ctx) {
        return;
    }

    firmware_update_status_t status = {0};
    if (firmware_update_get_status(&status) == ESP_OK &&
        (status.state == FIRMWARE_UPDATE_STATE_UP_TO_DATE ||
         status.state == FIRMWARE_UPDATE_STATE_FAILED ||
         (status.state == FIRMWARE_UPDATE_STATE_STAGED &&
          status.last_error != ESP_OK))) {
        s_device_info_ota_result_dismissed = true;
        s_device_info_ota_result_revision = status.revision;
        screen_device_info_set_update_state(view,
                                            SCREEN_DEVICE_INFO_UPDATE_IDLE,
                                            NULL);
        ESP_LOGI(TAG,
                 "Device information OTA result dismissed: state=%s revision=%" PRIu32,
                 firmware_update_state_to_string(status.state),
                 status.revision);
    }
}

static void screen_manual_ota_start_timer_cb(lv_timer_t *timer)
{
    screen_ctx_t *ctx = lv_timer_get_user_data(timer);
    if (!ctx || ctx->manual_ota_start_timer != timer) {
        return;
    }

    ctx->manual_ota_start_timer = NULL;
    screen_manual_ota_start_install(ctx);
}

static void screen_manual_ota_start_install(screen_ctx_t *ctx)
{
    if (!ctx || !ctx->manual_ota_active) {
        return;
    }

    if (ctx->manual_ota_retry_check) {
        esp_err_t check_err = firmware_update_check_async();
        if (check_err != ESP_OK) {
            ctx->manual_ota_retry_check = false;
            screen_manual_ota_show_failure(ctx, check_err, false);
        }
        else {
            ESP_LOGI(TAG, "Rechecking firmware before retrying failed download");
        }
        return;
    }

    firmware_update_status_t status = {0};
    bool staged = firmware_update_get_status(&status) == ESP_OK &&
                  status.state == FIRMWARE_UPDATE_STATE_STAGED;
    if (staged) {
        ctx->manual_ota_activation_requested = true;
        esp_err_t activate_err = firmware_update_activate_staged();
        if (activate_err != ESP_OK) {
            ctx->manual_ota_activation_requested = false;
            screen_manual_ota_show_failure(ctx, activate_err, true);
            return;
        }
        ESP_LOGI(TAG, "Previously staged firmware activation requested");
        return;
    }

    esp_err_t err = firmware_update_install_async();
    if (err != ESP_OK) {
        screen_manual_ota_show_failure(ctx, err, false);
        return;
    }

    ESP_LOGI(TAG, "Manual firmware installation requested");
}

static void screen_manual_ota_retry_cb(screen_ota_update_t *view,
                                       void *user_data)
{
    screen_ctx_t *ctx = user_data;
    if (!ctx || view != ctx->ota_update || !ctx->manual_ota_active) {
        return;
    }

    app_set_manual_ota_web_lock(true);
    screen_ota_update_show_updating(view, 0);
    ctx->manual_ota_failure_visible = false;
    ctx->manual_ota_reboot_requested = false;

    esp_err_t err;
    if (ctx->manual_ota_retry_activation) {
        ctx->manual_ota_activation_requested = true;
        ctx->manual_ota_retry_activation = false;
        err = firmware_update_activate_staged();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Manual staged firmware activation retry requested");
            return;
        }
        ctx->manual_ota_activation_requested = false;
        screen_manual_ota_show_failure(ctx, err, true);
        return;
    }

    ctx->manual_ota_retry_check = true;
    ctx->manual_ota_activation_requested = false;
    err = firmware_update_check_async();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Manual firmware update retry check requested");
        return;
    }

    ctx->manual_ota_retry_check = false;
    screen_manual_ota_show_failure(ctx, err, false);
}

static void screen_manual_ota_cancel_cb(screen_ota_update_t *view,
                                        void *user_data)
{
    screen_ctx_t *ctx = user_data;
    if (!ctx || view != ctx->ota_update) {
        return;
    }

    ctx->manual_ota_active = false;
    ctx->manual_ota_retry_check = false;
    ctx->manual_ota_activation_requested = false;
    ctx->manual_ota_reboot_requested = false;
    ctx->manual_ota_retry_activation = false;
    ctx->manual_ota_failure_visible = false;
    app_set_manual_ota_web_lock(false);
    screen_ota_update_hide(view);
    ESP_LOGI(TAG, "Manual firmware update result dismissed");
}

static void screen_update_manual_ota(screen_ctx_t *ctx,
                                     const firmware_update_status_t *status)
{
    if (!ctx || !ctx->ota_update || !status || !ctx->manual_ota_active) {
        return;
    }

    switch (status->state) {
    case FIRMWARE_UPDATE_STATE_CHECKING:
        screen_ota_update_show_updating(ctx->ota_update, 0);
        break;

    case FIRMWARE_UPDATE_STATE_AVAILABLE:
        if (ctx->manual_ota_retry_check) {
            ctx->manual_ota_retry_check = false;
            esp_err_t err = firmware_update_install_async();
            if (err != ESP_OK) {
                screen_manual_ota_show_failure(ctx, err, false);
            }
            else {
                ESP_LOGI(TAG, "Manual firmware installation retry requested");
            }
        }
        break;

    case FIRMWARE_UPDATE_STATE_DOWNLOADING:
    case FIRMWARE_UPDATE_STATE_VERIFYING:
        screen_ota_update_show_updating(ctx->ota_update, status->progress);
        break;

    case FIRMWARE_UPDATE_STATE_STAGED:
        screen_ota_update_show_updating(ctx->ota_update, 100);
        if (status->last_error != ESP_OK) {
            ctx->manual_ota_activation_requested = false;
            screen_manual_ota_show_failure(ctx, status->last_error, true);
        }
        else if (!ctx->manual_ota_activation_requested) {
            ctx->manual_ota_activation_requested = true;
            esp_err_t err = firmware_update_activate_staged();
            if (err != ESP_OK) {
                ctx->manual_ota_activation_requested = false;
                screen_manual_ota_show_failure(ctx, err, true);
            }
            else {
                ESP_LOGI(TAG, "Downloaded firmware staged; activation requested");
            }
        }
        break;

    case FIRMWARE_UPDATE_STATE_ACTIVATING:
        screen_ota_update_show_updating(ctx->ota_update, 100);
        break;

    case FIRMWARE_UPDATE_STATE_READY_TO_REBOOT:
        screen_ota_update_show_restarting(ctx->ota_update);
        if (!ctx->manual_ota_reboot_requested) {
            ctx->manual_ota_reboot_requested = true;
            esp_err_t err = firmware_update_reboot();
            if (err != ESP_OK) {
                ctx->manual_ota_reboot_requested = false;
                screen_manual_ota_show_failure(ctx, err, false);
            }
            else {
                ESP_LOGI(TAG, "Firmware activated; reboot requested");
            }
        }
        break;

    case FIRMWARE_UPDATE_STATE_REBOOTING:
        screen_ota_update_show_restarting(ctx->ota_update);
        break;

    case FIRMWARE_UPDATE_STATE_FAILED:
        ctx->manual_ota_retry_check = false;
        ctx->manual_ota_activation_requested = false;
        screen_manual_ota_show_failure(ctx, status->last_error, false);
        break;

    case FIRMWARE_UPDATE_STATE_UP_TO_DATE:
        if (ctx->manual_ota_retry_check) {
            ctx->manual_ota_retry_check = false;
            screen_manual_ota_show_failure(ctx, ESP_ERR_NOT_FOUND, false);
        }
        break;

    case FIRMWARE_UPDATE_STATE_IDLE:
    case FIRMWARE_UPDATE_STATE_BOOT_VERIFYING:
    case FIRMWARE_UPDATE_STATE_SUCCESS:
    case FIRMWARE_UPDATE_STATE_ROLLED_BACK:
    default:
        break;
    }
}

static void screen_manual_ota_show_failure(screen_ctx_t *ctx,
                                           esp_err_t error,
                                           bool retry_activation)
{
    if (!ctx || !ctx->ota_update) {
        return;
    }
    app_set_manual_ota_web_lock(false);
    if (!retry_activation) {
        firmware_update_status_t status = {0};
        bool failed_download = firmware_update_get_status(&status) == ESP_OK &&
                               status.state == FIRMWARE_UPDATE_STATE_FAILED &&
                               status.target_version[0] != '\0';
        ctx->manual_ota_failed_candidate_visible = failed_download;
        if (failed_download && !ctx->manual_ota_failed_candidate_text[0]) {
            screen_format_target_firmware_version(
                status.target_version,
                NULL,
                ctx->manual_ota_failed_candidate_text,
                sizeof(ctx->manual_ota_failed_candidate_text));
        }
        else if (!failed_download) {
            ctx->manual_ota_failed_candidate_text[0] = '\0';
        }
    }
    bool already_visible = ctx->manual_ota_failure_visible &&
                           ctx->manual_ota_retry_activation == retry_activation;
    ctx->manual_ota_retry_activation = retry_activation;
    ctx->manual_ota_failure_visible = true;
    screen_ota_update_show_failed(ctx->ota_update);
    if (!already_visible) {
        ESP_LOGE(TAG,
                 "Manual firmware update failed: %s (retry=%s)",
                 esp_err_to_name(error),
                 retry_activation ? "activate" : "check-and-download");
    }
}

static bool screen_get_netif_ipv4(const char *if_key,
                                  char *output,
                                  size_t output_size)
{
    if (!if_key || !output || output_size == 0) {
        return false;
    }

    output[0] = '\0';
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(if_key);
    if (!netif) {
        return false;
    }

    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
        return false;
    }

    snprintf(output, output_size, IPSTR, IP2STR(&ip_info.ip));
    return true;
}

static void screen_get_firmware_version(char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }

    const esp_app_desc_t *description = esp_app_get_description();
    if (!description) {
        strlcpy(output, "--", output_size);
        return;
    }

    char month_name[4] = {0};
    int day = 0;
    int year = 0;
    int month = 0;
    if (sscanf(description->date, "%3s %d %d", month_name, &day, &year) == 3) {
        month = screen_month_number(month_name);
    }

    if (month > 0) {
        snprintf(output,
                 output_size,
                 "%s%s(%04d%02d%02d)",
                 description->version[0] == 'v' ? "" : "v",
                 description->version,
                 year,
                 month,
                 day);
    }
    else {
        snprintf(output,
                 output_size,
                 "%s%s",
                 description->version[0] == 'v' ? "" : "v",
                 description->version);
    }
}

static void screen_get_available_firmware_version(const firmware_update_status_t *status,
                                                  char *output,
                                                  size_t output_size)
{
    if (!status || !output || output_size == 0) {
        return;
    }
    output[0] = '\0';
    if (!status->target_version[0]) {
        return;
    }

    firmware_manifest_t manifest = {0};
    const char *published_at = NULL;
    if (firmware_update_get_available_manifest(&manifest) == ESP_OK) {
        published_at = manifest.published_at;
    }
    screen_format_target_firmware_version(status->target_version,
                                          published_at,
                                          output,
                                          output_size);
}

static void screen_get_staged_firmware_version(char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }
    output[0] = '\0';

    firmware_update_staged_info_t staged = {0};
    if (firmware_update_get_staged_info(&staged) != ESP_OK) {
        return;
    }
    screen_format_target_firmware_version(staged.version,
                                          staged.published_at,
                                          output,
                                          output_size);
}

static void screen_format_target_firmware_version(const char *version,
                                                  const char *published_at,
                                                  char *output,
                                                  size_t output_size)
{
    if (!version || !version[0] || !output || output_size == 0) {
        return;
    }

    const char *prefix = version[0] == 'v' ? "" : "v";
    if (published_at && strlen(published_at) >= 10) {
        snprintf(output,
                 output_size,
                 "%s%s(%.4s%.2s%.2s)",
                 prefix,
                 version,
                 published_at,
                 published_at + 5,
                 published_at + 8);
        return;
    }
    snprintf(output, output_size, "%s%s", prefix, version);
}

static int screen_month_number(const char *month_name)
{
    static const char *const months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };

    if (!month_name) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(months) / sizeof(months[0]); i++) {
        if (strcmp(month_name, months[i]) == 0) {
            return (int)i + 1;
        }
    }
    return 0;
}

static void screen_device_info_open_gesture_cb(lv_event_t *event)
{
    screen_ctx_t *ctx = lv_event_get_user_data(event);
    lv_indev_t *indev = lv_indev_active();
    if (!ctx || !ctx->device_info || !indev || !screen_device_info_is_available()) {
        return;
    }

    if (lv_indev_get_gesture_dir(indev) == LV_DIR_BOTTOM) {
        ESP_LOGI(TAG, "Opening device information page by swipe-down gesture");
        screen_ui_show_device_info();
    }
}

static void screen_device_info_close_gesture_cb(lv_event_t *event)
{
    screen_ctx_t *ctx = lv_event_get_user_data(event);
    lv_indev_t *indev = lv_indev_active();
    if (!ctx || !ctx->device_info || !indev) {
        return;
    }

    if (lv_indev_get_gesture_dir(indev) == LV_DIR_TOP) {
        ESP_LOGI(TAG, "Closing device information page by swipe-up gesture");
        screen_ui_hide_device_info();
    }
}

static void screen_create_status_bar(screen_ctx_t *ctx, lv_obj_t *screen)
{
    ctx->status_bar = system_status_bar_create(screen);
}

static void screen_create_pages(screen_ctx_t *ctx, lv_obj_t *screen, const screen_config_t *config)
{
    ctx->tileview = lv_tileview_create(screen);
    lv_obj_set_size(ctx->tileview, LV_PCT(100), LV_VER_RES - SYSTEM_STATUS_BAR_HEIGHT);
    lv_obj_align(ctx->tileview, LV_ALIGN_TOP_MID, 0, SYSTEM_STATUS_BAR_HEIGHT);
    lv_obj_set_style_bg_opa(ctx->tileview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->tileview, 0, 0);
    lv_obj_set_style_pad_all(ctx->tileview, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(ctx->tileview, screen_tileview_event_cb, LV_EVENT_VALUE_CHANGED, ctx);

    ctx->tiles = lv_malloc(sizeof(lv_obj_t *) * config->screen_count);
    ctx->main_views = lv_malloc(sizeof(main_view_t *) * config->screen_count);
    if (!ctx->tiles || !ctx->main_views) {
        lv_free(ctx->tiles);
        lv_free(ctx->main_views);
        ctx->tiles = NULL;
        ctx->main_views = NULL;
        return;
    }
    memset(ctx->tiles, 0, sizeof(lv_obj_t *) * config->screen_count);
    memset(ctx->main_views, 0, sizeof(main_view_t *) * config->screen_count);

    for (size_t i = 0; i < config->screen_count; i++) {
        lv_obj_t *tile = lv_tileview_add_tile(ctx->tileview, (uint8_t)i, 0, screen_page_scroll_dirs(i, config->screen_count));
        ctx->tiles[i] = tile;
        lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tile, 0, 0);
        lv_obj_set_style_pad_all(tile, 0, 0);
        const screen_view_t *view = &config->screens[i];
        if (screen_view_has_badges_or_entities(view)) {
            ctx->main_views[i] = main_view_create(tile, view);
        }
        else {
            screen_create_default_content(tile, -(SYSTEM_STATUS_BAR_HEIGHT / 2));
        }
    }

    /* A configuration reload releases every previous view before rebuilding
     * it. Keep cached snapshots through that handoff so unchanged cards do
     * not fall back to their entity IDs, then discard entities absent from the
     * newly created views and rebuild the filtered HA subscription once. */
    ha_entity_store_t *store = app_get_ha_entity_store();
    if (store) {
        ha_entity_store_prune_unreferenced(store);
        esp_err_t err = ha_entity_store_sync(store);
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "Failed to synchronize entities after screen configuration reload: %s",
                     esp_err_to_name(err));
        }
    }
}

static void screen_create_indicator(screen_ctx_t *ctx, lv_obj_t *screen)
{
    if (!ctx || !screen || ctx->page_count < 2) {
        return;
    }

    ctx->screen_indicator = screen_indicator_create(screen, ctx->page_count);
    if (!ctx->screen_indicator) {
        ESP_LOGW(TAG, "Failed to create screen indicator");
    }
}

static void screen_create_default_content(lv_obj_t *parent, lv_coord_t y_offset)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_remove_style_all(container);
    lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(container, SCREEN_DEFAULT_CONTENT_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_pad_gap(container, 8, 0);

    lv_obj_t *image = lv_img_create(container);
    lv_img_set_src(image, &no_entity_160);

    lv_obj_t *label = lv_label_create(container);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_text(label, "No content. Add entities in Web Console > Screen Management");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &ha_font_misan_regular_16, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_align(container, LV_ALIGN_CENTER, 0, y_offset);
}

static bool screen_view_has_badges_or_entities(const screen_view_t *view)
{
    if (!view) {
        return false;
    }

    for (size_t i = 0; view->badges && i < view->badge_count; i++) {
        if (view->badges[i].type == SCREEN_BADGE_TYPE_ENTITY &&
            view->badges[i].has_data &&
            badge_display_supported(view->badges[i].data.entity)) {
            return true;
        }
    }

    for (size_t i = 0; view->cards && i < view->card_count; i++) {
        const screen_card_config_t *card = &view->cards[i];
        if (card->type == SCREEN_CARD_TYPE_TILE &&
            card->data.tile.entity &&
            card->data.tile.entity[0] != '\0') {
            return true;
        }
    }

    return false;
}
static lv_dir_t screen_page_scroll_dirs(size_t index, size_t count)
{
    lv_dir_t dir = LV_DIR_NONE;
    if (index > 0) {
        dir = (lv_dir_t)(dir | LV_DIR_LEFT);
    }
    if (index + 1 < count) {
        dir = (lv_dir_t)(dir | LV_DIR_RIGHT);
    }

    return dir;
}

static void screen_tileview_event_cb(lv_event_t *event)
{
    screen_ctx_t *ctx = lv_event_get_user_data(event);
    lv_obj_t *active_tile = lv_tileview_get_tile_active(lv_event_get_target_obj(event));
    if (!ctx || !active_tile || !ctx->tiles) {
        return;
    }

    for (size_t i = 0; i < ctx->page_count; i++) {
        if (ctx->tiles[i] == active_tile) {
            screen_update_active_page(ctx, i);
            return;
        }
    }
}

static void screen_update_active_page(screen_ctx_t *ctx, size_t active_page)
{
    if (!ctx || !ctx->config || active_page >= ctx->page_count) {
        return;
    }

    ctx->active_page = active_page;
    screen_indicator_set_active(ctx->screen_indicator, active_page);
    const screen_view_t *view = &ctx->config->screens[active_page];
    screen_ui_set_status_title(view->title ? view->title : "");
}

static bool screen_find_page_position(const screen_ctx_t *ctx, int32_t screen_index, size_t *position)
{
    if (!ctx || !ctx->config || !ctx->config->screens || !position) {
        return false;
    }

    for (size_t i = 0; i < ctx->page_count; i++) {
        if (ctx->config->screens[i].index == screen_index) {
            *position = i;
            return true;
        }
    }

    return false;
}
