#ifndef NOC_W7_ANONAUTH_C583C5_H
#define NOC_W7_ANONAUTH_C583C5_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_anonauth_c583c5_friendly_wait @ hermes_cli/anon_auth.py:friendly_wait
 * Rounded, spoken duration; seconds is a decimal string (None/invalid -> 0). */
struct json_t *w7_anonauth_c583c5_friendly_wait(const char *seconds);

/* PoP: w7_anonauth_c583c5_is_guest_state @ hermes_cli/anon_auth.py:is_guest_state
 * isinstance(state, dict) and state.get("auth_method") == "anonymous". */
int w7_anonauth_c583c5_is_guest_state(const struct json_t *state);

/* PoP: w7_anonauth_c583c5_parse_welcome_refusal @ hermes_cli/anon_auth.py:parse_welcome_refusal
 * Structured 429 refusal -> {reason, retry_after, alternates, upgrade_url} or null. */
struct json_t *w7_anonauth_c583c5_parse_welcome_refusal(const struct json_t *body);

/* PoP: w7_anonauth_c583c5_anon_failure_copy @ hermes_cli/anon_auth.py:anon_failure_copy
 * Surface-agnostic sentence for a free-tier failure code; retry_after is a
 * decimal string (empty -> None -> 60s). */
struct json_t *w7_anonauth_c583c5_anon_failure_copy(const char *code, const char *retry_after);

/* PoP: w7_anonauth_c583c5_route_is_welcome_host @ hermes_cli/anon_auth.py:route_is_welcome_host
 * extra_hosts is a comma-separated seam for HERMES_EXTRA_WELCOME_HOSTS. */
int w7_anonauth_c583c5_route_is_welcome_host(const char *base_url, const char *extra_hosts);

/* PoP: w7_anonauth_c583c5_pin_model_for_route @ hermes_cli/anon_auth.py:pin_model_for_route
 * On the welcome host the model is nous/welcome; elsewhere the caller's model stands. */
struct json_t *w7_anonauth_c583c5_pin_model_for_route(const char *provider, const char *base_url,
                                                       const char *model, const char *extra_hosts);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_ANONAUTH_C583C5_H */
