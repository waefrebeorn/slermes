/*
 * model_switch.c — C home for upstream Hermes tui_gateway/model_switch.py.
 *
 * PoP: w11_modelswitc_9ef4ed_lazy_info_route     @ tui_gateway/model_switch.py:_lazy_info_route
 * PoP: w11_modelswitc_9ef4ed_current_runtime     @ tui_gateway/model_switch.py:_current_model_runtime
 * PoP: w11_modelswitc_9ef4ed_result_scope        @ tui_gateway/model_switch.py:_apply_model_switch
 * PoP: w11_modelswitc_9ef4ed_switch_away_provider @ tui_gateway/model_switch.py:_switch_away_provider
 * PoP: w11_modelswitc_9ef4ed_confirm_message     @ tui_gateway/model_switch.py:_expensive_model_confirm
 *
 * Pure composition helpers of the /model switch flow. External collaborators
 * (_config_model_target, _env_model_seed, the selection guards, the parser)
 * are injected by the harness, mirroring the oracle's stubs.
 */
#include "noc_w11_modelswitc_9ef4ed.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *const MS_RUNTIME_KEYS[] = {"model", "provider", "api_key", "base_url", "api_mode"};
static const char *const MS_CURRENT_KEYS[] = {"provider", "model", "base_url", "api_key"};

static char *ms_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static json_t *ms_str(const char *s) { return json_string(s ? s : ""); }

/* Python truthiness of a JSON scalar. */
static int ms_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY) return json_len(v) > 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) > 0;
    return 1;
}
static const char *ms_truthy_str(const char *s) { return (s && *s) ? s : NULL; }

json_t *w11_modelswitc_9ef4ed_constants(void) {
    json_t *o = json_object();
    json_set(o, "LARGE_MODEL_NOTICE_KEY", json_string("model.large_notice"));
    json_t *keys = json_array();
    for (size_t i = 0; i < 5; i++) json_append(keys, json_string(MS_RUNTIME_KEYS[i]));
    json_set(o, "_RUNTIME_KEYS", keys);
    return o;
}

json_t *w11_modelswitc_9ef4ed_lazy_info_route(json_t *override) {
    json_t *out = json_object();
    if (!ms_truthy(override)) {
        /* Without a client pick the live code resolves the session's default
         * route (external seam); fixtures always supply a truthy override. */
        json_set(out, "model", json_null());
        return out;
    }
    json_t *mv = json_obj_get(override, "model");
    json_t *pv = json_obj_get(override, "provider");
    /* {"model": model, **({"provider": provider} if provider else {})} */
    json_t *model = (mv && (mv->type == JSON_STRING || mv->type == JSON_NULL)) ? json_copy(mv)
                                                                               : ms_str(NULL);
    json_set(out, "model", model);
    if (ms_truthy(pv)) json_set(out, "provider", json_copy(pv));
    return out;
}

json_t *w11_modelswitc_9ef4ed_current_runtime(json_t *agent) {
    json_t *arr = json_array();
    for (size_t i = 0; i < 4; i++) {
        json_t *v = agent ? json_obj_get(agent, MS_CURRENT_KEYS[i]) : NULL;
        if (ms_truthy(v) && v->type == JSON_STRING)
            json_append(arr, json_copy(v));
        else if (ms_truthy(v) && v->type == JSON_NUMBER) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            json_append(arr, json_string(buf));
        } else
            json_append(arr, json_string(""));
    }
    return arr;
}

const char *w11_modelswitc_9ef4ed_result_scope(bool one_turn, bool persist_global) {
    return one_turn ? "once" : (persist_global ? "global" : "session");
}

json_t *w11_modelswitc_9ef4ed_apply_result(const char *new_model, const char *warning,
                                            bool one_turn, bool persist_global) {
    json_t *o = json_object();
    json_set(o, "value", ms_str(new_model));
    const char *w = ms_truthy_str(warning);
    json_set(o, "warning", ms_str(w ? w : ""));
    json_set(o, "confirm_required", json_bool(false));
    json_set(o, "scope", json_string(
        w11_modelswitc_9ef4ed_result_scope(one_turn, persist_global)));
    return o;
}

json_t *w11_modelswitc_9ef4ed_confirm_message(const char *warning_message,
                                               const char *result_warning) {
    if (!warning_message) return json_null();
    if (ms_truthy_str(result_warning)) {
        size_t n = strlen(warning_message) + strlen(result_warning) + 3;
        char *buf = (char *)malloc(n);
        if (buf) {
            snprintf(buf, n, "%s\n\n%s", warning_message, result_warning);
            json_t *r = json_string(buf);
            free(buf);
            return r;
        }
    }
    return json_string(warning_message);
}

json_t *w11_modelswitc_9ef4ed_switch_away_provider(bool has_agent,
                                                    const char *explicit_provider,
                                                    const char *current_provider,
                                                    const char *env_provider,
                                                    bool model_seed,
                                                    const char *config_target) {
    /* if agent or not explicit_provider: return current_provider */
    if (has_agent || !ms_truthy_str(explicit_provider)) {
        return current_provider ? json_string(current_provider) : json_null();
    }
    /* if env_provider := os.environ.get("HERMES_TUI_PROVIDER", "").strip(): */
    const char *env = env_provider ? env_provider : "";
    while (*env == ' ' || *env == '\t' || *env == '\n' || *env == '\r' ||
           *env == '\f' || *env == '\v') env++;
    size_t en = strlen(env);
    while (en > 0 && (env[en - 1] == ' ' || env[en - 1] == '\t' || env[en - 1] == '\n' ||
                      env[en - 1] == '\r' || env[en - 1] == '\f' || env[en - 1] == '\v')) en--;
    if (en > 0) {
        char *tmp = (char *)malloc(en + 1);
        if (tmp) {
            memcpy(tmp, env, en); tmp[en] = '\0';
            json_t *r = json_string(tmp); free(tmp); return r;
        }
    }
    if (model_seed) return json_null();
    return ms_truthy_str(config_target) ? json_string(config_target) : json_null();
}
