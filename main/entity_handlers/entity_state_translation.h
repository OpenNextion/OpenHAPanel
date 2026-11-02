#pragma once

/* Presentation only. Unknown/custom values remain byte-for-byte unchanged. */
const char *entity_state_translate(const char *state);
const char *entity_state_translate_binary(const char *state, const char *device_class);
const char *entity_state_translate_climate_option(unsigned selector, const char *raw);
