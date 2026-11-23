/*
 * platform_activation.c — faithful C11 port of the pure activation-evidence
 * helpers in gateway/platform_activation.py (gate w14_platformac_7882b1).
 * PoP: w14_platformac_7882b1_yaml_block_names @ gateway/platform_activation.py:_yaml_block_names
 * PoP: w14_platformac_7882b1_matches @ gateway/platform_activation.py:_matches
 * PoP: w14_platformac_7882b1_has_evidence @ gateway/platform_activation.py:activation_predicate.<locals>.has_evidence
 */
#include "noc_w14_platformac_7882b1.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *lower_copy(const char *s) {
    size_t n = strlen(s ? s : "");
    char *p = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; ++i) p[i] = (char)tolower((unsigned char)s[i]);
    p[n] = '\0';
    return p;
}
static char *upper_copy(const char *s) {
    size_t n = strlen(s ? s : "");
    char *p = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; ++i) p[i] = (char)toupper((unsigned char)s[i]);
    p[n] = '\0';
    return p;
}

/* Collect keys of every mapping a platform block may live in. */
json_t *w14_platformac_7882b1_yaml_block_names(json_t *yaml_cfg) {
    json_t *out = json_array();
    if (!yaml_cfg || yaml_cfg->type != JSON_OBJECT) return out;

    json_t *gw = json_obj_get(yaml_cfg, "gateway");
    json_t *gateway = (gw && gw->type == JSON_OBJECT) ? gw : NULL;
    json_t *parents[4];
    size_t np = 0;
    parents[np++] = yaml_cfg;
    json_t *pl = json_obj_get(yaml_cfg, "platforms");
    if (pl && pl->type == JSON_OBJECT) parents[np++] = pl;
    if (gateway) parents[np++] = gateway;
    if (gateway) {
        json_t *gpl = json_obj_get(gateway, "platforms");
        if (gpl && gpl->type == JSON_OBJECT) parents[np++] = gpl;
    }

    char names[512][256];
    size_t n = 0;
    for (size_t p = 0; p < np; ++p) {
        for (size_t i = 0; i < json_object_size(parents[p]) && n < 512; ++i) {
            const char *k = json_object_get_key_at(parents[p], i);
            json_t *v = json_object_get_at(parents[p], i);
            if (!k || !v || v->type != JSON_OBJECT) continue; /* only dict values */
            size_t j = 0;
            for (; j < n; ++j) if (!strcmp(names[j], k)) break;
            if (j == n) { snprintf(names[n], sizeof(names[j]), "%s", k); n++; }
        }
    }
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            if (strcmp(names[i], names[j]) > 0) {
                char t[256]; strcpy(t, names[i]); strcpy(names[i], names[j]); strcpy(names[j], t);
            }
    for (size_t i = 0; i < n; ++i) json_append(out, json_string(names[i]));
    return out;
}

/* `c.lower() == lowered or c.lower().startswith(lowered + "_")` */
json_t *w14_platformac_7882b1_matches(const char *name, json_t *candidates) {
    char *lowered = lower_copy(name);
    size_t plen = strlen(lowered) + 2;
    char *prefix = (char *)malloc(plen);
    snprintf(prefix, plen, "%s_", lowered);
    int result = 0;
    if (candidates && candidates->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(candidates); ++i) {
            json_t *it = json_get(candidates, i);
            if (!it || it->type != JSON_STRING || !it->str_val) continue;
            char *cl = lower_copy(it->str_val);
            if (!strcmp(cl, lowered) || !strncmp(cl, prefix, strlen(prefix))) { result = 1; free(cl); break; }
            free(cl);
        }
    }
    free(prefix); free(lowered);
    return json_bool(result ? true : false);
}

/* Inner has_evidence: blocks/pools exact-or-prefix match, env prefix match,
 * declared exact match against the uppercased env set. */
json_t *w14_platformac_7882b1_has_evidence(const char *name, json_t *blocks,
                                           json_t *pools, json_t *env,
                                           json_t *declared) {
    json_t *r1 = w14_platformac_7882b1_matches(name, blocks);
    json_t *r2 = w14_platformac_7882b1_matches(name, pools);
    int yes = json_bool_value(r1) || json_bool_value(r2);
    json_free(r1); json_free(r2);
    if (yes) return json_bool(true);

    /* prefix = name.upper().replace("-", "_") + "_" */
    char *up = upper_copy(name);
    for (char *p = up; *p; ++p) if (*p == '-') *p = '_';
    size_t plen = strlen(up) + 2;
    char *prefix = (char *)malloc(plen);
    snprintf(prefix, plen, "%s_", up);

    size_t ne = 0;
    char *upper_env[512];
    if (env && env->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(env) && ne < 512; ++i) {
            json_t *it = json_get(env, i);
            if (it && it->type == JSON_STRING && it->str_val) upper_env[ne++] = upper_copy(it->str_val);
        }
    }
    for (size_t i = 0; i < ne; ++i) {
        if (!strncmp(upper_env[i], prefix, strlen(prefix))) { yes = 1; break; }
    }
    if (!yes && declared && declared->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(declared); ++i) {
            json_t *it = json_get(declared, i);
            if (!it || it->type != JSON_STRING || !it->str_val) continue;
            char *d = upper_copy(it->str_val);
            for (size_t j = 0; j < ne; ++j) {
                if (!strcmp(d, upper_env[j])) { yes = 1; break; }
            }
            free(d);
            if (yes) break;
        }
    }
    for (size_t i = 0; i < ne; ++i) free(upper_env[i]);
    free(prefix); free(up);
    return json_bool(yes ? true : false);
}
