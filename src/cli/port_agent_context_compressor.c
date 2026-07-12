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
/* wall clock in seconds (mirrors Python time.time()) */
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
static size_t cc_content_length_for_budget(const json_t *content) {
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

/* PoP: cli_agent_context_compressor__estimate_msg_budget_tokens @ agent/context_compressor.py:_estimate_msg_budget_tokens */
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

/* PoP: cli_agent_context_compressor__coerce_max_tokens @ agent/context_compressor.py:_coerce_max_tokens */
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
