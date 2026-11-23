/*
 * cli_config_load.c — faithful C11 port of two pure helpers in
 * hermes_cli/cli_config_load.py (gate w13_cliconfig_2dca4f).
 * PoP: w13_cliconfig_2dca4f_merge_file_config @ hermes_cli/cli_config_load.py:_merge_file_config
 * PoP: w13_cliconfig_2dca4f_resolve_prefill_messages_file @ hermes_cli/cli_config_load.py:_resolve_prefill_messages_file
 *
 * The env-dependent helper uses an explicit env_value seam instead of mutating
 * the process environment (Windows CRT setenv("","") unsets; Python keeps it).
 */
#include "noc_w13_cliconfig_2dca4f.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int json_falsy(const json_t *v) {
    if (!v) return 1;
    switch (v->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return !v->bool_val;
    case JSON_NUMBER: return v->num_val == 0.0;
    case JSON_STRING: return !v->str_val || v->str_val[0] == '\0';
    case JSON_ARRAY: case JSON_OBJECT: return v->c.count == 0;
    }
    return 1;
}

/* Python str(v or "").strip() for the scalar shapes configs use. */
static char *cfg_str_value(const json_t *v) {
    if (json_falsy(v)) return strdup("");
    char *raw;
    if (v->type == JSON_STRING) raw = strdup(v->str_val ? v->str_val : "");
    else if (v->type == JSON_BOOL) raw = strdup(v->bool_val ? "True" : "False");
    else if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9e15)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        raw = strdup(buf);
    } else {
        char *s = json_dumps(v, 0);
        raw = s ? s : strdup("");
    }
    char *a = raw;
    while (*a && isspace((unsigned char)*a)) a++;
    char *b = raw + strlen(raw);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *r = (char *)malloc((size_t)(b - a) + 1);
    memcpy(r, a, (size_t)(b - a));
    r[b - a] = '\0';
    free(raw);
    return r;
}

static void shallow_update(json_t *dst, const json_t *src) {
    if (!dst || !src || dst->type != JSON_OBJECT || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < json_object_size(src); i++)
        json_set(dst, json_object_get_key_at(src, i),
                 json_copy(json_object_get_at(src, i)));
}

json_t *w13_cliconfig_2dca4f_merge_file_config(const json_t *defaults,
                                               const json_t *file_config) {
    json_t *d = defaults ? json_copy(defaults) : json_object();
    if (!file_config || file_config->type != JSON_OBJECT) return d;

    /* model: str -> default; dict -> update + legacy model.model promotion. */
    json_t *fm = json_obj_get(file_config, "model");
    if (fm) {
        json_t *dm = json_obj_get(d, "model");
        if (!dm || dm->type != JSON_OBJECT) {
            dm = json_object();
            json_set(d, "model", dm);
        }
        if (fm->type == JSON_STRING) {
            json_set(dm, "default", json_string(fm->str_val ? fm->str_val : ""));
        } else if (fm->type == JSON_OBJECT) {
            shallow_update(dm, fm);
            json_t *inner = json_obj_get(fm, "model");
            if (inner && !json_obj_get(fm, "default"))
                json_set(dm, "default", json_copy(inner));
        }
    }

    /* Deep-merge dict sections, overwrite scalars; None section is kept. */
    for (size_t i = 0; i < json_object_size(file_config); i++) {
        const char *key = json_object_get_key_at(file_config, i);
        if (!key || !strcmp(key, "model")) continue;
        json_t *value = json_object_get_at(file_config, i);
        json_t *existing = json_obj_get(d, key);
        if (existing && existing->type == JSON_OBJECT) {
            if (value && value->type == JSON_OBJECT)
                shallow_update(existing, value);
            else if (value && value->type != JSON_NULL)
                json_set(d, key, json_copy(value));
        } else {
            json_set(d, key, json_copy(value));
        }
    }

    /* Legacy root-level max_turns -> agent.max_turns when nested key absent/None. */
    json_t *root_mt = json_obj_get(file_config, "max_turns");
    if (root_mt) {
        int nested_present = 0;
        json_t *af = json_obj_get(file_config, "agent");
        if (af && af->type == JSON_OBJECT) {
            json_t *amt = json_obj_get(af, "max_turns");
            nested_present = amt && amt->type != JSON_NULL;
        }
        if (!nested_present) {
            json_t *da = json_obj_get(d, "agent");
            if (!da || da->type != JSON_OBJECT) {
                da = json_object();
                json_set(d, "agent", da);
            }
            json_set(da, "max_turns", json_copy(root_mt));
        }
    }
    return d;
}

char *w13_cliconfig_2dca4f_resolve_prefill_messages_file(const char *env_value,
                                                         const json_t *config) {
    /* Python: session_env("HERMES_PREFILL_MESSAGES_FILE").strip(). */
    const char *a = env_value ? env_value : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    if (b > a) {
        char *r = (char *)malloc((size_t)(b - a) + 1);
        memcpy(r, a, (size_t)(b - a));
        r[b - a] = '\0';
        return r;
    }

    char *top = config ? cfg_str_value(json_obj_get(config, "prefill_messages_file")) : strdup("");
    if (top[0]) return top;
    free(top);

    json_t *agent = config ? json_obj_get(config, "agent") : NULL;
    if (agent && agent->type == JSON_OBJECT) {
        char *legacy = cfg_str_value(json_obj_get(agent, "prefill_messages_file"));
        if (legacy[0]) return legacy;
        free(legacy);
    }
    return strdup("");
}

