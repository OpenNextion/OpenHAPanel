#ifdef __cplusplus
extern "C" {
#endif

#ifndef OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_API_H
#define OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_API_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "esp_event.h"

#define SYSTIME_ERR_TIMEZONE_CHECKING (905)
#define SYSTIME_ERR_NOT_NETWORK (906)

#define TIMEZONE_MAX_LEN 64

/** System time and timezone synchronization event base. */
ESP_EVENT_DECLARE_BASE(SYS_TIME_EVENT);

enum
{
    /** Time and timezone information have been synchronized successfully. */
    SYS_TIME_EVENT_SYNCED,
};

typedef struct
{
    char timezone[TIMEZONE_MAX_LEN];
    uint64_t seq;
} timezone_query_info_t;

typedef struct
{
    bool sync_time;
    char timezone[TIMEZONE_MAX_LEN];
    int32_t utc_office;
    bool is_dst;
    bool has_dst;
    uint64_t next_dst;
    int32_t next_utc_office;
} timezone_info_t;

/**
 * @brief Gets the current Daylight Saving Time (DST) synchronization status.
 *
 * This function returns whether the system time has been synchronized with DST
 * information from the timezone server.
 *
 * @return
 *         - true if the system time is synchronized with DST info.
 *         - false otherwise.
 */
bool sys_time_get_dst_sync_status(void);

/**
 * @brief Gets the current timezone information.
 *
 * Copies the current timezone information into the provided structure.
 *
 * @param[out] info Pointer to a `timezone_info_t` structure to receive the data.
 *
 * @return Returns 0 on success.
 */
int sys_time_get_time_info(timezone_info_t *info);

/**
 * @brief Retrieves the system run time in milliseconds.
 *
 * Returns the elapsed time since the system start time was recorded. If the start time
 * is not set, it returns the current ESP log timestamp as a fallback.
 *
 * @return System run time in milliseconds.
 */
uint64_t sys_time_get_system_run_time(void);

/**
 * @brief Get the current system UTC timestamp.
 *
 * Returns the current time as the number of seconds elapsed since the Unix epoch (1970-01-01 00:00:00 UTC).
 *
 * @return Current UTC time as a 32-bit unsigned integer.
 */
uint32_t sys_time_get_sys_utc_time_stamp(void);

/**
 * @brief Get the current local time adjusted for timezone and DST.
 *
 * Returns a pointer to a `struct tm` containing the local time, adjusted
 * according to the system timezone and daylight saving time information.
 *
 * @return Pointer to a `struct tm` with the adjusted local time.
 */
struct tm *sys_time_get_local_time(void);

/**
 * @brief Get the formatted local time string.
 *
 * Formats the current local time into the "YYYY-MM-DD HH:MM:SS" string format.
 *
 * @param[out] out Buffer to store the formatted time string. Should be at least 20 bytes.
 *
 * @return Returns 0 on success.
 */
int sys_time_get_local_time_format(char *out);

/**
 * @brief Get the current local time as an ISO 8601 formatted string.
 *
 * Returns the local time formatted as an ISO 8601 timestamp string including the timezone offset.
 *
 * @return Pointer to a static string buffer containing the ISO 8601 time,
 *         or NULL on failure.
 *
 * @note The returned string is stored in a static buffer and should not be modified or freed.
 */
char *sys_time_get_iso8601_time(void);

char *sys_time_get_iso8601_time_by_ms(void);

/**
 * @brief Format a UTC timestamp in milliseconds as an ISO 8601 local time string.
 *
 * The timestamp is converted using the current timezone and DST configuration.
 *
 * @param[in]  utc_ms   UTC timestamp in milliseconds.
 * @param[out] out      Output buffer.
 * @param[in]  out_len  Size of the output buffer.
 *
 * @return
 *         - 0 on success.
 *         - -1 on failure.
 */
int sys_time_format_iso8601_time_by_utc_ms(uint64_t utc_ms, char *out, size_t out_len);

#endif // OPEN_NEXTION_COMPONENTS_SYS_TIME_INCLUDE_SYS_TIME_API_H

#ifdef __cplusplus
}
#endif
