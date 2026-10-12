#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_H
#define OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_H

#include "webserver_client_node.h"
#include "webserver_api.h"

#define MAX_WEBSOCKET_CLIENT (4)                                             /* number of client-side landing pages */
#define WEBSOCKET_CLIENT_LOGIN_TIP (1)                                       /* client login number prompt page */
#define WEB_UI_HTTP_GET (1)                                                  /* the web page requests a connection */

/* httpd session-table capacity, NOT the console limit (that is MAX_WEBSOCKET_CLIENT).
 * A console occupies up to two sessions at peak: the keep-alive HTTP connection that
 * fetched the SPA plus its WebSocket. One extra slot absorbs the over-limit console
 * while its "limit reached" notice is delivered, and one slot of slack covers a
 * browser prefetch racing the page load.
 * Must satisfy MAX_OPEN_SOCKETS + 3 <= CONFIG_LWIP_MAX_SOCKETS (see httpd_main.c). */
#define MAX_OPEN_SOCKETS (MAX_WEBSOCKET_CLIENT * 2 + WEBSOCKET_CLIENT_LOGIN_TIP + 1) /* total number of connections */

/* event declare */
ESP_EVENT_DECLARE_BASE(WEB_SERVER_EVENT);

/* event */
enum
{
    WEB_SERVER_EVENT_CONNECT,           /* connection */
    WEB_SERVER_EVENT_RECEIVE,           /* data received */
    WEB_SERVER_EVENT_DISCONNECT,        /* disconnected */
    WEB_SERVER_EVENT_AUTH_OK,           /* certification successful */
    WEB_SERVER_EVENT_SKIP_AUTH,         /* no certification is required */
    WEB_SERVER_EVENT_MAX
};

/**
 * @brief Initialize and start the web server.
 *
 * This function sets up necessary event handlers, creates synchronization primitives,
 * initializes client lists, creates message queues and associated tasks,
 * starts the event loop for web server events, and finally creates the web server instance.
 *
 * It must be called once during system startup to launch the web server.
 */
void webserver_init(void);

#endif // OPEN_NEXTION_COMPONENTS_WEBSERVER_INCLUDE_WEBSERVER_H
#ifdef __cplusplus
}
#endif
