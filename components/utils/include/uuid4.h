#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UUID v4 string length:
 *
 * xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
 *
 * 36 characters + '\0'
 */
#define UUID4_LEN 37

/*
 * Kept for compatibility with jeremytubongbanua/uuid4.
 *
 * On ESP-IDF we use esp_fill_random(), so no explicit seed/init is needed.
 */
void uuid4_init(void);

/*
 * Generate UUID v4 string.
 *
 * buf must point to a writable buffer of at least UUID4_LEN bytes.
 */
void uuid4_generate(char *buf);

#ifdef __cplusplus
}
#endif