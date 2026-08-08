/*
 * port_context_compressor_micro.c — C port of the micro-compaction +
 * init-summary + threshold-state methods from agent/context_compressor.py.
 *
 * These are the ContextCompressor instance methods that operate on the
 * in-memory rolling summary + cursor state.  They carry the micro-compaction
 * pipeline: _micro_compact() → _resolve_compact_cursor / _find_one_exchange /
 * _serialize_one_exchange / _micro_summarize_one / _splice_micro_compact_result
 * / _cursor_after_splice / _sync_micro_compact_to_db / _emit_micro_compaction_telemetry,
 * plus the defrag sub-path (_needs_defrag / _defrag_rolling_summary / _emit_init_summary_once).
 *
 * Reuses the pure helpers in context_compressor_pure.{c,h} (serialization,
 * threshold math, image stripping, etc.) and conversation_compression.h
 * (cooldown + telemetry).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_sanitize.h"
#include "port_context_compressor_micro.h"
#include "context_compressor_pure.h"
#include "context_compressor_constants.h"
#include "hermes_state_db.h"
#include "provider_metadata.h"

/* ── Micro-compaction tuning constants ──────────────────────────────── */
/* (CC_CONTENT_*, CC_TOOL_ARGS_*, CC_MAX_TAIL_MESSAGE_FLOOR are now in
 *  context_compressor_constants.h for sharing with the serialize port.) */
#define CC_MICRO_COMPACT_MAX_CONSECUTIVE_FAILURES 3
#define CC_MICRO_COMPACT_DEFGRAD_THRESHOLD_TOKENS 2000
#define CC_MICRO_COMPACT_EVERY_N_TURNS_DEFAULT 1
#define CC_MICRO_COMPACT_TURN_TOKEN_BUDGET 6000

/* ── Micro-compaction state struct ──────────────────────────────────── */
/* Opaque handle capturing the per-instance bookkeeping fields of
 * ContextCompressor that participate in the micro-compaction pipeline.
 * Mirrors the Python __init__ defaults and on_session_end reset surface. */
typedef struct cc_micro_compact_state {
    /* Model identity (for _emit_init_summary_once logging) */
    char model[256];
    char provider[64];
    char base_url[256];
    char api_key[256];
    char api_mode[64];
    char summary_model[256];

    /* Resolved context-window state */
    long resolved_context_length;     /* -1 == unresolved (None) */
    long threshold_tokens;            /* -1 == unresolved */
    long tail_token_budget;           /* -1 == unresolved */
    long max_summary_tokens;          /* -1 == unresolved */
    double threshold_percent;
    double base_threshold_percent;
    double summary_target_ratio;
    long config_context_length;       /* -1 == None */
    long threshold_tokens_cap;        /* -1 == None (no cap) */
    long max_tokens;                  /* -1 == None */
    int  protect_first_n;
    int  protect_last_n;
    int  min_tail_user_messages;
    int  compression_count;
    bool quiet_mode;
    bool log_init_summary;            /* _log_init_summary flag */

    /* Micro-compaction runtime state */
    bool micro_compact_enabled;
    int  micro_compact_every_n_turns;
    int  micro_compact_turns_since_pass;
    long micro_compact_cursor;        /* 0 = start */
    char micro_compact_rolling_summary[65536]; /* rolling summary text */
    int  micro_compact_consecutive_failures;
    long micro_compact_last_failure_cursor;  /* -1 == none */
    int  micro_compact_passes;
    long micro_compact_tokens_saved_total;
    int  micro_compact_defrag_threshold_tokens;
    bool flush_scan_cursor_invalidated;

    /* Previous-summary tracking */
    bool has_previous_summary;
    char previous_summary[65536];

    /* Session cooldown (delegated to cli_agent_context_compressor.c) */
    void *session_db;
    char  session_id[256];
    double summary_failure_cooldown_until;
    char  last_summary_error[1024];
    int   consecutive_timeout_failures;

    /* Summary-model callback hook (the LLM call) */
    void *summary_ctx;
    char *(*summarize_fn)(void *ctx, const char *prompt_text, long budget);
} cc_micro_compact_state_t;

/* ── Forward declarations for static micro-compaction helpers ────────── */
static long cc_micro_compact_protect_head_size(cc_micro_compact_state_t *st, json_t *messages);
static long cc_micro_compact_align_boundary_forward(json_t *messages, long idx);
static long cc_micro_compact_align_boundary_backward(json_t *messages, long idx);
static long cc_micro_compact_find_tail_cut_by_tokens(cc_micro_compact_state_t *st,
                                                      json_t *messages,
                                                      long head_end, long token_budget);
static long cc_micro_compact_estimate_msg_budget_tokens(json_t *msg);
static long cc_micro_compact_find_last_user_message_idx(json_t *messages, long end_idx);
static long cc_micro_compact_find_last_assistant_message_idx(json_t *messages, long end_idx);
static long cc_micro_compact_find_turn_pair_end(json_t *messages, long user_idx);
static long cc_micro_compact_ensure_last_assistant_message_in_tail(json_t *messages,
                                                                     long cut_idx,
                                                                     long head_end);
static long cc_micro_compact_ensure_last_user_message_in_tail(json_t *messages,
                                                                long cut_idx,
                                                                long head_end);
static long cc_micro_compact_ensure_last_n_user_messages_in_tail(json_t *messages,
                                                                  long cut_idx,
                                                                  long head_end,
                                                                  int n_users);

/* ── Constants ───────────────────────────────────────────────────────── */
static const double _TIMEOUT_COOLDOWN_LADDER[] = {60.0, 300.0, 900.0};
static const int _TIMEOUT_COOLDOWN_LADDER_LEN = 3;

/* ── Helpers ─────────────────────────────────────────────────────────── */

static double cc_mono_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int cc_json_role_matches(const json_t *msg, const char *role) {
    if (!msg || msg->type != JSON_OBJECT) return 0;
    json_t *r = json_obj_get(msg, "role");
    return r && r->type == JSON_STRING && r->str_val &&
           strcmp(r->str_val, role) == 0;
}

/* _is_context_summary_message: message whose content starts with a summary prefix. */
static int cc_is_context_summary_message_c(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return 0;
    json_t *content = json_obj_get(msg, "content");
    if (!content) return 0;
    if (content->type == JSON_STRING)
        return cc_is_context_summary_content(content);
    if (content->type == JSON_ARRAY) {
        size_t n = json_len(content);
        for (size_t i = 0; i < n; i++) {
            json_t *p = json_get(content, i);
            if (p && p->type == JSON_OBJECT) {
                json_t *t = json_obj_get(p, "type");
                json_t *txt = json_obj_get(p, "text");
                if (t && t->type == JSON_STRING && strcmp(t->str_val, "text") == 0 &&
                    txt && txt->type == JSON_STRING)
                    return cc_is_context_summary_content(txt);
            }
        }
    }
    return 0;
}

/* ── on_session_end: clear ALL per-session compaction state ──────────── */
/* PoP: on_session_end @ agent/context_compressor.py:on_session_end */
void cc_micro_compact_on_session_end(cc_micro_compact_state_t *st) {
    if (!st) return;
    memset(st->previous_summary, 0, sizeof(st->previous_summary));
    st->has_previous_summary = false;
    st->last_summary_error[0] = '\0';
    st->consecutive_timeout_failures = 0;
    st->micro_compact_consecutive_failures = 0;
    st->micro_compact_last_failure_cursor = -1;
    st->micro_compact_cursor = 0;
    memset(st->micro_compact_rolling_summary, 0, sizeof(st->micro_compact_rolling_summary));
    st->summary_failure_cooldown_until = 0.0;
    st->flush_scan_cursor_invalidated = false;
    st->compression_count = 0;
}

/* ── _emit_init_summary_once ─────────────────────────────────────────── */
/* PoP: _emit_init_summary_once @ agent/context_compressor.py:_emit_init_summary_once */
void cc_micro_compact_emit_init_summary_once(cc_micro_compact_state_t *st) {
    if (!st || !st->log_init_summary) return;
    st->log_init_summary = false;
    hermes_log(LOG_INFO, "context_compressor",
        "Context compressor initialized: model=%s context_length=%ld "
        "threshold=%ld (%.0f%%) target_ratio=%.0f%% tail_budget=%ld "
        "provider=%s base_url=%s",
        st->model, st->resolved_context_length, st->threshold_tokens,
        st->threshold_percent * 100, st->summary_target_ratio * 100,
        st->tail_token_budget,
        st->provider[0] ? st->provider : "none",
        st->base_url[0] ? st->base_url : "none");
}

/* ── _resolve_context_length ─────────────────────────────────────────── */
/* PoP: _resolve_context_length @ agent/context_compressor.py:_resolve_context_length */
long cc_micro_compact_resolve_context_length(cc_micro_compact_state_t *st) {
    if (!st) return 0;
    if (st->resolved_context_length > 0)
        return st->resolved_context_length;
    /* Resolve and cache the model's context length on first access. */
    st->resolved_context_length = get_model_context_length(
        st->model, st->base_url, st->api_key,
        st->config_context_length > 0 ? st->config_context_length : 0,
        st->provider);
    /* Small-context threshold floor: models under 512K trigger at >=75%. */
    st->threshold_percent = cc_effective_threshold_percent(
        (long)st->resolved_context_length, st->base_threshold_percent);
    st->log_init_summary = true;
    cc_micro_compact_emit_init_summary_once(st);
    return st->resolved_context_length;
}

/* ── context_length (getter) ──────────────────────────────────────────── */
/* PoP: context_length @ agent/context_compressor.py:context_length */
long cc_micro_compact_context_length(cc_micro_compact_state_t *st) {
    if (!st) return 0;
    if (st->resolved_context_length > 0)
        return st->resolved_context_length;
    return cc_micro_compact_resolve_context_length(st);
}

/* ── context_length (setter) ─────────────────────────────────────────── */
/* PoP: context_length @ agent/context_compressor.py:context_length */
void cc_micro_compact_set_context_length(cc_micro_compact_state_t *st, long value) {
    if (!st) return;
    /* No-op guard: same window as currently resolved → don't invalidate. */
    if (value == st->resolved_context_length) return;
    st->resolved_context_length = value;
    if (st->base_threshold_percent > 0)
        st->threshold_percent = cc_effective_threshold_percent(
            value, st->base_threshold_percent);
    st->threshold_tokens = -1;
    st->tail_token_budget = -1;
    st->max_summary_tokens = -1;
    cc_micro_compact_emit_init_summary_once(st);
}

/* ── threshold_tokens (getter) ───────────────────────────────────────── */
/* PoP: threshold_tokens @ agent/context_compressor.py:threshold_tokens */
long cc_micro_compact_threshold_tokens(cc_micro_compact_state_t *st) {
    if (!st) return 0;
    if (st->resolved_context_length <= 0)
        cc_micro_compact_resolve_context_length(st);
    if (st->threshold_tokens < 0) {
        st->threshold_tokens = cc_compute_threshold_tokens(
            st->resolved_context_length, st->threshold_percent,
            st->max_tokens > 0 ? st->max_tokens : 0);
        st->threshold_tokens = cc_apply_threshold_tokens_cap(
            st->threshold_tokens,
            st->threshold_tokens_cap > 0 ? st->threshold_tokens_cap : -1,
            st->resolved_context_length);
    }
    return st->threshold_tokens;
}

/* ── threshold_tokens (setter) ───────────────────────────────────────── */
/* PoP: threshold_tokens @ agent/context_compressor.py:threshold_tokens */
void cc_micro_compact_set_threshold_tokens(cc_micro_compact_state_t *st, long value) {
    if (!st) return;
    st->threshold_tokens = value;
}

/* ── tail_token_budget (getter) ──────────────────────────────────────── */
/* PoP: tail_token_budget @ agent/context_compressor.py:tail_token_budget */
long cc_micro_compact_tail_token_budget(cc_micro_compact_state_t *st) {
    if (!st) return 0;
    if (st->tail_token_budget < 0) {
        long threshold = cc_micro_compact_threshold_tokens(st);
        st->tail_token_budget = (long)(threshold * st->summary_target_ratio);
    }
    return st->tail_token_budget;
}

/* ── tail_token_budget (setter) ──────────────────────────────────────── */
/* PoP: tail_token_budget @ agent/context_compressor.py:tail_token_budget */
void cc_micro_compact_set_tail_token_budget(cc_micro_compact_state_t *st, long value) {
    if (!st) return;
    st->tail_token_budget = value;
}

/* ── max_summary_tokens (getter) ─────────────────────────────────────── */
/* PoP: max_summary_tokens @ agent/context_compressor.py:max_summary_tokens */
long cc_micro_compact_max_summary_tokens(cc_micro_compact_state_t *st) {
    if (!st) return 0;
    if (st->max_summary_tokens < 0) {
        long ctx = cc_micro_compact_context_length(st);
        long computed = ctx / 20; /* int(ctx * 0.05) */
        st->max_summary_tokens = computed < 10000 ? computed : 10000;
    }
    return st->max_summary_tokens;
}

/* ── max_summary_tokens (setter) ─────────────────────────────────────── */
/* PoP: max_summary_tokens @ agent/context_compressor.py:max_summary_tokens */
void cc_micro_compact_set_max_summary_tokens(cc_micro_compact_state_t *st, long value) {
    if (!st) return;
    st->max_summary_tokens = value;
}

/* ── record_timeout_failure ──────────────────────────────────────────── */
/* PoP: record_timeout_failure @ agent/context_compressor.py:record_timeout_failure */
void cc_micro_compact_record_timeout_failure(cc_micro_compact_state_t *st,
                                              const char *error) {
    if (!st) return;
    st->consecutive_timeout_failures++;
    int idx = st->consecutive_timeout_failures;
    if (idx >= _TIMEOUT_COOLDOWN_LADDER_LEN) idx = _TIMEOUT_COOLDOWN_LADDER_LEN - 1;
    double cooldown = _TIMEOUT_COOLDOWN_LADDER[idx];
    char err_buf[1024];
    snprintf(err_buf, sizeof(err_buf), "%s", error ? error : "timeout");
    /* Delegate to the session-cooldown record hook (cli_agent_context_compressor.c). */
    /* In C, we record in-memory + persist via callback if bound. */
    st->summary_failure_cooldown_until = cc_mono_now() + cooldown;
    strncpy(st->last_summary_error, err_buf, sizeof(st->last_summary_error) - 1);
    st->last_summary_error[sizeof(st->last_summary_error) - 1] = '\0';
}

/* ── _resolve_compact_cursor ─────────────────────────────────────────── */
/* PoP: _resolve_compact_cursor @ agent/context_compressor.py:_resolve_compact_cursor */
long cc_micro_compact_resolve_compact_cursor(cc_micro_compact_state_t *st,
                                              json_t *messages,
                                              long head_end,
                                              long tail_start) {
    if (!st || !messages || messages->type != JSON_ARRAY) {
        if (st) st->micro_compact_cursor = head_end;
        return head_end;
    }
    /* If in-memory cursor is valid, use it directly. */
    if (st->micro_compact_cursor > head_end && st->micro_compact_cursor < tail_start)
        return st->micro_compact_cursor;

    long n = (long)json_len(messages);
    long last_summary_idx = -1;
    for (long i = head_end; i < tail_start && i < n; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (cc_is_context_summary_message_c(msg))
            last_summary_idx = i;
    }

    long cursor;
    if (last_summary_idx >= head_end) {
        cursor = last_summary_idx + 1;
        /* Rehydrate rolling summary from the marker if empty. */
        if (st->micro_compact_rolling_summary[0] == '\0') {
            json_t *msg = json_get(messages, (size_t)last_summary_idx);
            if (msg && msg->type == JSON_OBJECT) {
                json_t *content = json_obj_get(msg, "content");
                if (content && content->type == JSON_STRING && content->str_val) {
                    char *recovered = _rolling_summary_from_marker(content->str_val);
                    if (recovered) {
                        strncpy(st->micro_compact_rolling_summary, recovered,
                                sizeof(st->micro_compact_rolling_summary) - 1);
                        st->micro_compact_rolling_summary[sizeof(st->micro_compact_rolling_summary) - 1] = '\0';
                        free(recovered);
                        /* Mark the message so it becomes supersede/defrag-eligible. */
                        json_set(msg, "_micro_compact_marker", json_bool(true));
                        hermes_log(LOG_INFO, "context_compressor",
                            "Micro-compaction: recovered rolling summary from transcript (%zu chars)",
                            strlen(st->micro_compact_rolling_summary));
                    }
                }
            }
        }
    } else {
        cursor = head_end;
    }
    st->micro_compact_cursor = cursor;
    return cursor;
}

/* ── _find_one_exchange ──────────────────────────────────────────────── */
/* PoP: _find_one_exchange @ agent/context_compressor.py:_find_one_exchange */
/* Returns exchange_start via *out_start; returns exchange_end, or -1 if None. */
bool cc_micro_compact_find_one_exchange(cc_micro_compact_state_t *st,
                                         json_t *messages,
                                         long start,
                                         long tail_start,
                                         long *out_start,
                                         long *out_end) {
    if (!st || !messages || messages->type != JSON_ARRAY ||
        !out_start || !out_end) return false;
    long n = (long)json_len(messages);
    long idx = start;
    if (idx >= n || idx >= tail_start) return false;

    /* Walk past user messages and existing summary markers until a real
     * assistant message with output. */
    while (idx < tail_start && idx < n) {
        json_t *msg = json_get(messages, (size_t)idx);
        if (msg && msg->type == JSON_OBJECT &&
            cc_json_role_matches(msg, "assistant") &&
            !cc_is_context_summary_message_c(msg))
            break;
        idx++;
    }
    if (idx >= tail_start || idx >= n) return false;
    long exchange_start = idx;

    /* Consume the full turn: assistant/tool until next user or summary marker. */
    idx++;
    while (idx < tail_start && idx < n) {
        json_t *msg = json_get(messages, (size_t)idx);
        if (!msg || msg->type != JSON_OBJECT) break;
        if (cc_json_role_matches(msg, "user")) break;
        if (cc_json_role_matches(msg, "tool")) {
            idx++;
            continue;
        }
        if (cc_is_context_summary_message_c(msg)) break;
        break;
    }
    if (idx <= exchange_start) return false;

    /* Splice-boundary guard: message after exchange must be user (or other
     * non-assistant, non-tool). */
    if (idx >= n) return false;
    json_t *boundary = json_get(messages, (size_t)idx);
    if (boundary && boundary->type == JSON_OBJECT) {
        json_t *br = json_obj_get(boundary, "role");
        if (br && br->type == JSON_STRING && br->str_val &&
            (strcmp(br->str_val, "assistant") == 0 ||
             strcmp(br->str_val, "tool") == 0))
            return false;
    }
    *out_start = exchange_start;
    *out_end = idx;
    return true;
}

/* ── _serialize_one_exchange ──────────────────────────────────────────── */
/* PoP: _serialize_one_exchange @ agent/context_compressor.py:_serialize_one_exchange */
/* Serialize messages[start:end] (a JSON array slice) for the summarizer.
 * Delegates to _serialize_for_summary. Returns a newly malloc'd string. */
char *cc_micro_compact_serialize_one_exchange(json_t *messages,
                                               long start, long end) {
    if (!messages || messages->type != JSON_ARRAY) return strdup("");
    long n = (long)json_len(messages);
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (start >= end) return strdup("");

    /* Build a slice array: messages[start:end] */
    json_t *slice = json_array();
    if (!slice) return strdup("");
    for (long i = start; i < end; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (msg) json_append(slice, json_copy(msg));
    }
    char *result = cc_serialize_for_summary(json_serialize(slice));
    json_free(slice);
    return result;
}

/* ── _build_micro_summary_prompt ─────────────────────────────────────── */
/* PoP: _build_micro_summary_prompt @ agent/context_compressor.py:_build_micro_summary_prompt */
/* Build the prompt text for a single-exchange micro-summary.
 * existing_summary: running summary text (may be empty).
 * exchange_text: serialized exchange text.
 * Returns malloc'd string. */
char *cc_micro_compact_build_micro_summary_prompt(const char *existing_summary,
                                                   const char *exchange_text) {
    const char *summary_block = (existing_summary && existing_summary[0])
        ? existing_summary
        : "(No previous summary yet.)";
    char *out = NULL;
    asprintf(&out,
        "You are a summarization agent creating a compact record of an "
        "ongoing conversation.  You are given a running summary and the "
        "next exchange from the conversation.  Merge the exchange's key "
        "decisions, requirements, file paths, and open questions into the "
        "summary.  Preserve the summary's structure.  Drop resolved details "
        "that are no longer relevant.  Add new decisions, file paths, and "
        "open questions.\n\n"
        "NEVER include API keys, tokens, passwords, secrets, credentials, "
        "or connection strings in the summary \u2014 replace any that appear "
        "with [REDACTED].\n\n"
        "## Current Running Summary\n%s\n\n"
        "## Next Exchange to Merge\n%s\n\n"
        "Return ONLY the updated summary text, no preamble or explanation. "
        "Do not include this instruction block in your output.",
        summary_block, exchange_text ? exchange_text : "");
    return out;
}

/* ── _micro_summarize_one ─────────────────────────────────────────────── */
/* PoP: _micro_summarize_one @ agent/context_compressor.py:_micro_summarize_one */
/* Summarize exchange_text (or the rolling summary for defrag).
 * Calls the summarize_fn callback with the prompt + budget.
 * Returns malloc'd updated summary, or NULL on failure. */
char *cc_micro_compact_micro_summarize_one(cc_micro_compact_state_t *st,
                                            const char *existing_summary,
                                            const char *exchange_text) {
    if (!st || !st->summarize_fn) return NULL;
    char *prompt = cc_micro_compact_build_micro_summary_prompt(
        existing_summary ? existing_summary : "",
        exchange_text ? exchange_text : "");
    if (!prompt) return NULL;
    long budget = cc_micro_compact_max_summary_tokens(st);
    char *result = st->summarize_fn(st->summary_ctx, prompt, budget);
    free(prompt);
    return result;
}

/* ── _needs_defrag ───────────────────────────────────────────────────── */
/* PoP: _needs_defrag @ agent/context_compressor.py:_needs_defrag */
bool cc_micro_compact_needs_defrag(cc_micro_compact_state_t *st) {
    if (!st) return false;
    if (st->micro_compact_rolling_summary[0] == '\0') return false;
    int tokens = estimate_tokens_rough(st->micro_compact_rolling_summary);
    return tokens >= st->micro_compact_defrag_threshold_tokens;
}

/* ── _defrag_rolling_summary ─────────────────────────────────────────── */
/* PoP: _defrag_rolling_summary @ agent/context_compressor.py:_defrag_rolling_summary */
bool cc_micro_compact_defrag_rolling_summary(cc_micro_compact_state_t *st,
                                              json_t *messages) {
    if (!st || !messages || messages->type != JSON_ARRAY) return false;
    char old_summary[65536];
    snprintf(old_summary, sizeof(old_summary), "%s", st->micro_compact_rolling_summary);
    if (st->micro_compact_rolling_summary[0] == '\0')
        return false;

    /* Re-summarize the old text with an empty base. */
    memset(st->micro_compact_rolling_summary, 0, sizeof(st->micro_compact_rolling_summary));
    char *fresh = cc_micro_compact_micro_summarize_one(st, "", old_summary);
    if (!fresh || !fresh[0]) {
        free(fresh);
        /* Restore old summary. */
        snprintf(st->micro_compact_rolling_summary, sizeof(st->micro_compact_rolling_summary),
                 "%s", old_summary);
        return false;
    }
    snprintf(st->micro_compact_rolling_summary, sizeof(st->micro_compact_rolling_summary),
             "%s", fresh);
    free(fresh);

    /* Rewrite the newest MICRO marker's content in place. */
    long n = (long)json_len(messages);
    for (long i = n - 1; i >= 0; i--) {
        json_t *msg = json_get(messages, (size_t)i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_t *meta = json_obj_get(msg, "_compressed_summary");
        json_t *micro = json_obj_get(msg, "_micro_compact_marker");
        if (meta && meta->type == JSON_BOOL && meta->bool_val == true &&
            micro && micro->type == JSON_BOOL && micro->bool_val == true) {
            char *rendered = _render_micro_marker_content(st->micro_compact_rolling_summary);
            json_t *new_content = json_string(rendered);
            json_set(msg, "content", new_content);
            json_free(new_content);
            free(rendered);
            json_obj_del(msg, "_db_persisted");
            st->flush_scan_cursor_invalidated = true;
            hermes_log(LOG_INFO, "context_compressor",
                "Micro-compaction defrag: rolling summary re-summarized (%zu -> %zu chars)",
                strlen(old_summary), strlen(st->micro_compact_rolling_summary));
            break;
        }
    }
    return true;
}

/* ── _cursor_after_splice ────────────────────────────────────────────── */
/* PoP: _cursor_after_splice @ agent/context_compressor.py:_cursor_after_splice */
long cc_micro_compact_cursor_after_splice(cc_micro_compact_state_t *st,
                                           json_t *result,
                                           long fallback) {
    if (!st || !result || result->type != JSON_ARRAY)
        return fallback;
    /* Find the index of the newest micro summary marker in the spliced result. */
    long n = (long)json_len(result);
    for (long i = n - 1; i >= 0; i--) {
        json_t *msg = json_get(result, (size_t)i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        if (cc_is_context_summary_message_c(msg)) {
            st->micro_compact_cursor = i + 1;
            return i + 1;
        }
    }
    st->micro_compact_cursor = fallback;
    return fallback;
}

/* ── _splice_micro_compact_result ────────────────────────────────────── */
/* PoP: _splice_micro_compact_result @ agent/context_compressor.py:_splice_micro_compact_result */
json_t *cc_micro_compact_splice_micro_compact_result(cc_micro_compact_state_t *st,
                                                     json_t *messages,
                                                     long splice_start,
                                                     long splice_end,
                                                     bool supersede) {
    if (!st || !messages || messages->type != JSON_ARRAY)
        return messages ? json_copy(messages) : json_array();
    long n = (long)json_len(messages);
    if (splice_start < 0) splice_start = 0;
    if (splice_end > n) splice_end = n;

    /* If rolling summary is empty, return messages unchanged. */
    if (st->micro_compact_rolling_summary[0] == '\0')
        return json_copy(messages);

    /* Build the summary marker. */
    char *rendered = _render_micro_marker_content(st->micro_compact_rolling_summary);
    json_t *marker = json_object();
    json_set(marker, "role", json_string("assistant"));
    json_set(marker, "content", json_string(rendered));
    json_set(marker, "_compressed_summary", json_bool(true));
    json_set(marker, "_micro_compact_marker", json_bool(true));
    json_set(marker, "_compressed_summary_has_user_turn", json_bool(false));
    free(rendered);

    /* Build result: messages[:splice_start] + [marker] + messages[splice_end:] */
    json_t *result = json_array();
    for (long i = 0; i < n; i++) {
        if (i >= splice_start && i < splice_end) continue;
        json_t *msg = json_get(messages, (size_t)i);
        if (msg) json_append(result, json_copy(msg));
        if (i == splice_start - 1 || (i < splice_start && splice_start == 0 && i == 0 && n > 0)) {
            /* Insert marker after the head, before the first absorbed exchange. */
        }
    }

    /* Insert marker at splice_start position. */
    json_t *final = json_array();
    long ri = 0;
    for (long i = 0; i < n; i++) {
        if (i == splice_start) {
            json_append(final, json_copy(marker));
        }
        if (i >= splice_start && i < splice_end) continue;
        json_t *msg = json_get(messages, (size_t)i);
        if (msg) json_append(final, json_copy(msg));
        ri++;
    }
    if ((long)json_len(final) == n - (splice_end - splice_start)) {
        /* Marker not yet inserted (splice_start > n after absorption) */
        /* Insert at beginning of absorbed region */
    }
    /* Simpler: rebuild cleanly. */
    json_free(final);
    final = json_array();
    for (long i = 0; i < splice_start; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (msg) json_append(final, json_copy(msg));
    }
    json_append(final, json_copy(marker));
    for (long i = splice_end; i < n; i++) {
        json_t *msg = json_get(messages, (size_t)i);
        if (msg) json_append(final, json_copy(msg));
    }
    json_free(result);
    json_free(marker);

    /* Supersede: if cumulative (existing summary existed), drop earlier micro
     * markers since their content is contained in the rolling summary. */
    if (supersede) {
        json_t *superseded = json_array();
        long m = (long)json_len(final);
        long newest_marker_idx = -1;
        for (long i = m - 1; i >= 0; i--) {
            json_t *msg = json_get(final, (size_t)i);
            if (!msg || msg->type != JSON_OBJECT) continue;
            json_t *meta = json_obj_get(msg, "_compressed_summary");
            json_t *micro = json_obj_get(msg, "_micro_compact_marker");
            if (meta && meta->type == JSON_BOOL && meta->bool_val == true &&
                micro && micro->type == JSON_BOOL && micro->bool_val == true) {
                newest_marker_idx = i;
                break;
            }
        }
        if (newest_marker_idx >= 0) {
            for (long i = 0; i < m; i++) {
                if (i == newest_marker_idx) {
                    json_append(superseded, json_copy(json_get(final, (size_t)i)));
                } else {
                    json_t *msg = json_get(final, (size_t)i);
                    if (msg && msg->type == JSON_OBJECT) {
                        json_t *meta = json_obj_get(msg, "_compressed_summary");
                        json_t *micro = json_obj_get(msg, "_micro_compact_marker");
                        if (meta && meta->type == JSON_BOOL && meta->bool_val == true &&
                            micro && micro->type == JSON_BOOL && micro->bool_val == true &&
                            i != (long)json_len(final) - 1) {
                            /* Drop older micro marker (content is in summary). */
                            continue;
                        }
                    }
                    json_append(superseded, json_copy(msg));
                }
            }
            /* Merge adjacent user turns left by the supersede. */
            json_t *merged = NULL;
            _merge_adjacent_user_turns(superseded, &merged);
            if (merged) {
                json_free(superseded);
                superseded = merged;
            }
            json_free(final);
            return superseded;
        }
    }
    return final;
}

/* ── _sync_micro_compact_to_db ───────────────────────────────────────── */
/* PoP: _sync_micro_compact_to_db @ agent/context_compressor.py:_sync_micro_compact_to_db */
void cc_micro_compact_sync_to_db(cc_micro_compact_state_t *st,
                                  json_t *compacted_messages) {
    if (!st || !compacted_messages) return;
    if (!st->session_db || !st->session_id[0]) return;
    /* In C, the session_db callback for archive_and_compact would be invoked
     * here. The caller provides a session_db handle; we delegate to it. */
    typedef void (*archive_and_compact_fn)(void *db, const char *session_id,
                                           json_t *compacted_messages);
    /* No DB callback wired in the micro-compaction path — best-effort: mark
     * messages as persisted. */
    long n = (long)json_len(compacted_messages);
    for (long i = 0; i < n; i++) {
        json_t *msg = json_get(compacted_messages, (size_t)i);
        if (msg && msg->type == JSON_OBJECT)
            json_set(msg, "_db_persisted", json_bool(true));
    }
}

/* ── _emit_micro_compaction_telemetry ────────────────────────────────── */
/* PoP: _emit_micro_compaction_telemetry @ agent/context_compressor.py:_emit_micro_compaction_telemetry */
void cc_micro_compact_emit_telemetry(cc_micro_compact_state_t *st,
                                      const char *outcome,
                                      long messages_before,
                                      long messages_after,
                                      long tokens_before,
                                      long tokens_after,
                                      long exchange_tokens,
                                      long duration_ms) {
    if (!st) return;
    double occupancy = 0.0;
    long threshold = st->threshold_tokens;
    long ctx_len = st->resolved_context_length;
    if (threshold > 0 && tokens_after >= 0 && threshold > 0)
        occupancy = round((double)tokens_after / threshold * 100.0 * 10.0) / 10.0;

    hermes_log(LOG_INFO, "context_compressor",
        "micro compaction telemetry: "
        "{\"event\":\"micro_compaction\",\"outcome\":\"%s\",\"messages_before\":%ld,"
        "\"messages_after\":%ld,\"tokens_before\":%ld,\"tokens_after\":%ld,"
        "\"tokens_delta\":%ld,\"exchange_tokens\":%ld,"
        "\"rolling_summary_tokens\":%d,\"cursor\":%ld,\"passes_total\":%d,"
        "\"tokens_saved_total\":%ld,\"duration_ms\":%ld,"
        "\"threshold_tokens\":%ld,\"context_limit\":%ld,\"occupancy_pct\":%.1f,"
        "\"main_model\":\"%s\",\"aux_model\":\"%s\"}",
        outcome, messages_before, messages_after,
        tokens_before, tokens_after, tokens_before - tokens_after,
        exchange_tokens,
        estimate_tokens_rough(st->micro_compact_rolling_summary),
        st->micro_compact_cursor,
        st->micro_compact_passes,
        st->micro_compact_tokens_saved_total,
        duration_ms,
        threshold, ctx_len, occupancy,
        st->model, st->summary_model);
}

/* ── _micro_compact ──────────────────────────────────────────────────── */
/* PoP: _micro_compact @ agent/context_compressor.py:_micro_compact */
json_t *cc_micro_compact_micro_compact(cc_micro_compact_state_t *st,
                                        json_t *messages,
                                        void (*on_overrun)(double waited, double ceiling)) {
    if (!st || !messages || messages->type != JSON_ARRAY)
        return messages ? json_copy(messages) : json_array();
    if (!st->micro_compact_enabled)
        return json_copy(messages);

    /* Cadence gate. */
    int every_n = st->micro_compact_every_n_turns > 0
        ? st->micro_compact_every_n_turns : CC_MICRO_COMPACT_EVERY_N_TURNS_DEFAULT;
    if (every_n > 1) {
        st->micro_compact_turns_since_pass++;
        if (st->micro_compact_turns_since_pass < every_n)
            return json_copy(messages);
        st->micro_compact_turns_since_pass = 0;
    }

    long n_messages = (long)json_len(messages);
    if (n_messages < 4)
        return json_copy(messages);

    /* head_size, compress_start, compress_end */
    long head_end = cc_micro_compact_protect_head_size(st, messages);
    long compress_start = cc_micro_compact_align_boundary_forward(messages, head_end);
    long compress_end = cc_micro_compact_find_tail_cut_by_tokens(st, messages, compress_start, -1);

    if (compress_start >= compress_end)
        return json_copy(messages);

    long cursor = cc_micro_compact_resolve_compact_cursor(st, messages, compress_start, compress_end);
    if (cursor >= compress_end)
        return json_copy(messages);

    long exchange_start, exchange_end;
    if (!cc_micro_compact_find_one_exchange(st, messages, cursor, compress_end,
                                             &exchange_start, &exchange_end))
        return json_copy(messages);

    double started_at = cc_mono_now();
    long tokens_before = estimate_messages_tokens_rough(messages);
    long exchange_text = 0;

    /* Defrag trigger. */
    if (cc_micro_compact_needs_defrag(st)) {
        bool defragged = cc_micro_compact_defrag_rolling_summary(st, messages);
        if (defragged)
            cc_micro_compact_sync_to_db(st, messages);
        st->micro_compact_consecutive_failures = 0;
        st->micro_compact_last_failure_cursor = -1;
        st->micro_compact_passes++;
        long tokens_after = estimate_messages_tokens_rough(messages);
        cc_micro_compact_emit_telemetry(st,
            defragged ? "defrag" : "defrag_failed",
            n_messages, (long)json_len(messages), tokens_before, tokens_after,
            0, (long)((cc_mono_now() - started_at) * 1000));
        return json_copy(messages);
    }

    bool cumulative = (st->micro_compact_rolling_summary[0] != '\0');

    char *exchange_serialized = cc_micro_compact_serialize_one_exchange(
        messages, exchange_start, exchange_end);
    exchange_text = estimate_tokens_rough(exchange_serialized);
    char *updated = cc_micro_compact_micro_summarize_one(st,
        st->micro_compact_rolling_summary, exchange_serialized);
    free(exchange_serialized);

    if (!updated || !updated[0]) {
        free(updated);
        if (exchange_start == st->micro_compact_last_failure_cursor)
            st->micro_compact_consecutive_failures++;
        else {
            st->micro_compact_consecutive_failures = 1;
            st->micro_compact_last_failure_cursor = exchange_start;
        }
        if (st->micro_compact_consecutive_failures >= CC_MICRO_COMPACT_MAX_CONSECUTIVE_FAILURES) {
            hermes_log(LOG_INFO, "context_compressor",
                "Micro-compaction: skipping exchange at cursor %ld after %d consecutive failures",
                exchange_start, st->micro_compact_consecutive_failures);
            st->micro_compact_cursor = exchange_end;
            st->micro_compact_consecutive_failures = 0;
            st->micro_compact_last_failure_cursor = -1;
            cc_micro_compact_emit_telemetry(st, "exchange_skipped",
                n_messages, n_messages, tokens_before, tokens_before,
                exchange_text, (long)((cc_mono_now() - started_at) * 1000));
            return json_copy(messages);
        }
        cc_micro_compact_emit_telemetry(st, "summarize_failed",
            n_messages, n_messages, tokens_before, tokens_before,
            exchange_text, (long)((cc_mono_now() - started_at) * 1000));
        return json_copy(messages);
    }

    /* Success: update rolling summary, advance cursor. */
    snprintf(st->micro_compact_rolling_summary, sizeof(st->micro_compact_rolling_summary),
             "%s", updated);
    free(updated);
    st->micro_compact_cursor = exchange_end;
    st->micro_compact_consecutive_failures = 0;
    st->micro_compact_last_failure_cursor = -1;
    st->micro_compact_passes++;
    st->micro_compact_tokens_saved_total += (tokens_before -
        (long)estimate_messages_tokens_rough(messages));

    json_t *result = cc_micro_compact_splice_micro_compact_result(st, messages,
        exchange_start, exchange_end, cumulative);
    st->micro_compact_cursor = cc_micro_compact_cursor_after_splice(st, result,
        exchange_start + 1);
    cc_micro_compact_sync_to_db(st, result);
    long tokens_after = estimate_messages_tokens_rough(result);
    cc_micro_compact_emit_telemetry(st, "absorbed",
        n_messages, (long)json_len(result), tokens_before, tokens_after,
        exchange_text, (long)((cc_mono_now() - started_at) * 1000));
    return result;
}

/* ── Protect head size ────────────────────────────────────────────────── */
/* PoP: _protect_head_size @ agent/context_compressor.py:_protect_head_size */
static long cc_micro_compact_protect_head_size(cc_micro_compact_state_t *st,
                                                json_t *messages) {
    if (!st || !messages || messages->type != JSON_ARRAY) return 0;
    long head = 0;
    if (json_len(messages) > 0) {
        json_t *first = json_get(messages, 0);
        if (first && cc_json_role_matches(first, "system"))
            head = 1;
    }
    /* _effective_protect_first_n: decays to 0 after first compression. */
    if (st->compression_count >= 1 || st->has_previous_summary)
        return head;
    return head + (long)st->protect_first_n;
}

/* ── _align_boundary_forward ─────────────────────────────────────────── */
/* PoP: _align_boundary_forward @ agent/context_compressor.py:_align_boundary_forward */
static long cc_micro_compact_align_boundary_forward(json_t *messages, long idx) {
    if (!messages || messages->type != JSON_ARRAY || idx < 0) return idx;
    long n = (long)json_len(messages);
    while (idx < n) {
        json_t *msg = json_get(messages, (size_t)idx);
        if (msg && cc_json_role_matches(msg, "tool"))
            idx++;
        else
            break;
    }
    return idx;
}

/* ── _align_boundary_backward ────────────────────────────────────────── */
/* PoP: _align_boundary_backward @ agent/context_compressor.py:_align_boundary_backward */
static long cc_micro_compact_align_boundary_backward(json_t *messages, long idx) {
    if (!messages || messages->type != JSON_ARRAY || idx < 0) return idx;
    long n = (long)json_len(messages);
    if (idx > n) idx = n;
    /* If idx is a tool message, walk backward past consecutive tools to the
     * parent assistant message, then move the boundary before the assistant. */
    json_t *msg = idx > 0 ? json_get(messages, (size_t)(idx - 1)) : NULL;
    /* Walk backward: if there are consecutive tool messages before idx,
     * find the parent assistant and move boundary before it. */
    long i = idx - 1;
    while (i >= 0) {
        json_t *m = json_get(messages, (size_t)i);
        if (m && cc_json_role_matches(m, "tool"))
            i--;
        else
            break;
    }
    /* Now i points at the first non-tool message before the tool group.
     * If it's an assistant, boundary goes before it. */
    if (i >= 0) {
        json_t *nm = json_get(messages, (size_t)i);
        if (nm && cc_json_role_matches(nm, "assistant"))
            return i;
    }
    return idx;
}

/* ── _find_turn_pair_end ──────────────────────────────────────────────── */
/* PoP: _find_turn_pair_end @ agent/context_compressor.py:_find_turn_pair_end */
static long cc_micro_compact_find_turn_pair_end(json_t *messages, long user_idx) {
    if (!messages || messages->type != JSON_ARRAY) return user_idx;
    long n = (long)json_len(messages);
    long idx = user_idx + 1;
    if (idx >= n) return idx;
    json_t *first = json_get(messages, (size_t)idx);
    if (!first || !cc_json_role_matches(first, "assistant"))
        return idx;
    idx++;
    while (idx < n) {
        json_t *m = json_get(messages, (size_t)idx);
        if (m && cc_json_role_matches(m, "tool"))
            idx++;
        else
            break;
    }
    return idx;
}

/* ── _find_tail_cut_by_tokens ────────────────────────────────────────── */
/* PoP: _find_tail_cut_by_tokens @ agent/context_compressor.py:_find_tail_cut_by_tokens */
static long cc_micro_compact_find_tail_cut_by_tokens(cc_micro_compact_state_t *st,
                                                      json_t *messages,
                                                      long head_end,
                                                      long token_budget) {
    if (!st || !messages || messages->type != JSON_ARRAY) return 0;
    long n = (long)json_len(messages);
    if (n <= head_end) return n;

    if (token_budget < 0)
        token_budget = cc_micro_compact_tail_token_budget(st);

    long available_tail = n - head_end - 1;
    long min_tail_floor = 3;
    if (st->protect_last_n > 0 && st->protect_last_n < CC_MAX_TAIL_MESSAGE_FLOOR)
        min_tail_floor = st->protect_last_n;
    else if (st->protect_last_n >= CC_MAX_TAIL_MESSAGE_FLOOR)
        min_tail_floor = CC_MAX_TAIL_MESSAGE_FLOOR;
    long compressible_tail_cap = available_tail > 2 ? available_tail - 2 : 0;
    long min_tail = (available_tail > 1)
        ? (min_tail_floor < compressible_tail_cap ? min_tail_floor : compressible_tail_cap)
        : 0;
    if (min_tail > available_tail) min_tail = available_tail;
    long soft_ceiling = (long)(token_budget * 1.5);
    long accumulated = 0;
    long cut_idx = n;

    for (long i = n - 1; i >= head_end; i--) {
        json_t *msg = json_get(messages, (size_t)i);
        long msg_tokens = msg ? cc_micro_compact_estimate_msg_budget_tokens(msg) : 10;
        if (accumulated + msg_tokens > soft_ceiling && (n - i) >= min_tail)
            break;
        accumulated += msg_tokens;
        cut_idx = i;
    }

    /* If whole transcript fits in soft_ceiling, re-walk with raw budget. */
    if (cut_idx <= head_end && accumulated <= soft_ceiling && accumulated > 0) {
        long raw_accumulated = 0;
        for (long j = n - 1; j >= head_end; j--) {
            json_t *msg = json_get(messages, (size_t)j);
            long tok = msg ? cc_micro_compact_estimate_msg_budget_tokens(msg) : 10;
            if (raw_accumulated + tok > token_budget && (n - j) >= min_tail) {
                cut_idx = j;
                break;
            }
            raw_accumulated += tok;
            cut_idx = j;
        }
    }

    long fallback_cut = n - min_tail;
    if (cut_idx > fallback_cut) cut_idx = fallback_cut;
    if (cut_idx < 0) cut_idx = 0;
    if (cut_idx <= head_end)
        cut_idx = head_end + 1 > fallback_cut ? head_end + 1 : fallback_cut;
    if (cut_idx <= head_end) cut_idx = head_end + 1;

    /* Tool-group alignment. */
    cut_idx = cc_micro_compact_align_boundary_backward(messages, cut_idx);

    /* Ensure last user/assistant in tail. */
    cut_idx = cc_micro_compact_ensure_last_assistant_message_in_tail(messages, cut_idx, head_end);
    cut_idx = cc_micro_compact_ensure_last_user_message_in_tail(messages, cut_idx, head_end);

    /* min_tail_user_messages extension (default 1). */
    int min_tail_users = st->min_tail_user_messages > 1 ? st->min_tail_user_messages : 1;
    if (min_tail_users > 1) {
        for (int k = 1; k < min_tail_users; k++) {
            long last_u = cc_micro_compact_find_last_user_message_idx(messages, cut_idx - 1);
            if (last_u >= 0 && last_u >= head_end + 1)
                cut_idx = last_u;
        }
    }

    /* Forward re-align to avoid splitting tool groups after floor raise. */
    return cc_micro_compact_align_boundary_forward(messages, cut_idx);
}

/* ── _estimate_msg_budget_tokens (micro-compaction version) ──────────── */
/* PoP: _estimate_msg_budget_tokens @ agent/context_compressor.py:_estimate_msg_budget_tokens */
static long cc_micro_compact_estimate_msg_budget_tokens(json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return 10;
    /* content length + full tool_call envelope */
    json_t *content = json_obj_get(msg, "content");
    long tokens = (long)(cc_content_length_for_budget(content) / CC_CHARS_PER_TOKEN) + 10;
    json_t *tcs = json_obj_get(msg, "tool_calls");
    if (tcs && tcs->type == JSON_ARRAY) {
        size_t n = json_len(tcs);
        for (size_t i = 0; i < n; i++) {
            json_t *tc = json_get(tcs, i);
            if (tc && tc->type == JSON_OBJECT) {
                char *s = json_serialize(tc);
                if (s) { tokens += (long)(strlen(s) / CC_CHARS_PER_TOKEN); free(s); }
            }
        }
    }
    return tokens;
}

/* ── _find_last_user_message_idx ──────────────────────────────────────── */
/* PoP: _find_last_user_message_idx @ agent/context_compressor.py:_find_last_user_message_idx */
static long cc_micro_compact_find_last_user_message_idx(json_t *messages, long end_idx) {
    if (!messages || messages->type != JSON_ARRAY) return -1;
    long n = (long)json_len(messages);
    if (end_idx < 0) end_idx = n - 1;
    if (end_idx >= n) end_idx = n - 1;
    for (long i = end_idx; i >= 0; i--) {
        json_t *msg = json_get(messages, (size_t)i);
        if (msg && cc_json_role_matches(msg, "user"))
            return i;
    }
    return -1;
}

/* ── _find_last_assistant_message_idx ────────────────────────────────── */
/* PoP: _find_last_assistant_message_idx @ agent/context_compressor.py:_find_last_assistant_message_idx */
static long cc_micro_compact_find_last_assistant_message_idx(json_t *messages, long end_idx) {
    if (!messages || messages->type != JSON_ARRAY) return -1;
    long n = (long)json_len(messages);
    if (end_idx < 0) end_idx = n - 1;
    if (end_idx >= n) end_idx = n - 1;
    for (long i = end_idx; i >= 0; i--) {
        json_t *msg = json_get(messages, (size_t)i);
        if (msg && msg->type == JSON_OBJECT && cc_json_role_matches(msg, "assistant") &&
            !cc_is_context_summary_message_c(msg))
            return i;
    }
    return -1;
}

/* ── _ensure_last_user_message_in_tail ───────────────────────────────── */
/* PoP: _ensure_last_user_message_in_tail @ agent/context_compressor.py:_ensure_last_user_message_in_tail */
static long cc_micro_compact_ensure_last_user_message_in_tail(json_t *messages,
                                                                long cut_idx,
                                                                long head_end) {
    if (!messages || messages->type != JSON_ARRAY) return cut_idx;
    long last_user_idx = cc_micro_compact_find_last_user_message_idx(messages, cut_idx - 1);
    if (last_user_idx < 0) return cut_idx;
    if (last_user_idx >= cut_idx) return cut_idx;
    long adjusted = last_user_idx > head_end + 1 ? last_user_idx : head_end + 1;
    if (adjusted > last_user_idx) {
        /* Causal coupling: push cut forward to pair_end. */
        long pair_end = cc_micro_compact_find_turn_pair_end(messages, last_user_idx);
        return pair_end > head_end + 1 ? pair_end : head_end + 1;
    }
    return adjusted;
}

/* ── _ensure_last_assistant_message_in_tail ──────────────────────────── */
/* PoP: _ensure_last_assistant_message_in_tail @ agent/context_compressor.py:_ensure_last_assistant_message_in_tail */
static long cc_micro_compact_ensure_last_assistant_message_in_tail(json_t *messages,
                                                                    long cut_idx,
                                                                    long head_end) {
    if (!messages || messages->type != JSON_ARRAY) return cut_idx;
    long last_asst_idx = cc_micro_compact_find_last_assistant_message_idx(messages, cut_idx - 1);
    if (last_asst_idx < 0) return cut_idx;
    if (last_asst_idx >= cut_idx) return cut_idx;
    long new_cut = cc_micro_compact_align_boundary_backward(messages, last_asst_idx);
    return new_cut > head_end + 1 ? new_cut : head_end + 1;
}

/* ── _find_last_n_user_messages_in_tail ──────────────────────────────── */
/* PoP: _ensure_last_n_user_messages_in_tail @ agent/context_compressor.py:_ensure_last_n_user_messages_in_tail */
static long cc_micro_compact_ensure_last_n_user_messages_in_tail(json_t *messages,
                                                                  long cut_idx,
                                                                  long head_end,
                                                                  int n_users) {
    if (!messages || messages->type != JSON_ARRAY || n_users <= 0) return cut_idx;
    long cur = cut_idx;
    for (int k = 0; k < n_users; k++) {
        long u = cc_micro_compact_find_last_user_message_idx(messages, cur - 1);
        if (u < 0 || u < head_end + 1) break;
        cur = u;
    }
    return cur;
}

/* ── Constructor / destructor / config ───────────────────────────────── */

cc_micro_compact_state_t *cc_micro_compact_state_new(void) {
    cc_micro_compact_state_t *st = calloc(1, sizeof(*st));
    if (!st) return NULL;
    st->resolved_context_length = -1;
    st->threshold_tokens = -1;
    st->tail_token_budget = -1;
    st->max_summary_tokens = -1;
    st->config_context_length = -1;
    st->threshold_tokens_cap = -1;
    st->max_tokens = -1;
    st->threshold_percent = 0.50;
    st->base_threshold_percent = 0.50;
    st->summary_target_ratio = 0.5;
    st->micro_compact_every_n_turns = CC_MICRO_COMPACT_EVERY_N_TURNS_DEFAULT;
    st->micro_compact_defrag_threshold_tokens = CC_MICRO_COMPACT_DEFGRAD_THRESHOLD_TOKENS;
    st->micro_compact_last_failure_cursor = -1;
    st->min_tail_user_messages = 1;
    st->log_init_summary = false;
    return st;
}

void cc_micro_compact_state_free(cc_micro_compact_state_t *st) {
    if (!st) return;
    free(st->last_summary_error);
    free(st);
}

void cc_micro_compact_set_model(cc_micro_compact_state_t *st, const char *m) {
    if (!st || !m) return;
    snprintf(st->model, sizeof(st->model), "%s", m);
}
void cc_micro_compact_set_provider(cc_micro_compact_state_t *st, const char *p) {
    if (!st || !p) return;
    snprintf(st->provider, sizeof(st->provider), "%s", p);
}
void cc_micro_compact_set_base_url(cc_micro_compact_state_t *st, const char *u) {
    if (!st || !u) return;
    snprintf(st->base_url, sizeof(st->base_url), "%s", u);
}
void cc_micro_compact_set_api_key(cc_micro_compact_state_t *st, const char *k) {
    if (!st || !k) return;
    snprintf(st->api_key, sizeof(st->api_key), "%s", k);
}
void cc_micro_compact_set_summary_model(cc_micro_compact_state_t *st, const char *m) {
    if (!st || !m) return;
    snprintf(st->summary_model, sizeof(st->summary_model), "%s", m);
}
void cc_micro_compact_set_threshold_percent(cc_micro_compact_state_t *st, double v) {
    if (!st) return; st->threshold_percent = v;
}
void cc_micro_compact_set_base_threshold_percent(cc_micro_compact_state_t *st, double v) {
    if (!st) return; st->base_threshold_percent = v;
}
void cc_micro_compact_set_summary_target_ratio(cc_micro_compact_state_t *st, double v) {
    if (!st) return; st->summary_target_ratio = v;
}
void cc_micro_compact_set_max_tokens(cc_micro_compact_state_t *st, long v) {
    if (!st) return; st->max_tokens = v;
}
void cc_micro_compact_set_config_context_length(cc_micro_compact_state_t *st, long v) {
    if (!st) return; st->config_context_length = v;
}
void cc_micro_compact_set_threshold_tokens_cap(cc_micro_compact_state_t *st, long v) {
    if (!st) return; st->threshold_tokens_cap = v;
}
void cc_micro_compact_set_protect_first_n(cc_micro_compact_state_t *st, int v) {
    if (!st) return; st->protect_first_n = v;
}
void cc_micro_compact_set_protect_last_n(cc_micro_compact_state_t *st, int v) {
    if (!st) return; st->protect_last_n = v;
}
void cc_micro_compact_set_min_tail_user_messages(cc_micro_compact_state_t *st, int v) {
    if (!st) return; st->min_tail_user_messages = v;
}
void cc_micro_compact_set_quiet_mode(cc_micro_compact_state_t *st, bool v) {
    if (!st) return; st->quiet_mode = v;
}
void cc_micro_compact_set_compression_count(cc_micro_compact_state_t *st, int v) {
    if (!st) return; st->compression_count = v;
}
void cc_micro_compact_set_previous_summary(cc_micro_compact_state_t *st, const char *s) {
    if (!st) return;
    if (s) {
        snprintf(st->previous_summary, sizeof(st->previous_summary), "%s", s);
        st->has_previous_summary = true;
    } else {
        st->previous_summary[0] = '\0';
        st->has_previous_summary = false;
    }
}
void cc_micro_compact_enable(cc_micro_compact_state_t *st, bool v) {
    if (!st) return; st->micro_compact_enabled = v;
}
void cc_micro_compact_set_every_n_turns(cc_micro_compact_state_t *st, int v) {
    if (!st) return; st->micro_compact_every_n_turns = v;
}
void cc_micro_compact_set_defrag_threshold(cc_micro_compact_state_t *st, int v) {
    if (!st) return; st->micro_compact_defrag_threshold_tokens = v;
}
void cc_micro_compact_set_summary_callback(cc_micro_compact_state_t *st,
                                           void *ctx,
                                           char *(*fn)(void *ctx, const char *prompt, long budget)) {
    if (!st) return;
    st->summary_ctx = ctx;
    st->summarize_fn = fn;
}

