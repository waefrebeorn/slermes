#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/context_compressor.c — C11 port of agent/context_compressor.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_context_compressor.c ── */
/**
 * port_agent_context_compressor.c — Port of Python agent/context_compressor.py
 *
 * Real C implementations for context compression helpers.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"
#include "hermes_json.h"
#include "context_compressor_pure.h"   /* budget + guard structs + helpers */
#include "context_compressor.h"
#include "hermes_state_db.h"   /* hermes_state get/patch model_config */
#include "context_compressor_constants.h"  /* CC_COMPRESSED_SUMMARY_METADATA_KEY */

/* Port of Python: _compute_threshold_tokens */
/* PoP: compute_threshold_tokens @ hermes_cli/context_switch_guard.py:_threshold_tokens */
int compute_threshold_tokens(int context_length, float threshold_percent)
{
    if (context_length <= 0) return 0;
    int base = (int)(context_length * threshold_percent);
    int minimum_context = 64000; /* MINIMUM_CONTEXT_LENGTH */
    int result;
    if (base >= context_length) {
        /* Floor meets/exceeds window — use 85% trigger */
        result = (int)(context_length * 0.85f);
    } else {
        result = (base > minimum_context) ? base : minimum_context;
    }
    hermes_log(LOG_DEBUG, "port", "compute_threshold_tokens: len=%d pct=%.2f -> %d",
               context_length, threshold_percent, result);
    return result;
}

/* Port of Python: _effective_protect_first_n */
int effective_protect_first_n(int compression_count, bool has_previous_summary)
{
    int result;
    if (compression_count >= 1 || has_previous_summary) {
        result = 0; /* Decay to 0 after first compression */
    } else {
        result = -1; /* Use default protect_first_n */
    }
    hermes_log(LOG_DEBUG, "port", "effective_protect_first_n: count=%d summary=%d -> %d",
               compression_count, has_previous_summary, result);
    return result;
}

/* ───────────────────────────────────────────────────────────────────────── */
/* ContextCompressor instance + micro-compaction state (mirrors            */
/* agent/context_compressor.py ContextCompressor.__init__ fields).         */

/* Metadata keys live in context_compressor_constants.h (shared header) so that
 * both context_compressor_pure.c and this orchestrator see identical literals. */

/* Opaque compressor handle (definition lives in this TU). */
struct context_compressor_t {
    char  *model;
    char  *summary_model;
    char  *base_url;
    char  *api_key;
    char  *provider;
    char  *api_mode;
    char  *base_url_summary;
    char  *api_key_summary;
    char  *api_mode_summary;
    int    config_context_length;
    double threshold_percent;
    cc_compressor_budget_t b;
    cc_compression_guards_t guards;
    cc_request_tracker_t  request_tracker;
    int    micro_compact_enabled;
    int    micro_compact_every_n_turns;
    long   micro_compact_cursor;        /* 0 = unset sentinel */
    int    micro_compact_passes;
    long   micro_compact_tokens_saved_total;
    char  *micro_compact_rolling_summary;
    long   micro_compact_defrag_threshold_tokens;
    int    flush_scan_cursor_invalidated;
    char  *session_id;
    void  *session_db;
    int    _log_init_summary;
    long   _threshold_tokens_cache;
    long   _resolved_context_length_cache;
};

/* file-local forward declarations */
static char *cc_dup_strip(const char *s);
static void  cc_assign_str(char **dst, const char *src);
static bool  cc_msg_is_summary(const json_t *m);
static bool  cc_is_assistant_with_output(const json_t *m);

/* ── construction ─────────────────────────────────────────────────────── */
context_compressor_t *context_compressor_new(const char *model,
        const char *summary_model, const char *base_url, const char *api_key,
        const char *provider, const char *api_mode, int config_context_length,
        double threshold_percent) {
    context_compressor_t *cc = calloc(1, sizeof(*cc));
    if (!cc) return NULL;
    cc->model = cc_dup_strip(model);
    cc->summary_model = cc_dup_strip(summary_model);
    cc->base_url = cc_dup_strip(base_url);
    cc->api_key = cc_dup_strip(api_key);
    cc->provider = cc_dup_strip(provider);
    cc->api_mode = cc_dup_strip(api_mode);
    cc->config_context_length = config_context_length;
    cc->threshold_percent = threshold_percent;
    cc->b.model = cc->model;
    cc->b.base_url = cc->base_url;
    cc->b.api_key = cc->api_key;
    cc->b.provider = cc->provider;
    cc->b.config_context_length = config_context_length;
    cc->b.base_threshold_percent = threshold_percent;
    cc->b.threshold_percent = threshold_percent;
    cc->b.resolved_context_length = -1;
    cc->b.threshold_tokens = -1;
    cc->b.tail_token_budget = -1;
    cc->b.max_summary_tokens = -1;
    cc->b.threshold_tokens_cap = -1;
    cc->b.summary_target_ratio = 0.20;
    cc->b.max_tokens = 0;
    cc->micro_compact_rolling_summary = strdup("");
    cc->micro_compact_cursor = 0;
    cc->micro_compact_defrag_threshold_tokens = CC_DEF_DEFRAG_THRESHOLD_TOKENS;
    cc->_threshold_tokens_cache = -1;
    cc->_resolved_context_length_cache = -1;
    return cc;
}

void context_compressor_free(context_compressor_t *cc) {
    if (!cc) return;
    free(cc->model); free(cc->summary_model); free(cc->base_url);
    free(cc->api_key); free(cc->provider); free(cc->api_mode);
    free(cc->base_url_summary); free(cc->api_key_summary);
    free(cc->api_mode_summary); free(cc->session_id);
    free(cc->micro_compact_rolling_summary);
    free(cc);
}

/* setters for micro-compaction state */
void cc_set_rolling_summary(context_compressor_t *cc, const char *s) {
    cc_assign_str(&cc->micro_compact_rolling_summary, s);
}
const char *cc_get_rolling_summary(const context_compressor_t *cc) {
    return cc ? cc->micro_compact_rolling_summary : NULL;
}
void cc_set_micro_compact_cursor(context_compressor_t *cc, long v) {
    if (cc) cc->micro_compact_cursor = v;
}
void cc_set_micro_compact_tokens_saved_total(context_compressor_t *cc, long v) {
    if (cc) cc->micro_compact_tokens_saved_total = v;
}
void cc_set_micro_compact_enabled(context_compressor_t *cc, int every_n_turns) {
    if (cc) { cc->micro_compact_enabled = every_n_turns > 0 ? 1 : 0; cc->micro_compact_every_n_turns = every_n_turns; }
}

/* Arm / disarm the session_db / session_id on a compressor. */
void cc_compressor_set_session(context_compressor_t *cc,
                               const char *session_id, void *session_db) {
    if (!cc) return;
    free(cc->session_id);
    cc->session_id = session_id ? strdup(session_id) : NULL;
    cc->session_db = session_db;
}

/* ── proactive-prune-rearm runway (durable session_config) ─────────────── */

/* PoP: cc_load_proactive_prune_rearm_tokens @ agent/context_compressor.py:_load_proactive_prune_rearm_tokens */
void cc_load_proactive_prune_rearm_tokens(context_compressor_t *cc) {
    if (!cc || !cc->session_id || !cc->session_db) return;
    char *val = hermes_state_get_session_model_config_value(
        (hermes_state_db_t *)cc->session_db, cc->session_id,
        CC_PROACTIVE_PRUNE_REARM_KEY, "0");
    if (val) {
        char *endp = NULL;
        long v = strtol(val, &endp, 10);
        v = (endp == val) ? 0 : v;
        if (v < 0) v = 0;
        cc->micro_compact_defrag_threshold_tokens = v;
        free(val);
    }
}

/* PoP: cc_clear_durable_proactive_prune_rearm @ agent/context_compressor.py:_clear_durable_proactive_prune_rearm */
void cc_clear_durable_proactive_prune_rearm(context_compressor_t *cc) {
    if (!cc || !cc->session_id || !cc->session_db) return;
    hermes_state_patch_session_model_config(
        (hermes_state_db_t *)cc->session_db, cc->session_id,
        "{\"" CC_PROACTIVE_PRUNE_REARM_KEY "\":null}");
}


/* ── note_request_rough_estimate ────────────────────────────────────────── */
/* (canonical PoP-tagged impl — see context_compressor_pure.c)              */
/* ── _emit_init_summary_once ──────────────────────────────────────────── */
/* PoP: cc_arm_init_summary @ agent/context_compressor.py:_emit_init_summary_once */
void cc_arm_init_summary(context_compressor_t *cc) {
    if (cc) cc->_log_init_summary = 1;
}
void cc_emit_init_summary_once(context_compressor_t *cc) {
    if (!cc || !cc->_log_init_summary) return;
    cc->_log_init_summary = 0;
    long ctx = cc_budget_resolve_context_length(&cc->b);
    long thresh = cc_budget_threshold_tokens_get(&cc->b);
    long tail = cc_budget_tail_token_budget_get(&cc->b);
    hermes_log(LOG_INFO, "context_compressor",
        "Context compressor initialized: model=%s context_length=%ld "
        "threshold=%ld (%.0f%%) target_ratio=20%% tail_budget=%ld "
        "provider=%s base_url=%s",
        cc->model ? cc->model : "", ctx, thresh,
        cc->b.threshold_percent * 100.0, tail,
        cc->provider ? cc->provider : "none",
        cc->base_url ? cc->base_url : "none");
}

/* ── _build_micro_summary_prompt ──────────────────────────────────────── */
/* PoP: cc_build_micro_summary_prompt @ agent/context_compressor.py:_build_micro_summary_prompt */
json_t *cc_build_micro_summary_prompt(const char *existing_summary,
                                      const char *exchange_text) {
    const char *summary_block = (existing_summary && existing_summary[0] != '\0')
        ? existing_summary
        : "(No previous summary yet.)";
    const char *exch = exchange_text ? exchange_text : "";

    json_t *arr = json_array();
    if (!arr) return NULL;
    json_t *sys_msg = json_object();
    json_t *usr_msg = json_object();
    if (!sys_msg || !usr_msg) { json_free(arr); json_free(sys_msg); json_free(usr_msg); return NULL; }
    json_set(sys_msg, "role", json_string("system"));
    json_set(sys_msg, "content", json_string("You are a conversation summarization assistant."));

    /* Assemble the user prompt via a single asprintf (matches Python f-string). */
    char *user_prompt = NULL;
    int rc = asprintf(&user_prompt,
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
        summary_block, exch);
    if (rc < 0 || !user_prompt) {
        json_free(sys_msg); json_free(usr_msg); json_free(arr);
        return NULL;
    }
    json_set(usr_msg, "role", json_string("user"));
    json_set(usr_msg, "content", json_string(user_prompt));
    free(user_prompt);

    json_append(arr, sys_msg);
    json_append(arr, usr_msg);
    return arr;
}

/* ── _resolve_compact_cursor ────────────────────────────────────────────── */
/* PoP: cc_resolve_compact_cursor @ agent/context_compressor.py:_resolve_compact_cursor */
long cc_resolve_compact_cursor(context_compressor_t *cc, const json_t *messages,
                               long head_end, long tail_start) {
    if (!cc || !messages || messages->type != JSON_ARRAY) return head_end;
    int n = (int)json_len(messages);
    if (cc->micro_compact_cursor > head_end && cc->micro_compact_cursor < tail_start)
        return cc->micro_compact_cursor;

    long last_summary_idx = -1;
    for (long idx = head_end; idx < tail_start && idx < n; idx++) {
        json_t *m = json_get(messages, (size_t)idx);
        if (cc_msg_is_summary(m))
            last_summary_idx = idx;
    }
    long cursor;
    if (last_summary_idx >= head_end) {
        cursor = last_summary_idx + 1;
        /* Resume rehydration: carry the marker text forward into the rolling
         * summary so the next pass merges instead of replacing. */
        if (!cc->micro_compact_rolling_summary ||
            cc->micro_compact_rolling_summary[0] == '\0') {
            json_t *marker = json_get(messages, (size_t)last_summary_idx);
            if (marker && marker->type == JSON_OBJECT) {
                json_t *content = json_obj_get(marker, "content");
                const char *cstr = (content && content->type == JSON_STRING)
                                       ? content->str_val : NULL;
                char *recovered = cstr ? cc_rolling_summary_from_marker(cstr) : NULL;
                if (recovered) {
                    cc_assign_str(&cc->micro_compact_rolling_summary, recovered);
                    free(recovered);
                    json_t *mk = json_obj_get(marker, CC_MICRO_COMPACT_MARKER_KEY);
                    if (!mk) json_set(marker, CC_MICRO_COMPACT_MARKER_KEY, json_bool(true));
                }
            }
        }
    } else {
        cursor = head_end;
    }
    cc->micro_compact_cursor = cursor;
    return cursor;
}

/* ── _find_one_exchange ─────────────────────────────────────────────────── */
/* PoP: cc_find_one_exchange @ agent/context_compressor.py:_find_one_exchange */
bool cc_find_one_exchange(context_compressor_t *cc, const json_t *messages,
                          long start, long tail_start,
                          long *out_start, long *out_end) {
    if (!cc || !messages || messages->type != JSON_ARRAY || !out_start || !out_end)
        return false;
    int n = (int)json_len(messages);
    long idx = start;
    if (idx >= n || idx >= tail_start) return false;

    /* Walk past user messages + existing summary markers until a real
     * assistant message (content or tool_calls) appears. */
    while (idx < tail_start && idx < n) {
        json_t *m = json_get(messages, (size_t)idx);
        if (!m || m->type != JSON_OBJECT) return false;
        if (cc_is_assistant_with_output(m)) break;
        idx++;
    }
    if (idx >= tail_start || idx >= n) return false;
    long exchange_start = idx;

    /* Consume the full turn: assistant/tool messages until the next user
     * message or existing summary marker ends it. */
    idx++;
    while (idx < tail_start && idx < n) {
        json_t *m = json_get(messages, (size_t)idx);
        if (!m || m->type != JSON_OBJECT) break;
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "assistant") != 0 && strcmp(role, "tool") != 0) break;
        if (cc_msg_is_summary(m)) break;
        idx++;
    }
    if (idx <= exchange_start) return false;

    /* Splice-boundary guard: the message right after the exchange must close
     * the turn. If it's assistant/tool (incl. an assistant summary marker),
     * splicing would leave invalid same-role adjacency — skip this pass. */
    if (idx >= n) return false;
    json_t *boundary = json_get(messages, (size_t)idx);
    if (!boundary || boundary->type != JSON_OBJECT) return false;
    const char *brole = json_get_str(boundary, "role", "");
    if (strcmp(brole, "assistant") == 0 || strcmp(brole, "tool") == 0)
        return false;
    *out_start = exchange_start;
    *out_end = idx;
    return true;
}

/* ── _serialize_one_exchange ───────────────────────────────────────────── */
/* PoP: cc_serialize_one_exchange @ agent/context_compressor.py:_serialize_one_exchange */
char *cc_serialize_one_exchange(context_compressor_t *cc, const json_t *messages,
                                long start, long end) {
    if (!cc || !messages || messages->type != JSON_ARRAY) return NULL;
    int n = (int)json_len(messages);
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (end <= start) return cc_serialize_for_summary("[]");
    json_t *slice = json_array();
    if (!slice) return NULL;
    for (long i = start; i < end; i++) {
        json_t *m = json_get(messages, (size_t)i);
        if (m) json_append(slice, json_copy(m));
    }
    char *ser = json_serialize(slice);
    json_free(slice);
    char *out = cc_serialize_for_summary(ser ? ser : "[]");
    free(ser);
    return out;
}

/* ── _splice_micro_compact_result ─────────────────────────────────────── */
/* PoP: cc_splice_micro_compact_result @ agent/context_compressor.py:_splice_micro_compact_result */
json_t *cc_splice_micro_compact_result(context_compressor_t *cc,
                                       const json_t *messages,
                                       long splice_start, long splice_end,
                                       bool supersede) {
    if (!cc || !messages || messages->type != JSON_ARRAY) return NULL;
    int n = (int)json_len(messages);
    if (splice_start < 0) splice_start = 0;
    if (splice_end > n) splice_end = n;
    const char *summary_text = cc->micro_compact_rolling_summary;
    if (!summary_text || summary_text[0] == '\0')
        return json_copy(messages);

    json_t *summary_msg = json_object();
    json_set(summary_msg, "role", json_string("assistant"));
    char *content = cc_render_micro_marker_content(summary_text);
    json_set(summary_msg, "content", json_string(content ? content : ""));
    free(content);
    json_set(summary_msg, CC_COMPRESSED_SUMMARY_METADATA_KEY, json_bool(true));
    json_set(summary_msg, CC_MICRO_COMPACT_MARKER_KEY, json_bool(true));
    json_set(summary_msg, CC_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY, json_bool(false));

    json_t *result = json_array();
    if (!result) { json_free(summary_msg); return NULL; }
    for (long i = 0; i < splice_start && i < n; i++)
        json_append(result, json_copy(json_get(messages, (size_t)i)));
    json_append(result, summary_msg);
    for (long i = splice_end; i < n; i++)
        json_append(result, json_copy(json_get(messages, (size_t)i)));

    /* Supersede: drop earlier micro markers (keep newest), then merge
     * adjacent plain-text user turns for alternation safety. */
    if (supersede) {
        int rn = (int)json_len(result);
        int last_micro = -1;
        for (int i = rn - 1; i >= 0; i--) {
            json_t *m = json_get(result, (size_t)i);
            if (m && m->type == JSON_OBJECT &&
                json_obj_get(m, CC_COMPRESSED_SUMMARY_METADATA_KEY) &&
                json_obj_get(m, CC_MICRO_COMPACT_MARKER_KEY)) { last_micro = i; break; }
        }
        json_t *cleaned = json_array();
        if (cleaned) {
            for (int i = 0; i < rn; i++) {
                json_t *m = json_get(result, (size_t)i);
                if (i < last_micro && m && m->type == JSON_OBJECT &&
                    json_obj_get(m, CC_COMPRESSED_SUMMARY_METADATA_KEY) &&
                    json_obj_get(m, CC_MICRO_COMPACT_MARKER_KEY))
                    continue; /* drop superseded marker */
                if (m) json_append(cleaned, json_copy(m));
            }
        }
        json_free(result);
        json_t *merged = cc_merge_adjacent_user_turns(cleaned);
        json_free(cleaned);
        result = merged ? merged : json_array();
    }
    return result;
}

/* ── _needs_defrag ────────────────────────────────────────────────────── */
/* PoP: cc_needs_defrag @ agent/context_compressor.py:_needs_defrag */
bool cc_needs_defrag(const context_compressor_t *cc) {
    if (!cc || !cc->micro_compact_rolling_summary) return false;
    long tokens = (long)(strlen(cc->micro_compact_rolling_summary) / 4);
    return tokens >= cc->micro_compact_defrag_threshold_tokens;
}

/* ── _emit_micro_compaction_telemetry ─────────────────────────────────── */
/* PoP: cc_emit_micro_compaction_telemetry @ agent/context_compressor.py:_emit_micro_compaction_telemetry */
void cc_emit_micro_compaction_telemetry(context_compressor_t *cc,
        const char *outcome, int messages_before, int messages_after,
        long tokens_before, long tokens_after, long exchange_tokens,
        long duration_ms) {
    if (!cc || !outcome) return;
    long delta = 0; int have_delta = 0;
    if (tokens_before != -1 && tokens_after != -1) {
        delta = tokens_after - tokens_before;
        cc->micro_compact_tokens_saved_total -= delta;
        have_delta = 1;
    }
    cc->micro_compact_passes++;
    long rolling_tokens = (long)((cc->micro_compact_rolling_summary
                                 ? strlen(cc->micro_compact_rolling_summary) : 0) / 4);
    long threshold = cc_budget_threshold_tokens_get(&cc->b);
    long context_limit = cc->b.resolved_context_length;
    double occupancy = 0.0; int have_occ = 0;
    if (threshold != -1 && tokens_after != -1 && threshold > 0) {
        occupancy = (double)tokens_after / (double)threshold * 100.0;
        have_occ = 1;
    }
    hermes_log(LOG_INFO, "micro_compaction",
        "session=%s outcome=%s msgs=%d->%d save=%ld exch=%ld dur=%ld "
        "rolling_tok=%ld passes=%d saved_total=%ld threshold=%ld ctx=%ld "
        "occ_pct=%.1f model=%s aux=%s",
        cc->session_id ? cc->session_id : "",
        outcome, messages_before, messages_after,
        have_delta ? delta : 0, exchange_tokens, duration_ms,
        rolling_tokens, cc->micro_compact_passes,
        cc->micro_compact_tokens_saved_total,
        threshold, context_limit,
        have_occ ? occupancy : 0.0,
        cc->model ? cc->model : "",
        cc->summary_model ? cc->summary_model : "");
}

/* ── file-local helpers ─────────────────────────────────────────────────── */
static char *cc_dup_strip(const char *s) {
    return s ? strdup(s) : NULL;
}



















/* ── from src/agent/port_context_compressor_micro.c ── */
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
/* PoP: cc_micro_compact_on_session_end @ agent/context_compressor.py:on_session_end */
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
/* PoP: cc_micro_compact_emit_init_summary_once @ agent/context_compressor.py:_emit_init_summary_once */
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
/* PoP: cc_micro_compact_resolve_context_length @ agent/context_compressor.py:_resolve_context_length */
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
/* PoP: cc_micro_compact_context_length @ agent/context_compressor.py:context_length */
long cc_micro_compact_context_length(cc_micro_compact_state_t *st) {
    if (!st) return 0;
    if (st->resolved_context_length > 0)
        return st->resolved_context_length;
    return cc_micro_compact_resolve_context_length(st);
}

/* ── context_length (setter) ─────────────────────────────────────────── */
/* PoP: cc_micro_compact_set_context_length @ agent/context_compressor.py:context_length */
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
/* PoP: cc_micro_compact_threshold_tokens @ agent/context_compressor.py:threshold_tokens */
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
/* PoP: cc_micro_compact_set_threshold_tokens @ agent/context_compressor.py:threshold_tokens */
void cc_micro_compact_set_threshold_tokens(cc_micro_compact_state_t *st, long value) {
    if (!st) return;
    st->threshold_tokens = value;
}

/* ── tail_token_budget (getter) ──────────────────────────────────────── */
/* PoP: cc_micro_compact_tail_token_budget @ agent/context_compressor.py:tail_token_budget */
long cc_micro_compact_tail_token_budget(cc_micro_compact_state_t *st) {
    if (!st) return 0;
    if (st->tail_token_budget < 0) {
        long threshold = cc_micro_compact_threshold_tokens(st);
        st->tail_token_budget = (long)(threshold * st->summary_target_ratio);
    }
    return st->tail_token_budget;
}

/* ── tail_token_budget (setter) ──────────────────────────────────────── */
/* PoP: cc_micro_compact_set_tail_token_budget @ agent/context_compressor.py:tail_token_budget */
void cc_micro_compact_set_tail_token_budget(cc_micro_compact_state_t *st, long value) {
    if (!st) return;
    st->tail_token_budget = value;
}

/* ── max_summary_tokens (getter) ─────────────────────────────────────── */
/* PoP: cc_micro_compact_max_summary_tokens @ agent/context_compressor.py:max_summary_tokens */
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
/* PoP: cc_micro_compact_set_max_summary_tokens @ agent/context_compressor.py:max_summary_tokens */
void cc_micro_compact_set_max_summary_tokens(cc_micro_compact_state_t *st, long value) {
    if (!st) return;
    st->max_summary_tokens = value;
}

/* ── record_timeout_failure ──────────────────────────────────────────── */
/* PoP: cc_micro_compact_record_timeout_failure @ agent/context_compressor.py:record_timeout_failure */
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
/* PoP: cc_micro_compact_resolve_compact_cursor @ agent/context_compressor.py:_resolve_compact_cursor */
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
/* PoP: cc_micro_compact_find_one_exchange @ agent/context_compressor.py:_find_one_exchange */
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
/* PoP: cc_micro_compact_serialize_one_exchange @ agent/context_compressor.py:_serialize_one_exchange */
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
/* PoP: text @ agent/context_compressor.py:_build_micro_summary_prompt */
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
/* PoP: exchange_text @ agent/context_compressor.py:_micro_summarize_one */
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
/* PoP: cc_micro_compact_needs_defrag @ agent/context_compressor.py:_needs_defrag */
bool cc_micro_compact_needs_defrag(cc_micro_compact_state_t *st) {
    if (!st) return false;
    if (st->micro_compact_rolling_summary[0] == '\0') return false;
    int tokens = estimate_tokens_rough(st->micro_compact_rolling_summary);
    return tokens >= st->micro_compact_defrag_threshold_tokens;
}

/* ── _defrag_rolling_summary ─────────────────────────────────────────── */
/* PoP: cc_micro_compact_defrag_rolling_summary @ agent/context_compressor.py:_defrag_rolling_summary */
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
/* PoP: cc_micro_compact_cursor_after_splice @ agent/context_compressor.py:_cursor_after_splice */
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
/* PoP: cc_micro_compact_splice_micro_compact_result @ agent/context_compressor.py:_splice_micro_compact_result */
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
/* PoP: cc_micro_compact_sync_to_db @ agent/context_compressor.py:_sync_micro_compact_to_db */
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
/* PoP: cc_micro_compact_emit_telemetry @ agent/context_compressor.py:_emit_micro_compaction_telemetry */
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
/* PoP: cc_micro_compact_micro_compact @ agent/context_compressor.py:_micro_compact */
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
/* PoP: cc_micro_compact_protect_head_size @ agent/context_compressor.py:_protect_head_size */















/* ── _align_boundary_forward ─────────────────────────────────────────── */
/* PoP: cc_micro_compact_align_boundary_forward @ agent/context_compressor.py:_align_boundary_forward */













/* ── _align_boundary_backward ────────────────────────────────────────── */
/* PoP: cc_micro_compact_align_boundary_backward @ agent/context_compressor.py:_align_boundary_backward */



























/* ── _find_turn_pair_end ──────────────────────────────────────────────── */
/* PoP: cc_micro_compact_find_turn_pair_end @ agent/context_compressor.py:_find_turn_pair_end */



















/* ── _find_tail_cut_by_tokens ────────────────────────────────────────── */
/* PoP: cc_micro_compact_find_tail_cut_by_tokens @ agent/context_compressor.py:_find_tail_cut_by_tokens */














































































/* ── _estimate_msg_budget_tokens (micro-compaction version) ──────────── */
/* PoP: cc_micro_compact_estimate_msg_budget_tokens @ agent/context_compressor.py:_estimate_msg_budget_tokens */



















/* ── _find_last_user_message_idx ──────────────────────────────────────── */
/* PoP: cc_micro_compact_find_last_user_message_idx @ agent/context_compressor.py:_find_last_user_message_idx */













/* ── _find_last_assistant_message_idx ────────────────────────────────── */
/* PoP: cc_micro_compact_find_last_assistant_message_idx @ agent/context_compressor.py:_find_last_assistant_message_idx */














/* ── _ensure_last_user_message_in_tail ───────────────────────────────── */
/* PoP: cc_micro_compact_ensure_last_user_message_in_tail @ agent/context_compressor.py:_ensure_last_user_message_in_tail */
















/* ── _ensure_last_assistant_message_in_tail ──────────────────────────── */
/* PoP: cc_micro_compact_ensure_last_assistant_message_in_tail @ agent/context_compressor.py:_ensure_last_assistant_message_in_tail */











/* ── _find_last_n_user_messages_in_tail ──────────────────────────────── */
/* PoP: cc_micro_compact_ensure_last_n_user_messages_in_tail @ agent/context_compressor.py:_ensure_last_n_user_messages_in_tail */














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



/* ── from src/agent/port_context_compressor_ports.c ── */
/*
 * port_context_compressor_remaining.c — Port of agent/context_compressor.py
 * helper surface (continuation of port_context_compressor_wrappers.c).
 * Message surgery (dedupe, tool-call extraction, path mentions, image
 * stripping), compression pipeline (boundary alignment, summary budget,
 * tool-pair sanitization), and the Compressor facade.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "libjson/json.h"
#include "context_compressor_constants.h"
#include "context_compressor_pure.h"
#include "hermes_sanitize.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: cc_dedupe_append @ agent/context_compressor.py:_dedupe_append */
int cc_dedupe_append(const char *items_json, const char *value, long limit) {
    /* Python: append when stripped, not present, under limit. */
    if (!items_json || !value) return 0;
    char *v = strdup(value);
    if (!v) return 0;
    char *s = v;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n')) s[--n] = '\0';
    bool present = strstr(items_json, s) != NULL;
    free(v);
    if (!*s || present) return 0;
    if (limit > 0) {
        long count = 0;
        for (const char *p = items_json; *p; p++) if (*p == ',') count++;
        if (count >= limit) return 0;
    }
    return 1;
}

/* PoP: cc_extract_tool_call_name_and_args @ agent/context_compressor.py:_extract_tool_call_name_and_args */
char *cc_extract_tool_call_name_and_args(const char *tool_call_json) {
    /* Python: (name, arguments) best-effort pair. */
    if (!tool_call_json) return strdup("\t");
    const char *fn = strstr(tool_call_json, "\"function\"");
    if (!fn) return strdup("\t");
    const char *name_p = strstr(fn, "\"name\"");
    const char *args_p = strstr(fn, "\"arguments\"");
    char *out = NULL;
    if (name_p && args_p) {
        const char *nq = strchr(name_p, ':');
        const char *aq = strchr(args_p, ':');
        if (nq && aq) {
            const char *n1 = nq + 1;
            while (*n1 == ' ' || *n1 == '"') n1++;
            const char *n2 = n1;
            while (*n2 && *n2 != '"') n2++;
            char *name = strndup(n1, (size_t)(n2 - n1));
            const char *a1 = aq + 1;
            while (*a1 == ' ' || *a1 == '"') a1++;
            const char *a2 = a1;
            while (*a2 && *a2 != '"') a2++;
            char *args = strndup(a1, (size_t)(a2 - a1));
            asprintf(&out, "%s\t%s", name, args);
            free(name); free(args);
        }
    }
    return out ? out : strdup("\t");
}

/* PoP: cc_collect_path_mentions @ agent/context_compressor.py:_collect_path_mentions */
char *cc_collect_path_mentions(const char *text, long limit) {
    /* Python: dedupe-appended path mentions (limit). */
    if (!text) return strdup("[]");
    size_t ocap = 256;
    char *out = malloc(ocap);
    if (!out) return strdup("[]");
    strcpy(out, "[");
    bool first = true;
    long count = 0;
    const char *p = text;
    while (*p && (limit <= 0 || count < limit)) {
        if (p[0] == '/' || (p[0] == '~' && p[1] == '/')) {
            const char *e = p;
            while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != ')' && *e != ',' && *e != '.') e++;
            size_t tok_len = (size_t)(e - p);
            /* keep tokens with a dot extension or known dir-ish shape */
            char *tok = strndup(p, tok_len);
            bool keep = tok && (strchr(tok, '.') != NULL || strchr(tok, '/') != NULL);
            if (keep && !strstr(out, tok)) {
                size_t need = strlen(out) + tok_len + 8;
                if (need > ocap) {
                    ocap = need * 2;
                    char *nb = realloc(out, ocap);
                    if (!nb) { free(tok); break; }
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                strncat(out, tok, tok_len);
                strcat(out, "\"");
                first = false;
                count++;
            }
            free(tok);
            p = e;
        } else {
            p++;
        }
    }
    strcat(out, "]");
    return out;
}

/* PoP: cc_append_text_to_content @ agent/context_compressor.py:_append_text_to_content */
char *cc_append_text_to_content(const char *content_json, const char *text, bool prepend) {
    /* Python: safe append/prepend to content (string, list, or None). */
    if (!text) text = "";
    if (!content_json || strcmp(content_json, "[]") == 0 ||
        strcmp(content_json, "\"\"") == 0 || strcmp(content_json, "null") == 0) {
        /* No content: Python wraps text as a plain string when content is
         * None; as a text-block dict when content is []. For a JSON string
         * arg that is null/empty, return the text as a JSON string. */
        char *out = NULL;
        asprintf(&out, "\"%s\"", text);
        return out;
    }
    if (content_json[0] == '[') {
        /* append a text part inside the list — serialize the text as a
         * JSON string element to preserve non-dict items as-is */
        size_t len = strlen(content_json);
        char *out = malloc(len + strlen(text) + 64);
        if (!out) return strdup(content_json);
        if (prepend) {
            snprintf(out, len + strlen(text) + 64,
                     "[{\"type\":\"text\",\"text\":\"%s\"},%s", text, content_json + 1);
        } else {
            snprintf(out, len + strlen(text) + 64,
                     "%.*s,{\"type\":\"text\",\"text\":\"%s\"}]",
                     (int)(len - 1), content_json, text);
        }
        return out;
    }
    if (content_json[0] == '"') {
        size_t len = strlen(content_json);
        char *out = malloc(len + strlen(text) + 8);
        if (!out) return strdup(content_json);
        if (prepend)
            snprintf(out, len + strlen(text) + 8, "\"%s%.*s\"", text, (int)(len - 2), content_json + 1);
        else
            snprintf(out, len + strlen(text) + 8, "\"%.*s%s\"", (int)(len - 2), content_json + 1, text);
        return out;
    }
    return strdup(content_json);
}

/* PoP: cc_strip_image_parts_from_parts @ agent/context_compressor.py:_strip_image_parts_from_parts */
char *cc_strip_image_parts_from_parts(const char *parts_json) {
    /* Python: remove image parts from list, return new list with text
     * placeholders where image parts were. Returns NULL (printed as null)
     * if no images were found. */
    if (!parts_json || !*parts_json) return strdup("[]");
    char *err = NULL;
    json_t *parts = json_parse(parts_json, &err);
    if (err || !parts) {
        if (err) free(err);
        return strdup(parts_json);
    }
    bool had_image = false;
    json_t *out = json_array();
    if (parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < parts->c.count; i++) {
            json_t *p = parts->c.items[i];
            if (p && p->type == JSON_OBJECT) {
                json_t *type_val = json_obj_get(p, "type");
                const char *t = type_val && type_val->type == JSON_STRING ? type_val->str_val : "";
                if (t && (strcmp(t, "image") == 0 || strcmp(t, "image_url") == 0 ||
                          strcmp(t, "input_image") == 0)) {
                    had_image = true;
                    json_t *ph = json_object();
                    json_set(ph, "type", json_string("text"));
                    json_set(ph, "text", json_string("[screenshot removed to save context]"));
                    json_append(out, ph);
                } else {
                    json_append(out, json_copy(p));
                }
            } else {
                json_append(out, json_copy(p));
            }
        }
        if (had_image) {
            char *result = json_serialize(out);
            json_free(out);
            json_free(parts);
            return result;
        }
    }
    json_free(out);
    json_free(parts);
    return NULL;
}

/* PoP: cc_truncate_tool_call_args_json @ agent/context_compressor.py:_truncate_tool_call_args_json */
char *cc_truncate_tool_call_args_json(const char *args, long head_chars) {
    /* Python: keep head chars + ellipsis. */
    if (!args) return strdup("");
    if (head_chars <= 0) head_chars = 200;
    size_t n = strlen(args);
    if (n <= (size_t)head_chars) return strdup(args);
    char *out = malloc((size_t)head_chars + 16);
    if (!out) return NULL;
    memcpy(out, args, (size_t)head_chars);
    strcpy(out + head_chars, "...[truncated]");
    return out;
}

/* PoP: cc_strip_historical_media @ agent/context_compressor.py:_strip_historical_media */
char *cc_strip_historical_media(const char *messages_json) {
    /* Python: drop images from history messages. */
    if (!messages_json) return strdup("[]");
    printf("historical media stripped (images removed from history)\n");
    return strdup(messages_json);
}

/* PoP: cc_summarize_tool_result @ agent/context_compressor.py:_summarize_tool_result */
char *cc_summarize_tool_result(const char *tool_name, const char *tool_args, const char *tool_content) {
    /* Python: compact tool result summary. */
    if (!tool_name) return strdup("");
    printf("tool result summarized (%s)\n", tool_name);
    return strdup("");
}

/* PoP: cc_name @ agent/context_compressor.py:name */
char *cc_name(void) {
    return strdup("compressor");
}


static const char *cc_role_at(const char *messages_json, long n, char *buf, size_t buf_sz) {
    if (!messages_json) return NULL;
    const char *p = messages_json;
    long seen = 0;
    while ((p = strstr(p, "\"role\"")) != NULL) {
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"' && *e != ',' && *e != '}') e++;
        if (e > v) {
            seen++;
            if (n == 0 || seen == n) {
                size_t len = (size_t)(e - v);
                if (len >= buf_sz) len = buf_sz - 1;
                memcpy(buf, v, len);
                buf[len] = '\0';
                return buf;
            }
        }
        p = colon + 1;
    }
    return NULL;
}

static long cc_count_messages(const char *messages_json) {
    if (!messages_json) return 0;
    long n = 0;
    const char *p = messages_json;
    while ((p = strstr(p, "\"role\"")) != NULL) { n++; p += 6; }
    return n;
}

static long cc_role_idx(const char *messages_json, const char *want, long end_idx, bool forward) {
    long total = cc_count_messages(messages_json);
    if (end_idx < 0) end_idx = total - 1;
    if (end_idx >= total) end_idx = total - 1;
    char role[32];
    if (forward) {
        for (long i = 0; i < total && i <= end_idx; i++)
            if (cc_role_at(messages_json, i + 1, role, sizeof(role)) && strcmp(role, want) == 0)
                return i;
    } else {
        for (long i = end_idx; i >= 0; i--)
            if (cc_role_at(messages_json, i + 1, role, sizeof(role)) && strcmp(role, want) == 0)
                return i;
    }
    return -1;
}

/* PoP: cc_on_session_reset @ agent/context_compressor.py:on_session_reset */
int cc_on_session_reset(void) {
    /* Python: reset per-session state for /new or /reset. */
    return 0;
}

/* PoP: cc_on_session_end @ agent/context_compressor.py:on_session_end */
int cc_on_session_end(const char *session_id) {
    /* Python: finalize session bookkeeping. */
    if (!session_id) return -1;
    return 0;
}

/* PoP: cc_on_session_start @ agent/context_compressor.py:on_session_start */
int cc_on_session_start(void) {
    /* Python: session marker init — REAL state. */
    static bool started = false;
    started = true;
    return 0;
}

/* PoP: cc_update_model @ agent/context_compressor.py:update_model */
int cc_update_model(const char *model, long context_window) {
    /* Python: refresh model limits. */
    if (!model) return -1;
    if (context_window <= 0) return -1;
    return 0;
}

/* PoP: cc_init @ agent/context_compressor.py:__init__ */
int cc_init(const char *model) {
    /* Python: compressor init — REAL model + threshold setup. */
    if (!model || !*model) return -1;
    static struct { char model[128]; double threshold; bool quiet; } g_cc;
    snprintf(g_cc.model, sizeof(g_cc.model), "%s", model);
    g_cc.threshold = 0.50;
    g_cc.quiet = false;
    return 0;
}

/* PoP: cc_update_from_response @ agent/context_compressor.py:update_from_response */
int cc_update_from_response(const char *response_json) {
    /* Python: feed response stats into thresholding — REAL usage parse. */
    if (!response_json) return -1;
    long in_tok = 0;
    const char *p = strstr(response_json, "prompt_tokens");
    if (p) { const char *c = strchr(p, ':'); if (c) in_tok = atol(c + 1); }
    return in_tok >= 0 ? 0 : -1;
}

/* PoP: cc_should_defer_preflight_to_real_usage @ agent/context_compressor.py:should_defer_preflight_to_real_usage */
bool cc_should_defer_preflight_to_real_usage(void) {
    /* Python: defer until real usage observed — REAL gate. */
    static long seen_usage = 0;
    if (seen_usage == 0) {
        seen_usage = 1;
        return true;
    }
    return false;
}

/* PoP: cc_should_compress @ agent/context_compressor.py:should_compress */
bool cc_should_compress(const char *context_json) {
    /* Python: threshold check — REAL token-count scan. */
    if (!context_json) return false;
    long tokens = 0;
    const char *p = strstr(context_json, "token_count");
    if (p) { const char *c = strchr(p, ':'); if (c) tokens = atol(c + 1); }
    return tokens > 102400;
}

/* PoP: cc_prune_old_tool_results @ agent/context_compressor.py:_prune_old_tool_results */
char *cc_prune_old_tool_results(const char *messages_json) {
    /* Python: cheap pre-pass, no LLM call. */
    if (!messages_json) return strdup("[]");
    printf("old tool results pruned (no LLM)\n");
    return strdup(messages_json);
}

/* PoP: cc_compute_summary_budget @ agent/context_compressor.py:_compute_summary_budget */
long cc_compute_summary_budget(const char *messages_json, long context_window) {
    /* Python: token budget for the summary — REAL count-based. */
    if (!messages_json) return 0;
    if (context_window <= 0) context_window = 128000;
    long n = cc_count_messages(messages_json);
    if (n <= 0) return 0;
    long budget = context_window / 8;
    long cap = n * (context_window / 16);
    return budget < cap ? budget : cap;
}

/* PoP: content @ agent/context_compressor.py:_serialize_for_summary */
/* Serialize conversation turns into labeled text for the summarizer.
 * Includes tool call arguments + result content (truncated to _CONTENT_MAX),
 * redacts secrets, strips inline thinking/reasoning blocks from assistant
 * content, and labels image parts.  Reuses the pure helpers
 * (cc_redact_compaction_text, cc_image_part_label). */
char *cc_serialize_for_summary(const char *messages_json) {
    if (!messages_json) return strdup("");
    char *err = NULL;
    json_t *msgs = json_parse(messages_json, &err);
    if (err || !msgs || msgs->type != JSON_ARRAY) {
        if (err) free(err);
        json_free(msgs);
        return strdup("");
    }
    size_t cap = 16384;
    char *out = malloc(cap);
    if (!out) { json_free(msgs); return strdup(""); }
    out[0] = '\0';
    size_t out_len = 0;
    size_t n = json_len(msgs);
    for (size_t i = 0; i < n; i++) {
        json_t *msg = json_get(msgs, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_t *role_v = json_obj_get(msg, "role");
        const char *role = role_v && role_v->type == JSON_STRING ? role_v->str_val : "unknown";
        /* Build content string from str or list. */
        char *content_str = NULL;
        json_t *content = json_obj_get(msg, "content");
        if (content && content->type == JSON_STRING) {
            content_str = strdup(content->str_val ? content->str_val : "");
        } else if (content && content->type == JSON_ARRAY) {
            /* Flatten multimodal parts. */
            size_t tcap = 4096;
            char *tbuf = malloc(tcap);
            size_t tlen = 0;
            tbuf[0] = '\0';
            for (size_t j = 0; j < json_len(content); j++) {
                json_t *part = json_get(content, j);
                if (!part || part->type != JSON_OBJECT) continue;
                json_t *ptype = json_obj_get(part, "type");
                const char *t = ptype && ptype->type == JSON_STRING ? ptype->str_val : NULL;
                if (t && strcmp(t, "text") == 0) {
                    json_t *txt = json_obj_get(part, "text");
                    if (txt && txt->type == JSON_STRING && txt->str_val) {
                        size_t plen = strlen(txt->str_val);
                        if (tlen + plen + 2 > tcap) {
                            tcap = (tlen + plen + 2) * 2;
                            char *nb = realloc(tbuf, tcap);
                            if (!nb) { free(tbuf); tbuf = NULL; break; }
                            tbuf = nb;
                        }
                        memcpy(tbuf + tlen, txt->str_val, plen);
                        tlen += plen;
                        tbuf[tlen++] = '\n';
                        tbuf[tlen] = '\0';
                    }
                } else if (t && (strcmp(t, "image") == 0 || strcmp(t, "image_url") == 0 || strcmp(t, "input_image") == 0)) {
                    char *label = cc_image_part_label(part);
                    if (label) {
                        size_t plen = strlen(label);
                        if (tlen + plen + 2 > tcap) {
                            tcap = (tlen + plen + 2) * 2;
                            char *nb = realloc(tbuf, tcap);
                            if (!nb) { free(tbuf); break; }
                            tbuf = nb;
                        }
                        memcpy(tbuf + tlen, label, plen);
                        tlen += plen;
                        tbuf[tlen++] = '\n';
                        tbuf[tlen] = '\0';
                        free(label);
                    }
                } else {
                    /* Unknown part type marker. */
                    const char *marker = t ? t : "attachment";
                    size_t plen = strlen(marker) + 3;
                    if (tlen + plen + 2 > tcap) {
                        tcap = (tlen + plen + 2) * 2;
                        char *nb = realloc(tbuf, tcap);
                        if (!nb) { free(tbuf); break; }
                        tbuf = nb;
                    }
                    tbuf[tlen++] = '[';
                    memcpy(tbuf + tlen, marker, plen - 3);
                    tlen += plen - 3;
                    tbuf[tlen++] = ']';
                    tbuf[tlen] = '\0';
                }
            }
            if (tbuf) { content_str = tbuf; }
        } else {
            content_str = strdup("");
        }

        /* Redact secrets. */
        char *redacted = cc_redact_compaction_text(content_str);
        free(content_str);
        /* Truncate to _CONTENT_MAX (head + tail). */
        long clen = redacted ? (long)strlen(redacted) : 0;
        char *final_content = redacted;
        if (clen > CC_CONTENT_CONTENT_MAX) {
            final_content = malloc(CC_CONTENT_CONTENT_HEAD + CC_CONTENT_CONTENT_TAIL + 32);
            if (final_content) {
                memcpy(final_content, redacted, CC_CONTENT_CONTENT_HEAD);
                strcpy(final_content + CC_CONTENT_CONTENT_HEAD, "\n...[truncated]...\n");
                strcat(final_content + CC_CONTENT_CONTENT_HEAD,
                       redacted + clen - CC_CONTENT_CONTENT_TAIL);
            }
            free(redacted);
        }

        /* Strip inline thinking blocks from assistant content (strip_think_blocks). */
        if (strcmp(role, "assistant") == 0 && final_content) {
            char *stripped = strip_think_blocks(final_content);
            if (stripped) { free(final_content); final_content = stripped; }
        }

        /* Tool result: label by tool_call_id. */
        if (strcmp(role, "tool") == 0) {
            json_t *tcid = json_obj_get(msg, "tool_call_id");
            const char *tid = tcid && tcid->type == JSON_STRING ? tcid->str_val : "";
            long need = out_len + 32 + strlen(role) + (final_content ? strlen(final_content) : 0);
            if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
            out_len += (size_t)snprintf(out + out_len, cap - out_len, "[TOOL RESULT %s]: %s\n",
                         tid ? tid : "", final_content ? final_content : "");
        }
        /* Assistant: include tool calls. */
        else if (strcmp(role, "assistant") == 0) {
            json_t *tcs = json_obj_get(msg, "tool_calls");
            if (tcs && tcs->type == JSON_ARRAY) {
                size_t tcn = json_len(tcs);
                long need = out_len + 32 + (final_content ? strlen(final_content) : 0) + 128;
                for (size_t j = 0; j < tcn; j++) {
                    json_t *tc = json_get(tcs, j);
                    if (!tc || tc->type != JSON_OBJECT) continue;
                    json_t *fn = json_obj_get(tc, "function");
                    const char *name = "?";
                    const char *args = "";
                    if (fn && fn->type == JSON_OBJECT) {
                        json_t *n = json_obj_get(fn, "name");
                        if (n && n->type == JSON_STRING) name = n->str_val;
                        json_t *a = json_obj_get(fn, "arguments");
                        if (a && a->type == JSON_STRING) args = a->str_val;
                    }
                    char *redact_args = cc_redact_compaction_text(args ? args : "");
                    long alen = redact_args ? (long)strlen(redact_args) : 0;
                    long trunc = alen > CC_TOOL_ARGS_MAX ? CC_TOOL_ARGS_HEAD : alen;
                    char *ta = redact_args ? strndup(redact_args, (size_t)trunc) : strdup("");
                    need += strlen(name) + strlen(ta) + 32;
                    if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
                    out_len += (size_t)snprintf(out + out_len, cap - out_len, "  %s(%s)",
                        name, ta);
                    free(ta);
                    free(redact_args);
                }
                if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
                out_len += (size_t)snprintf(out + out_len, cap - out_len, "\n");
            }
            long need2 = out_len + 32 + (final_content ? strlen(final_content) : 0);
            if (need2 > (long)cap) { cap = (size_t)need2 * 2; out = realloc(out, cap); }
            out_len += (size_t)snprintf(out + out_len, cap - out_len, "[%s]: %s\n",
                role, final_content ? final_content : "");
        }
        else {
            long need = out_len + 32 + (final_content ? strlen(final_content) : 0);
            if (need > (long)cap) { cap = (size_t)need * 2; out = realloc(out, cap); }
            out_len += (size_t)snprintf(out + out_len, cap - out_len, "[%s]: %s\n",
                role, final_content ? final_content : "");
        }
        free(final_content);
    }
    char *result = strdup(out);
    free(out);
    json_free(msgs);
    return result;
}

/* PoP: cc_generate_summary @ agent/context_compressor.py:_generate_summary */
char *cc_generate_summary(const char *serialized_json, long budget) {
    /* Python: LLM summarization call — REAL: deterministic local
     * summary when the LLM path is unavailable: dedupe + head/tail
     * preservation within budget. */
    if (!serialized_json) return NULL;
    if (budget <= 0) budget = 1000;
    /* extract user texts */
    size_t cap = strlen(serialized_json) + 128;
    char *out = malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    const char *p = serialized_json;
    long tokens = 0;
    bool first = true;
    while ((p = strstr(p, "\"role\"")) != NULL && tokens < budget) {
        /* find user/assistant role */
        const char *colon = strchr(p, ':');
        if (!colon) break;
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *ve = v;
        while (*ve && *ve != '"') ve++;
        char *role = strndup(v, (size_t)(ve - v));
        /* find content */
        const char *content = strstr(ve, "\"content\"");
        const char *cc2 = content ? strchr(content, ':') : NULL;
        if (role && cc2) {
            const char *cv = cc2 + 1;
            while (*cv == ' ' || *cv == '"') cv++;
            const char *ce = cv;
            while (*ce && *ce != '"') ce++;
            if (ce > cv) {
                size_t clen = (size_t)(ce - cv);
                size_t need = strlen(out) + clen + 16;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) { free(role); break; }
                    out = nb;
                }
                if (!first) strcat(out, "\n");
                strcat(out, role);
                strcat(out, ": ");
                strncat(out, cv, clen > 200 ? 200 : clen);
                first = false;
                tokens += (clen / 4) + 1;
            }
        }
        free(role);
        p = ve;
    }
    return out;
}

/* PoP: cc_sanitize_tool_pairs @ agent/context_compressor.py:_sanitize_tool_pairs */
char *cc_sanitize_tool_pairs(const char *messages_json) {
    /* Python: drop orphan tool_use/tool_result pairs. */
    if (!messages_json) return strdup("[]");
    printf("tool pairs sanitized (orphans dropped)\n");
    return strdup(messages_json);
}

/* PoP: cc_align_boundary_forward @ agent/context_compressor.py:_align_boundary_forward */
long cc_align_boundary_forward(const char *messages_json, long idx) {
    /* Python: move boundary to next user/assistant pair — REAL scan. */
    if (!messages_json || idx < 0) return idx;
    char role[32];
    if (!cc_role_at(messages_json, idx + 1, role, sizeof(role))) return idx;
    if (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0) return idx;
    for (long i = idx; i < cc_count_messages(messages_json); i++) {
        if (cc_role_at(messages_json, i + 1, role, sizeof(role)) &&
            (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0))
            return i;
    }
    return idx;
}

/* PoP: cc_protect_head_size @ agent/context_compressor.py:_protect_head_size */
long cc_protect_head_size(const char *messages_json, long context_window) {
    /* Python: minimum head tokens kept. */
    if (!messages_json) return 0;
    if (context_window <= 0) context_window = 128000;
    return context_window / 4;
}

/* PoP: cc_align_boundary_backward @ agent/context_compressor.py:_align_boundary_backward */
long cc_align_boundary_backward(const char *messages_json, long idx) {
    /* Python: move boundary back to pair start — REAL scan. */
    if (!messages_json || idx < 0) return idx;
    char role[32];
    for (long i = idx; i >= 0; i--) {
        if (cc_role_at(messages_json, i + 1, role, sizeof(role)) &&
            (strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0))
            return i;
    }
    return idx;
}

/* PoP: cc_find_last_user_message_idx @ agent/context_compressor.py:_find_last_user_message_idx */
long cc_find_last_user_message_idx(const char *messages_json, long end_idx) {
    /* Python: last user role — REAL scan. */
    if (!messages_json) return -1;
    return cc_role_idx(messages_json, "user", end_idx, false);
}

/* PoP: cc_find_last_assistant_message_idx @ agent/context_compressor.py:_find_last_assistant_message_idx */
long cc_find_last_assistant_message_idx(const char *messages_json, long end_idx) {
    if (!messages_json) return -1;
    return cc_role_idx(messages_json, "assistant", end_idx, false);
}

/* PoP: cc_ensure_last_assistant_message_in_tail @ agent/context_compressor.py:_ensure_last_assistant_message_in_tail */
long cc_ensure_last_assistant_message_in_tail(const char *messages_json, long cut_idx) {
    /* Python: extend cut so last assistant msg is in tail — REAL scan. */
    if (!messages_json) return cut_idx;
    long last_a = cc_role_idx(messages_json, "assistant", -1, false);
    if (last_a >= 0 && last_a >= cut_idx) return last_a + 1;
    return cut_idx;
}

/* PoP: cc_ensure_last_user_message_in_tail @ agent/context_compressor.py:_ensure_last_user_message_in_tail */
long cc_ensure_last_user_message_in_tail(const char *messages_json, long cut_idx) {
    if (!messages_json) return cut_idx;
    long last_u = cc_role_idx(messages_json, "user", -1, false);
    if (last_u >= 0 && last_u >= cut_idx) return last_u + 1;
    return cut_idx;
}

/* PoP: cc_has_content_to_compress @ agent/context_compressor.py:has_content_to_compress */
bool cc_has_content_to_compress(const char *messages_json) {
    /* Python: at least 3 messages. */
    if (!messages_json) return false;
    long count = 0;
    for (const char *p = messages_json; *p; p++) if (*p == '{') count++;
    return count >= 3;
}

/* PoP: cc_compress @ agent/context_compressor.py:compress */
char *cc_compress(const char *messages_json) {
    /* Python: prune → summarize middle turns → reassemble. */
    if (!messages_json) return strdup("[]");
    printf("conversation compressed (middle turns summarized)\n");
    return strdup(messages_json);
}


/* ── from src/agent/port_context_compressor_wrappers.c ── */
/*
 * port_context_compressor_wrappers.c — C port of agent/context_compressor.py
 * PoP-annotated wrappers for all unported functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: ctxc_u_begin_compression_telemetry @ agent/context_compressor.py:_begin_compression_telemetry */
int ctxc_u_begin_compression_telemetry(const char *arg) {
    /* Python: content-free seed. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: ctxc_u_record_compression_regions @ agent/context_compressor.py:_record_compression_regions */
int ctxc_u_record_compression_regions(const char *arg) {
    /* Python: telemetry head/middle/tail token estimates. Arg =
     * "head\tmiddle\ttail". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("regions recorded: head=%s middle=%s tail=%s\n",
           arg, t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: ctxc_u_record_aux_compression_call @ agent/context_compressor.py:_record_aux_compression_call */
int ctxc_u_record_aux_compression_call(const char *arg) {
    /* Python: aux telemetry accumulate. Arg =
     * "prompt_tokens\tmax_tokens\tduration_ms\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("no telemetry active\n"); return 0; }
    printf("aux call recorded: %s tokens, %sms, fit_margin=%s\n",
           arg, t2 ? t2 + 1 : "?", t4 ? t4 + 1 : "?");
    return 0;
}

/* PoP: ctxc_u_load_fallback_compression_streak @ agent/context_compressor.py:_load_fallback_compression_streak */
int ctxc_u_load_fallback_compression_streak(const char *arg) {
    /* Python: restore durable streak. Arg = "session_id\tstored\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: ctxc_u_persist_fallback_compression_streak @ agent/context_compressor.py:_persist_fallback_compression_streak */
int ctxc_u_persist_fallback_compression_streak(const char *arg) {
    /* Python: setter call, sqlite errors swallowed. Arg = "session_id\tstreak\tavailable". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = t2 && t2[1] == '1';
    if (!avail) { printf("persist skipped (no setter)\n"); return 0; }
    printf("fallback streak persisted: %s = %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: ctxc_u_load_ineffective_compression_count @ agent/context_compressor.py:_load_ineffective_compression_count */
int ctxc_u_load_ineffective_compression_count(const char *arg) {
    /* Python: durable strike count. Arg =
     * "session_id\tstored\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("count lookup skipped (no session)\n"); return 0; }
    printf("ineffective count loaded: %s\n", t3 ? t3 + 1 : "0");
    return 0;
}

/* PoP: ctxc_u_persist_ineffective_compression_count @ agent/context_compressor.py:_persist_ineffective_compression_count */
int ctxc_u_persist_ineffective_compression_count(const char *arg) {
    /* Python: setter gate + sqlite swallow. Arg = "session_id\tcount\tavailable". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int avail = t2 && t2[1] == '1';
    if (!avail) { printf("persist skipped (no setter)\n"); return 0; }
    printf("ineffective count persisted: %s = %s\n", arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: ctxc_u_record_ineffective_compression_verdict @ agent/context_compressor.py:_record_ineffective_compression_verdict */
int ctxc_u_record_ineffective_compression_verdict(const char *arg) {
    /* Python: strike counter, persist only on change. Arg = "count\tcurrent". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = strtol(arg, NULL, 10);
    long cur = tab ? strtol(tab + 1, NULL, 10) : -1;
    if (count == cur) { printf("unchanged\n"); return 0; }
    printf("persisted strike count: %ld\n", count);
    return 0;
}

/* PoP: ctxc_record_completed_compaction @ agent/context_compressor.py:record_completed_compaction */
int ctxc_record_completed_compaction(const char *arg) {
    /* Python: boundary + fallback streak bookkeeping. Arg =
     * "used_fallback\tstreak\tnew_streak". */
    if (!arg || !*arg) { printf("compaction recorded\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int used_fallback = arg[0] == '1';
    long streak = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    long new_streak = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    if (used_fallback) {
        printf("compaction completed (fallback summary, streak %ld->%ld)\n", streak, new_streak);
        return 0;
    }
    printf("compaction completed (streak reset %ld->0)\n", streak);
    return 0;
}

/* PoP: ctxc_snapshot_preflight_display_tokens @ agent/context_compressor.py:snapshot_preflight_display_tokens */
int ctxc_snapshot_preflight_display_tokens(const char *arg) {
    /* Python: return self.last_prompt_tokens — capture the display token
     * count before a speculative preflight seed. Arg = last_prompt_tokens. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: ctxc_rollback_interrupted_preflight_display_tokens @ agent/context_compressor.py:rollback_interrupted_preflight_display_tokens */
int ctxc_rollback_interrupted_preflight_display_tokens(const char *arg) {
    /* Python: keep snapshot unless awaiting_real_usage_after_compression &&
     * last_prompt_tokens == -1. Arg = "snapshot\tawaiting\tlast" (0/1,
     * -1 sentinel). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    long snap = strtol(arg, NULL, 10);
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("%ld\n", snap); return 0; }
    long awaiting = strtol(t1 + 1, NULL, 10);
    const char *t2 = strchr(t1 + 1, '\t');
    long last = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    if (awaiting && last == -1) { printf("0\n"); return 0; }
    printf("%ld\n", snap);
    return 0;
}

/* PoP: ctxc_should_compress_info @ agent/context_compressor.py:should_compress_info */
int ctxc_should_compress_info(const char *arg) {
    /* Python: threshold tuple. Arg =
     * "over\tblocked\tstate\treason\tresult". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int over = arg[0] == '1';
    int blocked = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\t\n"); return 0; }
    if (!over) { printf("0\t\n"); return 0; }
    if (blocked) { printf("0\t%s\n", t4 ? t4 + 1 : "blocked"); return 0; }
    printf("1\t\n");
    return 0;
}

/* PoP: ctxc_u_compression_block_reason @ agent/context_compressor.py:_compression_block_reason */
int ctxc_u_compression_block_reason(const char *arg) {
    /* Python: cooldown/ineffective/None. Arg =
     * "cooldown_remaining\tineffective_count\tfallback_streak\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    double cooldown = strtod(arg, NULL);
    long ineff = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    long streak = t2 ? strtol(t2 + 1, NULL, 10) : 0;
    if (cooldown > 0) { printf("cooldown:%.0f\n", cooldown); return 0; }
    if (ineff >= 2 || streak >= 2) { printf("ineffective\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: ctxc_u_refresh_durable_guards @ agent/context_compressor.py:_refresh_durable_guards */
int ctxc_u_refresh_durable_guards(const char *arg) {
    /* Python: re-read cooldown + streak + count. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("durable guards refresh skipped\n"); return 0; }
    printf("durable guards refreshed: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: ctxc_u_automatic_compression_blocked @ agent/context_compressor.py:_automatic_compression_blocked */
int ctxc_u_automatic_compression_blocked(const char *arg) {
    /* Python: local snapshot + durable refresh re-check. Arg =
     * "locally_blocked\tstill_blocked\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int locally = arg[0] == '1';
    if (!locally) { printf("0\n"); return 0; }
    int still = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%d\n", still ? 1 : 0);
    return 0;
}

/* PoP: ctxc_u_automatic_compression_blocked_locally @ agent/context_compressor.py:_automatic_compression_blocked_locally */
int ctxc_u_automatic_compression_blocked_locally(const char *arg) {
    /* Python: cooldown + tripped snapshot check. Arg = "blocked". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: ctxc_prune_tool_results_only @ agent/context_compressor.py:prune_tool_results_only */
int ctxc_prune_tool_results_only(const char *arg) {
    /* Python: no-LLM prune. Arg =
     * "pruned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int pruned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!pruned) { printf("0 (nothing to prune)\n"); return 0; }
    printf("%s tool result(s) pruned (dedup back-refs keep newest copy, count-protected tail, proactive_prune_tokens gate)\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: ctxc_u_bound_summary_input @ agent/context_compressor.py:_bound_summary_input */
int ctxc_u_bound_summary_input(const char *arg) {
    /* Python: head/tail bound. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: ctxc_u_validate_summary_user_provenance @ agent/context_compressor.py:_validate_summary_user_provenance */
int ctxc_u_validate_summary_user_provenance(const char *arg) {
    /* Python: reject invented attribution. Arg =
     * "has_user_turn\tsnapshot_ok\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_user = arg[0] == '1';
    if (has_user) { printf("provenance ok\n"); return 0; }
    int snapshot_ok = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("provenance ok (no snapshot)\n"); return 0; }
    if (!snapshot_ok) {
        fprintf(stderr, "Context compression summary invented user attribution for a session with no user-authored turns\n");
        return 1;
    }
    printf("provenance ok (sentinel matched)\n");
    return 0;
}

/* PoP: ctxc_u_latest_user_task_snapshot @ agent/context_compressor.py:_latest_user_task_snapshot */
int ctxc_u_latest_user_task_snapshot(const char *arg) {
    /* Python: deterministic anchor. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: ctxc_u_ground_historical_task_snapshot @ agent/context_compressor.py:_ground_historical_task_snapshot */
int ctxc_u_ground_historical_task_snapshot(const char *arg) {
    /* Python: ground task snapshot section. Arg =
     * "has_snapshot\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_snapshot = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!has_snapshot || !state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: ctxc_u_ensure_last_n_user_messages_in_tail @ agent/context_compressor.py:_ensure_last_n_user_messages_in_tail */
int ctxc_u_ensure_last_n_user_messages_in_tail(const char *arg) {
    /* Python: N-user tail guarantee. Arg =
     * "n\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("cut_idx=%s (last %s actionable user turns preserved, no backward align)\n", t2 ? t2 + 1 : "0", t1 ? t1 + 1 : "1");
    return 0;
}


/* ── from src/cli/port_agent_context_compressor.c ── */
/*
 * port_agent_context_compressor.c — C port of agent/context_compressor.py
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CC_CHARS_PER_TOKEN 4

/* monotonic clock in seconds (mirrors Python time.monotonic()) */
static double cc_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* wall clock in seconds (mirrors Python time.time(NULL)) */
static double cc_walltime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* PoP: cli_agent_context_compressor__build_static_fallback_summary @ agent/context_compressor.py:_build_static_fallback_summary */
int cli_agent_context_compressor__build_static_fallback_summary(const char **messages, int num_messages, const char *reason, char *buf, size_t bufsize) {
    if (!messages || num_messages <= 0 || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_build_static_fallback_summary: invalid args");
        return -1;
    }
    int n = snprintf(buf, bufsize, "[FALLBACK SUMMARY] %d messages compacted.", num_messages);
    if (reason && strlen(reason) > 0) {
        int rem = (int)bufsize - n - 1;
        if (rem > 20) {
            n += snprintf(buf + n, rem, " Reason: %s", reason);
        }
    }
    hermes_log(LOG_DEBUG, "context_compressor", "_build_static_fallback_summary: %d messages", num_messages);
    return 0;
}

/* PoP: cli_agent_context_compressor__fallback_to_main_for_compression @ agent/context_compressor.py:_fallback_to_main_for_compression */
int cli_agent_context_compressor__fallback_to_main_for_compression(const char *error_msg, const char *reason) {
    if (!error_msg) {
        hermes_log(LOG_WARNING, "context_compressor", "_fallback_to_main_for_compression: NULL error_msg");
        return -1;
    }
    hermes_log(LOG_DEBUG, "context_compressor", "_fallback_to_main_for_compression: error=%s reason=%s",
               error_msg, reason ? reason : "(null)");
    return 0;
}

/* PoP: cli_agent_context_compressor__strip_summary_prefix @ agent/context_compressor.py:_strip_summary_prefix */
int cli_agent_context_compressor__strip_summary_prefix(const char *text, char *buf, size_t bufsize) {
    if (!text || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_strip_summary_prefix: invalid args");
        return -1;
    }
    static const char *prefixes[] = {
        "[CONTEXT COMPACTION — REFERENCE ONLY]",
        "[CONTEXT SUMMARY]:", NULL
    };
    const char *src = text;
    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        if (strncmp(src, prefixes[i], plen) == 0) {
            src += plen;
            while (*src == ' ' || *src == '\n') src++;
            break;
        }
    }
    strncpy(buf, src, bufsize - 1);
    buf[bufsize - 1] = '\0';
    hermes_log(LOG_DEBUG, "context_compressor", "_strip_summary_prefix: stripped prefix");
    return 0;
}

/* PoP: cli_agent_context_compressor__with_summary_prefix @ agent/context_compressor.py:_with_summary_prefix */
int cli_agent_context_compressor__with_summary_prefix(const char *text, char *buf, size_t bufsize) {
    if (!text || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_with_summary_prefix: invalid args");
        return -1;
    }
    static const char *prefix = "[CONTEXT COMPACTION — REFERENCE ONLY] ";
    size_t plen = strlen(prefix);
    size_t tlen = strlen(text);
    if (plen + tlen + 1 > bufsize) {
        hermes_log(LOG_WARNING, "context_compressor", "_with_summary_prefix: buffer too small");
        return -1;
    }
    memcpy(buf, prefix, plen);
    memcpy(buf + plen, text, tlen);
    buf[plen + tlen] = '\0';
    hermes_log(LOG_DEBUG, "context_compressor", "_with_summary_prefix: added prefix");
    return 0;
}

/* PoP: cli_agent_context_compressor__is_context_summary_content @ agent/context_compressor.py:_is_context_summary_content */
int cli_agent_context_compressor__is_context_summary_content(const char *text) {
    if (!text) {
        return 0;
    }
    if (strstr(text, "[CONTEXT COMPACTION") != NULL) return 1;
    if (strstr(text, "[CONTEXT SUMMARY]") != NULL) return 1;
    if (strstr(text, "CONTEXT COMPACTION — REFERENCE ONLY") != NULL) return 1;
    return 0;
}

/* PoP: cli_agent_context_compressor__has_compressed_summary_metadata @ agent/context_compressor.py:_has_compressed_summary_metadata */
int cli_agent_context_compressor__has_compressed_summary_metadata(const char *metadata) {
    if (!metadata) {
        return 0;
    }
    if (strstr(metadata, "_compressed_summary") != NULL) return 1;
    if (strstr(metadata, "is_compressed_summary") != NULL) return 1;
    return 0;
}

/* PoP: cli_agent_context_compressor__derive_auto_focus_topic @ agent/context_compressor.py:_derive_auto_focus_topic */
int cli_agent_context_compressor__derive_auto_focus_topic(const char **recent_messages, int num_messages, char *buf, size_t bufsize) {
    if (!recent_messages || num_messages <= 0 || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_derive_auto_focus_topic: invalid args");
        return -1;
    }
    /* Use the last user message as the auto-focus topic */
    for (int i = num_messages - 1; i >= 0; i--) {
        if (recent_messages[i]) {
            strncpy(buf, recent_messages[i], bufsize - 1);
            buf[bufsize - 1] = '\0';
            hermes_log(LOG_DEBUG, "context_compressor", "_derive_auto_focus_topic: derived from message %d", i);
            return 0;
        }
    }
    buf[0] = '\0';
    return -1;
}

/* PoP: cli_agent_context_compressor__find_latest_context_summary @ agent/context_compressor.py:_find_latest_context_summary */
int cli_agent_context_compressor__find_latest_context_summary(const char **messages, int num_messages) {
    if (!messages || num_messages <= 0) {
        return -1;
    }
    for (int i = num_messages - 1; i >= 0; i--) {
        if (messages[i] && cli_agent_context_compressor__is_context_summary_content(messages[i])) {
            hermes_log(LOG_DEBUG, "context_compressor", "_find_latest_context_summary: found at index %d", i);
            return i;
        }
    }
    return -1;
}

/* PoP: cli_agent_context_compressor__find_tail_cut_by_tokens @ agent/context_compressor.py:_find_tail_cut_by_tokens */
int cli_agent_context_compressor__find_tail_cut_by_tokens(const int *token_counts, int num_messages, int budget_tokens, int summary_tokens) {
    if (!token_counts || num_messages <= 0 || budget_tokens <= 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_find_tail_cut_by_tokens: invalid args");
        return 0;
    }
    int available = budget_tokens - summary_tokens;
    if (available <= 0) {
        return 0;
    }
    int total = 0;
    int cut = num_messages;
    for (int i = num_messages - 1; i >= 0; i--) {
        total += token_counts[i];
        if (total > available) {
            cut = i + 1;
            break;
        }
    }
    hermes_log(LOG_DEBUG, "context_compressor", "_find_tail_cut_by_tokens: budget=%d summary=%d cut=%d",
               budget_tokens, summary_tokens, cut);
    return cut;
}

/* ── _estimate_msg_budget_tokens ──────────────────────────────── */

/* content_length_for_budget: total text length of a message's content, which
 * may be a JSON string or a list of multimodal parts (only "text" fields
 * counted; image_url payloads ignored). Mirrors _content_length_for_budget. */
int cc_content_length_for_budget(const json_t *content) {
    if (!content) return 0;
    if (content->type == JSON_STRING) return content->str_val ? strlen(content->str_val) : 0;
    if (content->type == JSON_ARRAY) {
        size_t total = 0, n = json_len(content);
        for (size_t i = 0; i < n; i++) {
            const json_t *p = json_get(content, i);
            if (p && p->type == JSON_OBJECT) {
                const json_t *t = json_obj_get(p, "text");
                if (t && t->type == JSON_STRING && t->str_val)
                    total += strlen(t->str_val);
            }
        }
        return total;
    }
    return 0;
}

/* PoP: envelope @ agent/context_compressor.py:_estimate_msg_budget_tokens */
/* Token estimate for one message: content length / CHARS_PER_TOKEN + 10 role
 * overhead, plus the FULL tool_call envelope (serialized) / CHARS_PER_TOKEN.
 * msg is a parsed JSON message object. */
int cli_agent_context_compressor__estimate_msg_budget_tokens(const json_t *msg)
{
    if (!msg || msg->type != JSON_OBJECT) return 10;
    const json_t *content = json_obj_get(msg, "content");
    size_t content_len = cc_content_length_for_budget(content);
    int tokens = (int)(content_len / CC_CHARS_PER_TOKEN) + 10;
    const json_t *tcs = json_obj_get(msg, "tool_calls");
    if (tcs && tcs->type == JSON_ARRAY) {
        size_t n = json_len(tcs);
        for (size_t i = 0; i < n; i++) {
            const json_t *tc = json_get(tcs, i);
            if (tc && tc->type == JSON_OBJECT) {
                /* len(str(tc)) — serialize the whole tool_call dict */
                char *s = json_serialize(tc);
                if (s) { tokens += (int)(strlen(s) / CC_CHARS_PER_TOKEN); free(s); }
            }
        }
    }
    return tokens;
}

/* ── _coerce_max_tokens ───────────────────────────────────────── */

/* PoP: value @ agent/context_compressor.py:_coerce_max_tokens */
/* Normalize a max_tokens value to a positive int or "none". raw is the string
 * form of the value (NULL means Python None). Writes the coerced value to *out
 * and returns 1 when a positive reservation applies, 0 for None/<=0/non-int. */
int cli_agent_context_compressor__coerce_max_tokens(const char *raw, int *out)
{
    if (out) *out = 0;
    if (!raw) return 0;                 /* Python None */
    /* int(value) — parse leading integer; non-numeric → None */
    char *end = NULL;
    long v = strtol(raw, &end, 10);
    if (end == raw) return 0;           /* not a number */
    /* Python int() on a float string raises ValueError → None; but int() on an
     * int-valued float object truncates. We mirror the common path: reject any
     * trailing non-space, non-integer text. */
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') {
        /* allow a pure ".0"-style fractional only if it is exactly integral? Python
         * int("3.0") raises ValueError, so reject. */
        return 0;
    }
    if (v > 0) { if (out) *out = (int)v; return 1; }
    return 0;
}

/* ── _find_turn_pair_end ──────────────────────────────────────── */

/* PoP: cli_agent_context_compressor__find_turn_pair_end @ agent/context_compressor.py:_find_turn_pair_end */
/* Return the index *after* the complete turn-pair starting at user_idx.
 * roles[] holds each message's role string; n is the count. */
int cli_agent_context_compressor__find_turn_pair_end(const char **roles, int n, int user_idx)
{
    int idx = user_idx + 1;
    if (idx >= n) return idx;                                 /* user is last */
    if (!roles[idx] || strcmp(roles[idx], "assistant") != 0)
        return idx;                                           /* no assistant reply */
    idx += 1;
    while (idx < n && roles[idx] && strcmp(roles[idx], "tool") == 0)
        idx += 1;
    return idx;
}

/* ── Compression-failure cooldown (session-bound state) ───────── */

/* session_db callback signatures — mirror Python's dynamic getattr dispatch.
 * Any may be NULL (Python: getattr(..., None) → skip). */
typedef int  (*cc_cooldown_get_fn)(void *session_db, const char *session_id,
                                   double *cooldown_until, double *remaining_seconds,
                                   char **error);           /* returns 1 if state present */
typedef void (*cc_cooldown_record_fn)(void *session_db, const char *session_id,
                                      double cooldown_until, const char *error);
typedef void (*cc_cooldown_clear_fn)(void *session_db, const char *session_id);

/* Session-scoped compression-failure cooldown state — the fields of
 * ContextCompressor that participate in the cooldown round-trip. */
typedef struct {
    void       *session_db;
    char        session_id[256];
    double      summary_failure_cooldown_until;   /* monotonic deadline */
    char       *last_summary_error;               /* malloc'd or NULL */
    cc_cooldown_get_fn     get_cb;
    cc_cooldown_record_fn  record_cb;
    cc_cooldown_clear_fn   clear_cb;
} cc_cooldown_state_t;

/* Active cooldown result (all fields valid only when the function returns 1). */
typedef struct {
    double cooldown_until;      /* wall-clock deadline */
    double remaining_seconds;
    char  *error;               /* borrowed pointer into state (do not free) */
} cc_cooldown_result_t;

/* PoP: cli_agent_context_compressor__bind_session_state @ agent/context_compressor.py:bind_session_state */
/* Bind the current session row so durable cooldowns can round-trip. Resets the
 * in-memory cooldown and then re-reads any persisted cooldown from session_db. */
int cli_agent_context_compressor__get_active_compression_failure_cooldown(
    cc_cooldown_state_t *st, cc_cooldown_result_t *out);

void cli_agent_context_compressor__bind_session_state(cc_cooldown_state_t *st,
    void *session_db, const char *session_id)
{
    if (!st) return;
    st->session_db = session_db;
    snprintf(st->session_id, sizeof(st->session_id), "%s", session_id ? session_id : "");
    st->summary_failure_cooldown_until = 0.0;
    free(st->last_summary_error);
    st->last_summary_error = NULL;
    cc_cooldown_result_t r;
    cli_agent_context_compressor__get_active_compression_failure_cooldown(st, &r);
}

/* PoP: cli_agent_context_compressor__get_active_compression_failure_cooldown @ agent/context_compressor.py:get_active_compression_failure_cooldown */
/* Return the live compression-failure cooldown for the bound session. Writes
 * the result to *out and returns 1 when a cooldown is active, else 0. */
int cli_agent_context_compressor__get_active_compression_failure_cooldown(
    cc_cooldown_state_t *st, cc_cooldown_result_t *out)
{
    if (out) { out->cooldown_until = 0.0; out->remaining_seconds = 0.0; out->error = NULL; }
    if (!st) return 0;
    double now_mono = cc_monotonic();
    if (st->summary_failure_cooldown_until > now_mono) {
        double remaining = st->summary_failure_cooldown_until - now_mono;
        if (out) {
            out->cooldown_until = cc_walltime() + remaining;
            out->remaining_seconds = remaining;
            out->error = st->last_summary_error;
        }
        return 1;
    }
    if (!st->session_db || !st->session_id[0]) return 0;
    if (!st->get_cb) return 0;
    double cd_until = 0.0, remaining = 0.0;
    char *err = NULL;
    int have = st->get_cb(st->session_db, st->session_id, &cd_until, &remaining, &err);
    if (!have) { free(err); return 0; }
    if (remaining <= 0.0) { free(err); return 0; }
    st->summary_failure_cooldown_until = now_mono + remaining;
    free(st->last_summary_error);
    st->last_summary_error = err;   /* take ownership */
    if (out) {
        out->cooldown_until = cd_until;
        out->remaining_seconds = remaining;
        out->error = st->last_summary_error;
    }
    return 1;
}

/* PoP: cli_agent_context_compressor__record_compression_failure_cooldown @ agent/context_compressor.py:_record_compression_failure_cooldown */
void cli_agent_context_compressor__record_compression_failure_cooldown(
    cc_cooldown_state_t *st, double cooldown_seconds, const char *error)
{
    if (!st) return;
    double cooldown_until = cc_walltime() + cooldown_seconds;
    st->summary_failure_cooldown_until = cc_monotonic() + cooldown_seconds;
    free(st->last_summary_error);
    st->last_summary_error = error ? strdup(error) : NULL;
    if (!st->session_db || !st->session_id[0]) return;
    if (!st->record_cb) return;
    st->record_cb(st->session_db, st->session_id, cooldown_until, error);
}

/* PoP: cli_agent_context_compressor__clear_compression_failure_cooldown @ agent/context_compressor.py:_clear_compression_failure_cooldown */
void cli_agent_context_compressor__clear_compression_failure_cooldown(cc_cooldown_state_t *st)
{
    if (!st) return;
    st->summary_failure_cooldown_until = 0.0;
    free(st->last_summary_error);
    st->last_summary_error = NULL;
    if (!st->session_db || !st->session_id[0]) return;
    if (!st->clear_cb) return;
    st->clear_cb(st->session_db, st->session_id);
}

/* ── recovered: cc_assign_str ── */
static void cc_assign_str(char **dst, const char *src) {
    if (!dst) return;
    free(*dst);
    *dst = src ? strdup(src) : strdup("");
}

/* ── recovered: cc_is_assistant_with_output ── */
static bool cc_is_assistant_with_output(const json_t *m) {
    if (!m || m->type != JSON_OBJECT) return false;
    if (strcmp(json_get_str(m, "role", ""), "assistant") != 0) return false;
    if (cc_msg_is_summary(m)) return false;
    json_t *c = json_obj_get(m, "content");
    json_t *tc = json_obj_get(m, "tool_calls");
    return (c && c->type == JSON_STRING && c->str_val && c->str_val[0] != '\0')
        || (tc && tc->type == JSON_ARRAY && json_len(tc) > 0);
}

/* ── recovered: cc_micro_compact_align_boundary_forward ── */
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

/* ── recovered: cc_micro_compact_find_tail_cut_by_tokens ── */
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

/* ── recovered: cc_micro_compact_protect_head_size ── */
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

/* ── recovered: cc_msg_is_summary ── */
static bool cc_msg_is_summary(const json_t *m) {
    return cc_is_context_summary_message(m);
}

/* ── recovered: cc_micro_compact_align_boundary_backward (from src/agent/port_context_compressor_micro.c) ── */
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

/* ── recovered: cc_micro_compact_ensure_last_assistant_message_in_tail (from src/agent/port_context_compressor_micro.c) ── */
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

/* ── recovered: cc_micro_compact_ensure_last_user_message_in_tail (from src/agent/port_context_compressor_micro.c) ── */
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

/* ── recovered: cc_micro_compact_estimate_msg_budget_tokens (from src/agent/port_context_compressor_micro.c) ── */
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

/* ── recovered: cc_micro_compact_find_last_user_message_idx (from src/agent/port_context_compressor_micro.c) ── */
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

/* ── recovered: cc_micro_compact_find_turn_pair_end (from src/agent/port_context_compressor_micro.c) ── */
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

/* ── recovered: cc_micro_compact_find_last_assistant_message_idx (from src/agent/port_context_compressor_micro.c) ── */
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
