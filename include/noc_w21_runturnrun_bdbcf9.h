#ifndef NOC_W21_RUNTURNRUN_BDBCF9_H
#define NOC_W21_RUNTURNRUN_BDBCF9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/run_turn_runner_approval_settle.py (gate w21_runturnrun_bdbcf9).
 *
 * The module tells a chat that an exec-approval prompt timed out. What is ported
 * is every decision and every byte of copy between the approval record and the
 * adapter call: the arming guard (request id + session key), the timeout/flag
 * fallbacks, the settle hook's own guards (reason == "timeout", run still
 * current), the timeout wording, the card body with its backtick splitting and
 * measure-aware fitting, and the edit-then-send routing.
 *
 * Not ported (bound to a live TurnRunner / asyncio / gateway.send): the
 * `runner._schedule` submission itself, `adapter.edit_message` / `adapter.send`
 * I/O, and `automatic_notices_allowed`'s SessionSource introspection (the model
 * takes its `is_bot` value, which is that function's whole input).
 */

/* PoP: w21_runturnrun_bdbcf9_window @ gateway/platforms/base_exec_approval.py:format_approval_window
 * `max(int(seconds or 0), 0)` then hour / minute / second wording ("5 minutes",
 * "90 seconds", "2 hours"). Caller frees. */
char *w21_runturnrun_bdbcf9_window(double seconds);

/* PoP: w21_runturnrun_bdbcf9_notice @ gateway/platforms/base_exec_approval.py:format_approval_timed_out_notice
 * `t("gateway.exec_approval.timed_out_notice", window=...)`. Caller frees. */
char *w21_runturnrun_bdbcf9_notice(double timeout_s);

/* PoP: w21_runturnrun_bdbcf9_arm @ gateway/run_turn_runner_approval_settle.py:register_timeout_notice
 * The arming half: `request_id` and `session_key` must both be truthy, `timeout_s`
 * falls back to the configured value, `flagged` to the default reason.
 *   {"armed":false}
 *   {"armed":true,"request_id":...,"session_key":"...","timeout_s":N,
 *    "timeout_from":"params"|"fallback","flagged":"..."} */
json_t *w21_runturnrun_bdbcf9_arm(const json_t *approval_data, const char *session_key,
                                  const char *fallback_reason, double fallback_timeout);

/* PoP: w21_runturnrun_bdbcf9_settle @ gateway/run_turn_runner_approval_settle.py:register_timeout_notice.settle
 * The hook's decision: only reason "timeout" schedules, and only while the run is
 * still current. `still_current` is JSON bool, or null/absent for a ctx without
 * the callable.
 *   {"scheduled":false,"why":"reason"|"not_current"} | {"scheduled":true,
 *    "label":"...","args":{"command":...,"card_message_id":...,"timeout_s":N,
 *    "reason":...}}   (the hook passes `flagged` as _post_timeout_notice's reason) */
json_t *w21_runturnrun_bdbcf9_settle(const char *reason, const json_t *still_current,
                                     const json_t *arm, const char *command,
                                     const char *card_message_id);

/* PoP: w21_runturnrun_bdbcf9_card_body @ gateway/run_turn_runner_approval_settle.py:_timed_out_card_body
 * `t("gateway.exec_approval.timed_out_card_body", notice=..., command=fit_prefix(...))`
 * with `re.sub("`{3,}")` ZWSP splitting and the reason line. `measure_mode` is
 * "len" (codepoints) or "utf16" (Telegram's unit). Caller frees. */
char *w21_runturnrun_bdbcf9_card_body(const char *notice, const char *command, const char *reason,
                                      const char *measure_mode, double max_len);

/* PoP: w21_runturnrun_bdbcf9_post_notice @ gateway/run_turn_runner_approval_settle.py:_post_timeout_notice
 * The whole notice route as one record, `req` carrying:
 *   timeout_s, command, reason, card_message_id, source{is_bot}|null,
 *   has_edit_message, edit_success, measure, max_len, status_chat_id,
 *   thread_metadata.
 *   {"action":"skipped"} | {"action":"edited","chat_id":...,"body":...}
 *   | {"action":"sent","chat_id":...,"text":"...","metadata":{...}} */
json_t *w21_runturnrun_bdbcf9_post_notice(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_RUNTURNRUN_BDBCF9_H */
