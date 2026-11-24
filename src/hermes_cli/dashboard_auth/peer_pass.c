/*
 * peer_pass.c — C11 port of upstream Hermes hermes_cli/dashboard_auth/peer_pass.py
 *               (gate w25_peerpass_8acf90).
 *
 * PoP: w25_peerpass_8acf90_is_peer_path @ hermes_cli/dashboard_auth/peer_pass.py:is_peer_path
 * PoP: w25_peerpass_8acf90_peer_access_contract @ hermes_cli/dashboard_auth/peer_pass.py:peer_access_contract
 * PoP: w25_peerpass_8acf90_verify @ hermes_cli/dashboard_auth/peer_pass.py:_verify
 * PoP: w25_peerpass_8acf90_constants @ hermes_cli/dashboard_auth/peer_pass.py:PEER_PREFIX / PEER_ACCESS_CONTRACT
 *
 * The provider registry, the threadpool hop into ``verify_peer_pass`` and the
 * audit log are seams; each provider's verify outcome arrives as a fixed input
 * value and the values decide which provider, if any, opened the route.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_peerpass_8acf90.h"

static const char *const PEER_PREFIX = W25_PEERPASS_8ACF90_PEER_PREFIX;
static const int PEER_ACCESS_CONTRACT = W25_PEERPASS_8ACF90_PEER_ACCESS_CONTRACT;

/* Python truthiness over a JSON value; NULL json_t* is None. */
static int pp_truthy(const json_t *v) {
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

static json_t *pp_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "__error__", json_string(name));
    return o;
}

/* A provider's name as Python's ``provider.name`` would be: a str, else str(). */
static const char *pp_name(const json_t *provider) {
    const json_t *n = provider && provider->type == JSON_OBJECT
                          ? json_obj_get(provider, "name") : NULL;
    if (n && n->type == JSON_STRING) return n->str_val;
    if (n && n->type == JSON_NUMBER) return NULL;  /* never a fixture shape */
    return "";
}

json_t *w25_peerpass_8acf90_is_peer_path(const json_t *path) {
    size_t plen = strlen(PEER_PREFIX);
    if (!path || path->type != JSON_STRING) return pp_error("AttributeError");
    return json_bool(strncmp(path->str_val ? path->str_val : "", PEER_PREFIX, plen) == 0);
}

json_t *w25_peerpass_8acf90_peer_access_contract(const json_t *providers) {
    size_t i, n;
    if (!providers || providers->type != JSON_ARRAY) return pp_error("TypeError");
    n = json_len(providers);
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(providers, i);
        if (p && p->type == JSON_OBJECT && pp_truthy(json_obj_get(p, "supports_peer_pass")))
            return json_number((double)PEER_ACCESS_CONTRACT);
    }
    return json_number(0.0);
}

json_t *w25_peerpass_8acf90_verify(const json_t *providers, const json_t *token) {
    json_t *out;
    const char *unreachable = NULL;
    size_t i, n;

    (void)token;
    if (!providers || providers->type != JSON_ARRAY) return pp_error("TypeError");
    n = json_len(providers);
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(providers, i);
        const char *name, *verdict;
        if (!p || p->type != JSON_OBJECT) return pp_error("AttributeError");
        name = pp_name(p);
        verdict = json_get_str(p, "verify", "reject");
        if (!strcmp(verdict, "unreachable")) {
            /* ``unreachable = unreachable or provider.name``: the first truthy
             * name sticks, so an empty name does not claim the report. */
            if (!unreachable || !unreachable[0]) unreachable = name;
            continue;
        }
        if (!strcmp(verdict, "accept")) {
            out = json_object();
            json_set(out, "accepted", json_string(name));
            json_set(out, "unreachable", json_null());
            return out;
        }
    }
    out = json_object();
    json_set(out, "accepted", json_null());
    if (unreachable) json_set(out, "unreachable", json_string(unreachable));
    else json_set(out, "unreachable", json_null());
    return out;
}

json_t *w25_peerpass_8acf90_constants(void) {
    json_t *o = json_object();
    json_set(o, "peer_prefix", json_string(PEER_PREFIX));
    json_set(o, "peer_access_contract", json_number((double)PEER_ACCESS_CONTRACT));
    return o;
}
