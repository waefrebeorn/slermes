/*
 * welcome_trial.c — faithful C11 port of hermes_cli/welcome_trial.py.
 * Gate w11_welcometri_be5319.
 *
 * PoP: w11_welcometri_be5319_claim_blocker @ hermes_cli/welcome_trial.py:claim_blocker
 * PoP: w11_welcometri_be5319_inference_available @ hermes_cli/welcome_trial.py:inference_available
 */
#include "hermes_json.h"
#include "noc_w11_welcometri_be5319.h"

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

/* Cheapest-first claim ladder. cfg_present/cfg_val mirror the caller's
 * ``configured`` Optional[bool]; when absent the blocker walks provider_configured. */
static const char *blocker_core(int assume_fresh, int cfg_present, int cfg_val,
        int local, int guest, int nous, int prov, int machine, int reserved) {
    if (!local) return "not the desktop's local backend";
    if (!guest) return "guests are switched off";
    if (nous) return "a Nous identity exists";
    if ((cfg_present ? cfg_val : prov)) return "a provider is configured";
    if (!assume_fresh && machine && !reserved)
        return "Hermes was used on this machine";
    return NULL;
}

const char *w11_welcometri_be5319_claim_blocker(int assume_fresh,
        const struct json_t *configured, int local_backend, int guest_allowed,
        int has_nous_identity, int provider_configured,
        int machine_existing, int trial_reserved) {
    int present = configured && configured->type != JSON_NULL;
    return blocker_core(assume_fresh, present, py_truthy(configured),
                        local_backend, guest_allowed, has_nous_identity,
                        provider_configured, machine_existing, trial_reserved);
}

int w11_welcometri_be5319_inference_available(int assume_fresh, int free_inference_active,
        int local_backend, int guest_allowed, int has_nous_identity,
        int provider_configured, int machine_existing, int trial_reserved) {
    if (free_inference_active) return 1;
    if (provider_configured) return 1;
    /* claim_blocker(assume_fresh, configured=provider_configured) */
    return blocker_core(assume_fresh, 1, provider_configured,
                        local_backend, guest_allowed, has_nous_identity,
                        provider_configured, machine_existing, trial_reserved) == NULL;
}
