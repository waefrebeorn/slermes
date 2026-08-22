/*
 * model_selection_guards.c — C11 port of
 * hermes_cli/model_selection_guards.py.
 *
 * Single evaluation point for selection-time guards. _GUARDS is evaluated in
 * order (cost, then data_policy); individual guard exceptions are swallowed
 * so a misbehaving guard never breaks model selection.
 */
#define _POSIX_C_SOURCE 200809L
#include "model_selection_guards.h"
#include "model_data_policy_guard.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static char *msg_strip_dup(const char *s)
{
    char *d = strdup(s ? s : "");
    if (!d)
        return NULL;
    size_t n = strlen(d), start = 0, end = n;
    while (start < end && (unsigned char)d[start] <= ' ')
        start++;
    while (end > start && (unsigned char)d[end - 1] <= ' ')
        end--;
    memmove(d, d + start, end - start);
    d[end - start] = '\0';
    return d;
}

/* Build a SelectionWarning JSON object. */
static json_t *selection_warning_new(const char *kind, const char *title,
                                     const char *model, const char *provider,
                                     const char *message)
{
    json_t *o = json_object();
    json_set(o, "kind", json_string(kind));
    json_set(o, "title", json_string(title));
    json_set(o, "model", json_string(model));
    json_set(o, "provider", json_string(provider));
    json_set(o, "message", json_string(message));
    return o;
}

/* PoP: _cost_guard @ hermes_cli/model_selection_guards.py:_cost_guard */
static json_t *_cost_guard(const char *model_name, const char *provider,
                           const char *base_url, const char *api_key,
                           const json_t *model_info,
                           const model_cost_guard_seams *seams)
{
    ExpensiveModelWarning *warning = expensive_model_warning(
        model_name, provider, base_url, api_key, model_info, seams);
    if (!warning)
        return NULL;
    /* getattr(warning, "model"/"provider", default) — our payload always
     * carries both fields, matching the dataclass. */
    json_t *out = selection_warning_new(
        "cost", "Expensive Model Warning",
        warning->model ? warning->model : (model_name ? model_name : ""),
        warning->provider ? warning->provider : (provider ? provider : ""),
        warning->message);
    expensive_model_warning_free(warning);
    return out;
}

/* PoP: _data_policy_guard @ hermes_cli/model_selection_guards.py:_data_policy_guard */
static json_t *_data_policy_guard(const char *model_name, const char *provider,
                                  const char *base_url)
{
    DataTrainingWarning *warning =
        data_training_warning(model_name, provider, base_url);
    if (!warning)
        return NULL;
    json_t *out = selection_warning_new(
        "data_policy", "Data-Training Tier Warning",
        warning->model ? warning->model : (model_name ? model_name : ""),
        warning->provider ? warning->provider : (provider ? provider : ""),
        warning->message);
    data_training_warning_free(warning);
    return out;
}

/* Registry, evaluated in order (_GUARDS). */

/* PoP: selection_warnings @ hermes_cli/model_selection_guards.py:selection_warnings */
json_t *selection_warnings(const char *model_name, const char *provider,
                           const char *base_url, const char *api_key,
                           const json_t *model_info,
                           const json_t *include_kinds,
                           const model_cost_guard_seams *seams)
{
    json_t *results = json_array();
    json_t *warnings[2];
    const char *kinds[2] = { "cost", "data_policy" };

    warnings[0] = _cost_guard(model_name, provider, base_url, api_key,
                              model_info, seams);
    warnings[1] = _data_policy_guard(model_name, provider, base_url);

    for (int i = 0; i < 2; i++) {
        json_t *w = warnings[i];
        if (!w)
            continue;   /* guard returned None / raised */
        int wanted_all = (include_kinds == NULL || include_kinds->type != JSON_ARRAY);
        int wanted = wanted_all;
        if (!wanted_all) {
            for (size_t k = 0; k < json_len(include_kinds); k++) {
                json_t *kk = json_get(include_kinds, k);
                if (kk && kk->type == JSON_STRING
                    && strcmp(kk->str_val, kinds[i]) == 0) {
                    wanted = 1;
                    break;
                }
            }
        }
        if (wanted)
            json_append(results, w);
        else
            json_free(w);
    }
    return results;
}

/* PoP: combined_message @ hermes_cli/model_selection_guards.py:combined_message */
char *combined_message(const json_t *warnings)
{
    if (!warnings || warnings->type != JSON_ARRAY)
        return strdup("");
    size_t total = 1;
    size_t n = json_len(warnings);
    for (size_t i = 0; i < n; i++) {
        json_t *w = json_get(warnings, i);
        json_t *m = w ? json_obj_get(w, "message") : NULL;
        total += ((m && m->type == JSON_STRING) ? strlen(m->str_val) : 0) + 2 + 1;
    }
    char *out = malloc(total);
    if (!out)
        return NULL;
    out[0] = '\0';
    size_t off = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *w = json_get(warnings, i);
        json_t *m = w ? json_obj_get(w, "message") : NULL;
        if (i > 0) {
            memcpy(out + off, "\n\n", 2);
            off += 2;
        }
        if (m && m->type == JSON_STRING) {
            size_t l = strlen(m->str_val);
            memcpy(out + off, m->str_val, l);
            off += l;
        }
    }
    out[off] = '\0';
    return out;
}

/* PoP: combined_selection_warning @ hermes_cli/model_selection_guards.py:combined_selection_warning */
json_t *combined_selection_warning(const char *model_name, const char *provider,
                                   const char *base_url, const char *api_key,
                                   const json_t *model_info,
                                   const model_cost_guard_seams *seams)
{
    json_t *warnings = selection_warnings(model_name, provider, base_url,
                                          api_key, model_info, NULL, seams);
    if (!warnings)
        return NULL;
    size_t n = json_len(warnings);
    if (n == 0) {
        json_free(warnings);
        return NULL;
    }
    if (n == 1) {
        json_t *single = json_copy(json_get(warnings, 0));
        json_free(warnings);
        return single;
    }
    json_t *first = json_get(warnings, 0);
    json_t *fm = json_obj_get(first, "model");
    json_t *fp = json_obj_get(first, "provider");
    char *message = combined_message(warnings);
    json_t *merged = selection_warning_new(
        "multiple", "Model Selection Warning",
        (fm && fm->type == JSON_STRING) ? fm->str_val : "",
        (fp && fp->type == JSON_STRING) ? fp->str_val : "",
        message ? message : "");
    free(message);
    json_free(warnings);
    return merged;
}
