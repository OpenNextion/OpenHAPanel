#include "entity_state_translation.h"
#include "i18n.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *device_class;
    i18n_key_t on_key;
    i18n_key_t off_key;
} binary_state_t;

static const binary_state_t s_binary_states[] = {
    {"battery", I18N_STATE_LOW, I18N_STATE_NORMAL},
    {"battery_charging", I18N_STATE_CHARGING, I18N_STATE_NOT_CHARGING},
    {"carbon_monoxide", I18N_STATE_DETECTED, I18N_STATE_CLEAR},
    {"cold", I18N_STATE_COLD, I18N_STATE_NORMAL},
    {"connectivity", I18N_STATE_CONNECTED, I18N_STATE_DISCONNECTED},
    {"door", I18N_STATE_OPEN, I18N_STATE_CLOSED},
    {"garage_door", I18N_STATE_OPEN, I18N_STATE_CLOSED},
    {"gas", I18N_STATE_DETECTED, I18N_STATE_CLEAR},
    {"glass_break", I18N_STATE_GLASS_BREAK, I18N_STATE_CLEAR},
    {"heat", I18N_STATE_HOT, I18N_STATE_NORMAL},
    {"light", I18N_STATE_LIGHT_DETECTED, I18N_STATE_NO_LIGHT},
    {"lock", I18N_STATE_UNLOCKED, I18N_STATE_LOCKED},
    {"moisture", I18N_STATE_WET, I18N_STATE_DRY},
    {"motion", I18N_STATE_DETECTED, I18N_STATE_CLEAR},
    {"moving", I18N_STATE_MOVING, I18N_STATE_NOT_MOVING},
    {"occupancy", I18N_STATE_DETECTED, I18N_STATE_CLEAR},
    {"opening", I18N_STATE_OPEN, I18N_STATE_CLOSED},
    {"plug", I18N_STATE_PLUGGED_IN, I18N_STATE_UNPLUGGED},
    {"power", I18N_STATE_ON, I18N_STATE_OFF},
    {"presence", I18N_STATE_HOME, I18N_STATE_AWAY},
    {"problem", I18N_STATE_PROBLEM, I18N_STATE_OK},
    {"running", I18N_STATE_RUNNING, I18N_STATE_NOT_RUNNING},
    {"safety", I18N_STATE_UNSAFE, I18N_STATE_SAFE},
    {"smoke", I18N_STATE_DETECTED, I18N_STATE_CLEAR},
    {"sound", I18N_STATE_DETECTED, I18N_STATE_CLEAR},
    {"tamper", I18N_STATE_TAMPERING, I18N_STATE_CLEAR},
    {"update", I18N_STATE_UPDATE_AVAILABLE, I18N_STATE_UP_TO_DATE},
    {"vibration", I18N_STATE_DETECTED, I18N_STATE_CLEAR},
    {"window", I18N_STATE_OPEN, I18N_STATE_CLOSED},
};

const char *entity_state_translate(const char *state)
{
    if (!state) return "";
    static const struct { const char *raw; i18n_key_t key; } states[] = {
        {"on", I18N_STATE_ON}, {"off", I18N_STATE_OFF},
        {"unknown", I18N_STATE_UNKNOWN}, {"unavailable", I18N_STATE_UNAVAILABLE},
        {"open", I18N_STATE_OPEN}, {"closed", I18N_STATE_CLOSED},
        {"opening", I18N_STATE_OPENING}, {"closing", I18N_STATE_CLOSING},
        {"locked", I18N_STATE_LOCKED}, {"unlocked", I18N_STATE_UNLOCKED},
        
    };
    for (size_t i = 0; i < sizeof(states) / sizeof(states[0]); ++i) {
        if (strcmp(state, states[i].raw) == 0) return i18n_get(states[i].key);
    }
    return state;
}

const char *entity_state_translate_binary(const char *state, const char *device_class)
{
    if (!state) return "";
    bool on = strcmp(state, "on") == 0;
    if (!on && strcmp(state, "off") != 0) return entity_state_translate(state);
    if (device_class) {
        for (size_t i = 0; i < sizeof(s_binary_states) / sizeof(s_binary_states[0]); ++i) {
            if (strcmp(device_class, s_binary_states[i].device_class) == 0)
                return i18n_get(on ? s_binary_states[i].on_key : s_binary_states[i].off_key);
        }
    }
    return i18n_get(on ? I18N_STATE_ON : I18N_STATE_OFF);
}

const char *entity_state_translate_climate_option(unsigned selector, const char *raw)
{
    static const char *const scopes[] = {"hvac", "preset", "fan", "swing", "horizontal"};
    if (!raw) return "";
    if (selector >= sizeof(scopes) / sizeof(scopes[0])) return raw;
    char key[128];
    int length = snprintf(key, sizeof(key), "option.%s.%s", scopes[selector], raw);
    const char *text = length > 0 && (size_t)length < sizeof(key) ? i18n_find(key) : NULL;
    return text ? text : raw;
}
