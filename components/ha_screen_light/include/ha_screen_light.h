#pragma once

#include <stdbool.h>
#include "ha_ws_types.h"

/* Called after the application WebSocket authenticates. Safe to call again on reconnect. */
void ha_screen_light_start(ha_ws_client_t *client);

/* Stop the worker before the application destroys its HA WebSocket client. */
bool ha_screen_light_stop(void);
