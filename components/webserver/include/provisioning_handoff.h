#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_PROVISIONING_HANDOFF_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_PROVISIONING_HANDOFF_H

#include <stddef.h>
#include <esp_err.h>
#include "webserver_method_api.h"

#define PROVISIONING_HANDOFF_TICKET_HEX_LEN 64

/** Arm a single AP-to-STA handoff after the device password is initialized. */
void provisioning_handoff_arm_from_initial_password(int socket_fd);

/** Invalidate any armed or issued handoff, for example after a password change. */
void provisioning_handoff_invalidate(void);

/** Issue the armed handoff to the same AP peer after STA obtains an address. */
esp_err_t provisioning_handoff_issue(int socket_fd,
                                     char ticket_out[PROVISIONING_HANDOFF_TICKET_HEX_LEN + 1]);

/** Redeem a short-lived handoff ticket on an unauthenticated STA connection. */
esp_err_t method_api_handle_exchange_provisioning_handoff(cJSON *data,
                                                           async_resp_arg *now_resp_arg);

#endif
