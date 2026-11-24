#ifndef NOC_W20_RUNTURNRUN_9CE529_H
#define NOC_W20_RUNTURNRUN_9CE529_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/run_turn_runner_approval_text.py (gate w20_runturnrun_9ce529).
 *
 * The plain-text ``/approve`` prompt for adapters whose ``send`` cuts long text
 * instead of splitting it.  The localized strings are data (the caller's i18n
 * bundle) and the adapter is reduced to its two measurement hooks, so the
 * layout, the choice ladder, the deny-only fallback and the shortening
 * arithmetic are real C.
 */

/* PoP: w20_runturnrun_9ce529_cutting_text_prompt @ gateway/run_turn_runner_approval_text.py:_cutting_text_prompt
 * Request:
 *   {"command": str, "description": str, "prefix": str,
 *    "limit": int,                       adapter.max_message_length_for_chat
 *    "allow_permanent": bool, "allow_session": bool, "smart_denied": bool,
 *    "i18n": {"header","reason_label","deadline","joiner","last","once",
 *             "session","always","deny","deny_only","truncated","smart_heading"}}
 *   `{prefix}` in the choice strings and `{choice}`, `{total}` in the others
 *   are the same substitutions the live module's `t()` performs.
 * Answer: {"text": <prompt>, "deny_only": bool}.
 * The prompt puts the instructions and the deadline ahead of the command, so a
 * cut can only shorten the command; when even the deny-only layout does not
 * fit, the command is shortened with a fence sized from the shown text. */
json_t *w20_runturnrun_9ce529_cutting_text_prompt(const json_t *req);

/* PoP: w20_runturnrun_9ce529_code_fence @ gateway/platforms/base_exec_approval.py:code_fence
 * A backtick fence longer than any backtick run in ``command`` (at least 3), so
 * the command cannot close it. */
json_t *w20_runturnrun_9ce529_code_fence(const char *command);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_RUNTURNRUN_9CE529_H */
