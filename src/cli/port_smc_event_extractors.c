/*
 * port_smc_event_extractors.c — Faithful C11 port of the event-extraction
 * functions in hermes_cli/observability/shared_metrics_contract.py.
 *
 * Each extractor takes a telemetry event encoded as a JSON string and returns
 * the validated bounded counter dimensions as a JSON object string (malloc'd,
 * caller frees) or "None" for invalid events, matching Python's
 * `-> ... | None` semantics. Fields are read from the same shapes the Python
 * reads via getattr()/dict access.
 *
 * Two distinct metadata validators exist in the module:
 *   _event_metadata_is_valid      -> uses SCHEMA_KEY/SCHEMA_VERSION (metrics v2)
 *   _auxiliary_model_call...      -> uses RUNTIME_SCHEMA_KEY/VERSION (relay v1)
 * The counters all gate on _event_metadata_is_valid; only the auxiliary
 * model-call projection gates on the relay-runtime envelope.
 */
#define _POSIX_C_SOURCE 200809L
#include "port_hermes_cli_shared_metrics_contract.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"

/* ── constants from hermes_cli/observability/shared_metrics_contract.py ── */

#define SMC_SCHEMA_KEY_STR        "hermes.metrics.schema_version"
#define SMC_SCHEMA_VERSION_STR    "hermes.metrics.event.v2"
#define SMC_RUNTIME_INSTANCE_KEY  "hermes.relay.runtime_instance"
#define SMC_RUNTIME_SCHEMA_KEY    "hermes.relay.schema_version"
#define SMC_RUNTIME_SCHEMA_VERSION "hermes.relay.runtime.v1"
#define SMC_LOGICAL_LLM_SCOPE     "hermes.logical_llm_call"
#define SMC_MODEL_CALL_SCOPE      "hermes.model_call"
#define SMC_TASK_SCOPE            "hermes.task_run"
#define SMC_TOOL_CALL_SCOPE       "hermes.tool_call"
#define SMC_CLIENT_ACTIVE_MARK    "hermes.client.active"
#define SMC_TOOL_APPROVAL_MARK    "hermes.tool_approval"
#define SMC_SKILL_LIFECYCLE_MARK  "hermes.skill.lifecycle"
#define SMC_SKILL_LOAD_MARK       "hermes.skill.load"
#define SMC_MODEL_ROUTE_METRIC    "hermes.model_route.count"
#define SMC_TASK_STARTED_METRIC   "hermes.task_run.started"
#define SMC_TASK_FINISHED_METRIC  "hermes.task_run.finished"
#define SMC_TOOL_CALL_METRIC      "hermes.tool_call.count"
#define SMC_TOOL_APPROVAL_METRIC  "hermes.tool_approval.count"
#define SMC_SKILL_LIFECYCLE_METRIC "hermes.skill.lifecycle.count"
#define SMC_SKILL_LOAD_METRIC     "hermes.skill.load.count"
#define SMC_CLIENT_ACTIVE_METRIC  "hermes.client.active"
#define SMC_MODEL_IDENTIFIER_MAX_LENGTH 256
#define SMC_PROVIDER_IDENTIFIER_MAX_LENGTH 64
#define SMC_METRIC_IDENTIFIER_CHARACTERS "abcdefghijklmnopqrstuvwxyz0123456789._:/@+-"
#define SMC_METRIC_IDENTIFIER_START_CHARACTERS "abcdefghijklmnopqrstuvwxyz0123456789"

/* ── internal helpers ───────────────────────────────────────────── */

static char *smc_dup_lower_strip(const char *s) {
    if (!s) return NULL;
    while (*s && (unsigned char)*s <= ' ') s++;
    size_t n = strlen(s);
    while (n && (unsigned char)s[n-1] <= ' ') n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static const char *smc_json_str(const json_t *obj, const char *key) {
    const json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static bool smc_has_key(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return false;
    for (size_t i = 0; i < obj->c.count; i++)
        if (strcmp(obj->c.keys[i], key) == 0) return true;
    return false;
}

static bool smc_str_in(const char *s, const char *const *set) {
    if (!s) return false;
    for (size_t i = 0; set[i]; i++) if (strcmp(s, set[i]) == 0) return true;
    return false;
}

/* build {"k":"v","k":"v"} from parallel key/value arrays */
static char *smc_build_dims(const char *const *keys, const char *const *vals, int count) {
    int cap = 2;
    for (int i = 0; i < count; i++)
        cap += (int)strlen(keys[i]) + (int)(vals[i] ? strlen(vals[i]) : 0) + 6;
    char *out = malloc((size_t)cap);
    if (!out) return strdup("None");
    out[0] = '{'; out[1] = '\0';
    for (int i = 0; i < count; i++) {
        snprintf(out + strlen(out), (size_t)cap, "\"%s\":\"%s\"%s",
                 keys[i], vals[i] ? vals[i] : "", i < count - 1 ? "," : "");
    }
    strcat(out, "}");
    return out;
}

/* ── shared metric identifier normalizer ────────────────────────── */

/* PoP: smc_metric_identifier @ hermes_cli/observability/shared_metrics_contract.py:_metric_identifier */
char *smc_metric_identifier(const char *value, int max_length) {
    if (!value) return strdup("unknown");
    if ((int)strlen(value) > max_length) return strdup("unknown");
    char *norm = smc_dup_lower_strip(value);
    if (!norm) return strdup("unknown");
    size_t n = strlen(norm);
    if (n == 0 || n > (size_t)max_length) { free(norm); return strdup("unknown"); }
    if (!strchr(SMC_METRIC_IDENTIFIER_START_CHARACTERS, norm[0])) { free(norm); return strdup("unknown"); }
    for (size_t i = 0; i < n; i++)
        if (!strchr(SMC_METRIC_IDENTIFIER_CHARACTERS, norm[i])) { free(norm); return strdup("unknown"); }
    return norm;
}

/* ── non-negative number ────────────────────────────────────────── */

/* PoP: smc_non_negative_number @ hermes_cli/observability/shared_metrics_contract.py:_non_negative_number */
double smc_non_negative_number(const json_t *value) {
    if (!value || value->type == JSON_BOOL || value->type != JSON_NUMBER) return NAN;
    double num = value->num_val;
    return (isfinite(num) && num >= 0) ? num : NAN;
}

/* ── _event_metadata_is_valid (metrics v2 envelope) ─────────────── */

/* PoP: smc_event_metadata_is_valid @ hermes_cli/observability/shared_metrics_contract.py:_event_metadata_is_valid */
bool smc_event_metadata_is_valid(const json_t *event) {
    if (!event || event->type != JSON_OBJECT) return false;
    const json_t *metadata = json_obj_get(event, "metadata");
    if (!metadata || metadata->type != JSON_OBJECT) return false;
    const json_t *sv = json_obj_get(metadata, SMC_SCHEMA_KEY_STR);
    if (!sv || sv->type != JSON_STRING || strcmp(sv->str_val, SMC_SCHEMA_VERSION_STR) != 0)
        return false;
    /* relay_metadata = set(metadata) - {SCHEMA_KEY, RUNTIME_INSTANCE_KEY};
       must not contain anything besides otel.status_code */
    for (size_t i = 0; i < metadata->c.count; i++) {
        const char *key = metadata->c.keys[i];
        if (strcmp(key, SMC_SCHEMA_KEY_STR) == 0) continue;
        if (strcmp(key, SMC_RUNTIME_INSTANCE_KEY) == 0) continue;
        if (strcmp(key, "otel.status_code") != 0) return false;
    }
    const json_t *osc = json_obj_get(metadata, "otel.status_code");
    const char *oscs = (osc && osc->type == JSON_STRING) ? osc->str_val : "OK";
    return (strcmp(oscs, "OK") == 0 || strcmp(oscs, "ERROR") == 0);
}

/* ── model_call_fields (returns {model, provider}) ──────────────── */

static void smc_model_call_fields(const json_t *data, char **out_model, char **out_provider) {
    const json_t *rm = json_obj_get(data, "response_model");
    const char *rmv = (rm && rm->type == JSON_STRING) ? rm->str_val : NULL;
    char *model = smc_metric_identifier(rmv, SMC_MODEL_IDENTIFIER_MAX_LENGTH);
    if (strcmp(model, "unknown") == 0) {
        const json_t *m = json_obj_get(data, "model");
        const char *mv = (m && m->type == JSON_STRING) ? m->str_val : NULL;
        free(model);
        model = smc_metric_identifier(mv, SMC_MODEL_IDENTIFIER_MAX_LENGTH);
    }
    const json_t *p = json_obj_get(data, "provider");
    const char *pv = (p && p->type == JSON_STRING) ? p->str_val : NULL;
    *out_model = model;
    *out_provider = smc_metric_identifier(pv, SMC_PROVIDER_IDENTIFIER_MAX_LENGTH);
}

/* ── _auxiliary_model_call_dimensions (relay runtime envelope) ──── */

static bool smc_aux_metadata_valid(const json_t *event) {
    if (!event || event->type != JSON_OBJECT) return false;
    const json_t *metadata = json_obj_get(event, "metadata");
    if (!metadata || metadata->type != JSON_OBJECT) return false;
    const json_t *sv = json_obj_get(metadata, SMC_RUNTIME_SCHEMA_KEY);
    if (!sv || sv->type != JSON_STRING || strcmp(sv->str_val, SMC_RUNTIME_SCHEMA_VERSION) != 0)
        return false;
    for (size_t i = 0; i < metadata->c.count; i++) {
        const char *key = metadata->c.keys[i];
        if (strcmp(key, SMC_RUNTIME_INSTANCE_KEY) == 0) continue;
        if (strcmp(key, SMC_RUNTIME_SCHEMA_KEY) == 0) continue;
        if (strcmp(key, "hermes.call_role") == 0) continue;
        if (strcmp(key, "otel.status_code") != 0) return false;
    }
    const json_t *osc = json_obj_get(metadata, "otel.status_code");
    const char *oscs = (osc && osc->type == JSON_STRING) ? osc->str_val : "OK";
    if (strcmp(oscs, "OK") != 0 && strcmp(oscs, "ERROR") != 0) return false;
    const json_t *cr = json_obj_get(metadata, "hermes.call_role");
    if (!cr || cr->type != JSON_STRING) return false;
    return strncmp(cr->str_val, "auxiliary:", 10) == 0;
}

/* data must have exactly {model,outcome,provider} or {model,outcome,provider,response_model} */
static bool smc_aux_data_valid(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) return false;
    if (data->c.count != 3 && data->c.count != 4) return false;
    if (!smc_has_key(data, "model") || !smc_has_key(data, "outcome") || !smc_has_key(data, "provider"))
        return false;
    if (data->c.count == 4 && !smc_has_key(data, "response_model")) return false;
    const json_t *o = json_obj_get(data, "outcome");
    const char *ocs = (o && o->type == JSON_STRING) ? o->str_val : "";
    return strcmp(ocs, "cancelled") == 0 || strcmp(ocs, "failed") == 0 || strcmp(ocs, "success") == 0;
}

/* PoP: smc_auxiliary_model_call_dimensions @ hermes_cli/observability/shared_metrics_contract.py:_auxiliary_model_call_dimensions */
char *smc_auxiliary_model_call_dimensions(const char *event_json) {
    if (!event_json) return strdup("None");
    json_t *event = json_parse(event_json, NULL);
    if (!event) return strdup("None");
    char *result = strdup("None");

    const json_t *cp = json_obj_get(event, "category_profile");
    bool cp_none = (!cp || cp->type == JSON_NULL);

    if (smc_aux_metadata_valid(event) &&
        strcmp(smc_json_str(event, "kind"), "scope") == 0 &&
        strcmp(smc_json_str(event, "category"), "function") == 0 &&
        strcmp(smc_json_str(event, "name"), SMC_LOGICAL_LLM_SCOPE) == 0 &&
        strcmp(smc_json_str(event, "scope_category"), "end") == 0 && cp_none) {
        const json_t *data = json_obj_get(event, "data");
        if (smc_aux_data_valid(data)) {
            char *model, *provider;
            smc_model_call_fields(data, &model, &provider);
            /* counter_dimensions_are_valid(MODEL_ROUTE_METRIC, {model, provider}) */
            bool valid = (strcmp(model, "unknown") != 0 && strcmp(provider, "unknown") != 0);
            if (valid) {
                free(result);
                static const char *keys[] = {"model", "provider"};
                const char *vals[] = {model, provider};
                result = smc_build_dims(keys, vals, 2);
            }
            free(model); free(provider);
        }
    }
    json_free(event);
    return result;
}

/* ── model call dimensions ────────────────────────────────────── */

/* PoP: smc_model_call_dimensions @ hermes_cli/observability/shared_metrics_contract.py:model_call_dimensions */
char *smc_model_call_dimensions(const char *event_json) {
    if (!event_json) return strdup("None");
    /* 1) auxiliary projection wins */
    char *aux = smc_auxiliary_model_call_dimensions(event_json);
    if (strcmp(aux, "None") != 0) return aux;
    free(aux);

    json_t *event = json_parse(event_json, NULL);
    if (!event) return strdup("None");
    char *result = strdup("None");

    const json_t *cp = json_obj_get(event, "category_profile");
    if (smc_event_metadata_is_valid(event) &&
        strcmp(smc_json_str(event, "kind"), "scope") == 0 &&
        strcmp(smc_json_str(event, "category"), "llm") == 0 &&
        strcmp(smc_json_str(event, "name"), SMC_MODEL_CALL_SCOPE) == 0 &&
        strcmp(smc_json_str(event, "scope_category"), "end") == 0 &&
        cp && cp->type == JSON_OBJECT && cp->c.count == 1 && smc_has_key(cp, "model_name")) {
        const json_t *mn = json_obj_get(cp, "model_name");
        const char *mns = (mn && mn->type == JSON_STRING) ? mn->str_val : "";
        if (strcmp(mns, "unknown") == 0) {
            const json_t *data = json_obj_get(event, "data");
            if (data && data->type == JSON_OBJECT && data->c.count == 2 &&
                smc_has_key(data, "model") && smc_has_key(data, "provider")) {
                char *model, *provider;
                smc_model_call_fields(data, &model, &provider);
                bool valid = (strcmp(model, "unknown") != 0 && strcmp(provider, "unknown") != 0);
                if (valid) {
                    free(result);
                    static const char *keys[] = {"model", "provider"};
                    const char *vals[] = {model, provider};
                    result = smc_build_dims(keys, vals, 2);
                }
                free(model); free(provider);
            }
        }
    }
    json_free(event);
    return result;
}

/* ── task counter ───────────────────────────────────────────────── */

static bool smc_task_finished_fields_ok(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) return false;
    if (data->c.count != 9) return false;
    static const char *req[] = {
        "duration_bucket","end_reason","entrypoint","execution_surface",
        "model_call_count_bucket","outcome","retry_count_bucket","termination",
        "tool_call_count_bucket", NULL
    };
    int seen = 0;
    for (int k = 0; req[k]; k++) if (smc_has_key(data, req[k])) seen++;
    return seen == 9;
}

char *smc_task_terminal_fields_json(const json_t *data) {
    static const char *keys[] = {
        "duration_bucket","end_reason","entrypoint","execution_surface",
        "model_call_count_bucket","outcome","retry_count_bucket","termination",
        "tool_call_count_bucket"
    };
    char *vals[9];
    for (int i = 0; i < 9; i++) {
        const json_t *v = json_obj_get(data, keys[i]);
        vals[i] = (v && v->type == JSON_STRING) ? v->str_val : "unknown";
    }
    return smc_build_dims(keys, (const char *const *)vals, 9);
}

/* PoP: smc_task_counter @ hermes_cli/observability/shared_metrics_contract.py:task_counter */
char *smc_task_counter(const char *event_json) {
    if (!event_json) return strdup("None");
    json_t *event = json_parse(event_json, NULL);
    if (!event) return strdup("None");
    char *result = strdup("None");

    if (smc_event_metadata_is_valid(event) &&
        strcmp(smc_json_str(event, "kind"), "scope") == 0 &&
        strcmp(smc_json_str(event, "category"), "function") == 0 &&
        strcmp(smc_json_str(event, "name"), SMC_TASK_SCOPE) == 0) {
        const json_t *cp = json_obj_get(event, "category_profile");
        if (!cp || cp->type == JSON_NULL) {
            const char *scope_cat = smc_json_str(event, "scope_category");
            const json_t *data = json_obj_get(event, "data");
            if (strcmp(scope_cat, "start") == 0 && data && data->type == JSON_OBJECT &&
                data->c.count == 2 && smc_has_key(data, "entrypoint") &&
                smc_has_key(data, "execution_surface")) {
                free(result);
                result = smc_build_dims(
                    (const char *[]){"entrypoint","execution_surface"},
                    (const char *[]){smc_json_str(data,"entrypoint"),
                                     smc_json_str(data,"execution_surface")}, 2);
            } else if (strcmp(scope_cat, "end") == 0 && smc_task_finished_fields_ok(data)) {
                free(result);
                result = smc_task_terminal_fields_json(data);
            }
        }
    }
    json_free(event);
    return result;
}

/* ── tool call dimensions ───────────────────────────────────────── */

/* PoP: smc_tool_call_dimensions @ hermes_cli/observability/shared_metrics_contract.py:tool_call_dimensions */
char *smc_tool_call_dimensions(const char *event_json) {
    if (!event_json) return strdup("None");
    json_t *event = json_parse(event_json, NULL);
    if (!event) return strdup("None");
    char *result = strdup("None");

    const json_t *cp = json_obj_get(event, "category_profile");
    bool cp_empty = (cp && cp->type == JSON_OBJECT && cp->c.count == 0);

    if (smc_event_metadata_is_valid(event) &&
        strcmp(smc_json_str(event, "kind"), "scope") == 0 &&
        strcmp(smc_json_str(event, "category"), "tool") == 0 &&
        strcmp(smc_json_str(event, "name"), SMC_TOOL_CALL_SCOPE) == 0 &&
        strcmp(smc_json_str(event, "scope_category"), "end") == 0 && cp_empty) {
        const json_t *data = json_obj_get(event, "data");
        if (data && data->type == JSON_OBJECT && data->c.count == 5 &&
            smc_has_key(data, "approval_outcome") && smc_has_key(data, "latency_bucket") &&
            smc_has_key(data, "outcome") && smc_has_key(data, "retry_count_bucket") &&
            smc_has_key(data, "tool_category")) {
            free(result);
            result = smc_tool_terminal_fields_json(data, NULL, NULL, NULL);
        }
    }
    json_free(event);
    return result;
}

/* ── tool approval counter ──────────────────────────────────────── */

/* PoP: smc_tool_approval_counter @ hermes_cli/observability/shared_metrics_contract.py:tool_approval_counter */
char *smc_tool_approval_counter(const char *event_json) {
    if (!event_json) return strdup("None");
    json_t *event = json_parse(event_json, NULL);
    if (!event) return strdup("None");
    char *result = strdup("None");

    const json_t *cat = json_obj_get(event, "category");
    const json_t *sc = json_obj_get(event, "scope_category");
    const json_t *cp = json_obj_get(event, "category_profile");
    bool none_props = (!cat || cat->type == JSON_NULL) &&
                      (!sc || sc->type == JSON_NULL) &&
                      (!cp || cp->type == JSON_NULL);

    if (smc_event_metadata_is_valid(event) &&
        strcmp(smc_json_str(event, "kind"), "mark") == 0 &&
        strcmp(smc_json_str(event, "name"), SMC_TOOL_APPROVAL_MARK) == 0 && none_props) {
        const json_t *data = json_obj_get(event, "data");
        if (data && data->type == JSON_OBJECT && data->c.count == 2 &&
            smc_has_key(data, "attribution") && smc_has_key(data, "outcome")) {
            const char *attr = smc_json_str(data, "attribution");
            const char *ocs = smc_json_str(data, "outcome");
            static const char *attrs[] = {"tool_call","unattributed",NULL};
            static const char *outs[] = {"approved","denied","not_required","timed_out","unknown",NULL};
            if (smc_str_in(attr, attrs) && smc_str_in(ocs, outs) &&
                strcmp(ocs, "not_required") != 0) {
                free(result);
                static const char *keys[] = {"attribution", "outcome"};
                const char *vals[] = {attr, ocs};
                result = smc_build_dims(keys, vals, 2);
            }
        }
    }
    json_free(event);
    return result;
}

/* ── skill counter ──────────────────────────────────────────────── */

/* PoP: smc_skill_counter @ hermes_cli/observability/shared_metrics_contract.py:skill_counter */
char *smc_skill_counter(const char *event_json) {
    if (!event_json) return strdup("None");
    json_t *event = json_parse(event_json, NULL);
    if (!event) return strdup("None");
    char *result = strdup("None");

    const json_t *cat = json_obj_get(event, "category");
    const json_t *sc = json_obj_get(event, "scope_category");
    const json_t *cp = json_obj_get(event, "category_profile");
    bool none_props = (!cat || cat->type == JSON_NULL) &&
                      (!sc || sc->type == JSON_NULL) &&
                      (!cp || cp->type == JSON_NULL);

    if (smc_event_metadata_is_valid(event) &&
        strcmp(smc_json_str(event, "kind"), "mark") == 0 && none_props) {
        const char *name = smc_json_str(event, "name");
        const json_t *data = json_obj_get(event, "data");
        if (strcmp(name, SMC_SKILL_LIFECYCLE_MARK) == 0 && data &&
            data->type == JSON_OBJECT && data->c.count == 2 &&
            smc_has_key(data, "action") && smc_has_key(data, "provenance")) {
            const char *action = smc_json_str(data, "action");
            static const char *acts[] = {"archived","created","edited","installed","patched","restored","stale",NULL};
            if (smc_str_in(action, acts)) {
                free(result);
                static const char *keys[] = {"action", "provenance"};
                const char *vals[] = {action, smc_skill_provenance(smc_json_str(data, "provenance"))};
                result = smc_build_dims(keys, vals, 2);
            }
        } else if (strcmp(name, SMC_SKILL_LOAD_MARK) == 0 && data &&
                   data->type == JSON_OBJECT && data->c.count == 4 &&
                   smc_has_key(data, "post_patch_state") && smc_has_key(data, "provenance") &&
                   smc_has_key(data, "reuse_state") && smc_has_key(data, "use_count_bucket")) {
            free(result);
            result = smc_build_dims(
                (const char *[]){"post_patch_state","provenance","reuse_state","use_count_bucket"},
                (const char *[]){smc_json_str(data,"post_patch_state"),
                                 smc_skill_provenance(smc_json_str(data,"provenance")),
                                 smc_json_str(data,"reuse_state"),
                                 smc_json_str(data,"use_count_bucket")}, 4);
        }
    }
    json_free(event);
    return result;
}

/* ── client active counter ─────────────────────────────────────── */

/* PoP: smc_client_active_counter @ hermes_cli/observability/shared_metrics_contract.py:client_active_counter */
char *smc_client_active_counter(const char *event_json) {
    if (!event_json) return strdup("None");
    json_t *event = json_parse(event_json, NULL);
    if (!event) return strdup("None");
    char *result = strdup("None");

    const json_t *cat = json_obj_get(event, "category");
    const json_t *sc = json_obj_get(event, "scope_category");
    const json_t *cp = json_obj_get(event, "category_profile");
    const json_t *data = json_obj_get(event, "data");
    bool none_props = (!cat || cat->type == JSON_NULL) &&
                      (!sc || sc->type == JSON_NULL) &&
                      (!cp || cp->type == JSON_NULL);

    if (smc_event_metadata_is_valid(event) &&
        strcmp(smc_json_str(event, "kind"), "mark") == 0 &&
        strcmp(smc_json_str(event, "name"), SMC_CLIENT_ACTIVE_MARK) == 0 && none_props &&
        data && data->type == JSON_OBJECT && data->c.count == 0) {
        free(result);
        result = strdup("{}");  /* CLIENT_ACTIVE_METRIC -> {} */
    }
    json_free(event);
    return result;
}

/* ── skill provenance / tool categories / outcomes ─────────────── */

/* PoP: smc_skill_provenance @ hermes_cli/observability/shared_metrics_contract.py:skill_provenance */
const char *smc_skill_provenance(const char *value) {
    static const char *prov[] = {"agent_created","external","installed","local","unknown",NULL};
    char *norm = smc_dup_lower_strip(value);
    if (!norm) return "unknown";
    const char *ret = smc_str_in(norm, prov) ? norm : "unknown";
    if (ret != norm) { free(norm); }
    /* if ret == norm, caller must not free; but we leak for allowlist matches.
       To keep the contract simple (caller never frees return), intern it. */
    if (ret == norm) {
        static const char *cache[] = {"agent_created","external","installed","local","unknown",NULL};
        for (int i = 0; cache[i]; i++) if (strcmp(norm, cache[i]) == 0) { ret = cache[i]; break; }
        free(norm);
    }
    return ret;
}

/* PoP: smc_tool_category @ hermes_cli/observability/shared_metrics_contract.py:tool_category */
const char *smc_tool_category(const char *toolset) {
    char *s = smc_dup_lower_strip(toolset);
    if (!s || !*s) { if (s) free(s); return "unknown"; }
    static const char *known[] = {
        "browser","code_execution","communication","computer_use","delegation","file",
        "home_automation","mcp","media","memory","other","planning","project","scheduler",
        "skill","terminal","unknown","web", NULL
    };
    if (smc_str_in(s, known)) {
        for (int i = 0; known[i]; i++)
            if (strcmp(s, known[i]) == 0) { free(s); return known[i]; }
    }
    if (strncmp(s,"mcp",3)==0) { free(s); return "mcp"; }
    if (strncmp(s,"browser",7)==0) { free(s); return "browser"; }
    if (strncmp(s,"image",5)==0||strncmp(s,"tts",3)==0||strncmp(s,"video",5)==0||strncmp(s,"vision",6)==0) { free(s); return "media"; }
    if (strncmp(s,"homeassistant",13)==0) { free(s); return "home_automation"; }
    if (strcmp(s,"clarify")==0||strcmp(s,"kanban")==0||strcmp(s,"todo")==0) { free(s); return "planning"; }
    if (strcmp(s,"session_search")==0) { free(s); return "memory"; }
    if (strcmp(s,"cronjob")==0) { free(s); return "scheduler"; }
    if (strcmp(s,"skills")==0) { free(s); return "skill"; }
    if (strcmp(s,"x_search")==0) { free(s); return "web"; }
    if (strncmp(s,"discord",7)==0||strncmp(s,"email",5)==0||strncmp(s,"feishu",6)==0||
        strncmp(s,"hermes-yuanbao",14)==0||strncmp(s,"slack",5)==0||strncmp(s,"sms",3)==0) { free(s); return "communication"; }
    free(s);
    return "other";
}

/* PoP: smc_tool_outcome @ hermes_cli/observability/shared_metrics_contract.py:tool_outcome */
const char *smc_tool_outcome(const char *status) {
    char *s = smc_dup_lower_strip(status);
    if (!s) return "unknown";
    const char *ret;
    if (strcmp(s,"blocked")==0) ret = "blocked";
    else if (strcmp(s,"cancelled")==0) ret = "cancelled";
    else if (strcmp(s,"error")==0) ret = "failed";
    else if (strcmp(s,"failed")==0) ret = "failed";
    else if (strcmp(s,"ok")==0) ret = "success";
    else if (strcmp(s,"success")==0) ret = "success";
    else if (strcmp(s,"timed_out")==0) ret = "timed_out";
    else if (strcmp(s,"timeout")==0) ret = "timed_out";
    else ret = "unknown";
    free(s);
    return ret;
}

/* PoP: smc_tool_approval_outcome @ hermes_cli/observability/shared_metrics_contract.py:tool_approval_outcome */
const char *smc_tool_approval_outcome(const char *choice) {
    char *s = smc_dup_lower_strip(choice);
    if (!s) return "unknown";
    const char *ret;
    if (strcmp(s,"always")==0||strcmp(s,"approve")==0||strcmp(s,"approved")==0||
        strcmp(s,"once")==0||strcmp(s,"session")==0||strcmp(s,"smart_approve")==0) ret = "approved";
    else if (strcmp(s,"deny")==0||strcmp(s,"denied")==0||strcmp(s,"smart_deny")==0) ret = "denied";
    else if (strcmp(s,"timed_out")==0||strcmp(s,"timeout")==0) ret = "timed_out";
    else ret = "unknown";
    free(s);
    return ret;
}

/* ── tool terminal fields / latency / retry ────────────────────── */

/* PoP: smc_tool_latency_bucket @ hermes_cli/observability/shared_metrics_contract.py:tool_latency_bucket */
const char *smc_tool_latency_bucket(const json_t *value, const json_t *fallback) {
    double ms = smc_non_negative_number(value);
    if (isnan(ms)) {
        ms = smc_non_negative_number(fallback);
        if (isnan(ms)) return "unknown";
    }
    if (ms < 100) return "lt_100ms";
    if (ms < 250) return "100ms_to_250ms";
    if (ms < 500) return "250ms_to_500ms";
    if (ms < 1000) return "500ms_to_1s";
    if (ms < 2000) return "1s_to_2s";
    if (ms < 5000) return "2s_to_5s";
    if (ms < 10000) return "5s_to_10s";
    if (ms < 30000) return "10s_to_30s";
    return "gte_30s";
}

/* PoP: smc_tool_retry_bucket @ hermes_cli/observability/shared_metrics_contract.py:tool_retry_bucket */
const char *smc_tool_retry_bucket(const json_t *value) {
    if (!value || value->type == JSON_BOOL || value->type != JSON_NUMBER) return "unknown";
    double d = value->num_val;
    if (d != (double)(long long)d || d < 0) return "unknown";
    return smc_count_bucket((int)d);
}

/* PoP: smc_tool_terminal_fields_json @ hermes_cli/observability/shared_metrics_contract.py:tool_terminal_fields */
char *smc_tool_terminal_fields_json(const json_t *data, const char *category,
                                    const char *approval_outcome,
                                    const json_t *fallback_duration_ms) {
    if (!data || data->type != JSON_OBJECT) return strdup("None");
    static const char *approvals[] = {"approved","denied","not_required","timed_out","unknown",NULL};
    const char *ao = approval_outcome ? approval_outcome : "not_required";
    const char *ao_clean = smc_str_in(ao, approvals) ? ao : "unknown";
    const char *latency = smc_tool_latency_bucket(
        json_obj_get(data, "duration_ms"), fallback_duration_ms);
    const json_t *st = json_obj_get(data, "status");
    const char *outcome = smc_tool_outcome((st && st->type == JSON_STRING) ? st->str_val : "");
    const char *rb = smc_tool_retry_bucket(json_obj_get(data, "retry_count"));

    static const char *tcats[] = {
        "browser","code_execution","communication","computer_use","delegation","file",
        "home_automation","mcp","media","memory","other","planning","project","scheduler",
        "skill","terminal","unknown","web", NULL
    };
    const char *cat_final;
    if (category && smc_str_in(category, tcats)) {
        cat_final = category;
    } else {
        /* prefer the data's own tool_category if present and allowlisted */
        const json_t *dcat = json_obj_get(data, "tool_category");
        const char *dcv = (dcat && dcat->type == JSON_STRING) ? dcat->str_val : NULL;
        if (dcv && smc_str_in(dcv, tcats)) { cat_final = dcv; }
        else {
            const json_t *ts = json_obj_get(data, "toolset");
            const char *toolset = (ts && ts->type == JSON_STRING) ? ts->str_val : "";
            cat_final = smc_tool_category(toolset);
        }
    }
    static const char *keys[] = {"approval_outcome","latency_bucket","outcome",
                                 "retry_count_bucket","tool_category"};
    const char *vals[] = {ao_clean, latency, outcome, rb, cat_final};
    return smc_build_dims(keys, vals, 5);
}

/* ── client resource taxonomies ─────────────────────────────────── */

/* PoP: smc_client_os_family @ hermes_cli/observability/shared_metrics_contract.py:client_os_family */
const char *smc_client_os_family(const char *value) {
    char *s = smc_dup_lower_strip(value);
    if (!s) return "unknown";
    const char *ret = "unknown";
    if (strcmp(s,"darwin")==0||strcmp(s,"macos")==0) ret = "macos";
    else if (strcmp(s,"linux")==0) ret = "linux";
    else if (strcmp(s,"windows")==0) ret = "windows";
    free(s);
    return ret;
}

/* PoP: smc_client_architecture @ hermes_cli/observability/shared_metrics_contract.py:client_architecture */
const char *smc_client_architecture(const char *value) {
    char *s = smc_dup_lower_strip(value);
    if (!s) return "unknown";
    for (char *p = s; *p; p++) if (*p == '-') *p = '_';
    const char *ret = "unknown";
    if (strcmp(s,"amd64")==0||strcmp(s,"x64")==0||strcmp(s,"x86_64")==0) ret = "x86_64";
    else if (strcmp(s,"aarch64")==0||strcmp(s,"arm64")==0) ret = "arm64";
    else if (strcmp(s,"i386")==0||strcmp(s,"i486")==0||strcmp(s,"i586")==0||
             strcmp(s,"i686")==0||strcmp(s,"x86")==0) ret = "x86";
    else if (strncmp(s,"armv",4)==0) ret = "arm";
    free(s);
    return ret;
}

/* PoP: smc_client_install_method @ hermes_cli/observability/shared_metrics_contract.py:client_install_method */
const char *smc_client_install_method(const char *value) {
    char *s = smc_dup_lower_strip(value);
    if (!s || !*s) { if (s) free(s); return "unknown"; }
    static const char *methods[] = {"docker","git","homebrew","nixos","pip","unknown",NULL};
    for (int i = 0; methods[i]; i++)
        if (strcmp(s, methods[i]) == 0) { free(s); return methods[i]; }
    free(s);
    return "unknown";
}

/* PoP: smc_client_resource @ hermes_cli/observability/shared_metrics_contract.py:client_resource */
char *smc_client_resource(const char *hermes_version, const char *os_name,
                          const char *architecture, const char *install_method) {
    char hv[80];
    if (!hermes_version) { strcpy(hv, ""); }
    else {
        size_t i = 0;
        for (; hermes_version[i] && i < sizeof(hv)-1; i++) hv[i] = hermes_version[i];
        hv[i] = '\0';
        char *t = hv; while (*t && (unsigned char)*t <= ' ') t++;
        memmove(hv, t, strlen(t) + 1);
        size_t len = strlen(hv); while (len && (unsigned char)hv[len-1] <= ' ') hv[--len] = '\0';
        if (strlen(hv) == 0 || strlen(hv) > 64) strcpy(hv, "unknown");
    }
    static const char *keys[] = {"architecture","hermes_version","install_method","os_family"};
    const char *vals[] = {
        smc_client_architecture(architecture),
        hv,
        smc_client_install_method(install_method),
        smc_client_os_family(os_name)
    };
    return smc_build_dims(keys, vals, 4);
}

/* PoP: smc_client_resource_is_valid @ hermes_cli/observability/shared_metrics_contract.py:client_resource_is_valid */
bool smc_client_resource_is_valid(const char *resource_json) {
    if (!resource_json) return false;
    json_t *r = json_parse(resource_json, NULL);
    if (!r || r->type != JSON_OBJECT) { if (r) json_free(r); return false; }
    if (r->c.count != 4) { json_free(r); return false; }
    static const char *need[] = {"architecture","hermes_version","install_method","os_family",NULL};
    int ok = 0;
    for (int k = 0; need[k]; k++) if (smc_has_key(r, need[k])) ok++;
    bool valid = (ok == 4);
    if (valid) {
        const json_t *hv = json_obj_get(r, "hermes_version");
        if (!(hv && hv->type == JSON_STRING && hv->str_val && strlen(hv->str_val) > 0 && strlen(hv->str_val) <= 64)) valid = false;
        const json_t *osf = json_obj_get(r, "os_family");
        const char *osfv = (osf && osf->type==JSON_STRING) ? osf->str_val : "";
        static const char *osfams[] = {"linux","macos","unknown","windows",NULL};
        if (!smc_str_in(osfv, osfams)) valid = false;
        const json_t *arch = json_obj_get(r, "architecture");
        const char *av = (arch && arch->type==JSON_STRING) ? arch->str_val : "";
        static const char *archs[] = {"arm","arm64","unknown","x86","x86_64",NULL};
        if (!smc_str_in(av, archs)) valid = false;
        const json_t *im = json_obj_get(r, "install_method");
        const char *iv = (im && im->type==JSON_STRING) ? im->str_val : "";
        static const char *methods[] = {"docker","git","homebrew","nixos","pip","unknown",NULL};
        if (!smc_str_in(iv, methods)) valid = false;
    }
    json_free(r);
    return valid;
}
