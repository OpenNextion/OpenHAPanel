#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifndef OPEN_NEXTION_COMPONENTS_NIGHT_MODE_INCLUDE_NIGHT_MODE_H
#define OPEN_NEXTION_COMPONENTS_NIGHT_MODE_INCLUDE_NIGHT_MODE_H

bool night_mode_notify_manual_brightness(void);
uint8_t night_mode_get_effective_brightness(uint8_t manual_brightness);
void night_mode_init(void);
esp_err_t night_mode_api_set_config(bool enable, uint8_t brightness,
                                    const char *from, const char *to);


#endif 

#ifdef __cplusplus
}
#endif
