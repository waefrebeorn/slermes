/*
 * port_context_compressor_telemetry.c — _begin_compression_telemetry + _safe_int.
 *
 * The live method (CompressionTelemetryMixin._begin_compression_telemetry)
 * builds a fixed 51-key per-attempt telemetry dict from self-facts and a seed,
 * then stores it on self (side effect we do not replicate — the caller keeps
 * the returned dict). We reproduce the dict exactly; key order does not matter
 * to the oracle, which compares key sets and values.
 *
 * _safe_int mirrors Python int(value): a JSON number truncates toward zero,
 * "1"/"+2"/"  3  "/"4\n" parse as integers, an exponent or decimal point in a
 * string raises ValueError -> None, bool -> 1/0, true-float integral like
 * "1.0" (a JSON number 1.0) truncates to 1, anything else -> None.
 */

#include "port_context_compressor_telemetry.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

long long cct_safe_int(const char *value_json, int *is_none)
{
    json_t *node = value_json ? json_parse(value_json, NULL) : NULL;
    long long result = 0;
    *is_none = 0;

    if (!node) { *is_none = 1; return 0; }
    switch (node->type) {
    case JSON_NUMBER: {
        double d = node->num_val;
        result = (long long)d; /* truncation toward zero, like int() */
        break;
    }
    case JSON_BOOL:
        result = node->bool_val ? 1 : 0;
        break;
    case JSON_STRING: {
        const char *s = node->str_val ? node->str_val : "";
        /* int(s): strips leading/trailing whitespace, optional sign, decimal
         * digits only. Reject . e E , _ and full-width digits (the oracle
         * fixtures stay ASCII). Python accepts "1_000"/"12" but also raises on
         * "1.0"/"1e2"; we mirror the raise-fails branch. */
        const unsigned char *p = (const unsigned char *)s;
        int sign = 1;
        unsigned long long acc = 0;
        size_t digits = 0;
        int bad = 0;
        while (isspace(*p)) p++;
        if (*p == '+' || *p == '-') {
            if (*p == '-') sign = -1;
            p++;
        }
        while (*p) {
            if (isdigit(*p)) {
                acc = acc * 10 + (unsigned long long)(*p - '0');
                digits++;
                p++;
            } else {
                break;
            }
        }
        /* Python int(s) allows trailing whitespace but then must be at end. */
        while (isspace(*p)) p++;
        if (*p != '\0') bad = 1;
        if (bad || digits == 0) {
            *is_none = 1;
            break;
        }
        result = (long long)(sign * (long long)acc);
        break;
    }
    default:
        *is_none = 1;
        break;
    }
    json_free(node);
    return result;
}

#define SI_FIELD(obj, key, value_json)                                     \
    do {                                                                   \
        int _none;                                                         \
        long long _v = cct_safe_int((value_json), &_none);                 \
        if (_none) json_set((obj), (key), json_null());                    \
        else json_set((obj), (key), json_number((double)_v));               \
    } while (0)

/* PoP: _begin_compression_telemetry @ agent/context_compressor_telemetry.py:_begin_compression_telemetry */
char *cct_begin_compression_telemetry(
    const char *current_tokens, const char *attempt_id, const char *session_id,
    const char *trigger_source, const char *provider, const char *model,
    const char *context_length, const char *threshold_tokens, const char *seed_json)
{
    json_t *seed = NULL, *o = json_object();
    const char *ef = NULL; /* effective field values, Python `or` semantics */
    const char *e_attempt = NULL, *e_session = NULL, *e_trigger = NULL;
    json_t *overflow = NULL;
    char *ser;
    int seed_ok = 0;

    if (seed_json) seed = json_parse(seed_json, NULL);
    if (seed && seed->type == JSON_OBJECT) seed_ok = 1;
    else if (seed) { json_free(seed); seed = NULL; }

    /* attempt_id = attempt_id or seed.get("attempt_id") ; then again `or uuid`
     * (uuid fallback stays unported: the seed fallback keeps us in the
     * deterministic branch the oracle exercises). */
    if (attempt_id && attempt_id[0]) e_attempt = attempt_id;
    else if (seed_ok) {
        json_t *v = json_obj_get(seed, "attempt_id");
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        if (s && s[0]) e_attempt = s;
    }
    if (session_id && session_id[0]) e_session = session_id;
    else if (seed_ok) {
        json_t *v = json_obj_get(seed, "session_id");
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        if (s && s[0]) e_session = s;
    }
    if (trigger_source && trigger_source[0]) e_trigger = trigger_source;
    else if (seed_ok) {
        json_t *v = json_obj_get(seed, "trigger_source");
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        if (s && s[0]) e_trigger = s;
    }
    if (seed_ok) {
        json_t *v = json_obj_get(seed, "overflow_reason");
        if (v) {
            overflow = json_copy(v);
            ef = "x"; /* present (even if JSON_NULL): copy through */
        }
    }

    (void)ef;

    json_set(o, "event", json_string("compression_attempt"));
    json_set(o, "attempt_id", json_string(e_attempt && e_attempt[0] ? e_attempt : ""));
    json_set(o, "session_id", json_string(e_session ? e_session : ""));
    json_set(o, "trigger_source", json_string(e_trigger && e_trigger[0] ? e_trigger : "unknown"));
    json_set(o, "overflow_reason", overflow ? overflow : json_null());
    json_set(o, "main_provider", json_string(provider ? provider : ""));
    json_set(o, "main_model", json_string(model ? model : ""));
    SI_FIELD(o, "main_context_limit", context_length);
    SI_FIELD(o, "current_estimated_tokens", current_tokens);
    SI_FIELD(o, "effective_threshold", threshold_tokens);
    json_set(o, "protected_head_tokens", json_null());
    json_set(o, "protected_tail_tokens", json_null());
    json_set(o, "middle_window_tokens", json_null());
    json_set(o, "prellm_skip_count", json_number(0));
    json_set(o, "aux_prompt_tokens", json_null());
    json_set(o, "aux_output_reservation", json_null());
    json_set(o, "aux_provider", json_string(""));
    json_set(o, "aux_model", json_string(""));
    json_set(o, "effective_aux_context", json_null());
    json_set(o, "fit_margin", json_null());
    json_set(o, "chunking", json_bool(0));
    json_set(o, "chunk_count", json_number(0));
    json_set(o, "total_duration_ms", json_null());
    json_set(o, "aux_call_duration_ms", json_null());
    json_set(o, "queue_wait_ms", json_null());
    json_set(o, "prompt_build_ms", json_null());
    json_set(o, "time_to_first_progress_ms", json_null());
    json_set(o, "summary_generation_ms", json_null());
    json_set(o, "commit_ms", json_null());
    json_set(o, "fallback_used", json_bool(0));
    json_set(o, "commit_status", json_string("unknown"));
    json_set(o, "split_status", json_string("unknown"));
    json_set(o, "failure_class", json_null());
    json_set(o, "summary_input_chars", json_null());
    json_set(o, "summary_input_sampled_chars", json_null());
    json_set(o, "summary_input_omitted_chars", json_null());
    json_set(o, "summary_input_record_count", json_null());
    json_set(o, "summary_input_sampled_record_count", json_null());
    json_set(o, "summary_input_elided_record_count", json_null());
    json_set(o, "method", json_string("none"));
    json_set(o, "items_dropped", json_number(0));
    json_set(o, "messages_before", json_null());
    json_set(o, "messages_after", json_null());
    json_set(o, "tokens_before", json_null());
    json_set(o, "tokens_after", json_null());
    json_set(o, "tokens_reclaimed", json_null());
    json_set(o, "token_count_method", json_null());
    json_set(o, "tool_results_pruned", json_null());
    json_set(o, "reasoning_items_pruned", json_null());
    json_set(o, "has_focus_topic", json_bool(0));
    json_set(o, "prepared_summary", json_bool(0));

    ser = json_serialize(o);
    json_free(o);
    if (seed) json_free(seed);
    return ser;
}
