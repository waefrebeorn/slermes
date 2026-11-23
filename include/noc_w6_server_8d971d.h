#ifndef NOC_W6_SERVER_8D971D_H
#define NOC_W6_SERVER_8D971D_H

#include "hermes_json.h"

/* Public C API for the pure dispatch leaves of tui_gateway/server.py
 * (gate w6_server_8d971d). Every symbol is tuisrv_-prefixed to avoid
 * collisions with the many other "server_*" symbols in this repo. */
struct json_t *tuisrv_ok(struct json_t *rid, const struct json_t *result);
struct json_t *tuisrv_err(struct json_t *rid, double code, const char *msg,
                          const struct json_t *data);
struct json_t *tuisrv_normalize_request(const struct json_t *req);
struct json_t *tuisrv_is_model_switch_marker(const struct json_t *entry);
struct json_t *tuisrv_is_pivot_marker(const struct json_t *entry);
struct json_t *tuisrv_coerce_int_config_value(const struct json_t *value,
                                              double def, double min_value);
struct json_t *tuisrv_cli_exec_blocked(const struct json_t *argv);
struct json_t *tuisrv_turn_started_at(const struct json_t *session);
struct json_t *tuisrv_event_frame(const char *event, const char *sid,
                                  const struct json_t *payload);

#endif /* NOC_W6_SERVER_8D971D_H */
