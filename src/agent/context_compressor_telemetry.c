/*
 * context_compressor_telemetry.c - real C11 port of upstream Hermes
 *   agent/context_compressor_telemetry.py (CompressionTelemetryMixin).
 * Pure dict/coercion work: one telemetry dict is seeded, filled phase by phase, and
 * replaced with the committed shape. The message-token estimator is the seam: the caller
 * passes the estimate it answered, exactly as the live method receives it.
 * PoP: w19_contextcom_da88c1_safe_int @ agent/context_compressor_telemetry.py:_safe_int
 * PoP: w19_contextcom_da88c1_compression_method @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._compression_method
 * PoP: w19_contextcom_da88c1_note_aux_fallback @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._note_aux_route_fallback
 * PoP: w19_contextcom_da88c1_record_aux_call @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_aux_compression_call
 * PoP: w19_contextcom_da88c1_record_effect @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_compression_effect
 * PoP: w19_contextcom_da88c1_record_committed_effect @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_committed_compression_effect
 * PoP: w19_contextcom_da88c1_record_regions @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._record_compression_regions
 * PoP: w19_contextcom_da88c1_begin_telemetry @ agent/context_compressor_telemetry.py:CompressionTelemetryMixin._begin_compression_telemetry
 */
#include "hermes_json.h"
#include "noc_w19_contextcom_da88c1.h"

static json_t *jget(json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static json_t *cp(json_t *v) { return v ? json_copy(v) : json_null(); }

static int truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

static int py_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python int("...") text form: optional whitespace/sign, digits with single underscores. */
static int py_int_str(const char *s, long long *out) {
    if (!s) return 0;
    const char *p = s;
    while (*p && py_space(*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (*p < '0' || *p > '9') return 0;
    long long acc = 0;
    int digits = 0;
    for (;;) {
        if (*p >= '0' && *p <= '9') {
            acc = acc * 10 + (*p - '0');
            digits = 1;
            p++;
        } else if (*p == '_' && digits && p[1] >= '0' && p[1] <= '9') {
            p++;
        } else {
            break;
        }
    }
    while (*p && py_space(*p)) p++;
    if (*p != '\0' || !digits) return 0;
    *out = neg ? -acc : acc;
    return 1;
}

/* int(value) for a JSON scalar; 0 when int() would raise (TypeError/ValueError). */
static int py_int_value(json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) return py_int_str(v->str_val, out);
    return 0;
}

static long long as_ll(json_t *v) {
    long long x = 0;
    py_int_value(v, &x);
    return x;
}

json_t *w19_contextcom_da88c1_safe_int(json_t *value) {
    long long x = 0;
    if (!py_int_value(value, &x)) return json_null();
    return json_number((double)x);
}

const char *w19_contextcom_da88c1_compression_method(int last_summary_fallback_used,
                                                     int has_aux_model_failure_error) {
    if (last_summary_fallback_used) return "deterministic_fallback";
    if (has_aux_model_failure_error) return "aux_fallback_main";
    return "llm_summary";
}

/* startswith on the str() coercion of route.get("fallback_label") or "" - route labels are
 * strings; a non-string label never names the main-agent route. */
static int label_is_main_agent(json_t *route) {
    json_t *lab = jget(route, "fallback_label");
    if (!lab || lab->type != JSON_STRING || !lab->str_val) return 0;
    return strncmp(lab->str_val, "main-agent(", 11) == 0;
}

json_t *w19_contextcom_da88c1_note_aux_fallback(json_t *telemetry, json_t *route) {
    json_t *out = json_object();
    json_t *tel = (telemetry && telemetry->type == JSON_OBJECT) ? telemetry : NULL;
    json_set(out, "telemetry", cp(telemetry));
    json_set(out, "failure_model", json_null());
    json_set(out, "failure_error", json_null());
    json_t *failed = jget(route, "failed_model");
    if (!truthy(failed)) return out;

    if (tel) {
        json_set(out, "telemetry", cp(tel));
        json_t *t = json_obj_get(out, "telemetry");
        json_set(t, "fallback_used", json_bool(true));
        json_t *fc = jget(tel, "failure_class");
        json_set(t, "failure_class", truthy(fc) ? cp(fc) : json_string("aux_model_fallback"));
    }
    /* Only the main-agent route earns the user warning. */
    if (label_is_main_agent(route)) {
        json_set(out, "failure_model", cp(failed));
        json_t *failure = jget(route, "failure");
        if (!truthy(failure)) failure = json_string("unknown error");
        if (failure->type == JSON_STRING) {
            size_t len = strlen(failure->str_val);
            if (len <= 220) {
                json_set(out, "failure_error", json_string(failure->str_val));
            } else {
                char buf[256];
                size_t take = 217;
                memcpy(buf, failure->str_val, take);
                buf[take] = '\0';
                size_t n = strlen(buf);
                while (n > 0 && py_space(buf[n - 1])) buf[--n] = '\0';
                char *joined = (char *)malloc(n + 4);
                if (joined) {
                    memcpy(joined, buf, n);
                    memcpy(joined + n, "...", 4);
                    json_set(out, "failure_error", json_string(joined));
                    free(joined);
                }
            }
        }
    }
    return out;
}

static json_t *telemetry_copy(json_t *telemetry) {
    return (telemetry && telemetry->type == JSON_OBJECT) ? json_copy(telemetry) : cp(telemetry);
}

json_t *w19_contextcom_da88c1_record_aux_call(json_t *telemetry,
                                              json_t *rough_prompt_tokens,
                                              json_t *max_tokens,
                                              json_t *duration_ms,
                                              json_t *aux_provider,
                                              json_t *aux_model,
                                              json_t *effective_aux_context,
                                              json_t *phase_timings) {
    json_t *out = telemetry_copy(telemetry);
    if (!telemetry || telemetry->type != JSON_OBJECT) return out;

    json_set(out, "aux_prompt_tokens", cp(rough_prompt_tokens));
    json_set(out, "aux_output_reservation", w19_contextcom_da88c1_safe_int(max_tokens));
    if (truthy(aux_provider)) json_set(out, "aux_provider", cp(aux_provider));
    if (truthy(aux_model)) json_set(out, "aux_model", cp(aux_model));
    if (effective_aux_context && effective_aux_context->type != JSON_NULL)
        json_set(out, "effective_aux_context", w19_contextcom_da88c1_safe_int(effective_aux_context));

    json_t *eff = json_obj_get(out, "effective_aux_context");
    json_t *pr = json_obj_get(out, "aux_prompt_tokens");
    if (eff && eff->type != JSON_NULL && pr && pr->type != JSON_NULL) {
        long long e = as_ll(eff), p = as_ll(pr);
        json_t *rv = json_obj_get(out, "aux_output_reservation");
        long long res = (rv && rv->type != JSON_NULL) ? as_ll(rv) : 0;
        json_set(out, "fit_margin", json_number((double)(e - p - res)));
    }

    long long dur = 0;
    if (!py_int_value(duration_ms, &dur)) dur = 0;
    if (dur < 0) dur = 0;                    /* max(0, int(duration_ms)) */
    json_t *old = json_obj_get(out, "aux_call_duration_ms");
    long long base = (old && old->type == JSON_NUMBER) ? (long long)old->num_val : 0;
    json_set(out, "aux_call_duration_ms", json_number((double)(base + dur)));

    static const char *const PHASES[] = {
        "queue_wait_ms", "prompt_build_ms", "time_to_first_progress_ms",
        "summary_generation_ms", "commit_ms", NULL
    };
    if (phase_timings && phase_timings->type == JSON_OBJECT) {
        for (int i = 0; PHASES[i]; i++) {
            if (!json_has(phase_timings, PHASES[i])) continue;
            json_t *val = w19_contextcom_da88c1_safe_int(json_obj_get(phase_timings, PHASES[i]));
            int accumulate = (!strcmp(PHASES[i], "queue_wait_ms")
                              || !strcmp(PHASES[i], "summary_generation_ms"))
                             && val->type == JSON_NUMBER;
            if (accumulate) {
                json_t *prev = json_obj_get(out, PHASES[i]);
                long long pv = (prev && prev->type == JSON_NUMBER) ? (long long)prev->num_val : 0;
                json_set(out, PHASES[i], json_number((double)(pv + (long long)val->num_val)));
                json_free(val);
            } else {
                json_set(out, PHASES[i], val);
            }
        }
    }
    return out;
}

json_t *w19_contextcom_da88c1_record_effect(json_t *telemetry,
                                            int last_summary_fallback_used,
                                            int has_aux_model_failure_error,
                                            json_t *messages_before,
                                            json_t *compressed_len,
                                            json_t *tokens_before,
                                            json_t *tool_results_pruned,
                                            json_t *reasoning_items_pruned,
                                            json_t *last_dropped_count,
                                            json_t *rough_tokens_after) {
    json_t *out = telemetry_copy(telemetry);
    if (!telemetry || telemetry->type != JSON_OBJECT) return out;

    json_set(out, "method", json_string(w19_contextcom_da88c1_compression_method(
        last_summary_fallback_used, has_aux_model_failure_error)));
    long long dropped = 0;
    if (!py_int_value(last_dropped_count, &dropped)) dropped = 0;
    json_set(out, "items_dropped", json_number((double)dropped));
    json_set(out, "messages_before", cp(messages_before));
    json_set(out, "messages_after", cp(compressed_len));
    json_set(out, "tokens_before", cp(tokens_before));
    json_set(out, "tokens_after", cp(rough_tokens_after));
    long long before = as_ll(tokens_before), after = as_ll(rough_tokens_after);
    json_set(out, "tokens_reclaimed", json_number((double)(before - after)));
    json_set(out, "token_count_method", json_string("estimate_rough"));
    json_set(out, "tool_results_pruned", cp(tool_results_pruned));
    json_set(out, "reasoning_items_pruned", cp(reasoning_items_pruned));
    return out;
}

json_t *w19_contextcom_da88c1_record_committed_effect(json_t *telemetry,
                                                      json_t *before_len,
                                                      json_t *after_len,
                                                      json_t *tokens_before,
                                                      json_t *tokens_after) {
    json_t *out = telemetry_copy(telemetry);
    if (!telemetry || telemetry->type != JSON_OBJECT) return out;

    json_set(out, "messages_before", cp(before_len));
    json_set(out, "messages_after", cp(after_len));
    json_set(out, "tokens_before", cp(tokens_before));
    json_set(out, "tokens_after", cp(tokens_after));
    long long before = as_ll(tokens_before), after = as_ll(tokens_after);
    json_set(out, "tokens_reclaimed", json_number((double)(before - after)));
    json_set(out, "token_count_method", json_string("estimate_rough"));
    return out;
}

json_t *w19_contextcom_da88c1_record_regions(json_t *telemetry, json_t *head_tokens,
                                             json_t *middle_tokens, json_t *tail_tokens) {
    json_t *out = telemetry_copy(telemetry);
    if (!telemetry || telemetry->type != JSON_OBJECT) return out;
    json_set(out, "protected_head_tokens", cp(head_tokens));
    json_set(out, "middle_window_tokens", cp(middle_tokens));
    json_set(out, "protected_tail_tokens", cp(tail_tokens));
    return out;
}

json_t *w19_contextcom_da88c1_begin_telemetry(json_t *attempt_id, json_t *session_id,
                                               json_t *trigger_source, json_t *seed,
                                               json_t *provider, json_t *model,
                                               json_t *context_length, json_t *current_tokens,
                                               json_t *threshold_tokens) {
    json_t *sd = (seed && seed->type == JSON_OBJECT) ? seed : NULL;
    json_t *attempt = truthy(attempt_id) ? attempt_id : (sd ? json_obj_get(sd, "attempt_id") : NULL);
    json_t *session = truthy(session_id) ? session_id : (sd ? json_obj_get(sd, "session_id") : NULL);
    json_t *trigger = truthy(trigger_source) ? trigger_source : (sd ? json_obj_get(sd, "trigger_source") : NULL);

    json_t *out = json_object();
    json_set(out, "event", json_string("compression_attempt"));
    json_set(out, "attempt_id", truthy(attempt) ? cp(attempt) : json_string(""));
    json_set(out, "session_id", truthy(session) ? cp(session) : json_string(""));
    json_set(out, "trigger_source", truthy(trigger) ? cp(trigger) : json_string("unknown"));
    json_set(out, "overflow_reason", sd ? cp(json_obj_get(sd, "overflow_reason")) : json_null());
    json_set(out, "main_provider", truthy(provider) ? cp(provider) : json_string(""));
    json_set(out, "main_model", truthy(model) ? cp(model) : json_string(""));
    json_set(out, "main_context_limit", w19_contextcom_da88c1_safe_int(context_length));
    json_set(out, "current_estimated_tokens", w19_contextcom_da88c1_safe_int(current_tokens));
    json_set(out, "effective_threshold", w19_contextcom_da88c1_safe_int(threshold_tokens));
    json_set(out, "protected_head_tokens", json_null());
    json_set(out, "protected_tail_tokens", json_null());
    json_set(out, "middle_window_tokens", json_null());
    json_set(out, "prellm_skip_count", json_number(0));
    json_set(out, "aux_prompt_tokens", json_null());
    json_set(out, "aux_output_reservation", json_null());
    json_set(out, "aux_provider", json_string(""));
    json_set(out, "aux_model", json_string(""));
    json_set(out, "effective_aux_context", json_null());
    json_set(out, "fit_margin", json_null());
    json_set(out, "chunking", json_bool(false));
    json_set(out, "chunk_count", json_number(0));
    json_set(out, "total_duration_ms", json_null());
    json_set(out, "aux_call_duration_ms", json_null());
    json_set(out, "queue_wait_ms", json_null());
    json_set(out, "prompt_build_ms", json_null());
    json_set(out, "time_to_first_progress_ms", json_null());
    json_set(out, "summary_generation_ms", json_null());
    json_set(out, "commit_ms", json_null());
    json_set(out, "fallback_used", json_bool(false));
    json_set(out, "commit_status", json_string("unknown"));
    json_set(out, "split_status", json_string("unknown"));
    json_set(out, "failure_class", json_null());
    json_set(out, "summary_input_chars", json_null());
    json_set(out, "summary_input_sampled_chars", json_null());
    json_set(out, "summary_input_omitted_chars", json_null());
    json_set(out, "summary_input_record_count", json_null());
    json_set(out, "summary_input_sampled_record_count", json_null());
    json_set(out, "summary_input_elided_record_count", json_null());
    json_set(out, "method", json_string("none"));
    json_set(out, "items_dropped", json_number(0));
    json_set(out, "messages_before", json_null());
    json_set(out, "messages_after", json_null());
    json_set(out, "tokens_before", json_null());
    json_set(out, "tokens_after", json_null());
    json_set(out, "tokens_reclaimed", json_null());
    json_set(out, "token_count_method", json_null());
    json_set(out, "tool_results_pruned", json_null());
    json_set(out, "reasoning_items_pruned", json_null());
    json_set(out, "has_focus_topic", json_bool(false));
    json_set(out, "prepared_summary", json_bool(false));
    return out;
}
