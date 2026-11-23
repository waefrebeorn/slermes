/*
 * voice_turn_route.c — C port of upstream Hermes agent/voice_turn_route.py.
 *
 * PoP: voiceturnr_route_target @ agent/voice_turn_route.py:_route_target
 * PoP: voiceturnr_voice_reasoning @ agent/voice_turn_route.py:_voice_reasoning
 *
 * The route-binding / aux-model / reasoning-floor collaborators are resolved
 * late in the Python source and are represented as explicit fixture seams here,
 * mirroring the stubs registered by tests/oracle_w8_voiceturnr_3c1c02.py.
 */
#include "noc_w8_voiceturnr_3c1c02.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}
static char *py_strip(const char *s) {
    if (!s) return xstrdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    char *r = (char *)malloc((size_t)(b - a) + 1);
    memcpy(r, a, (size_t)(b - a)); r[b - a] = '\0';
    return r;
}
static char *py_strip_lower(const char *s) {
    char *r = py_strip(s);
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}
/* str(value or "") for the JSON primitive values a config field may carry. */
static char *jstr_or_empty(json_t *v) {
    if (!v || v->type == JSON_NULL) return xstrdup("");
    if (v->type == JSON_STRING) return xstrdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return xstrdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        snprintf(b, sizeof(b), "%g", v->num_val);
        return xstrdup(b);
    }
    return xstrdup("");
}

json_t *voiceturnr_route_target(json_t *cfg,
    const char *agent_provider, const char *agent_model,
    const char *aux_model, const char *alias_provider, const char *alias_base) {
    json_t *cv = cfg ? json_obj_get(cfg, "provider") : NULL;
    json_t *cm = cfg ? json_obj_get(cfg, "model") : NULL;
    json_t *cb = cfg ? json_obj_get(cfg, "base_url") : NULL;
    char *provider = jstr_or_empty(cv);
    char *pv = py_strip_lower(provider); free(provider); provider = pv;
    char *mraw = jstr_or_empty(cm);
    char *model = py_strip(mraw); free(mraw);
    char *braw = jstr_or_empty(cb);
    char *base_url = py_strip(braw); free(braw);

    if (!strcmp(provider, "") || !strcmp(provider, "auto") || !strcmp(provider, "main")) {
        if (base_url[0]) {
            free(provider); provider = xstrdup("custom");
        } else if (model[0]) {
            char *ap = py_strip_lower(agent_provider);
            free(provider); provider = ap;
        } else {
            free(provider); free(model); free(base_url);
            return json_null();
        }
    }
    if (!model[0]) {
        free(model); model = xstrdup(aux_model ? aux_model : "");
    }
    if (!provider[0] || !model[0]) {
        free(provider); free(model); free(base_url);
        return json_null();
    }
    char *ap_lower = py_strip_lower(agent_provider);
    if (!base_url[0] && !strcmp(provider, ap_lower) &&
        !strcmp(model, agent_model ? agent_model : "")) {
        free(ap_lower); free(provider); free(model); free(base_url);
        return json_null();
    }
    free(ap_lower);

    json_t *out = cfg ? json_copy(cfg) : json_object();
    json_set(out, "provider", json_string(alias_provider ? alias_provider : provider));
    json_set(out, "model", json_string(model));
    json_set(out, "base_url", json_string(alias_base ? alias_base : ""));
    free(provider); free(model); free(base_url);
    return out;
}

json_t *voiceturnr_voice_reasoning(json_t *effort, const char *lowest) {
    json_t *disabled = NULL;
    if (effort && effort->type == JSON_OBJECT) {
        json_t *en = json_obj_get(effort, "enabled");
        if (en && en->type == JSON_BOOL && !en->bool_val) disabled = en;
    }
    if (!disabled) return effort ? json_copy(effort) : json_null();
    if (lowest && lowest[0]) {
        json_t *out = json_object();
        json_set(out, "enabled", json_bool(true));
        json_set(out, "effort", json_string(lowest));
        return out;
    }
    return json_copy(effort);
}
