#ifndef NOC_W6_ACPCLIENT_6F530E_H
#define NOC_W6_ACPCLIENT_6F530E_H

/* Port of upstream Hermes agent/acp_client.py -- the pure JSON-RPC / session
 * model shaping helpers. The subprocess/session machinery is not ported; the
 * `disabled` callable parameter is implemented as its default (_never_disabled).
 * Every exported symbol is prefixed with the gate id to avoid collisions.
 */
#include "hermes_json.h"

#define W6_ACPCLIENT_6F530E_DEFAULT_TIMEOUT_SECONDS 900.0

/* {"jsonrpc":"2.0","id":<id>,"result":<result>}; id/result are raw JSON text
 * (NULL => null). Caller owns the returned node. */
json_t *w6_acpclient_6f530e_jsonrpc_result(const char *id_json, const char *result_json);

/* {"jsonrpc":"2.0","id":<id>,"error":{"code":<int>,"message":<str>}} */
json_t *w6_acpclient_6f530e_jsonrpc_error(const char *id_json, int code, const char *message);

/* Ordered unique ids: non-objects skipped, str(value).strip(), drops empties
 * and repeats. entries_json is a JSON array, key the id field. */
json_t *w6_acpclient_6f530e_enabled_id_list(const char *entries_json, const char *key);

/* First config option whose category or id is "model", else NULL. */
json_t *w6_acpclient_6f530e_model_config_option(const char *session_json);

/* Model ids from configOptions (v1) else models.availableModels (legacy). */
json_t *w6_acpclient_6f530e_session_model_ids(const char *session_json);
json_t *w6_acpclient_6f530e_legacy_session_model_ids(const char *session_json);

/* {"method","params"} tuple selecting the model, or NULL. */
json_t *w6_acpclient_6f530e_model_selection_request(const char *session_json,
                                                    const char *requested_model,
                                                    const char *placeholder);

/* httpx.Timeout-like component set (Python reads attributes). */
typedef struct {
    int has_read, has_write, has_connect, has_pool, has_timeout;
    double read, write, connect, pool, timeout;
} w6_acpclient_6f530e_timeout_t;

/* Bare number is itself; otherwise largest present component; else 900s. */
double w6_acpclient_6f530e_effective_timeout(int is_number, double number,
                                            const w6_acpclient_6f530e_timeout_t *obj);

#endif /* NOC_W6_ACPCLIENT_6F530E_H */
