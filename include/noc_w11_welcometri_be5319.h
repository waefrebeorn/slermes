#ifndef NOC_W11_WELCOMETRI_BE5319_H
#define NOC_W11_WELCOMETRI_BE5319_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w11_welcometri_be5319_claim_blocker @ hermes_cli/welcome_trial.py:claim_blocker
 * All external predicates are injected seams: local_backend = on_local_desktop_backend(),
 * guest_allowed, has_nous_identity, provider_configured, machine_existing = probe().existing,
 * trial_reserved. configured is JSON null for None (use provider_configured) or a bool.
 * Returns a static reason string or NULL (do not free). */
const char *w11_welcometri_be5319_claim_blocker(int assume_fresh,
        const struct json_t *configured, int local_backend, int guest_allowed,
        int has_nous_identity, int provider_configured,
        int machine_existing, int trial_reserved);
/* PoP: w11_welcometri_be5319_inference_available @ hermes_cli/welcome_trial.py:inference_available
 * free_inference_active and the claim predicates are injected seams. Returns 1/0. */
int w11_welcometri_be5319_inference_available(int assume_fresh, int free_inference_active,
        int local_backend, int guest_allowed, int has_nous_identity,
        int provider_configured, int machine_existing, int trial_reserved);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_WELCOMETRI_BE5319_H */
