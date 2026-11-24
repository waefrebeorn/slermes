#ifndef NOC_W19_BROWSERSUP_1C8A2D_H
#define NOC_W19_BROWSERSUP_1C8A2D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_browsersup_1c8a2d_invalid_message @ tools/browser_supervisor_capture.py:CapturedCDP._invalid
 * "captured CDP connection for task {task_id!r} is gone ...; capture again". Caller frees. */
char *w19_browsersup_1c8a2d_invalid_message(const char *task_id);

/* PoP: w19_browsersup_1c8a2d_is_valid @ tools/browser_supervisor_capture.py:CapturedCDP.is_valid
 * registry.get(task_id) is sup and sup._active and not sup._stop_requested and sup._ws is
 * captured ws and loop.is_running(). */
struct json_t *w19_browsersup_1c8a2d_is_valid(int registry_same, int active, int stop_requested,
                                              int ws_same, int loop_running);

/* PoP: w19_browsersup_1c8a2d_call_gate @ tools/browser_supervisor_capture.py:CapturedCDP.call
 * The ordered admission checks of call(): timeout shape -> before_send callable -> "cannot run on
 * the supervisor loop" -> connection validity -> dispatch wait. before_send_kind is 0 none /
 * 1 callable / 2 not callable. Returns {"error":<class name or null>,"message":<str or null>}. */
struct json_t *w19_browsersup_1c8a2d_call_gate(const struct json_t *timeout, double timeout_max,
                                               int before_send_kind, int valid, int loop_misuse,
                                               const char *method, const char *task_id);

/* PoP: w19_browsersup_1c8a2d_capture_error @ tools/browser_supervisor_capture.py:capture
 * {"error":<str or null>}: the "no running browser supervisor" precheck and the "no attached
 * page" snapshot guard, with the _LoopUnavailable loser mapped onto the same message. */
struct json_t *w19_browsersup_1c8a2d_capture_error(int found, int loop_present, int loop_running,
                                                   int active, int ws_present, int page_present,
                                                   int schedule_unavailable, const char *task_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_BROWSERSUP_1C8A2D_H */
