/*
 * noc_w33_configfree_3f224a.h - C surface for upstream Hermes
 * tui_gateway/contracts/config_free_tier_control.py (gate w33_configfree_3f224a).
 *
 * The contracts module is the wire shape of the free-tier control surface:
 * ``free_tier.status``'s identity payload, the browser-challenge payload the
 * ``free_tier.challenge`` event and the status field share, and the
 * ``claim_nudge`` / ``provision`` availability answer.  The values behind that
 * shape are decided by five pure bodies:
 *
 *   tui_gateway/methods_free_tier.py : the ``free_tier.status`` payload assembly
 *   hermes_cli/anon_auth.py          : the entitlement predicates and failure copy
 *   hermes_cli/anon_challenge.py     : challenge parsing, payload and host-outcome match
 *   hermes_cli/welcome_trial.py      : the claim ladder (why the trial is not claimable)
 *
 * Every I/O seam stays outside: the auth-store state, the config ``nous.guest``
 * switch, the mint memo, the install probe, the process clock and the offer
 * schedule arrive as fixture values, so what is compared is exactly what the
 * values decide.  Nothing here reads a file, a clock or the network.
 */
#ifndef NOC_W33_CONFIGFREE_3F224A_H
#define NOC_W33_CONFIGFREE_3F224A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The free-tier word the surface names itself with (anon_auth module constants). */
#define W33_CONFIGFREE_3F224A_GUEST_MODEL "nous/welcome"
#define W33_CONFIGFREE_3F224A_LABEL "Nous \xc2\xb7 free tier"
#define W33_CONFIGFREE_3F224A_AUTH_METHOD "anonymous"
#define W33_CONFIGFREE_3F224A_WELCOME_FLAG "welcome_claim"
#define W33_CONFIGFREE_3F224A_NOTICE_FLAG "guest_notice_shown"

/*
 * PoP: w33_configfree_3f224a_status_payload @ tui_gateway/methods_free_tier.py:free_tier.status
 * identity = {state, guest_allowed, trial_blocker, nudge_due_in,
 *             connector_nudge_due_in, mint_failure, challenge} ->
 * {has_guest, enabled, available, notice_pending, model, label}
 * plus nudge_due_in / connector_nudge_due_in when due, the mint failure block
 * when enabled without an identity, and challenge when one is pending.
 */
json_t *w33_configfree_3f224a_status_payload(const json_t *identity);

/*
 * PoP: w33_configfree_3f224a_guest_state @ hermes_cli/anon_auth.py:is_guest_state
 * 1 when state is an object whose auth_method is "anonymous".
 */
int w33_configfree_3f224a_guest_state(const json_t *state);

/*
 * PoP: w33_configfree_3f224a_free_inference @ hermes_cli/anon_auth.py:free_inference_active
 * guest state, welcome_claim is literally true, and guests are allowed here.
 */
int w33_configfree_3f224a_free_inference(const json_t *state, int guest_allowed);

/*
 * PoP: w33_configfree_3f224a_guest_without_tier @ hermes_cli/anon_auth.py:guest_without_free_tier
 * a guest that does not carry inference here (connectors only).
 */
int w33_configfree_3f224a_guest_without_tier(const json_t *state, int guest_allowed);

/*
 * PoP: w33_configfree_3f224a_notice_pending @ hermes_cli/anon_auth.py:guest_notice_pending
 * free inference is active and guest_notice_shown is not set on the identity.
 */
int w33_configfree_3f224a_notice_pending(const json_t *state, int guest_allowed);

/*
 * PoP: w33_configfree_3f224a_failure_copy @ hermes_cli/anon_auth.py:anon_failure_copy
 * The surface-agnostic sentence for a free-tier failure code; retry_after is the
 * raw value (number / string / null), null when absent.  Unknown codes fall back
 * to the server-error sentence.
 */
json_t *w33_configfree_3f224a_failure_copy(const char *code, const json_t *retry_after);

/*
 * PoP: w33_configfree_3f224a_mint_payload @ hermes_cli/anon_auth.py:MintFailure.as_payload
 * failure = {code, message, retryable, retry_after, not_before}, now the process
 * clock -> {error_code, error, retryable, retry_after}; null for no failure.
 */
json_t *w33_configfree_3f224a_mint_payload(const json_t *failure, double now);

/*
 * PoP: w33_configfree_3f224a_challenge_parse @ hermes_cli/anon_challenge.py:parse_browser_challenge
 * payload = the 428 body, portal_base_url the exchange's own origin ->
 * {url, required, expires_in, interval, message, attempt} for the first
 * browser challenge this client can run, or null.
 */
json_t *w33_configfree_3f224a_challenge_parse(const json_t *payload, const char *portal_base_url);

/*
 * PoP: w33_configfree_3f224a_challenge_payload @ hermes_cli/anon_challenge.py:pending_challenge
 * rec = {pending, deadline, outcome}, now the monotonic clock -> the
 * free_tier.status challenge payload with expires_in = ceil(deadline - now),
 * or null when nothing is pending / it expired / the host already ended it.
 */
json_t *w33_configfree_3f224a_challenge_payload(const json_t *rec, double now);

/*
 * PoP: w33_configfree_3f224a_challenge_outcome @ hermes_cli/anon_challenge.py:record_host_outcome
 * 1 when (url, attempt) names the live pending attempt of rec (and is not
 * expired): the presentation hint is accepted.
 */
int w33_configfree_3f224a_challenge_outcome(const json_t *rec, const char *url, long attempt, double now);

/*
 * PoP: w33_configfree_3f224a_claim_blocker @ hermes_cli/welcome_trial.py:claim_blocker
 * ask = {assume_fresh, configured, provider_configured,
 *        on_local_desktop_backend, guest_allowed, has_nous_identity,
 *        machine_used, trial_reserved} -> why the welcome trial cannot be
 * claimed here, or null when it can.
 */
json_t *w33_configfree_3f224a_claim_blocker(const json_t *ask);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W33_CONFIGFREE_3F224A_H */
