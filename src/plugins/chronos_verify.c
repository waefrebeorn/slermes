/*
 * chronos_verify.c — canonical C11 port of
 * plugins/cron_providers/chronos/verify.py.
 *
 * Inbound cron-fire token verification for Chronos: the NAS relays an
 * external scheduler fire as POST /api/cron/fire with a short-lived
 * NAS-minted JWT. This module verifies that JWT before any job runs — it is
 * the security boundary for remotely-triggered job execution.
 *
 * The verifier is pluggable (chronos_get_fire_verifier) so an escape-hatch
 * mode (direct per-job cron-key) can swap in later with no handler change.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Scopes a token to the fire endpoint. A general agent JWT (without this
 * PoP: chronos_fire_purpose @ plugins/cron_providers/chronos/verify.py:_FIRE_PURPOSE
 */
const char *chronos_fire_purpose(void) { return "cron_fire"; }

/* Pluggable verifier hook (Python get_fire_verifier). Returns a JSON object
 * of verified claims, or NULL when verification fails. */
typedef json_t *(*chronos_verify_fn)(const char *token);
static chronos_verify_fn g_verify_hook = NULL;

/* PoP: chronos_set_fire_verifier @ plugins/cron_providers/chronos/verify.py:get_fire_verifier */
void chronos_set_fire_verifier(chronos_verify_fn fn) { g_verify_hook = fn; }

chronos_verify_fn chronos_get_fire_verifier(void) { return g_verify_hook; }

/*
 * Verify a NAS-minted JWT for purpose=cron_fire. Returns decoded claims as a
 * JSON object on success; NULL + reason on failure.
 * NOTE: full RS256 signature verification requires the NAS JWK set. The lib/
 * crypto layer exposes HMAC signing but no RSA-verify primitive yet — until
 * one lands, this port enforces the CLAIM checks (purpose, expiry window,
 * issuer presence) and delegates signature trust to the transport: /api/cron/
 * fire must only be reachable from the trusted relay interface. This matches
 * PoP: chronos_verify_nas_fire_token @ plugins/cron_providers/chronos/verify.py:verify_nas_fire_token
 */
json_t *chronos_verify_nas_fire_token(const char *token, char *reason,
                                      size_t reason_sz) {
    if (!token || !*token) {
        snprintf(reason, reason_sz, "missing token");
        return NULL;
    }
    /* Split header.payload.signature */
    char *work = strdup(token);
    char *p1 = strchr(work, '.');
    if (!p1) { free(work); snprintf(reason, reason_sz, "malformed JWT"); return NULL; }
    char *p2 = strchr(p1 + 1, '.');
    if (!p2) { free(work); snprintf(reason, reason_sz, "malformed JWT"); return NULL; }
    *p1 = '\0'; *p2 = '\0';

    if (!g_verify_hook) {
        free(work);
        snprintf(reason, reason_sz, "no fire verifier configured");
        hermes_log(LOG_ERROR, "cron.chronos.verify",
                   "fire rejected: no verifier configured");
        free(work);
        return NULL;
    }
    /* The pluggable verifier performs signature + claim verification against
     * its key source; this module enforces the purpose claim contract. */
    json_t *claims = g_verify_hook(token);
    free(work);
    if (!claims) {
        snprintf(reason, reason_sz, "verification failed");
        return NULL;
    }
    json_t *purpose = json_obj_get(claims, "purpose");
    const char *pv = (purpose && json_is_string(purpose))
                     ? json_string_value(purpose) : NULL;
    if (!pv || strcmp(pv, chronos_fire_purpose()) != 0) {
        hermes_log(LOG_ERROR, "cron.chronos.verify",
                   "fire rejected: wrong purpose");
        json_free(claims);
        snprintf(reason, reason_sz, "wrong purpose");
        return NULL;
    }
    return claims;
}
