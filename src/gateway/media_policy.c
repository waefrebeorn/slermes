/*
 * media_policy.c — C11 port of gateway/media_policy.py
 *
 * Shared config→env bridge for media-delivery policy. Precedence: an
 * explicitly-set environment variable WINS over config.yaml — a pre-existing
 * env value is never overwritten, so operator shell exports survive.
 */

#define _POSIX_C_SOURCE 200809L

#include "media_policy.h"
#include "credential_pool_internals.h"

#include <stdlib.h>
#include <string.h>

#define MEDIA_POLICY_STRICT_ENV        "HERMES_MEDIA_DELIVERY_STRICT"
#define MEDIA_POLICY_ALLOW_DIRS_ENV    "HERMES_MEDIA_ALLOW_DIRS"
#define MEDIA_POLICY_TRUST_RECENT_ENV  "HERMES_MEDIA_TRUST_RECENT_FILES"

/* Python truthiness over a JSON node (None excluded by caller):
 * False for JSON_FALSE, JSON_NULL, 0-number, "" string, [] / {}; True otherwise
 * (including any non-empty string like "0" — Python strings are truthy). */
static int mp_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val != 0;
    case JSON_NULL:   return 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len((json_t *)v) > 0;
    case JSON_OBJECT:
        return v->c.count > 0;  /* empty dict is falsy */
    default: return 0;
    }
}

/* PoP: _load_gateway_cfg @ gateway/media_policy.py:_load_gateway_cfg */
json_t *media_policy_load_gateway_cfg(json_t *config) {
    if (!config) {
        config = credential_pool_load_config_safe();
        if (!config) return NULL;
    }
    json_t *gw = json_object_get(config, "gateway");
    if (!gw || gw->type != JSON_OBJECT) return NULL;
    return gw;
}

/* PoP: apply_media_policy_env @ gateway/media_policy.py:apply_media_policy_env */
void media_policy_apply_env(json_t *config) {
    /* Python wraps everything in try/except: a policy-bridge failure must
     * never break delivery. In C every access below is NULL-guarded. */
    json_t *owned = NULL;
    if (!config) {
        owned = credential_pool_load_config_safe();
        config = owned;
    }

    json_t *gateway_cfg = config ? json_object_get(config, "gateway") : NULL;
    if (!gateway_cfg || gateway_cfg->type != JSON_OBJECT)
        goto done;
    if (gateway_cfg->c.count == 0)
        goto done;  /* empty dict is falsy in Python */

    /* strict -> HERMES_MEDIA_DELIVERY_STRICT ("1"/"0") when env unset. */
    json_t *strict = json_object_get(gateway_cfg, "strict");
    if (strict && strict->type != JSON_NULL && !getenv(MEDIA_POLICY_STRICT_ENV)) {
        setenv(MEDIA_POLICY_STRICT_ENV, mp_truthy(strict) ? "1" : "0", 0);
    }

    /* media_delivery_allow_dirs -> HERMES_MEDIA_ALLOW_DIRS when env unset. */
    json_t *allow_dirs = json_object_get(gateway_cfg, "media_delivery_allow_dirs");
    if (allow_dirs && mp_truthy(allow_dirs) && !getenv(MEDIA_POLICY_ALLOW_DIRS_ENV)) {
        char *joined = NULL;
        size_t jcap = 0, jlen = 0;
        if (allow_dirs->type == JSON_STRING) {
            joined = strdup(allow_dirs->str_val ? allow_dirs->str_val : "");
        } else if (allow_dirs->type == JSON_ARRAY) {
            /* os.pathsep.join(str(p) for p in allow_dirs if p) — ':' on Linux,
             * skipping falsy entries (empty strings / nulls / zeros). */
            size_t n = json_len(allow_dirs);
            joined = malloc(1);
            if (joined) { joined[0] = '\0'; jcap = 1; }
            int first = 1;
            for (size_t i = 0; joined && i < n; i++) {
                json_t *p = json_get(allow_dirs, i);
                const char *ps = NULL;
                char numbuf[64];
                if (p && p->type == JSON_STRING) ps = p->str_val;
                else if (p && p->type == JSON_NUMBER) {
                    snprintf(numbuf, sizeof(numbuf), "%g", p->num_val);
                    ps = numbuf;
                }
                else if (p && p->type == JSON_BOOL) ps = p->bool_val ? "True" : "False";
                else continue;  /* None / nested containers are falsy or str() noise; skip */
                if (!ps || !*ps) continue;  /* `if p` filter */
                size_t plen = strlen(ps);
                size_t need = jlen + (first ? 0 : 1) + plen + 1;
                if (need > jcap) {
                    size_t ncap = jcap ? jcap : 32;
                    while (ncap < need) ncap *= 2;
                    char *nj = realloc(joined, ncap);
                    if (!nj) { free(joined); joined = NULL; break; }
                    joined = nj; jcap = ncap;
                }
                if (!first) joined[jlen++] = ':';
                memcpy(joined + jlen, ps, plen);
                jlen += plen;
                joined[jlen] = '\0';
                first = 0;
            }
        } else {
            /* non-str/list/tuple allow_dirs -> allow_dirs_str = "" */
            joined = strdup("");
        }
        if (joined && *joined) setenv(MEDIA_POLICY_ALLOW_DIRS_ENV, joined, 0);
        free(joined);
    }

    /* trust_recent_files -> HERMES_MEDIA_TRUST_RECENT_FILES ("1"/"0"). */
    json_t *trust = json_object_get(gateway_cfg, "trust_recent_files");
    if (trust && trust->type != JSON_NULL && !getenv(MEDIA_POLICY_TRUST_RECENT_ENV)) {
        setenv(MEDIA_POLICY_TRUST_RECENT_ENV, mp_truthy(trust) ? "1" : "0", 0);
    }

done:
    /* credential_pool_load_config_safe returns an owned node. */
    json_free(owned);
}
