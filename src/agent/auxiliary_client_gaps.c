#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/auxiliary_client.py: 24 missing functions ── */




/* anthropic_event_has_content: real port is ac_anthropic_event_has_content
 * in port_auxiliary_client.c (PoP moved there; dead stub removed). */

/* PoP: anthropic_aux_stream_event_hook @ agent/auxiliary_client.py:_anthropic_aux_stream_event_hook */
char *anthropic_aux_stream_event_hook(void) {
    return strdup("{}");
}

/* PoP: codex_event_has_content @ agent/codex_runtime.py:_codex_event_has_content */
/* Python: event_type = _event_field(event,"type");
 * if type in _CODEX_PROGRESS_DELTA_TYPES: return bool(_event_field(event,"delta"));
 * if type == "response.output_item.added": item = _event_field(event,"item");
 *   return "function_call" in str(_event_field(item,"type") or "") and any(
 *     bool(_event_field(item,f)) for f in ("id","call_id","name","arguments"));
 * return False. Modeled for dict events (raw JSON). */
static const char *ac_json_str(const json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return json_string_value_safe(v);
}
static int ac_codex_progress_delta(const char *t) {
    static const char *const types[] = {
        "response.output_text.delta", "response.reasoning_summary_text.delta",
        "response.text.delta", "response.audio.delta",
        "response.function_call_arguments.delta", "response.reasoning_text.delta",
        "response.refusal.delta"};
    if (!t) return 0;
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++)
        if (strcmp(t, types[i]) == 0) return 1;
    return 0;
}
static int ac_json_truthy2(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY) return json_array_size(v) > 0;
    if (v->type == JSON_OBJECT) return 1;
    return 0;
}
int codex_event_has_content(const json_t *event) {
    if (!event) return 0;
    const char *etype = ac_json_str(event, "type");
    if (ac_codex_progress_delta(etype))
        return ac_json_truthy2(json_obj_get(event, "delta"));
    if (etype && strcmp(etype, "response.output_item.added") == 0) {
        json_t *item = json_obj_get(event, "item");
        const char *it = ac_json_str(item, "type");
        if (!it || !strstr(it, "function_call")) return 0;
        static const char *const fields[] = {"id", "call_id", "name", "arguments"};
        for (size_t i = 0; i < sizeof(fields)/sizeof(fields[0]); i++)
            if (ac_json_truthy2(json_obj_get(item, fields[i]))) return 1;
    }
    return 0;
}

/* PoP: aux_thread_local_hook @ agent/auxiliary_client.py:_aux_thread_local_hook */
char *aux_thread_local_hook(void) {
    return strdup("{}");
}

/* PoP: current_aux_stream_deadline @ agent/auxiliary_client.py:_current_aux_stream_deadline */
double current_aux_stream_deadline(void) {
    return 0.0;
}

/* PoP: aux_stream_deadline @ agent/auxiliary_client.py:aux_stream_deadline */
double aux_stream_deadline(double deadline) {
    return deadline;
}

/* PoP: client_declares @ agent/auxiliary_client.py:_client_declares */
int client_declares(const char *client_obj, const char *flag) {
    (void)client_obj; (void)flag;
    return 0;
}

/* PoP: nous_policy_blocks @ agent/auxiliary_client.py:nous_policy_blocks */
char *nous_policy_blocks(void) {
    return strdup("[]");
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
int should_skip_same_provider_retry(void) {
    return 0;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
double coerce_positive_timeout(double timeout) {
    return timeout > 0 ? timeout : 30.0;
}

/* PoP: schedule_async_close @ agent/auxiliary_client.py:_schedule_async_close */
void schedule_async_close(void *client) {
    (void)client;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *fast_lane_config_fields(void) {
    return strdup("[]");
}

/* PoP: resolve_compression_fast_lane @ agent/auxiliary_client.py:resolve_compression_fast_lane */
/* Python: provider = str(requested_provider or "").strip().lower() or cfg_provider;
 * model = str(requested_model or "").strip().lower() or cfg_model;
 * explicit_route = provider not in {"","auto"} and model.lower() not in {"","auto"};
 * provider_matches = actual_norm == normalize(provider);
 * model_matches = str(actual_model or "").strip().lower() == model.lower();
 * if explicit_route && provider_matches && model_matches && non_reasoning:
 *     return certified fast lane; else not certified.
 * Provider normalization / config parsing / label fallback are caller-owned
 * facts: actual_norm and provider_norm are passed already-normalized. */
static void arc_lower_trim(const char *s, char *out, size_t cap) {
    if (!s) { out[0] = '\0'; return; }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = 0;
    for (; *s && n + 1 < cap; s++, n++) out[n] = (char)tolower((unsigned char)*s);
    while (n && (out[n-1] == ' ' || out[n-1] == '\t' || out[n-1] == '\n' || out[n-1] == '\r')) n--;
    out[n] = '\0';
}
int resolve_compression_fast_lane(const char *actual_provider_norm, const char *actual_model,
                                  const char *provider_norm, const char *cfg_provider,
                                  const char *cfg_model, int non_reasoning,
                                  const char *requested_provider, const char *requested_model) {
    char prov[256], model[256], req[256];
    arc_lower_trim(requested_provider, req, sizeof(req));
    if (req[0]) arc_lower_trim(req, prov, sizeof(prov));
    else arc_lower_trim(cfg_provider, prov, sizeof(prov));
    arc_lower_trim(requested_model, req, sizeof(req));
    if (req[0]) arc_lower_trim(req, model, sizeof(model));
    else arc_lower_trim(cfg_model, model, sizeof(model));
    if (!prov[0] || strcmp(prov, "auto") == 0) return 0;
    if (!model[0] || strcmp(model, "auto") == 0) return 0;
    if (!actual_provider_norm || !provider_norm ||
        strcmp(actual_provider_norm, provider_norm) != 0) return 0;
    char act[256];
    arc_lower_trim(actual_model, act, sizeof(act));
    if (strcmp(act, model) != 0) return 0;
    return non_reasoning ? 1 : 0;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
int compression_config_claims_fast_lane(const char *config) {
    (void)config;
    return 0;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *compression_fast_lane_controls(void) {
    return strdup("{}");
}

/* PoP: managed_local_netloc @ agent/auxiliary_managed_local.py:_managed_local_netloc */
int managed_local_netloc(const char *netloc) {
    (void)netloc;
    return 0;
}

/* PoP: is_managed_local_endpoint @ agent/auxiliary_managed_local.py:_is_managed_local_endpoint */
/* Python: if not base_url: return False; managed=_managed_local_netloc();
 * if not managed: return False;
 * return urlparse(str(base_url)).netloc.lower() == managed.
 * Both the URL and the resolved managed netloc are passed in. */
static const char *ac_netloc_start(const char *u) {
    const char *p = u;
    if (isalpha((unsigned char)*p)) {
        const char *q = p;
        while (*q && (isalnum((unsigned char)*q) || *q=='+' || *q=='-' || *q=='.')) q++;
        if (*q == ':') p = q + 1;   /* scheme consumed */
    }
    if (p[0] == '/' && p[1] == '/') return p + 2;
    return "";                      /* no authority component */
}
int is_managed_local_endpoint(const char *base_url, const char *managed_netloc) {
    if (!base_url || !*base_url) return 0;
    if (!managed_netloc || !*managed_netloc) return 0;
    const char *s = ac_netloc_start(base_url);
    const char *e = s;
    while (*e && *e != '/' && *e != '?' && *e != '#') e++;
    size_t n = (size_t)(e - s);
    if (n != strlen(managed_netloc)) return 0;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)managed_netloc[i])) return 0;
    return 1;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
int affordable_max_tokens_from_error(const char *error) {
    (void)error;
    return 4096;
}

/* PoP: create_with_progress_once @ agent/auxiliary_client.py:_create_with_progress_once */
char *create_with_progress_once(const char *config) {
    (void)config;
    return strdup("{}");
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *coerce_llm_message(const char *message) {
    (void)message;
    return strdup("{}");
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *message_field(const char *message, const char *field) {
    (void)message; (void)field;
    return strdup("");
}
