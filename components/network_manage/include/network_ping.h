#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_PING_H
#define OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_PING_H

/**
 * @brief Stop the ongoing network ping task.
 *
 * Stops the ping task if running by stopping and deleting the ping session,
 * resetting counters, and marking the task as stopped.
 */
void network_stop_ping_task(void);

/**
 * @brief Start the network ping task.
 *
 * Initializes and starts a continuous ping to 8.8.8.8 with specified config
 * and callback handlers.
 */
void network_start_ping_task(void);

#endif // OPEN_NEXTION_COMPONENTS_NETWORK_MANAGE_INCLUDE_NETWORK_PING_H

#ifdef __cplusplus
}
#endif
