#ifndef NOC_W17_QUIETSINGL_E79BF7_H
#define NOC_W17_QUIETSINGL_E79BF7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_quietsingl_e79bf7_take_report_path @ hermes_cli/quiet_single_query.py:take_turn_report_path
 * Pure over an explicit environ object: pops HERMES_QUIET_TURN_REPORT_FILE and
 * returns {"value": str|null, "remaining": <environ without the key>}.
 */
struct json_t *w17_quietsingl_e79bf7_take_report_path(const struct json_t *envmap);

/* PoP: w17_quietsingl_e79bf7_exit_trailer @ hermes_cli/quiet_single_query.py:exit_single_query
 * Returns "\n[kanban-worker-exit] rc=<int(code)>" (code is a decimal string).
 */
struct json_t *w17_quietsingl_e79bf7_exit_trailer(const char *code);

/* PoP: w17_quietsingl_e79bf7_diagnostic_muted @ hermes_cli/quiet_single_query.py:_diagnostic_only_wake_muted
 * events is an array; warnings_enabled is injected (the oracle stubs
 * warning_notifications_enabled("cli") to this value and diagnostic_process_event
 * to bool(e.get("diagnostic")) for dict events).
 */
int w17_quietsingl_e79bf7_diagnostic_muted(const struct json_t *events, int warnings_enabled);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_QUIETSINGL_E79BF7_H */
