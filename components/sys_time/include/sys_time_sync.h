#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_SYNC_H
#define OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_SYNC_H

#include "webserver_method_api.h"

/**
 * @brief Get the system time synchronization status.
 *
 * Returns the status of whether the system time has been successfully
 * synchronized. Useful for modules that depend on accurate time to determine
 * whether it's safe to proceed with time-based operations.
 *
 * @return true if the system time has been synchronized successfully.
 * @return false if the system time has not yet been synchronized.
 */
bool sys_time_get_sync_success_flag(void);

/**
 * @brief Starts a task to query timezone and UTC time information from a remote server.
 *
 * This function checks for network connectivity and ensures no duplicate query task is running.
 * It then launches a FreeRTOS task to perform the query.
 *
 * @param[in] timezone Null-terminated timezone string (e.g., "Asia/Shanghai").
 * @param[in] seq      Sequence ID for response tracking.
 *
 * @return
 *         -  0 on success (task created).
 *         -  905 if a query task is already running.
 *         -  906 if no network is available.
 *         - -1 on memory allocation or task creation failure.
 */
int sys_time_timezone_query_task_start(char *timezone, uint64_t seq);

/**
 * @brief Launches the network DST info synchronization task.
 *
 * Ensures that only one instance of the DST synchronization task runs at a time.
 *
 * @return
 *         - 0 on successful task creation.
 *         - -1 if the task is already running or creation failed.
 */
int sync_dst_info_from_network_task_start(void);

#endif // OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_SYNC_H

#ifdef __cplusplus
}
#endif
