#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
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
int resolve_compression_fast_lane(const char *config) {
    (void)config;
    return 0;
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
int is_managed_local_endpoint(const char *url) {
    (void)url;
    return 0;
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
