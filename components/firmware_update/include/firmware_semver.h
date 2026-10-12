#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check whether a string is a strict SemVer 2.0.0 version.
 *
 * A leading "v" is deliberately not accepted.
 */
bool firmware_semver_is_valid(const char *version);

/**
 * @brief Compare two strict SemVer 2.0.0 versions.
 *
 * @param left First version.
 * @param right Second version.
 * @param result Receives -1, 0, or 1. Build metadata does not affect precedence.
 */
esp_err_t firmware_semver_compare(const char *left, const char *right, int *result);

#ifdef __cplusplus
}
#endif
