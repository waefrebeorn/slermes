/*
 * compaction_events.c — C home for upstream Hermes agent/compaction_events.py.
 *
 * Content-free ``hermes.compaction`` v1 records for history rewrites and their
 * fan-out. Every categorical field comes from a closed set with an ``other``
 * fallback. Faithful port of the schema/payload builders; the publishing and
 * hook-dispatch side effects are intentionally not part of this pure C API.
 *
 * PoP: w9_compactione_f03b0c_closed @ agent/compaction_events.py:_closed
 * PoP: w9_compactione_f03b0c_int @ agent/compaction_events.py:_int
 * PoP: w9_compactione_f03b0c_failure_class @ agent/compaction_events.py:failure_class
 * PoP: w9_compactione_f03b0c_event_name @ agent/compaction_events.py:event_name
 * PoP: w9_compactione_f03b0c_attempt_outcome @ agent/compaction_events.py:_attempt_outcome
 * PoP: w9_compactione_f03b0c_attempt_trigger @ agent/compaction_events.py:_attempt_trigger
 * PoP: w9_compactione_f03b0c_payload @ agent/compaction_events.py:_payload
 * PoP: w9_compactione_f03b0c_attempt_payload @ agent/compaction_events.py:attempt_payload
 * PoP: w9_compactione_f03b0c_micro_payload @ agent/compaction_events.py:micro_payload
 * PoP: w9_compactione_f03b0c_prune_payload @ agent/compaction_events.py:prune_payload
 * PoP: w9_compactione_f03b0c_provider_native_payload @ agent/compaction_events.py:provider_native_payload
 */
#include "noc_w9_compactione_f03b0c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CE_COMPACTION_MARK "compaction"
#define CE_ATTEMPT_MARK "compaction.attempt"

static const char *const KINDS[] = {
    "summarize", "micro_summarize", "prune_tool_results", "provider_native",
};
static const char *const SCOPES[] = {"history", "provider"};
static const char *const METHODS[] = {
    "llm_summary", "aux_fallback_main", "deterministic_fallback",
    "deterministic_prune", "provider", "none",
};
static const char *const TRIGGERS[] = {
    "manual", "turn_start_threshold", "pre_api", "post_tool", "idle",
    "engine_preflight", "overflow", "gateway_hygiene", "between_turns",
    "proactive_prune", "provider", "unknown",
};
static const char *const TRIGGER_CLASS_VALUES[] = {
    "auto", "manual", "overflow", "provider", "unknown",
};
static const char *const OVERFLOW_REASONS[] = {
    "context_overflow", "payload_too_large", "long_context_tier",
};
static const char *const OUTCOMES[] = {
    "committed", "aborted", "failed", "skipped", "blocked",
};
static const char *const SPLIT_STATUSES[] = {
    "not_applicable", "in_place_committed", "rotated_committed",
    "failed_not_indexed", "aborted",
};
static const char *const TOKEN_COUNT_METHODS[] = {"estimate_rough"};
static const char *const SKIP_CLASSES[] = {
    "attempt_superseded", "empty_post_handoff_window", "explicit_interrupt",
    "insufficient_messages", "lock_contended", "no_compressible_window",
    "snapshot_stale",
};
static const char *const FAILURE_CLASSES[] = {
    "attempt_superseded", "empty_post_handoff_window", "explicit_interrupt",
    "insufficient_messages", "lock_contended", "no_compressible_window",
    "snapshot_stale",
    "aux_model_fallback", "commit_fence_cancelled", "empty_transcript",
    "exception", "feasibility_skip", "no_progress", "none", "other",
    "pool_saturated", "rollback", "session_split_failed",
    "stall_deterministic_fallback", "stall_interrupted",
    "summary_auth_failure", "summary_empty_content_failure",
    "summary_generation_aborted", "summary_generation_failed",
    "summary_network_failure", "summary_overload_degraded",
    "summary_overload_failure", "summary_truncated_failure", "unknown",
    "would_grow",
};
static const char *const EXTRA_FAILURE_CLASSES[] = {
    "blocked:cooldown", "blocked:structural_backoff", "blocked:ineffective",
    "blocked:unknown", "codex_auto_native", "codex_auto_off",
    "codex_no_thread", "codex_compaction_failed", "session_ownership_lost",
    "session_ownership_unreadable", "cooldown_state_unreadable",
    "empty_transcript", "summary_model_benched", "summarize_failed",
    "exchange_skipped", "defrag_failed", "stale_generation",
    "watermark_unavailable",
};
static const char *const ABORTED_CLASSES[] = {
    "would_grow", "no_progress", "commit_fence_cancelled",
};
static const char *const COMMIT_MODES[] = {"in_place_committed", "rotated_committed"};

#define NEOF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const char *const FIELDS[] = {
    "harness", "kind", "scope", "official", "method", "trigger",
    "trigger_class", "overflow_reason", "outcome", "failure_class",
    "attempt_id", "session_id", "tokens_before", "tokens_after",
    "tokens_reclaimed", "token_count_method", "messages_before",
    "messages_after", "items_dropped", "context_limit", "threshold_tokens",
    "protected_head_tokens", "middle_window_tokens", "protected_tail_tokens",
    "duration_ms", "summary_generation_ms", "aux_call_duration_ms",
    "queue_wait_ms", "commit_ms", "summarizer_provider", "summarizer_model",
    "tool_results_pruned", "reasoning_items_pruned",
    "summary_input_omitted_chars", "compression_count", "in_place",
    "session_rotated", "split_status", "cache_break", "has_focus_topic",
};

static int in_set(const char *s, const char *const *set, int n)
{
    if (!s) return 0;
    for (int i = 0; i < n; i++) if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

static int py_falsy(const json_t *v)
{
    if (!v || json_is_null(v)) return 1;
    if (json_is_bool(v)) return !json_bool_value(v);
    if (json_is_number(v)) return json_number_value(v) == 0;
    if (json_is_string(v)) { const char *s = json_string_value(v); return !s || !*s; }
    if (json_is_array(v) || json_is_object(v)) return json_len(v) == 0;
    return 0;
}

static json_t *copy_or_null(json_t *v) { return v ? json_copy(v) : json_new_null(); }

static const char *trigger_class_for(const char *trigger)
{
    if (!trigger) return "auto";
    if (strcmp(trigger, "manual") == 0 || strcmp(trigger, "overflow") == 0 ||
        strcmp(trigger, "provider") == 0 || strcmp(trigger, "unknown") == 0)
        return trigger;
    if (strcmp(trigger, "other") == 0) return "unknown";
    return "auto";
}

static const char *micro_outcome_for(const char *outcome)
{
    if (!outcome) return "failed";
    if (strcmp(outcome, "absorbed") == 0 || strcmp(outcome, "defrag") == 0) return "committed";
    if (strcmp(outcome, "stale_generation") == 0 ||
        strcmp(outcome, "watermark_unavailable") == 0) return "skipped";
    return "failed";
}

/* _closed(value, allowed, dflt) */
static json_t *ce_closed(json_t *value, const char *const *allowed, int nallowed,
                         const char *dflt)
{
    if (!json_is_string(value)) return json_new_null();
    const char *s = json_string_value(value);
    if (!s || !*s) return json_new_null();
    for (int i = 0; i < nallowed; i++)
        if (strcmp(s, allowed[i]) == 0) return json_new_string(s);
    return dflt ? json_new_string(dflt) : json_new_null();
}

/* _int(value) */
static json_t *ce_int(json_t *value)
{
    if (!json_is_number(value) || json_is_bool(value)) return json_new_null();
    return json_new_number(json_number_value(value));
}

/* failure_class(value) */
static json_t *ce_failure_class(json_t *value)
{
    if (!json_is_string(value) || !json_string_value(value) || !*json_string_value(value))
        return json_new_string("none");
    const char *s = json_string_value(value);
    char pref[16];
    if (strncmp(s, "exception:", 10) == 0) { memcpy(pref, s, 9); pref[9] = '\0'; s = pref; }
    else if (strncmp(s, "rollback:", 9) == 0) { memcpy(pref, s, 8); pref[8] = '\0'; s = pref; }
    if (in_set(s, FAILURE_CLASSES, NEOF(FAILURE_CLASSES)) ||
        in_set(s, EXTRA_FAILURE_CLASSES, NEOF(EXTRA_FAILURE_CLASSES)))
        return json_new_string(s);
    return json_new_string("other");
}

static const char *ce_event_name(json_t *payload)
{
    json_t *outcome = json_obj_get(payload, "outcome");
    json_t *scope = json_obj_get(payload, "scope");
    int committed = json_is_string(outcome) && json_string_value(outcome) &&
                    strcmp(json_string_value(outcome), "committed") == 0;
    int scoped = json_is_string(scope) && json_string_value(scope) &&
                 (strcmp(json_string_value(scope), "history") == 0 ||
                  strcmp(json_string_value(scope), "provider") == 0);
    return (committed && scoped) ? CE_COMPACTION_MARK : CE_ATTEMPT_MARK;
}

static json_t *closed_or(json_t *value, const char *const *allowed, int nallowed,
                         const char *coerce_dflt, const char *or_dflt)
{
    json_t *r = ce_closed(value, allowed, nallowed, coerce_dflt);
    if (or_dflt && json_is_null(r)) { json_free(r); r = json_new_string(or_dflt); }
    return r;
}

/* _payload(values) */
static json_t *ce_payload(json_t *values)
{
    json_t *p = json_new_object();
    for (size_t i = 0; i < NEOF(FIELDS); i++)
        json_object_set(p, FIELDS[i], json_new_null());
    if (json_is_object(values)) {
        size_t n = json_object_size(values);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(values, i);
            json_t *v = json_object_get_at(values, i);
            if (k && v) json_object_set(p, k, json_copy(v));
        }
    }
    json_object_set(p, "harness", json_new_string("hermes"));
    json_object_set(p, "kind", closed_or(json_obj_get(p, "kind"), KINDS, NEOF(KINDS), "other", NULL));
    json_object_set(p, "scope", closed_or(json_obj_get(p, "scope"), SCOPES, NEOF(SCOPES), "other", NULL));
    json_object_set(p, "method", closed_or(json_obj_get(p, "method"), METHODS, NEOF(METHODS), "other", "none"));
    json_object_set(p, "trigger", closed_or(json_obj_get(p, "trigger"), TRIGGERS, NEOF(TRIGGERS), "other", "unknown"));
    {
        json_t *tc = ce_closed(json_obj_get(p, "trigger_class"), TRIGGER_CLASS_VALUES,
                               NEOF(TRIGGER_CLASS_VALUES), NULL);
        if (json_is_null(tc)) {
            json_free(tc);
            tc = json_new_string(trigger_class_for(json_get_str(p, "trigger", NULL)));
        }
        json_object_set(p, "trigger_class", tc);
    }
    json_object_set(p, "outcome", closed_or(json_obj_get(p, "outcome"), OUTCOMES, NEOF(OUTCOMES), "other", "other"));
    json_object_set(p, "token_count_method",
                    closed_or(json_obj_get(p, "token_count_method"), TOKEN_COUNT_METHODS,
                              NEOF(TOKEN_COUNT_METHODS), "other", NULL));
    if (py_falsy(json_obj_get(p, "attempt_id")))
        json_object_set(p, "attempt_id", json_new_string("attempt"));
    if (py_falsy(json_obj_get(p, "session_id")))
        json_object_set(p, "session_id", json_new_string(""));
    {
        json_t *cb = json_obj_get(p, "cache_break");
        json_t *scope = json_obj_get(p, "scope");
        json_t *outcome = json_obj_get(p, "outcome");
        if (json_is_null(cb) && json_is_string(scope) && json_string_value(scope) &&
            strcmp(json_string_value(scope), "history") == 0) {
            int committed = json_is_string(outcome) && json_string_value(outcome) &&
                            strcmp(json_string_value(outcome), "committed") == 0;
            json_object_set(p, "cache_break", json_new_bool(committed));
        }
    }
    return p;
}

/* _attempt_outcome */
static json_t *ce_attempt_outcome(json_t *commit_status, json_t *failure, json_t *history_rewritten)
{
    if (json_is_bool(history_rewritten) && json_bool_value(history_rewritten))
        return json_new_string("committed");
    const char *cs = json_is_string(commit_status) ? json_string_value(commit_status) : NULL;
    if (!cs || strcmp(cs, "aborted") != 0)
        return in_set(cs, OUTCOMES, NEOF(OUTCOMES)) ? json_new_string(cs) : json_new_string("other");
    const char *f = json_is_string(failure) ? json_string_value(failure) : NULL;
    if (in_set(f, SKIP_CLASSES, NEOF(SKIP_CLASSES))) return json_new_string("skipped");
    return in_set(f, ABORTED_CLASSES, NEOF(ABORTED_CLASSES)) ? json_new_string("aborted")
                                                             : json_new_string("failed");
}

/* _attempt_trigger */
static json_t *ce_attempt_trigger(json_t *trigger_source)
{
    json_t *arr = json_new_array();
    const char *ts = json_is_string(trigger_source) ? json_string_value(trigger_source) : NULL;
    if (ts && strcmp(ts, "auto") == 0) {
        json_array_append(arr, json_new_string("unknown"));
        json_array_append(arr, json_new_string("auto"));
        return arr;
    }
    json_array_append(arr, json_new_string(ts && *ts ? ts : "unknown"));
    json_array_append(arr, json_new_null());
    return arr;
}

static const char *const ATTEMPT_INTS[][2] = {
    {"tokens_before", "tokens_before"}, {"tokens_after", "tokens_after"},
    {"tokens_reclaimed", "tokens_reclaimed"}, {"messages_before", "messages_before"},
    {"messages_after", "messages_after"}, {"items_dropped", "items_dropped"},
    {"main_context_limit", "context_limit"}, {"effective_threshold", "threshold_tokens"},
    {"protected_head_tokens", "protected_head_tokens"},
    {"middle_window_tokens", "middle_window_tokens"},
    {"protected_tail_tokens", "protected_tail_tokens"},
    {"total_duration_ms", "duration_ms"}, {"summary_generation_ms", "summary_generation_ms"},
    {"aux_call_duration_ms", "aux_call_duration_ms"}, {"queue_wait_ms", "queue_wait_ms"},
    {"commit_ms", "commit_ms"}, {"tool_results_pruned", "tool_results_pruned"},
    {"reasoning_items_pruned", "reasoning_items_pruned"},
    {"summary_input_omitted_chars", "summary_input_omitted_chars"},
};

/* attempt_payload(record, compression_count) */
static json_t *ce_attempt_payload(json_t *record, json_t *compression_count)
{
    json_t *values = json_new_object();
    if (json_is_object(record)) {
        json_t *route = json_obj_get(record, "route");
        int codex = json_is_string(route) && json_string_value(route) &&
                    strcmp(json_string_value(route), "codex_app_server") == 0;
        json_object_set(values, "kind", json_new_string(codex ? "provider_native" : "summarize"));
        json_object_set(values, "scope", json_new_string(codex ? "provider" : "history"));
        json_object_set(values, "official", json_new_bool(1));
        json_t *fc = ce_failure_class(json_obj_get(record, "failure_class"));
        json_object_set(values, "failure_class", fc);
        json_object_set(values, "split_status",
                        ce_closed(json_obj_get(record, "split_status"), SPLIT_STATUSES, NEOF(SPLIT_STATUSES), "other"));
        json_t *outcome = ce_attempt_outcome(json_obj_get(record, "commit_status"), fc,
                                             json_obj_get(record, "history_rewritten"));
        json_object_set(values, "outcome", outcome);
        json_t *trig = ce_attempt_trigger(json_obj_get(record, "trigger_source"));
        json_object_set(values, "trigger", json_copy(json_get(trig, 0)));
        json_object_set(values, "trigger_class", json_copy(json_get(trig, 1)));
        json_free(trig);
        json_object_set(values, "overflow_reason",
                        ce_closed(json_obj_get(record, "overflow_reason"), OVERFLOW_REASONS, NEOF(OVERFLOW_REASONS), "other"));
        json_object_set(values, "attempt_id", copy_or_null(json_obj_get(record, "attempt_id")));
        json_object_set(values, "session_id", copy_or_null(json_obj_get(record, "session_id")));
        json_object_set(values, "token_count_method", copy_or_null(json_obj_get(record, "token_count_method")));
        json_object_set(values, "summarizer_provider", copy_or_null(json_obj_get(record, "aux_provider")));
        json_object_set(values, "summarizer_model", copy_or_null(json_obj_get(record, "aux_model")));
        json_object_set(values, "has_focus_topic", copy_or_null(json_obj_get(record, "has_focus_topic")));
        for (size_t i = 0; i < NEOF(ATTEMPT_INTS); i++) {
            json_t *v = ce_int(json_obj_get(record, ATTEMPT_INTS[i][0]));
            json_object_set(values, ATTEMPT_INTS[i][1], v);
        }
        json_t *ss = json_obj_get(values, "split_status");
        int in_modes = json_is_string(ss) && json_string_value(ss) &&
                       in_set(json_string_value(ss), COMMIT_MODES, NEOF(COMMIT_MODES));
        if (in_modes) {
            json_object_set(values, "in_place", json_new_bool(strcmp(json_string_value(ss), "in_place_committed") == 0));
            json_object_set(values, "session_rotated", json_new_bool(strcmp(json_string_value(ss), "rotated_committed") == 0));
        }
        if (codex) json_object_set(values, "cache_break", json_new_null());
    }
    json_object_set(values, "compression_count", ce_int(compression_count));
    return ce_payload(values);
}

/* micro_payload(record) */
static json_t *ce_micro_payload(json_t *record)
{
    const char *raw = NULL;
    json_t *outcome_node = NULL;
    if (json_is_object(record)) {
        outcome_node = json_obj_get(record, "outcome");
        if (json_is_string(outcome_node)) raw = json_string_value(outcome_node);
    }
    const char *outcome = micro_outcome_for(raw);
    json_t *before = ce_int(json_obj_get(record, "tokens_before"));
    json_t *after = ce_int(json_obj_get(record, "tokens_after"));
    json_t *reclaimed = json_new_null();
    if (!json_is_null(before) && !json_is_null(after))
        reclaimed = json_new_number(json_number_value(before) - json_number_value(after));
    json_t *v = json_new_object();
    json_object_set(v, "kind", json_new_string("micro_summarize"));
    json_object_set(v, "scope", json_new_string("history"));
    json_object_set(v, "official", json_new_bool(0));
    json_object_set(v, "trigger", json_new_string("between_turns"));
    json_object_set(v, "outcome", json_new_string(outcome));
    json_object_set(v, "method", json_new_string(strcmp(outcome, "committed") == 0 ? "llm_summary" : "none"));
    if (strcmp(outcome, "committed") == 0)
        json_object_set(v, "failure_class", json_new_string("none"));
    else
        json_object_set(v, "failure_class", ce_failure_class(outcome_node));
    json_object_set(v, "session_id", copy_or_null(json_obj_get(record, "session_id")));
    json_object_set(v, "tokens_before", before);
    json_object_set(v, "tokens_after", after);
    json_object_set(v, "tokens_reclaimed", reclaimed);
    json_object_set(v, "token_count_method", json_new_string("estimate_rough"));
    json_object_set(v, "messages_before", ce_int(json_obj_get(record, "messages_before")));
    json_object_set(v, "messages_after", ce_int(json_obj_get(record, "messages_after")));
    json_object_set(v, "duration_ms", ce_int(json_obj_get(record, "duration_ms")));
    json_object_set(v, "threshold_tokens", ce_int(json_obj_get(record, "threshold_tokens")));
    json_object_set(v, "context_limit", ce_int(json_obj_get(record, "context_limit")));
    json_object_set(v, "summarizer_model", copy_or_null(json_obj_get(record, "aux_model")));
    return ce_payload(v);
}

static json_t *ce_prune_payload(const char *session_id, double tb, double ta, double m, double tr)
{
    json_t *v = json_new_object();
    json_object_set(v, "kind", json_new_string("prune_tool_results"));
    json_object_set(v, "scope", json_new_string("history"));
    json_object_set(v, "official", json_new_bool(0));
    json_object_set(v, "trigger", json_new_string("proactive_prune"));
    json_object_set(v, "outcome", json_new_string("committed"));
    json_object_set(v, "method", json_new_string("deterministic_prune"));
    json_object_set(v, "failure_class", json_new_string("none"));
    json_object_set(v, "session_id", json_new_string(session_id ? session_id : ""));
    json_object_set(v, "tokens_before", json_new_number(tb));
    json_object_set(v, "tokens_after", json_new_number(ta));
    json_object_set(v, "tokens_reclaimed", json_new_number(tb - ta));
    json_object_set(v, "token_count_method", json_new_string("estimate_rough"));
    json_object_set(v, "messages_before", json_new_number(m));
    json_object_set(v, "messages_after", json_new_number(m));
    json_object_set(v, "tool_results_pruned", json_new_number(tr));
    return ce_payload(v);
}

static json_t *ce_provider_native_payload(const char *session_id, json_t *compression_count)
{
    json_t *v = json_new_object();
    json_object_set(v, "kind", json_new_string("provider_native"));
    json_object_set(v, "scope", json_new_string("provider"));
    json_object_set(v, "official", json_new_bool(0));
    json_object_set(v, "trigger", json_new_string("provider"));
    json_object_set(v, "outcome", json_new_string("committed"));
    json_object_set(v, "method", json_new_string("provider"));
    json_object_set(v, "failure_class", json_new_string("none"));
    json_object_set(v, "session_id", json_new_string(session_id ? session_id : ""));
    json_object_set(v, "compression_count", ce_int(compression_count));
    return ce_payload(v);
}

/* ---- exported wrappers -------------------------------------------------- */

json_t *w9_compactione_f03b0c_closed(json_t *a)
{
    json_t *allowed = json_obj_get(a, "allowed");
    const json_t *d = json_obj_get(a, "dflt");
    const char *dflt = json_is_string(d) ? json_string_value(d) : NULL;
    json_t *out = json_new_array();
    if (json_is_array(allowed)) {
        for (size_t i = 0; i < json_len(allowed); i++) json_append(out, json_copy(json_get(allowed, i)));
    }
    json_t *r = ce_closed(json_obj_get(a, "value"),
                          (const char *const *)NULL, 0, dflt); /* placeholder, replaced below */
    json_free(r);
    /* Build allowed[] from the JSON array. */
    const char **buf = (const char **)calloc(json_is_array(out) ? json_len(out) + 1 : 1, sizeof(char *));
    size_t n = json_is_array(out) ? json_len(out) : 0;
    for (size_t i = 0; i < n; i++) buf[i] = json_string_value(json_get(out, i));
    r = ce_closed(json_obj_get(a, "value"), buf, (int)n, dflt);
    free(buf);
    json_free(out);
    return r;
}

json_t *w9_compactione_f03b0c_int(json_t *a)
{
    return ce_int(json_obj_get(a, "value"));
}

json_t *w9_compactione_f03b0c_failure_class(json_t *a)
{
    return ce_failure_class(json_obj_get(a, "value"));
}

json_t *w9_compactione_f03b0c_event_name(json_t *a)
{
    return json_new_string(ce_event_name(json_obj_get(a, "payload")));
}

json_t *w9_compactione_f03b0c_attempt_outcome(json_t *a)
{
    return ce_attempt_outcome(json_obj_get(a, "cs"), json_obj_get(a, "failure"), json_obj_get(a, "hr"));
}

json_t *w9_compactione_f03b0c_attempt_trigger(json_t *a)
{
    return ce_attempt_trigger(json_obj_get(a, "trigger_source"));
}

json_t *w9_compactione_f03b0c_payload(json_t *a)
{
    return ce_payload(json_obj_get(a, "values"));
}

json_t *w9_compactione_f03b0c_attempt_payload(json_t *a)
{
    return ce_attempt_payload(json_obj_get(a, "record"), json_obj_get(a, "cc"));
}

json_t *w9_compactione_f03b0c_micro_payload(json_t *a)
{
    return ce_micro_payload(json_obj_get(a, "record"));
}

json_t *w9_compactione_f03b0c_prune_payload(json_t *a)
{
    return ce_prune_payload(json_get_str(a, "session_id", ""),
                            json_get_num(a, "tb", 0), json_get_num(a, "ta", 0),
                            json_get_num(a, "m", 0), json_get_num(a, "tr", 0));
}

json_t *w9_compactione_f03b0c_provider_native_payload(json_t *a)
{
    return ce_provider_native_payload(json_get_str(a, "session_id", ""), json_obj_get(a, "cc"));
}
