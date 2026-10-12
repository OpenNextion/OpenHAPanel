#include <esp_log.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "sys_time_api.h"
#include <sys/time.h>

extern uint32_t sys_time_start_time_s;
extern timezone_info_t sys_time_timezone_info;

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
bool sys_time_get_dst_sync_status(void)
{
    return sys_time_timezone_info.sync_time;
}

/**
 * @brief Retrieves the current timezone information.
 *
 * This function copies the current timezone information stored in
 * the global `sys_time_timezone_info` into the user-provided structure.
 *
 * @param[out] info Pointer to a `timezone_info_t` structure where the information
 *                  will be copied.
 *
 * @return Always returns 0.
 */
int sys_time_get_time_info(timezone_info_t *info)
{
    memcpy(info, &sys_time_timezone_info, sizeof(timezone_info_t));
    return 0;
}

/**
 * @brief Gets the system run time in milliseconds since system start.
 *
 * If the system start time (`sys_time_start_time_s`) is not initialized (zero),
 * this function returns the ESP log timestamp (milliseconds since boot).
 * Otherwise, it returns the elapsed time in milliseconds since the recorded start time.
 *
 * @return The system run time in milliseconds.
 */
uint64_t sys_time_get_system_run_time(void)
{
    if (sys_time_start_time_s == 0) {
        return esp_log_timestamp();
    }
    else {
        time_t timestamp;
        time(&timestamp);
        return (timestamp - sys_time_start_time_s) * 1000;
    }
}

/**
 * @brief Retrieves the current system UTC timestamp.
 *
 * This function obtains the current time in UTC and returns it as a 32-bit
 * unsigned integer representing the number of seconds since the Unix epoch.
 *
 * @return Current UTC timestamp as a uint32_t.
 */
uint32_t sys_time_get_sys_utc_time_stamp(void)
{
    time_t raw_time;
    time(&raw_time);
    return (uint32_t)raw_time;
}

/**
 * @brief Retrieves the local time adjusted for the current timezone and DST.
 *
 * This function obtains the current UTC time and adjusts it based on the
 * configured timezone offset and Daylight Saving Time (DST) rules stored in
 * `sys_time_timezone_info`.
 *
 * If DST is applicable and the current time is before the next DST transition,
 * the current UTC offset is applied; otherwise, the next UTC offset is used.
 *
 * @return Pointer to a `struct tm` representing the local time.
 */
struct tm *sys_time_get_local_time(void)
{
    time_t raw_time;
    time(&raw_time);

    if (sys_time_timezone_info.has_dst) {
        if ((uint32_t)raw_time < sys_time_timezone_info.next_dst) {
            raw_time += sys_time_timezone_info.utc_office;
        }
        else {
            raw_time += sys_time_timezone_info.next_utc_office;
        }
    }
    else {
        raw_time += sys_time_timezone_info.utc_office;
    }

    struct tm *_tm;
    _tm = localtime(&raw_time);

    return _tm;
}

/**
 * @brief Formats the current local time as a string.
 *
 * Retrieves the current local time adjusted for timezone and DST, then formats
 * it into a string with the format "YYYY-MM-DD HH:MM:SS".
 *
 * @param[out] out Pointer to a character buffer to receive the formatted time string.
 *                 The buffer should be at least 20 bytes to hold the formatted string.
 *
 * @return Always returns 0.
 */
int sys_time_get_local_time_format(char *out)
{
    struct tm *_tm = sys_time_get_local_time();
    strftime(out, 20, "%Y-%m-%d %H:%M:%S", _tm);
    return 0;
}

/**
 * @brief Get the current UTC offset in seconds, considering DST.
 *
 * This function returns the current timezone offset in seconds from UTC.
 * If Daylight Saving Time (DST) is enabled and the current time is before
 * the next DST transition, the current UTC offset is returned; otherwise,
 * the upcoming UTC offset after DST change is returned.
 *
 * @return The current UTC offset in seconds (can be negative).
 */
static int32_t sys_time_get_time_offset_by_utc(time_t raw_time)
{
    if (sys_time_timezone_info.has_dst == true) {
        if ((uint32_t)raw_time < sys_time_timezone_info.next_dst) {
            return sys_time_timezone_info.utc_office;
        }
        else {
            return sys_time_timezone_info.next_utc_office;
        }
    }
    return sys_time_timezone_info.utc_office;
}

static int32_t sys_time_get_current_time_offset(void)
{
    time_t raw_time;
    time(&raw_time);
    return sys_time_get_time_offset_by_utc(raw_time);
}

/**
 * @brief Get the current local time formatted as an ISO 8601 string with timezone offset.
 *
 * This function returns the current local time as a static string formatted according
 * to ISO 8601 (e.g., "YYYY-MM-DDTHH:MM:SS±HH:MM"). The timezone offset is calculated
 * based on the current UTC offset considering DST.
 *
 * @note The returned pointer points to a static buffer which is overwritten on each call.
 *
 * @return Pointer to a null-terminated ISO 8601 formatted time string on success,
 *         or NULL if there is an error (e.g., time retrieval or buffer overflow).
 */
char *sys_time_get_iso8601_time(void)
{
    static char iso8601_time[30];
    int offset = (int)sys_time_get_current_time_offset();

    struct tm *utc_time = sys_time_get_local_time();
    if (utc_time == NULL) {
        return NULL;
    }

    strftime(iso8601_time, sizeof(iso8601_time), "%Y-%m-%dT%H:%M:%S", utc_time);

    offset = offset / 60;
    int hours = offset / 60;
    int minutes = abs(offset % 60);

    char tz_offset[15];
    snprintf(tz_offset, sizeof(tz_offset), "%+03d:%02d", hours, minutes);

    size_t time_part_length = strlen(iso8601_time);
    size_t tz_length = strlen(tz_offset);
    if (time_part_length + tz_length + 1 > sizeof(iso8601_time)) {
        return NULL;
    }
    strcat(iso8601_time, tz_offset);
    return iso8601_time;
}

int sys_time_format_iso8601_time_by_utc_ms(uint64_t utc_ms, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return -1;
    }

    time_t utc_time = (time_t)(utc_ms / 1000ULL);
    int milliseconds = (int)(utc_ms % 1000ULL);
    int32_t offset = sys_time_get_time_offset_by_utc(utc_time);
    time_t local_time = utc_time + offset;
    struct tm local_tm = {0};

    if (gmtime_r(&local_time, &local_tm) == NULL) {
        return -1;
    }

    size_t time_len = strftime(out, out_len, "%Y-%m-%dT%H:%M:%S", &local_tm);
    if (time_len == 0) {
        return -1;
    }

    int written = snprintf(out + time_len, out_len - time_len, ".%03d", milliseconds);
    if (written < 0 || (size_t)written >= out_len - time_len) {
        return -1;
    }
    time_len += (size_t)written;

    offset = offset / 60;
    int hours = offset / 60;
    int minutes = abs(offset % 60);

    written = snprintf(out + time_len, out_len - time_len, "%+03d:%02d", hours, minutes);
    if (written < 0 || (size_t)written >= out_len - time_len) {
        return -1;
    }

    return 0;
}

char *sys_time_get_iso8601_time_by_ms(void)
{
    static char iso8601_time[40];

    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return NULL;
    }

    uint64_t utc_ms = ((uint64_t)tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000ULL);
    if (sys_time_format_iso8601_time_by_utc_ms(utc_ms, iso8601_time, sizeof(iso8601_time)) != 0) {
        return NULL;
    }

    return iso8601_time;
}