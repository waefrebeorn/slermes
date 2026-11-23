#ifndef NOC_W7_REDACTHEADE_29D7DB_H
#define NOC_W7_REDACTHEADE_29D7DB_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_redactheade_29d7db_cookie_value_credential @ agent/redact_headers.py:cookie_value_is_credential
 * Mirrors cookie_value_is_credential(value, qualified=qualified): a bare `cookie`
 * key is weak (value shape decides), a qualified `*_COOKIE` key treats any
 * literal value as a credential. */
bool w7_redactheade_29d7db_cookie_value_credential(const char *value, bool qualified);

/* PoP: w7_redactheade_29d7db_json_header_credential @ agent/redact_headers.py:json_header_value_is_credential
 * Gate for a JSON "<header>": "<value>" field: cookie fields use the cookie
 * value shape; API-key headers mask every value but an already-masked value or
 * an env-lookup / shell-variable placeholder. */
bool w7_redactheade_29d7db_json_header_credential(const char *key, const char *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_REDACTHEADE_29D7DB_H */
