#ifndef NOC_W22_FREETIERTA_E95C57_H
#define NOC_W22_FREETIERTA_E95C57_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tui_gateway/free_tier_task_done.py (gate w22_freetierta_e95c57).
 *
 * "A task finished on its own": the turn completed with a reply, on the free
 * tier, and nobody steered, redirected, interrupted or typed into it. The
 * module's two decisions are pure; only `record_task_done()` (which writes the
 * sign-in-offer state) stays at the seam:
 *
 *   - is_clean_task(result, agent, user_input, display_kind)
 *   - note_task_done(session, result, agent, display_kind) -> did it record?
 *
 * The live agent travels as the two attributes the predicate reads
 * (`provider`, `api_key`), and `is_anonymous_agent` is ported here in full:
 * provider == "nous" and the credential's JWT payload claim
 * `account_tier == "anonymous"` (`hermes_cli.anon_auth.is_anonymous_request`,
 * whose `_decode_jwt_claims` is a base64url + json read of the token body).
 */

/* PoP: w22_freetierta_e95c57_is_anonymous_agent @ hermes_cli/anon_auth.py:is_anonymous_agent
 * 1 when `provider == "nous"` and the JWT in `api_key` carries
 * `"account_tier": "anonymous"` in its payload. A token that is not three
 * dot-separated parts, whose body is not base64url, whose body is not JSON, or
 * whose JSON is not an object reads as "no claims" (0), never as an error. */
int w22_freetierta_e95c57_is_anonymous_agent(const char *provider, const char *api_key);

/* PoP: w22_freetierta_e95c57_is_clean_task @ tui_gateway/free_tier_task_done.py:is_clean_task
 * The four gates, in the source's order: no `auto_continue` display and no user
 * input; a dict result with a truthy "completed"; no "pending_steer" and no
 * "user_intervened"; a non-blank str(final_response); and an anonymous agent.
 * `display_kind` may be NULL (None). */
int w22_freetierta_e95c57_is_clean_task(const json_t *result, const char *provider,
                                        const char *api_key, int user_input,
                                        const char *display_kind);

/* PoP: w22_freetierta_e95c57_should_record @ tui_gateway/free_tier_task_done.py:note_task_done
 * note_task_done's guard: `bool(session.get("_turn_user_input"))` decides
 * user_input, then is_clean_task decides whether `record_task_done()` runs.
 * 1 means the sign-in offer would be (re)armed; the record itself is the seam. */
int w22_freetierta_e95c57_should_record(const json_t *session, const json_t *result,
                                        const char *provider, const char *api_key,
                                        const char *display_kind);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_FREETIERTA_E95C57_H */
