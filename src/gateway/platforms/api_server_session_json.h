/*
 * api_server_session_json.h - session/message JSON helpers.
 *
 * Module-internal header (api_server_adapter_sessions.c <-> api_server_session_json.c).
 */
#ifndef API_SERVER_SESSION_JSON_H
#define API_SERVER_SESSION_JSON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "hermes_json.h"          /* json_t */
#include "hermes_core_types.h"   /* db_t */


json_t * message_to_json(const char *msg_json);
int parse_nonnegative_int(const char *value, int default_val, int maximum);
json_t * session_to_json(db_t *db, const char *session_id);

#endif /* API_SERVER_SESSION_JSON_H */
