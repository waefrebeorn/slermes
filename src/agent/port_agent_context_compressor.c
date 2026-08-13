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

/* Metadata keys (mirror Python module literals). */
#define CC_MICRO_COMPACT_MARKER_KEY    "_micro_compact_marker"
#define CC_COMPRESSED_SUMMARY_HAS_USER_TURN_KEY "_compressed_summary_has_user_turn"
#define CC_DB_PERSISTED_MARKER         "_db_persisted"
#define CC_PROACTIVE_PRUNE_REARM_KEY   "_proactive_prune_rearm_tokens"
#define CC_DEF_DEFRAG_THRESHOLD_TOKENS 8000   /* _PRUNE_MIN_CHARS default */

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

/* PoP: _load_proactive_prune_rearm_tokens @ agent/context_compressor.py:_load_proactive_prune_rearm_tokens */
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

/* PoP: _clear_durable_proactive_prune_rearm @ agent/context_compressor.py:_clear_durable_proactive_prune_rearm */
void cc_clear_durable_proactive_prune_rearm(context_compressor_t *cc) {
    if (!cc || !cc->session_id || !cc->session_db) return;
    hermes_state_patch_session_model_config(
        (hermes_state_db_t *)cc->session_db, cc->session_id,
        "{\"" CC_PROACTIVE_PRUNE_REARM_KEY "\":null}");
}


/* ── note_request_rough_estimate ────────────────────────────────────────── */
/* (canonical PoP-tagged impl — see context_compressor_pure.c)              */
/* ── _emit_init_summary_once ──────────────────────────────────────────── */
/* PoP: _emit_init_summary_once @ agent/context_compressor.py:_emit_init_summary_once */
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
/* PoP: _build_micro_summary_prompt @ agent/context_compressor.py:_build_micro_summary_prompt */
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
/* PoP: _resolve_compact_cursor @ agent/context_compressor.py:_resolve_compact_cursor */
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
/* PoP: _find_one_exchange @ agent/context_compressor.py:_find_one_exchange */
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
/* PoP: _serialize_one_exchange @ agent/context_compressor.py:_serialize_one_exchange */
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
/* PoP: _splice_micro_compact_result @ agent/context_compressor.py:_splice_micro_compact_result */
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
/* PoP: _needs_defrag @ agent/context_compressor.py:_needs_defrag */
bool cc_needs_defrag(const context_compressor_t *cc) {
    if (!cc || !cc->micro_compact_rolling_summary) return false;
    long tokens = (long)(strlen(cc->micro_compact_rolling_summary) / 4);
    return tokens >= cc->micro_compact_defrag_threshold_tokens;
}

/* ── _emit_micro_compaction_telemetry ─────────────────────────────────── */
/* PoP: _emit_micro_compaction_telemetry @ agent/context_compressor.py:_emit_micro_compaction_telemetry */
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
static void cc_assign_str(char **dst, const char *src) {
    if (!dst) return;
    free(*dst);
    *dst = src ? strdup(src) : strdup("");
}
static bool cc_msg_is_summary(const json_t *m) {
    return cc_is_context_summary_message(m);
}
static bool cc_is_assistant_with_output(const json_t *m) {
    if (!m || m->type != JSON_OBJECT) return false;
    if (strcmp(json_get_str(m, "role", ""), "assistant") != 0) return false;
    if (cc_msg_is_summary(m)) return false;
    json_t *c = json_obj_get(m, "content");
    json_t *tc = json_obj_get(m, "tool_calls");
    return (c && c->type == JSON_STRING && c->str_val && c->str_val[0] != '\0')
        || (tc && tc->type == JSON_ARRAY && json_len(tc) > 0);
}
