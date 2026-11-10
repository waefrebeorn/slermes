/*
 * port_gateway_platform_activation.c — pure core of gateway/platform_activation.py.
 */

#include "port_gateway_platform_activation.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

static int add_unique(json_t *arr, const char *key)
{
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *e = arr->c.items[i];
        if (e && e->type == JSON_STRING && strcmp(e->str_val, key) == 0)
            return 0;
    }
    json_append(arr, json_string(key));
    return 1;
}

static void collect_dict_keys(const json_t *parent, json_t *out)
{
    if (!parent || parent->type != JSON_OBJECT)
        return;
    for (size_t i = 0; i < parent->c.count; i++)
        if (parent->c.items[i] && parent->c.items[i]->type == JSON_OBJECT)
            add_unique(out, parent->c.keys[i]);
}

/* PoP: pa_yaml_block_names @ gateway/platform_activation.py:_yaml_block_names */
json_t *pa_yaml_block_names(const char *yaml_cfg_json)
{
    json_t *out = json_array();
    json_t *yaml = yaml_cfg_json ? json_parse(yaml_cfg_json, NULL) : NULL;
    if (!yaml || yaml->type != JSON_OBJECT) {
        if (yaml) json_free(yaml);
        return out;
    }
    json_t *gw = json_obj_get(yaml, "gateway");
    if (gw && gw->type != JSON_OBJECT) gw = NULL;
    json_t *parents[4] = {
        yaml,
        json_obj_get(yaml, "platforms"),
        gw,
        gw ? json_obj_get(gw, "platforms") : NULL,
    };
    for (int i = 0; i < 4; i++)
        collect_dict_keys(parents[i], out);
    json_free(yaml);
    return out;
}

static char *lowerdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}

/* PoP: pa_matches @ gateway/platform_activation.py:_matches */
int pa_matches(const char *name, const json_t *candidates)
{
    if (!name || !candidates || candidates->type != JSON_ARRAY)
        return 0;
    char *lowered = lowerdup(name);
    if (!lowered) return 0;
    size_t nlen = strlen(lowered);
    int match = 0;
    for (size_t i = 0; i < candidates->c.count && !match; i++) {
        json_t *c = candidates->c.items[i];
        if (!c || c->type != JSON_STRING || !c->str_val) continue;
        char *cl = lowerdup(c->str_val);
        if (!cl) continue;
        size_t clen = strlen(cl);
        if (strcmp(cl, lowered) == 0) {
            match = 1;
        } else if (clen > nlen && strncmp(cl, lowered, nlen) == 0 && cl[nlen] == '_') {
            match = 1;
        }
        free(cl);
    }
    free(lowered);
    return match;
}

/* PoP: pa_has_activation_evidence @ gateway/platform_activation.py:activation_predicate */
int pa_has_activation_evidence(const char *name,
                               const json_t *blocks, const json_t *pools,
                               const json_t *upper_env, const json_t *declared)
{
    if (!name) return 0;
    if (pa_matches(name, blocks) || pa_matches(name, pools))
        return 1;
    /* prefix = name.upper().replace('-','_') + '_' */
    size_t nlen = strlen(name);
    char *prefix = (char *)malloc(nlen + 2);
    if (!prefix) return 0;
    for (size_t i = 0; i < nlen; i++)
        prefix[i] = (name[i] == '-') ? '_' : (char)toupper((unsigned char)name[i]);
    prefix[nlen] = '_';
    prefix[nlen + 1] = '\0';
    if (upper_env && upper_env->type == JSON_ARRAY) {
        for (size_t i = 0; i < upper_env->c.count; i++) {
            json_t *e = upper_env->c.items[i];
            if (!e || e->type != JSON_STRING || !e->str_val) continue;
            if (strncmp(e->str_val, prefix, strlen(prefix)) == 0) {
                free(prefix);
                return 1;
            }
        }
    }
    if (declared && declared->type == JSON_ARRAY) {
        for (size_t i = 0; i < declared->c.count; i++) {
            json_t *d = declared->c.items[i];
            if (!d || d->type != JSON_STRING || !d->str_val) continue;
            size_t dl = strlen(d->str_val);
            char *up = (char *)malloc(dl + 1);
            if (!up) continue;
            for (size_t k = 0; k < dl; k++)
                up[k] = (char)toupper((unsigned char)d->str_val[k]);
            up[dl] = '\0';
            if (upper_env && upper_env->type == JSON_ARRAY) {
                for (size_t j = 0; j < upper_env->c.count; j++) {
                    json_t *en = upper_env->c.items[j];
                    if (en && en->type == JSON_STRING && en->str_val &&
                        strcmp(en->str_val, up) == 0) {
                        free(up); free(prefix);
                        return 1;
                    }
                }
            }
            free(up);
        }
    }
    free(prefix);
    return 0;
}
