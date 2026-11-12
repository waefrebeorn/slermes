#ifndef SLERMES_PORT_ANON_AUTH_H
#define SLERMES_PORT_ANON_AUTH_H

#include <hermes_json.h>
#include <stddef.h>

/* hermes_cli/anon_auth.py — the free-tier (anonymous Nous) policy cluster:
 * spoken durations, guest-state predicates, welcome-host routing, and the
 * per-code failure copy. Used by 47 reference modules.
 *
 * The pure routing predicates take their config answer as a parameter
 * (`guest_allowed`) rather than reading config.yaml, so the C caller owns the
 * config seam. String outputs go into a caller-supplied buffer. */

void anon_friendly_wait(double seconds, char *out, size_t outsz);

int anon_is_guest_state(json_t *state);

/* HERMES_EXTRA_WELCOME_HOSTS plus the built-in welcome host, as a json array. */
json_t *anon_welcome_hosts(void);

int anon_route_is_welcome_host(const char *base_url);
int anon_route_can_serve_model(const char *provider, const char *base_url,
                               const char *model);
void anon_pin_model_for_route(const char *provider, const char *base_url,
                              const char *model, char *out, size_t outsz);

/* The welcome trial carries inference. */
int anon_free_inference_active(json_t *state, int guest_allowed);
int anon_guest_without_free_tier(json_t *state, int guest_allowed);
int anon_has_free_tier_account(json_t *state, int guest_allowed);
void anon_trial_model(json_t *state, int guest_allowed, char *out,
                      size_t outsz);

void anon_failure_copy(const char *code, double retry_after, char *out,
                       size_t outsz);

#endif /* SLERMES_PORT_ANON_AUTH_H */
