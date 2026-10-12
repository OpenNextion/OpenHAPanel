#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOGIN_SESSION_ID_HEX_LEN 32
#define LOGIN_SESSION_TOKEN_HEX_LEN 64
#define LOGIN_SESSION_PROOF_HEX_LEN 64
#define LOGIN_SESSION_CNONCE_MAX_LEN 64
#define LOGIN_SESSION_LIFETIME_SECONDS (30UL * 24UL * 60UL * 60UL)

typedef enum {
    LOGIN_SESSION_REASON_NONE = 0,
    LOGIN_SESSION_REASON_INVALID,
    LOGIN_SESSION_REASON_EXPIRED,
    LOGIN_SESSION_REASON_TIME_UNAVAILABLE,
    LOGIN_SESSION_REASON_STORAGE_FAILED,
    LOGIN_SESSION_REASON_FACTORY_RESETTING,
} login_session_reason_t;

typedef struct {
    char session_id[LOGIN_SESSION_ID_HEX_LEN + 1];
    char session_token[LOGIN_SESSION_TOKEN_HEX_LEN + 1];
    uint32_t expires_at;
} login_session_credential_t;

esp_err_t login_session_init(void);

esp_err_t login_session_issue(login_session_credential_t *credential,
                              login_session_reason_t *reason);

esp_err_t login_session_verify(const char *session_id,
                               const char *server_nonce,
                               const char *client_nonce,
                               const char *proof,
                               uint32_t *expires_at,
                               login_session_reason_t *reason);

esp_err_t login_session_revoke(const char *session_id);
esp_err_t login_session_revoke_all(void);
esp_err_t login_session_prepare_factory_reset(void);

bool login_session_time_is_valid(void);
const char *login_session_reason_to_string(login_session_reason_t reason);
void login_session_secure_zero(void *data, size_t length);

#ifdef __cplusplus
}
#endif
