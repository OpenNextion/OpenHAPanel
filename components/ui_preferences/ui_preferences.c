#include "ui_preferences.h"

#include <stdint.h>

#include "nvs_storage.h"

#define UI_PREFERENCES_NVS_NAMESPACE        "ui_prefs"
#define UI_PREFERENCES_HA_SWIPE_HINT_KEY    "ha_swipe_seen"

esp_err_t ui_preferences_get_ha_swipe_hint_seen(bool *seen)
{
    if (!seen) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t value = 0;
    esp_err_t err = nvs_storage_get_u8(UI_PREFERENCES_NVS_NAMESPACE,
                                       UI_PREFERENCES_HA_SWIPE_HINT_KEY,
                                       &value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *seen = false;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    *seen = value != 0;
    return ESP_OK;
}

esp_err_t ui_preferences_set_ha_swipe_hint_seen(void)
{
    return nvs_storage_set_u8(UI_PREFERENCES_NVS_NAMESPACE,
                              UI_PREFERENCES_HA_SWIPE_HINT_KEY,
                              1);
}

esp_err_t ui_preferences_prepare_factory_reset(void)
{
    esp_err_t err = nvs_storage_erase_namespace(UI_PREFERENCES_NVS_NAMESPACE);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}
