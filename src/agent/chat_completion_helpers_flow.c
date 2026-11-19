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
    if (jtruth(jget(in, "monitor_interrupted"))) {
        json_t *a = act("raise");
        json_set(a, "kind", json_string("InterruptedError"));
        json_set(a, "message", json_string("Agent interrupted during streaming API call"));
        push(log, a);
        return log;
    }
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