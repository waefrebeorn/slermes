/*
 * auxiliary_fallback_recovery.c — C home for upstream Hermes agent/auxiliary_fallback_recovery.py.
 * STATUS: BEHAVIOR PORT — value->value parameter-rung parity, gate w25_auxiliaryf_40632a.
 * PoP: agent_auxiliary_fallback_recovery_pending @ agent/auxiliary_fallback_recovery.py:<module>
 * PoP: w25_auxiliaryf_40632a_reapply_repairs @ agent/auxiliary_fallback_recovery.py:reapply_repairs
 * PoP: w25_auxiliaryf_40632a_ladder_route @ agent/auxiliary_fallback_recovery.py:_parameter_ladder
 * PoP: w25_auxiliaryf_40632a_constants @ agent/auxiliary_profile_gates.py:PROFILE_REASONING_KEYS,
 *      agent/auxiliary_reasoning_floor.py:_DISABLED_EFFORTS,REASONING_FLOOR_EFFORT
 *
 * The client, the send, the provider's 400 and the ladder's drive loop are
 * seams: a fallback candidate used to get a single shot, and this module hands
 * it the parameter rungs the primary path already had.  Ported here is what the
 * values decide -- which strip rewrites a request (``None`` when nothing was on
 * the wire, so an unchanged request is never re-sent), and the route facts the
 * ladder is built from.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_auxiliaryf_40632a.h"

#define AF_FLOOR_EFFORT "low"

/* PROFILE_REASONING_KEYS (agent/auxiliary_profile_gates.py). */
static const char *const AF_PROFILE_REASONING_KEYS[W25_AUXILIARYF_40632A_PROFILE_REASONING_KEYS] = {
    "reasoning", "reasoning_effort", "thinking", "thinking_config", "thinkingconfig",
    "thinking_budget", "thinkingbudget", "enable_thinking", "think", "verbosity",
};

/* agent/auxiliary_reasoning_floor.py:_DISABLED_EFFORTS */
static const char *const AF_DISABLED_EFFORTS[W25_AUXILIARYF_40632A_DISABLED_EFFORTS] = {
    "none", "off", "disabled", "false", "0",
};

static int af_truthy(const json_t *v) {
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

static char *af_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str() over the scalars a kwargs value can hold; NULL for containers. */
static char *af_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return af_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return af_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return af_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return af_dup(buf);
    default: return NULL;
    }
}

/* str(x).strip().lower(), written into a caller buffer. */
static void af_norm(const char *src, char *out, size_t cap) {
    size_t i = 0, n;
    const char *start = src ? src : "";
    const char *end;
    while (*start && isspace((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    n = (size_t)(end - start);
    if (n >= cap) n = cap - 1;
    for (; i < n; i++) out[i] = (char)tolower((unsigned char)start[i]);
    out[n] = '\0';
}

static int af_ci_member(const char *value, const char *const *table, size_t n) {
    char norm[128];
    size_t i;
    af_norm(value, norm, sizeof(norm));
    for (i = 0; i < n; i++) if (!strcmp(norm, table[i])) return 1;
    return 0;
}

static int af_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void af_sort_strings(const char *const *table, size_t n) {
    qsort((void *)table, n, sizeof(*table), af_cmp_str);
}

/* _is_disabled(x): isinstance(x, dict) and x.get("enabled") is False. */
static int af_is_disabled(const json_t *v) {
    const json_t *enabled;
    if (!v || v->type != JSON_OBJECT) return 0;
    enabled = json_obj_get(v, "enabled");
    return enabled && enabled->type == JSON_BOOL && !enabled->bool_val;
}

/* A fresh object holding src's pairs except the named keys. */
static json_t *af_copy_without(const json_t *src, const char *const *drop, size_t ndrop) {
    json_t *out = json_object();
    size_t i, k;
    if (!src || src->type != JSON_OBJECT) return out;
    for (i = 0; i < json_object_size(src); i++) {
        const char *key = json_object_get_key_at(src, i);
        int skipped = 0;
        if (!key) continue;
        for (k = 0; k < ndrop; k++)
            if (!strcmp(key, drop[k])) { skipped = 1; break; }
        if (skipped) continue;
        json_set(out, key, json_copy(json_object_get_at(src, i)));
    }
    return out;
}

/* A fresh object holding src's pairs whose normalised key is not in the profile
 * reasoning table. */
static json_t *af_copy_without_profile_reasoning(const json_t *src) {
    json_t *out = json_object();
    size_t i;
    if (!src || src->type != JSON_OBJECT) return out;
    for (i = 0; i < json_object_size(src); i++) {
        const char *key = json_object_get_key_at(src, i);
        if (!key) continue;
        if (af_ci_member(key, AF_PROFILE_REASONING_KEYS, W25_AUXILIARYF_40632A_PROFILE_REASONING_KEYS)) continue;
        json_set(out, key, json_copy(json_object_get_at(src, i)));
    }
    return out;
}

static json_t *af_floor_config(void) {
    json_t *cfg = json_object();
    json_set(cfg, "enabled", json_bool(true));
    json_set(cfg, "effort", json_string(AF_FLOOR_EFFORT));
    return cfg;
}

/* _without_temperature: None when the field was not on the wire. */
static json_t *af_strip_temperature(const json_t *kwargs) {
    static const char *const drop[] = {"temperature"};
    if (!kwargs || kwargs->type != JSON_OBJECT) return NULL;
    if (!json_obj_get(kwargs, "temperature")) return NULL;
    return af_copy_without(kwargs, drop, 1);
}

/* _without_max_tokens: never None -- a route may hide the cap it translated. */
static json_t *af_strip_max_tokens(const json_t *kwargs) {
    static const char *const drop[] = {"max_tokens", "max_completion_tokens"};
    if (!kwargs || kwargs->type != JSON_OBJECT) return NULL;
    return af_copy_without(kwargs, drop, 2);
}

/* _without_structured_output_format: None when nothing was removed. */
static json_t *af_strip_structured_output(const json_t *kwargs) {
    static const char *const drop[] = {"response_format"};
    json_t *retry, *extra;
    int changed = 0;
    if (!kwargs || kwargs->type != JSON_OBJECT) return NULL;
    if (json_obj_get(kwargs, "response_format")) changed = 1;
    retry = af_copy_without(kwargs, drop, 1);
    extra = json_obj_get(retry, "extra_body");
    if (extra && extra->type == JSON_OBJECT && json_obj_get(extra, "response_format")) {
        json_t *remaining = af_copy_without(extra, drop, 1);
        if (json_object_size(remaining) > 0) {
            json_set(retry, "extra_body", remaining);
        } else {
            json_obj_del(retry, "extra_body");
            json_free(remaining);
        }
        changed = 1;
    }
    if (!changed) { json_free(retry); return NULL; }
    return retry;
}

/* with_reasoning_floor: step every thinking-OFF encoding up to the floor. */
static json_t *af_strip_reasoning_floor(const json_t *kwargs) {
    json_t *retry, *extra;
    int changed = 0;
    if (!kwargs || kwargs->type != JSON_OBJECT) return NULL;
    retry = json_copy(kwargs);

    {
        const json_t *effort = json_obj_get(retry, "reasoning_effort");
        char *text = effort ? af_pystr(effort) : af_dup("");
        if (text && af_ci_member(text, AF_DISABLED_EFFORTS, W25_AUXILIARYF_40632A_DISABLED_EFFORTS)) {
            json_set(retry, "reasoning_effort", json_string(AF_FLOOR_EFFORT));
            changed = 1;
        }
        free(text);
    }
    if (af_is_disabled(json_obj_get(retry, "_reasoning_config"))) {
        json_set(retry, "_reasoning_config", af_floor_config());
        changed = 1;
    }
    extra = json_obj_get(retry, "extra_body");
    if (extra && extra->type == JSON_OBJECT) {
        const json_t *reasoning = json_obj_get(extra, "reasoning");
        int lift = af_is_disabled(reasoning);
        if (!lift && reasoning && reasoning->type == JSON_OBJECT) {
            const json_t *effort = json_obj_get(reasoning, "effort");
            char *text = effort ? af_pystr(effort) : af_dup("");
            lift = text && af_ci_member(text, AF_DISABLED_EFFORTS, W25_AUXILIARYF_40632A_DISABLED_EFFORTS);
            free(text);
        }
        if (lift) {
            json_t *grown = json_copy(extra);
            json_set(grown, "reasoning", af_floor_config());
            json_set(retry, "extra_body", grown);
            changed = 1;
        }
    }
    if (!changed) { json_free(retry); return NULL; }
    return retry;
}

/* _without_reasoning_fields: drop every reasoning wire control. */
static json_t *af_strip_reasoning_fields(const json_t *kwargs) {
    json_t *retry, *extra;
    int changed = 0;
    if (!kwargs || kwargs->type != JSON_OBJECT) return NULL;
    retry = json_copy(kwargs);

    {
        const json_t *popped = json_obj_get(retry, "reasoning_effort");
        if (popped && popped->type != JSON_NULL) changed = 1;
        json_obj_del(retry, "reasoning_effort");
    }
    {
        const json_t *popped = json_obj_get(retry, "_reasoning_config");
        if (popped && popped->type != JSON_NULL) changed = 1;
        json_obj_del(retry, "_reasoning_config");
    }
    extra = json_obj_get(retry, "extra_body");
    if (extra && extra->type == JSON_OBJECT) {
        json_t *remaining = af_copy_without_profile_reasoning(extra);
        if (json_object_size(remaining) != json_object_size(extra)) {
            if (json_object_size(remaining) > 0) json_set(retry, "extra_body", remaining);
            else { json_obj_del(retry, "extra_body"); json_free(remaining); }
            changed = 1;
        } else {
            json_free(remaining);
        }
    }
    if (!changed) { json_free(retry); return NULL; }
    return retry;
}

static json_t *af_apply_strip(const char *name, const json_t *kwargs) {
    if (!name) return NULL;
    if (!strcmp(name, "temperature")) return af_strip_temperature(kwargs);
    if (!strcmp(name, "max_tokens")) return af_strip_max_tokens(kwargs);
    if (!strcmp(name, "structured_output")) return af_strip_structured_output(kwargs);
    if (!strcmp(name, "reasoning_floor")) return af_strip_reasoning_floor(kwargs);
    if (!strcmp(name, "reasoning")) return af_strip_reasoning_fields(kwargs);
    return NULL;
}

json_t *w25_auxiliaryf_40632a_reapply_repairs(const json_t *request) {
    const json_t *source = json_obj_get(request, "kwargs");
    const json_t *repairs = json_obj_get(request, "repairs");
    json_t *kwargs = (source && source->type == JSON_OBJECT) ? json_copy(source) : json_object();
    size_t i;

    if (repairs && repairs->type == JSON_ARRAY) {
        for (i = 0; i < json_len(repairs); i++) {
            const json_t *name = json_get(repairs, i);
            json_t *next;
            if (!name || name->type != JSON_STRING) continue;
            next = af_apply_strip(name->str_val, kwargs);
            if (!next) continue;              /* strip returned None -> kwargs */
            if (json_object_size(next) == 0) {
                /* ``strip(kwargs) or kwargs``: an empty rebuild is falsy too. */
                json_free(next);
                continue;
            }
            json_free(kwargs);
            kwargs = next;
        }
    }
    return kwargs;
}

json_t *w25_auxiliaryf_40632a_ladder_route(const json_t *request) {
    const json_t *kwargs = json_obj_get(request, "kwargs");
    const json_t *tag_value = json_obj_get(request, "tag");
    const json_t *task_value = json_obj_get(request, "task");
    const json_t *base_url_value = json_obj_get(request, "base_url");
    const json_t *client_base_value = json_obj_get(request, "client_base_url");
    const char *tag = tag_value && tag_value->type == JSON_STRING ? tag_value->str_val : "";
    const char *provider = json_get_str(request, "provider", "");
    json_t *out = json_object();
    json_t *max_tokens = NULL;

    /* max_tokens = kwargs.get("max_tokens") or kwargs.get("max_completion_tokens") */
    if (kwargs && kwargs->type == JSON_OBJECT) {
        const json_t *mt = json_obj_get(kwargs, "max_tokens");
        if (af_truthy(mt)) max_tokens = json_copy(mt);
        else {
            const json_t *mc = json_obj_get(kwargs, "max_completion_tokens");
            max_tokens = mc ? json_copy(mc) : json_null();
        }
        {
            const json_t *model = json_obj_get(kwargs, "model");
            json_set(out, "final_model", model ? json_copy(model) : json_null());
        }
    } else {
        max_tokens = json_null();
        json_set(out, "final_model", json_null());
    }
    json_set(out, "max_tokens", max_tokens);

    /* base_info = base_url or str(getattr(client, "base_url", "") or "") */
    if (af_truthy(base_url_value) && base_url_value->type == JSON_STRING) {
        json_set(out, "base_info", json_copy(base_url_value));
    } else if (client_base_value && client_base_value->type == JSON_STRING && client_base_value->str_val
               && client_base_value->str_val[0]) {
        json_set(out, "base_info", json_copy(client_base_value));
    } else {
        json_set(out, "base_info", json_string(""));
    }
    json_set(out, "async_mode", json_bool(tag[0] != '\0'));
    json_set(out, "tag", json_string(tag));
    json_set(out, "task", task_value ? json_copy(task_value) : json_null());
    json_set(out, "resolved_provider", json_string(provider));
    return out;
}

json_t *w25_auxiliaryf_40632a_constants(void) {
    json_t *out = json_object();
    json_t *keys = json_array();
    json_t *efforts = json_array();
    const char *sorted_keys[W25_AUXILIARYF_40632A_PROFILE_REASONING_KEYS];
    const char *sorted_efforts[W25_AUXILIARYF_40632A_DISABLED_EFFORTS];
    size_t i;

    /* The live tables are sets: emit them in a stable (sorted) order. */
    for (i = 0; i < W25_AUXILIARYF_40632A_PROFILE_REASONING_KEYS; i++) sorted_keys[i] = AF_PROFILE_REASONING_KEYS[i];
    for (i = 0; i < W25_AUXILIARYF_40632A_DISABLED_EFFORTS; i++) sorted_efforts[i] = AF_DISABLED_EFFORTS[i];
    af_sort_strings(sorted_keys, W25_AUXILIARYF_40632A_PROFILE_REASONING_KEYS);
    af_sort_strings(sorted_efforts, W25_AUXILIARYF_40632A_DISABLED_EFFORTS);
    for (i = 0; i < W25_AUXILIARYF_40632A_PROFILE_REASONING_KEYS; i++)
        json_append(keys, json_string(sorted_keys[i]));
    for (i = 0; i < W25_AUXILIARYF_40632A_DISABLED_EFFORTS; i++)
        json_append(efforts, json_string(sorted_efforts[i]));
    json_set(out, "profile_reasoning_keys", keys);
    json_set(out, "disabled_efforts", efforts);
    json_set(out, "reasoning_floor_effort", json_string(AF_FLOOR_EFFORT));
    return out;
}
