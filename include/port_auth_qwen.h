#ifndef PORT_AUTH_QWEN_H
#define PORT_AUTH_QWEN_H

#include "hermes_json.h"

/* Port of Python: hermes_cli/auth_qwen.py — the file/env half.
 *
 * Qwen OAuth reads its credentials from the qwen-cli token file
 * (<home>/.qwen/oauth_creds.json) and derives the runtime base URL from an
 * environment override. The refresh call itself (httpx POST + auth-store
 * write) is not ported here; it needs the shared auth store.
 */

#define QWEN_DEFAULT_BASE_URL "https://portal.qwen.ai/v1"
#define QWEN_REFRESH_SKEW_SECONDS 120

/* <home>/.qwen/oauth_creds.json (malloc'd). */
char *qwen_auth_path(const char *home);

/* Parse the token file (UTF-8 BOM tolerated). NULL with *err set to the exact
 * Python AuthError text: not found / unreadable / not an object. */
json_t *qwen_read_tokens(const char *path, char **err);

/* Write tokens as owner-only JSON. The caller passes the serialized document;
 * the parent directory is created. Returns 1 on success. */
int qwen_save_tokens(const char *path, const char *json_text);

/* True when (now + max(0, skew)) * 1000 >= expiry_ms, and always true when
 * the value is not an integer. */
int qwen_token_is_expiring(const char *expiry_date_ms, int skew_seconds,
                           double now_seconds);

/* HERMES_QWEN_BASE_URL, stripped of whitespace and trailing '/', else the
 * default. Malloc'd. */
char *qwen_base_url(const char *env_value);

#endif /* PORT_AUTH_QWEN_H */
