/*
 * verify.c - C11 port of upstream plugins/cron_providers/chronos/verify.py
 * (gate w25_verify_1e9aa1).
 *
 * PoP: w25_verify_1e9aa1_verify @ plugins/cron_providers/chronos/verify.py:verify_nas_fire_token
 * PoP: w25_verify_1e9aa1_plan @ plugins/cron_providers/chronos/verify.py:verify_nas_fire_token:<decode kwargs>
 * PoP: w25_verify_1e9aa1_constants @ plugins/cron_providers/chronos/verify.py:<module constants>
 *
 * PyJWT, the JWKS fetch (and its process-wide client cache) and the logger are
 * seams: the decoded claims, or the decode failure, arrive as fixed inputs.
 * Ported here is the boundary itself -- the guards that refuse a token before
 * any crypto runs, the asymmetric-only plan it is held to, and the purpose
 * claim that keeps a general agent JWT from replaying at the fire endpoint.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_verify_1e9aa1.h"

static const char *const VER_ALGORITHMS[W25_VERIFY_1E9AA1_ALGORITHMS] = {
    "RS256", "RS384", "RS512", "ES256", "ES384",
};

static const char *const VER_REQUIRED[W25_VERIFY_1E9AA1_REQUIRED_CLAIMS] = {
    "exp", "aud",
};

static int ver_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

json_t *w25_verify_1e9aa1_verify(const json_t *request) {
    const json_t *token = json_obj_get(request, "token");
    const json_t *audience = json_obj_get(request, "audience");
    const json_t *key = json_obj_get(request, "jwks_or_key");
    const json_t *decode = json_obj_get(request, "decode");
    json_t *out = json_object();
    const char *kind = NULL;

    /* if not token or not expected_audience: return None */
    if (!ver_truthy(token) || !ver_truthy(audience)) goto done;
    /* if not jwks_or_key: never fall back to an unsigned decode */
    if (!ver_truthy(key)) goto done;
    /* key.startswith(("http://", "https://")) -- a non-str key raises and the
     * swallowed exception is the same None. */
    if (!key || key->type != JSON_STRING || !key->str_val) goto done;
    if (!strncmp(key->str_val, "http://", 7) || !strncmp(key->str_val, "https://", 8))
        kind = "jwks";
    else
        kind = "inline";

    if (!decode || !ver_truthy(json_obj_get(decode, "ok"))) goto done;
    {
        const json_t *claims = json_obj_get(decode, "claims");
        const json_t *purpose;
        if (!claims || claims->type != JSON_OBJECT) goto done;
        purpose = json_obj_get(claims, "purpose");
        if (!(purpose && purpose->type == JSON_STRING && purpose->str_val &&
              !strcmp(purpose->str_val, W25_VERIFY_1E9AA1_FIRE_PURPOSE)))
            goto done;
        json_set(out, "claims", json_copy(claims));
        json_set(out, "key_kind", json_string(kind));
        return out;
    }
done:
    json_set(out, "claims", json_null());
    json_set(out, "key_kind", kind ? json_string(kind) : json_null());
    return out;
}

json_t *w25_verify_1e9aa1_plan(const json_t *request) {
    const json_t *audience = json_obj_get(request, "audience");
    const json_t *issuer = json_obj_get(request, "issuer");
    const json_t *leeway = json_obj_get(request, "leeway");
    json_t *out = json_object();
    json_t *algs = json_array();
    json_t *required = json_array();
    size_t i;

    for (i = 0; i < W25_VERIFY_1E9AA1_ALGORITHMS; i++)
        json_append(algs, json_string(VER_ALGORITHMS[i]));
    for (i = 0; i < W25_VERIFY_1E9AA1_REQUIRED_CLAIMS; i++)
        json_append(required, json_string(VER_REQUIRED[i]));

    json_set(out, "algorithms", algs);
    json_set(out, "audience", audience ? json_copy(audience) : json_null());
    json_set(out, "leeway", leeway ? json_copy(leeway) : json_number(W25_VERIFY_1E9AA1_DEFAULT_LEEWAY));
    json_set(out, "options_require", required);
    /* if issuer: decode_kwargs["issuer"] = issuer -- falsy leaves it out. */
    json_set(out, "issuer", ver_truthy(issuer) ? json_copy(issuer) : json_null());
    return out;
}

json_t *w25_verify_1e9aa1_constants(void) {
    json_t *out = json_object();
    json_t *algs = json_array();
    size_t i;
    for (i = 0; i < W25_VERIFY_1E9AA1_ALGORITHMS; i++)
        json_append(algs, json_string(VER_ALGORITHMS[i]));
    json_set(out, "fire_purpose", json_string(W25_VERIFY_1E9AA1_FIRE_PURPOSE));
    json_set(out, "algorithms", algs);
    json_set(out, "required_claims", json_number(W25_VERIFY_1E9AA1_REQUIRED_CLAIMS));
    json_set(out, "default_leeway_seconds", json_number(W25_VERIFY_1E9AA1_DEFAULT_LEEWAY));
    json_set(out, "verifier", json_string("verify_nas_fire_token"));
    return out;
}
