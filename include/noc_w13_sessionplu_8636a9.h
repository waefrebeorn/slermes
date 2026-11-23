#ifndef NOC_W13_SESSIONPLU_8636A9_H
#define NOC_W13_SESSIONPLU_8636A9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_sessionplu_8636a9_resolve_plugin_rpc @ gateway/session_plugin_rpc.py:resolve_plugin_rpc
 * A method not naming a plugin event returns JSON null. Otherwise the worker
 * resolves the handler (stubbed in the oracle): the projection is the string
 * "handled:<method>". */
struct json_t *w13_sessionplu_8636a9_resolve_plugin_rpc(const struct json_t *method);

/* PoP: w13_sessionplu_8636a9_live_key @ gateway/session_plugin_rpc.py:_live
 * (conversation id, live session) lookup: the session id itself when it is a
 * live key, else the current conversation key whose resident agent's
 * session_id equals it (compression), else JSON null. */
struct json_t *w13_sessionplu_8636a9_live_key(const struct json_t *session_id,
                                               const struct json_t *sessions,
                                               const struct json_t *agents);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONPLU_8636A9_H */
