/*
 * chat_completion_helpers_flow.c -- streaming / tool-call FLOW cores ported
 * from agent/chat_completion_helpers.py (+ the tool-call accumulator from
 * agent/chat_completion_helpers_tool_calls.py).
 *
 * These port the ORCHESTRATION logic: decision sequences, call order, branch
 * conditions, state transitions and error paths. Live I/O (network stream,
 * display callbacks, clock, retry backoff, error classification, argument
 * repair, repetition detection) is injected through SEAMS -- callback typedefs
 * and injected-result parameters backed by scripted json_t state. The decision
 * cores below never perform I/O themselves.
 *
 * Every entry takes the scripted input object `in` (borrowed) and returns a
 * fresh json_t ARRAY capturing the ordered decision sequence. Caller json_free.
 *
 * Naming follows the existing cch_* / agent_cch_* prefixes; json_t is libjson.
 * Ownership: unless noted, returned json_t* is caller-owned; `in` and values
 * pulled from it are borrowed and copied before being stored in `out`.
 */

#include "hermes_cch_flow.h"
#include "port_chat_completion_helpers.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  Small shared utilities (Python truthiness / string helpers)
 * =========================================================================== */

/* Python bool(x) over a json_t value: None/null/0/""/[]/{}/False -> 0. */
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
    return (o && o->type == JSON_OBJECT) ? json_obj_get((json_t *)o, k) : NULL;
}

/* Borrowed string attribute or "" (never NULL). */
static const char *jgets(const json_t *o, const char *k)
{
    const char *s = jstrv(jget(o, k));
    return s ? s : "";
}

static int jhas(const json_t *o, const char *k)
{
    return jget(o, k) != NULL;
}

/* Deep structural equality (used for tool-call id comparisons). */
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
            json_t *bv = json_obj_get((json_t *)b, a->c.keys[i]);
            if (!bv || !jequal(a->c.items[i], bv)) return 0;
        }
        return 1;
    }
    return 0;
}

/* Action-log helpers: each action is {"a":name, ...}. */
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

/* malloc'd join of a json array of strings with `sep`. Caller frees. */
static char *join_str_array(const json_t *arr, const char *sep)
{
    size_t seplen = sep ? strlen(sep) : 0;
    size_t cap = 1;
    size_t n = arr ? json_len((json_t *)arr) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = jstrv(json_get((json_t *)arr, i));
        cap += (s ? strlen(s) : 0) + seplen;
    }
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        const char *s = jstrv(json_get((json_t *)arr, i));
        if (!s) s = "";
        if (i && seplen) { memcpy(out + used, sep, seplen); used += seplen; }
        size_t l = strlen(s);
        memcpy(out + used, s, l); used += l;
    }
    out[used] = '\0';
    return out;
}

/* Python str.strip() for ASCII whitespace into a malloc'd copy. Caller frees. */
static char *jstrip(const char *s)
{
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n); out[n] = '\0';
    return out;
}

static char *jlower(const char *s)
{
    char *out = strdup(s ? s : "");
    if (out) for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

/* Scripted-result cursor: pops the next value from a json array seam queue.
 * Returns NULL when exhausted (callers apply a documented default). */
typedef struct { const json_t *arr; size_t i; } jcursor_t;

static const json_t *jnext(jcursor_t *c)
{
    if (!c->arr || c->arr->type != JSON_ARRAY || c->i >= json_len((json_t *)c->arr))
        return NULL;
    return json_get((json_t *)c->arr, c->i++);
}

static int jnext_bool(jcursor_t *c, int dflt)
{
    const json_t *v = jnext(c);
    return v ? jtruth(v) : dflt;
}

static char *jnext_str(jcursor_t *c, const char *dflt)
{
    const json_t *v = jnext(c);
    const char *s = jstrv(v);
    return strdup(s ? s : dflt);
}

/* ===========================================================================
 *  Pure leaves the flow calls (ported faithfully; PoP points at live origin)
 * =========================================================================== */

/* PoP: cch_flow_text_from_part @ session_rules/turns.py:_text_from_part */
static char *flow_text_from_part(const json_t *part)
{
    if (!part || part->type == JSON_NULL) return strdup("");
    if (part->type == JSON_STRING) return strdup(part->str_val ? part->str_val : "");
    static const char *const NON_TEXT[] = { "image", "image_url", "input_image",
                                            "audio", "input_audio", NULL };
    const char *type = jgets(part, "type");
    char *tl = jlower(type);
    char *tstrip = jstrip(tl);
    int non_text = 0;
    for (int i = 0; NON_TEXT[i]; i++)
        if (strcmp(tstrip, NON_TEXT[i]) == 0) non_text = 1;
    free(tl); free(tstrip);
    if (non_text) return strdup("");
    static const char *const TEXT_KEYS[] = { "text", "content", "input_text",
                                             "output_text", "summary_text", NULL };
    for (int i = 0; TEXT_KEYS[i]; i++) {
        const json_t *v = jget(part, TEXT_KEYS[i]);
        if (v && v->type == JSON_STRING)
            return strdup(v->str_val ? v->str_val : "");
    }
    return strdup("");
}

/* PoP: cch_flow_flatten_message_text @ session_rules/turns.py:flatten_message_text
 * Only the shapes the stream produces (None / str / list-of-parts / single part)
 * are exercised; the str(content) fallback for exotic objects is not reachable
 * from json_t scripted chunks. sep is "" on the streaming path. */
static char *flow_flatten_message_text(const json_t *content, const char *sep)
{
    if (!content || content->type == JSON_NULL) return strdup("");
    if (content->type == JSON_STRING)
        return strdup(content->str_val ? content->str_val : "");
    if (content->type == JSON_ARRAY) {
        /* sep.join(chunk for chunk in map(_text_from_part, content) if chunk) */
        size_t seplen = sep ? strlen(sep) : 0;
        size_t cap = 1;
        char **pieces = (char **)calloc(json_len((json_t *)content), sizeof(char *));
        size_t np = 0;
        for (size_t i = 0; i < json_len((json_t *)content); i++) {
            char *t = flow_text_from_part(json_get((json_t *)content, i));
            if (t && t[0]) { pieces[np++] = t; cap += strlen(t) + seplen; }
            else free(t);
        }
        char *out = (char *)malloc(cap);
        out[0] = '\0'; size_t used = 0;
        for (size_t i = 0; i < np; i++) {
            if (i && seplen) { memcpy(out + used, sep, seplen); used += seplen; }
            size_t l = strlen(pieces[i]);
            memcpy(out + used, pieces[i], l); used += l;
            free(pieces[i]);
        }
        out[used] = '\0';
        free(pieces);
        return out;
    }
    char *t = flow_text_from_part(content);
    if (t && t[0]) return t;
    free(t);
    return strdup("");
}

/* PoP: cch_flow_separate_glued_reasoning_blocks @ agent/reasoning_summaries.py:separate_glued_reasoning_blocks */
static char *flow_separate_glued(const char *previous, const json_t *delta)
{
    char *d = flow_flatten_message_text(delta, "");
    int glued = 0;
    if (previous && previous[0] && d && d[0]) {
        size_t pl = strlen(previous);
        int prev_last_space = isspace((unsigned char)previous[pl - 1]);
        int opens_bold = (strncmp(d, "**", 2) == 0);
        int has_close = opens_bold && strstr(d + 2, "**") != NULL;
        glued = !prev_last_space && opens_bold && has_close;
    }
    if (glued) {
        size_t dl = strlen(d);
        char *out = (char *)malloc(dl + 3);
        out[0] = '\n'; out[1] = '\n';
        memcpy(out + 2, d, dl + 1);
        free(d);
        return out;
    }
    return d;
}

/* PoP: cch_flow_normalize_finish_reason @ agent/message_sanitization.py:normalize_finish_reason */
static char *flow_normalize_finish_reason(const json_t *raw)
{
    const char *s = jstrv(raw);
    if (!s || !s[0]) {
        /* Non-string / empty passes through unchanged. */
        if (!raw || raw->type == JSON_NULL) return NULL;
        if (raw->type == JSON_STRING) return strdup(s ? s : "");
        return NULL; /* non-string non-null: caller keeps it as-is (not a str) */
    }
    char *lower = jlower(s);
    const char *folded = lower;
    if (strcmp(lower, "max_tokens") == 0) folded = "length";
    else if (strcmp(lower, "end") == 0) folded = "stop";
    else if (strcmp(lower, "function_call") == 0) folded = "tool_calls";
    char *out = strdup(folded);
    free(lower);
    return out;
}

#define FLOW_ROUTER_SHIM "Connect timeout, please try again later."

/* PoP: cch_flow_router_timeout_shim_may_follow @ agent/transports/chat_completions.py:router_timeout_shim_may_follow */
static int flow_router_shim_may_follow(const char *text)
{
    if (!text || !text[0]) return 0;
    char *ls = jstrip(text);
    int r = (strncmp(FLOW_ROUTER_SHIM, ls, strlen(ls)) == 0);
    free(ls);
    return r;
}

/* PoP: cch_flow_has_positive_completion_tokens @ agent/transports/chat_completions.py:_has_positive_completion_tokens */
static int flow_positive_completion_tokens(const json_t *usage)
{
    if (!usage) return 0;
    static const char *const F[] = { "completion_tokens", "output_tokens", NULL };
    for (int i = 0; F[i]; i++) {
        const json_t *v = jget(usage, F[i]);
        if (v && v->type == JSON_NUMBER && v->num_val > 0) return 1;
    }
    return 0;
}

/* PoP: cch_flow_is_router_timeout_shim @ agent/transports/chat_completions.py:is_router_timeout_shim */
static int flow_is_router_timeout_shim(const json_t *response)
{
    const json_t *choices = jget(response, "choices");
    if (!choices || choices->type != JSON_ARRAY || json_len((json_t *)choices) != 1)
        return 0;
    const json_t *message = jget(json_get((json_t *)choices, 0), "message");
    const char *content = jstrv(jget(message, "content"));
    if (!content) return 0;
    char *cs = jstrip(content);
    int eq = (strcmp(cs, FLOW_ROUTER_SHIM) == 0);
    free(cs);
    if (!eq) return 0;
    if (jtruth(jget(message, "tool_calls"))) return 0;
    return !flow_positive_completion_tokens(jget(response, "usage"));
}

/* PoP: cch_flow_streamed_reasoning_detail_text @ agent/reasoning_summaries.py:streamed_reasoning_detail_text */
static char *flow_detail_text(const json_t *detail)
{
    const char *dtype = jstrv(jget(detail, "type"));
    const char *key = NULL;
    if (dtype) {
        if (strcmp(dtype, "reasoning.text") == 0) key = "text";
        else if (strcmp(dtype, "reasoning.summary") == 0) key = "summary";
    }
    if (!key) return strdup("");
    const char *text = jstrv(jget(detail, key));
    return strdup(text ? text : "");
}

/* PoP: cch_flow_append_streamed_reasoning_detail @ agent/reasoning_summaries.py:append_streamed_reasoning_detail
 * details_acc is a json array of dict entries; detail is a json object. */
static void flow_append_detail(json_t *details_acc, const json_t *detail)
{
    if (!detail || detail->type != JSON_OBJECT) return;
    const char *dtype = jstrv(jget(detail, "type"));
    const char *merge_key = NULL;
    if (dtype) {
        if (strcmp(dtype, "reasoning.text") == 0) merge_key = "text";
        else if (strcmp(dtype, "reasoning.summary") == 0) merge_key = "summary";
    }
    size_t n = json_len(details_acc);
    json_t *last = n ? json_get(details_acc, n - 1) : NULL;
    const json_t *dval = jget(detail, merge_key ? merge_key : "");
    if (last && merge_key && dtype
        && strcmp(jgets(last, "type"), dtype) == 0
        && dval && dval->type == JSON_STRING) {
        const char *lastv = jgets(last, merge_key);
        const char *addv = jstrv(dval);
        size_t ll = strlen(lastv), al = strlen(addv ? addv : "");
        char *joined = (char *)malloc(ll + al + 1);
        memcpy(joined, lastv, ll);
        memcpy(joined + ll, addv ? addv : "", al + 1);
        json_set(last, merge_key, json_string(joined));
        free(joined);
        static const char *const BACKFILL[] = { "signature", "id", "format", "index", NULL };
        for (int i = 0; BACKFILL[i]; i++) {
            const json_t *lv = jget(last, BACKFILL[i]);
            const json_t *dv = jget(detail, BACKFILL[i]);
            int last_empty = !lv || lv->type == JSON_NULL
                             || (lv->type == JSON_STRING && (!lv->str_val || !lv->str_val[0]));
            int dv_present = dv && dv->type != JSON_NULL
                             && !(dv->type == JSON_STRING && (!dv->str_val || !dv->str_val[0]));
            if (last_empty && dv_present)
                json_set(last, BACKFILL[i], json_copy(dv));
        }
        return;
    }
    json_append(details_acc, json_copy(detail));
}

/* PoP: cch_flow_provider_error_body @ agent/chat_completion_helpers.py:_provider_error_body */
static json_t *flow_provider_error_body(const json_t *payload, int status_code)
{
    if (!payload || payload->type != JSON_OBJECT) payload = NULL;
    if (payload) {
        const json_t *err = jget(payload, "error");
        if (err && err->type == JSON_OBJECT) return json_copy((json_t *)payload);
    }
    /* code = payload.code or error_code or type or HTTP_<status> or default */
    const char *code = NULL;
    char code_buf[64];
    if (payload) {
        const json_t *c = jget(payload, "code");
        if (jtruth(c)) code = jstrv(c);
        if (!code) { const json_t *ec = jget(payload, "error_code"); if (jtruth(ec)) code = jstrv(ec); }
        if (!code) { const json_t *t = jget(payload, "type"); if (jtruth(t)) code = jstrv(t); }
    }
    if (!code) {
        if (status_code > 0) { snprintf(code_buf, sizeof(code_buf), "HTTP_%d", status_code); code = code_buf; }
        else code = "provider_stream_error";
    }
    const char *message = NULL;
    if (payload) {
        const json_t *m = jget(payload, "message");
        if (jtruth(m)) message = jstrv(m);
        if (!message) { const json_t *ed = jget(payload, "error_description"); if (jtruth(ed)) message = jstrv(ed); }
        if (!message) { const json_t *e = jget(payload, "error"); if (jtruth(e)) message = jstrv(e); }
    }
    if (!message) message = "Provider stream returned an error event.";
    json_t *normalized = json_object();
    json_set(normalized, "message", json_string(message));
    if (code && code[0]) json_set(normalized, "code", json_string(code));
    if (payload) {
        static const char *const KEYS[] = { "request_id", "param", "type", NULL };
        for (int i = 0; KEYS[i]; i++) {
            const json_t *v = jget(payload, KEYS[i]);
            if (jtruth(v)) json_set(normalized, KEYS[i], json_copy((json_t *)v));
        }
    }
    json_t *out = json_object();
    json_set(out, "error", normalized);
    return out;
}

/* PoP: cch_flow_provider_stream_text_may_be_sse @ agent/chat_completion_helpers.py:_provider_stream_text_may_be_sse */
static int flow_text_may_be_sse(const char *text)
{
    if (!text) text = "";
    while (*text && isspace((unsigned char)*text)) text++;
    if (!*text) return 0;
    static const char *const SSE_FIELDS[] = { "event", "data", "id", "retry", NULL };
    /* splitlines(); trailing_newline = endswith(("\n","\r")) */
    size_t len = strlen(text);
    int trailing_newline = (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'));
    /* Count lines and iterate. We walk line by line. */
    int saw_sse_field = 0;
    const char *p = text;
    int index = 0;
    /* total lines (splitlines semantics: split on \n/\r, no trailing empty) */
    int total_lines = 0;
    {
        const char *q = text;
        while (*q) {
            total_lines++;
            while (*q && *q != '\n' && *q != '\r') q++;
            if (*q == '\r') { q++; if (*q == '\n') q++; }
            else if (*q == '\n') q++;
        }
    }
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n' && *eol != '\r') eol++;
        size_t ll = (size_t)(eol - p);
        /* line.rstrip("\r") — the line content excludes the terminator already */
        char *line = (char *)malloc(ll + 1);
        memcpy(line, p, ll); line[ll] = '\0';
        while (ll > 0 && line[ll - 1] == '\r') line[--ll] = '\0';
        if (line[0] == '\0') { free(line); goto next_line; }
        if (line[0] == ':') { saw_sse_field = 1; free(line); goto next_line; }
        {
            char *colon = strchr(line, ':');
            char *field = line;
            if (colon) *colon = '\0';
            char *fs = jstrip(field);
            char *fl = jlower(fs);
            free(fs);
            int is_field = 0;
            for (int i = 0; SSE_FIELDS[i]; i++)
                if (strcmp(fl, SSE_FIELDS[i]) == 0) is_field = 1;
            if (colon && is_field) { saw_sse_field = 1; free(fl); free(line); goto next_line; }
            int is_last_incomplete = (index == total_lines - 1) && !trailing_newline;
            int prefix_match = 0;
            if (is_last_incomplete)
                for (int i = 0; SSE_FIELDS[i]; i++)
                    if (strncmp(SSE_FIELDS[i], fl, strlen(fl)) == 0) prefix_match = 1;
            free(fl); free(line);
            if (is_last_incomplete && prefix_match) return 1;
            return 0;
        }
    next_line:
        index++;
        p = eol;
        if (*p == '\r') { p++; if (*p == '\n') p++; }
        else if (*p == '\n') p++;
    }
    return saw_sse_field;
}

/* PoP: cch_flow_format_provider_stream_error_message @ agent/chat_completion_helpers.py:_format_message */
static char *flow_pse_format_message(int status_code, const json_t *body)
{
    const json_t *error_obj = jget(body, "error");
    if (!error_obj || error_obj->type != JSON_OBJECT) error_obj = NULL;
    /* parts = ["Provider stream returned an error event"] (+HTTP, +code) */
    char head[256];
    snprintf(head, sizeof(head), "Provider stream returned an error event");
    char http_part[64] = "";
    if (status_code > 0) snprintf(http_part, sizeof(http_part), "HTTP %d", status_code);
    const char *code = error_obj ? jstrv(jget(error_obj, "code")) : NULL;
    int code_truthy = error_obj ? jtruth(jget(error_obj, "code")) : 0;
    size_t cap = strlen(head) + strlen(http_part) + (code ? strlen(code) : 0) + 16;
    char *text = (char *)malloc(cap);
    text[0] = '\0';
    strcat(text, head);
    if (status_code > 0) { strcat(text, " - "); strcat(text, http_part); }
    if (code_truthy && code) { strcat(text, " - "); strcat(text, code); }
    const char *msg = error_obj ? jstrv(jget(error_obj, "message")) : NULL;
    if (error_obj && jtruth(jget(error_obj, "message")) && msg) {
        size_t need = strlen(text) + strlen(msg) + 4;
        char *grown = (char *)realloc(text, need);
        if (grown) { text = grown; strcat(text, ": "); strcat(text, msg); }
    }
    return text;
}

/* ===========================================================================
 *  _ToolCallAccumulator  (agent/chat_completion_helpers_tool_calls.py)
 *
 *  Slot index -> entry dict. Ollama-compatible endpoints reuse index 0 for a
 *  parallel batch (distinguished by id); a new id at a seen raw index is
 *  redirected to a fresh slot. Argument deltas are buffered per slot and joined
 *  once in materialize. Structural closure is tracked incrementally.
 * =========================================================================== */

typedef struct {
    json_t *acc;                 /* object: slot(str) -> entry */
    json_t *argument_parts;      /* object: slot -> array of str */
    json_t *arg_scan;            /* object: slot -> {done,depth,in_string,escaped,started} */
    json_t *notified;            /* array of slot numbers already announced */
    json_t *last_id_at_idx;      /* object: raw_idx -> last delta_id (json value) */
    json_t *active_slot_by_idx;  /* object: raw_idx -> current slot number */
} tc_acc_t;

static void tc_init(tc_acc_t *t)
{
    t->acc = json_object();
    t->argument_parts = json_object();
    t->arg_scan = json_object();
    t->notified = json_array();
    t->last_id_at_idx = json_object();
    t->active_slot_by_idx = json_object();
}

static void tc_free(tc_acc_t *t)
{
    json_free(t->acc); json_free(t->argument_parts); json_free(t->arg_scan);
    json_free(t->notified); json_free(t->last_id_at_idx); json_free(t->active_slot_by_idx);
}

static void slot_key(int slot, char *buf, size_t cap) { snprintf(buf, cap, "%d", slot); }

static int tc_max_slot(const tc_acc_t *t)
{
    int mx = -1;
    for (size_t i = 0; i < t->acc->c.count; i++) {
        int v = atoi(t->acc->c.keys[i]);
        if (v > mx) mx = v;
    }
    return mx;
}

static int tc_notified(const tc_acc_t *t, int slot)
{
    for (size_t i = 0; i < json_len(t->notified); i++) {
        const json_t *v = json_get(t->notified, i);
        if (v && v->type == JSON_NUMBER && (int)v->num_val == slot) return 1;
    }
    return 0;
}

/* PoP: cch_flow_tc_arguments_closed @ agent/chat_completion_helpers_tool_calls.py:_arguments_closed */
static int tc_arguments_closed(tc_acc_t *t, int slot)
{
    char key[32]; slot_key(slot, key, sizeof(key));
    json_t *parts = json_obj_get(t->argument_parts, key);
    json_t *scan = json_obj_get(t->arg_scan, key);
    int done = 0, depth = 0, in_string = 0, escaped = 0, started = 0;
    if (scan) {
        done = (int)json_get_num(scan, "done", 0);
        depth = (int)json_get_num(scan, "depth", 0);
        in_string = json_get_bool(scan, "in_string", false) ? 1 : 0;
        escaped = json_get_bool(scan, "escaped", false) ? 1 : 0;
        started = json_get_bool(scan, "started", false) ? 1 : 0;
    }
    int nparts = parts ? (int)json_len(parts) : 0;
    for (int pi = done; pi < nparts; pi++) {
        const char *s = jstrv(json_get(parts, (size_t)pi));
        if (!s) continue;
        for (const char *ch = s; *ch; ch++) {
            if (in_string) {
                if (escaped) escaped = 0;
                else if (*ch == '\\') escaped = 1;
                else if (*ch == '"') in_string = 0;
            } else if (*ch == '"') in_string = 1;
            else if (*ch == '{' || *ch == '[') { depth++; started = 1; }
            else if (*ch == '}' || *ch == ']') depth--;
        }
    }
    json_t *ns = json_object();
    json_set(ns, "done", json_number(nparts));
    json_set(ns, "depth", json_number(depth));
    json_set(ns, "in_string", json_bool(in_string != 0));
    json_set(ns, "escaped", json_bool(escaped != 0));
    json_set(ns, "started", json_bool(started != 0));
    json_set(t->arg_scan, key, ns);
    return started && depth == 0 && !in_string;
}

/* PoP: cch_flow_tc_starts_new_call @ agent/chat_completion_helpers_tool_calls.py:_starts_new_call */
static int tc_starts_new_call(tc_acc_t *t, const json_t *tc_delta, int slot)
{
    const json_t *fn = jget(tc_delta, "function");
    const char *name = jstrv(jget(fn, "name"));
    if (!name || !name[0]) return 0;
    char key[32]; slot_key(slot, key, sizeof(key));
    const json_t *entry = json_obj_get(t->acc, key);
    const char *cur_name = jstrv(jget(jget(entry, "function"), "name"));
    if (!cur_name || strcmp(name, cur_name) != 0) return 1;
    return tc_arguments_closed(t, slot);
}

/* PoP: cch_flow_tc_feed @ agent/chat_completion_helpers_tool_calls.py:feed
 * Returns the entry name the first time a slot is complete (borrowed pointer
 * into acc), else NULL. */
static const char *tc_feed(tc_acc_t *t, const json_t *tc_delta)
{
    const json_t *ridx = jget(tc_delta, "index");
    int raw_idx = (ridx && ridx->type == JSON_NUMBER) ? (int)ridx->num_val : 0;
    const json_t *tc_id = jget(tc_delta, "id");           /* may be str/int/null */
    /* delta_id = tc_id or ""  (original value, before int->str) */
    json_t *delta_id = jtruth(tc_id) ? json_copy((json_t *)tc_id) : json_string("");
    /* if isinstance(tc_id, int): tc_id = str(tc_id) */
    char tc_id_norm_buf[64];
    const char *tc_id_norm = NULL;
    if (tc_id && tc_id->type == JSON_NUMBER) {
        snprintf(tc_id_norm_buf, sizeof(tc_id_norm_buf), "%lld", (long long)tc_id->num_val);
        tc_id_norm = tc_id_norm_buf;
    } else if (tc_id && tc_id->type == JSON_STRING) {
        tc_id_norm = tc_id->str_val;
    }

    char rkey[32]; slot_key(raw_idx, rkey, sizeof(rkey));
    /* setdefault(raw_idx, raw_idx) */
    if (!json_obj_get(t->active_slot_by_idx, rkey))
        json_set(t->active_slot_by_idx, rkey, json_number(raw_idx));

    json_t *last_id = json_obj_get(t->last_id_at_idx, rkey);
    int active_slot;
    {
        json_t *asv = json_obj_get(t->active_slot_by_idx, rkey);
        active_slot = asv ? (int)asv->num_val : raw_idx;
    }
    if (jtruth(delta_id) && last_id && !jequal(delta_id, last_id)
        && tc_starts_new_call(t, tc_delta, active_slot)) {
        int new_slot = tc_max_slot(t) + 1;
        json_set(t->active_slot_by_idx, rkey, json_number(new_slot));
        active_slot = new_slot;
    }
    if (jtruth(delta_id))
        json_set(t->last_id_at_idx, rkey, json_copy(delta_id));
    int idx = active_slot;

    char key[32]; slot_key(idx, key, sizeof(key));
    json_t *entry = json_obj_get(t->acc, key);
    if (!entry) {
        entry = json_object();
        json_set(entry, "id", json_string(tc_id_norm ? tc_id_norm : ""));
        json_set(entry, "type", json_string("function"));
        json_t *fn = json_object();
        json_set(fn, "name", json_string(""));
        json_set(fn, "arguments", json_string(""));
        json_set(entry, "function", fn);
        json_set(entry, "extra_content", json_null());
        json_set(t->acc, key, entry);
    }
    json_t *parts = json_obj_get(t->argument_parts, key);
    if (!parts) { parts = json_array(); json_set(t->argument_parts, key, parts); }
    if (tc_id_norm && tc_id_norm[0])
        json_set(entry, "id", json_string(tc_id_norm));
    const json_t *tc_function = jget(tc_delta, "function");
    if (jtruth(tc_function)) {
        const char *nm = jstrv(jget(tc_function, "name"));
        if (nm && nm[0])
            json_set(jget(entry, "function"), "name", json_string(nm));
        const char *args = jstrv(jget(tc_function, "arguments"));
        if (args && args[0])
            json_append(parts, json_string(args));
        const json_t *sig = jget(tc_function, "thought_signature");
        if (sig && sig->type == JSON_STRING && sig->str_val && sig->str_val[0])
            json_set(entry, "thought_signature", json_string(sig->str_val));
    }
    const json_t *extra = jget(tc_delta, "extra_content");
    if (!extra) {
        const json_t *me = jget(tc_delta, "model_extra");
        if (me && me->type == JSON_OBJECT) extra = jget(me, "extra_content");
    }
    if (extra && extra->type != JSON_NULL)
        json_set(entry, "extra_content", json_copy((json_t *)extra));

    const char *name = jstrv(jget(jget(entry, "function"), "name"));
    json_free(delta_id);
    if (name && name[0] && !tc_notified(t, idx)) {
        json_append(t->notified, json_number(idx));
        return name;
    }
    return NULL;
}

/* PoP: cch_flow_tc_materialize @ agent/chat_completion_helpers_tool_calls.py:materialize */
static void tc_materialize(tc_acc_t *t)
{
    for (size_t i = 0; i < t->argument_parts->c.count; i++) {
        const char *key = t->argument_parts->c.keys[i];
        json_t *parts = t->argument_parts->c.items[i];
        json_t *entry = json_obj_get(t->acc, key);
        if (!entry) continue;
        char *joined = join_str_array(parts, "");
        json_set(jget(entry, "function"), "arguments", json_string(joined ? joined : ""));
        free(joined);
    }
}

/* Sorted numeric slot keys of acc (Python sorted(tool_calls_acc)). */
static int *tc_sorted_slots(const json_t *acc, int *out_n)
{
    int n = (acc && acc->type == JSON_OBJECT) ? (int)acc->c.count : 0;
    int *slots = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
    for (int i = 0; i < n; i++) slots[i] = atoi(acc->c.keys[i]);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (slots[j] < slots[i]) { int t = slots[i]; slots[i] = slots[j]; slots[j] = t; }
    *out_n = n;
    return slots;
}

/* Entry: drive feed over scripted deltas, then optionally materialize. */
json_t *cch_flow_tc_acc(const json_t *in)
{
    json_t *log = json_array();
    tc_acc_t t; tc_init(&t);
    const json_t *deltas = jget(in, "deltas");
    size_t n = deltas ? json_len((json_t *)deltas) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *d = json_get((json_t *)deltas, i);
        const char *name = tc_feed(&t, d);
        json_t *a = act("feed");
        set_str(a, "ret", name);
        push(log, a);
    }
    if (jtruth(jget(in, "materialize"))) {
        tc_materialize(&t);
        json_t *a = act("materialize");
        json_set(a, "acc", json_copy(t.acc));
        push(log, a);
    }
    tc_free(&t);
    return log;
}

/* ===========================================================================
 *  _StreamingCall._choiceless_chunk  (usage accounting / lastOne / error)
 * =========================================================================== */

/* PoP: cch_flow_choiceless_chunk @ agent/chat_completion_helpers.py:_choiceless_chunk */
json_t *cch_flow_choiceless_chunk(const json_t *in)
{
    json_t *log = json_array();
    const json_t *chunk = jget(in, "chunk");
    const json_t *fr_in = jget(in, "finish_reason");
    char *finish_reason = jstrv(fr_in) ? strdup(jstrv(fr_in)) : NULL;

    const json_t *usage = jget(chunk, "usage");
    const json_t *err_type = jget(chunk, "error_type");
    const json_t *err_msg = jget(chunk, "error_message");
    if (jtruth(err_type) || jtruth(err_msg)) {
        json_t *probe = json_object();
        json_set(probe, "code", err_type ? json_copy((json_t *)err_type) : json_null());
        json_set(probe, "message", err_msg ? json_copy((json_t *)err_msg) : json_null());
        int status = cch_status_code_from_payload(probe);
        if (status < 0) {
            int kind = err_type ? (err_type->type == JSON_NUMBER ? 1
                                   : err_type->type == JSON_STRING ? 2 : 0) : 0;
            status = cch_status_code_from_value(kind,
                err_type ? (long long)err_type->num_val : 0, jstrv(err_type));
        }
        json_free(probe);
        json_t *body_in = json_object();
        const char *code_s = jtruth(err_type) ? jstrv(err_type) : NULL;
        json_set(body_in, "code", json_string(code_s ? code_s : "provider_in_stream_error"));
        const char *msg_s = jtruth(err_msg) ? jstrv(err_msg) : NULL;
        json_set(body_in, "message", json_string(msg_s ? msg_s : ""));
        json_t *body = flow_provider_error_body(body_in, status);
        json_free(body_in);
        char raw[512];
        snprintf(raw, sizeof(raw), "%s: %s", jgets(chunk, "error_type"), jgets(chunk, "error_message"));
        char *message = flow_pse_format_message(status, body);
        json_t *a = act("raise");
        json_set(a, "kind", json_string("ProviderStreamError"));
        json_set(a, "status_code", status >= 0 ? json_number(status) : json_null());
        json_set(a, "body", body);
        json_set(a, "raw_text", json_string(raw));
        json_set(a, "message", json_string(message ? message : ""));
        free(message);
        push(log, a);
        free(finish_reason);
        return log;
    }
    const json_t *last_one = jget(chunk, "lastOne");
    if (!last_one) {
        const json_t *me = jget(chunk, "model_extra");
        if (me && me->type == JSON_OBJECT) last_one = jget(me, "lastOne");
    }
    int is_last = 0;
    if (last_one) {
        if (last_one->type == JSON_BOOL && last_one->bool_val) is_last = 1;
        else if (last_one->type == JSON_NUMBER && last_one->num_val == 1) is_last = 1;
        else if (last_one->type == JSON_STRING && jstrv(last_one)
                 && strcmp(jstrv(last_one), "true") == 0) is_last = 1;
    }
    if (is_last && !finish_reason) finish_reason = strdup("stop");

    json_t *a = act("choiceless");
    json_set(a, "usage", (usage && jtruth(usage)) ? json_copy((json_t *)usage) : json_null());
    set_str(a, "finish_reason", finish_reason);
    push(log, a);
    free(finish_reason);
    return log;
}

/* ===========================================================================
 *  _StreamingCall._assemble_tool_calls  (materialize + truncated/repair flags)
 * =========================================================================== */

/* PoP: cch_flow_assemble_tool_calls @ agent/chat_completion_helpers.py:_assemble_tool_calls */
json_t *cch_flow_assemble_tool_calls(const json_t *in)
{
    json_t *log = json_array();
    const json_t *acc = jget(in, "acc");
    const json_t *fr_in = jget(in, "finish_reason");
    const char *finish_reason = jstrv(fr_in);   /* NULL == None */
    jcursor_t repair = { jget(in, "repair_results"), 0 };
    jcursor_t repetition = { jget(in, "repetition"), 0 };

    json_t *mock = json_array();
    int has_truncated = 0;
    int n = 0;
    int *slots = tc_sorted_slots(acc, &n);
    for (int si = 0; si < n; si++) {
        char key[32]; slot_key(slots[si], key, sizeof(key));
        const json_t *tc = json_obj_get((json_t *)acc, key);
        const json_t *fn = jget(tc, "function");
        const char *orig_args = jgets(fn, "arguments");
        char *arguments = strdup(orig_args);
        const char *name = jgets(fn, "name");
        (void)name;
        char *args_stripped = jstrip(arguments);
        if (arguments[0] && args_stripped[0]) {
            json_t *parsed = json_parse(arguments, NULL);
            if (!parsed) {
                if (!finish_reason) {
                    has_truncated = 1;
                } else {
                    char *repaired = jnext_str(&repair, "{}");
                    if (strcmp(repaired, "{}") != 0) {
                        free(arguments); arguments = strdup(repaired);
                    } else {
                        has_truncated = 1;
                    }
                    free(repaired);
                }
            } else {
                json_free(parsed);
            }
            if (!finish_reason) {
                if (jnext_bool(&repetition, 0)) has_truncated = 1;
            }
        } else if (!finish_reason) {
            has_truncated = 1;
        }
        free(args_stripped);
        json_t *m = json_object();
        const json_t *tc_id = jget(tc, "id");
        json_set(m, "id", tc_id ? json_copy((json_t *)tc_id) : json_string(""));
        const json_t *tc_type = jget(tc, "type");
        json_set(m, "type", tc_type ? json_copy((json_t *)tc_type) : json_string("function"));
        const json_t *extra = jget(tc, "extra_content");
        json_set(m, "extra_content", extra ? json_copy((json_t *)extra) : json_null());
        json_t *mfn = json_object();
        json_set(mfn, "name", json_string(name));
        json_set(mfn, "arguments", json_string(arguments));
        json_set(mfn, "args_repaired", json_bool(strcmp(arguments, orig_args) != 0));
        const json_t *sig = jget(tc, "thought_signature");
        json_set(mfn, "thought_signature", sig ? json_copy((json_t *)sig) : json_null());
        json_set(m, "function", mfn);
        json_append(mock, m);
        free(arguments);
    }
    free(slots);
    json_t *a = act("assemble");
    json_set(a, "tool_calls", json_len(mock) > 0 ? mock : json_null());
    if (json_len(mock) == 0) json_free(mock);
    json_set(a, "has_truncated", json_bool(has_truncated != 0));
    push(log, a);
    return log;
}

/* ===========================================================================
 *  _provider_stream_error_from_text  (SSE error-text -> exception decision)
 * =========================================================================== */

/* PoP: cch_flow_provider_stream_error_from_text @ agent/chat_completion_helpers.py:_provider_stream_error_from_text */
json_t *cch_flow_provider_stream_error_from_text(const json_t *in)
{
    json_t *log = json_array();
    const char *text = jgets(in, "text");
    const char *fr = jstrv(jget(in, "finish_reason"));
    if (!text || !text[0]) return log;                 /* not text -> None */
    char *fr_lower = jlower(fr ? fr : "");
    int fr_is_error = (strcmp(fr_lower, "error") == 0 || strcmp(fr_lower, "error_finish") == 0);
    free(fr_lower);
    if (!fr_is_error) return log;                      /* finish gate -> None */

    int nev = 0;
    json_t **events = cch_parse_provider_sse_events(text, &nev);
    for (int i = 0; i < nev; i++) {
        json_t *ev = events[i];
        const char *ev_name = jgets(ev, "event");
        char *ev_strip = jstrip(ev_name);
        char *ev_low = jlower(ev_strip);
        int is_error_event = (strcmp(ev_low, "error") == 0);
        free(ev_strip); free(ev_low);
        const char *data = jgets(ev, "data");
        json_t *payload = cch_json_object_from_text(data);
        if (!payload) payload = json_object();
        int ev_status = -1;
        const json_t *sc = jget(ev, "status_code");
        if (sc && sc->type == JSON_NUMBER) ev_status = (int)sc->num_val;
        int status = ev_status >= 0 ? ev_status : cch_status_code_from_payload(payload);
        int qualifies = (status >= 400) || is_error_event || cch_payload_has_error_shape(payload);
        if (qualifies) {
            json_t *body = flow_provider_error_body(payload, status);
            char *message = flow_pse_format_message(status, body);
            json_t *a = act("error");
            json_set(a, "status_code", status >= 0 ? json_number(status) : json_null());
            json_set(a, "body", body);
            json_set(a, "raw_text", json_string(text));
            json_set(a, "message", json_string(message ? message : ""));
            free(message);
            push(log, a);
            json_free(payload);
            for (int k = i; k < nev; k++) json_free(events[k]);
            free(events);
            return log;
        }
        json_free(payload);
    }
    for (int k = 0; k < nev; k++) json_free(events[k]);
    free(events);

    json_t *payload = cch_json_object_from_text(text);
    if (payload) {
        int status = cch_status_code_from_payload(payload);
        json_t *body = flow_provider_error_body(payload, status);
        char *message = flow_pse_format_message(status, body);
        json_t *a = act("error");
        json_set(a, "status_code", status >= 0 ? json_number(status) : json_null());
        json_set(a, "body", body);
        json_set(a, "raw_text", json_string(text));
        json_set(a, "message", json_string(message ? message : ""));
        free(message);
        push(log, a);
        json_free(payload);
        return log;
    }
    char *ts = jstrip(text);
    if (ts && ts[0]) {
        json_t *body = flow_provider_error_body(NULL, -1);
        char *message = flow_pse_format_message(-1, body);
        json_t *a = act("error");
        json_set(a, "status_code", json_null());
        json_set(a, "body", body);
        json_set(a, "raw_text", json_string(text));
        json_set(a, "message", json_string(message ? message : ""));
        free(message);
        push(log, a);
    }
    free(ts);
    return log;
}

/* ===========================================================================
 *  _build_partial_stream_stub  (chat + anthropic stub shapes)
 * =========================================================================== */

#define FLOW_PARTIAL_STUB_ID "partial-stream-stub"
#define FLOW_FINISH_LENGTH "length"

/* Shared stub shape builder. dropped_names is a json array (may be empty) or
 * NULL; `dropped_tool_names or None` -> non-empty array else null. */
static json_t *build_stub_shape(const char *role, const char *full_content,
                                const char *full_reasoning, const char *model_name,
                                const json_t *usage, const json_t *dropped_names,
                                int overflow_terminal, const char *api_mode, int clean_eof)
{
    json_t *stub = json_object();
    json_set(stub, "id", json_string(FLOW_PARTIAL_STUB_ID));
    json_set(stub, "_overflow_terminal", json_bool(overflow_terminal != 0));
    json_set(stub, "_clean_eof", json_bool(clean_eof != 0));
    json_set(stub, "_dropped_tool_names",
             (dropped_names && json_len((json_t *)dropped_names) > 0)
                 ? json_copy((json_t *)dropped_names) : json_null());
    if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
        json_set(stub, "type", json_string("message"));
        json_set(stub, "role", json_string(role ? role : ""));
        json_set(stub, "model", json_string(model_name ? model_name : ""));
        json_t *block = json_object();
        json_set(block, "type", json_string("text"));
        json_set(block, "text", json_string(full_content ? full_content : ""));
        json_t *content = json_array();
        json_append(content, block);
        json_set(stub, "content", content);
        json_set(stub, "stop_reason", json_string("max_tokens"));
        json_set(stub, "stop_sequence", json_null());
        json_set(stub, "usage", usage ? json_copy((json_t *)usage) : json_null());
        return stub;
    }
    json_set(stub, "model", json_string(model_name ? model_name : ""));
    json_t *message = json_object();
    json_set(message, "role", json_string(role ? role : ""));
    json_set(message, "content", full_content ? json_string(full_content) : json_null());
    json_set(message, "tool_calls", json_null());
    json_set(message, "reasoning_content", full_reasoning ? json_string(full_reasoning) : json_null());
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_set(choice, "message", message);
    json_set(choice, "finish_reason", json_string(FLOW_FINISH_LENGTH));
    json_t *choices = json_array();
    json_append(choices, choice);
    json_set(stub, "choices", choices);
    json_set(stub, "usage", usage ? json_copy((json_t *)usage) : json_null());
    return stub;
}

/* PoP: cch_flow_build_partial_stream_stub @ agent/chat_completion_helpers.py:_build_partial_stream_stub */
json_t *cch_flow_build_partial_stream_stub(const json_t *in)
{
    json_t *log = json_array();
    json_t *shape = build_stub_shape(
        jgets(in, "role"), jstrv(jget(in, "full_content")), jstrv(jget(in, "full_reasoning")),
        jstrv(jget(in, "model_name")), jget(in, "usage"), jget(in, "dropped_tool_names"),
        jtruth(jget(in, "overflow_terminal")), jstrv(jget(in, "api_mode")),
        jtruth(jget(in, "clean_eof")));
    json_t *a = act("stub");
    json_set(a, "shape", shape);
    push(log, a);
    return log;
}

/* ===========================================================================
 *  _StreamingCall._finish_chat_stream  (finish-reason routing / stub / error)
 * =========================================================================== */

/* PoP: cch_flow_finish_chat_stream @ agent/chat_completion_helpers.py:_finish_chat_stream */
json_t *cch_flow_finish_chat_stream(const json_t *in)
{
    json_t *log = json_array();
    const json_t *content_parts = jget(in, "content_parts");
    const json_t *reasoning_parts = jget(in, "reasoning_parts");
    const json_t *refusal_parts = jget(in, "refusal_parts");
    const json_t *acc = jget(in, "acc");
    const char *finish_reason = jstrv(jget(in, "finish_reason"));
    const char *model_name = jstrv(jget(in, "model"));
    const json_t *usage_obj = jget(in, "usage");
    const char *role = jgets(in, "role");
    if (!role[0]) role = "assistant";
    const char *response_id = jstrv(jget(in, "response_id"));
    const char *upstream_provider = jstrv(jget(in, "upstream_provider"));
    const json_t *reasoning_details = jget(in, "reasoning_details");

    char *fc = join_str_array(content_parts, "");
    char *fr = join_str_array(reasoning_parts, "");
    const char *full_content = (fc && fc[0]) ? fc : NULL;
    const char *full_reasoning = (fr && fr[0]) ? fr : NULL;

    if (!full_reasoning && full_content) {
        const json_t *er = jget(in, "extract_reasoning");
        const char *ers = jstrv(er);
        if (ers && ers[0]) full_reasoning = ers;
    }

    /* _assemble_tool_calls via the ported entry (reuse). */
    json_t *asm_in = json_object();
    json_set(asm_in, "acc", acc ? json_copy((json_t *)acc) : json_object());
    json_set(asm_in, "finish_reason", finish_reason ? json_string(finish_reason) : json_null());
    if (jget(in, "repair_results")) json_set(asm_in, "repair_results", json_copy((json_t *)jget(in, "repair_results")));
    if (jget(in, "repetition")) json_set(asm_in, "repetition", json_copy((json_t *)jget(in, "repetition")));
    json_t *asm_log = cch_flow_assemble_tool_calls(asm_in);
    json_free(asm_in);
    json_t *asm_res = json_get(asm_log, 0);
    const json_t *mock_tool_calls = jget(asm_res, "tool_calls");
    int has_truncated = jtruth(jget(asm_res, "has_truncated"));

    int acc_nonempty = acc && acc->type == JSON_OBJECT && acc->c.count > 0;
    int cp_nonempty = content_parts && json_len((json_t *)content_parts) > 0;
    int rp_nonempty = reasoning_parts && json_len((json_t *)reasoning_parts) > 0;
    int refp_nonempty = refusal_parts && json_len((json_t *)refusal_parts) > 0;

    if (!finish_reason && !cp_nonempty && !rp_nonempty && !refp_nonempty && !acc_nonempty) {
        json_t *a = act("raise");
        json_set(a, "kind", json_string("EmptyStreamError"));
        json_set(a, "message", json_string("Provider returned an empty stream with no finish_reason (possible upstream error or malformed SSE response)."));
        push(log, a);
        goto cleanup;
    }
    if (has_truncated && !finish_reason) {
        json_t *dropped = json_array();
        int n = 0; int *slots = tc_sorted_slots(acc, &n);
        for (int i = 0; i < n; i++) {
            char key[32]; slot_key(slots[i], key, sizeof(key));
            const char *fnname = jgets(jget(json_obj_get((json_t *)acc, key), "function"), "name");
            json_append(dropped, json_string(fnname[0] ? fnname : "?"));
        }
        free(slots);
        json_t *shape = build_stub_shape(role, full_content, full_reasoning, model_name,
                                         usage_obj, dropped, 0, NULL, 1);
        json_t *a = act("stub");
        json_set(a, "shape", shape);
        push(log, a);
        json_free(dropped);
        goto cleanup;
    }
    if (!finish_reason && (cp_nonempty || rp_nonempty) && !acc_nonempty && !usage_obj) {
        json_t *shape = build_stub_shape(role, full_content, full_reasoning, model_name,
                                         usage_obj, NULL, 0, NULL, 1);
        json_t *a = act("stub");
        json_set(a, "shape", shape);
        push(log, a);
        goto cleanup;
    }
    const char *effective = has_truncated ? "length" : (finish_reason ? finish_reason : "stop");
    json_t *pse_in = json_object();
    json_set(pse_in, "text", json_string(full_content ? full_content : ""));
    json_set(pse_in, "finish_reason", json_string(effective));
    json_t *pse_log = cch_flow_provider_stream_error_from_text(pse_in);
    json_free(pse_in);
    if (json_len(pse_log) > 0) {
        json_t *err = json_get(pse_log, 0);
        json_t *a = act("raise");
        json_set(a, "kind", json_string("ProviderStreamError"));
        json_set(a, "status_code", json_copy(jget(err, "status_code")));
        json_set(a, "body", json_copy(jget(err, "body")));
        json_set(a, "message", json_copy(jget(err, "message")));
        push(log, a);
        json_free(pse_log);
        goto cleanup;
    }
    json_free(pse_log);

    /* Build the response object. */
    char *refusal_join = join_str_array(refusal_parts, "");
    json_t *message = json_object();
    json_set(message, "role", json_string(role));
    json_set(message, "content", full_content ? json_string(full_content) : json_null());
    json_set(message, "tool_calls", mock_tool_calls ? json_copy((json_t *)mock_tool_calls) : json_null());
    json_set(message, "reasoning_content", full_reasoning ? json_string(full_reasoning) : json_null());
    json_set(message, "refusal", (refusal_join && refusal_join[0]) ? json_string(refusal_join) : json_null());
    free(refusal_join);
    if (reasoning_details && json_len((json_t *)reasoning_details) > 0)
        json_set(message, "reasoning_details", json_copy((json_t *)reasoning_details));
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_set(choice, "message", message);
    json_set(choice, "finish_reason", json_string(effective));
    json_t *choices = json_array();
    json_append(choices, choice);
    json_t *response = json_object();
    if (response_id && response_id[0]) {
        json_set(response, "id", json_string(response_id));
    } else {
        const char *uuid = jgets(in, "uuid");
        char idbuf[128];
        snprintf(idbuf, sizeof(idbuf), "stream-%s", uuid[0] ? uuid : "00000000-0000-0000-0000-000000000000");
        json_set(response, "id", json_string(idbuf));
    }
    json_set(response, "model", model_name ? json_string(model_name) : json_null());
    json_set(response, "usage", usage_obj ? json_copy((json_t *)usage_obj) : json_null());
    json_set(response, "provider", upstream_provider ? json_string(upstream_provider) : json_null());
    json_set(response, "choices", choices);

    int is_shim = flow_is_router_timeout_shim(response);
    json_t *ra = act("response");
    json_set(ra, "response", response);
    json_set(ra, "flushed", json_bool(!is_shim));
    push(log, ra);

cleanup:
    json_free(asm_log);
    free(fc); free(fr);
    return log;
}

/* ===========================================================================
 *  _StreamingCall._partial_stream_stub  (partial-delivery fallback decision)
 * =========================================================================== */

/* PoP: cch_flow_partial_stream_stub @ agent/chat_completion_helpers.py:_partial_stream_stub */
json_t *cch_flow_partial_stream_stub(const json_t *in)
{
    json_t *log = json_array();
    const char *streamed = jgets(in, "streamed_text");
    char *stripped = jstrip(streamed);
    char *partial_text = (stripped && stripped[0]) ? strdup(stripped) : NULL;
    free(stripped);
    const json_t *names = jget(in, "partial_tool_names");
    int nn = names ? (int)json_len((json_t *)names) : 0;
    const char *model = jgets(in, "model");
    if (!model[0]) model = "unknown";
    const char *api_mode = jstrv(jget(in, "api_mode"));

    if (nn > 0) {
        /* _name_str = ", ".join(names[:3]) (+ ", +N more") */
        size_t cap = 64;
        for (int i = 0; i < nn && i < 3; i++) cap += strlen(jgets(json_get((json_t *)names, i), "")) + 4;
        char *name_str = (char *)malloc(cap);
        name_str[0] = '\0';
        for (int i = 0; i < nn && i < 3; i++) {
            if (i) strcat(name_str, ", ");
            strcat(name_str, jstrv(json_get((json_t *)names, i)) ? jstrv(json_get((json_t *)names, i)) : "");
        }
        if (nn > 3) {
            char more[32]; snprintf(more, sizeof(more), ", +%d more", nn - 3);
            strcat(name_str, more);
        }
        const char *warn_tpl = "\n\n\xe2\x9a\xa0 Stream stalled mid tool-call (%s); the action was not executed. "
                               "Ask me to retry if you want to continue.";
        size_t wlen = strlen(warn_tpl) + strlen(name_str) + 8;
        char *warn = (char *)malloc(wlen);
        snprintf(warn, wlen, warn_tpl, name_str);
        free(name_str);
        /* _partial_text = (_partial_text or "") + _warn */
        size_t plen = partial_text ? strlen(partial_text) : 0;
        char *joined = (char *)malloc(plen + strlen(warn) + 1);
        memcpy(joined, partial_text ? partial_text : "", plen);
        memcpy(joined + plen, warn, strlen(warn) + 1);
        free(partial_text);
        partial_text = joined;
        free(warn);
    }

    const char *cls_reason = jstrv(jget(jget(in, "classify"), "reason"));

    json_t *dropped = names ? json_copy((json_t *)names) : json_array();
    if (cls_reason && strcmp(cls_reason, "context_overflow") == 0) {
        json_t *shape = build_stub_shape("assistant", NULL, NULL, model, NULL,
                                         dropped, 1, api_mode, 0);
        json_t *a = act("stub");
        json_set(a, "shape", shape);
        push(log, a);
        json_free(dropped);
        free(partial_text);
        return log;
    }
    json_t *shape = build_stub_shape("assistant", partial_text, NULL, model, NULL,
                                     dropped, 0, api_mode, 0);
    if (cls_reason && strcmp(cls_reason, "content_policy_blocked") == 0)
        json_set(shape, "_content_filter_terminated", json_bool(true));
    json_t *a = act("stub");
    json_set(a, "shape", shape);
    push(log, a);
    json_free(dropped);
    free(partial_text);
    return log;
}

/* ===========================================================================
 *  _StreamingCall._maybe_disable_streaming  (channel-switch decision)
 * =========================================================================== */

/* PoP: cch_flow_maybe_disable_streaming @ agent/chat_completion_helpers.py:_maybe_disable_streaming */
json_t *cch_flow_maybe_disable_streaming(const json_t *in)
{
    json_t *log = json_array();
    int disabled = 0;
    const char *notice_kind = NULL;
    const char *notice_text = NULL;

    if (jtruth(jget(in, "llamacpp"))) {
        /* llama-server stays on streaming. */
    } else if (jtruth(jget(in, "empty_frame"))) {
        disabled = 1;
        notice_kind = "warning";
        notice_text = "\xe2\x9a\xa0\xef\xb8\x8f Provider stream returned an empty keepalive frame \xe2\x80\x94 retrying this turn "
                      "without streaming (streaming stays off on this route).";
    } else if (jtruth(jget(in, "stream_unavailable"))) {
        const char *err_text = jgets(jget(in, "error"), "text");
        char *err_lower = jlower(err_text);
        int is_unsupported = (strstr(err_lower, "stream") && strstr(err_lower, "not supported"));
        int has_order = (strstr(err_lower, "unexpected event order") != NULL);
        const char *api_mode = jgets(in, "api_mode");
        const char *provider = jgets(in, "provider");
        if (has_order && !is_unsupported) {
            if (!(strcmp(api_mode, "anthropic_messages") != 0 || strcmp(provider, "bedrock") == 0)) {
                disabled = 1;
                notice_kind = "print";
                notice_text = "\n\xe2\x9a\xa0  Provider sent Anthropic stream events out of order. Switching to non-streaming.\n";
            }
        } else {
            disabled = 1;
            notice_kind = "print";
            notice_text = is_unsupported
                ? "\n\xe2\x9a\xa0  Streaming is not supported for this model/provider. Switching to non-streaming.\n"
                  "   To avoid this delay, set display.streaming: false in config.yaml\n"
                : "\n\xe2\x9a\xa0  AWS IAM denied bedrock:InvokeModelWithResponseStream. Switching to non-streaming.\n"
                  "   Grant that action to restore streaming output.\n";
        }
        free(err_lower);
    }

    json_t *a = act("result");
    json_set(a, "disabled", json_bool(disabled != 0));
    set_str(a, "notice_kind", notice_kind);
    set_str(a, "notice_text", notice_text);
    push(log, a);
    return log;
}

/* ===========================================================================
 *  _StreamingCall._retry_after_drop  (teardown + backoff sequence)
 * =========================================================================== */

/* PoP: cch_flow_retry_after_drop @ agent/chat_completion_helpers.py:_retry_after_drop */
json_t *cch_flow_retry_after_drop(const json_t *in)
{
    json_t *log = json_array();
    int attempt = (int)json_get_num(in, "attempt", 0);
    int max_retries = (int)json_get_num(in, "max_retries", 0);
    int mid_tool = jtruth(jget(in, "mid_tool_call"));
    const char *reason = jgets(in, "reason");

    json_t *ed = act("emit_stream_drop");
    json_set(ed, "attempt", json_number(attempt + 1));
    json_set(ed, "max_attempts", json_number(max_retries + 1));
    json_set(ed, "mid_tool_call", json_bool(mid_tool != 0));
    push(log, ed);
    if (jtruth(jget(in, "is_parse_error")))
        push(log, act("buffer_anthropic_tool_input"));
    json_t *ca = act("cancel_attempt"); json_set(ca, "reason", json_string(reason)); push(log, ca);
    json_t *co = act("close_once"); json_set(co, "reason", json_string(reason)); push(log, co);
    /* jittered_backoff(attempt+1, base=1.0, max=4.0, jitter=0.0) is injected. */
    double delay = json_get_num(in, "backoff_delay", 0.0);
    json_t *wb = act("wait_backoff"); json_set(wb, "delay", json_number(delay)); push(log, wb);
    return log;
}

/* ===========================================================================
 *  _reasoning_config_for_wire  (+ _consume_ephemeral_reasoning_off)
 * =========================================================================== */

/* PoP: cch_flow_consume_ephemeral_reasoning_off @ agent/chat_completion_helpers.py:_consume_ephemeral_reasoning_off */
static int flow_consume_ephemeral(json_t *agent)
{
    int consumed = jtruth(jget(agent, "_ephemeral_reasoning_off"));
    if (consumed) json_set(agent, "_ephemeral_reasoning_off", json_bool(false));
    return consumed;
}

/* PoP: cch_flow_reasoning_config_for_wire @ agent/chat_completion_helpers.py:_reasoning_config_for_wire */
json_t *cch_flow_reasoning_config_for_wire(const json_t *in)
{
    json_t *log = json_array();
    /* Mutable agent attribute bag (copy of the scripted attrs). */
    json_t *agent = json_object();
    const json_t *cfg_in = jget(in, "reasoning_config");
    json_set(agent, "reasoning_config", cfg_in ? json_copy((json_t *)cfg_in) : json_null());
    json_set(agent, "_ephemeral_reasoning_off", json_bool(jtruth(jget(in, "ephemeral_reasoning_off"))));
    json_set(agent, "_reasoning_effort_rejected", json_bool(jtruth(jget(in, "effort_rejected"))));
    json_set(agent, "_reasoning_disable_rejected", json_bool(jtruth(jget(in, "disable_rejected"))));
    json_set(agent, "_reasoning_floor_required", json_bool(jtruth(jget(in, "floor_required"))));

    json_t *cfg = jget(agent, "reasoning_config");
    int ephemeral_off = flow_consume_ephemeral(agent);
    json_t *ce = act("consume_ephemeral");
    json_set(ce, "consumed", json_bool(ephemeral_off != 0));
    push(log, ce);

    json_t *wire = NULL;      /* returned config (owned ref into agent or new) */
    int owned_new = 0;
    if (jtruth(jget(agent, "_reasoning_effort_rejected"))) {
        json_set(agent, "_wire_reasoning_config", json_null());
        wire = NULL;
    } else {
        if (!cfg || cfg->type == JSON_NULL) {
            const json_t *ud = jget(in, "unset_default");
            cfg = ud ? json_copy((json_t *)ud) : json_null();
            json_set(agent, "reasoning_config", cfg);
        }
        if (jtruth(jget(agent, "_reasoning_disable_rejected"))) {
            int is_disable = 0;
            if (cfg && cfg->type == JSON_OBJECT) {
                const json_t *en = jget(cfg, "enabled");
                const json_t *ef = jget(cfg, "effort");
                if ((en && en->type == JSON_BOOL && !en->bool_val)
                    || (ef && ef->type == JSON_STRING && jstrv(ef) && strcmp(jstrv(ef), "none") == 0))
                    is_disable = 1;
            }
            if (is_disable) {
                if (jtruth(jget(agent, "_reasoning_floor_required"))) {
                    const char *floor_effort = jgets(in, "floor_effort");
                    if (!floor_effort[0]) floor_effort = "low";
                    json_t *floored = (cfg && cfg->type == JSON_OBJECT) ? json_copy(cfg) : json_object();
                    json_set(floored, "enabled", json_bool(true));
                    json_set(floored, "effort", json_string(floor_effort));
                    json_set(agent, "_wire_reasoning_config", floored);
                    wire = floored; owned_new = 1;
                } else {
                    json_set(agent, "_wire_reasoning_config", json_null());
                    wire = NULL;
                }
            } else {
                json_set(agent, "_wire_reasoning_config", cfg ? json_copy(cfg) : json_null());
                wire = cfg;
            }
        } else {
            if (ephemeral_off) {
                json_t *nc = (cfg && cfg->type == JSON_OBJECT) ? json_copy(cfg) : json_object();
                json_set(nc, "enabled", json_bool(false));
                json_set(nc, "effort", json_string("none"));
                json_set(agent, "reasoning_config", nc);
                cfg = nc;
            }
            json_set(agent, "_wire_reasoning_config", cfg ? json_copy(cfg) : json_null());
            wire = cfg;
        }
    }
    json_t *wa = act("wire");
    json_set(wa, "config", wire ? json_copy(wire) : json_null());
    json_set(wa, "wire_reasoning_config", json_copy(jget(agent, "_wire_reasoning_config")));
    json_set(wa, "ephemeral_consumed", json_bool(ephemeral_off != 0));
    push(log, wa);
    (void)owned_new;
    json_free(agent);
    return log;
}

/* Splice a sub-sequence (owned) into the main log, then free it. */
static void splice(json_t *log, json_t *sub)
{
    for (size_t i = 0; i < json_len(sub); i++)
        json_append(log, json_copy(json_get(sub, i)));
    json_free(sub);
}

/* ===========================================================================
 *  _StreamingCall._handle_stream_error  (retry / fallback classification)
 * =========================================================================== */

/* PoP: cch_flow_handle_stream_error @ agent/chat_completion_helpers.py:_handle_stream_error */
json_t *cch_flow_handle_stream_error(const json_t *in)
{
    json_t *log = json_array();
    int attempt = (int)json_get_num(in, "attempt", 0);
    int max_retries = (int)json_get_num(in, "max_retries", 0);
    int deltas_were_sent = jtruth(jget(in, "deltas_were_sent"));
    int stream_options_unsupported = jtruth(jget(in, "stream_options_unsupported"));
    int provider_tool_in_flight = jtruth(jget(in, "provider_tool_in_flight"));
    const json_t *partial_names = jget(in, "partial_tool_names");
    const char *streamed_text = jgets(in, "streamed_text");
    int retry = 0;
    const json_t *result_error = NULL;

    /* Our own interrupt force-close: exit without retry. */
    if (jtruth(jget(in, "request_cancelled"))) goto done;

    /* _as_lifetime_error seam: a cap kill replaces the transport error. */
    const json_t *lifetime = jget(in, "lifetime_error");
    const json_t *err = (lifetime && lifetime->type != JSON_NULL) ? lifetime : jget(in, "error");

    int is_timeout = jtruth(jget(in, "is_timeout"));
    int is_conn_err = jtruth(jget(in, "is_conn_err"));
    int is_parse_err = jtruth(jget(in, "is_parse_err"));
    int is_empty_stream = jtruth(jget(in, "is_empty_stream"));
    int sse_predicate = jtruth(jget(in, "sse_conn_predicate"));
    int is_sse_conn_err = !is_timeout && !is_conn_err && sse_predicate;
    int is_transient = is_timeout || is_conn_err || is_sse_conn_err || is_parse_err;

    /* stream_options compatibility retry (not a network retry). */
    if (!deltas_were_sent && !stream_options_unsupported && jtruth(jget(in, "rejects_stream_options"))) {
        retry = 1;
        goto done;
    }

    /* _forget_invisible_deltas: may reset delivery to undelivered. */
    int tool_in_flight = (partial_names && json_len((json_t *)partial_names) > 0) || provider_tool_in_flight;
    char *vis = jstrip(streamed_text);
    int visible_delivered = (vis && vis[0]);
    free(vis);
    if (deltas_were_sent && !tool_in_flight && !visible_delivered) deltas_were_sent = 0;

    if (deltas_were_sent) {
        /* Died after delivery: retry only a transient error with a tool in flight. */
        if (!(tool_in_flight && is_transient && attempt < max_retries)) {
            result_error = err;
            goto done;
        }
        retry = 1;   /* mid-tool reconnect (retry_after_drop owns the teardown) */
        goto done;
    }

    if (is_transient || is_empty_stream) {
        if (attempt < max_retries) { retry = 1; goto done; }
        result_error = err;   /* exhausted */
        goto done;
    }
    /* Non-transient: _maybe_disable_streaming + the 5xx unmask probe run; when the
     * probe handles the error it owns result, otherwise the error propagates. */
    if (jtruth(jget(in, "unmask_handles"))) goto done;
    result_error = err;

done:
    {
        json_t *a = act("result");
        json_set(a, "retry", json_bool(retry != 0));
        json_set(a, "result_error", result_error ? json_copy((json_t *)result_error) : json_null());
        push(log, a);
    }
    return log;
}

/* ===========================================================================
 *  _StreamingCall._unmask_server_error_with_nonstreaming  (5xx probe decision)
 * =========================================================================== */

#define FLOW_5XX_PROBE_WINDOW_S 60.0

/* PoP: cch_flow_unmask_probe @ agent/chat_completion_helpers.py:_unmask_server_error_with_nonstreaming */
json_t *cch_flow_unmask_probe(const json_t *in)
{
    json_t *log = json_array();
    int handled = 0;
    const json_t *result_error = NULL;
    const json_t *result_response = NULL;

    if (jtruth(jget(in, "interrupt_requested"))) goto done;
    if (jtruth(jget(in, "llamacpp"))) goto done;
    const json_t *status_v = jget(in, "status");
    int has_status = status_v && status_v->type == JSON_NUMBER;
    int status = has_status ? (int)status_v->num_val : -1;
    if (!has_status || status < 500 || jtruth(jget(in, "deltas_were_sent"))) goto done;
    const char *api_mode = jgets(in, "api_mode");
    if (!(api_mode[0] == '\0' || strcmp(api_mode, "chat_completions") == 0)) goto done;
    double now = json_get_num(in, "now", 0.0);
    const json_t *last_v = jget(in, "last_probe_ts");
    if (last_v && last_v->type == JSON_NUMBER && (now - last_v->num_val) < FLOW_5XX_PROBE_WINDOW_S) goto done;
    /* arm: agent._stream_5xx_probe_ts = now; suspend stale timeout (state seams) */

    const char *outcome = jgets(in, "probe_outcome");
    if (strcmp(outcome, "interrupt") == 0) {
        json_t *a = act("raise"); json_set(a, "kind", json_string("InterruptedError")); push(log, a);
        return log;
    }
    if (strcmp(outcome, "error") == 0) {
        const json_t *ps_v = jget(in, "probe_status");
        if (ps_v && ps_v->type == JSON_NUMBER && (int)ps_v->num_val < 500) {
            result_error = jget(in, "probe_error");
            handled = 1;
        }
        goto done;
    }
    /* success */
    if (jtruth(jget(in, "replay_ok"))) {
        result_response = jget(in, "probe_response");
        handled = 1;
    }

done:
    {
        json_t *a = act("result");
        json_set(a, "handled", json_bool(handled != 0));
        json_set(a, "result_error", result_error ? json_copy((json_t *)result_error) : json_null());
        json_set(a, "result_response", result_response ? json_copy((json_t *)result_response) : json_null());
        push(log, a);
    }
    return log;
}

/* ===========================================================================
 *  _StreamingCall._call  (retry-loop orchestration)
 * =========================================================================== */

/* PoP: cch_flow_call_retry_loop @ agent/chat_completion_helpers.py:_call */
json_t *cch_flow_call_retry_loop(const json_t *in)
{
    json_t *log = json_array();
    int max_retries = (int)json_get_num(in, "max_stream_retries", 2);
    int compat_retries = (int)json_get_num(in, "compat_retries", 0);
    int budget = max_retries + compat_retries;
    const json_t *interrupt_before = jget(in, "interrupt_before");
    const json_t *wire = jget(in, "wire");
    const json_t *handle_retry = jget(in, "handle_retry");
    const json_t *route_switched = jget(in, "route_switched");

    int attempt = -1;
    int err_idx = 0;
    int have_response = 0;
    int attempts = 0;
    const char *outcome = NULL;
    const json_t *result_response = NULL;
    const json_t *result_error = NULL;
    json_t *interrupt_err = NULL;

    for (;;) {
        if (!(attempt < budget)) { outcome = "budget_exhausted"; break; }
        attempt++;
        attempts = attempt + 1;   /* _start_stream_attempt: current += 1 */

        int interrupted = 0;
        if (interrupt_before)
            for (size_t i = 0; i < json_len((json_t *)interrupt_before); i++)
                if ((int)json_get((json_t *)interrupt_before, i)->num_val == attempt) interrupted = 1;
        if (interrupted) {
            /* raise InterruptedError -> outer except routes it into result["error"]. */
            interrupt_err = json_object();
            json_set(interrupt_err, "kind", json_string("InterruptedError"));
            result_error = interrupt_err;
            outcome = "interrupted";
            break;
        }

        const json_t *w = (wire && (size_t)attempt < json_len((json_t *)wire))
                          ? json_get((json_t *)wire, (size_t)attempt) : NULL;
        const char *wout = jgets(w, "outcome");
        if (w && strcmp(wout, "response") == 0) {
            have_response = 1;
            result_response = jget(w, "response");
            outcome = "success";
            break;
        }
        /* error path: _handle_stream_error decides retry vs stop. */
        int retry = (handle_retry && (size_t)err_idx < json_len((json_t *)handle_retry))
                    ? jtruth(json_get((json_t *)handle_retry, (size_t)err_idx)) : 0;
        if (!retry) { outcome = "handler_stop"; result_error = jget(w, "error"); err_idx++; break; }
        int switched = (route_switched && (size_t)err_idx < json_len((json_t *)route_switched))
                       ? jtruth(json_get((json_t *)route_switched, (size_t)err_idx)) : 0;
        err_idx++;
        if (switched) { outcome = "route_switched"; result_error = jget(w, "error"); break; }
        /* else continue to next attempt */
    }

    /* finally: close_once reuses the client only after a clean stream. */
    json_t *a = act("result");
    json_set(a, "outcome", json_string(outcome ? outcome : "unknown"));
    json_set(a, "attempts", json_number(attempts));
    json_set(a, "close_reason", json_string(have_response ? "stream_request_complete" : "stream_error_cleanup"));
    json_set(a, "response", result_response ? json_copy((json_t *)result_response) : json_null());
    json_set(a, "error", result_error ? json_copy((json_t *)result_error) : json_null());
    push(log, a);
    json_free(interrupt_err);
    return log;
}

/* ===========================================================================
 *  _StreamingCall.run  (post-monitor outcome translation)
 * =========================================================================== */

/* PoP: cch_flow_run_outcome @ agent/chat_completion_helpers.py:run */
json_t *cch_flow_run_outcome(const json_t *in)
{
    json_t *log = json_array();
    /* Live run() RE-INITIALIZES self._monitor_interrupted = {"yes": False}
     * before starting the monitor thread (chat_completion_helpers.py:run);
     * the scripted monitor is a no-op seam (its Thread.start never runs the
     * target), so the flag is always False at the decision tail -- a seeded
     * monitor_interrupted input is discarded exactly as live discards it.
     * Only the monitor's own writes could set it, and this model has none. */
    if (jtruth(jget(in, "interrupt_requested"))) {
        json_t *a = act("raise");
        json_set(a, "kind", json_string("InterruptedError"));
        json_set(a, "message", json_string("Agent interrupted during streaming API call (post-worker)"));
        push(log, a);
        return log;
    }
    const json_t *error = jget(in, "result_error");
    const json_t *response = jget(in, "result_response");
    int deltas_were_sent = jtruth(jget(in, "deltas_were_sent"));
    const json_t *abandon = jget(in, "abandon_error");
    if (abandon && abandon->type != JSON_NULL) {
        error = abandon;
        response = NULL;
        /* _forget_invisible_deltas(error): may reset delivery to undelivered. */
        const json_t *pn = jget(in, "partial_tool_names");
        int tool_in_flight = (pn && json_len((json_t *)pn) > 0) || jtruth(jget(in, "provider_tool_in_flight"));
        char *vis = jstrip(jgets(in, "streamed_text"));
        int visible = vis && vis[0];
        free(vis);
        if (deltas_were_sent && !tool_in_flight && !visible) deltas_were_sent = 0;
    }
    if (error && error->type != JSON_NULL) {
        if (deltas_were_sent) {
            json_t *a = act("return");
            json_set(a, "outcome", json_string("stub"));
            json_set(a, "response", jget(in, "partial_stub") ? json_copy((json_t *)jget(in, "partial_stub")) : json_null());
            push(log, a);
            return log;
        }
        json_t *a = act("raise");
        json_set(a, "kind", jget(error, "kind") ? json_copy((json_t *)jget(error, "kind")) : json_string("Exception"));
        push(log, a);
        return log;
    }
    json_t *a = act("return");
    json_set(a, "outcome", json_string("response"));
    json_set(a, "response", response ? json_copy((json_t *)response) : json_null());
    push(log, a);
    return log;
}

/* ===========================================================================
 *  _StreamingCall._call_chat_completions  (per-chunk accumulation / routing)
 *
 *  Seams (all injected via `in`, never performed inside the core):
 *   - network stream / relay open  -> scripted `chunks` array
 *   - RunawayStreamWatch.feed      -> scripted per-channel bool queues
 *   - prompt_progress / llamacpp   -> `llamacpp` flag + chunk.model_extra
 *   - StreamCarriers.feed/apply    -> identity (no declared carrier keys)
 *   - display callbacks            -> cch_display_seams_t (recorded actions)
 *   - clock / diag                 -> omitted (bookkeeping, not decisions)
 * =========================================================================== */

/* Display I/O seam: stands in for agent._fire_stream_delta / _fire_reasoning_delta
 * / _fire_tool_gen_started / on_first_delta. The harness wires a recorder; the
 * decision core only invokes it, never renders. */
typedef struct {
    void *ud;
    void (*fire_stream_delta)(void *ud, const char *text);
    void (*fire_reasoning_delta)(void *ud, const char *text);
    void (*fire_tool_gen_started)(void *ud, const char *name);
    void (*on_first_delta)(void *ud);
} cch_display_seams_t;

static void display_noop(void *ud, const char *s) { (void)ud; (void)s; }
static void display_noop0(void *ud) { (void)ud; }

typedef struct {
    json_t *log;
    json_t *content_parts, *reasoning_parts, *detail_display_parts;
    json_t *refusal_parts, *reasoning_details, *pending_text_parts;
    json_t *partial_tool_names;
    tc_acc_t tc;
    int deltas_were_sent, first_delta_fired;
    int hold_stream, stream_delta_callback, on_first_delta_present;
    const char *runaway;
    char *finish_reason, *model_name, *response_id, *upstream_provider;
    json_t *usage_obj;
    jcursor_t watch_content, watch_reasoning, watch_detail;
    cch_display_seams_t display;
} chunk_loop_t;

/* PoP: cch_flow_fire_first_delta @ agent/chat_completion_helpers.py:_fire_first_delta */
static void loop_fire_first(chunk_loop_t *s)
{
    if (!s->first_delta_fired && s->on_first_delta_present) {
        s->first_delta_fired = 1;
        if (s->display.on_first_delta) s->display.on_first_delta(s->display.ud);
        push(s->log, act("first_delta"));
    }
}

/* PoP: cch_flow_emit_text @ agent/chat_completion_helpers.py:_emit_text */
static void loop_emit_text(chunk_loop_t *s, const char *text)
{
    loop_fire_first(s);
    if (s->display.fire_stream_delta) s->display.fire_stream_delta(s->display.ud, text);
    json_t *a = act("emit_text"); json_set(a, "text", json_string(text)); push(s->log, a);
    s->deltas_were_sent = 1;
}

/* PoP: cch_flow_emit_reasoning @ agent/chat_completion_helpers.py:_emit_reasoning */
static void loop_emit_reasoning(chunk_loop_t *s, const char *text)
{
    loop_fire_first(s);
    if (s->display.fire_reasoning_delta) s->display.fire_reasoning_delta(s->display.ud, text);
    json_t *a = act("emit_reasoning"); json_set(a, "text", json_string(text)); push(s->log, a);
}

/* PoP: cch_flow_emit_tool_started @ agent/chat_completion_helpers.py:_emit_tool_started */
static void loop_emit_tool_started(chunk_loop_t *s, const char *name)
{
    loop_fire_first(s);
    if (s->display.fire_tool_gen_started) s->display.fire_tool_gen_started(s->display.ud, name);
    json_t *a = act("emit_tool_started"); json_set(a, "name", json_string(name)); push(s->log, a);
}

/* PoP: cch_flow_route_suppressed_text @ agent/chat_completion_helpers.py:_route_suppressed_text */
static void loop_route_suppressed(chunk_loop_t *s, const char *text)
{
    if (s->stream_delta_callback && !s->hold_stream) {
        json_t *a = act("route_suppressed"); json_set(a, "text", json_string(text)); push(s->log, a);
    }
}

/* PoP: cch_flow_flush_pending_stream_text @ agent/chat_completion_helpers.py:_flush_pending_stream_text */
static void loop_flush_pending(chunk_loop_t *s)
{
    json_t *pending = s->pending_text_parts;
    s->pending_text_parts = json_array();
    int acc_nonempty = s->tc.acc->c.count > 0;
    for (size_t i = 0; i < json_len(pending); i++) {
        const char *t = jstrv(json_get(pending, i));
        if (acc_nonempty) loop_route_suppressed(s, t ? t : "");
        else loop_emit_text(s, t ? t : "");
    }
    json_free(pending);
}

/* PoP: cch_flow_prompt_progress @ agent/local_prompt_progress.py:prompt_progress */
static int loop_prompt_progress(const json_t *chunk)
{
    const json_t *extra = jget(chunk, "model_extra");
    const json_t *progress = (extra && extra->type == JSON_OBJECT) ? jget(extra, "prompt_progress") : NULL;
    if (!progress && chunk && chunk->type == JSON_OBJECT) progress = jget(chunk, "prompt_progress");
    if (!progress || progress->type != JSON_OBJECT) return 0;
    const json_t *choices = jget(chunk, "choices");
    static const char *const FIELDS[] = { "content", "tool_calls", "reasoning_content", "reasoning", "refusal", NULL };
    if (choices && choices->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len((json_t *)choices); i++) {
            const json_t *choice = json_get((json_t *)choices, i);
            if (jtruth(jget(choice, "finish_reason"))) return 0;
            const json_t *delta = jget(choice, "delta");
            for (int f = 0; FIELDS[f]; f++)
                if (jtruth(jget(delta, FIELDS[f]))) return 0;
        }
    }
    return 1;
}

/* PoP: cch_flow_chat_chunk_loop @ agent/chat_completion_helpers.py:_call_chat_completions */
json_t *cch_flow_chat_chunk_loop(const json_t *in)
{
    chunk_loop_t s;
    memset(&s, 0, sizeof(s));
    s.log = json_array();
    s.content_parts = json_array();
    s.reasoning_parts = json_array();
    s.detail_display_parts = json_array();
    s.refusal_parts = json_array();
    s.reasoning_details = json_array();
    s.pending_text_parts = json_array();
    s.partial_tool_names = json_array();
    tc_init(&s.tc);
    s.deltas_were_sent = jtruth(jget(in, "deltas_were_sent"));
    s.hold_stream = jtruth(jget(in, "hold_stream"));
    s.stream_delta_callback = jtruth(jget(in, "stream_delta_callback"));
    s.on_first_delta_present = jhas(in, "on_first_delta") ? jtruth(jget(in, "on_first_delta")) : 1;
    s.display.ud = NULL;
    s.display.fire_stream_delta = display_noop;
    s.display.fire_reasoning_delta = display_noop;
    s.display.fire_tool_gen_started = display_noop;
    s.display.on_first_delta = display_noop0;
    const json_t *watch = jget(in, "watch");
    s.watch_content.arr = jget(watch, "content");
    s.watch_reasoning.arr = jget(watch, "reasoning");
    s.watch_detail.arr = jget(watch, "detail");
    int llamacpp = jtruth(jget(in, "llamacpp"));
    int attempt_active = jhas(in, "attempt_active") ? jtruth(jget(in, "attempt_active")) : 1;
    int attempt_cancelled = jtruth(jget(in, "attempt_cancelled"));
    const json_t *interrupt_at = jget(in, "interrupt_at");
    const json_t *chunks = jget(in, "chunks");
    size_t nchunks = chunks ? json_len((json_t *)chunks) : 0;
    int terminal_error = 0;

    for (size_t ci = 0; ci < nchunks; ci++) {
        const json_t *chunk = json_get((json_t *)chunks, ci);
        int is_progress = llamacpp ? loop_prompt_progress(chunk) : 0;
        int interrupted = 0;
        if (interrupt_at)
            for (size_t k = 0; k < json_len((json_t *)interrupt_at); k++)
                if ((int)json_get((json_t *)interrupt_at, k)->num_val == (int)ci) interrupted = 1;
        if (interrupted) { break; }
        if (!attempt_active) { continue; }
        if (is_progress) { continue; }

        /* _chunk_identity */
        int ident_changed = 0;
        const char *cm = jstrv(jget(chunk, "model"));
        if (cm && cm[0]) { free(s.model_name); s.model_name = strdup(cm); ident_changed = 1; }
        const char *cid = jstrv(jget(chunk, "id"));
        if (!s.response_id && cid && cid[0]) { s.response_id = strdup(cid); ident_changed = 1; }
        const char *cp = jstrv(jget(chunk, "provider"));
        if (!s.upstream_provider && cp && cp[0]) { s.upstream_provider = strdup(cp); ident_changed = 1; }
        (void)ident_changed;

        const json_t *choices = jget(chunk, "choices");
        if (!choices || choices->type != JSON_ARRAY || json_len((json_t *)choices) == 0) {
            json_t *cl_in = json_object();
            json_set(cl_in, "chunk", json_copy((json_t *)chunk));
            json_set(cl_in, "finish_reason", s.finish_reason ? json_string(s.finish_reason) : json_null());
            json_t *cl_log = cch_flow_choiceless_chunk(cl_in);
            json_free(cl_in);
            json_t *cla = json_get(cl_log, 0);
            if (cla && strcmp(jgets(cla, "a"), "raise") == 0) {
                splice(s.log, cl_log);
                terminal_error = 1;
                break;
            }
            const json_t *u = jget(cla, "usage");
            if (u && u->type != JSON_NULL) { json_free(s.usage_obj); s.usage_obj = json_copy((json_t *)u); }
            const char *nfr = jstrv(jget(cla, "finish_reason"));
            if (nfr) { free(s.finish_reason); s.finish_reason = strdup(nfr); }
            splice(s.log, cl_log);
            continue;
        }

        const json_t *choice = json_get((json_t *)choices, 0);
        const json_t *delta = jget(choice, "delta");
        char *nfr = flow_normalize_finish_reason(jget(choice, "finish_reason"));
        if (nfr && nfr[0]) { free(s.finish_reason); s.finish_reason = nfr; }
        else free(nfr);
        const json_t *cu = jget(chunk, "usage");
        if (cu && jtruth(cu)) {
            json_free(s.usage_obj); s.usage_obj = json_copy((json_t *)cu);
        }

        /* reasoning_text = delta.reasoning_content or delta.reasoning (+model_extra) */
        const json_t *rt_v = jget(delta, "reasoning_content");
        if (!jtruth(rt_v)) rt_v = jget(delta, "reasoning");
        const char *reasoning_text = jstrv(rt_v);
        const json_t *dme = jget(delta, "model_extra");
        if (!reasoning_text && dme && dme->type == JSON_OBJECT) {
            const json_t *mrc = jget(dme, "reasoning_content");
            if (!jtruth(mrc)) mrc = jget(dme, "reasoning");
            reasoning_text = jstrv(mrc);
        }
        /* carriers.feed seam: identity (no declared carrier keys on this route). */
        if (reasoning_text && reasoning_text[0]) {
            const char *prev = json_len(s.reasoning_parts)
                ? jstrv(json_get(s.reasoning_parts, json_len(s.reasoning_parts) - 1)) : "";
            json_t *rt_json = json_string(reasoning_text);
            char *sep = flow_separate_glued(prev ? prev : "", rt_json);
            json_free(rt_json);
            json_append(s.reasoning_parts, json_string(sep));
            free(sep);
        }

        /* reasoning_details deltas */
        const json_t *rd_delta = jget(delta, "reasoning_details");
        if (!rd_delta && dme && dme->type == JSON_OBJECT) rd_delta = jget(dme, "reasoning_details");
        json_t *detail_text_parts = json_array();
        if (rd_delta && rd_delta->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len((json_t *)rd_delta); i++) {
                const json_t *rd = json_get((json_t *)rd_delta, i);
                char *dt = flow_detail_text(rd);
                json_append(detail_text_parts, json_string(dt));
                free(dt);
                flow_append_detail(s.reasoning_details, rd);
            }
        }
        char *detail_text = join_str_array(detail_text_parts, "");
        json_free(detail_text_parts);
        if (detail_text && detail_text[0]) {
            const char *prev = json_len(s.detail_display_parts)
                ? jstrv(json_get(s.detail_display_parts, json_len(s.detail_display_parts) - 1)) : "";
            json_t *dt_json = json_string(detail_text);
            char *sep = flow_separate_glued(prev ? prev : "", dt_json);
            json_free(dt_json);
            json_append(s.detail_display_parts, json_string(sep));
            free(sep);
            free(detail_text);
            /* display uses the latest de-glued detail piece */
            detail_text = strdup(jstrv(json_get(s.detail_display_parts, json_len(s.detail_display_parts) - 1)));
        }
        const char *display_reasoning = (detail_text && detail_text[0]) ? detail_text : reasoning_text;
        if (display_reasoning && display_reasoning[0]) {
            loop_emit_reasoning(&s, display_reasoning);
            /* RunawayStreamWatch.feed returns False for empty text WITHOUT consuming
             * a check; mirror that so the scripted queues stay in call-order sync. */
            int rw = 0;
            if (reasoning_text && reasoning_text[0]) rw = jnext_bool(&s.watch_reasoning, 0);
            int runaway_hit = rw;
            if (!rw) {
                int dw = 0;
                if (detail_text && detail_text[0]) dw = jnext_bool(&s.watch_detail, 0);
                runaway_hit = dw;
            }
            if (runaway_hit) { s.runaway = "reasoning"; free(detail_text); break; }
        }
        free(detail_text);

        /* refusal */
        const json_t *ref_v = jget(delta, "refusal");
        if (!ref_v && dme && dme->type == JSON_OBJECT) ref_v = jget(dme, "refusal");
        const char *refusal = jstrv(ref_v);
        if (refusal && refusal[0]) {
            json_append(s.refusal_parts, json_string(refusal));
        }

        /* content */
        char *delta_content = flow_flatten_message_text(jget(delta, "content"), "");
        if (delta_content && delta_content[0]) {
            json_append(s.content_parts, json_string(delta_content));
            if (jnext_bool(&s.watch_content, 0)) { s.runaway = "content"; free(delta_content); break; }
            int acc_nonempty = s.tc.acc->c.count > 0;
            if (acc_nonempty) {
                loop_route_suppressed(&s, delta_content);
            } else {
                char *joined_content = join_str_array(s.content_parts, "");
                int hold = (json_len(s.pending_text_parts) > 0)
                    || flow_text_may_be_sse(delta_content)
                    || (!s.deltas_were_sent && flow_router_shim_may_follow(joined_content));
                free(joined_content);
                if (hold) {
                    json_append(s.pending_text_parts, json_string(delta_content));
                    char *pending = join_str_array(s.pending_text_parts, "");
                    if (!(flow_text_may_be_sse(pending) || flow_router_shim_may_follow(pending)))
                        loop_flush_pending(&s);
                    free(pending);
                    free(delta_content);
                    continue;   /* skips tool-call processing for this chunk */
                } else {
                    loop_emit_text(&s, delta_content);
                }
            }
        }
        free(delta_content);

        /* tool calls */
        const json_t *dtc = jget(delta, "tool_calls");
        if (dtc && jtruth(dtc)) {
            loop_flush_pending(&s);
            for (size_t i = 0; i < json_len((json_t *)dtc); i++) {
                const json_t *tcd = json_get((json_t *)dtc, i);
                const char *name = tc_feed(&s.tc, tcd);
                if (name) {
                    loop_emit_tool_started(&s, name);
                    json_append(s.partial_tool_names, json_string(name));
                }
            }
        }
    }

    /* post-loop */
    if (s.runaway) {
        json_t *rc = act("runaway_cut"); json_set(rc, "channel", json_string(s.runaway)); push(s.log, rc);
    }
    tc_materialize(&s.tc);
    if (attempt_cancelled) {
        json_t *a = act("raise"); json_set(a, "kind", json_string("RemoteProtocolError")); push(s.log, a);
        goto done;
    }
    if (terminal_error) goto done;
    if (jget(in, "stream_final_response")) {
        push(s.log, act("adopt_final_response"));
        goto done;
    }
    {
        /* The hookable boundary: _finish_chat_stream's inputs (accumulated state). */
        json_t *fc = act("finish_call");
        char *content = join_str_array(s.content_parts, "");
        char *reasoning = join_str_array(s.reasoning_parts, "");
        json_set(fc, "content", json_string(content ? content : ""));
        json_set(fc, "reasoning", json_string(reasoning ? reasoning : ""));
        free(content); free(reasoning);
        const char *eff = s.runaway ? "length" : s.finish_reason;
        set_str(fc, "finish_reason", eff);
        json_set(fc, "usage", s.usage_obj ? json_copy(s.usage_obj) : json_null());
        json_set(fc, "tool_names", json_copy(s.partial_tool_names));
        json_set(fc, "acc", json_copy(s.tc.acc));
        json_set(fc, "refusal_parts", json_copy(s.refusal_parts));
        json_set(fc, "reasoning_details", json_copy(s.reasoning_details));
        set_str(fc, "model", s.model_name);
        push(s.log, fc);
    }

done:
    json_free(s.content_parts); json_free(s.reasoning_parts); json_free(s.detail_display_parts);
    json_free(s.refusal_parts); json_free(s.reasoning_details); json_free(s.pending_text_parts);
    json_free(s.partial_tool_names);
    tc_free(&s.tc);
    free(s.finish_reason); free(s.model_name); free(s.response_id); free(s.upstream_provider);
    json_free(s.usage_obj);
    return s.log;
}
/* ===========================================================================
 * cch2 -- second-wave FLOW + leaf port of agent/chat_completion_helpers.py
 * ===========================================================================
 * Self-contained libjson-only section appended to the cch_flow port file.
 * Every symbol here is prefixed cch2_ (the cch_ / cch_flow_ names belong to
 * the first-wave port above). All helpers are static; the cch2_<fn> entries
 * are the only non-static symbols and need no header (the harness #includes
 * this .c directly).
 *
 * UNIVERSAL SEAM. Live I/O and claimed siblings are injected through one
 * callback:
 *     json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * `name` identifies the live call (table below), `args` is a BORROWED JSON
 * object (the dispatcher copies it into the trace; the port owns and frees
 * the args object it builds). The result is a NEW json_t owned by the port:
 *   - NULL                == Python None
 *   - {"__raise__":K,...}  == a raised exception of kind K (message = str(exc))
 *   - anything else        == the return value
 * A scripted JSON null result is returned as a NULL pointer (not a json_null
 * node) so truthiness tests read it as falsy. Fail-closed: a missing or
 * exhausted script yields {"__raise__":"RuntimeError","message":"no script
 * for <name>"} which the dispatcher records BEFORE the port propagates it.
 *
 * ENTRIES. json_t *cch2_<fn>(cch2_io_t *io, const json_t *in, json_t *st)
 *   io  -- universal seam channel (NULL => fail-closed production default)
 *   in  -- borrowed case input object (never mutated)
 *   st  -- fresh object the entry fills with the final state projection
 *          ({} for pure functions). Caller owns the returned json_t.
 * The return is the Python return value projected to JSON, or a raise spec
 * {"__raise__":kind,"message":str(exc)} when the live function raises.
 * Tuples project to JSON arrays; None projects to NULL.
 *
 * BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 * CancelledError) PASS THROUGH every `except Exception` / suppress site.
 *
 * Seam name -> live call table:
 *   env_float                       utils.env_float(name, default)
 *   env_has                         name in os.environ
 *   load_config_readonly            hermes_cli.config.load_config_readonly()
 *   load_config                     hermes_cli.config.load_config()
 *   time.time                       time.time()
 *   time.monotonic                  time.monotonic()
 *   estimate_request_context_tokens estimate_request_context_tokens(api_kwargs)
 *   scale_stale_timeout_for_context _scale_stale_timeout_for_context(base, est)
 *   get_reasoning_stale_timeout_floor agent.reasoning_timeouts.get_reasoning_stale_timeout_floor(model_id)
 *   bedrock_reasoning_stale_floor   _bedrock_reasoning_stale_floor(model_id)
 *   get_provider_stale_timeout      hermes_cli.timeouts.get_provider_stale_timeout(provider, model)
 *   model_dump                      obj.model_dump(warnings=False) / obj.model_dump()
 *   hint_fn                         agent._codex_silent_hang_hint(model=...)
 *   logger.info / logger.warning / logger.debug   logger.<lvl>(fmt, *args)
 *   get_hermes_home                 hermes_constants.get_hermes_home()
 *   profile_name_for_home           hermes_constants.profile_name_for_home(home)
 *   load_route_pool                 agent.custom_entry_pool.load_route_pool(provider, base_url)
 *   load_pool                       agent.credential_pool.load_pool(provider)
 *   pool_has_credentials            pool.has_credentials()
 *   pool_has_available              pool.has_available(model=...)
 *   pool_next_available_at          pool.next_available_at(model=...)
 *   get_model_context_length        agent.model_metadata.get_model_context_length(...)
 *   compressor.update_model         compressor.update_model(...)
 *   revalidate_compression_feasibility  ...revalidate_compression_feasibility(agent)
 *   resolve_reasoning_config        hermes_constants.resolve_reasoning_config(cfg, model)
 *   custom_provider_extra_body_for_agent  agent.agent_init._custom_provider_extra_body_for_agent(...)
 *   merge_custom_provider_extra_body      agent.agent_init._merge_custom_provider_extra_body(agent, cp)
 *   parse_api_mode                  hermes_cli.runtime_provider._parse_api_mode(value)
 *   get_named_custom_provider       hermes_cli.runtime_provider._get_named_custom_provider(provider)
 *   host_mandated_api_mode          hermes_cli.providers.host_mandated_api_mode(url)
 *   relay_llm.execute_current       agent.relay_llm.execute_current(...)
 *   is_router_timeout_shim          agent.transports.chat_completions.is_router_timeout_shim(response)
 *   transport.normalize_response    transport.normalize_response(response, **kw)
 *   emit_stream_start               agent._emit_stream_start()
 *   run                             run()
 *   emit_stream_end                 agent._emit_stream_end(final_text=,finished=,error=)
 *   stream_converse_with_callbacks  agent.bedrock_adapter.stream_converse_with_callbacks(kwargs)
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - str()-computed positions carry strings or integral numbers only (a
 *     parsed JSON number has no int/float identity; integral doubles print
 *     as ints). Non-integral floats and containers are excluded there.
 *   - strip()/lower() operate on ASCII whitespace / ASCII letters; fixtures
 *     are ASCII in those positions.
 *   - exc.body in _rejects_stream_options is a string or None (not a dict).
 *   - pool / compressor / transport doubles are OBJECTS: their Python
 *     truthiness is "is not None" (an object is always truthy), modeled as
 *     non-null-node, NOT json count>0.
 * =========================================================================== */

typedef struct cch2_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} cch2_io_t;

/* ---- truthiness / accessors ---- */

static int cch2_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

/* Object-double truthiness: an object is always truthy in Python; only
 * None (NULL / json_null) is falsy. Used for pool/compressor/transport. */
static int cch2_obj_truth(const json_t *v)
{
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

static const char *cch2_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *cch2_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static int cch2_has(const json_t *o, const char *k)
{
    return cch2_get(o, k) != NULL;
}

static double cch2_num(const json_t *v)
{
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

static json_t *cch2_copy_or_null(const json_t *v)
{
    return v ? json_copy(v) : NULL;
}

/* ---- raise specs ---- */

static int cch2_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cch2_kind(const json_t *r)
{
    const char *k = cch2_str(cch2_get(r, "__raise__"));
    return k ? k : "";
}

static int cch2_is_base(const char *kind)
{
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0;
}

static json_t *cch2_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

static json_t *cch2_attr_raise(const char *name)
{
    char buf[256];
    snprintf(buf, sizeof buf,
             "'types.SimpleNamespace' object has no attribute '%s'", name);
    return cch2_mkraise("AttributeError", buf);
}

/* malloc'd printf-style string builder */
static char *cch2_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* ---- universal seam call ----
 * args is OWNED by the caller (built fresh); borrowed by the dispatcher,
 * freed here. Result is caller-owned (NULL == None). Fail-closed when no
 * channel is installed. */
static json_t *cch2_call(cch2_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = cch2_xfmt("no seam installed for %s", name);
        r = cch2_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* convenience: build an args object */
static json_t *cch2_args(void)
{
    return json_object();
}

/* drop None-valued keys is done by the harness/oracle arg builders; the port
 * simply omits keys whose value is None by not setting them. */

/* ---- string helpers (ASCII domain) ---- */

static int cch2_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *cch2_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && cch2_is_ws((unsigned char)s[a])) a++;
    while (b > a && cch2_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *cch2_lower(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

/* Python str() projection (domain: strings / integral numbers / bool / None).
 * Caller frees. Containers are excluded from str() positions by the generator. */
static char *cch2_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return cch2_xfmt("None");
    if (v->type == JSON_BOOL) return cch2_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) return cch2_xfmt("%.0f", d);
        return cch2_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return cch2_xfmt("%s", v->str_val ? v->str_val : "");
    /* containers: compact JSON stand-in (excluded from fixtures) */
    char *s = json_serialize(v);
    return s ? s : cch2_xfmt("");
}

/* str(x or "") projection: falsy -> "", else str(x). Caller frees. */
static char *cch2_pystr_or_empty(const json_t *v)
{
    if (!cch2_truth(v)) return cch2_xfmt("");
    return cch2_pystr(v);
}

/* ---- Python equality (bool is a subclass of int: True == 1) ---- */
static int cch2_pyequal(const json_t *a, const json_t *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    int an = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    int bn = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_NULL: return 1;
    case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                    b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!cch2_pyequal(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    case JSON_OBJECT:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !cch2_pyequal(a->c.items[i], bv)) return 0;
        }
        return 1;
    default: return 0;
    }
}
#include <stdarg.h>
#include <math.h>
#include <errno.h>

/* ---- Python int()/float() coercion ----
 * Return the value; on failure set *err to a raise spec (caller propagates).
 * Messages probed on the quarry CPython 3.14.9. */

static const char *cch2_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL: return "bool";
    case JSON_NUMBER: return "int";   /* parsed JSON numbers read as int */
    case JSON_STRING: return "str";
    case JSON_ARRAY: return "list";
    case JSON_OBJECT: return "dict";
    default: return "object";
    }
}

/* validate every '_' sits between two digits (Python numeric underscore rule) */
static int cch2_underscores_ok(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n) return 0;
            unsigned char p = (unsigned char)s[i - 1], q = (unsigned char)s[i + 1];
            if (!(p >= '0' && p <= '9') || !(q >= '0' && q <= '9')) return 0;
        }
    }
    return 1;
}

/* build a clean digit buffer with underscores removed; caller frees */
static char *cch2_strip_underscores(const char *s, size_t n)
{
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] != '_') out[j++] = s[i];
    out[j] = '\0';
    return out;
}

static int cch2_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* validate a cleaned (no underscores, no leading sign) float mantissa+exponent:
 * (\d+\.?\d*|\.\d+)([eE][+-]?\d+)?  — rejects double signs, '1e', '1.2.3'. */
static int cch2_float_grammar_ok(const char *s)
{
    const char *p = s;
    int intdigits = 0, fracdigits = 0;
    while (*p >= '0' && *p <= '9') { p++; intdigits++; }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') { p++; fracdigits++; }
    }
    if (intdigits + fracdigits == 0) return 0;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        int expdigits = 0;
        while (*p >= '0' && *p <= '9') { p++; expdigits++; }
        if (expdigits == 0) return 0;
    }
    return *p == '\0';
}

/* parse a float string (already stripped, sign NOT yet consumed).
 * returns 1 on success with *out set; 0 on failure. */
static int cch2_parse_float_str(const char *s, double *out)
{
    if (!s || !*s) return 0;
    int neg = 0;
    const char *p = s;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return 0;
    /* inf / infinity / nan special forms (case-insensitive, full match) */
    if (cch2_ieq(p, "inf") || cch2_ieq(p, "infinity")) {
        *out = neg ? -INFINITY : INFINITY;
        return 1;
    }
    if (cch2_ieq(p, "nan")) { *out = NAN; return 1; }
    /* reject hex (strtod would accept it; Python float() does not) */
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) return 0;
    size_t n = strlen(p);
    if (!cch2_underscores_ok(p, n)) return 0;
    char *clean = cch2_strip_underscores(p, n);
    if (!clean) return 0;
    if (!cch2_float_grammar_ok(clean)) { free(clean); return 0; }
    char *full = cch2_xfmt("%s%s", neg ? "-" : "", clean);
    free(clean);
    if (!full) return 0;
    char *end = NULL;
    errno = 0;
    double d = strtod(full, &end);
    int ok = (end && *end == '\0' && end != full);
    free(full);
    if (!ok) return 0;
    *out = d;
    return 1;
}

/* parse an int string (base 10, already stripped). 1 on success. */
static int cch2_parse_int_str(const char *s, long long *out)
{
    if (!s || !*s) return 0;
    int neg = 0;
    const char *p = s;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return 0;
    size_t n = strlen(p);
    if (!cch2_underscores_ok(p, n)) return 0;
    int has_digit = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] >= '0' && p[i] <= '9') has_digit = 1;
        else if (p[i] != '_') return 0;
    }
    if (!has_digit) return 0;
    char *clean = cch2_strip_underscores(p, n);
    if (!clean) return 0;
    char *full = cch2_xfmt("%s%s", neg ? "-" : "", clean);
    free(clean);
    if (!full) return 0;
    char *end = NULL;
    errno = 0;
    long long v = strtoll(full, &end, 10);
    int ok = (end && *end == '\0' && end != full && errno != ERANGE);
    free(full);
    if (!ok) return 0;
    *out = v;
    return 1;
}

/* float(value) projection. *err set on failure (caller owns/propagates). */
static double cch2_pyfloat(const json_t *v, json_t **err)
{
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = cch2_mkraise("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
        return 0.0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        char *t = cch2_strip(v->str_val ? v->str_val : "");
        double d;
        int ok = t && cch2_parse_float_str(t, &d);
        if (!ok) {
            char *m = cch2_xfmt("could not convert string to float: '%s'",
                                v->str_val ? v->str_val : "");
            *err = cch2_mkraise("ValueError", m ? m : "");
            free(m);
        }
        free(t);
        return ok ? d : 0.0;
    }
    char *m = cch2_xfmt("float() argument must be a string or a real number, not '%s'",
                        cch2_typename(v));
    *err = cch2_mkraise("TypeError", m ? m : "");
    free(m);
    return 0.0;
}

/* int(value) projection. *err set on failure. */
static long long cch2_pyint(const json_t *v, json_t **err)
{
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = cch2_mkraise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)trunc(v->num_val);
    if (v->type == JSON_STRING) {
        char *t = cch2_strip(v->str_val ? v->str_val : "");
        long long d;
        int ok = t && cch2_parse_int_str(t, &d);
        if (!ok) {
            char *m = cch2_xfmt("invalid literal for int() with base 10: '%s'",
                                v->str_val ? v->str_val : "");
            *err = cch2_mkraise("ValueError", m ? m : "");
            free(m);
        }
        free(t);
        return ok ? d : 0;
    }
    char *m = cch2_xfmt("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                        cch2_typename(v));
    *err = cch2_mkraise("TypeError", m ? m : "");
    free(m);
    return 0;
}

/* logger seam: record fmt + args verbatim (args projected: exceptions -> str).
 * name is "logger.info" / "logger.warning" / "logger.debug". */
static json_t *cch2_log(cch2_io_t *io, const char *name, const char *fmt, json_t *args)
{
    json_t *a = cch2_args();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());
    return cch2_call(io, name, a);
}
/* ===========================================================================
 * Group A -- pure leaves
 * =========================================================================== */

static int cch2_contains(const char *hay, const char *needle)
{
    return hay && needle && strstr(hay, needle) != NULL;
}

/* PoP: cch2_is_provider_stream_empty_frame_error @ agent/chat_completion_helpers.py:_is_provider_stream_empty_frame_error */
json_t *cch2_is_provider_stream_empty_frame_error(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *exc = cch2_get(in, "exc");
    const json_t *body = cch2_get(exc, "body");
    const json_t *error_obj = (body && body->type == JSON_OBJECT) ? cch2_get(body, "error") : NULL;
    int r = 0;
    if (error_obj && error_obj->type == JSON_OBJECT) {
        const char *code = cch2_str(cch2_get(error_obj, "code"));
        r = code && strcmp(code, "provider_stream_empty_frame") == 0;
    }
    return json_bool(r);
}

/* PoP: cch2_rejects_stream_options @ agent/chat_completion_helpers.py:_rejects_stream_options */
json_t *cch2_rejects_stream_options(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *exc = cch2_get(in, "exc");
    const json_t *status = cch2_get(exc, "status_code");
    int ok_status = status && status->type == JSON_NUMBER
                    && (status->num_val == 400.0 || status->num_val == 422.0);
    if (!ok_status) return json_bool(0);
    char *body = cch2_pystr_or_empty(cch2_get(exc, "body"));
    const char *msg = cch2_str(cch2_get(exc, "message"));
    char *combined = cch2_xfmt("%s %s", body ? body : "", msg ? msg : "");
    free(body);
    char *low = cch2_lower(combined ? combined : "");
    free(combined);
    int r = cch2_contains(low, "stream_options") &&
        (cch2_contains(low, "extra") || cch2_contains(low, "not supported") ||
         cch2_contains(low, "unrecognized") || cch2_contains(low, "unexpected") ||
         cch2_contains(low, "unknown"));
    free(low);
    return json_bool(r);
}

static const char *CCH2_SSE_CONN_PHRASES[] = {
    "connection lost", "connection reset", "connection closed", "connection terminated",
    "network error", "network connection", "terminated", "peer closed", "broken pipe",
    "upstream connect error", NULL
};

/* Projection: in.is_api_error carries isinstance(exc, openai.APIError) (the oracle
 * builds exc as an instance of a stub openai.APIError class when true). */
/* PoP: cch2_is_sse_connection_error @ agent/chat_completion_helpers.py:_is_sse_connection_error */
json_t *cch2_is_sse_connection_error(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *isae = cch2_get(in, "is_api_error");
    if (!(isae && isae->type == JSON_BOOL && isae->bool_val)) return json_bool(0);
    const json_t *exc = cch2_get(in, "exc");
    if (cch2_truth(cch2_get(exc, "status_code"))) return json_bool(0);
    const char *msg = cch2_str(cch2_get(exc, "message"));
    char *low = cch2_lower(msg ? msg : "");
    int r = 0;
    for (size_t i = 0; CCH2_SSE_CONN_PHRASES[i]; i++)
        if (cch2_contains(low, CCH2_SSE_CONN_PHRASES[i])) { r = 1; break; }
    free(low);
    return json_bool(r);
}

/* PoP: cch2_relay_stream_identity @ agent/chat_completion_helpers.py:_relay_stream_identity */
json_t *cch2_relay_stream_identity(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *agent = cch2_get(in, "agent");
    const json_t *name_default = cch2_get(in, "name_default");
    char *session = cch2_pystr_or_empty(cch2_get(agent, "session_id"));
    const json_t *prov = cch2_get(agent, "provider");
    char *name = cch2_pystr(cch2_truth(prov) ? prov : name_default);
    char *model = cch2_pystr_or_empty(cch2_get(agent, "model"));
    json_t *o = json_object();
    json_set(o, "session_id", json_string(session ? session : ""));
    json_set(o, "name", json_string(name ? name : ""));
    json_set(o, "model_name", json_string(model ? model : ""));
    free(session); free(name); free(model);
    return o;
}

/* PoP: cch2_relay_stream_metadata @ agent/chat_completion_helpers.py:_relay_stream_metadata */
json_t *cch2_relay_stream_metadata(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *agent = cch2_get(in, "agent");
    const char *role;
    if (cch2_truth(cch2_get(agent, "is_subagent"))) {
        role = "delegated";
    } else {
        const json_t *fb = cch2_get(agent, "_fallback_index");
        json_t *zero = json_number(0);
        json_t *err = NULL;
        long long idx = cch2_pyint(cch2_truth(fb) ? fb : zero, &err);
        json_free(zero);
        if (err) return err;
        role = idx > 0 ? "fallback" : "primary";
    }
    json_t *o = json_object();
    const json_t *api_mode = cch2_get(in, "api_mode");
    json_set(o, "api_mode", api_mode ? json_copy(api_mode) : json_null());
    const json_t *rid = cch2_get(agent, "_current_api_request_id");
    json_set(o, "api_request_id", rid ? json_copy(rid) : json_null());
    json_set(o, "call_role", json_string(role));
    return o;
}

/* PoP: cch2_stream_final_text @ agent/chat_completion_helpers.py:_stream_final_text */
json_t *cch2_stream_final_text(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *response = cch2_get(in, "response");
    /* first suppress block: choices[0].message.content if str */
    const json_t *choices = cch2_get(response, "choices");
    if (choices && choices->type == JSON_ARRAY && choices->c.count > 0) {
        const json_t *first = json_get(choices, 0);
        const json_t *content = cch2_get(cch2_get(first, "message"), "content");
        const char *s = cch2_str(content);
        if (s) return json_string(s);
    }
    /* second suppress block: response.content str or list of parts */
    const json_t *content = cch2_get(response, "content");
    const char *s = cch2_str(content);
    if (s) return json_string(s);
    if (content && content->type == JSON_ARRAY) {
        size_t cap = 1;
        for (size_t i = 0; i < content->c.count; i++) {
            const char *t = cch2_str(cch2_get(json_get(content, i), "text"));
            if (t) cap += strlen(t);
        }
        char *buf = (char *)malloc(cap);
        if (buf) {
            size_t k = 0;
            for (size_t i = 0; i < content->c.count; i++) {
                const char *t = cch2_str(cch2_get(json_get(content, i), "text"));
                if (t) { size_t n = strlen(t); memcpy(buf + k, t, n); k += n; }
            }
            buf[k] = '\0';
            json_t *r = json_string(buf);
            free(buf);
            return r;
        }
    }
    return json_string("");
}

static const char *CCH2_EFFORT_LADDER[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", "max", "ultra", NULL
};

/* PoP: cch2_high_effort_silence_floor @ agent/chat_completion_helpers.py:_high_effort_silence_floor */
json_t *cch2_high_effort_silence_floor(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *agent = cch2_get(in, "agent");
    const json_t *cfg = cch2_get(agent, "reasoning_config");
    if (!cfg || cfg->type != JSON_OBJECT) return json_number(0.0);
    const json_t *enabled = cch2_get(cfg, "enabled");
    if (enabled && enabled->type == JSON_BOOL && !enabled->bool_val) return json_number(0.0);
    char *eff = cch2_pystr_or_empty(cch2_get(cfg, "effort"));
    char *t = cch2_strip(eff ? eff : "");
    free(eff);
    char *low = cch2_lower(t ? t : "");
    free(t);
    int idx = -1;
    for (int i = 0; CCH2_EFFORT_LADDER[i]; i++)
        if (strcmp(low, CCH2_EFFORT_LADDER[i]) == 0) { idx = i; break; }
    free(low);
    /* index("high") == 4 */
    if (idx < 0 || idx < 4) return json_number(0.0);
    return json_number(300.0);
}
/* ===========================================================================
 * Group B -- small state / seam flows
 * =========================================================================== */

/* PoP: cch2_consume_ephemeral_max_output @ agent/chat_completion_helpers.py:_consume_ephemeral_max_output */
json_t *cch2_consume_ephemeral_max_output(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)io;
    const json_t *agent = cch2_get(in, "agent");
    const json_t *out = cch2_get(agent, "_ephemeral_max_output_tokens");
    /* final state is always None: a present value is cleared, an absent/null
     * value stays None. */
    json_set(st, "_ephemeral_max_output_tokens", json_null());
    return cch2_copy_or_null(out);
}

/* PoP: cch2_model_dump_safe @ agent/chat_completion_helpers.py:_model_dump_safe */
json_t *cch2_model_dump_safe(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *a = cch2_args();
    json_set(a, "warnings", json_bool(0));
    json_t *r = cch2_call(io, "model_dump", a);
    if (cch2_is_raise(r) && strcmp(cch2_kind(r), "TypeError") == 0) {
        json_free(r);
        r = cch2_call(io, "model_dump", cch2_args());
    }
    return r;
}

/* Projection: in.has_model_dump carries hasattr(value, "model_dump"). */
/* PoP: cch2_dump_if_model @ agent/chat_completion_helpers.py:_dump_if_model */
json_t *cch2_dump_if_model(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *has = cch2_get(in, "has_model_dump");
    if (has && has->type == JSON_BOOL && has->bool_val)
        return cch2_model_dump_safe(io, in, st);
    return cch2_copy_or_null(cch2_get(in, "value"));
}

/* Projection: in.hint_callable carries callable(getattr(agent,"_codex_silent_hang_hint",None)). */
/* PoP: cch2_codex_silent_hang_hint @ agent/chat_completion_helpers.py:_codex_silent_hang_hint */
json_t *cch2_codex_silent_hang_hint(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *hc = cch2_get(in, "hint_callable");
    if (!(hc && hc->type == JSON_BOOL && hc->bool_val)) return NULL;
    const json_t *api_kwargs = cch2_get(in, "api_kwargs");
    json_t *a = cch2_args();
    const json_t *model = cch2_get(api_kwargs, "model");
    if (model && model->type != JSON_NULL) json_set(a, "model", json_copy(model));
    json_t *r = cch2_call(io, "hint_fn", a);
    if (cch2_is_raise(r)) {
        if (cch2_is_base(cch2_kind(r))) return r;
        json_free(r);
        return NULL;   /* suppress(Exception) */
    }
    return r;
}

/* PoP: cch2_stream_env_stale_base @ agent/chat_completion_helpers.py:_stream_env_stale_base */
json_t *cch2_stream_env_stale_base(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    json_t *a = cch2_args();
    json_set(a, "name", json_string("HERMES_STREAM_STALE_TIMEOUT"));
    json_set(a, "default", json_number(180.0));
    json_t *r1 = cch2_call(io, "env_float", a);
    if (cch2_is_raise(r1)) return r1;
    json_t *a2 = cch2_args();
    json_set(a2, "name", json_string("HERMES_STREAM_STALE_TIMEOUT"));
    json_t *r2 = cch2_call(io, "env_has", a2);
    if (cch2_is_raise(r2)) { json_free(r1); return r2; }
    json_t *arr = json_array();
    json_append(arr, r1 ? r1 : json_null());
    json_append(arr, r2 ? r2 : json_null());
    return arr;
}

/* PoP: cch2_local_stream_stale_timeout_default @ agent/chat_completion_helpers.py:_local_stream_stale_timeout_default */
json_t *cch2_local_stream_stale_timeout_default(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)in; (void)st;
    double local_default = 900.0;
    json_t *cfg = cch2_call(io, "load_config_readonly", cch2_args());
    if (cch2_is_raise(cfg)) {
        if (cch2_is_base(cch2_kind(cfg))) return cfg;
        json_free(cfg);
        cfg = NULL;   /* suppress(Exception): keep 900.0 */
    }
    if (cfg) {
        const json_t *agent_cfg = (cfg->type == JSON_OBJECT) ? cch2_get(cfg, "agent") : NULL;
        const json_t *value = (agent_cfg && agent_cfg->type == JSON_OBJECT)
                              ? cch2_get(agent_cfg, "local_stream_stale_timeout") : NULL;
        if (value && (value->type == JSON_NUMBER || value->type == JSON_BOOL))
            local_default = value->type == JSON_BOOL ? (value->bool_val ? 1.0 : 0.0) : value->num_val;
        json_free(cfg);
    }
    json_t *a = cch2_args();
    json_set(a, "name", json_string("HERMES_LOCAL_STREAM_STALE_TIMEOUT"));
    json_set(a, "default", json_number(local_default));
    return cch2_call(io, "env_float", a);   /* outside suppress: propagates */
}

/* PoP: cch2_cap_to_run_budget @ agent/chat_completion_helpers.py:cap_to_run_budget */
json_t *cch2_cap_to_run_budget(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *agent = cch2_get(in, "agent");
    const json_t *timeout = cch2_get(in, "timeout");
    const json_t *run_budget = cch2_get(agent, "run_budget_seconds");
    const json_t *started = cch2_get(agent, "_run_budget_started_at");
    if (!cch2_truth(run_budget) || !cch2_truth(started))
        return cch2_copy_or_null(timeout);
    /* eval order: float(run_budget), time.time(), float(started) */
    json_t *err = NULL;
    double fb = cch2_pyfloat(run_budget, &err);
    if (err) return err;
    json_t *t = cch2_call(io, "time.time", cch2_args());
    if (cch2_is_raise(t)) return t;
    double tn = cch2_num(t);
    json_free(t);
    double fs = cch2_pyfloat(started, &err);
    if (err) return err;
    double remaining = fb - (tn - fs);
    double half = remaining * 0.5;
    if (half < 60.0) half = 60.0;
    double to = cch2_num(timeout);
    return json_number(to < half ? to : half);
}
/* ===========================================================================
 * Group C -- stale-timeout composition (claimed siblings are seams)
 * =========================================================================== */

/* core of _cloud_stale_timeout: base + api_kwargs borrowed */
static json_t *cch2_cloud_stale_timeout_core(cch2_io_t *io, const json_t *base,
                                             const json_t *api_kwargs)
{
    json_t *a = cch2_args();
    json_set(a, "api_kwargs", cch2_copy_or_null(api_kwargs));
    json_t *est = cch2_call(io, "estimate_request_context_tokens", a);
    if (cch2_is_raise(est)) return est;

    json_t *a2 = cch2_args();
    json_set(a2, "base", cch2_copy_or_null(base));
    json_set(a2, "est_tokens", est ? json_copy(est) : json_null());
    json_t *timeout = cch2_call(io, "scale_stale_timeout_for_context", a2);
    json_free(est);
    if (cch2_is_raise(timeout)) return timeout;

    /* api_kwargs.get("model") or api_kwargs.get("modelId") or "" */
    const json_t *m = cch2_get(api_kwargs, "model");
    if (!cch2_truth(m)) m = cch2_get(api_kwargs, "modelId");
    json_t *m_owned = NULL;
    if (!cch2_truth(m)) { m_owned = json_string(""); m = m_owned; }
    json_t *a3 = cch2_args();
    json_set(a3, "model_id", json_copy(m));
    json_free(m_owned);
    json_t *floor = cch2_call(io, "get_reasoning_stale_timeout_floor", a3);
    if (cch2_is_raise(floor)) { json_free(timeout); return floor; }

    const json_t *model_id = cch2_get(api_kwargs, "modelId");
    if ((!floor || floor->type == JSON_NULL) && cch2_truth(model_id)) {
        json_t *a4 = cch2_args();
        json_set(a4, "model_id", json_copy(model_id));
        json_t *floor2 = cch2_call(io, "bedrock_reasoning_stale_floor", a4);
        if (cch2_is_raise(floor2)) { json_free(timeout); json_free(floor); return floor2; }
        json_free(floor);
        floor = floor2;
    }
    double to = cch2_num(timeout);
    double result = to;
    if (floor && floor->type != JSON_NULL) {
        double fl = cch2_num(floor);
        if (fl > result) result = fl;
    }
    json_free(timeout);
    json_free(floor);
    return json_number(result);
}

/* PoP: cch2_cloud_stale_timeout @ agent/chat_completion_helpers.py:_cloud_stale_timeout */
json_t *cch2_cloud_stale_timeout(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return cch2_cloud_stale_timeout_core(io, cch2_get(in, "base"), cch2_get(in, "api_kwargs"));
}

/* required attribute access (agent.provider / agent.model): missing -> raise */
static const json_t *cch2_req(const json_t *agent, const char *name, json_t **err)
{
    const json_t *v = cch2_get(agent, name);
    if (!v) *err = cch2_attr_raise(name);
    return v;
}

/* PoP: cch2_stream_stale_timeout_is_explicit @ agent/chat_completion_helpers.py:_stream_stale_timeout_is_explicit */
json_t *cch2_stream_stale_timeout_is_explicit(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *agent = cch2_get(in, "agent");
    json_t *err = NULL;
    const json_t *prov = cch2_req(agent, "provider", &err);
    if (err) return err;
    const json_t *model = cch2_req(agent, "model", &err);
    if (err) return err;
    json_t *a = cch2_args();
    if (prov->type != JSON_NULL) json_set(a, "provider", json_copy(prov));
    if (model->type != JSON_NULL) json_set(a, "model", json_copy(model));
    json_t *explicit = cch2_call(io, "get_provider_stale_timeout", a);
    if (cch2_is_raise(explicit)) return explicit;
    if (explicit && explicit->type != JSON_NULL) { json_free(explicit); return json_bool(1); }
    json_free(explicit);
    json_t *arr = cch2_stream_env_stale_base(io, in, st);
    if (cch2_is_raise(arr)) return arr;
    int explicit_env = cch2_truth(json_get(arr, 1));
    json_free(arr);
    return json_bool(explicit_env);
}

/* PoP: cch2_cloud_stale_timeout_for @ agent/chat_completion_helpers.py:_cloud_stale_timeout_for */
json_t *cch2_cloud_stale_timeout_for(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *agent = cch2_get(in, "agent");
    const json_t *api_kwargs = cch2_get(in, "api_kwargs");
    json_t *err = NULL;
    const json_t *prov = cch2_req(agent, "provider", &err);
    if (err) return err;
    const json_t *model = cch2_req(agent, "model", &err);
    if (err) return err;
    json_t *a = cch2_args();
    if (prov->type != JSON_NULL) json_set(a, "provider", json_copy(prov));
    if (model->type != JSON_NULL) json_set(a, "model", json_copy(model));
    json_t *explicit = cch2_call(io, "get_provider_stale_timeout", a);
    if (cch2_is_raise(explicit)) return explicit;
    if (explicit && explicit->type != JSON_NULL) return explicit;   /* wins over everything */
    json_free(explicit);

    json_t *arr = cch2_stream_env_stale_base(io, in, st);
    if (cch2_is_raise(arr)) return arr;
    json_t *base = json_copy(json_get(arr, 0));
    int explicit_env = cch2_truth(json_get(arr, 1));
    json_free(arr);

    json_t *timeout = cch2_cloud_stale_timeout_core(io, base, api_kwargs);
    json_free(base);
    if (cch2_is_raise(timeout)) return timeout;
    if (explicit_env) return timeout;

    json_t *in3 = json_object();
    json_set(in3, "agent", cch2_copy_or_null(agent));
    json_set(in3, "timeout", json_copy(timeout));
    json_free(timeout);
    json_t *r = cch2_cap_to_run_budget(io, in3, st);
    json_free(in3);
    return r;
}
/* ===========================================================================
 * Group D -- provider-fallback flows
 * =========================================================================== */

#define CCH2_FMT_FALLBACK_INFO "Fallback activated: %s \xe2\x86\x92 %s (%s)"
#define CCH2_FMT_BILLING_WARN "Profile %s: %s via %s refused for billing/credits \xe2\x80\x94 using fallback %s via %s. Top up credits, or run `%s` to pick a model this account can use."
#define CCH2_FMT_POOL_CLEAR "Fallback to %s/%s: clearing primary credential pool (pool_provider=%s) to prevent cross-provider contamination"
#define CCH2_FMT_POOL_ATTACH "Fallback to %s/%s: attached fallback credential pool"
#define CCH2_FMT_POOL_DEBUG "Fallback to %s/%s: could not attach credential pool: %s"
#define CCH2_FMT_RC_INFO "Fallback %s: reasoning_config resolved: %s"
#define CCH2_FMT_RC_DEBUG "Failed to resolve reasoning_config for fallback %s; keeping current: %s"
#define CCH2_FMT_EB_INFO "Fallback %s: extra_body resolved: %s"
#define CCH2_FMT_EB_DEBUG "Failed to resolve extra_body for fallback %s; keeping current: %s"

static const char *CCH2_RATE_LIMIT_REASONS[] = { "rate_limit", "billing", "upstream_rate_limit", NULL };

static int cch2_reason_in_set(const char *r)
{
    if (!r) return 0;
    for (size_t i = 0; CCH2_RATE_LIMIT_REASONS[i]; i++)
        if (strcmp(r, CCH2_RATE_LIMIT_REASONS[i]) == 0) return 1;
    return 0;
}

/* PoP: cch2_fallback_chain_exhausted @ agent/chat_completion_helpers.py:_fallback_chain_exhausted */
json_t *cch2_fallback_chain_exhausted(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *agent = cch2_get(in, "agent");
    json_t *err = NULL;
    const json_t *chain = cch2_req(agent, "_fallback_chain", &err);
    if (err) return err;
    const char *reason = cch2_str(cch2_get(in, "reason"));
    const json_t *old = cch2_get(agent, "_rate_limited_until");
    double old_eff = cch2_truth(old) ? cch2_num(old) : 0.0;
    double final = old_eff;
    if (cch2_truth(chain) && !cch2_reason_in_set(reason)) {
        json_t *mono = cch2_call(io, "time.monotonic", cch2_args());
        if (cch2_is_raise(mono)) return mono;
        double cand = cch2_num(mono) + 5.0;
        json_free(mono);
        if (cand > old_eff) final = cand;
    }
    json_set(st, "_rate_limited_until", json_number(final));
    return json_bool(0);
}

/* PoP: cch2_log_fallback_activated @ agent/chat_completion_helpers.py:_log_fallback_activated */
json_t *cch2_log_fallback_activated(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *reason = cch2_str(cch2_get(in, "reason"));
    const json_t *old_model = cch2_get(in, "old_model");
    const json_t *old_provider = cch2_get(in, "old_provider");
    const json_t *fb_model = cch2_get(in, "fb_model");
    const json_t *fb_provider = cch2_get(in, "fb_provider");
    if (!reason || strcmp(reason, "billing") != 0) {
        json_t *args = json_array();
        json_append(args, cch2_copy_or_null(old_model));
        json_append(args, cch2_copy_or_null(fb_model));
        json_append(args, cch2_copy_or_null(fb_provider));
        json_t *r = cch2_log(io, "logger.info", CCH2_FMT_FALLBACK_INFO, args);
        if (cch2_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    json_t *home = cch2_call(io, "get_hermes_home", cch2_args());
    if (cch2_is_raise(home)) return home;
    json_t *a = cch2_args();
    json_set(a, "home", cch2_copy_or_null(home));
    json_t *prof = cch2_call(io, "profile_name_for_home", a);
    json_free(home);
    if (cch2_is_raise(prof)) return prof;
    char *profile = cch2_truth(prof) ? cch2_pystr(prof) : cch2_xfmt("default");
    json_free(prof);
    char *remedy = (strcmp(profile, "default") == 0) ? cch2_xfmt("hermes model")
                   : cch2_xfmt("hermes -p %s model", profile);
    json_t *args = json_array();
    json_append(args, json_string(profile));
    json_append(args, cch2_copy_or_null(old_model));
    json_append(args, cch2_copy_or_null(old_provider));
    json_append(args, cch2_copy_or_null(fb_model));
    json_append(args, cch2_copy_or_null(fb_provider));
    json_append(args, json_string(remedy));
    free(profile); free(remedy);
    json_t *r = cch2_log(io, "logger.warning", CCH2_FMT_BILLING_WARN, args);
    if (cch2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* log a debug line whose last arg is the str(exc) of a caught raise spec */
static json_t *cch2_log_debug_exc(cch2_io_t *io, const char *fmt,
                                  const json_t *fb_provider, const json_t *fb_model,
                                  const json_t *caught)
{
    json_t *args = json_array();
    json_append(args, cch2_copy_or_null(fb_provider));
    json_append(args, cch2_copy_or_null(fb_model));
    const char *msg = cch2_str(cch2_get(caught, "message"));
    json_append(args, json_string(msg ? msg : ""));
    return cch2_log(io, "logger.debug", fmt, args);
}

/* PoP: cch2_rebind_fallback_credential_pool @ agent/chat_completion_helpers.py:_rebind_fallback_credential_pool */
json_t *cch2_rebind_fallback_credential_pool(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *agent = cch2_get(in, "agent");
    const json_t *fb_provider = cch2_get(in, "fb_provider");
    const json_t *fb_model = cch2_get(in, "fb_model");
    const json_t *prefetched = cch2_get(in, "prefetched_pool");
    int prefetched_notnull = cch2_obj_truth(prefetched);
    const json_t *orig_pool = cch2_get(agent, "_credential_pool");
    const json_t *orig_entry = cch2_get(agent, "_credential_pool_entry_id");

    char *pp0 = cch2_pystr_or_empty(cch2_get(orig_pool, "provider"));
    char *pp1 = cch2_strip(pp0 ? pp0 : ""); free(pp0);
    char *pool_provider = cch2_lower(pp1 ? pp1 : ""); free(pp1);
    const char *fbp = cch2_str(fb_provider);
    int cond = prefetched_notnull ||
        (strcmp(pool_provider, "") != 0 && strcmp(pool_provider, fbp ? fbp : "") != 0);

    json_t *pool_final = cch2_copy_or_null(orig_pool);
    json_t *entry_final = cch2_copy_or_null(orig_entry);

    if (cond) {
        if (!prefetched_notnull) {
            json_t *args = json_array();
            json_append(args, cch2_copy_or_null(fb_provider));
            json_append(args, cch2_copy_or_null(fb_model));
            json_append(args, json_string(pool_provider));
            json_t *li = cch2_log(io, "logger.info", CCH2_FMT_POOL_CLEAR, args);
            if (cch2_is_raise(li)) { free(pool_provider); json_free(pool_final); json_free(entry_final); return li; }
            json_free(li);
        }
        json_free(pool_final); pool_final = NULL;
        json_free(entry_final); entry_final = NULL;
    }
    free(pool_provider);

    if (!cch2_obj_truth(pool_final)) {
        json_t *fallback = NULL; int fb_owned = 0; json_t *caught = NULL;
        if (prefetched_notnull) {
            fallback = (json_t *)prefetched;   /* borrowed; always truthy object */
        } else {
            json_t *a = cch2_args();
            json_set(a, "provider", cch2_copy_or_null(fb_provider));
            const json_t *burl = cch2_get(agent, "base_url");
            if (burl && burl->type != JSON_NULL) json_set(a, "base_url", json_copy(burl));
            fallback = cch2_call(io, "load_route_pool", a);
            fb_owned = 1;
            if (cch2_is_raise(fallback)) {
                if (cch2_is_base(cch2_kind(fallback))) { json_free(pool_final); json_free(entry_final); return fallback; }
                caught = fallback; fallback = NULL; fb_owned = 0;
                goto debug;
            }
        }
        if (cch2_obj_truth(fallback)) {
            json_t *hc = cch2_call(io, "pool_has_credentials", cch2_args());
            if (cch2_is_raise(hc)) {
                if (cch2_is_base(cch2_kind(hc))) { if (fb_owned) json_free(fallback); json_free(pool_final); json_free(entry_final); return hc; }
                caught = hc; goto debug_fb;
            }
            if (cch2_truth(hc)) {
                json_free(pool_final); pool_final = json_copy(fallback);
                json_t *args = json_array();
                json_append(args, cch2_copy_or_null(fb_provider));
                json_append(args, cch2_copy_or_null(fb_model));
                json_t *li = cch2_log(io, "logger.info", CCH2_FMT_POOL_ATTACH, args);
                if (cch2_is_raise(li)) {
                    if (cch2_is_base(cch2_kind(li))) { if (fb_owned) json_free(fallback); json_free(pool_final); json_free(entry_final); return li; }
                    caught = li; goto debug_fb;
                }
                json_free(li);
            }
            json_free(hc);
        }
        if (fb_owned) json_free(fallback);
        goto done;
    debug_fb:
        if (fb_owned) json_free(fallback);
    debug:
        {
            json_t *dl = cch2_log_debug_exc(io, CCH2_FMT_POOL_DEBUG, fb_provider, fb_model, caught);
            json_free(caught);
            if (cch2_is_raise(dl)) { json_free(pool_final); json_free(entry_final); return dl; }
            json_free(dl);
        }
    }
done:
    json_set(st, "_credential_pool", pool_final ? pool_final : json_null());
    json_set(st, "_credential_pool_entry_id", entry_final ? entry_final : json_null());
    return NULL;
}

/* PoP: cch2_candidate_pool_exhausted @ agent/chat_completion_helpers.py:_candidate_pool_exhausted */
json_t *cch2_candidate_pool_exhausted(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *agent = cch2_get(in, "agent");
    const json_t *fb_provider = cch2_get(in, "fb_provider");
    const json_t *fb_model = cch2_get(in, "fb_model");
    const json_t *pool = cch2_get(agent, "_credential_pool");
    char *pp0 = cch2_pystr_or_empty(cch2_get(pool, "provider"));
    char *pp1 = cch2_strip(pp0 ? pp0 : ""); free(pp0);
    char *pp = cch2_lower(pp1 ? pp1 : ""); free(pp1);
    const char *fbp = cch2_str(fb_provider);
    int need_load = !cch2_obj_truth(pool) || strcmp(pp, fbp ? fbp : "") != 0;
    free(pp);
    json_t *loaded = NULL;
    if (need_load) {
        json_t *a = cch2_args();
        json_set(a, "provider", cch2_copy_or_null(fb_provider));
        loaded = cch2_call(io, "load_pool", a);
        if (cch2_is_raise(loaded)) {
            if (cch2_is_base(cch2_kind(loaded))) return loaded;
            json_free(loaded);
            return json_bool(0);   /* except Exception -> False */
        }
        pool = loaded;
    }
    if (!cch2_obj_truth(pool)) { json_free(loaded); return json_bool(0); }
    json_t *hc = cch2_call(io, "pool_has_credentials", cch2_args());
    if (cch2_is_raise(hc)) { json_free(loaded); return hc; }
    if (!cch2_truth(hc)) { json_free(hc); json_free(loaded); return json_bool(0); }
    json_free(hc);
    json_t *a2 = cch2_args();
    json_set(a2, "model", cch2_copy_or_null(fb_model));
    json_t *ha = cch2_call(io, "pool_has_available", a2);
    if (cch2_is_raise(ha)) { json_free(loaded); return ha; }
    if (cch2_truth(ha)) { json_free(ha); json_free(loaded); return json_bool(0); }
    json_free(ha);
    json_t *a3 = cch2_args();
    json_set(a3, "model", cch2_copy_or_null(fb_model));
    json_t *until = cch2_call(io, "pool_next_available_at", a3);
    if (cch2_is_raise(until)) { json_free(loaded); return until; }
    if (!until || until->type == JSON_NULL) { json_free(until); json_free(loaded); return json_bool(1); }
    json_t *t = cch2_call(io, "time.time", cch2_args());
    if (cch2_is_raise(t)) { json_free(until); json_free(loaded); return t; }
    int r = (cch2_num(until) - cch2_num(t)) > 600.0;
    json_free(until); json_free(t); json_free(loaded);
    return json_bool(r);
}
/* PoP: cch2_update_fallback_context_compressor @ agent/chat_completion_helpers.py:_update_fallback_context_compressor */
json_t *cch2_update_fallback_context_compressor(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *agent = cch2_get(in, "agent");
    const json_t *compressor = cch2_get(agent, "context_compressor");
    if (!cch2_obj_truth(compressor)) return NULL;
    json_t *err = NULL;
    const json_t *model = cch2_req(agent, "model", &err); if (err) return err;
    const json_t *base_url = cch2_req(agent, "base_url", &err); if (err) return err;
    const json_t *api_key = cch2_req(agent, "api_key", &err); if (err) return err;
    const json_t *provider = cch2_req(agent, "provider", &err); if (err) return err;
    json_t *a = cch2_args();
    json_set(a, "model", json_copy(model));
    json_set(a, "base_url", json_copy(base_url));
    json_set(a, "api_key", (api_key->type == JSON_STRING) ? json_copy(api_key) : json_string(""));
    json_set(a, "provider", json_copy(provider));
    const json_t *ccl = cch2_get(agent, "_config_context_length");
    if (ccl && ccl->type != JSON_NULL) json_set(a, "config_context_length", json_copy(ccl));
    const json_t *cps = cch2_get(agent, "_custom_providers");
    if (cps && cps->type != JSON_NULL) json_set(a, "custom_providers", json_copy(cps));
    json_t *fb_ctx = cch2_call(io, "get_model_context_length", a);
    if (cch2_is_raise(fb_ctx)) return fb_ctx;

    const json_t *api_mode = cch2_req(agent, "api_mode", &err);
    if (err) { json_free(fb_ctx); return err; }
    json_t *a2 = cch2_args();
    json_set(a2, "model", json_copy(model));
    json_set(a2, "context_length", cch2_copy_or_null(fb_ctx));
    json_set(a2, "base_url", json_copy(base_url));
    const json_t *ak2 = cch2_get(agent, "api_key");
    json_set(a2, "api_key", ak2 ? json_copy(ak2) : json_string(""));
    json_set(a2, "provider", json_copy(provider));
    json_set(a2, "api_mode", json_copy(api_mode));
    json_t *um = cch2_call(io, "compressor.update_model", a2);
    if (cch2_is_raise(um)) { json_free(fb_ctx); return um; }
    json_free(um);

    const json_t *checked = cch2_get(agent, "_compression_feasibility_checked");
    if (checked && checked->type == JSON_BOOL && checked->bool_val) {
        json_t *rv = cch2_call(io, "revalidate_compression_feasibility", cch2_args());
        if (cch2_is_raise(rv)) { json_free(fb_ctx); return rv; }
        json_free(rv);
    }
    json_free(fb_ctx);
    return NULL;
}

/* PoP: cch2_reresolve_fallback_reasoning_config @ agent/chat_completion_helpers.py:_reresolve_fallback_reasoning_config */
json_t *cch2_reresolve_fallback_reasoning_config(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *agent = cch2_get(in, "agent");
    const json_t *orig_rc = cch2_get(agent, "reasoning_config");
    json_t *rc_final = NULL;
    json_t *caught = NULL;

    json_t *cfg = cch2_call(io, "load_config", cch2_args());
    if (cch2_is_raise(cfg)) {
        if (cch2_is_base(cch2_kind(cfg))) return cfg;
        caught = cfg; goto debug;
    }
    {
        json_t *cfg_arg = cch2_truth(cfg) ? json_copy(cfg) : json_object();
        json_free(cfg);
        json_t *err = NULL;
        const json_t *model = cch2_req(agent, "model", &err);
        if (err) { json_free(cfg_arg); caught = err; goto debug; }
        json_t *a = cch2_args();
        json_set(a, "config", cfg_arg);
        json_set(a, "model", json_copy(model));
        json_t *rc = cch2_call(io, "resolve_reasoning_config", a);
        if (cch2_is_raise(rc)) {
            if (cch2_is_base(cch2_kind(rc))) return rc;
            caught = rc; goto debug;
        }
        rc_final = rc;
        json_t *args = json_array();
        json_append(args, json_copy(model));
        json_append(args, json_copy(rc_final));
        json_t *li = cch2_log(io, "logger.info", CCH2_FMT_RC_INFO, args);
        if (cch2_is_raise(li)) {
            if (cch2_is_base(cch2_kind(li))) { json_free(rc_final); return li; }
            caught = li; goto debug;
        }
        json_free(li);
    }
    goto done;
debug:
    {
        json_t *err = NULL;
        const json_t *model = cch2_req(agent, "model", &err);
        if (err) { json_free(caught); return err; }
        json_t *args = json_array();
        json_append(args, json_copy(model));
        const char *msg = cch2_str(cch2_get(caught, "message"));
        json_append(args, json_string(msg ? msg : ""));
        json_free(caught);
        json_t *dl = cch2_log(io, "logger.debug", CCH2_FMT_RC_DEBUG, args);
        if (cch2_is_raise(dl)) return dl;
        json_free(dl);
    }
done:
    json_set(st, "reasoning_config",
             rc_final ? rc_final : (orig_rc ? json_copy(orig_rc) : json_null()));
    return NULL;
}

/* PoP: cch2_rescope_fallback_extra_body @ agent/chat_completion_helpers.py:_rescope_fallback_extra_body */
json_t *cch2_rescope_fallback_extra_body(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *agent = cch2_get(in, "agent");
    const json_t *old_model = cch2_get(in, "old_model");
    const json_t *old_provider = cch2_get(in, "old_provider");
    const json_t *old_base_url = cch2_get(in, "old_base_url");
    const json_t *orig_ro = cch2_get(agent, "request_overrides");
    json_t *overrides = cch2_truth(orig_ro) ? json_copy(orig_ro) : json_object();
    const json_t *cp_node = cch2_get(agent, "_custom_providers");
    json_t *cp = cch2_truth(cp_node) ? json_copy(cp_node) : json_array();
    json_t *caught = NULL;

    json_t *a = cch2_args();
    json_set(a, "provider", cch2_copy_or_null(old_provider));
    json_set(a, "model", cch2_copy_or_null(old_model));
    json_set(a, "base_url", cch2_copy_or_null(old_base_url));
    json_set(a, "custom_providers", json_copy(cp));
    json_t *old_res = cch2_call(io, "custom_provider_extra_body_for_agent", a);
    if (cch2_is_raise(old_res)) {
        if (cch2_is_base(cch2_kind(old_res))) { json_free(overrides); json_free(cp); return old_res; }
        caught = old_res; goto debug;
    }
    {
        json_t *old_eb = cch2_truth(old_res) ? old_res : json_object();
        const json_t *existing_eb = cch2_get(overrides, "extra_body");
        if (existing_eb && existing_eb->type == JSON_OBJECT && cch2_truth(old_eb)) {
            json_t *scrubbed = json_object();
            for (size_t i = 0; i < existing_eb->c.count; i++) {
                const char *k = existing_eb->c.keys[i];
                json_t *v = existing_eb->c.items[i];
                json_t *ov = cch2_get(old_eb, k);
                if (!(ov && cch2_pyequal(v, ov)))
                    json_set(scrubbed, k, json_copy(v));
            }
            if (scrubbed->c.count > 0) {
                json_set(overrides, "extra_body", scrubbed);
            } else {
                json_obj_del(overrides, "extra_body");
                json_free(scrubbed);
            }
        }
        if (old_eb != old_res) json_free(old_eb);
        json_free(old_res);
    }
    {
        json_t *a2 = cch2_args();
        json_set(a2, "custom_providers", json_copy(cp));
        json_t *mg = cch2_call(io, "merge_custom_provider_extra_body", a2);
        if (cch2_is_raise(mg)) {
            if (cch2_is_base(cch2_kind(mg))) { json_free(overrides); json_free(cp); return mg; }
            caught = mg; goto debug;
        }
        json_free(mg);
    }
    {
        json_t *err = NULL;
        const json_t *model = cch2_req(agent, "model", &err);
        if (err) { json_free(overrides); json_free(cp); caught = err; goto debug; }
        const json_t *eb_val = cch2_get(overrides, "extra_body");
        json_t *args = json_array();
        json_append(args, json_copy(model));
        json_append(args, eb_val ? json_copy(eb_val) : json_null());
        json_t *li = cch2_log(io, "logger.info", CCH2_FMT_EB_INFO, args);
        if (cch2_is_raise(li)) {
            if (cch2_is_base(cch2_kind(li))) { json_free(overrides); json_free(cp); return li; }
            caught = li; goto debug;
        }
        json_free(li);
    }
    json_free(cp);
    goto done;
debug:
    {
        json_free(cp);
        json_t *err = NULL;
        const json_t *model = cch2_req(agent, "model", &err);
        if (err) { json_free(caught); json_free(overrides); return err; }
        json_t *args = json_array();
        json_append(args, json_copy(model));
        const char *msg = cch2_str(cch2_get(caught, "message"));
        json_append(args, json_string(msg ? msg : ""));
        json_free(caught);
        json_t *dl = cch2_log(io, "logger.debug", CCH2_FMT_EB_DEBUG, args);
        if (cch2_is_raise(dl)) { json_free(overrides); return dl; }
        json_free(dl);
    }
done:
    json_set(st, "request_overrides", overrides);
    return NULL;
}
/* ===========================================================================
 * Group E -- api-mode hint + summary / stream bracket flows
 * =========================================================================== */

#define CCH2_FMT_SHIM_WARN "Iteration summary returned a router timeout shim; retrying"
#define CCH2_FMT_TOOLS_WARN "Iteration summary emitted tool calls; discarding them"

/* core of _is_anthropic_wire_url */
static json_t *cch2_is_anthropic_wire_url_core(cch2_io_t *io, const json_t *url)
{
    json_t *a = cch2_args();
    json_set(a, "url", cch2_copy_or_null(url));
    json_t *r = cch2_call(io, "host_mandated_api_mode", a);
    if (cch2_is_raise(r)) return r;
    const char *s = cch2_str(r);
    int yes = s && strcmp(s, "anthropic_messages") == 0;
    json_free(r);
    return json_bool(yes);
}

/* PoP: cch2_is_anthropic_wire_url @ agent/chat_completion_helpers.py:_is_anthropic_wire_url */
json_t *cch2_is_anthropic_wire_url(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return cch2_is_anthropic_wire_url_core(io, cch2_get(in, "url"));
}

/* PoP: cch2_fallback_api_mode_hint @ agent/chat_completion_helpers.py:_fallback_api_mode_hint */
json_t *cch2_fallback_api_mode_hint(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *fb = cch2_get(in, "fb");
    const json_t *fb_provider = cch2_get(in, "fb_provider");
    const json_t *hint = cch2_get(in, "fb_base_url_hint");
    const char *fbp = cch2_str(fb_provider);

    const json_t *mode = cch2_get(fb, "api_mode");
    if (!cch2_truth(mode)) mode = cch2_get(fb, "transport");
    json_t *a = cch2_args();
    if (mode && mode->type != JSON_NULL) json_set(a, "value", json_copy(mode));
    json_t *explicit = cch2_call(io, "parse_api_mode", a);
    if (cch2_is_raise(explicit)) return explicit;
    if (cch2_truth(explicit)) {
        json_t *arr = json_array();
        json_append(arr, json_bool(1));
        json_append(arr, explicit);
        return arr;
    }
    json_free(explicit);

    if (cch2_truth(fb_provider) && strcmp(fbp, "custom") != 0 && strcmp(fbp, "moa") != 0) {
        json_t *a2 = cch2_args();
        json_set(a2, "provider", json_copy(fb_provider));
        json_t *named = cch2_call(io, "get_named_custom_provider", a2);
        if (cch2_is_raise(named)) return named;
        const json_t *declared = (named && named->type == JSON_OBJECT) ? cch2_get(named, "api_mode") : NULL;
        if (cch2_truth(declared)) {
            json_t *arr = json_array();
            json_append(arr, json_bool(1));
            json_append(arr, json_copy(declared));
            json_free(named);
            return arr;
        }
        json_free(named);
    }
    int anthropic = (fbp && strcmp(fbp, "anthropic") == 0);
    if (!anthropic && cch2_truth(hint)) {
        json_t *w = cch2_is_anthropic_wire_url_core(io, hint);
        if (cch2_is_raise(w)) return w;
        anthropic = cch2_truth(w);
        json_free(w);
    }
    json_t *arr = json_array();
    json_append(arr, json_bool(0));
    json_append(arr, json_string(anthropic ? "anthropic_messages" : "chat_completions"));
    return arr;
}

/* Projection: in.callback is the marker string for agent._interruptible_api_call. */
/* PoP: cch2_managed_summary_call @ agent/chat_completion_helpers.py:_managed_summary_call */
json_t *cch2_managed_summary_call(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *agent = cch2_get(in, "agent");
    const json_t *prov = cch2_get(agent, "provider");
    const json_t *model = cch2_get(agent, "model");
    const json_t *am = cch2_get(agent, "api_mode");
    char *name = cch2_truth(prov) ? cch2_pystr(prov) : cch2_xfmt("provider");
    char *model_name = cch2_pystr_or_empty(model);
    char *api_mode = cch2_truth(am) ? cch2_pystr(am) : cch2_xfmt("chat_completions");

    json_t *meta = json_object();
    json_set(meta, "api_mode", json_string(api_mode ? api_mode : ""));
    json_set(meta, "api_request_id", cch2_copy_or_null(cch2_get(in, "api_request_id")));
    json_set(meta, "call_role", json_string("iteration_summary"));
    json_set(meta, "retry_count", cch2_copy_or_null(cch2_get(in, "retry_count")));

    json_t *a = cch2_args();
    json_set(a, "request", cch2_copy_or_null(cch2_get(in, "request")));
    json_set(a, "callback", cch2_copy_or_null(cch2_get(in, "callback")));
    json_set(a, "name", json_string(name ? name : ""));
    json_set(a, "model_name", json_string(model_name ? model_name : ""));
    json_set(a, "metadata", meta);
    json_set(a, "defer_logical_completion", json_bool(1));
    free(name); free(model_name); free(api_mode);
    return cch2_call(io, "relay_llm.execute_current", a);
}

/* PoP: cch2_summary_text @ agent/chat_completion_helpers.py:_summary_text */
json_t *cch2_summary_text(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *response = cch2_get(in, "response");
    const json_t *nkw = cch2_get(in, "normalize_kwargs");
    json_t *a = cch2_args();
    json_set(a, "response", cch2_copy_or_null(response));
    json_t *shim = cch2_call(io, "is_router_timeout_shim", a);
    if (cch2_is_raise(shim)) return shim;
    if (cch2_truth(shim)) {
        json_free(shim);
        json_t *w = cch2_log(io, "logger.warning", CCH2_FMT_SHIM_WARN, json_array());
        if (cch2_is_raise(w)) return w;
        json_free(w);
        return json_string("");
    }
    json_free(shim);

    json_t *a2 = cch2_args();
    json_set(a2, "response", cch2_copy_or_null(response));
    if (nkw && nkw->type == JSON_OBJECT)
        for (size_t i = 0; i < nkw->c.count; i++)
            json_set(a2, nkw->c.keys[i], json_copy(nkw->c.items[i]));
    json_t *normalized = cch2_call(io, "transport.normalize_response", a2);
    if (cch2_is_raise(normalized)) return normalized;

    if (cch2_truth(cch2_get(normalized, "tool_calls"))) {
        json_t *w = cch2_log(io, "logger.warning", CCH2_FMT_TOOLS_WARN, json_array());
        if (cch2_is_raise(w)) { json_free(normalized); return w; }
        json_free(w);
    }
    char *content = cch2_pystr_or_empty(cch2_get(normalized, "content"));
    char *stripped = cch2_strip(content ? content : "");
    free(content);
    json_free(normalized);
    json_t *r = json_string(stripped ? stripped : "");
    free(stripped);
    return r;
}

/* PoP: cch2_with_stream_emitters @ agent/chat_completion_helpers.py:_with_stream_emitters */
json_t *cch2_with_stream_emitters(cch2_io_t *io, const json_t *in, json_t *st)
{
    const json_t *agent = cch2_get(in, "agent");
    int has_start = cch2_obj_truth(cch2_get(agent, "_emit_stream_start"));
    int has_end = cch2_obj_truth(cch2_get(agent, "_emit_stream_end"));
    if (has_start) {
        json_t *s = cch2_call(io, "emit_stream_start", cch2_args());
        if (cch2_is_raise(s)) return s;
        json_free(s);
    }
    json_t *r = cch2_call(io, "run", cch2_args());
    if (cch2_is_raise(r)) {
        if (cch2_is_base(cch2_kind(r))) return r;   /* except Exception does not catch */
        if (has_end) {
            json_t *a = cch2_args();
            json_set(a, "final_text", json_string(""));
            json_set(a, "finished", json_bool(0));
            const char *msg = cch2_str(cch2_get(r, "message"));
            json_set(a, "error", json_string(msg ? msg : ""));
            json_t *e = cch2_call(io, "emit_stream_end", a);
            if (cch2_is_raise(e)) { json_free(r); return e; }
            json_free(e);
        }
        return r;   /* re-raise */
    }
    if (has_end) {
        json_t *tmp = json_object();
        json_set(tmp, "response", json_copy(r));
        json_t *ft = cch2_stream_final_text(io, tmp, st);
        json_free(tmp);
        json_t *a = cch2_args();
        json_set(a, "final_text", ft ? ft : json_string(""));
        json_set(a, "finished", json_bool(1));
        json_t *e = cch2_call(io, "emit_stream_end", a);
        if (cch2_is_raise(e)) { json_free(r); return e; }
        json_free(e);
    }
    return r;
}

/* PoP: cch2_finalize_bedrock_relay_events @ agent/chat_completion_helpers.py:_finalize_bedrock_relay_events */
json_t *cch2_finalize_bedrock_relay_events(cch2_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *events = cch2_get(in, "events");
    json_t *kw = json_object();
    json_set(kw, "stream", cch2_copy_or_null(events));
    json_t *a = cch2_args();
    json_set(a, "kwargs", kw);
    json_t *r = cch2_call(io, "stream_converse_with_callbacks", a);
    if (cch2_is_raise(r)) {
        if (strcmp(cch2_kind(r), "EmptyStreamError") == 0) { json_free(r); return NULL; }
        return r;
    }
    return r;
}
