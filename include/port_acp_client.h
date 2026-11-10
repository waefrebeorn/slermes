#ifndef PORT_ACP_CLIENT_H
#define PORT_ACP_CLIENT_H

#include "hermes_json.h"

/* Port of Python: agent/acp_client.py — the JSON-RPC / session-model shaping.
 *
 * The agent process, subprocess environment and filesystem handlers are not
 * ported here. The Python `disabled` callables are parameters of several
 * helpers; this port implements the default (`_never_disabled`), which is the
 * behaviour every caller gets unless it injects a filter.
 */

#define ACP_DEFAULT_TIMEOUT_SECONDS 900.0

/* {"jsonrpc":"2.0","id":<id>,"result":<result>} — id/result are JSON text
 * (NULL means null). Caller owns the returned object. */
json_t *acp_jsonrpc_result(const char *id_json, const char *result_json);

/* {"jsonrpc":"2.0","id":<id>,"error":{"code":<code>,"message":<message>}} */
json_t *acp_jsonrpc_error(const char *id_json, int code, const char *message);

/* Ordered unique ids of the entries the (default) filter does not reject:
 * non-objects skipped, values stringified+stripped, empties and repeats
 * dropped. Returns a JSON array. */
json_t *acp_enabled_id_list(const char *entries_json, const char *key);

/* The first config option whose category or id is "model", or NULL. */
json_t *acp_model_config_option(const char *session_json);

/* Model ids advertised by session/new (v1 config option, else the legacy
 * models.availableModels extension). Returns a JSON array. */
json_t *acp_session_model_ids(const char *session_json);
json_t *acp_legacy_session_model_ids(const char *session_json);

/* {"method","params"} selecting `requested_model`, or NULL when the session is
 * unusable, the id is the placeholder, or a reported model list does not
 * contain it. */
json_t *acp_model_selection_request(const char *session_json,
                                    const char *requested_model,
                                    const char *placeholder);

/* httpx.Timeout-like component set (Python reads ATTRIBUTES, not dict keys, so
 * a plain dict yields the default there too). */
typedef struct {
    int has_read, has_write, has_connect, has_pool, has_timeout;
    double read, write, connect, pool, timeout;
} acp_timeout_t;

/* Wall-clock seconds: a bare number is itself; otherwise the largest present
 * component, else the default (900s). */
double acp_effective_timeout(int is_number, double number,
                             const acp_timeout_t *obj);

#endif /* PORT_ACP_CLIENT_H */
