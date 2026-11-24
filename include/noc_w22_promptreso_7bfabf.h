#ifndef NOC_W22_PROMPTRESO_7BFABF_H
#define NOC_W22_PROMPTRESO_7BFABF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/relay/prompt_resolvers.py (gate w22_promptreso_7bfabf).
 *
 * One resolver per prompt kind, all running on the transport read loop. The
 * adapter (acks, typing, canonicalize), the pending-prompt stores, the
 * authority core (`gateway.approval_actor`) and the i18n lookup are the seam
 * and are injected: what the resolvers *decide* is ported here.
 *
 *   - actor:        the answering identity dict (`actor_id` + `admin_ids` +
 *                   `admitted`) assembled for a press.
 *   - refusal_plan: which authority-core call guards the answer, with the exact
 *                   positional/keyword arguments the resolver passes, and
 *                   whether a returned requester name turns into a refusal.
 *   - exec_ack:     the exec-approval choice (unknown option -> "deny"), the
 *                   ack label ("expired" when nothing was waiting) and whether
 *                   typing resumes.
 *   - slash_ack:    the slash-confirm choice (unknown option -> "cancel") and
 *                   the acks it sends (label, then the result text when set).
 *   - clarify:      "Other" -> text capture, a `c<index>` option -> the resolved
 *                   choice, anything unmappable -> text capture with no ack.
 */

/* PoP: w22_promptreso_7bfabf_actor @ gateway/relay/prompt_resolvers.py:prompt_actor
 * source: the event source ({"user_id": ...}; a missing key reads as "").
 * admin_ids / admitted: the `runner_answer_policy` / `answer_admitted` seam.
 * -> {"actor_id": <as the event carries it, or "">, "admin_ids": <copy>,
 *     "admitted": bool} */
json_t *w22_promptreso_7bfabf_actor(const json_t *source, const json_t *admin_ids,
                                    int admitted);

/* PoP: w22_promptreso_7bfabf_refusal_plan @ gateway/relay/prompt_resolvers.py:prompt_refusal
 * state: {"kind": ..., "session_key": ..., "request_id": ..., "confirm_id": ...,
 *         "clarify_id": ...}; actor: the dict prompt_actor built; option_id:
 * NULL for None; requester: the authority core's answer (NULL for None).
 * -> {"fn": "approval_refusal"|"slash_confirm_refusal"|"clarify_refusal",
 *     "args": [...], "kwargs": {...}, "refused": bool}
 * `refused` is `if requester` -- the only thing the resolver decides about a
 * returned requester name (refusal_text is the i18n seam). */
json_t *w22_promptreso_7bfabf_refusal_plan(const json_t *state, const json_t *actor,
                                           const char *option_id, const char *requester);

/* PoP: w22_promptreso_7bfabf_exec_ack @ gateway/relay/prompt_resolvers.py:resolve_exec_approval
 * count: the pending-approval rows the settlement found (0 = nothing waiting).
 * actor_state: state["actor"] (NULL/{} means no extra kwargs).
 * -> {"choice": str, "label": str, "acks": [label], "resume": bool,
 *     "kwargs": {...}} */
json_t *w22_promptreso_7bfabf_exec_ack(const char *option_id, int count,
                                       const json_t *actor_state);

/* PoP: w22_promptreso_7bfabf_slash_ack @ gateway/relay/prompt_resolvers.py:resolve_slash_confirm
 * -> {"choice": str, "acks": [label] or [label, result_text]} */
json_t *w22_promptreso_7bfabf_slash_ack(const char *option_id, const char *result_text);

/* PoP: w22_promptreso_7bfabf_clarify @ gateway/relay/prompt_resolvers.py:resolve_clarify
 * choices: state["choices"] (missing/null reads as []).
 * -> {"action": "await_text"|"resolve"|"fallback_text",
 *     "ack": str|null, "value": str|null} */
json_t *w22_promptreso_7bfabf_clarify(const char *option_id, const json_t *choices);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_PROMPTRESO_7BFABF_H */
