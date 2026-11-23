#ifndef NOC_W13_PLUGININJE_70086E_H
#define NOC_W13_PLUGININJE_70086E_H

struct json_t;
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: plugininj_inject_tui_session_message @
 *      tui_gateway/plugin_inject.py:inject_tui_session_message
 * Pure routing decision for a JSON-array model of _sessions (each object has
 * session_key, lazy, _closing, _finalized, history_lock, running). Returns bool.
 * Side effects (enqueue/last_active/drain thread) are not observable on the wire. */
struct json_t *plugininj_inject_tui_session_message(const struct json_t *session_key,
                                                    const struct json_t *content,
                                                    const struct json_t *sessions);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_PLUGININJE_70086E_H */
