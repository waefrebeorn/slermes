#ifndef NOC_W9_RUNAUTORES_9A4E1C_H
#define NOC_W9_RUNAUTORES_9A4E1C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_runautores_9a4e1c_session_route @ gateway/run_auto_resume.py:_session_route
 *
 * Faithful port of _session_route(unit).  `parsed` is the already-computed
 * result of gateway.run._parse_session_key(origin_session) (a JSON object, or
 * null when parsing failed -> treated as {}), injected as a seam so the C side
 * never imports the heavyweight gateway runtime.  Returns the routing dict:
 *   platform, chat_id, chat_type, thread_id, user_id, user_name, scope_id,
 *   session_key, session_id, ui_session_id, and (when origin_session_id is
 *   truthy) session_history_delivery="1".  Caller frees. */
struct json_t *w9_runautores_9a4e1c_session_route(const struct json_t *unit,
                                                  const struct json_t *parsed);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_RUNAUTORES_9A4E1C_H */
