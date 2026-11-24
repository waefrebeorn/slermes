/*
 * noc_w25_verify_1e9aa1.h - C surface for upstream
 * plugins/cron_providers/chronos/verify.py (gate w25_verify_1e9aa1).
 *
 * Inbound cron-fire token verification.  PyJWT, the JWKS fetch and the process
 * wide PyJWKClient cache are seams; what is ported is the decision they sit
 * behind -- the guards that refuse a token before any crypto runs, the decode
 * plan the token is held to, and the purpose claim that scopes it to the fire
 * endpoint.
 */
#ifndef NOC_W25_VERIFY_1E9AA1_H
#define NOC_W25_VERIFY_1E9AA1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The purpose claim that scopes a token to /api/cron/fire. */
#define W25_VERIFY_1E9AA1_FIRE_PURPOSE "cron_fire"
#define W25_VERIFY_1E9AA1_ALGORITHMS 5
#define W25_VERIFY_1E9AA1_REQUIRED_CLAIMS 2
#define W25_VERIFY_1E9AA1_DEFAULT_LEEWAY 30

/* request: {"token", "audience", "jwks_or_key", "issuer", "leeway",
 *           "decode": {"ok": bool, "claims": {...}}} ->
 * {"claims": <decoded claims|null>, "key_kind": "jwks"|"inline"|null}.
 * Nothing raises: a missing token/audience/key, an unusable key, a failed
 * decode and a wrong purpose all end as null claims. */
json_t *w25_verify_1e9aa1_verify(const json_t *request);

/* request: {"audience", "issuer", "leeway"} -> the kwargs jwt.decode is called
 * with: {"algorithms", "audience", "leeway", "options_require", "issuer"}.
 * "issuer" is null when the module is not configured with one. */
json_t *w25_verify_1e9aa1_plan(const json_t *request);

/* The module's literals: purpose claim, allowed asymmetric algorithms, the
 * required claims and the default leeway. */
json_t *w25_verify_1e9aa1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
