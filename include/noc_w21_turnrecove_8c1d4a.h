#ifndef NOC_W21_TURNRECOVE_8C1D4A_H
#define NOC_W21_TURNRECOVE_8C1D4A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/turn_recovery_autorecover.py (gate w21_turnrecove_8c1d4a).
 *
 * The bounded post-exhaustion recovery ladder: `agent.auto_recovery_cycles`
 * (default 5) cycles on a 15/30/60/60/60 s schedule, entered only while the
 * provider is *temporarily* away (timeout / overloaded / server_error) and no
 * answer text has reached the user. Seams, because each is a live object or
 * clock:
 *   agent._auto_recovery_cycles, agent.platform  -> the fixture's `agent`
 *   provider_retry_after_seconds(api_error)      -> the fixture's
 *                                                   api_error.retry_after
 *   jittered_backoff()                           -> the deterministic base
 *                                                   delay (jitter is 0 here)
 *   agent._has_content_after_think_block(text)   -> the fixture's `has_content`
 *                                                   verdict, which the oracle
 *                                                   recomputes from the live
 *                                                   think-block scrubber
 *   interruptible_backoff_sleep()                -> the fixture's `interrupt`
 * Everything else -- the eligibility rules, the cycle counter, the notice text
 * and the action it returns -- is ported here.
 */

/* PoP: w21_turnrecove_8c1d4a_cycles @ agent/turn_recovery_autorecover.py:auto_recovery_cycles
 * `max(int(getattr(agent, "_auto_recovery_cycles", 0) or 0), 0)`. */
json_t *w21_turnrecove_8c1d4a_cycles(const json_t *agent);

/* PoP: w21_turnrecove_8c1d4a_wait_seconds @ agent/turn_recovery_autorecover.py:ladder_wait_seconds
 * {"wait": seconds, "source": "retry_after"|"schedule"}; a provider
 * Retry-After wins up to 120 s, else 15/30/60/60/60 s. */
json_t *w21_turnrecove_8c1d4a_wait_seconds(int cycle, const json_t *api_error);

/* PoP: w21_turnrecove_8c1d4a_eligible @ agent/turn_recovery_autorecover.py:ladder_eligible
 * True only with cycles configured, a transient reason, and no delivered text. */
json_t *w21_turnrecove_8c1d4a_eligible(const json_t *agent, const char *reason,
                                       const char *streamed, int has_content);

/* PoP: w21_turnrecove_8c1d4a_notice @ agent/turn_recovery_autorecover.py:ladder_notice
 * The countdown line for this surface (`_STOP_HINTS`, else the /stop hint). */
json_t *w21_turnrecove_8c1d4a_notice(const char *platform, double wait_s,
                                     int cycle, int total);

/* PoP: w21_turnrecove_8c1d4a_recover @ agent/turn_recovery_autorecover.py:auto_recover_after_exhaustion
 * One recovery cycle after retries + fallback are spent.
 *   case = {
 *     "agent": {"_auto_recovery_cycles": n, "platform": "cli"},
 *     "reason": "timeout",
 *     "streamed": "...", "has_content": false,
 *     "retry": {"auto_recovery_cycles_used": u,
 *               "restart_with_redirected_messages": false},
 *     "api_error": {"retry_after": null},
 *     "interrupt": null
 *   }
 * Answers {"action": "continue"|"break"|"return"|null, "result": ...,
 *          "used_cycles": n, "wait": f|null, "notice": str|null,
 *          "status": [...], "wait_lines": [...]}. */
json_t *w21_turnrecove_8c1d4a_recover(const json_t *case_);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_TURNRECOVE_8C1D4A_H */
