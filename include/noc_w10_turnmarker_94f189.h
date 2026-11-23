#ifndef NOC_W10_TURNMARKER_94F189_H
#define NOC_W10_TURNMARKER_94F189_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_turnmarker_94f189_started_at @ tui_gateway/turn_marker.py:_started_at
 * float(entry.get("started_at") or 0). */
double w10_turnmarker_94f189_started_at(const struct json_t *entry);
/* PoP: w10_turnmarker_94f189_marker_writer_state @ tui_gateway/turn_marker.py:marker_writer_state
 * liveness seam: 0 = _pid_liveness returned None, 1 = truthy (alive), 2 = falsy (dead),
 * 3 = liveness lookup raised. Any case degrades to "unknown" as in the live code. */
const char *w10_turnmarker_94f189_marker_writer_state(const struct json_t *entry, int liveness);
/* PoP: w10_turnmarker_94f189_prune @ tui_gateway/turn_marker.py:_prune
 * Age/count pruning with the module's _MAX_AGE_SECS (86400) and _MAX_ENTRIES (32). */
struct json_t *w10_turnmarker_94f189_prune(const struct json_t *entries, double now);
/* PoP: w10_turnmarker_94f189_read_turn_marker @ tui_gateway/turn_marker.py:read_turn_marker
 * stored seam: the dict that _load(home) would return. NULL (None) for no marker; caller frees. */
struct json_t *w10_turnmarker_94f189_read_turn_marker(const char *session_key, const struct json_t *stored);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_TURNMARKER_94F189_H */
