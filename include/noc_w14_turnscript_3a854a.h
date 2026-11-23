#ifndef NOC_W14_TURNSCRIPT_3A854A_H
#define NOC_W14_TURNSCRIPT_3A854A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_turnscript_3a854a_prelude_call_id @ agent/turn_scripted_prelude.py:prelude_call_id
 *
 * Mirrors deterministic_call_id(name, f"{session_id}:{json.dumps(args,
 * sort_keys=True)}", index). The deterministic_call_id seam is injected by the
 * oracle; the C port emits the same stub format. Returns malloc'd string. */
char *w14_turnscript_3a854a_prelude_call_id(const char *session_id, const char *name,
                                           const struct json_t *args, long long index);

/* PoP: w14_turnscript_3a854a_play_prelude @ agent/turn_scripted_prelude.py:play_prelude
 * verdict: {"action": ..., "result": ...} or null (the run_scripted_prelude seam).
 * Returns a JSON array [action, result]; caller frees. */
struct json_t *w14_turnscript_3a854a_play_prelude(int prelude_present, const struct json_t *verdict);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_TURNSCRIPT_3A854A_H */
