#ifndef NOC_W18_SESSIONMAN_98782B_H
#define NOC_W18_SESSIONMAN_98782B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionman_98782b_continuation_path @ gateway/session_managed_worker.py:_continuation_path
 * PoP: w18_sessionman_98782b_compression_ancestors @ gateway/session_managed_worker.py:_compression_ancestors
 * PoP: w18_sessionman_98782b_with_stop_cause @ gateway/session_managed_worker.py:_with_stop_cause
 *
 * ``store`` is a JSON object {"sessions": {<sid>: {"parent_session_id": <sid>,
 * "compression_child": <bool>}}}. ``worker`` is {"stop_cause": <string>}.
 * All three return a new json_t (caller frees).
 */
struct json_t *w18_sessionman_98782b_continuation_path(
    const struct json_t *store, const char *start, const char *tip);
struct json_t *w18_sessionman_98782b_compression_ancestors(
    const struct json_t *store, const char *session_id);
struct json_t *w18_sessionman_98782b_with_stop_cause(
    const struct json_t *accepted, const struct json_t *worker);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_SESSIONMAN_98782B_H */
