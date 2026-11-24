/*
 * methods_session_model_guard.c -- real C11 port of the composer overrides and
 * model x provider routing of upstream Hermes
 * tui_gateway/methods_session_model_guard.py.
 *
 * PoP: w21_methodsses_5b7963_str_or_empty @ tui_gateway/methods_session_model_guard.py:create_overrides
 * PoP: w21_methodsses_5b7963_truthy @ utils/env.py:is_truthy_value
 * PoP: w21_methodsses_5b7963_reasoning @ hermes_constants.py:parse_reasoning_effort
 * PoP: w21_methodsses_5b7963_service_tier @ agent/fast_mode.py:parse_exact_service_tier
 * PoP: w21_methodsses_5b7963_model_override @ tui_gateway/methods_session_model_guard.py:model_override_conflict
 * PoP: w21_methodsses_5b7963_create_overrides @ tui_gateway/methods_session_model_guard.py:create_overrides
 *
 * `static_model_provider_conflict` (curated catalogs + difflib) is a different
 * module; this port decides the pair it is asked about and the override triple.
 */
#include "hermes_json.h"
#include "noc_w21_methodsses_5b7963.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* hermes_constants.py: VALID_REASONING_EFFORTS */
static const char *const MS_REASONING_EFFORTS[] = {
    "minimal", "low", "medium", "high", "xhigh", "max", "ultra", NULL
};
/* agent/fast_mode.py: NORMAL_TIER_WORDS */
static const char *const MS_NORMAL_TIER_WORDS[] = {
    "normal", "default", "standard", "off", "none", NULL
};
/* agent/fast_mode.py: SERVICE_TIER_WORDS */
typedef struct { const char *word; const char *tier; } ms_tier_row;
static const ms_tier_row MS_SERVICE_TIER_WORDS[] = {
    {"fast",      "priority"},
    {"priority",  "priority"},
    {"on",        "priority"},
    {"ultrafast", "ultrafast"},
    {"ultra",     "ultrafast"},
    {"auto",      "auto"},
    {"cold",      "cold"},
    {NULL, NULL}
};
/* utils/env.py: TRUTHY_STRINGS */
static const char *const MS_TRUTHY_STRINGS[] = {"1", "true", "yes", "on", NULL};

/* --- helpers ------------------------------------------------------------- */

static char *ms_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python str() for the scalar shapes a create param carries. */
static char *ms_str_of(const json_t *v) {
    char buf[64];
    if (!v) return ms_dup("None");
    switch (v->type) {
    case JSON_NULL:   return ms_dup("None");
    case JSON_BOOL:   return ms_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return ms_dup(buf);
    case JSON_STRING: return ms_dup(v->str_val ? v->str_val : "");
    case JSON_ARRAY:  return ms_dup("[]");
    case JSON_OBJECT: return ms_dup("{}");
    }
    return ms_dup("");
}

static int ms_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* str.strip() over ASCII whitespace, in place; returns the start pointer. */
static char *ms_strip_inplace(char *s) {
    size_t n;
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v') start++;
    n = strlen(start);
    while (n && (start[n - 1] == ' ' || start[n - 1] == '\t' || start[n - 1] == '\n' ||
                 start[n - 1] == '\r' || start[n - 1] == '\f' || start[n - 1] == '\v'))
        start[--n] = '\0';
    return start;
}

static char *ms_lower(char *s) {
    size_t i;
    for (i = 0; s[i]; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
    return s;
}

static int ms_in_list(const char *value, const char *const *list) {
    int i;
    for (i = 0; list[i]; i++)
        if (!strcmp(value, list[i])) return 1;
    return 0;
}

/* --- str(v or "").strip() ------------------------------------------------ */

char *w21_methodsses_5b7963_str_or_empty(const json_t *value) {
    char *raw = ms_truthy(value) ? ms_str_of(value) : ms_dup("");
    char *start;
    char *out;
    if (!raw) return NULL;
    start = ms_strip_inplace(raw);
    out = ms_dup(start);
    free(raw);
    return out;
}

int w21_methodsses_5b7963_truthy(const json_t *value) {
    char *base, *text;
    int result;
    if (!value || value->type == JSON_NULL) return 0;      /* None -> default */
    if (value->type != JSON_STRING) return ms_truthy(value);
    base = ms_dup(value->str_val ? value->str_val : "");
    if (!base) return 0;
    text = ms_lower(ms_strip_inplace(base));
    result = ms_in_list(text, MS_TRUTHY_STRINGS);
    free(base);
    return result;
}

/* --- parse_reasoning_effort ---------------------------------------------- */

json_t *w21_methodsses_5b7963_reasoning(const json_t *effort) {
    if (!effort || effort->type == JSON_NULL) return json_null();          /* None */
    if (effort->type == JSON_BOOL && effort->bool_val) return json_null(); /* True */

    if (effort->type == JSON_OBJECT) {
        json_t *enabled = json_obj_get(effort, "enabled");
        json_t *level_value;
        char *level;
        json_t *out;
        if (enabled && enabled->type == JSON_BOOL && !enabled->bool_val) {
            out = json_object();
            json_set(out, "enabled", json_bool(0));
            return out;
        }
        level_value = json_obj_get(effort, "effort");
        level = ms_truthy(level_value) ? ms_str_of(level_value) : ms_dup("");
        if (!level) return json_null();
        ms_strip_inplace(level);
        if (!level[0]) { free(level); return json_null(); }
        out = json_object();
        json_set(out, "enabled", json_bool(1));
        json_set(out, "effort", json_string(level));
        free(level);
        return out;
    }
    {
        char *base = ms_str_of(effort);
        char *text;
        json_t *out;
        if (!base) return json_null();
        text = ms_lower(ms_strip_inplace(base));
        if (!strcmp(text, "none") || !strcmp(text, "false") || !strcmp(text, "disabled")) {
            free(base);
            out = json_object();
            json_set(out, "enabled", json_bool(0));
            return out;
        }
        if (ms_in_list(text, MS_REASONING_EFFORTS)) {
            out = json_object();
            json_set(out, "enabled", json_bool(1));
            json_set(out, "effort", json_string(text));
            free(base);
            return out;
        }
        free(base);
        return json_null();
    }
}

/* --- parse_exact_service_tier -------------------------------------------- */

json_t *w21_methodsses_5b7963_service_tier(const json_t *raw) {
    char *base = w21_methodsses_5b7963_str_or_empty(raw);    /* str(raw or "").strip() */
    char *lowered;
    json_t *out;
    if (!base) return json_null();
    lowered = ms_lower(base);
    if (lowered[0] && !ms_in_list(lowered, MS_NORMAL_TIER_WORDS)) {   /* "" is a NORMAL_TIER_WORD */
        int i, found = 0;
        for (i = 0; MS_SERVICE_TIER_WORDS[i].word; i++)
            if (!strcmp(lowered, MS_SERVICE_TIER_WORDS[i].word)) { found = 1; break; }
        if (!found) {
            size_t need = strlen(lowered) + 40;
            char *msg = (char *)malloc(need);
            out = json_object();
            json_set(out, "raised", json_string("ValueError"));
            if (msg) snprintf(msg, need, "unknown service tier: %s", lowered);
            json_set(out, "message", json_string(msg ? msg : ""));
            free(msg);
            free(base);
            return out;
        }
        out = json_object();
        json_set(out, "value", json_string(MS_SERVICE_TIER_WORDS[i].tier));
        free(base);
        return out;
    }
    out = json_object();
    json_set(out, "value", json_string(""));                 /* parse_service_tier(value) or "" */
    free(base);
    return out;
}

/* --- model_override_conflict -------------------------------------------- */

json_t *w21_methodsses_5b7963_model_override(const json_t *params, const char *scope_provider,
                                             const json_t *conflict) {
    char *model = w21_methodsses_5b7963_str_or_empty(json_obj_get(params, "model"));
    char *provider;
    json_t *out;
    int used_scope;

    if (!model) return json_null();
    out = json_object();
    if (!model[0]) {
        json_set(out, "model", json_string(""));
        json_set(out, "checked", json_bool(0));
        free(model);
        return out;
    }
    provider = w21_methodsses_5b7963_str_or_empty(json_obj_get(params, "provider"));
    used_scope = (provider && provider[0]) ? 0 : 1;
    if (used_scope) {                                        /* with build_scope: ... */
        free(provider);
        provider = ms_dup(scope_provider ? scope_provider : "");
    }
    json_set(out, "model", json_string(model));
    json_set(out, "provider", json_string(provider ? provider : ""));
    json_set(out, "used_scope", json_bool(used_scope));
    json_set(out, "checked", json_bool(1));
    /* `return static_model_provider_conflict(model, provider)` -- the record is
     * that other module's answer; the port echoes it so the return is compared. */
    json_set(out, "result", conflict ? json_copy(conflict) : json_null());
    free(provider);
    free(model);
    return out;
}

/* --- create_overrides ---------------------------------------------------- */

json_t *w21_methodsses_5b7963_create_overrides(const json_t *params) {
    char *model = w21_methodsses_5b7963_str_or_empty(json_obj_get(params, "model"));
    char *effort = w21_methodsses_5b7963_str_or_empty(json_obj_get(params, "reasoning_effort"));
    json_t *out, *model_override, *reasoning_override, *tier_override;

    if (!model || !effort) { free(model); free(effort); return json_null(); }

    model_override = json_null();
    if (model[0]) {
        char *provider = w21_methodsses_5b7963_str_or_empty(json_obj_get(params, "provider"));
        model_override = json_object();
        json_set(model_override, "model", json_string(model));
        json_set(model_override, "provider",
                 (provider && provider[0]) ? json_string(provider) : json_null());
        free(provider);
    }

    reasoning_override = json_null();
    if (effort[0]) {
        json_t *parsed = w21_methodsses_5b7963_reasoning(json_string(effort));
        if (parsed) { json_free(reasoning_override); reasoning_override = parsed; }
    }

    tier_override = json_null();
    {
        json_t *tier_value = json_obj_get(params, "service_tier");
        if (tier_value && tier_value->type != JSON_NULL) {
            json_t *parsed = w21_methodsses_5b7963_service_tier(tier_value);
            json_t *raised = parsed ? json_obj_get(parsed, "raised") : NULL;
            if (raised) {
                /* `parse_exact_service_tier` raises out of create_overrides */
                out = json_object();
                json_set(out, "status", json_string("error"));
                json_set(out, "type", json_string("ValueError"));
                json_set(out, "message", json_copy(json_obj_get(parsed, "message")));
                json_free(parsed);
                json_free(model_override);
                json_free(reasoning_override);
                free(model);
                free(effort);
                return out;
            }
            if (parsed) {
                json_t *value = json_obj_get(parsed, "value");
                json_free(tier_override);
                tier_override = value ? json_copy(value) : json_string("");
            }
            json_free(parsed);
        } else if (json_obj_get(params, "fast")) {   /* "fast" in params */
            tier_override = json_string(w21_methodsses_5b7963_truthy(json_obj_get(params, "fast"))
                                        ? "priority" : "");
        }
    }

    out = json_array();
    json_append(out, json_copy(model_override));
    json_append(out, json_copy(reasoning_override));
    json_append(out, json_copy(tier_override));
    json_free(model_override);
    json_free(reasoning_override);
    json_free(tier_override);
    free(model);
    free(effort);
    return out;
}
