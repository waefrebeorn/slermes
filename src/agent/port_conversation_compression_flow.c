/*
 * port_conversation_compression_flow.c -- compression-pipeline FLOW cores
 * ported from agent/conversation_compression.py. See
 * port_conversation_compression_flow.h for the seam contract.
 *
 * Decision cores perform NO I/O: every live side effect (LLM summary call,
 * SessionDB mutation, clock read, tokenizer estimate, telemetry emit, warning
 * emit, uuid mint, thread/future wait) is a SEAM fed from scripted json_t
 * results in `in`. Each seam call is recorded in the returned decision
 * sequence so the differential oracle can compare ordered behavior against
 * the live Python under identical scripted inputs.
 *
 * Extends the oracle-proven port_conversation_compression.c pure slice;
 * context_compressor*.c (concurrent port) is deliberately untouched.
 */

#include "port_conversation_compression_flow.h"

/* Oracle-proven pure slice (port_conversation_compression.c); declared here
 * so this file compiles without the runner's extra -I lib path. The symbols
 * themselves are untouched and link from port_conversation_compression.o. */
extern json_t *cc_strip_marker_for_comparison(const json_t *msgs);
extern int cc_is_compaction_progress_status(const char *text, int marked);

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 * Shared utilities (Python truthiness / equality / string semantics)
 * =========================================================================== */

static int jtruth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *jstrv(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *jget(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static const char *jgets(const json_t *o, const char *k)
{
    const char *s = jstrv(jget(o, k));
    return s ? s : "";
}

static int jhas(const json_t *o, const char *k)
{
    return jget(o, k) != NULL;
}

static int jequal(const json_t *a, const json_t *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_NULL: return 1;
    case JSON_BOOL: return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                    b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!jequal(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    case JSON_OBJECT:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !jequal(a->c.items[i], bv)) return 0;
        }
        return 1;
    }
    return 0;
}

static json_t *act(const char *name)
{
    json_t *o = json_object();
    json_set(o, "a", json_string(name));
    return o;
}

static void push(json_t *log, json_t *a)
{
    json_append(log, a);
}

static void set_str(json_t *o, const char *k, const char *s)
{
    json_set(o, k, s ? json_string(s) : json_null());
}

static void set_num(json_t *o, const char *k, double n)
{
    json_set(o, k, json_number(n));
}

static void set_bool(json_t *o, const char *k, int b)
{
    json_set(o, k, json_bool(b != 0));
}

static void set_val(json_t *o, const char *k, const json_t *v)
{
    json_set(o, k, v ? json_copy(v) : json_null());
}

/* Python str.strip() over ASCII whitespace (space \t \n \r \v \f). Returns a
 * malloc'd copy; caller frees. */
static char *py_strip(const char *s)
{
    if (!s) s = "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'
           || *s == '\v' || *s == '\f') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                     || s[n - 1] == '\r' || s[n - 1] == '\v' || s[n - 1] == '\f'))
        n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int py_isspace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Remove array element at idx (Python list.pop(idx)); frees the item. */
static void arr_remove(json_t *arr, size_t idx)
{
    if (!arr || arr->type != JSON_ARRAY || idx >= arr->c.count) return;
    json_free(arr->c.items[idx]);
    for (size_t i = idx; i + 1 < arr->c.count; i++)
        arr->c.items[i] = arr->c.items[i + 1];
    arr->c.count--;
}

/* _message_text (agent/conversation_compression_scaffolding.py): str content
 * verbatim; list content joins dict parts' text-or-content with "\n". Static
 * duplicate of the claimed cc_message_text leaf so this file links without
 * the helpers object. Non-string text values are stringified for numbers;
 * container text values are out of harness scope (Python repr differs). */
static char *msg_text(const json_t *message)
{
    const json_t *content = (message && message->type == JSON_OBJECT)
                                ? json_obj_get(message, "content") : NULL;
    if (!content) return strdup("");
    if (content->type == JSON_STRING)
        return strdup(content->str_val ? content->str_val : "");
    if (content->type != JSON_ARRAY) return strdup("");
    size_t cap = 256, len = 0;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    for (size_t i = 0; i < content->c.count; i++) {
        const json_t *part = content->c.items[i];
        if (!part || part->type != JSON_OBJECT) continue;
        const json_t *t = json_obj_get(part, "text");
        if (!jtruth(t)) t = json_obj_get(part, "content");
        char buf[64];
        const char *text = "";
        if (jtruth(t)) {
            if (t->type == JSON_STRING) text = t->str_val ? t->str_val : "";
            else if (t->type == JSON_NUMBER) {
                if (t->num_val == (double)(long long)t->num_val)
                    snprintf(buf, sizeof buf, "%lld", (long long)t->num_val);
                else snprintf(buf, sizeof buf, "%g", t->num_val);
                text = buf;
            } else if (t->type == JSON_BOOL) text = t->bool_val ? "True" : "False";
        }
        size_t tlen = strlen(text);
        size_t need = len + tlen + 2;
        if (need > cap) {
            while (need > cap) cap *= 2;
            char *nw = realloc(out, cap);
            if (!nw) { free(out); return strdup(""); }
            out = nw;
        }
        if (len > 0) out[len++] = '\n';
        memcpy(out + len, text, tlen);
        len += tlen;
        out[len] = '\0';
    }
    return out;
}

/* Python int(x) for the type-tagged seam values used by the type-pinned
 * readers. kind: "missing"/"null"/"bool"/"int"/"float"/"str". Returns 1 on
 * success (out set), 0 when Python raises TypeError/ValueError. Strings:
 * optional surrounding whitespace, optional sign, decimal digits with
 * underscores BETWEEN digits (Python int() literal rules, ASCII subset). */
static int py_int_coerce(const json_t *v, const char *kind, long long *out)
{
    if (!kind) kind = "null";
    if (strcmp(kind, "int") == 0) {
        *out = (long long)(v ? v->num_val : 0);
        return 1;
    }
    if (strcmp(kind, "bool") == 0) {
        *out = (v && v->type == JSON_BOOL && v->bool_val) ? 1 : 0;
        return 1;
    }
    if (strcmp(kind, "float") == 0) {
        double d = v ? v->num_val : 0.0;
        *out = (long long)d;  /* truncation toward zero */
        return 1;
    }
    if (strcmp(kind, "str") == 0) {
        const char *s = jstrv(v);
        if (!s) return 0;
        while (py_isspace(*s)) s++;
        const char *e = s + strlen(s);
        while (e > s && py_isspace(e[-1])) e--;
        int neg = 0;
        if (e > s && (*s == '+' || *s == '-')) { neg = (*s == '-'); s++; }
        if (s == e) return 0;
        long long val = 0;
        int prev_digit = 0;
        for (const char *p = s; p < e; p++) {
            if (*p == '_') {
                if (!prev_digit || p + 1 == e || !isdigit((unsigned char)p[1]))
                    return 0;
                prev_digit = 0;
                continue;
            }
            if (!isdigit((unsigned char)*p)) return 0;
            val = val * 10 + (*p - '0');
            prev_digit = 1;
        }
        *out = neg ? -val : val;
        return 1;
    }
    return 0;  /* null / missing / containers: TypeError */
}

/* ===========================================================================
 * Pure helpers
 * =========================================================================== */

/* compaction_status_kind: "compacting" iff kind == "lifecycle" and the text
 * is a marked, non-empty CompactionProgressText; else str(kind). The type
 * mark rides `marked` (the proven cc_is_compaction_progress_status port). */
/* PoP: ccflow_compaction_status_kind @ agent/conversation_compression.py:compaction_status_kind */
json_t *ccflow_compaction_status_kind(const json_t *in)
{
    const char *kind = jgets(in, "kind");
    const char *text = jgets(in, "text");
    int marked = jtruth(jget(in, "marked"));
    if (strcmp(kind, "lifecycle") == 0
        && cc_is_compaction_progress_status(text, marked))
        return json_string("compacting");
    return json_string(kind);
}

#define CCFLOW_STEER_FALLBACK_OPEN "[OUT-OF-BAND USER MESSAGE"
#define CCFLOW_STEER_FALLBACK_CLOSE "[/OUT-OF-BAND USER MESSAGE]"

/* _steer_markers: prompt_builder constants when the import succeeds, else the
 * stable fallback literals. SEAM: the import (scripted open/close or error). */
/* PoP: ccflow_steer_markers @ agent/conversation_compression.py:_steer_markers */
json_t *ccflow_steer_markers(const json_t *in)
{
    json_t *out = json_array();
    if (jtruth(jget(in, "import_error"))) {
        json_append(out, json_string(CCFLOW_STEER_FALLBACK_OPEN));
        json_append(out, json_string(CCFLOW_STEER_FALLBACK_CLOSE));
        return out;
    }
    json_append(out, json_string(jgets(in, "open")));
    json_append(out, json_string(jgets(in, "close")));
    return out;
}

/* Borrowed open/close markers from the injected _steer_markers seam result. */
static void markers_from_in(const json_t *in, const char **open, const char **close)
{
    const json_t *m = jget(in, "markers");
    *open = CCFLOW_STEER_FALLBACK_OPEN;
    *close = CCFLOW_STEER_FALLBACK_CLOSE;
    if (m && m->type == JSON_ARRAY && m->c.count >= 2) {
        const char *o = jstrv(m->c.items[0]);
        const char *c = jstrv(m->c.items[1]);
        if (o) *open = o;
        if (c) *close = c;
    }
}

/* _message_contains_busy_steer: both markers present in the message text. */
/* PoP: ccflow_message_contains_busy_steer @ agent/conversation_compression.py:_message_contains_busy_steer */
json_t *ccflow_message_contains_busy_steer(const json_t *in)
{
    char *text = msg_text(jget(in, "message"));
    int hit = 0;
    if (text && text[0]) {
        const char *open, *close;
        markers_from_in(in, &open, &close);
        hit = strstr(text, open) != NULL && strstr(text, close) != NULL;
    }
    free(text);
    return json_bool(hit != 0);
}

/* _extract_steer_text_from_message: inner text between markers, stripped, or
 * None. Fallback open: skip to end of the opening line; fallback close used
 * when the primary close is absent. */
/* PoP: ccflow_extract_steer_text_from_message @ agent/conversation_compression.py:_extract_steer_text_from_message */
json_t *ccflow_extract_steer_text_from_message(const json_t *in)
{
    char *text = msg_text(jget(in, "message"));
    if (!text || !text[0]) { free(text); return json_null(); }
    const char *open, *close;
    markers_from_in(in, &open, &close);
    const char *start = strstr(text, open);
    if (!start) {
        start = strstr(text, CCFLOW_STEER_FALLBACK_OPEN);
        if (!start) { free(text); return json_null(); }
        const char *nl = strchr(start, '\n');
        start = nl ? nl + 1 : start + strlen(CCFLOW_STEER_FALLBACK_OPEN);
    } else {
        start += strlen(open);
    }
    const char *end = strstr(start, close);
    if (!end) {
        end = strstr(start, CCFLOW_STEER_FALLBACK_CLOSE);
        if (!end) { free(text); return json_null(); }
    }
    size_t n = (size_t)(end - start);
    char *inner = malloc(n + 1);
    if (!inner) { free(text); return json_null(); }
    memcpy(inner, start, n);
    inner[n] = '\0';
    char *stripped = py_strip(inner);
    free(inner);
    free(text);
    json_t *out = (stripped && stripped[0]) ? json_string(stripped) : json_null();
    free(stripped);
    return out;
}

/* _compressed_has_busy_steer: any role=tool dict row carrying both markers. */
/* PoP: ccflow_compressed_has_busy_steer @ agent/conversation_compression.py:_compressed_has_busy_steer */
json_t *ccflow_compressed_has_busy_steer(const json_t *in)
{
    const json_t *msgs = jget(in, "messages");
    const char *open, *close;
    markers_from_in(in, &open, &close);
    if (msgs && msgs->type == JSON_ARRAY) {
        for (size_t i = 0; i < msgs->c.count; i++) {
            const json_t *m = msgs->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            if (strcmp(jgets(m, "role"), "tool") != 0) continue;
            char *text = msg_text(m);
            int hit = text && text[0] && strstr(text, open) && strstr(text, close);
            free(text);
            if (hit) return json_bool(true);
        }
    }
    return json_bool(false);
}

/* _messages_match_scoped_identity: role+content equal; timestamps equal when
 * BOTH present, else True. */
/* PoP: ccflow_messages_match_scoped_identity @ agent/conversation_compression.py:_messages_match_scoped_identity */
json_t *ccflow_messages_match_scoped_identity(const json_t *in)
{
    const json_t *l = jget(in, "left"), *r = jget(in, "right");
    if (!l || l->type != JSON_OBJECT || !r || r->type != JSON_OBJECT)
        return json_bool(false);
    if (!jequal(jget(l, "role"), jget(r, "role"))) return json_bool(false);
    if (!jequal(jget(l, "content"), jget(r, "content"))) return json_bool(false);
    const json_t *lt = jget(l, "timestamp"), *rt = jget(r, "timestamp");
    if (lt && lt->type != JSON_NULL && rt && rt->type != JSON_NULL)
        return json_bool(jequal(lt, rt) != 0);
    return json_bool(true);
}

#define CCFLOW_DB_MARKER "_db_persisted"

static int scoped_identity_match(const json_t *target, const json_t *source)
{
    json_t *pair = json_object();
    json_set(pair, "left", json_copy(target));
    json_set(pair, "right", json_copy(source));
    json_t *res = ccflow_messages_match_scoped_identity(pair);
    int hit = res && res->type == JSON_BOOL && res->bool_val;
    json_free(pair);
    json_free(res);
    return hit;
}

/* _stamp_scoped_twins: stamp _db_persisted on unstamped scoped twins of
 * source. Exact-timestamp pass first; an exact hit (stamped twin counts only
 * with exact_counts_stamped) suppresses the broad pass. */
/* PoP: ccflow_stamp_scoped_twins @ agent/conversation_compression.py:_stamp_scoped_twins */
json_t *ccflow_stamp_scoped_twins(const json_t *in)
{
    json_t *targets = json_copy(jget(in, "targets"));
    const json_t *source = jget(in, "source");
    int exact_counts = jtruth(jget(in, "exact_counts_stamped"));
    if (!targets || targets->type != JSON_ARRAY) {
        json_free(targets);
        return json_array();
    }
    if (!source || source->type != JSON_OBJECT) return targets;
    const json_t *sts = jget(source, "timestamp");
    int exact_hit = 0;
    if (sts && sts->type != JSON_NULL) {
        for (size_t i = 0; i < targets->c.count; i++) {
            json_t *t = targets->c.items[i];
            if (!t || t->type != JSON_OBJECT) continue;
            if (!jequal(jget(t, "timestamp"), sts)) continue;
            if (!scoped_identity_match(t, source)) continue;
            if (jtruth(jget(t, CCFLOW_DB_MARKER))) {
                exact_hit = exact_hit || exact_counts;
                continue;
            }
            json_set(t, CCFLOW_DB_MARKER, json_bool(true));
            exact_hit = 1;
        }
        if (exact_hit) return targets;
    }
    for (size_t i = 0; i < targets->c.count; i++) {
        json_t *t = targets->c.items[i];
        if (!t || t->type != JSON_OBJECT) continue;
        if (jtruth(jget(t, CCFLOW_DB_MARKER))) continue;
        if (scoped_identity_match(t, source))
            json_set(t, CCFLOW_DB_MARKER, json_bool(true));
    }
    return targets;
}

/* _todo_snapshot_is_only_content: str/str -> stripped empty after strip();
 * list/list -> stripped empty; mixed -> False. */
/* PoP: ccflow_todo_snapshot_is_only_content @ agent/conversation_compression.py:_todo_snapshot_is_only_content */
json_t *ccflow_todo_snapshot_is_only_content(const json_t *in)
{
    const json_t *content = jget(in, "content");
    const json_t *stripped = jget(in, "stripped");
    if (content && stripped && content->type == JSON_STRING
        && stripped->type == JSON_STRING) {
        char *s = py_strip(stripped->str_val ? stripped->str_val : "");
        int only = s && !s[0];
        free(s);
        return json_bool(only != 0);
    }
    if (content && stripped && content->type == JSON_ARRAY
        && stripped->type == JSON_ARRAY)
        return json_bool(stripped->c.count == 0);
    return json_bool(false);
}

#define CCFLOW_SUMMARY_META_KEY "_compressed_summary"

/* _direct_messages_for_pre_compress_memory: user/assistant evidence rows;
 * summary-metadata rows dropped; assistant tool-call wrappers kept only with
 * prose, and then with tool_calls stripped. */
/* PoP: ccflow_direct_messages_for_pre_compress_memory @ agent/conversation_compression.py:_direct_messages_for_pre_compress_memory */
json_t *ccflow_direct_messages_for_pre_compress_memory(const json_t *in)
{
    json_t *out = json_array();
    const json_t *msgs = jget(in, "messages");
    if (!msgs || msgs->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < msgs->c.count; i++) {
        const json_t *m = msgs->c.items[i];
        if (!m || m->type != JSON_OBJECT) continue;
        const char *role = jgets(m, "role");
        if (strcmp(role, "user") != 0 && strcmp(role, "assistant") != 0) continue;
        if (jtruth(jget(m, CCFLOW_SUMMARY_META_KEY))) continue;
        if (strcmp(role, "assistant") == 0 && jtruth(jget(m, "tool_calls"))) {
            const json_t *content = jget(m, "content");
            int has_prose;
            if (content && content->type == JSON_STRING) {
                char *s = py_strip(content->str_val ? content->str_val : "");
                has_prose = s && s[0];
                free(s);
            } else {
                has_prose = jtruth(content);
            }
            if (!has_prose) continue;
            json_t *copy = json_copy(m);
            json_obj_del(copy, "tool_calls");
            json_append(out, copy);
            continue;
        }
        json_append(out, json_copy(m));
    }
    return out;
}

/* _prior_timeout_failures: type-pinned int read (bool is NOT an int here). */
/* PoP: ccflow_prior_timeout_failures @ agent/conversation_compression.py:_prior_timeout_failures */
json_t *ccflow_prior_timeout_failures(const json_t *in)
{
    const char *kind = jgets(in, "kind");
    if (strcmp(kind, "int") == 0)
        return json_number((double)(long long)json_get_num(in, "count", 0));
    return json_number(0);
}

/* request_exceeds_model_window: None when either side is unknown; bool is
 * rejected on BOTH sides; window must be a positive int. */
/* PoP: ccflow_request_exceeds_model_window @ agent/conversation_compression.py:request_exceeds_model_window */
json_t *ccflow_request_exceeds_model_window(const json_t *in)
{
    const char *wk = jgets(in, "window_kind");
    const char *tk = jgets(in, "tokens_kind");
    if (strcmp(wk, "int") != 0) return json_null();
    double window = json_get_num(in, "window", 0);
    if (window <= 0) return json_null();
    if (strcmp(tk, "int") != 0) return json_null();
    double tokens = json_get_num(in, "tokens", 0);
    return json_bool(tokens > window);
}

/* _stall_source_fingerprint: "msgs=N:tokens=T:model=M"; model is the first
 * truthy of compressor.summary_model / agent.model / ""; tokens int(or 0)
 * with TypeError/ValueError suppressed. */
/* PoP: ccflow_stall_source_fingerprint @ agent/conversation_compression.py:_stall_source_fingerprint */
json_t *ccflow_stall_source_fingerprint(const json_t *in)
{
    const char *model = jgets(in, "summary_model");
    if (!model[0]) model = jgets(in, "agent_model");
    const json_t *msgs = jget(in, "messages");
    long long n = (msgs && msgs->type == JSON_ARRAY) ? (long long)msgs->c.count : 0;
    long long tokens = 0;
    const json_t *approx = jget(in, "approx_tokens");
    if (jtruth(approx)) {
        long long v;
        if (py_int_coerce(approx, jgets(in, "tokens_kind"), &v)) tokens = v;
    }
    char *out = NULL;
    if (asprintf(&out, "msgs=%lld:tokens=%lld:model=%s", n, tokens, model) < 0)
        return json_string("");
    json_t *res = json_string(out);
    free(out);
    return res;
}

/* _restore_prune_rearm_tokens: restore ONLY the two prune-runway keys that
 * exist in the snapshot. */
/* PoP: ccflow_restore_prune_rearm_tokens @ agent/conversation_compression.py:_restore_prune_rearm_tokens */
json_t *ccflow_restore_prune_rearm_tokens(const json_t *in)
{
    json_t *state = json_copy(jget(in, "state"));
    if (!state || state->type != JSON_OBJECT) {
        json_free(state);
        return json_object();
    }
    const json_t *snap = jget(in, "snapshot");
    static const char *keys[] = { "_proactive_prune_rearm_tokens",
                                  "_proactive_prune_cold_credit" };
    for (int i = 0; i < 2; i++) {
        const json_t *v = jget(snap, keys[i]);
        if (v) json_set(state, keys[i], json_copy(v));
    }
    return state;
}

/* _existing_system_prompt: cached prompt when truthy, else the build seam.
 * SEAM: agent._build_system_prompt(system_message) -> scripted build_result. */
/* PoP: ccflow_existing_system_prompt @ agent/conversation_compression.py:_existing_system_prompt */
json_t *ccflow_existing_system_prompt(const json_t *in)
{
    const json_t *cached = jget(in, "cached");
    if (jtruth(cached)) return json_copy(cached);
    return json_string(jgets(in, "build_result"));
}

/* _configured_compression_context_length: live config read first; on read
 * failure the agent-start capture; int() coercion failures -> None.
 * SEAM: _get_auxiliary_task_config("compression")["context_length"]. */
/* PoP: ccflow_configured_compression_context_length @ agent/conversation_compression.py:_configured_compression_context_length */
json_t *ccflow_configured_compression_context_length(const json_t *in)
{
    const json_t *raw;
    if (jtruth(jget(in, "config_error"))) {
        raw = jget(in, "fallback");
        if (!raw || raw->type == JSON_NULL) return json_null();
    } else {
        raw = jget(in, "config_raw");
        if (!raw || raw->type == JSON_NULL) return json_null();
    }
    long long v;
    const char *kind = jgets(in, "raw_kind");
    if (!kind[0]) kind = jtruth(jget(in, "config_error")) ? jgets(in, "fallback_kind") : "int";
    if (!py_int_coerce(raw, kind, &v)) return json_null();
    return json_number((double)v);
}

/* context_engine_persistence: {"persistence": p} iff the hook accepts the
 * keyword. SEAM: accepts_keyword_argument(hook, "persistence"). */
/* PoP: ccflow_context_engine_persistence @ agent/conversation_compression.py:context_engine_persistence */
json_t *ccflow_context_engine_persistence(const json_t *in)
{
    json_t *out = json_object();
    if (jtruth(jget(in, "accepts")))
        json_set(out, "persistence", json_copy(jget(in, "persistence")));
    return out;
}

/* _restore_messages_snapshot: put the deep snapshot back iff it drifted. */
/* PoP: ccflow_restore_messages_snapshot @ agent/conversation_compression.py:_restore_messages_snapshot */
json_t *ccflow_restore_messages_snapshot(const json_t *in)
{
    const json_t *msgs = jget(in, "messages");
    const json_t *snap = jget(in, "snapshot");
    json_t *out = json_object();
    int restored = 0;
    json_t *result = msgs ? json_copy(msgs) : json_array();
    if (snap && snap->type != JSON_NULL && !jequal(msgs, snap)) {
        json_free(result);
        result = json_copy(snap);
        restored = 1;
    }
    set_bool(out, "restored", restored);
    json_set(out, "messages", result);
    return out;
}

#define CCFLOW_TODO_HEADER "[Your active task list was preserved across context compression]"
#define CCFLOW_PRUNED_HEADER \
    "[Skills pruned during compression \xe2\x80\x94 reload before acting on these tasks]"

/* _pruned_skill_reload_notice: first-seen dedup of pruned-skill names across
 * the compressed transcript, capped, then the reload instruction.
 * SEAM: _extract_pruned_skill_names per message (scripted `extracted` lists,
 * one per dict row) and _MAX_PRUNED_SKILL_MARKERS (`max_markers`). */
/* PoP: ccflow_pruned_skill_reload_notice @ agent/conversation_compression.py:_pruned_skill_reload_notice */
json_t *ccflow_pruned_skill_reload_notice(const json_t *in)
{
    const json_t *compressed = jget(in, "compressed");
    const json_t *extracted = jget(in, "extracted");
    long long max_markers = (long long)json_get_num(in, "max_markers", 20);
    char **names = NULL;
    size_t n_names = 0, cap_names = 0;
    size_t row = 0;
    if (compressed && compressed->type == JSON_ARRAY) {
        for (size_t i = 0; i < compressed->c.count; i++) {
            const json_t *m = compressed->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            const json_t *hits = (extracted && extracted->type == JSON_ARRAY)
                                     ? json_get(extracted, row) : NULL;
            row++;
            if (!hits || hits->type != JSON_ARRAY) continue;
            for (size_t h = 0; h < hits->c.count; h++) {
                const char *name = jstrv(hits->c.items[h]);
                if (!name) continue;
                int seen = 0;
                for (size_t q = 0; q < n_names; q++)
                    if (strcmp(names[q], name) == 0) { seen = 1; break; }
                if (seen) continue;
                if ((long long)n_names >= max_markers) continue;
                if (n_names == cap_names) {
                    cap_names = cap_names ? cap_names * 2 : 8;
                    char **nw = realloc(names, cap_names * sizeof(char *));
                    if (!nw) break;
                    names = nw;
                }
                names[n_names++] = strdup(name);
            }
        }
    }
    /* del names[max:] */
    while ((long long)n_names > max_markers) free(names[--n_names]);
    if (n_names == 0) { free(names); return json_string(""); }
    size_t cap = strlen(CCFLOW_PRUNED_HEADER) + 512 + n_names * 64;
    char *out = malloc(cap);
    if (!out) {
        for (size_t q = 0; q < n_names; q++) free(names[q]);
        free(names);
        return json_string("");
    }
    strcpy(out, CCFLOW_PRUNED_HEADER "\n"
           "The task list above crossed the compression boundary verbatim, but "
           "the skill instructions that governed it were pruned. Before "
           "executing any preserved task that depends on these skills, reload "
           "them first: ");
    for (size_t q = 0; q < n_names; q++) {
        if (q) strcat(out, "; ");
        strcat(out, "skill_view(name='");
        strcat(out, names[q]);
        strcat(out, "')");
    }
    strcat(out, ". After reloading, re-check that each pending task is still "
                "justified \xe2\x80\x94 findings recorded before the boundary may have "
                "invalidated it.");
    for (size_t q = 0; q < n_names; q++) free(names[q]);
    free(names);
    json_t *res = json_string(out);
    free(out);
    return res;
}
/* ===========================================================================
 * Attempt ownership + timeout-outcome state machines.
 * State is a json_t object mirroring vars(compressor)/vars(agent); locks are
 * modeled as boolean flags (single-threaded harness), threading.local as
 * {"__local__":true, ...attrs}. Shape: {"seq":[...],"state":{...},"ret":...}.
 * =========================================================================== */

static json_t *state_result(json_t *seq, json_t *state, json_t *ret)
{
    json_t *out = json_object();
    json_set(out, "seq", seq ? seq : json_array());
    json_set(out, "state", state ? state : json_object());
    json_set(out, "ret", ret ? ret : json_null());
    return out;
}

static json_t *copy_state(const json_t *in, const char *key)
{
    json_t *s = json_copy(jget(in, key));
    if (!s || s->type != JSON_OBJECT) {
        json_free(s);
        s = json_object();
    }
    return s;
}

/* _compressor_attempt_serial_lock: ensure the per-compressor serial lock
 * attribute; a frozen compressor gets a no-op nullcontext. The process-wide
 * claim lock and the attribute write are internal (state-flag model). */
/* PoP: ccflow_compressor_attempt_serial_lock @ agent/conversation_compression.py:_compressor_attempt_serial_lock */
json_t *ccflow_compressor_attempt_serial_lock(const json_t *in)
{
    json_t *state = copy_state(in, "state");
    int frozen = jtruth(jget(in, "frozen"));
    json_t *ret;
    if (jget(state, "_compression_attempt_serial_lock")) {
        ret = json_string("lock");
    } else if (frozen) {
        ret = json_string("nullcontext");
    } else {
        json_set(state, "_compression_attempt_serial_lock", json_bool(true));
        ret = json_string("lock");
    }
    return state_result(NULL, state, ret);
}

/* _claim_compressor_attempt: monotonic generation claim; frozen -> gen 0
 * disables the guard (attribute never written). */
/* PoP: ccflow_claim_compressor_attempt @ agent/conversation_compression.py:_claim_compressor_attempt */
json_t *ccflow_claim_compressor_attempt(const json_t *in)
{
    json_t *state = copy_state(in, "state");
    int frozen = jtruth(jget(in, "frozen"));
    /* the claim takes the serial lock first (ensured, not observed) */
    if (!jget(state, "_compression_attempt_serial_lock") && !frozen)
        json_set(state, "_compression_attempt_serial_lock", json_bool(true));
    long long gen = 0;
    const json_t *cur = jget(state, "_compression_attempt_generation");
    if (jtruth(cur)) gen = (long long)cur->num_val;
    gen += 1;
    if (frozen)
        return state_result(NULL, state, json_number(0));
    json_set(state, "_compression_attempt_generation", json_number((double)gen));
    return state_result(NULL, state, json_number((double)gen));
}

/* _compressor_attempt_is_current: gen 0 (guard disabled) is always current. */
/* PoP: ccflow_compressor_attempt_is_current @ agent/conversation_compression.py:_compressor_attempt_is_current */
json_t *ccflow_compressor_attempt_is_current(const json_t *in)
{
    const json_t *state = jget(in, "state");
    double generation = json_get_num(in, "generation", 0);
    int current;
    if (generation == 0) {
        current = 1;
    } else {
        const json_t *cur = jget(state, "_compression_attempt_generation");
        long long entry = jtruth(cur) ? (long long)cur->num_val : 0;
        current = (double)entry == generation;
    }
    return state_result(NULL, json_copy(state), json_bool(current != 0));
}

/* _mark_compressor_working_attempt: publish the working generation; frozen
 * compressors keep the entry-generation fallback (write suppressed). */
/* PoP: ccflow_mark_compressor_working_attempt @ agent/conversation_compression.py:_mark_compressor_working_attempt */
json_t *ccflow_mark_compressor_working_attempt(const json_t *in)
{
    json_t *state = copy_state(in, "state");
    if (!jtruth(jget(in, "frozen")))
        json_set(state, "_compression_working_attempt_generation",
                 json_number(json_get_num(in, "generation", 0)));
    return state_result(NULL, state, NULL);
}

/* _working_attempt_is_current: without a published marker fall back to the
 * entry-generation ownership check; an unclaimed compressor is current. */
/* PoP: ccflow_working_attempt_is_current @ agent/conversation_compression.py:_working_attempt_is_current */
json_t *ccflow_working_attempt_is_current(const json_t *in)
{
    const json_t *state = jget(in, "state");
    double generation = json_get_num(in, "generation", 0);
    int current;
    if (generation == 0) {
        current = 1;
    } else {
        const json_t *marker = jget(state, "_compression_working_attempt_generation");
        if (!marker || marker->type == JSON_NULL) {
            const json_t *cur = jget(state, "_compression_attempt_generation");
            long long entry = jtruth(cur) ? (long long)cur->num_val : 0;
            current = entry == 0 || (double)entry == generation;
        } else {
            current = (long long)marker->num_val == (long long)generation;
        }
    }
    return state_result(NULL, json_copy(state), json_bool(current != 0));
}

/* _caller_attempt_is_current: the calling generation rides the ContextVar
 * (SEAM: injected as ctx_generation; None -> unguarded). */
/* PoP: ccflow_caller_attempt_is_current @ agent/conversation_compression.py:_caller_attempt_is_current */
json_t *ccflow_caller_attempt_is_current(const json_t *in)
{
    json_t *fwd = json_object();
    json_set(fwd, "state", json_copy(jget(in, "state")));
    const json_t *ctx = jget(in, "ctx_generation");
    json_set(fwd, "generation",
             (ctx && ctx->type == JSON_NUMBER) ? json_copy(ctx) : json_number(0));
    json_t *res = ccflow_working_attempt_is_current(fwd);
    json_free(fwd);
    return res;
}

/* _raise_if_stale_attempt: unwind the calling attempt as a cancellation when
 * a newer working attempt owns the compressor. */
/* PoP: ccflow_raise_if_stale_attempt @ agent/conversation_compression.py:_raise_if_stale_attempt */
json_t *ccflow_raise_if_stale_attempt(const json_t *in)
{
    json_t *cur = ccflow_caller_attempt_is_current(in);
    json_t *ret = jget(cur, "ret");
    int current = ret && ret->type == JSON_BOOL && ret->bool_val;
    json_t *out = state_result(NULL, json_copy(jget(in, "state")),
                               current ? NULL : json_string("AuxiliaryExplicitCancellation"));
    json_free(cur);
    return out;
}

/* _install_compression_cancelled_check: stamp the F4 consult with its owner
 * generation (both writes suppressed on a frozen compressor). */
/* PoP: ccflow_install_compression_cancelled_check @ agent/conversation_compression.py:_install_compression_cancelled_check */
json_t *ccflow_install_compression_cancelled_check(const json_t *in)
{
    json_t *state = copy_state(in, "state");
    if (!jtruth(jget(in, "frozen"))) {
        json_set(state, "_compression_cancelled_check", json_copy(jget(in, "check")));
        json_set(state, "_compression_cancelled_check_owner",
                 json_number(json_get_num(in, "generation", 0)));
    }
    return state_result(NULL, state, NULL);
}

/* _clear_compression_cancelled_check_if_owner: a different owning generation
 * keeps its callback; gen 0 / no owner clears. Returns True when cleared. */
/* PoP: ccflow_clear_compression_cancelled_check_if_owner @ agent/conversation_compression.py:_clear_compression_cancelled_check_if_owner */
json_t *ccflow_clear_compression_cancelled_check_if_owner(const json_t *in)
{
    json_t *state = copy_state(in, "state");
    double generation = json_get_num(in, "generation", 0);
    const json_t *owner = jget(state, "_compression_cancelled_check_owner");
    int has_owner = owner && owner->type != JSON_NULL;
    if (has_owner && generation != 0 && owner->num_val != generation)
        return state_result(NULL, state, json_bool(false));
    if (!jtruth(jget(in, "frozen"))) {
        json_set(state, "_compression_cancelled_check", json_null());
        json_set(state, "_compression_cancelled_check_owner", json_null());
    }
    return state_result(NULL, state, json_bool(true));
}

/* _get_context_compression_timeout_state: stable lock + thread-local state.
 * vars() failure (no_vars) -> None. create replaces a non-local state. */
/* PoP: ccflow_get_context_compression_timeout_state @ agent/conversation_compression.py:_get_context_compression_timeout_state */
json_t *ccflow_get_context_compression_timeout_state(const json_t *in)
{
    json_t *agent = copy_state(in, "agent");
    int create = jtruth(jget(in, "create"));
    if (jtruth(jget(in, "no_vars"))) {
        json_free(agent);
        return state_result(NULL, NULL, NULL);
    }
    int lock_created = 0;
    if (!jget(agent, "_context_compression_timeout_state_lock")) {
        json_set(agent, "_context_compression_timeout_state_lock", json_bool(true));
        lock_created = 1;
    }
    json_t *seq = json_array();
    if (lock_created) push(seq, act("lock_created"));
    json_t *state = jget(agent, "_context_compression_timeout_state");
    int is_local = state && state->type == JSON_OBJECT
                   && jtruth(jget(state, "__local__"));
    if (create && !is_local) {
        json_t *local = json_object();
        json_set(local, "__local__", json_bool(true));
        json_set(agent, "_context_compression_timeout_state", local);
        state = jget(agent, "_context_compression_timeout_state");
        is_local = 1;
        push(seq, act("local_created"));
    }
    json_t *ret = json_object();
    set_bool(ret, "lock", 1);
    json_set(ret, "state", is_local ? json_copy(state) : json_null());
    return state_result(seq, agent, ret);
}

/* _set_context_compression_timeout_outcome: thread-local write + the
 * authoritative agent mirror; minimal doubles get the mirror only. */
/* PoP: ccflow_set_context_compression_timeout_outcome @ agent/conversation_compression.py:_set_context_compression_timeout_outcome */
json_t *ccflow_set_context_compression_timeout_outcome(const json_t *in)
{
    json_t *agent = copy_state(in, "agent");
    int timed_out = jtruth(jget(in, "timed_out"));
    json_t *seq = json_array();
    json_t *state = NULL;
    if (!jtruth(jget(in, "no_vars"))) {
        if (!jget(agent, "_context_compression_timeout_state_lock"))
            json_set(agent, "_context_compression_timeout_state_lock", json_bool(true));
        state = jget(agent, "_context_compression_timeout_state");
        int is_local = state && state->type == JSON_OBJECT
                       && jtruth(jget(state, "__local__"));
        if (!is_local) {
            json_t *local = json_object();
            json_set(local, "__local__", json_bool(true));
            json_set(agent, "_context_compression_timeout_state", local);
            state = jget(agent, "_context_compression_timeout_state");
            push(seq, act("local_created"));
        }
        json_set(state, "timed_out", json_bool(timed_out != 0));
    }
    json_set(agent, "_last_compression_timed_out", json_bool(timed_out != 0));
    return state_result(seq, agent, NULL);
}

/* reset_context_compression_timeout_outcome: clear this thread's outcome. */
/* PoP: ccflow_reset_context_compression_timeout_outcome @ agent/conversation_compression.py:reset_context_compression_timeout_outcome */
json_t *ccflow_reset_context_compression_timeout_outcome(const json_t *in)
{
    json_t *fwd = json_copy(in);
    json_set(fwd, "timed_out", json_bool(false));
    json_t *res = ccflow_set_context_compression_timeout_outcome(fwd);
    json_free(fwd);
    return res;
}

/* mark_context_compression_timed_out: mark this thread's owned compression. */
/* PoP: ccflow_mark_context_compression_timed_out @ agent/conversation_compression.py:mark_context_compression_timed_out */
json_t *ccflow_mark_context_compression_timed_out(const json_t *in)
{
    json_t *fwd = json_copy(in);
    json_set(fwd, "timed_out", json_bool(true));
    json_t *res = ccflow_set_context_compression_timeout_outcome(fwd);
    json_free(fwd);
    return res;
}

/* _mark_compression_blocked_transient: cooldown:* / structural_backoff:* are
 * transient and publish the signal; ineffective stays unmarked. */
/* PoP: ccflow_mark_compression_blocked_transient @ agent/conversation_compression.py:_mark_compression_blocked_transient */
json_t *ccflow_mark_compression_blocked_transient(const json_t *in)
{
    json_t *agent = copy_state(in, "agent");
    const char *reason = jgets(in, "reason");
    json_t *seq = json_array();
    if (strncmp(reason, "cooldown", 8) == 0
        || strncmp(reason, "structural_backoff", 18) == 0) {
        if (!jtruth(jget(in, "frozen")))
            json_set(agent, "_compression_blocked_transient", json_string(reason));
    }
    return state_result(seq, agent, NULL);
}
/* ===========================================================================
 * Small flow cores. Every seam call is recorded; the final {"a":"ret",...}
 * event carries the return value (and mutated state where the live function
 * writes its arguments).
 * =========================================================================== */

static void ret_event(json_t *log, json_t *ret)
{
    json_t *r = act("ret");
    if (ret) {
        for (size_t i = 0; i < ret->c.count; i++)
            json_set(r, ret->c.keys[i], json_copy(ret->c.items[i]));
        json_free(ret);
    }
    push(log, r);
}

/* _swallow: best-effort block; on Exception log at DEBUG and continue.
 * SEAM: the wrapped block's outcome (scripted `raises`). */
/* PoP: ccflow_swallow @ agent/conversation_compression.py:_swallow */
json_t *ccflow_swallow(const json_t *in)
{
    json_t *log = json_array();
    int raises = jtruth(jget(in, "raises"));
    json_t *ret = json_object();
    set_bool(ret, "swallowed", raises);
    ret_event(log, ret);
    return log;
}

/* _emit_feasibility_notice: store + emit once per distinct text.
 * SEAM: agent._emit_warning. */
/* PoP: ccflow_emit_feasibility_notice @ agent/conversation_compression.py:_emit_feasibility_notice */
json_t *ccflow_emit_feasibility_notice(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    const char *msg = jgets(in, "msg");
    const json_t *last = jget(agent, "_last_feasibility_notice");
    int emitted = 0;
    if (!(last && last->type == JSON_STRING && strcmp(last->str_val, msg) == 0)) {
        json_set(agent, "_last_feasibility_notice", json_string(msg));
        json_set(agent, "_compression_warning", json_string(msg));
        json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
        set_str(ew, "msg", msg);
        push(log, ew);
        emitted = 1;
    }
    json_t *ret = json_object();
    set_bool(ret, "emitted", emitted);
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}

/* _warn_summary_or_aux_fallback: surface a failed summary once per error, or
 * a recovered-but-broken aux model once per (model, error).
 * SEAM: agent._emit_warning. */
/* PoP: ccflow_warn_summary_or_aux_fallback @ agent/conversation_compression.py:_warn_summary_or_aux_fallback */
json_t *ccflow_warn_summary_or_aux_fallback(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    const json_t *cc = jget(in, "compressor");
    const char *summary_error = jgets(cc, "_last_summary_error");
    if (summary_error[0]) {
        const json_t *last = jget(agent, "_last_compression_summary_warning");
        if (!(last && last->type == JSON_STRING
              && strcmp(last->str_val, summary_error) == 0)) {
            json_set(agent, "_last_compression_summary_warning",
                     json_string(summary_error));
            char *text = NULL;
            if (asprintf(&text,
                         "\xe2\x9a\xa0 Compression summary failed: %s. "
                         "Inserted a fallback context marker.", summary_error) >= 0) {
                json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
                set_str(ew, "msg", text);
                push(log, ew);
                free(text);
            }
        }
    } else {
        const char *model = jgets(cc, "_last_aux_model_failure_model");
        const char *err = jgets(cc, "_last_aux_model_failure_error");
        if (model[0]) {
            json_t *key = json_array();
            json_append(key, json_string(model));
            json_append(key, err[0] ? json_string(err) : json_null());
            const json_t *last = jget(agent, "_last_aux_fallback_warning_key");
            if (!jequal(last, key)) {
                json_set(agent, "_last_aux_fallback_warning_key", key);
                char *text = NULL;
                if (asprintf(&text,
                             "\xe2\x84\xb9 Configured compression model '%s' failed, so Hermes summarised "
                             "with your main model instead. Check auxiliary.compression.model in your config.",
                             model) >= 0) {
                    json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
                    set_str(ew, "msg", text);
                    push(log, ew);
                    free(text);
                }
            } else {
                json_free(key);
            }
        }
    }
    json_t *ret = json_object();
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}

/* _refresh_main_credential: rotating Anthropic OAuth refresh before summary.
 * SEAM: agent._try_refresh_anthropic_client_credentials (swallowed). */
/* PoP: ccflow_refresh_main_credential @ agent/conversation_compression.py:_refresh_main_credential */
json_t *ccflow_refresh_main_credential(const json_t *in)
{
    json_t *log = json_array();
    if (strcmp(jgets(in, "api_mode"), "anthropic_messages") == 0
        && jtruth(jget(in, "refresh_present"))) {
        int raises = jtruth(jget(in, "refresh_raises"));
        json_t *rc = act("refresh_credentials");
        set_bool(rc, "raised", raises);
        push(log, rc);
    }
    ret_event(log, NULL);
    return log;
}

/* _hand_off_metrics_segment: shared-metrics rotation (swallowed observer).
 * SEAM: relay_shared_metrics.rotate_segment. */
/* PoP: ccflow_hand_off_metrics_segment @ agent/conversation_compression.py:_hand_off_metrics_segment */
json_t *ccflow_hand_off_metrics_segment(const json_t *in)
{
    json_t *log = json_array();
    int raises = jtruth(jget(in, "raises"));
    json_t *rs = act("rotate_segment");
    set_str(rs, "old", jgets(in, "old"));
    set_str(rs, "new", jgets(in, "new"));
    set_bool(rs, "raised", raises);
    push(log, rs);
    ret_event(log, NULL);
    return log;
}

/* _reset_read_dedup_caches: advance file/screenshot/skill-view dedup gens.
 * SEAM: the three reset calls (each independently suppressed). */
/* PoP: ccflow_reset_read_dedup_caches @ agent/conversation_compression.py:_reset_read_dedup_caches */
json_t *ccflow_reset_read_dedup_caches(const json_t *in)
{
    json_t *log = json_array();
    const char *task_id = jgets(in, "task_id");
    const char *session_id = jgets(in, "session_id");
    json_t *f = act("reset_file_dedup");
    set_str(f, "task_id", task_id);
    set_bool(f, "raised", jtruth(jget(in, "file_raises")));
    push(log, f);
    if (session_id[0]) {
        json_t *s = act("reset_screenshot_dedup");
        set_str(s, "session_id", session_id);
        set_bool(s, "raised", jtruth(jget(in, "shot_raises")));
        push(log, s);
    }
    json_t *k = act("reset_skill_view_dedup");
    set_str(k, "task_id", task_id);
    set_bool(k, "raised", jtruth(jget(in, "skill_raises")));
    push(log, k);
    ret_event(log, NULL);
    return log;
}

/* _rebind_session_context: session ContextVar, env fallback, log context.
 * SEAM: gateway.session_context.set_current_session_id +
 *       hermes_logging.set_session_context. */
/* PoP: ccflow_rebind_session_context @ agent/conversation_compression.py:_rebind_session_context */
json_t *ccflow_rebind_session_context(const json_t *in)
{
    json_t *log = json_array();
    const char *sid = jgets(in, "session_id");
    int set_raises = jtruth(jget(in, "set_raises"));
    json_t *sc = act("set_current_session_id");
    set_str(sc, "session_id", sid);
    set_bool(sc, "raised", set_raises);
    push(log, sc);
    if (set_raises) {
        json_t *env = act("env_set");
        set_str(env, "key", "HERMES_SESSION_ID");
        set_str(env, "value", sid);
        push(log, env);
    }
    json_t *lc = act("set_session_context");
    set_str(lc, "session_id", sid);
    set_bool(lc, "raised", jtruth(jget(in, "logging_raises")));
    push(log, lc);
    ret_event(log, NULL);
    return log;
}

/* lowercase ASCII copy (Python str.lower() over the harness's ASCII scope) */
static char *ascii_lower(const char *s)
{
    char *d = strdup(s ? s : "");
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}

/* _aux_inherits_main_route: same model on the same (normalized) endpoint.
 * SEAM: hermes_cli.route_identity.normalize_route_base_url (scripted map). */
/* PoP: ccflow_aux_inherits_main_route @ agent/conversation_compression.py:_aux_inherits_main_route */
json_t *ccflow_aux_inherits_main_route(const json_t *in)
{
    json_t *log = json_array();
    char *aux_m = py_strip(jgets(in, "aux_model"));
    char *main_m = py_strip(jgets(in, "agent_model"));
    char *aux_l = ascii_lower(aux_m);
    char *main_l = ascii_lower(main_m);
    int same_model = strcmp(aux_l, main_l) == 0;
    free(aux_m); free(main_m); free(aux_l); free(main_l);
    int result = 0;
    if (same_model) {
        const char *main_raw = jgets(in, "agent_base_url");
        const json_t *map = jget(in, "normalize_map");
        const char *main_norm = jgets(map, main_raw);
        json_t *n1 = act("normalize");
        set_str(n1, "url", main_raw);
        set_str(n1, "ret", main_norm);
        push(log, n1);
        if (!main_norm[0]) {
            result = 1;
        } else {
            const char *aux_raw = jgets(in, "aux_base_url");
            const char *aux_norm = jgets(map, aux_raw);
            json_t *n2 = act("normalize");
            set_str(n2, "url", aux_raw);
            set_str(n2, "ret", aux_norm);
            push(log, n2);
            result = strcmp(aux_norm, main_norm) == 0;
        }
    }
    json_t *ret = json_object();
    set_bool(ret, "v", result);
    ret_event(log, ret);
    return log;
}

/* _compression_child_source: the parent row's persisted source, else the
 * platform default. SEAM: store/session_db read + session_source_for. */
/* PoP: ccflow_compression_child_source @ agent/conversation_compression.py:_compression_child_source */
json_t *ccflow_compression_child_source(const json_t *in)
{
    json_t *log = json_array();
    int store_present = jtruth(jget(in, "store_present"));
    int read_raises = jtruth(jget(in, "read_raises"));
    const char *source = NULL;
    json_t *rd = act("read_source");
    set_str(rd, "via", store_present ? "store" : "session_db");
    set_bool(rd, "raised", read_raises);
    push(log, rd);
    if (!read_raises)
        source = jgets(in, store_present ? "record_source" : "db_source");
    json_t *ret = json_object();
    if (source && source[0]) {
        set_str(ret, "v", source);
    } else {
        const char *platform = jgets(in, "platform");
        const char *fallback = jgets(in, "source_for");
        json_t *sf = act("session_source_for");
        set_str(sf, "platform", platform);
        set_str(sf, "ret", fallback);
        push(log, sf);
        set_str(ret, "v", fallback);
    }
    ret_event(log, ret);
    return log;
}

/* _held_watermark: the in-place commit cap.
 * SEAM: conversation_compression_watermark.held_archive_watermark. */
/* PoP: ccflow_held_watermark @ agent/conversation_compression.py:_held_watermark */
json_t *ccflow_held_watermark(const json_t *in)
{
    json_t *log = json_array();
    const json_t *msgs = jget(in, "messages");
    const json_t *tail = jget(in, "verbatim_tail");
    json_t *hw = act("held_archive_watermark");
    set_val(hw, "watermark", jget(in, "watermark"));
    set_num(hw, "n_messages", (msgs && msgs->type == JSON_ARRAY) ? (double)msgs->c.count : 0);
    set_bool(hw, "has_tail", jtruth(tail));
    set_val(hw, "ret", jget(in, "seam_result"));
    push(log, hw);
    json_t *ret = json_object();
    set_val(ret, "v", jget(in, "seam_result"));
    ret_event(log, ret);
    return log;
}
/* ===========================================================================
 * Breaker gate + attempt entry + lease acquisition flows.
 * logger.* calls are diagnostic and NOT traced (the oracle disables logging);
 * decision-relevant effects are state writes and seam calls.
 * =========================================================================== */

#define CCFLOW_COMPACTION_STATUS \
    "\xf0\x9f\x97\x9c\xef\xb8\x8f Compacting context \xe2\x80\x94 summarizing earlier conversation so I can continue..."
#define CCFLOW_DONE_STATUS \
    "\xe2\x9c\x93 Context compaction complete \xe2\x80\x94 continuing turn..."

/* _automatic_compression_gate_blocks: refresh durable guards, evaluate the
 * breaker, read the reason, publish the transient signal.
 * SEAMs: compressor guard methods, _automatic_compression_blocked,
 *        _compression_block_reason (scripted presence/results/raises). */
/* PoP: ccflow_automatic_compression_gate_blocks @ agent/conversation_compression.py:_automatic_compression_gate_blocks */
json_t *ccflow_automatic_compression_gate_blocks(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    int bypass = jtruth(jget(in, "bypass_cooldown"));
    int include_cooldown = jhas(in, "include_cooldown") ? jtruth(jget(in, "include_cooldown")) : 1;
    const json_t *guards = jget(in, "guards");
    static const char *order[] = { "get_active_compression_failure_cooldown",
                                   "_load_fallback_compression_streak",
                                   "_load_ineffective_compression_count" };
    for (int i = 0; i < 3; i++) {
        if (i == 0 && !include_cooldown) continue;
        const json_t *g = jget(guards, order[i]);
        if (!g || !jtruth(jget(g, "present"))) continue;
        int raises = jtruth(jget(g, "raises"));
        json_t *gr = act("guard_refresh");
        set_str(gr, "method", order[i]);
        set_bool(gr, "refresh_kwarg", i == 0);
        set_bool(gr, "raised", raises);
        push(log, gr);
    }
    const json_t *blocked = jget(in, "blocked");
    json_t *ret = json_object();
    if (!blocked || !jtruth(jget(blocked, "present"))) {
        set_val(ret, "v", NULL);
        json_set(ret, "state", agent);
        ret_event(log, ret);
        return log;
    }
    int ignore = bypass && jtruth(jget(blocked, "accepts_ignore"));
    int blocked_result = jtruth(jget(blocked, "result"));
    json_t *bc = act("blocked_check");
    set_bool(bc, "ignore_cooldown", ignore);
    set_bool(bc, "result", blocked_result);
    push(log, bc);
    if (!blocked_result) {
        set_val(ret, "v", NULL);
        json_set(ret, "state", agent);
        ret_event(log, ret);
        return log;
    }
    const char *reason = NULL;
    const json_t *rf = jget(in, "reason_fn");
    if (rf && jtruth(jget(rf, "present"))) {
        int reason_ignore = ignore && jtruth(jget(rf, "accepts_ignore"));
        int raises = jtruth(jget(rf, "raises"));
        const char *r = raises ? NULL : jstrv(jget(rf, "result"));
        json_t *br = act("block_reason");
        set_bool(br, "ignore_cooldown", reason_ignore);
        set_bool(br, "raised", raises);
        set_str(br, "ret", r);
        push(log, br);
        reason = r;
    }
    const char *final_reason = (reason && reason[0]) ? reason : "unknown";
    int marked = strncmp(final_reason, "cooldown", 8) == 0
                 || strncmp(final_reason, "structural_backoff", 18) == 0;
    json_t *mt = act("mark_transient");
    set_str(mt, "reason", final_reason);
    set_bool(mt, "marked", marked);
    push(log, mt);
    if (marked && !jtruth(jget(in, "frozen")))
        json_set(agent, "_compression_blocked_transient", json_string(final_reason));
    set_str(ret, "v", final_reason);
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}

/* _begin_compression_attempt: snapshot + claim, per-attempt signal reset,
 * telemetry seed. SEAMs: snapshot, claim generation, clock, uuid, metrics. */
/* PoP: ccflow_begin_compression_attempt @ agent/conversation_compression.py:_begin_compression_attempt */
json_t *ccflow_begin_compression_attempt(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    json_t *comp = copy_state(in, "compressor");
    int force = jtruth(jget(in, "force"));
    int defer = jtruth(jget(in, "defer_notification"));

    json_t *sn = act("snapshot_attempt_state");
    set_val(sn, "ret", jget(in, "snapshot"));
    push(log, sn);

    double generation = json_get_num(in, "generation", 0);
    json_t *cl = act("claim_attempt");
    set_num(cl, "ret", generation);
    push(log, cl);
    if (!jtruth(jget(in, "frozen"))) {
        if (!jget(comp, "_compression_attempt_serial_lock"))
            json_set(comp, "_compression_attempt_serial_lock", json_bool(true));
        json_set(comp, "_compression_attempt_generation", json_number(generation));
    }

    json_t *ret = json_object();
    if (defer && jtruth(jget(in, "pending_present"))) {
        set_str(ret, "raised", "RuntimeError");
        json_set(ret, "state", agent);
        json_set(ret, "compressor", comp);
        ret_event(log, ret);
        return log;
    }
    json_set(agent, "_last_compression_attempt_recorded", json_bool(true));
    json_set(agent, "_last_compression_attempt_in_place", json_null());
    json_set(agent, "_compression_skipped_due_to_lock", json_null());
    json_set(agent, "_compression_blocked_transient", json_null());

    double now = json_get_num(in, "now", 0);
    const char *attempt_id = jgets(in, "attempt_id");
    const char *trigger = jgets(in, "trigger");
    if (!trigger[0]) trigger = force ? "manual" : "auto";
    json_t *seed = json_object();
    set_str(seed, "attempt_id", attempt_id);
    set_str(seed, "session_id", jgets(agent, "session_id"));
    set_str(seed, "trigger_source", trigger);
    set_val(seed, "overflow_reason", jget(in, "overflow_reason"));

    int metrics_raises = jtruth(jget(in, "metrics_raises"));
    json_set(agent, "_compression_attempt_id", json_string(attempt_id));
    const json_t *approx = jget(in, "approx_tokens");
    const json_t *tokens = jtruth(approx) ? approx : jget(comp, "last_prompt_tokens");
    json_t *mb = act("metrics_begin");
    set_str(mb, "trigger", trigger);
    set_val(mb, "tokens", tokens);
    set_bool(mb, "raised", metrics_raises);
    push(log, mb);

    json_t *attempt = json_object();
    set_val(attempt, "snapshot", jget(in, "snapshot"));
    set_num(attempt, "generation", generation);
    set_num(attempt, "started_at", now);
    json_set(attempt, "seed", seed);
    json_set(ret, "attempt", attempt);
    json_set(ret, "state", agent);
    json_set(ret, "compressor", comp);
    ret_event(log, ret);
    return log;
}

/* _announce_compression_start: engine-customisable compacting status.
 * SEAM: automatic_compaction_status_message + agent._emit_status. */
/* PoP: ccflow_announce_compression_start @ agent/conversation_compression.py:_announce_compression_start */
json_t *ccflow_announce_compression_start(const json_t *in)
{
    json_t *log = json_array();
    int force = jtruth(jget(in, "force"));
    const char *status = CCFLOW_COMPACTION_STATUS;
    if (!force) {
        status = jstrv(jget(in, "auto_status"));
        json_t *as = act("auto_status");
        set_str(as, "phase", "compress");
        set_str(as, "ret", status);
        push(log, as);
    }
    int emitted = status && status[0];
    if (emitted) {
        json_t *es = act("emit_status");
        set_str(es, "text", status);
        push(log, es);
    }
    json_t *ret = json_object();
    json_t *lifecycle = json_object();
    set_bool(lifecycle, "status_emitted", emitted);
    set_str(lifecycle, "commit_status", "aborted");
    set_bool(lifecycle, "done_emitted", 0);
    json_set(ret, "lifecycle", lifecycle);
    ret_event(log, ret);
    return log;
}

/* _resolve_lock_api: (try_acquire, lookup_error). (None,None) = no db (fail
 * open); (None,exc) = lookup failed (caller fails closed). */
/* PoP: ccflow_resolve_lock_api @ agent/conversation_compression.py:_resolve_lock_api */
json_t *ccflow_resolve_lock_api(const json_t *in)
{
    json_t *log = json_array();
    int db_present = jtruth(jget(in, "db_present"));
    int store_present = jtruth(jget(in, "store_present"));
    json_t *ret = json_object();
    json_t *ev = act("resolve_lock_api");
    if (!db_present) {
        set_str(ev, "via", "none");
        push(log, ev);
        set_bool(ret, "api_present", 0);
        set_val(ret, "error", NULL);
    } else if (store_present) {
        set_str(ev, "via", "store");
        push(log, ev);
        set_bool(ret, "api_present", 1);
        set_val(ret, "error", NULL);
    } else if (jtruth(jget(in, "lookup_raises"))) {
        set_str(ev, "via", "db");
        push(log, ev);
        set_bool(ret, "api_present", 0);
        set_str(ret, "error", "lookup");
    } else if (jtruth(jget(in, "not_callable"))) {
        set_str(ev, "via", "db");
        push(log, ev);
        set_bool(ret, "api_present", 0);
        set_str(ret, "error", "not_callable");
    } else {
        set_str(ev, "via", "db");
        push(log, ev);
        set_bool(ret, "api_present", 1);
        set_val(ret, "error", NULL);
    }
    ret_event(log, ret);
    return log;
}

/* _release_compression_lock: holder-qualified release through the handle.
 * SEAM: store.coordination / db release_compression_lock. */
/* PoP: ccflow_release_compression_lock @ agent/conversation_compression.py:_release_compression_lock */
json_t *ccflow_release_compression_lock(const json_t *in)
{
    json_t *log = json_array();
    json_t *rl = act("release_compression_lock");
    set_str(rl, "via", jtruth(jget(in, "store_present")) ? "store" : "db");
    set_str(rl, "sid", jgets(in, "sid"));
    set_str(rl, "holder", jgets(in, "holder"));
    set_bool(rl, "raised", jtruth(jget(in, "raises")));
    push(log, rl);
    ret_event(log, NULL);
    return log;
}

/* _try_acquire_durable_lock: acquire + start-watermark capture. Capture is
 * safety-additive (failure never aborts); an acquire that RAISES fails closed
 * with a holder-qualified best-effort release.
 * SEAMs: try_acquire_compression_lock, _active_watermark, fence mark, release. */
/* PoP: ccflow_try_acquire_durable_lock @ agent/conversation_compression.py:_try_acquire_durable_lock */
json_t *ccflow_try_acquire_durable_lock(const json_t *in)
{
    json_t *log = json_array();
    const char *sid = jgets(in, "sid");
    const char *holder = jgets(in, "holder");
    double ttl = json_get_num(in, "ttl", 300.0);
    int fence_present = jtruth(jget(in, "fence_present"));
    const json_t *acq = jget(in, "acquire");
    int acquire_raises = jtruth(jget(acq, "raises"));
    int acquired = !acquire_raises && jtruth(jget(acq, "result"));
    json_t *ta = act("try_acquire");
    set_str(ta, "sid", sid);
    set_str(ta, "holder", holder);
    set_num(ta, "ttl", ttl);
    set_bool(ta, "raised", acquire_raises);
    set_bool(ta, "ret", acquired);
    push(log, ta);

    json_t *lease = json_object();
    json_t *ret = json_object();
    if (acquire_raises) {
        int release_raises = jtruth(jget(in, "release_raises"));
        json_t *rl = act("release_compression_lock");
        set_str(rl, "via", jtruth(jget(in, "store_present")) ? "store" : "db");
        set_str(rl, "sid", sid);
        set_str(rl, "holder", holder);
        set_bool(rl, "raised", release_raises);
        push(log, rl);
        set_val(lease, "holder", NULL);
        set_val(lease, "watermark", jget(in, "watermark_before"));
        set_bool(ret, "v", 0);
    } else {
        set_str(lease, "holder", holder);
        if (acquired) {
            const json_t *wm = jget(in, "watermark");
            int wm_raises = jtruth(jget(wm, "raises"));
            json_t *aw = act("active_watermark");
            set_bool(aw, "raised", wm_raises);
            set_val(aw, "ret", wm_raises ? NULL : jget(wm, "result"));
            push(log, aw);
            if (wm_raises) {
                set_val(lease, "watermark", NULL);
            } else {
                set_val(lease, "watermark", jget(wm, "result"));
                int has_method = !jhas(in, "fence_has_method")
                                 || jtruth(jget(in, "fence_has_method"));
                if (fence_present && has_method)
                    push(log, act("mark_watermark_fenced"));
            }
        } else {
            set_val(lease, "watermark", jget(in, "watermark_before"));
        }
        set_bool(ret, "v", acquired);
    }
    json_set(ret, "lease", lease);
    ret_event(log, ret);
    return log;
}

/* _abort_lease: sit-out return — prompt, aborted telemetry, terminal edge.
 * SEAMs: _existing_system_prompt (iff prompt absent), telemetry, lifecycle
 *        complete -> _emit_compaction_done -> status_callback. */
/* PoP: ccflow_abort_lease @ agent/conversation_compression.py:_abort_lease */
json_t *ccflow_abort_lease(const json_t *in)
{
    json_t *log = json_array();
    const json_t *prompt = jget(in, "prompt");
    const char *prompt_str;
    if (!prompt || prompt->type == JSON_NULL) {
        const json_t *cached = jget(in, "cached");
        int built = !jtruth(cached);
        prompt_str = built ? jgets(in, "build_result")
                           : (cached->type == JSON_STRING ? cached->str_val : "");
        json_t *ep = act("existing_system_prompt");
        set_bool(ep, "built", built);
        set_str(ep, "ret", prompt_str);
        push(log, ep);
    } else {
        prompt_str = jstrv(prompt) ? jstrv(prompt) : "";
    }
    json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
    set_str(tel, "failure_class", jgets(in, "failure_class"));
    set_val(tel, "seed", jget(in, "seed"));
    push(log, tel);

    const json_t *lc = jget(in, "lifecycle");
    int done_emitted = jtruth(jget(lc, "done_emitted"));
    int status_emitted = jtruth(jget(lc, "status_emitted"));
    const char *commit_status = jgets(lc, "commit_status");
    json_t *lc_out = json_copy(lc ? lc : json_object());
    if (!lc_out || lc_out->type != JSON_OBJECT) {
        json_free(lc_out);
        lc_out = json_object();
        set_bool(lc_out, "status_emitted", 0);
        set_str(lc_out, "commit_status", "aborted");
        set_bool(lc_out, "done_emitted", 0);
    }
    if (!done_emitted) {
        json_set(lc_out, "done_emitted", json_bool(true));
        if (status_emitted
            && (strcmp(commit_status, "committed") == 0 || jtruth(jget(in, "force_terminal")))) {
            if (jtruth(jget(in, "status_callback_present"))) {
                json_t *ed = act("emit_compaction_done");
                set_str(ed, "kind", "compacted");
                set_str(ed, "text", CCFLOW_DONE_STATUS);
                set_bool(ed, "raised", jtruth(jget(in, "callback_raises")));
                push(log, ed);
            }
        }
    }
    json_t *ret = json_object();
    json_t *v = json_array();
    json_append(v, json_null());
    json_append(v, json_string(prompt_str));
    json_set(ret, "v", v);
    json_set(ret, "lifecycle", lc_out);
    ret_event(log, ret);
    return log;
}

/* _sit_out_lock_contention: publish the lock-skip signal, warn once per sid,
 * abort the attempt. SEAMs: holder lookup, _emit_warning, _abort_lease leaves. */
/* PoP: ccflow_sit_out_lock_contention @ agent/conversation_compression.py:_sit_out_lock_contention */
json_t *ccflow_sit_out_lock_contention(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    const char *sid = jgets(in, "sid");
    int holder_raises = jtruth(jget(in, "holder_raises"));
    const char *existing = holder_raises ? NULL : jstrv(jget(in, "holder_result"));
    json_t *gh = act("get_lock_holder");
    set_str(gh, "via", jtruth(jget(in, "store_present")) ? "store" : "db");
    set_bool(gh, "raised", holder_raises);
    set_str(gh, "ret", existing);
    push(log, gh);

    json_set(agent, "_compression_skipped_due_to_lock",
             (existing && existing[0]) ? json_string(existing) : json_bool(true));
    const json_t *last_sid = jget(agent, "_last_compression_lock_warning_sid");
    if (!(last_sid && last_sid->type == JSON_STRING && strcmp(last_sid->str_val, sid) == 0)) {
        json_set(agent, "_last_compression_lock_warning_sid", json_string(sid));
        json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
        set_str(ew, "msg",
                "\xe2\x9a\xa0 Skipping concurrent compression \xe2\x80\x94 another path is already "
                "compressing this session. Will retry after it finishes.");
        set_bool(ew, "raised", jtruth(jget(in, "emit_raises")));
        push(log, ew);
    }
    /* _abort_lease with an explicitly computed prompt (no recompute inside) */
    const json_t *cached = jget(in, "cached");
    int built = !jtruth(cached);
    const char *prompt = built ? jgets(in, "build_result")
                               : (cached->type == JSON_STRING ? cached->str_val : "");
    json_t *ep = act("existing_system_prompt");
    set_bool(ep, "built", built);
    set_str(ep, "ret", prompt);
    push(log, ep);
    json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
    set_str(tel, "failure_class", "lock_contended");
    set_val(tel, "seed", jget(in, "seed"));
    push(log, tel);
    json_t *ret = json_object();
    json_t *v = json_array();
    json_append(v, json_null());
    json_append(v, json_string(prompt));
    json_set(ret, "v", v);
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}
/* ===========================================================================
 * Lease acquisition + rotation-adoption + summary-prep flows.
 * =========================================================================== */

/* _acquire_compression_lease: per-session durable lock; (None, prompt) means
 * sit out. Structural lock-API absence fails open; a resolved API that errors
 * fails closed. SEAMs: _resolve_lock_api, holder mint, fence lock-setup gate,
 * _try_acquire_durable_lock, _sit_out_lock_contention, _abort_lease,
 * register_cancelled_lock_release (scripted sub-results). */
/* PoP: ccflow_acquire_compression_lease @ agent/conversation_compression.py:_acquire_compression_lease */
json_t *ccflow_acquire_compression_lease(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    int db_present = jtruth(jget(in, "db_present"));
    int store_present = jtruth(jget(in, "store_present"));
    int fence_present = jtruth(jget(in, "fence_present"));
    const char *sid = jgets(agent, "session_id");

    json_set(agent, "_compression_skipped_due_to_lock", json_null());

    const json_t *api = jget(in, "lock_api");
    int api_present = jtruth(jget(api, "api_present"));
    const char *api_error = jgets(api, "error");
    json_t *rl = act("resolve_lock_api");
    set_bool(rl, "api_present", api_present);
    set_str(rl, "error", api_error[0] ? api_error : NULL);
    push(log, rl);

    /* ttl: float(agent value or 300.0), TypeError/ValueError suppressed */
    double ttl = 300.0;
    const json_t *ttl_raw = jget(in, "ttl_raw");
    if (jtruth(ttl_raw)) {
        long long iv;
        const char *tk = jgets(in, "ttl_kind");
        if (strcmp(tk, "float") == 0 || strcmp(tk, "int") == 0)
            ttl = ttl_raw->num_val;
        else if (py_int_coerce(ttl_raw, tk, &iv))
            ttl = (double)iv;
    }

    json_t *lease = json_object();
    set_bool(lease, "db_present", db_present);
    set_bool(lease, "store_present", store_present);
    set_str(lease, "sid", sid);
    set_num(lease, "ttl", ttl);
    set_val(lease, "holder", NULL);
    set_val(lease, "watermark", NULL);
    int lock_setup_entered = 0;
    int acquired = 0;
    json_t *ret = json_object();

    if (db_present && sid[0]) {
        const char *holder_id = jgets(in, "holder_id");
        json_t *mh = act("mint_holder");
        set_str(mh, "ret", holder_id);
        push(log, mh);
        json_set(lease, "holder", json_string(holder_id));
        if (api_error[0]) {
            json_set(lease, "holder", json_null());
            acquired = 0;
        } else if (!api_present) {
            json_set(lease, "holder", json_null());
            const json_t *last = jget(agent, "_last_compression_lock_error_sid");
            int warned = !(last && last->type == JSON_STRING
                           && strcmp(last->str_val, sid) == 0);
            if (warned)
                json_set(agent, "_last_compression_lock_error_sid", json_string(sid));
            json_t *la = act("lock_api_absent");
            set_bool(la, "warned", warned);
            push(log, la);
            acquired = 1;
        } else {
            int setup_ok = 1;
            if (fence_present) {
                setup_ok = jtruth(jget(in, "begin_lock_setup"));
                json_t *bs = act("begin_lock_setup");
                set_bool(bs, "ret", setup_ok);
                push(log, bs);
            }
            lock_setup_entered = fence_present && setup_ok;
            if (!setup_ok) {
                json_set(agent, "_last_compaction_in_place", json_bool(false));
                const char *prompt = jgets(jget(in, "abort"), "prompt");
                json_t *ab = act("abort_lease");
                set_str(ab, "failure_class", "commit_fence_cancelled");
                set_str(ab, "prompt", prompt);
                push(log, ab);
                json_t *v = json_array();
                json_append(v, json_null());
                json_append(v, json_string(prompt));
                json_set(ret, "v", v);
                json_set(ret, "state", agent);
                json_free(lease);
                ret_event(log, ret);
                return log;
            }
            const json_t *acq = jget(in, "acquire");
            acquired = jtruth(jget(acq, "acquired"));
            int holder_cleared = jtruth(jget(acq, "holder_cleared"));
            json_t *ta = act("try_acquire_durable");
            set_bool(ta, "acquired", acquired);
            set_val(ta, "watermark", jget(acq, "watermark"));
            set_bool(ta, "holder_cleared", holder_cleared);
            push(log, ta);
            if (holder_cleared)
                json_set(lease, "holder", json_null());
            if (acquired)
                json_set(lease, "watermark", json_copy(jget(acq, "watermark")));
        }
        if (!acquired) {
            if (lock_setup_entered && fence_present) {
                json_t *fs = act("finish_lock_setup");
                set_bool(fs, "entered", 1);
                push(log, fs);
            }
            const json_t *so = jget(in, "sit_out");
            const char *prompt = jgets(so, "prompt");
            json_t *se = act("sit_out_lock_contention");
            set_str(se, "prompt", prompt);
            push(log, se);
            json_t *v = json_array();
            json_append(v, json_null());
            json_append(v, json_string(prompt));
            json_set(ret, "v", v);
            json_set(ret, "state", agent);
            json_free(lease);
            ret_event(log, ret);
            return log;
        }
    }
    const json_t *holder = jget(lease, "holder");
    if (holder && holder->type != JSON_NULL) {
        json_set(agent, "_active_compression_lock_holder", json_copy(holder));
        if (fence_present) {
            int requested = jtruth(jget(in, "register_requested"));
            json_t *rr = act("register_cancelled_lock_release");
            set_bool(rr, "requested", requested);
            push(log, rr);
            if (requested) {
                json_set(agent, "_last_compaction_in_place", json_bool(false));
                const json_t *cached = jget(in, "cached");
                int built = !jtruth(cached);
                const char *prompt = built ? jgets(in, "build_result")
                                           : (cached->type == JSON_STRING ? cached->str_val : "");
                json_t *ep = act("existing_system_prompt");
                set_bool(ep, "built", built);
                set_str(ep, "ret", prompt);
                push(log, ep);
                json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
                set_num(tel, "started_at", json_get_num(in, "started_at", 0));
                set_str(tel, "failure_class", "commit_fence_cancelled");
                set_val(tel, "seed", jget(in, "seed"));
                push(log, tel);
                push(log, act("lease_release"));
                json_t *v = json_array();
                json_append(v, json_null());
                json_append(v, json_string(prompt));
                json_set(ret, "v", v);
                json_set(ret, "state", agent);
                json_free(lease);
                ret_event(log, ret);
                return log;
            }
        }
    }
    json_t *v = json_array();
    json_append(v, lease);
    json_append(v, json_null());
    json_set(ret, "v", v);
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}

/* _adopt_if_parent_rotated: sit out (or adopt the live child) when the parent
 * was already rotated. SEAMs: rotation lookup, _adopt_live_compression_child,
 * lease.release, bypassed telemetry, _existing_system_prompt. */
/* PoP: ccflow_adopt_if_parent_rotated @ agent/conversation_compression.py:_adopt_if_parent_rotated */
json_t *ccflow_adopt_if_parent_rotated(const json_t *in)
{
    json_t *log = json_array();
    json_t *ret = json_object();
    int db_present = jtruth(jget(in, "db_present"));
    const char *sid = jgets(in, "sid");
    if (!db_present || !sid[0]) {
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    const json_t *rot = jget(in, "rotated");
    int lookup_raises = jtruth(jget(rot, "raises"));
    int rotated = !lookup_raises && jtruth(jget(rot, "result"));
    json_t *lk = act("session_rotated_lookup");
    set_bool(lk, "raised", lookup_raises);
    set_bool(lk, "ret", rotated);
    push(log, lk);

    const json_t *cached = jget(in, "cached");
    int built = !jtruth(cached);
    const char *prompt = built ? jgets(in, "build_result")
                               : (cached->type == JSON_STRING ? cached->str_val : "");

    if (lookup_raises) {
        push(log, act("lease_release"));
        json_t *tel = act("telemetry_bypassed");
    set_val(tel, "route", NULL);
        set_str(tel, "commit_status", "aborted");
        set_str(tel, "failure_class", "session_ownership_unreadable");
        set_val(tel, "approx_tokens", jget(in, "approx_tokens"));
        set_val(tel, "seed", jget(in, "seed"));
        push(log, tel);
        json_t *ep = act("existing_system_prompt");
        set_bool(ep, "built", built);
        set_str(ep, "ret", prompt);
        push(log, ep);
        json_t *v = json_array();
        json_append(v, json_copy(jget(in, "messages")));
        json_append(v, json_string(prompt));
        json_set(ret, "v", v);
        ret_event(log, ret);
        return log;
    }
    if (!rotated) {
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    const json_t *recovered = jget(in, "recovered");
    json_t *ac = act("adopt_live_child");
    set_val(ac, "ret", recovered);
    push(log, ac);
    push(log, act("lease_release"));
    json_t *tel = act("telemetry_bypassed");
    set_val(tel, "route", NULL);
    set_str(tel, "commit_status", "skipped");
    set_str(tel, "failure_class", "session_ownership_lost");
    set_val(tel, "approx_tokens", jget(in, "approx_tokens"));
    set_val(tel, "seed", jget(in, "seed"));
    push(log, tel);
    json_t *ep = act("existing_system_prompt");
    set_bool(ep, "built", built);
    set_str(ep, "ret", prompt);
    push(log, ep);
    json_t *v = json_array();
    json_append(v, json_copy((recovered && recovered->type != JSON_NULL)
                                 ? recovered : jget(in, "messages")));
    json_append(v, json_string(prompt));
    json_set(ret, "v", v);
    ret_event(log, ret);
    return log;
}

/* _adopt_grown_durable_parent: adopt the durable transcript when it outgrew
 * the in-memory snapshot (rotation only). Flush the live tail first or skip.
 * SEAMs: durable loader (two reads), _flush_messages_to_session_db. */
/* PoP: ccflow_adopt_grown_durable_parent @ agent/conversation_compression.py:_adopt_grown_durable_parent */
json_t *ccflow_adopt_grown_durable_parent(const json_t *in)
{
    json_t *log = json_array();
    json_t *ret = json_object();
    int db_present = jtruth(jget(in, "db_present"));
    const char *sid = jgets(in, "sid");
    const json_t *messages = jget(in, "messages");
    size_t n_msgs = (messages && messages->type == JSON_ARRAY) ? messages->c.count : 0;
    if (!db_present || !sid[0]) {
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    if (!jtruth(jget(in, "store_present")) && !jtruth(jget(in, "loader_present"))) {
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    const json_t *d1 = jget(in, "durable1");
    json_t *l1 = act("durable_load");
    set_bool(l1, "include_row_ids", 0);
    set_val(l1, "ret", d1);
    push(log, l1);
    if (!d1 || d1->type != JSON_ARRAY || d1->c.count <= n_msgs) {
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    /* preflush the un-persisted current-turn tail, else skip adoption */
    const json_t *idxv = jget(in, "preflush_idx");
    int preflush_ok = 1;
    if (idxv && idxv->type == JSON_NUMBER) {
        long long idx = (long long)idxv->num_val;
        if (idx >= 0 && (size_t)idx < n_msgs) {
            const json_t *fl = jget(in, "flush");
            int raises = jtruth(jget(fl, "raises"));
            preflush_ok = !raises && jtruth(jget(fl, "result"));
            json_t *pf = act("preflush");
            set_num(pf, "idx", (double)idx);
            set_bool(pf, "raised", raises);
            set_bool(pf, "ret", preflush_ok);
            push(log, pf);
        }
    }
    if (!preflush_ok) {
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    json_t *d2 = json_copy(jget(in, "durable2"));
    json_t *l2 = act("durable_load");
    set_bool(l2, "include_row_ids", 1);
    set_val(l2, "ret", d2);
    push(log, l2);
    if (!d2 || d2->type != JSON_ARRAY || d2->c.count <= n_msgs) {
        json_free(d2);
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    /* pop int _row_id from every row; advance the watermark to the newest */
    const json_t *wm = jget(in, "watermark");
    int have_wm = wm && wm->type == JSON_NUMBER;
    double new_wm = have_wm ? wm->num_val : 0;
    int any_ids = 0;
    for (size_t i = 0; i < d2->c.count; i++) {
        json_t *m = d2->c.items[i];
        if (!m || m->type != JSON_OBJECT) continue;
        json_t *rid = json_obj_get(m, "_row_id");
        if (rid && rid->type == JSON_NUMBER) {
            any_ids = 1;
            if (rid->num_val > new_wm) new_wm = rid->num_val;
            json_obj_del(m, "_row_id");
        }
    }
    if (have_wm && any_ids)
        json_set(ret, "watermark", json_number(new_wm));
    else
        set_val(ret, "watermark", wm);
    json_set(ret, "v", d2);
    ret_event(log, ret);
    return log;
}

#define CCFLOW_CHECKPOINT_PREFIX \
    "BLOCKED_MISSING_PREREQUISITE: required pre-compress checkpoint unavailable: "
#define CCFLOW_CHECKPOINT_REMEDIATION \
    ". Recover by set compression.checkpoint_required: false, or switch to a memory " \
    "provider that implements checkpoint API v2"

static void checkpoint_raise(json_t *log, json_t *ret, const char *reason, int incapable)
{
    char *msg = NULL;
    if (asprintf(&msg, CCFLOW_CHECKPOINT_PREFIX "%s%s", reason,
                 incapable ? CCFLOW_CHECKPOINT_REMEDIATION : "") < 0)
        return;
    json_t *r = act("checkpoint_refused");
    set_str(r, "raised", "CompressionCheckpointUnavailable");
    set_str(r, "message", msg);
    push(log, r);
    set_str(ret, "raised", "CompressionCheckpointUnavailable");
    set_str(ret, "message", msg);
    free(msg);
}

/* _pre_compress_memory_context: provider insights for the summary; the
 * fail-closed checkpoint gate when required.
 * SEAMs: supports_pre_compress_checkpoint, on_pre_compress,
 *        sanitize_memory_context. */
/* PoP: ccflow_pre_compress_memory_context @ agent/conversation_compression.py:_pre_compress_memory_context */
json_t *ccflow_pre_compress_memory_context(const json_t *in)
{
    json_t *log = json_array();
    json_t *ret = json_object();
    int checkpoint_required = jtruth(jget(in, "checkpoint_required"));
    int mm_present = jtruth(jget(in, "mm_present"));
    const char *memory_context = "";

    /* detached review fork: no memory manager + persist disabled -> the
     * checkpoint requirement follows the session db's presence */
    if (checkpoint_required && jtruth(jget(in, "persist_disabled")) && !mm_present)
        checkpoint_required = jtruth(jget(in, "db_present"));

    const json_t *sanitize = jget(in, "sanitize");
    if (checkpoint_required) {
        if (!mm_present || !jtruth(jget(in, "supports_present"))) {
            checkpoint_raise(log, ret,
                             "no active provider implements checkpoint API v2", 1);
            set_str(ret, "memory_context", "");
            ret_event(log, ret);
            return log;
        }
        const json_t *sup = jget(in, "supports");
        int sup_raises = jtruth(jget(sup, "raises"));
        int compatible = !sup_raises && jtruth(jget(sup, "result"));
        json_t *sc = act("supports_checkpoint");
        set_num(sc, "version", 2);
        set_bool(sc, "raised", sup_raises);
        set_bool(sc, "ret", compatible);
        push(log, sc);
        if (sup_raises) {
            checkpoint_raise(log, ret, "provider capability probe failed", 0);
            set_str(ret, "memory_context", "");
            ret_event(log, ret);
            return log;
        }
        if (!compatible) {
            checkpoint_raise(log, ret,
                             "active provider does not implement checkpoint API v2", 1);
            set_str(ret, "memory_context", "");
            ret_event(log, ret);
            return log;
        }
        const json_t *opc = jget(in, "on_pre_compress");
        int raises = jtruth(jget(opc, "raises"));
        json_t *op = act("on_pre_compress");
        set_bool(op, "require_checkpoint", 1);
        set_bool(op, "raised", raises);
        set_val(op, "ret", raises ? NULL : jget(opc, "result"));
        push(log, op);
        if (raises) {
            checkpoint_raise(log, ret, "provider checkpoint API v2 failed", 0);
            set_str(ret, "memory_context", "");
            ret_event(log, ret);
            return log;
        }
        const json_t *ctx = jget(opc, "result");
        if (ctx && ctx->type == JSON_STRING) {
            memory_context = jgets(sanitize, "result");
            json_t *sn = act("sanitize_memory_context");
            set_str(sn, "ret", memory_context);
            push(log, sn);
        }
    } else if (mm_present) {
        const json_t *opc = jget(in, "on_pre_compress");
        int raises = jtruth(jget(opc, "raises"));
        json_t *op = act("on_pre_compress");
        set_bool(op, "require_checkpoint", 0);
        set_bool(op, "raised", raises);
        set_val(op, "ret", raises ? NULL : jget(opc, "result"));
        push(log, op);
        if (!raises) {
            const json_t *ctx = jget(opc, "result");
            if (ctx && ctx->type == JSON_STRING) {
                memory_context = jgets(sanitize, "result");
                json_t *sn = act("sanitize_memory_context");
                set_str(sn, "ret", memory_context);
                push(log, sn);
            }
        }
    }
    set_str(ret, "memory_context", memory_context);
    set_val(ret, "raised", NULL);
    ret_event(log, ret);
    return log;
}

/* _resolve_compress_call: bind compress() + only the accepted kwargs.
 * SEAM: _supported_compression_kwargs (signature inspection). */
/* PoP: ccflow_resolve_compress_call @ agent/conversation_compression.py:_resolve_compress_call */
json_t *ccflow_resolve_compress_call(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    const json_t *kwargs = jget(in, "kwargs");
    json_t *sk = act("supported_kwargs");
    set_val(sk, "ret", kwargs);
    push(log, sk);
    const char *mc = jgets(in, "memory_context");
    char *stripped = py_strip(mc);
    int mc_nonempty = stripped && stripped[0];
    free(stripped);
    int accepts_mc = kwargs && kwargs->type == JSON_OBJECT
                     && json_obj_get(kwargs, "memory_context") != NULL;
    if (mc_nonempty && !accepts_mc) {
        const char *engine = jgets(in, "engine_name");
        const json_t *last = jget(agent, "_last_memory_context_unsupported_engine");
        int warned = !(last && last->type == JSON_STRING
                       && strcmp(last->str_val, engine) == 0);
        if (warned)
            json_set(agent, "_last_memory_context_unsupported_engine", json_string(engine));
        json_t *mu = act("memory_context_unsupported");
        set_str(mu, "engine", engine);
        set_bool(mu, "warned", warned);
        push(log, mu);
    }
    json_t *ret = json_object();
    set_val(ret, "kwargs", kwargs);
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}

/* _run_summary_dispatch: the compressor under the fence's progress hook,
 * deadline and interrupt guard. SEAMs: compress_fn (the LLM summary call),
 * hard-cancel event; fence cancelled-check install/clear are state writes. */
/* PoP: ccflow_run_summary_dispatch @ agent/conversation_compression.py:_run_summary_dispatch */
json_t *ccflow_run_summary_dispatch(const json_t *in)
{
    json_t *log = json_array();
    json_t *comp = copy_state(in, "compressor");
    int fence_present = jtruth(jget(in, "fence_present"));
    int fence_cancelled = jtruth(jget(in, "fence_cancelled"));
    int hard_set = jtruth(jget(in, "hard_cancel_set"));
    double generation = json_get_num(in, "generation", 0);
    int frozen = jtruth(jget(in, "frozen"));
    json_t *ret = json_object();

    if (fence_present) {
        json_t *ic = act("install_cancelled_check");
        set_num(ic, "generation", generation);
        push(log, ic);
        if (!frozen) {
            json_set(comp, "_compression_cancelled_check", json_string("fence_is_cancelled"));
            json_set(comp, "_compression_cancelled_check_owner", json_number(generation));
        }
    }
    const char *raised = NULL;
    json_t *compressed = NULL;
    if (fence_present && fence_cancelled) {
        push(log, act("skip_cancelled"));
        compressed = json_copy(jget(in, "messages"));
    } else {
        json_t *mw = act("mark_working");
        set_num(mw, "generation", generation);
        push(log, mw);
        if (!frozen)
            json_set(comp, "_compression_working_attempt_generation", json_number(generation));
        const json_t *cf = jget(in, "compress");
        const char *cf_raises = jgets(cf, "raises");
        json_t *ce = act("compress_fn");
        set_val(ce, "kwargs", jget(in, "kwargs"));
        set_str(ce, "raised", cf_raises[0] ? cf_raises : NULL);
        set_val(ce, "ret", cf_raises[0] ? NULL : jget(cf, "result"));
        push(log, ce);
        if (cf_raises[0]) {
            raised = cf_raises;
        } else {
            compressed = json_copy(jget(cf, "result"));
            if (hard_set) {
                push(log, act("hard_cancel_raise"));
                raised = "AuxiliaryExplicitCancellation";
                json_free(compressed);
                compressed = NULL;
            }
        }
    }
    if (fence_present) {
        const json_t *owner = jget(comp, "_compression_cancelled_check_owner");
        int has_owner = owner && owner->type != JSON_NULL;
        int cleared = !(has_owner && generation != 0 && owner->num_val != generation);
        json_t *cc = act("clear_cancelled_check");
        set_num(cc, "generation", generation);
        set_bool(cc, "ret", cleared);
        push(log, cc);
        if (cleared && !frozen) {
            json_set(comp, "_compression_cancelled_check", json_null());
            json_set(comp, "_compression_cancelled_check_owner", json_null());
        }
    }
    set_val(ret, "compressed", compressed);
    json_free(compressed);
    set_str(ret, "raised", raised);
    json_set(ret, "state", comp);
    ret_event(log, ret);
    return log;
}
/* ===========================================================================
 * Summary phase + commit-site guards + post-summary construction order.
 * =========================================================================== */

/* _run_summary_phase: adopt grown parent, gather memory context, run the
 * summarizer; hard-cancel restores snapshot + live list and records the stall
 * backoff under the lease; other failures restore + release + re-raise.
 * SEAMs: lease refresher/release, adopt, memory context, resolve, heartbeat,
 *        dispatch, restore, stall backoff, telemetry, existing prompt. */
/* PoP: ccflow_run_summary_phase @ agent/conversation_compression.py:_run_summary_phase */
json_t *ccflow_run_summary_phase(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    json_t *comp = copy_state(in, "compressor");
    int in_place = jtruth(jget(in, "in_place"));
    json_t *messages = json_copy(jget(in, "messages"));
    if (!messages || messages->type != JSON_ARRAY) {
        json_free(messages);
        messages = json_array();
    }
    double approx_tokens = json_get_num(in, "approx_tokens", 0);
    int approx_present = jhas(in, "approx_tokens")
                         && jget(in, "approx_tokens")->type != JSON_NULL;
    size_t pre_msg_count = messages->c.count;
    int heartbeat_active = 0;
    json_t *before = NULL;
    json_t *ret = json_object();

    push(log, act("start_refresher"));

    if (!in_place) {
        const json_t *adopt = jget(in, "adopt");
        const json_t *adopted = jget(adopt, "result");
        json_t *ae = act("adopt_grown_parent");
        set_val(ae, "ret", adopted);
        push(log, ae);
        if (adopted && adopted->type == JSON_ARRAY) {
            json_free(messages);
            messages = json_copy(adopted);
            pre_msg_count = messages->c.count;
            approx_tokens = 0;
            approx_present = 1;
            json_set(agent, "_persist_user_message_idx", json_number((double)messages->c.count));
        }
    }

    const json_t *mem = jget(in, "memory");
    const char *mem_raised = jgets(mem, "raised");
    json_t *me = act("pre_compress_memory");
    set_str(me, "ret", jgets(mem, "memory_context"));
    set_str(me, "raised", mem_raised[0] ? mem_raised : NULL);
    push(log, me);

    const char *raised = NULL;
    json_t *compressed = NULL;
    if (mem_raised[0]) {
        raised = mem_raised;
    } else {
        const json_t *resolve = jget(in, "resolve");
        json_t *re = act("resolve_compress_call");
        set_val(re, "kwargs", jget(resolve, "kwargs"));
        push(log, re);
        before = json_copy(messages);
        json_t *hb = act("heartbeat_start");
        set_bool(hb, "emit_client_status", jtruth(jget(in, "status_emitted")));
        push(log, hb);
        heartbeat_active = 1;
        json_t *ts = act("telemetry_seed_reset");
        set_val(ts, "seed", jget(in, "seed"));
        push(log, ts);
        if (!jtruth(jget(in, "frozen"))) {
            json_set(comp, "_last_compression_telemetry", json_null());
            json_set(comp, "_active_compression_telemetry", json_null());
            json_set(comp, "_compression_telemetry_seed", json_copy(jget(in, "seed")));
        }
        const json_t *disp = jget(in, "dispatch");
        const char *disp_raised = jgets(disp, "raises");
        json_t *de = act("summary_dispatch");
        set_str(de, "raised", disp_raised[0] ? disp_raised : NULL);
        push(log, de);
        if (disp_raised[0]) {
            raised = disp_raised;
        } else {
            compressed = json_copy(jget(disp, "compressed"));
        }
    }

    /* exception unwind paths */
    if (raised && strcmp(raised, "AuxiliaryExplicitCancellation") == 0) {
        const json_t *restore = jget(in, "restore");
        const char *restore_raises = jgets(restore, "raises");
        json_t *rr = act("restore_compressor");
        set_str(rr, "raised", restore_raises[0] ? restore_raises : NULL);
        push(log, rr);
        if (restore_raises[0]) {
            int restored = before && !jequal(messages, before);
            if (restored) {
                json_free(messages);
                messages = json_copy(before);
            }
            json_t *rm = act("restore_messages");
            set_bool(rm, "restored", restored);
            push(log, rm);
            if (heartbeat_active) {
                json_t *hs = act("heartbeat_stop");
                set_str(hs, "desc", "context compression rollback failed");
                push(log, hs);
                heartbeat_active = 0;
            }
            push(log, act("lease_release"));
            char *fc = NULL;
            if (asprintf(&fc, "rollback:%s", restore_raises) >= 0) {
                json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
                set_str(tel, "failure_class", fc);
                set_val(tel, "seed", jget(in, "seed"));
                push(log, tel);
                free(fc);
            }
            set_str(ret, "raised", restore_raises);
            json_set(ret, "state", agent);
            json_set(ret, "compressor", comp);
            json_free(before);
            json_free(messages);
            json_free(compressed);
            ret_event(log, ret);
            return log;
        }
        int restored = before && !jequal(messages, before);
        if (restored) {
            json_free(messages);
            messages = json_copy(before);
        }
        json_t *rm = act("restore_messages");
        set_bool(rm, "restored", restored);
        push(log, rm);
        int stall_backoff = jtruth(jget(in, "stall_backoff"));
        json_t *sb = act("record_stall_backoff");
        set_bool(sb, "ret", stall_backoff);
        push(log, sb);
        if (heartbeat_active) {
            json_t *hs = act("heartbeat_stop");
            set_str(hs, "desc", "context compression cancelled");
            push(log, hs);
            heartbeat_active = 0;
        }
        push(log, act("lease_release"));
        int host_cancel = jtruth(jget(in, "fence_present"))
                          && jtruth(jget(in, "fence_cancelled"))
                          && !jtruth(jget(in, "hard_cancel_set"));
        const char *fc = stall_backoff ? "stall_interrupted"
                                       : (host_cancel ? "commit_fence_cancelled"
                                                      : "explicit_interrupt");
        json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
        set_str(tel, "failure_class", fc);
        set_val(tel, "seed", jget(in, "seed"));
        push(log, tel);
        const json_t *cached = jget(in, "cached");
        int built = !jtruth(cached);
        const char *prompt = built ? jgets(in, "build_result")
                                   : (cached->type == JSON_STRING ? cached->str_val : "");
        json_t *ep = act("existing_system_prompt");
        set_bool(ep, "built", built);
        set_str(ep, "ret", prompt);
        push(log, ep);
        json_t *phase = json_object();
        json_set(phase, "messages", messages);
        json_set(phase, "abort_prompt", json_string(prompt));
        json_set(ret, "phase", phase);
        json_set(ret, "state", agent);
        json_set(ret, "compressor", comp);
        json_free(before);
        json_free(compressed);
        ret_event(log, ret);
        return log;
    }
    if (raised) {
        int restored = before && !jequal(messages, before);
        if (restored) {
            json_free(messages);
            messages = json_copy(before);
        }
        json_t *rm = act("restore_messages");
        set_bool(rm, "restored", restored);
        push(log, rm);
        if (heartbeat_active) {
            json_t *hs = act("heartbeat_stop");
            set_str(hs, "desc", "context compression failed");
            push(log, hs);
            heartbeat_active = 0;
        }
        push(log, act("lease_release"));
        char *fc = NULL;
        if (asprintf(&fc, "exception:%s", raised) >= 0) {
            json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
            set_str(tel, "failure_class", fc);
            set_val(tel, "seed", jget(in, "seed"));
            push(log, tel);
            free(fc);
        }
        set_str(ret, "raised", raised);
        json_set(ret, "state", agent);
        json_set(ret, "compressor", comp);
        json_free(before);
        json_free(messages);
        json_free(compressed);
        ret_event(log, ret);
        return log;
    }
    if (heartbeat_active) {
        json_t *hs = act("heartbeat_stop");
        set_str(hs, "desc", "context compression completed");
        push(log, hs);
    }
    json_t *phase = json_object();
    json_set(phase, "messages", messages);
    json_set(phase, "compressed", compressed ? compressed : json_null());
    json_set(phase, "messages_before_compression", before ? before : json_null());
    if (approx_present)
        json_set(phase, "approx_tokens", json_number(approx_tokens));
    else
        json_set(phase, "approx_tokens", json_null());
    set_num(phase, "pre_msg_count", (double)pre_msg_count);
    json_set(ret, "phase", phase);
    json_set(ret, "state", agent);
    json_set(ret, "compressor", comp);
    ret_event(log, ret);
    return log;
}

/* _candidate_rejected: unusable-candidate gates in live order — compressor
 * abort, no progress, empty transcript, superseded working attempt.
 * SEAMs: warning emit, structural no-op recorder, telemetry. */
/* PoP: ccflow_candidate_rejected @ agent/conversation_compression.py:_candidate_rejected */
json_t *ccflow_candidate_rejected(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    const json_t *comp = jget(in, "compressor");
    const json_t *compressed = jget(in, "compressed");
    const json_t *before = jget(in, "before");
    json_t *ret = json_object();

    if (jtruth(jget(comp, "_last_compress_aborted"))) {
        const json_t *se = jget(comp, "_last_summary_error");
        const char *summary_error = (se && se->type == JSON_STRING) ? se->str_val : NULL;
        const char *err = (summary_error && summary_error[0]) ? summary_error : "unknown error";
        const json_t *last = jget(agent, "_last_compression_summary_warning");
        if (!(last && last->type == JSON_STRING && strcmp(last->str_val, err) == 0)) {
            json_set(agent, "_last_compression_summary_warning", json_string(err));
            char *text = NULL;
            if (asprintf(&text,
                         "\xe2\x9a\xa0 Compression aborted: %s. "
                         "No messages were dropped \xe2\x80\x94 conversation continues unchanged. "
                         "Run /compress to retry, or /new to start a fresh session.", err) >= 0) {
                json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
                set_str(ew, "msg", text);
                push(log, ew);
                free(text);
            }
        }
        json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
        if (summary_error && summary_error[0])
            set_str(tel, "failure_class", "summary_generation_aborted");
        else
            set_val(tel, "failure_class", se);
        set_val(tel, "seed", jget(in, "seed"));
        push(log, tel);
        set_bool(ret, "v", 1);
        json_set(ret, "state", agent);
        ret_event(log, ret);
        return log;
    }

    int no_progress = jequal(compressed, before);
    if (!no_progress) {
        json_t *sc = cc_strip_marker_for_comparison(compressed);
        json_t *sb = cc_strip_marker_for_comparison(before);
        no_progress = jequal(sc, sb);
        json_free(sc);
        json_free(sb);
    }
    if (no_progress) {
        const json_t *rec = jget(in, "structural_recorder");
        if (rec && jtruth(jget(rec, "present"))) {
            json_t *re = act("record_structural_no_op");
            set_bool(re, "raised", jtruth(jget(rec, "raises")));
            push(log, re);
        }
        json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
        set_str(tel, "failure_class", "no_progress");
        set_val(tel, "seed", jget(in, "seed"));
        push(log, tel);
        set_bool(ret, "v", 1);
        json_set(ret, "state", agent);
        ret_event(log, ret);
        return log;
    }
    if (!jtruth(compressed)) {
        json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
        set_str(ew, "msg",
                "\xe2\x9a\xa0 Compression returned an empty transcript. No session split was "
                "performed; conversation continues unchanged.");
        set_bool(ew, "raised", jtruth(jget(in, "emit_raises")));
        push(log, ew);
        json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
        set_str(tel, "failure_class", "empty_transcript");
        set_val(tel, "seed", jget(in, "seed"));
        push(log, tel);
        set_bool(ret, "v", 1);
        json_set(ret, "state", agent);
        ret_event(log, ret);
        return log;
    }
    /* supersession: working-attempt check with the entry-generation fallback */
    double generation = json_get_num(in, "generation", 0);
    int current;
    if (generation == 0) {
        current = 1;
    } else {
        const json_t *marker = jget(comp, "_compression_working_attempt_generation");
        if (!marker || marker->type == JSON_NULL) {
            const json_t *cur = jget(comp, "_compression_attempt_generation");
            long long entry = jtruth(cur) ? (long long)cur->num_val : 0;
            current = entry == 0 || (double)entry == generation;
        } else {
            current = (long long)marker->num_val == (long long)generation;
        }
    }
    if (!current) {
        json_set(agent, "_last_compaction_in_place", json_bool(false));
        json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
        set_str(tel, "failure_class", "attempt_superseded");
        set_val(tel, "seed", jget(in, "seed"));
        push(log, tel);
        set_bool(ret, "v", 1);
        json_set(ret, "state", agent);
        ret_event(log, ret);
        return log;
    }
    set_bool(ret, "v", 0);
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}

#define CCFLOW_REFUSED_WARNING \
    "\xe2\x9a\xa0\xef\xb8\x8f Compression refused: the generated summary would have GROWN the conversation " \
    "instead of shrinking it. No messages were dropped \xe2\x80\x94 conversation continues unchanged."

/* _salvage_or_refuse_grown_transcript: anti-growth guard at the commit site.
 * Like-for-like rough estimates; one mechanical salvage pass on growth.
 * SEAMs: estimate_messages_tokens_rough, salvage_grown_transcript,
 *        record_rejected_compaction, warning emit, telemetry. */
/* PoP: ccflow_salvage_or_refuse_grown_transcript @ agent/conversation_compression.py:_salvage_or_refuse_grown_transcript */
json_t *ccflow_salvage_or_refuse_grown_transcript(const json_t *in)
{
    json_t *log = json_array();
    json_t *comp = copy_state(in, "compressor");
    double rough_in = json_get_num(in, "rough_in", 0);
    double rough_out = json_get_num(in, "rough_out", 0);
    json_t *compressed = json_copy(jget(in, "compressed"));
    json_t *ret = json_object();

    json_t *e1 = act("estimate_rough");
    set_str(e1, "which", "in");
    set_num(e1, "ret", rough_in);
    push(log, e1);
    json_t *e2 = act("estimate_rough");
    set_str(e2, "which", "out");
    set_num(e2, "ret", rough_out);
    push(log, e2);

    if (rough_out > rough_in) {
        const json_t *salv = jget(in, "salvage");
        const json_t *salvaged = jget(salv, "result");
        json_t *se = act("salvage_grown");
        set_num(se, "budget", rough_in);
        set_val(se, "ret", salvaged);
        push(log, se);
        if (salvaged && salvaged->type != JSON_NULL) {
            double salv_est = json_get_num(in, "salv_est", 0);
            json_t *e3 = act("estimate_rough");
            set_str(e3, "which", "salvaged");
            set_num(e3, "ret", salv_est);
            push(log, e3);
            if (salv_est < rough_in) {
                json_free(compressed);
                compressed = json_copy(salvaged);
                rough_out = salv_est;
            }
        }
    }
    if (rough_out > rough_in) {
        if (!jtruth(jget(in, "frozen")))
            json_set(comp, "_last_compress_refused_would_grow", json_bool(true));
        json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
        set_str(ew, "msg", CCFLOW_REFUSED_WARNING);
        set_bool(ew, "raised", jtruth(jget(in, "emit_raises")));
        push(log, ew);
        const json_t *cached = jget(in, "cached");
        int built = !jtruth(cached);
        const char *prompt = built ? jgets(in, "build_result")
                                   : (cached->type == JSON_STRING ? cached->str_val : "");
        json_t *ep = act("existing_system_prompt");
        set_bool(ep, "built", built);
        set_str(ep, "ret", prompt);
        push(log, ep);
        json_t *tel = act("telemetry_aborted");
    set_num(tel, "started_at", json_get_num(in, "started_at", 0));
        set_str(tel, "failure_class", "would_grow");
        set_val(tel, "seed", jget(in, "seed"));
        push(log, tel);
        const json_t *rec = jget(in, "record_rejected");
        if (rec && jtruth(jget(rec, "present"))) {
            json_t *re = act("record_rejected_compaction");
            set_bool(re, "raised", jtruth(jget(rec, "raises")));
            push(log, re);
        }
        push(log, act("restore_prune_rearm"));
        static const char *keys[] = { "_proactive_prune_rearm_tokens",
                                      "_proactive_prune_cold_credit" };
        const json_t *snap = jget(in, "snapshot");
        for (int i = 0; i < 2; i++) {
            const json_t *v = jget(snap, keys[i]);
            if (v) json_set(comp, keys[i], json_copy(v));
        }
        json_free(compressed);
        json_t *v = json_array();
        json_append(v, json_null());
        json_append(v, json_string(prompt));
        json_set(ret, "v", v);
        json_set(ret, "compressor", comp);
        ret_event(log, ret);
        return log;
    }
    json_t *v = json_array();
    json_append(v, compressed);
    json_append(v, json_null());
    json_set(ret, "v", v);
    json_set(ret, "compressor", comp);
    ret_event(log, ret);
    return log;
}

/* _strip_stale_todo_snapshot over json content (pure leaf of the fold). */
static json_t *strip_todo_content(const json_t *content)
{
    if (!content) return json_null();
    if (content->type == JSON_STRING) {
        const char *s = content->str_val ? content->str_val : "";
        const char *idx = strstr(s, CCFLOW_TODO_HEADER);
        if (!idx) return json_copy(content);
        size_t n = (size_t)(idx - s);
        while (n > 0 && py_isspace(s[n - 1])) n--;
        char *cut = malloc(n + 1);
        if (!cut) return json_null();
        memcpy(cut, s, n);
        cut[n] = '\0';
        json_t *out = json_string(cut);
        free(cut);
        return out;
    }
    if (content->type == JSON_ARRAY) {
        json_t *cleaned = json_array();
        for (size_t i = 0; i < content->c.count; i++) {
            const json_t *part = content->c.items[i];
            const char *text = "";
            if (part && part->type == JSON_OBJECT
                && strcmp(jgets(part, "type"), "text") == 0) {
                const json_t *t = jget(part, "text");
                if (jtruth(t) && t->type == JSON_STRING) text = t->str_val;
            }
            const char *idx = text[0] ? strstr(text, CCFLOW_TODO_HEADER) : NULL;
            if (!idx) {
                json_append(cleaned, json_copy(part));
            } else {
                size_t n = (size_t)(idx - text);
                while (n > 0 && py_isspace(text[n - 1])) n--;
                if (n > 0) {
                    char *cut = malloc(n + 1);
                    if (cut) {
                        memcpy(cut, text, n);
                        cut[n] = '\0';
                        json_t *np = json_copy(part);
                        json_set(np, "text", json_string(cut));
                        json_append(cleaned, np);
                        free(cut);
                    }
                }
            }
        }
        return cleaned;
    }
    return json_copy(content);
}

/* _append_text_to_content (context_compressor leaf): str concat / list append
 * of a text block / None -> text. Static helper, no PoP (other module). */
static json_t *append_text(const json_t *content, const char *text)
{
    if (!content || content->type == JSON_NULL) return json_string(text);
    if (content->type == JSON_ARRAY) {
        json_t *out = json_copy(content);
        json_t *block = json_object();
        set_str(block, "type", "text");
        set_str(block, "text", text);
        json_append(out, block);
        return out;
    }
    const char *rendered = (content->type == JSON_STRING)
                               ? (content->str_val ? content->str_val : "") : "";
    char *joined = NULL;
    if (asprintf(&joined, "%s%s", rendered, text) < 0) return json_null();
    json_t *out = json_string(joined);
    free(joined);
    return out;
}

/* _fold_todo_snapshot: strip stale snapshots, fold the live one in place.
 * SEAMs: todo store format/has_items, _repair_message_sequence,
 *        _pruned_skill_reload_notice, _is_real_user_message probe. */
/* PoP: ccflow_fold_todo_snapshot @ agent/conversation_compression.py:_fold_todo_snapshot */
json_t *ccflow_fold_todo_snapshot(const json_t *in)
{
    json_t *log = json_array();
    json_t *compressed = json_copy(jget(in, "compressed"));
    if (!compressed || compressed->type != JSON_ARRAY) {
        json_free(compressed);
        compressed = json_array();
    }
    const char *todo_snapshot = jgets(in, "todo_snapshot");
    json_t *tf = act("todo_format");
    set_str(tf, "ret", todo_snapshot);
    push(log, tf);

    const json_t *hi = jget(in, "has_items");
    int authoritative = 0;
    if (hi && jtruth(jget(hi, "present"))) {
        int raises = jtruth(jget(hi, "raises"));
        authoritative = !raises && jtruth(jget(hi, "result"));
        json_t *he = act("todo_has_items");
        set_bool(he, "raised", raises);
        set_bool(he, "ret", authoritative);
        push(log, he);
    }
    if (authoritative) {
        for (size_t k = compressed->c.count; k > 0; k--) {
            size_t i = k - 1;
            json_t *row = compressed->c.items[i];
            if (!row || row->type != JSON_OBJECT) continue;
            if (strcmp(jgets(row, "role"), "user") != 0) continue;
            const json_t *content = jget(row, "content");
            json_t *stripped = strip_todo_content(content);
            if (jequal(stripped, content)) {
                json_free(stripped);
                continue;
            }
            int synthetic = jtruth(jget(row, "_todo_snapshot_synthetic"));
            json_t *only_in = json_object();
            json_set(only_in, "content", json_copy(content));
            json_set(only_in, "stripped", json_copy(stripped));
            json_t *only = ccflow_todo_snapshot_is_only_content(only_in);
            int only_content = only && only->type == JSON_BOOL && only->bool_val;
            json_free(only_in);
            json_free(only);
            if (synthetic && only_content) {
                arr_remove(compressed, i);
                json_t *pe = act("todo_strip");
                set_num(pe, "idx", (double)i);
                set_str(pe, "action", "pop");
                push(log, pe);
                if (i < compressed->c.count)
                    push(log, act("repair_message_sequence"));
            } else {
                json_set(row, "content", stripped);
                json_obj_del(row, "api_content");
                json_obj_del(row, "_todo_snapshot_synthetic");
                json_t *pe = act("todo_strip");
                set_num(pe, "idx", (double)i);
                set_str(pe, "action", "replace");
                push(log, pe);
            }
            break;
        }
    }
    if (todo_snapshot[0]) {
        const char *notice = jgets(in, "reload_notice");
        json_t *ne = act("pruned_skill_notice");
        set_str(ne, "ret", notice);
        push(log, ne);
        char *snapshot = NULL;
        if (notice[0]) {
            if (asprintf(&snapshot, "%s\n\n%s", todo_snapshot, notice) < 0) snapshot = NULL;
        } else {
            snapshot = strdup(todo_snapshot);
        }
        if (!snapshot) snapshot = strdup(todo_snapshot);
        int merged = 0;
        json_t *tail = compressed->c.count
                           ? compressed->c.items[compressed->c.count - 1] : NULL;
        if (tail && tail->type == JSON_OBJECT && strcmp(jgets(tail, "role"), "user") == 0) {
            const json_t *tail_content = jget(tail, "content");
            json_t *stripped = strip_todo_content(tail_content);
            int is_real = jtruth(jget(in, "tail_is_real"));
            json_t *probe = act("is_real_user_probe");
            set_bool(probe, "ret", is_real);
            push(log, probe);
            if (is_real) {
                int stripped_nonempty = stripped && stripped->type == JSON_STRING
                                        && stripped->str_val && stripped->str_val[0];
                char *snap_text = NULL;
                if (stripped_nonempty) {
                    if (asprintf(&snap_text, "\n\n%s", snapshot) < 0) snap_text = NULL;
                } else {
                    snap_text = strdup(snapshot);
                }
                json_t *new_content = append_text(stripped, snap_text ? snap_text : snapshot);
                free(snap_text);
                json_set(tail, "content", new_content);
                json_obj_del(tail, "api_content");
                merged = 1;
                push(log, act("todo_merge_append"));
            } else {
                int changed = !jequal(stripped, tail_content);
                json_t *probe_msg = json_object();
                set_str(probe_msg, "role", "user");
                json_set(probe_msg, "content", json_copy(stripped));
                char *pt = msg_text(probe_msg);
                char *pts = py_strip(pt);
                int empty = !pts[0];
                free(pt);
                free(pts);
                json_free(probe_msg);
                if (changed && empty) {
                    json_set(tail, "content", json_string(snapshot));
                    json_obj_del(tail, "api_content");
                    json_set(tail, "_todo_snapshot_synthetic", json_bool(true));
                    merged = 1;
                    push(log, act("todo_merge_refresh"));
                }
            }
            json_free(stripped);
        }
        if (!merged) {
            json_t *row = json_object();
            set_str(row, "role", "user");
            set_str(row, "content", snapshot);
            set_bool(row, "_todo_snapshot_synthetic", 1);
            json_append(compressed, row);
            push(log, act("todo_append"));
        }
        free(snapshot);
    }
    json_t *ret = json_object();
    json_set(ret, "compressed", compressed);
    ret_event(log, ret);
    return log;
}

/* _rebuild_system_prompt_at_boundary: seeded-prompt retention vs mandatory
 * rebuild with byte-equality identity preservation.
 * SEAMs: reconstruct_static_prefix, _invalidate_system_prompt,
 *        _refresh_agent_tool_definitions, _build_system_prompt. */
/* PoP: ccflow_rebuild_system_prompt_at_boundary @ agent/conversation_compression.py:_rebuild_system_prompt_at_boundary */
json_t *ccflow_rebuild_system_prompt_at_boundary(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    json_t *ret = json_object();
    const json_t *cached = jget(agent, "_cached_system_prompt");
    if (jtruth(jget(in, "retain_seeded"))) {
        if (jtruth(cached)) {
            json_t *rc = act("reconstruct_static_prefix");
            set_str(rc, "log_label", "compression seeded-prompt");
            push(log, rc);
        }
        set_val(ret, "v", cached);
        json_set(ret, "state", agent);
        ret_event(log, ret);
        return log;
    }
    push(log, act("invalidate_system_prompt"));
    json_t *rf = act("refresh_tool_definitions");
    set_bool(rf, "raised", jtruth(jget(in, "refresh_raises")));
    push(log, rf);
    const char *rebuilt = jgets(in, "build_result");
    json_t *bp = act("build_system_prompt");
    set_str(bp, "ret", rebuilt);
    push(log, bp);
    const char *new_prompt;
    if (cached && cached->type != JSON_NULL && cached->type == JSON_STRING
        && strcmp(cached->str_val ? cached->str_val : "", rebuilt) == 0) {
        new_prompt = cached->str_val ? cached->str_val : "";
        json_set(agent, "_cached_system_prompt", json_copy(cached));
        json_t *rc = act("reconstruct_static_prefix");
        set_str(rc, "log_label", "compression keep-prompt");
        push(log, rc);
    } else {
        new_prompt = rebuilt;
        json_set(agent, "_cached_system_prompt", json_string(rebuilt));
        if (cached && cached->type != JSON_NULL)
            json_set(agent, "_compaction_prompt_drift_logged", json_bool(true));
    }
    set_str(ret, "v", new_prompt);
    json_set(ret, "state", agent);
    ret_event(log, ret);
    return log;
}
/* ===========================================================================
 * Commit phase + boundary bookkeeping + rotation publication.
 * =========================================================================== */

/* _commit_compaction: anti-growth gate, memory extraction, then the in-place
 * archive or the parent->child rotation; failures roll the live list back and
 * arm the split-failure cooldown.
 * SEAMs: salvage gate, commit_memory_session (+telemetry hold), rejoin,
 * coverage, archive commit, held watermark, publish, update_system_prompt,
 * role lookup, split-failure cooldown. */
/* PoP: ccflow_commit_compaction @ agent/conversation_compression.py:_commit_compaction */
json_t *ccflow_commit_compaction(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    json_t *comp = copy_state(in, "compressor");
    json_t *messages = json_copy(jget(in, "messages"));
    json_t *compressed = json_copy(jget(in, "compressed"));
    int in_place = jtruth(jget(in, "in_place"));
    int made_progress = jtruth(jget(in, "made_progress"));
    const json_t *before = jget(in, "messages_before");
    const json_t *tail = jget(in, "verbatim_tail");
    int has_tail = jtruth(tail);
    double commit_started_at = json_get_num(in, "now", 0);
    const char *split_status = "not_applicable";
    const char *old_session_id = NULL;
    int succeeded = 0;
    int compacted_in_place = 0;
    json_t *ret = json_object();

    if (!jtruth(jget(in, "session_db_present"))) {
        json_set(ret, "compressed", compressed);
        set_num(ret, "commit_started_at", commit_started_at);
        set_val(ret, "refused_prompt", NULL);
        set_val(ret, "old_session_id", NULL);
        set_str(ret, "split_status", split_status);
        set_bool(ret, "session_commit_succeeded", 0);
        set_bool(ret, "compacted_in_place", 0);
        set_bool(ret, "made_progress", 0);
        json_set(ret, "state", agent);
        json_set(ret, "compressor", comp);
        json_free(messages);
        ret_event(log, ret);
        return log;
    }
    split_status = "pending";
    /* pop _compaction_tail tags before the size estimate; track truthy tags */
    size_t tail_count = 0;
    if (compressed && compressed->type == JSON_ARRAY) {
        for (size_t i = 0; i < compressed->c.count; i++) {
            json_t *m = compressed->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *tag = json_obj_get(m, "_compaction_tail");
            if (tag && jtruth(tag)) tail_count++;
            json_obj_del(m, "_compaction_tail");
        }
    }
    json_t *sg = act("salvage_gate");
    const json_t *salv = jget(in, "salvage");
    int refused = jtruth(jget(salv, "refused"));
    set_bool(sg, "refused", refused);
    set_str(sg, "prompt", jgets(salv, "prompt"));
    push(log, sg);
    if (refused) {
        json_free(compressed);
        json_set(ret, "compressed", messages);
        set_num(ret, "commit_started_at", commit_started_at);
        set_str(ret, "refused_prompt", jgets(salv, "prompt"));
        set_val(ret, "old_session_id", NULL);
        set_str(ret, "split_status", "not_applicable");
        set_bool(ret, "session_commit_succeeded", 0);
        set_bool(ret, "compacted_in_place", 0);
        set_bool(ret, "made_progress", 0);
        json_set(ret, "state", agent);
        json_set(ret, "compressor", comp);
        ret_event(log, ret);
        return log;
    }
    /* memory publish under the telemetry hold (restore after the call) */
    json_t *held[3];
    static const char *tfields[] = { "_last_compression_telemetry",
                                     "_active_compression_telemetry",
                                     "_compression_telemetry_seed" };
    for (int i = 0; i < 3; i++)
        held[i] = json_copy(jget(comp, tfields[i]));
    const json_t *mc = jget(in, "memory_call");
    int mc_raises = jtruth(jget(mc, "raises"));
    const json_t *resets = jget(mc, "resets");
    if (resets && resets->type == JSON_OBJECT) {
        for (size_t i = 0; i < resets->c.count; i++)
            json_set(comp, resets->c.keys[i], json_copy(resets->c.items[i]));
    }
    for (int i = 0; i < 3; i++) {
        json_set(comp, tfields[i], held[i]);
        held[i] = NULL;
    }
    json_t *mce = act("commit_memory_session");
    set_bool(mce, "raised", mc_raises);
    push(log, mce);

    const char *error_msg = jgets(in, "error_msg");
    int failed = mc_raises;
    if (!failed && in_place) {
        /* turn-lease adjustment: unmarked, un-id'd rows in the held tail are
         * this turn's fresh rows, not superseded duplicates */
        const json_t *holder = jget(agent, "_active_session_turn_lease_holder");
        const json_t *turn_idx = jget(agent, "_persist_user_message_idx");
        size_t n_msgs = (messages && messages->type == JSON_ARRAY) ? messages->c.count : 0;
        if (holder && holder->type != JSON_NULL && turn_idx && turn_idx->type == JSON_NUMBER) {
            long long ti = (long long)turn_idx->num_val;
            if (ti >= 0 && (size_t)ti < n_msgs) {
                size_t start = (size_t)ti;
                size_t tail_start = n_msgs > tail_count ? n_msgs - tail_count : 0;
                if (start < tail_start) start = tail_start;
                size_t fresh = 0;
                for (size_t i = start; i < n_msgs; i++) {
                    const json_t *m = messages->c.items[i];
                    if (!m || m->type != JSON_OBJECT) continue;
                    if (jtruth(jget(m, CCFLOW_DB_MARKER))) continue;
                    const json_t *rid = jget(m, "_row_id");
                    if (rid && rid->type == JSON_NUMBER) continue;
                    fresh++;
                }
                tail_count = fresh > tail_count ? 0 : tail_count - fresh;
            }
        }
        /* absorbed durable rows carried by held tail dicts (active only) */
        const json_t *roles = jget(in, "absorbed_roles");
        size_t tail_start = n_msgs > tail_count ? n_msgs - tail_count : 0;
        double seen[256];
        size_t n_seen = 0;
        for (size_t i = tail_start; i < n_msgs; i++) {
            const json_t *m = messages->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            const json_t *absorbed = jget(m, "_absorbed_row_ids");
            if (!absorbed || absorbed->type != JSON_ARRAY) continue;
            for (size_t k = 0; k < absorbed->c.count; k++) {
                const json_t *r = absorbed->c.items[k];
                if (!r || r->type != JSON_NUMBER || r->num_val <= 0) continue;
                char keybuf[32];
                snprintf(keybuf, sizeof keybuf, "%lld", (long long)r->num_val);
                const json_t *role = jget(roles, keybuf);
                if (!role || role->type == JSON_NULL) continue;
                int dup = 0;
                for (size_t q = 0; q < n_seen; q++)
                    if (seen[q] == r->num_val) { dup = 1; break; }
                if (!dup && n_seen < 256) seen[n_seen++] = r->num_val;
            }
        }
        tail_count += n_seen;
        json_t *persisted = compressed;
        if (has_tail) {
            const json_t *rj = jget(in, "rejoin");
            json_t *rje = act("rejoin_head_tail");
            set_val(rje, "ret", jget(rj, "result"));
            push(log, rje);
            persisted = json_copy(jget(rj, "result"));
            tail_count += tail->c.count;
        }
        const json_t *cov = jget(in, "coverage");
        json_t *cve = act("coverage_for_commit");
        set_val(cve, "covered_ids", jget(cov, "covered_ids"));
        set_val(cve, "unresolved_held", jget(cov, "unresolved_held"));
        push(log, cve);
        const json_t *hw = jget(in, "held_watermark");
        json_t *hwe = act("held_archive_watermark");
        set_val(hwe, "ret", jget(hw, "result"));
        push(log, hwe);
        const json_t *ac = jget(in, "archive_commit");
        int ac_raises = jtruth(jget(ac, "raises"));
        json_t *ace = act("commit_compaction");
        set_val(ace, "watermark", jget(hw, "result"));
        set_str(ace, "lock_holder", jgets(jget(in, "lease"), "holder"));
        set_num(ace, "tail_count", (double)tail_count);
        set_bool(ace, "raised", ac_raises);
        push(log, ace);
        if (ac_raises) {
            failed = 1;
            if (persisted != compressed) json_free(persisted);
        } else {
            if (persisted != compressed) {
                json_free(compressed);
                compressed = persisted;
            }
            split_status = "in_place_committed";
            /* stamp the compacted set + verbatim tail as persisted */
            for (size_t i = 0; i < compressed->c.count; i++) {
                json_t *m = compressed->c.items[i];
                if (m && m->type == JSON_OBJECT)
                    json_set(m, CCFLOW_DB_MARKER, json_bool(true));
            }
            if (has_tail) {
                for (size_t i = 0; i < tail->c.count; i++) {
                    json_t *m = tail->c.items[i];
                    if (m && m->type == JSON_OBJECT)
                        json_set(m, CCFLOW_DB_MARKER, json_bool(true));
                }
            }
            json_set(agent, "_flushed_db_message_ids", json_array());
            compacted_in_place = 1;
            json_t *up = act("update_system_prompt");
            set_str(up, "sid", jgets(agent, "session_id"));
            set_str(up, "prompt", jgets(in, "new_system_prompt"));
            set_bool(up, "raised", jtruth(jget(in, "update_prompt_raises")));
            push(log, up);
            if (jtruth(jget(in, "update_prompt_raises"))) {
                failed = 1;
            } else {
                json_set(agent, "_last_flushed_db_idx", json_number(0));
                succeeded = 1;
            }
        }
    } else if (!failed) {
        old_session_id = jgets(agent, "session_id");
        if (has_tail) {
            const json_t *rj = jget(in, "rejoin");
            json_t *rje = act("rejoin_head_tail");
            set_val(rje, "ret", jget(rj, "result"));
            push(log, rje);
            json_free(compressed);
            compressed = json_copy(jget(rj, "result"));
        }
        const json_t *pub = jget(in, "publish");
        int pub_raises = jtruth(jget(pub, "raises"));
        json_t *pe = act("publish_rotated");
        set_str(pe, "old_session_id", old_session_id);
        set_bool(pe, "raised", pub_raises);
        push(log, pe);
        if (pub_raises) {
            failed = 1;
        } else {
            const char *new_sid = jgets(pub, "new_session_id");
            json_set(agent, "session_id", json_string(new_sid));
            if (has_tail) {
                for (size_t i = 0; i < tail->c.count; i++) {
                    json_t *m = tail->c.items[i];
                    if (m && m->type == JSON_OBJECT)
                        json_set(m, CCFLOW_DB_MARKER, json_bool(true));
                }
            }
            split_status = "rotated_committed";
            json_set(agent, "_last_flushed_db_idx",
                     json_number((double)(compressed ? compressed->c.count : 0)));
            json_set(agent, "_flushed_db_message_session_id", json_string(new_sid));
            succeeded = 1;
        }
    }

    if (failed) {
        /* rollback: rotation before the id re-pointed, or in-place before the
         * atomic archive committed (gate on split_status, not the flag) */
        int rotation_rollback = !in_place && old_session_id && old_session_id[0]
                                && strcmp(jgets(agent, "session_id"), old_session_id) == 0;
        int do_rollback = rotation_rollback
                          || (in_place && strcmp(split_status, "in_place_committed") != 0
                              && before && before->type != JSON_NULL);
        if (do_rollback) {
            if (rotation_rollback) old_session_id = NULL;
            json_free(messages);
            messages = json_copy(before);
            json_free(compressed);
            compressed = json_copy(messages);
            made_progress = 0;
            push(log, act("restore_prune_rearm"));
            static const char *keys[] = { "_proactive_prune_rearm_tokens",
                                          "_proactive_prune_cold_credit" };
            const json_t *snap = jget(in, "snapshot");
            for (int i = 0; i < 2; i++) {
                const json_t *v = jget(snap, keys[i]);
                if (v) json_set(comp, keys[i], json_copy(v));
            }
        }
        split_status = (!old_session_id && !in_place) ? "aborted" : "failed_not_indexed";
        const json_t *cd = jget(in, "split_cooldown");
        if (cd && jtruth(jget(cd, "present"))) {
            json_t *ce = act("record_split_cooldown");
            set_num(ce, "seconds", 60);
            char *reason = NULL;
            if (asprintf(&reason, "session_split_failed: %s", error_msg) >= 0) {
                set_str(ce, "reason", reason);
                free(reason);
            }
            set_bool(ce, "raised", jtruth(jget(cd, "raises")));
            push(log, ce);
        }
    }
    json_set(ret, "compressed", compressed);
    set_num(ret, "commit_started_at", commit_started_at);
    set_val(ret, "refused_prompt", NULL);
    set_str(ret, "old_session_id", old_session_id);
    set_str(ret, "split_status", split_status);
    set_bool(ret, "session_commit_succeeded", succeeded);
    set_bool(ret, "compacted_in_place", compacted_in_place);
    set_bool(ret, "made_progress", made_progress);
    json_set(ret, "state", agent);
    json_set(ret, "compressor", comp);
    json_free(messages);
    ret_event(log, ret);
    return log;
}

/* _finish_compaction_boundary: post-commit notifications, warnings, usage
 * re-arm, dedup reset. SEAMs: label clear, engine notify, memory switch,
 * warning/event emits, request estimate, usage anchor, boundary record. */
/* PoP: ccflow_finish_compaction_boundary @ agent/conversation_compression.py:_finish_compaction_boundary */
json_t *ccflow_finish_compaction_boundary(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    json_t *comp = copy_state(in, "compressor");
    const char *old_sid = jgets(in, "old_session_id");
    int succeeded = jtruth(jget(in, "session_commit_succeeded"));
    int in_place = jtruth(jget(in, "in_place"));
    int compacted_in_place = jtruth(jget(in, "compacted_in_place"));
    int defer = jtruth(jget(in, "defer_notification"));
    const char *agent_sid = jgets(agent, "session_id");
    const char *boundary_parent = old_sid[0] ? old_sid : agent_sid;

    if (old_sid[0] && succeeded) {
        const json_t *cl = jget(in, "clear_labels");
        const char *via = jgets(cl, "via");
        if (strcmp(via, "none") != 0) {
            json_t *ce = act("clear_activity_labels");
            set_str(ce, "via", via);
            set_str(ce, "sid", old_sid);
            set_bool(ce, "raised", jtruth(jget(cl, "raises")));
            push(log, ce);
        }
    }
    if (succeeded && (old_sid[0] || compacted_in_place)) {
        json_t *ne = act("context_engine_notify");
        set_str(ne, "mode", defer ? "queue" : "direct");
        set_str(ne, "new_session_id", agent_sid);
        set_str(ne, "old_session_id", boundary_parent);
        set_bool(ne, "raised", jtruth(jget(in, "notify_raises")));
        push(log, ne);
    }
    if ((old_sid[0] || in_place) && jtruth(jget(in, "mm_present"))) {
        json_t *ms = act("memory_session_switch");
        set_str(ms, "new_session_id", agent_sid);
        set_str(ms, "parent_session_id", boundary_parent);
        set_bool(ms, "raised", jtruth(jget(in, "memory_switch_raises")));
        push(log, ms);
    }
    double cc_count = json_get_num(comp, "compression_count", 0);
    if (cc_count >= 2) {
        char *msg = NULL;
        if (asprintf(&msg,
                     "%s\xe2\x9a\xa0\xef\xb8\x8f  Session compressed %lld times \xe2\x80\x94 accuracy may degrade. "
                     "Consider /new to start fresh.",
                     jgets(agent, "log_prefix"), (long long)cc_count) >= 0) {
            json_set(agent, "_compression_warning", json_string(msg));
            json_t *ew = act("emit_warning");
        set_bool(ew, "raised", 0);
            set_str(ew, "msg", msg);
            push(log, ew);
            free(msg);
        }
    }
    if (jtruth(jget(in, "event_callback_present"))) {
        json_t *payload = json_object();
        set_str(payload, "platform", jgets(agent, "platform"));
        set_str(payload, "session_id", agent_sid);
        set_str(payload, "old_session_id", old_sid);
        set_bool(payload, "in_place", in_place);
        set_num(payload, "compression_count", cc_count);
        json_t *ec = act("event_callback");
        set_str(ec, "event", "session:compress");
        json_set(ec, "payload", payload);
        set_bool(ec, "raised", jtruth(jget(in, "event_callback_raises")));
        push(log, ec);
    }
    json_set(agent, "_last_compression_attempt_in_place", json_bool(compacted_in_place != 0));
    json_set(agent, "_last_compaction_in_place", json_bool(compacted_in_place != 0));
    double est = json_get_num(in, "estimate_request", 0);
    json_t *ee = act("estimate_request_rough");
    set_num(ee, "ret", est);
    push(log, ee);
    json_set(comp, "last_compression_rough_tokens", json_number(est));
    json_set(comp, "last_prompt_tokens", json_number(-1));
    json_set(comp, "last_completion_tokens", json_number(0));
    json_set(comp, "awaiting_real_usage_after_compression", json_bool(true));
    push(log, act("set_usage_anchor"));
    if (jtruth(jget(in, "compression_made_progress"))) {
        if (jtruth(jget(in, "record_boundary_present"))) {
            json_t *rb = act("record_completed_compaction");
            set_bool(rb, "used_fallback", jtruth(jget(in, "compression_used_fallback")));
            set_bool(rb, "feasibility_skip", jtruth(jget(in, "compression_feasibility_skip")));
            push(log, rb);
        } else {
            json_set(comp, "_verify_compaction_cleared_threshold", json_bool(true));
        }
    }
    json_t *rd = act("reset_read_dedup");
    set_str(rd, "task_id", jgets(in, "task_id"));
    set_str(rd, "session_id", agent_sid);
    push(log, rd);
    json_t *ret = json_object();
    set_num(ret, "v", est);
    json_set(ret, "state", agent);
    json_set(ret, "compressor", comp);
    ret_event(log, ret);
    return log;
}

/* _publish_rotated_compaction: flush parent, publish child, re-point agent.
 * SEAMs: parent-ended check, foreign ceiling, flush, profile, title, mint,
 * child source, yolo, publish, rebind, carry. */
/* PoP: ccflow_publish_rotated_compaction @ agent/conversation_compression.py:_publish_rotated_compaction */
json_t *ccflow_publish_rotated_compaction(const json_t *in)
{
    json_t *log = json_array();
    json_t *agent = copy_state(in, "agent");
    json_t *messages = json_copy(jget(in, "messages"));
    json_t *compressed = json_copy(jget(in, "compressed"));
    const char *old_sid = jgets(in, "old_session_id");
    json_t *ret = json_object();
    size_t n_msgs = (messages && messages->type == JSON_ARRAY) ? messages->c.count : 0;

    const json_t *idxv = jget(agent, "_persist_user_message_idx");
    int have_prefix = idxv && idxv->type == JSON_NUMBER
                      && idxv->num_val >= 0 && (size_t)idxv->num_val <= n_msgs;

    const json_t *pe = jget(in, "parent_ended");
    json_t *pee = act("parent_deliberately_ended");
    set_bool(pee, "ret", jtruth(pe));
    push(log, pee);
    if (jtruth(pe)) {
        char *msg = NULL;
        if (asprintf(&msg, "Compression parent already ended: %s", old_sid) >= 0) {
            set_str(ret, "raised", "RuntimeError");
            set_str(ret, "message", msg);
            free(msg);
        }
        json_set(ret, "state", agent);
        json_free(messages);
        json_free(compressed);
        ret_event(log, ret);
        return log;
    }
    const json_t *fc = jget(in, "foreign_ceiling");
    int fc_raises = jtruth(jget(fc, "raises"));
    json_t *fce = act("active_watermark");
    set_bool(fce, "raised", fc_raises);
    set_val(fce, "ret", fc_raises ? NULL : jget(fc, "result"));
    push(log, fce);
    const json_t *fl = jget(in, "flush");
    json_t *fle = act("flush_messages");
    if (have_prefix) set_num(fle, "history_len", idxv->num_val);
    else set_val(fle, "history_len", NULL);
    set_bool(fle, "raised", jtruth(jget(fl, "raises")));
    push(log, fle);
    const json_t *prof = jget(in, "profile");
    const char *profile_raw = jtruth(jget(prof, "raises")) ? NULL : jstrv(jget(prof, "result"));
    json_t *pre = act("active_profile");
    set_bool(pre, "raised", jtruth(jget(prof, "raises")));
    set_str(pre, "ret", profile_raw);
    const char *profile = profile_raw;
    if (profile && strcmp(profile, "default") == 0) profile = NULL;
    push(log, pre);
    const char *old_title = jgets(in, "title");
    json_t *te = act("get_title");
    set_str(te, "ret", old_title[0] ? old_title : NULL);
    push(log, te);
    const char *new_sid = jgets(in, "mint");
    json_t *me = act("mint_session_id");
    set_str(me, "ret", new_sid);
    push(log, me);
    json_t *cse = act("compression_child_source");
    set_str(cse, "ret", jgets(in, "child_source"));
    push(log, cse);
    json_t *ye = act("with_session_yolo");
    set_val(ye, "ret", jget(in, "yolo"));
    push(log, ye);

    const json_t *lease = jget(in, "lease");
    const char *holder = jgets(lease, "holder");
    int has_holder = holder[0] != 0;
    const json_t *wm = jget(lease, "watermark");
    json_t *terms = json_object();
    set_str(terms, "compression_lock_holder", has_holder ? holder : NULL);
    set_bool(terms, "require_compression_lease", has_holder);
    set_bool(terms, "require_lease_refresh", has_holder);
    set_num(terms, "lease_ttl_seconds", json_get_num(lease, "ttl", 300.0));
    json_set(terms, "watermark", (!fc_raises && jget(fc, "result") && jget(fc, "result")->type != JSON_NULL)
                                     ? json_copy(wm) : json_null());
    set_val(terms, "watermark_ceiling", fc_raises ? NULL : jget(fc, "result"));
    const json_t *pub = jget(in, "publish");
    json_t *pube = act("publish_compression_child");
    set_str(pube, "via", jtruth(jget(in, "store_present")) ? "store" : "db");
    set_str(pube, "new_session_id", new_sid);
    json_set(pube, "lease_terms", terms);
    set_bool(pube, "raised", jtruth(jget(pub, "raises")));
    push(log, pube);
    if (jtruth(jget(pub, "raises"))) {
        set_str(ret, "raised", jgets(pub, "raises_kind"));
        json_set(ret, "state", agent);
        json_free(messages);
        json_free(compressed);
        ret_event(log, ret);
        return log;
    }
    const char *outcome = jgets(in, "compressed_user_turn_outcome");
    if (strcmp(outcome, "inserted") == 0 || strcmp(outcome, "merged") == 0) {
        const json_t *flags = jget(in, "real_user_flags");
        long long anchor = -1;
        if (messages && messages->type == JSON_ARRAY && flags && flags->type == JSON_ARRAY) {
            for (size_t i = messages->c.count; i > 0; i--) {
                if (i - 1 < flags->c.count && jtruth(flags->c.items[i - 1])) {
                    anchor = (long long)(i - 1);
                    break;
                }
            }
        }
        if (anchor >= 0 && messages->c.items[anchor]->type == JSON_OBJECT) {
            json_t *src = messages->c.items[anchor];
            json_set(src, CCFLOW_DB_MARKER, json_bool(true));
            if (jtruth(jget(in, "session_messages_separate"))) {
                json_t *sm = json_copy(jget(in, "session_messages"));
                json_t *tw = json_object();
                json_set(tw, "targets", sm);
                json_set(tw, "source", json_copy(src));
                set_bool(tw, "exact_counts_stamped", 1);
                json_t *stamped = ccflow_stamp_scoped_twins(tw);
                json_free(tw);
                json_set(ret, "session_messages", stamped);
            }
        }
    }
    if (compressed && compressed->type == JSON_ARRAY) {
        for (size_t i = 0; i < compressed->c.count; i++) {
            json_t *m = compressed->c.items[i];
            if (m && m->type == JSON_OBJECT)
                json_set(m, CCFLOW_DB_MARKER, json_bool(true));
        }
    }
    json_set(agent, "session_id", json_string(new_sid));
    json_set(agent, "_db_flush_scan_prefix", json_null());
    json_t *rbe = act("rebind_session_context");
    set_str(rbe, "session_id", new_sid);
    push(log, rbe);
    json_set(agent, "_session_db_created", json_bool(true));
    json_t *cae = act("carry_session_state");
    set_str(cae, "old_session_id", old_sid);
    set_str(cae, "title", old_title[0] ? old_title : NULL);
    push(log, cae);
    json_set(ret, "state", agent);
    json_set(ret, "compressed", compressed);
    json_free(messages);
    ret_event(log, ret);
    return log;
}

/* _carry_session_state_to_child: automation, rejected thinking, title +
 * provenance. SEAMs: the carry calls and title reads/writes. */
/* PoP: ccflow_carry_session_state_to_child @ agent/conversation_compression.py:_carry_session_state_to_child */
json_t *ccflow_carry_session_state_to_child(const json_t *in)
{
    json_t *log = json_array();
    json_t *ret = json_object();
    const char *old_sid = jgets(in, "old_session_id");
    json_t *ae = act("carry_automation");
    set_str(ae, "old_session_id", old_sid);
    set_bool(ae, "raised", jtruth(jget(in, "automation_raises")));
    push(log, ae);
    if (jtruth(jget(in, "automation_raises"))) {
        set_str(ret, "raised", "Exception");
        ret_event(log, ret);
        return log;
    }
    json_t *re = act("carry_rejected_thinking");
    set_str(re, "old_session_id", old_sid);
    set_bool(re, "raised", jtruth(jget(in, "thinking_raises")));
    push(log, re);
    const char *old_title = jgets(in, "old_title");
    if (!old_title[0]) {
        ret_event(log, ret);
        return log;
    }
    const json_t *ts = jget(in, "title_source");
    int ts_raises = jtruth(jget(ts, "raises"));
    const char *src = ts_raises ? NULL : jstrv(jget(ts, "result"));
    json_t *tse = act("get_title_source");
    set_str(tse, "via", jtruth(jget(in, "store_present")) ? "store" : "db");
    set_bool(tse, "raised", ts_raises);
    set_str(tse, "ret", src);
    push(log, tse);
    const json_t *st = jget(in, "set_title");
    int st_raises = jtruth(jget(st, "raises"));
    json_t *ste = act("set_title");
    set_str(ste, "sid", jgets(in, "new_session_id"));
    set_str(ste, "title", old_title);
    set_bool(ste, "raised", st_raises);
    push(log, ste);
    if (st_raises) {
        ret_event(log, ret);
        return log;
    }
    if (src) {
        json_t *sse = act("set_title_source");
        set_str(sse, "sid", jgets(in, "new_session_id"));
        set_str(sse, "source", src);
        set_bool(sse, "raised", jtruth(jget(in, "set_source_raises")));
        push(log, sse);
    }
    ret_event(log, ret);
    return log;
}

/* _route_codex_compaction: codex owns the real thread under the commit fence
 * bracket. SEAMs: fence begin/finish, restore, telemetry, codex compact. */
/* PoP: ccflow_route_codex_compaction @ agent/conversation_compression.py:_route_codex_compaction */
json_t *ccflow_route_codex_compaction(const json_t *in)
{
    json_t *log = json_array();
    json_t *ret = json_object();
    int fence_present = jtruth(jget(in, "fence_present"));
    json_t *v = json_array();
    if (fence_present) {
        int admitted = jtruth(jget(in, "begin_commit"));
        json_t *bc = act("begin_commit");
        set_bool(bc, "ret", admitted);
        push(log, bc);
        if (!admitted) {
            push(log, act("restore_compressor"));
            json_t *tel = act("telemetry_bypassed");
    set_val(tel, "route", NULL);
            set_str(tel, "commit_status", "aborted");
            set_str(tel, "failure_class", "commit_fence_cancelled");
            set_val(tel, "approx_tokens", jget(in, "approx_tokens"));
            set_str(tel, "route", "codex_app_server");
            set_val(tel, "seed", jget(in, "seed"));
            push(log, tel);
            const json_t *cached = jget(in, "cached");
            int built = !jtruth(cached);
            const char *prompt = built ? jgets(in, "build_result")
                                       : (cached->type == JSON_STRING ? cached->str_val : "");
            json_t *ep = act("existing_system_prompt");
            set_bool(ep, "built", built);
            set_str(ep, "ret", prompt);
            push(log, ep);
            json_append(v, json_copy(jget(in, "messages")));
            json_append(v, json_string(prompt));
            json_set(ret, "v", v);
            ret_event(log, ret);
            return log;
        }
    }
    const json_t *cx = jget(in, "codex");
    const char *cx_raises = jgets(cx, "raises");
    json_t *ce = act("codex_compact");
    set_str(ce, "raised", cx_raises[0] ? cx_raises : NULL);
    set_val(ce, "ret", cx_raises[0] ? NULL : jget(cx, "result"));
    push(log, ce);
    if (fence_present)
        push(log, act("finish_commit"));
    if (cx_raises[0]) {
        set_str(ret, "raised", cx_raises);
    } else {
        json_append(v, json_copy(jget(jget(cx, "result"), "messages")));
        json_append(v, json_copy(jget(jget(cx, "result"), "prompt")));
        json_set(ret, "v", v);
    }
    ret_event(log, ret);
    return log;
}
/* ===========================================================================
 * Stall-fallback ladder + progress-aware worker waits (budget branches).
 * Clock / future / fence observations are scripted per-iteration ticks.
 * =========================================================================== */

/* _stall_retry_routes: configured chain entry first, then (once a stall
 * backoff burned a window) the deterministic fallback summary.
 * SEAM: resolve_compression_fallback_route. */
/* PoP: ccflow_stall_retry_routes @ agent/conversation_compression.py:_stall_retry_routes */
json_t *ccflow_stall_retry_routes(const json_t *in)
{
    json_t *log = json_array();
    const json_t *chain = jget(in, "chain_route");
    json_t *re = act("resolve_fallback_route");
    set_val(re, "ret", chain);
    push(log, re);
    json_t *routes = json_array();
    if (chain && chain->type != JSON_NULL)
        json_append(routes, json_copy(chain));
    if (jtruth(jget(in, "escalate_deterministic"))) {
        json_t *det = json_object();
        set_str(det, "label", "deterministic fallback summary");
        set_bool(det, "deterministic", 1);
        json_append(routes, det);
    }
    json_t *ret = json_object();
    json_set(ret, "v", routes);
    ret_event(log, ret);
    return log;
}

/* _retry_compression_on_fallback_chain: an explicit stop is not a stalled
 * route; otherwise walk the pinned routes until one recovers.
 * SEAMs: hard-cancel event, _stall_retry_routes, _run_pinned_compression_retry. */
/* PoP: ccflow_retry_compression_on_fallback_chain @ agent/conversation_compression.py:_retry_compression_on_fallback_chain */
json_t *ccflow_retry_compression_on_fallback_chain(const json_t *in)
{
    json_t *log = json_array();
    json_t *ret = json_object();
    if (jtruth(jget(in, "hard_cancel_present"))) {
        int set = jtruth(jget(in, "hard_cancel_set"));
        json_t *hc = act("hard_cancel_check");
        set_bool(hc, "set", set);
        push(log, hc);
        if (set) {
            set_val(ret, "v", NULL);
            ret_event(log, ret);
            return log;
        }
    }
    const json_t *routes = jget(in, "routes");
    json_t *sr = act("stall_retry_routes");
    set_bool(sr, "escalate", jtruth(jget(in, "escalate_deterministic")));
    set_val(sr, "ret", routes);
    push(log, sr);
    const json_t *results = jget(in, "pinned_results");
    if (routes && routes->type == JSON_ARRAY) {
        for (size_t i = 0; i < routes->c.count; i++) {
            const json_t *route = routes->c.items[i];
            const json_t *recovered = (results && results->type == JSON_ARRAY)
                                          ? json_get(results, i) : NULL;
            json_t *pe = act("pinned_retry");
            set_str(pe, "label", jgets(route, "label"));
            set_val(pe, "ret", recovered);
            push(log, pe);
            if (recovered && recovered->type != JSON_NULL) {
                json_set(ret, "v", json_copy(recovered));
                ret_event(log, ret);
                return log;
            }
        }
    }
    set_val(ret, "v", NULL);
    ret_event(log, ret);
    return log;
}

/* _run_pinned_compression_retry: one bounded re-run with the route pinned.
 * Summary retries spend only the primary's REMAINING pre-commit budget; the
 * deterministic route keeps its own (over-window last resort).
 * SEAMs: clock, new_fence factory, pin_summary_route,
 *        run_compress_context_with_progress_timeout, streak reads. */
/* PoP: ccflow_run_pinned_compression_retry @ agent/conversation_compression.py:_run_pinned_compression_retry */
json_t *ccflow_run_pinned_compression_retry(const json_t *in)
{
    json_t *log = json_array();
    json_t *ret = json_object();
    const json_t *route = jget(in, "route");
    const char *label = jgets(route, "label");
    int deterministic = jtruth(jget(route, "deterministic"));
    double now = json_get_num(in, "now", 0);
    double precommit = json_get_num(in, "precommit_deadline", 0);
    double idle_timeout = json_get_num(in, "idle_timeout_seconds", 0);
    double total_ceiling = json_get_num(in, "total_ceiling_seconds", 0);

    if (!deterministic) {
        int spent = now >= precommit;
        json_t *bc = act("budget_check");
        set_num(bc, "now", now);
        set_num(bc, "deadline", precommit);
        set_bool(bc, "spent", spent);
        push(log, bc);
        if (spent) {
            json_t *ls = act("log_skip");
            set_str(ls, "reason", "ceiling_spent");
            set_str(ls, "label", label);
            push(log, ls);
            set_val(ret, "v", NULL);
            ret_event(log, ret);
            return log;
        }
    }
    const json_t *nf = jget(in, "new_fence");
    int nf_present = jtruth(jget(nf, "present"));
    int nf_raises = jtruth(jget(nf, "raises"));
    int nf_valid = !nf_raises && jtruth(jget(nf, "valid"));
    json_t *nfe = act("new_fence");
    set_bool(nfe, "present", nf_present);
    set_bool(nfe, "raised", nf_raises);
    set_bool(nfe, "valid", nf_valid);
    push(log, nfe);
    if (nf_present && nf_raises)
        push(log, act("log_fence_factory_failed"));
    if (!nf_valid)
        push(log, act("log_unpublished_fence"));

    double idle = jtruth(jget(route, "timeout")) ? json_get_num(route, "timeout", 0)
                                                 : idle_timeout;
    double ceiling;
    double deadline = 0;
    int have_deadline = 0;
    if (deterministic) {
        ceiling = total_ceiling > idle ? total_ceiling : idle;
    } else {
        ceiling = precommit - now;
        deadline = precommit;
        have_deadline = 1;
        if (ceiling <= 0) {
            push(log, act("revoke_commit_admission"));
            json_t *ls = act("log_skip");
            set_str(ls, "reason", "setup_spent");
            set_str(ls, "label", label);
            push(log, ls);
            set_val(ret, "v", NULL);
            ret_event(log, ret);
            return log;
        }
        if (idle > ceiling) idle = ceiling;
    }
    if (deterministic) {
        json_t *ld = act("log_deterministic_commit");
        set_str(ld, "label", label);
        push(log, ld);
    } else {
        json_t *lr = act("log_retry");
        set_str(lr, "label", label);
        set_str(lr, "model", jgets(route, "model"));
        set_num(lr, "ceiling", ceiling);
        push(log, lr);
    }
    json_t *pin = act("pin_summary_route");
    set_str(pin, "label", label);
    push(log, pin);
    const json_t *run = jget(in, "run");
    int run_raises = jtruth(jget(run, "raises"));
    json_t *re = act("run_with_timeout");
    set_num(re, "idle", idle);
    set_num(re, "ceiling", ceiling);
    set_bool(re, "stall_fallback", 0);
    if (have_deadline) set_num(re, "deadline", deadline);
    else set_val(re, "deadline", NULL);
    set_bool(re, "raised", run_raises);
    push(log, re);
    if (run_raises) {
        json_t *lf = act("log_fallback_failed");
        set_str(lf, "label", label);
        push(log, lf);
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    if (jtruth(jget(run, "same_list"))) {
        json_t *ln = act("log_no_compression");
        set_str(ln, "label", label);
        push(log, ln);
        set_val(ret, "v", NULL);
        ret_event(log, ret);
        return log;
    }
    double streak_before = json_get_num(in, "streak_before", 0);
    double streak_after = json_get_num(in, "streak_after", 0);
    int before_int = jtruth(jget(in, "streak_before_int"));
    int after_int = jtruth(jget(in, "streak_after_int"));
    int det_fallback = deterministic || (before_int && after_int && streak_after > streak_before);
    json_t *ve = act("recovery_verdict");
    set_str(ve, "kind", det_fallback ? "deterministic_fallback" : "recovered");
    set_str(ve, "label", label);
    push(log, ve);
    json_set(ret, "v", json_copy(jget(run, "result")));
    ret_event(log, ret);
    return log;
}

/* _join_cancelled_worker: bounded grace join; a worker that DIED with a
 * timeout-class error has exited (future.done()), a cancelled one never ran.
 * SEAM: future.result outcome. */
/* PoP: ccflow_join_cancelled_worker @ agent/conversation_compression.py:_join_cancelled_worker */
json_t *ccflow_join_cancelled_worker(const json_t *in)
{
    json_t *log = json_array();
    double grace = 0.0;
    const json_t *gv = jget(in, "grace");
    const char *gk = jgets(in, "grace_kind");
    if (strcmp(gk, "int") == 0 || strcmp(gk, "float") == 0) {
        grace = gv ? gv->num_val : 0.0;
        if (grace < 0.0) grace = 0.0;
    } else if (strcmp(gk, "bool") == 0) {
        grace = (gv && gv->type == JSON_BOOL && gv->bool_val) ? 1.0 : 0.0;
    } else if (strcmp(gk, "str") == 0) {
        const char *s = jstrv(gv);
        if (s) {
            char *end = NULL;
            double d = strtod(s, &end);
            if (end && end != s) {
                while (py_isspace(*end)) end++;
                if (*end == '\0') {
                    grace = d;
                    if (grace < 0.0) grace = 0.0;
                }
            }
        }
    }
    const char *outcome = jgets(in, "outcome");
    int done = jtruth(jget(in, "done"));
    json_t *fe = act("future_result");
    set_num(fe, "grace", grace);
    set_str(fe, "outcome", outcome);
    push(log, fe);
    int settled;
    if (strcmp(outcome, "ok") == 0) settled = 1;
    else if (strcmp(outcome, "timeout") == 0) settled = done;
    else if (strcmp(outcome, "cancelled") == 0) settled = 1;
    else settled = 1;  /* exception: the fence keeps the failed attempt out */
    json_t *ret = json_object();
    set_bool(ret, "v", settled);
    ret_event(log, ret);
    return log;
}

/* _await_worker_within_budget: poll under idle budget + moving ceiling.
 * SEAM: scripted per-iteration clock/fence/future ticks. */
/* PoP: ccflow_await_worker_within_budget @ agent/conversation_compression.py:_await_worker_within_budget */
json_t *ccflow_await_worker_within_budget(const json_t *in)
{
    json_t *log = json_array();
    double idle = json_get_num(in, "idle", 0);
    double ceiling = json_get_num(in, "ceiling", 0);
    double wait_started = json_get_num(in, "wait_started", 0);
    const json_t *ticks = jget(in, "ticks");
    json_t *ret = json_object();
    size_t n = (ticks && ticks->type == JSON_ARRAY) ? ticks->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *t = ticks->c.items[i];
        double now = json_get_num(t, "now", 0);
        const json_t *dl = jget(t, "deadline");
        int have_dl = dl && dl->type == JSON_NUMBER;
        double base = have_dl ? dl->num_val : wait_started + ceiling;
        double remaining_ceiling = base - now;
        if (remaining_ceiling <= 0) {
            json_t *pe = act("poll");
            set_num(pe, "remaining_ceiling", remaining_ceiling);
            push(log, pe);
            set_bool(ret, "settled", 0);
            set_val(ret, "result", NULL);
            ret_event(log, ret);
            return log;
        }
        double since_progress = json_get_num(t, "since_progress", 0);
        double wait_slice = idle - since_progress;
        if (wait_slice < 0.005) wait_slice = 0.005;
        if (wait_slice > remaining_ceiling) wait_slice = remaining_ceiling;
        json_t *pe = act("poll");
        set_num(pe, "remaining_ceiling", remaining_ceiling);
        set_num(pe, "since_progress", since_progress);
        set_num(pe, "wait_slice", wait_slice);
        push(log, pe);
        const char *outcome = jgets(t, "outcome");
        if (strcmp(outcome, "result") == 0) {
            set_bool(ret, "settled", 1);
            set_val(ret, "result", jget(t, "result"));
            ret_event(log, ret);
            return log;
        }
        /* TimeoutError leg */
        int done = jtruth(jget(t, "done"));
        if (done) {
            if (jtruth(jget(t, "exc_none"))) {
                push(log, act("settled_done"));
                set_bool(ret, "settled", 1);
                set_val(ret, "result", jget(t, "result"));
                ret_event(log, ret);
                return log;
            }
            push(log, act("log_worker_exited"));
            set_bool(ret, "settled", 0);
            set_val(ret, "result", NULL);
            ret_event(log, ret);
            return log;
        }
        double waited = now - wait_started;
        int deadline_exceeded = have_dl && now >= dl->num_val;
        if (!deadline_exceeded && since_progress < idle) {
            json_t *le = act("log_extending_wait");
            set_num(le, "waited", waited);
            set_num(le, "since_progress", since_progress);
            set_num(le, "ceiling", ceiling);
            push(log, le);
            continue;
        }
        set_bool(ret, "settled", 0);
        set_val(ret, "result", NULL);
        ret_event(log, ret);
        return log;
    }
    set_bool(ret, "settled", 0);
    set_val(ret, "result", NULL);
    ret_event(log, ret);
    return log;
}

/* _await_in_flight_commit: an admitted commit cannot be abandoned; wait in
 * bounded slices, escalating logs, surface the overrun once.
 * SEAM: scripted clock/future ticks + on_commit_overrun callback. */
/* PoP: ccflow_await_in_flight_commit @ agent/conversation_compression.py:_await_in_flight_commit */
json_t *ccflow_await_in_flight_commit(const json_t *in)
{
    json_t *log = json_array();
    double ceiling = json_get_num(in, "ceiling", 0);
    double wait_started = json_get_num(in, "wait_started", 0);
    int callback_present = jtruth(jget(in, "callback_present"));
    const json_t *ticks = jget(in, "ticks");
    json_t *ret = json_object();
    int overrun_surfaced = 0;
    int overrun_reports = 0;
    size_t n = (ticks && ticks->type == JSON_ARRAY) ? ticks->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *t = ticks->c.items[i];
        double now = json_get_num(t, "now", 0);
        double waited = now - wait_started;
        double remaining = ceiling - waited;
        if (remaining <= 0) {
            double cap = ceiling > 0.05 ? ceiling : 0.05;
            remaining = 30.0 < cap ? 30.0 : cap;
            overrun_reports++;
            json_t *le = act("log_commit_overrun");
            set_str(le, "level", overrun_reports <= 2 ? "warning" : "error");
            set_num(le, "over", waited - ceiling);
            set_num(le, "waited", waited);
            set_num(le, "ceiling", ceiling);
            push(log, le);
            if (!overrun_surfaced && callback_present) {
                overrun_surfaced = 1;
                json_t *oe = act("on_commit_overrun");
                set_num(oe, "waited", waited);
                set_num(oe, "ceiling", ceiling);
                set_bool(oe, "raised", jtruth(jget(in, "callback_raises")));
                push(log, oe);
            }
        }
        json_t *pe = act("commit_poll");
        set_num(pe, "waited", waited);
        set_num(pe, "remaining", remaining);
        push(log, pe);
        const char *outcome = jgets(t, "outcome");
        if (strcmp(outcome, "result") == 0) {
            set_val(ret, "result", jget(t, "result"));
            ret_event(log, ret);
            return log;
        }
        if (jtruth(jget(t, "done"))) {
            push(log, act("settled_done"));
            set_val(ret, "result", jget(t, "result"));
            ret_event(log, ret);
            return log;
        }
    }
    set_val(ret, "result", NULL);
    ret_event(log, ret);
    return log;
}

/* _cancel_or_join_worker: cancel pre-commit; False when an admitted commit
 * owns the fence. SEAM: scripted fence observations + sleep. */
/* PoP: ccflow_cancel_or_join_worker @ agent/conversation_compression.py:_cancel_or_join_worker */
json_t *ccflow_cancel_or_join_worker(const json_t *in)
{
    json_t *log = json_array();
    const json_t *steps = jget(in, "steps");
    json_t *ret = json_object();
    size_t n = (steps && steps->type == JSON_ARRAY) ? steps->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *s = steps->c.items[i];
        int in_flight = jtruth(jget(s, "in_flight"));
        const json_t *tc = jget(s, "try_cancel");
        json_t *pe = act("cancel_poll");
        set_bool(pe, "in_flight", in_flight);
        set_val(pe, "try_cancel", tc);
        push(log, pe);
        if (in_flight) {
            set_bool(ret, "v", 0);
            ret_event(log, ret);
            return log;
        }
        if (tc && tc->type != JSON_NULL) {
            set_bool(ret, "v", jtruth(tc));
            ret_event(log, ret);
            return log;
        }
        json_t *sl = act("sleep");
        set_num(sl, "seconds", 0.025);
        push(log, sl);
    }
    set_bool(ret, "v", 0);
    ret_event(log, ret);
    return log;
}

/* _release_cancelled_worker: total-ceiling only — bounded grace join, then
 * the holder-qualified lease release. SEAM: join result. */
/* PoP: ccflow_release_cancelled_worker @ agent/conversation_compression.py:_release_cancelled_worker */
json_t *ccflow_release_cancelled_worker(const json_t *in)
{
    json_t *log = json_array();
    int total_exhausted = jtruth(jget(in, "total_exhausted"));
    double ceiling = json_get_num(in, "ceiling", 0);
    if (total_exhausted) {
        double grace = 5.0 < ceiling ? 5.0 : ceiling;
        int joined = jtruth(jget(in, "join_result"));
        json_t *je = act("join_cancelled_worker");
        set_num(je, "grace", grace);
        set_bool(je, "ret", joined);
        push(log, je);
        if (joined) {
            push(log, act("allow_cancelled_lock_release"));
        } else {
            json_t *lo = act("log_orphan");
            set_num(lo, "grace", grace);
            push(log, lo);
        }
    }
    push(log, act("release_cancelled_compression_lock"));
    ret_event(log, NULL);
    return log;
}