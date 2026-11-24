/*
 * api_server_openai_routes.c — C11 home for upstream Hermes
 * gateway/platforms/api_server_openai_routes.py.
 *
 * STATUS: behavior port of the module's PURE core, proven by three differential
 * gates (w27_apiservero_1a4ac2_p1/_p2/_p3).  The aiohttp handlers, the SSE
 * writers and every store/queue/agent seam stay in the caller; what lives here
 * is only what the wire values decide.
 *
 * PoP (proof-of-port) map — every exported symbol below is a 1:1 port of the
 * named upstream definition and is exercised value-for-value against the live
 * Python implementation by its gate's oracle:
 *
 *   gateway/platforms/api_server_openai_routes.py:_result_flags
 *     -> w27_apiservero_1a4ac2_p1_result_flags
 *   gateway/platforms/api_server_openai_routes.py:_budget_exhausted
 *     -> w27_apiservero_1a4ac2_p1_budget_exhausted
 *   gateway/platforms/api_server_openai_routes.py:_finish_reason
 *     -> w27_apiservero_1a4ac2_p1_finish_reason
 *   gateway/platforms/api_server_openai_routes.py:_response_status
 *     -> w27_apiservero_1a4ac2_p1_response_status
 *   gateway/platforms/api_server_openai_routes.py:_hermes_extras
 *     -> w27_apiservero_1a4ac2_p1_hermes_extras
 *   gateway/platforms/api_server_openai_routes.py:_settled
 *     -> w27_apiservero_1a4ac2_p1_settled
 *   gateway/platforms/api_server_openai_routes.py:_responses_fingerprint_keys
 *     -> w27_apiservero_1a4ac2_p1_fingerprint_keys
 *   gateway/platforms/api_server_openai_routes.py:_cap_text
 *     -> w27_apiservero_1a4ac2_p2_cap_text
 *   gateway/platforms/api_server_openai_routes.py:_cap_history_tool_outputs
 *     -> w27_apiservero_1a4ac2_p2_cap_history_tool_outputs
 *   gateway/platforms/api_server_openai_routes.py:_trim_tool_items
 *     -> w27_apiservero_1a4ac2_p2_trim_tool_items
 *   gateway/platforms/api_server_openai_routes.py:_message_item
 *     -> w27_apiservero_1a4ac2_p2_message_item
 *   gateway/platforms/api_server_openai_routes.py:_post_stream_transform
 *     -> w27_apiservero_1a4ac2_p2_post_stream_transform
 *   gateway/platforms/api_server_openai_routes.py:_is_reasoning_input_item
 *     -> w27_apiservero_1a4ac2_p2_is_reasoning_input_item
 *   gateway/platforms/api_server_openai_routes.py:_parse_chat_messages
 *     -> w27_apiservero_1a4ac2_p3_parse_chat_messages
 *   gateway/platforms/api_server_openai_routes.py:_parse_responses_input
 *     -> w27_apiservero_1a4ac2_p3_parse_responses_input
 *   gateway/platforms/api_server_openai_routes.py:_parse_conversation_history
 *     -> w27_apiservero_1a4ac2_p3_parse_conversation_history
 *   gateway/platforms/api_server_openai_routes.py:_ResponsesStream.envelope
 *     -> w27_apiservero_1a4ac2_p3_stream_envelope
 *   gateway/platforms/api_server_openai_routes.py:_ResponsesStream.terminal_envelope
 *     -> w27_apiservero_1a4ac2_p3_terminal_envelope
 *   gateway/platforms/api_server_openai_routes.py:_ResponsesStream._history_with_user
 *     -> w27_apiservero_1a4ac2_p3_history_with_user
 *   gateway/platforms/api_server_openai_routes.py:_ResponsesStream._final_items
 *     -> w27_apiservero_1a4ac2_p3_final_items
 *
 * SEAMS (deliberately not ported — no pure decision of their own to prove):
 *   the aiohttp handlers (_handle_chat_completions/_handle_responses/
 *   _handle_get_response/_handle_delete_response/_prepare_sse_response/
 *   _select_request_route), the SSE writer methods (write_event, the emit_
 *   family, dispatch/flush_batch/collect_result/close_message_item/
 *   persistence unit (persist_snapshot/_persist_snapshot/_commit_terminal/
 *   _store_nonstream_response/_matching_replay/_responses_durable_replay/
 *   _response_identity), the agent spawn + approval plumbing, _iter_stream_items,
 *   and the identity/redaction leaves: _reasoning_item, _extract_output_items and
 *   the two turn-boundary delegates (_turn_reasoning_text /
 *   _build_response_conversation_history) mint uuids or walk session_rules'
 *   compaction carriers, while _redact_api_error_text (agent.redact) and t()
 *   (agent.i18n) belong to their own modules — text reaching this file has
 *   already crossed that boundary.
 */

#include "hermes_json.h"
#include "noc_w27_apiservero_1a4ac2_p1.h"
#include "noc_w27_apiservero_1a4ac2_p2.h"
#include "noc_w27_apiservero_1a4ac2_p3.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* small allocation / string helpers                                    */
/* ------------------------------------------------------------------ */

static char *oar_dup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *oar_dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *oar_vfmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) { va_end(ap2); return oar_dup(""); }
    char *p = (char *)malloc((size_t)need + 1);
    if (p) vsnprintf(p, (size_t)need + 1, fmt, ap2);
    va_end(ap2);
    return p;
}

typedef struct { char *p; size_t n, cap; } oar_sb;

static void oar_sb_init(oar_sb *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static void oar_sb_reserve(oar_sb *b, size_t extra) {
    if (b->n + extra + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 64;
        while (c < b->n + extra + 1) c *= 2;
        char *np = (char *)realloc(b->p, c);
        if (!np) return;
        b->p = np;
        b->cap = c;
    }
}

static void oar_sb_putn(oar_sb *b, const char *s, size_t n) {
    if (!n) return;
    oar_sb_reserve(b, n);
    if (!b->p || b->cap < b->n + n + 1) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void oar_sb_puts(oar_sb *b, const char *s) { if (s) oar_sb_putn(b, s, strlen(s)); }
static void oar_sb_putc(oar_sb *b, char c) { oar_sb_putn(b, &c, 1); }

static char *oar_sb_take(oar_sb *b) {
    if (!b->p) return oar_dup("");
    char *p = b->p;
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
    return p;
}

/* ------------------------------------------------------------------ */
/* Python semantics: truthiness, str()/repr(), code-point indexing      */
/* ------------------------------------------------------------------ */

static int oar_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static int oar_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int oar_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int oar_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* dict.get(key) — NULL when absent (or when the receiver is not a dict). */
static json_t *oar_get(const json_t *o, const char *k) {
    return oar_is_obj(o) ? json_obj_get(o, k) : NULL;
}

/* bool(value) for a parsed JSON value. */
static int oar_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

/* Python str() of a number (integral values print without a decimal point). */
static char *oar_numstr(double d) {
    if (d == (double)(long long)d) return oar_vfmt("%lld", (long long)d);
    char buf[64];
    snprintf(buf, sizeof buf, "%.17g", d);
    return oar_dup(buf);
}

static size_t oar_cplen(const char *s) {
    size_t n = 0;
    for (size_t i = 0; s && s[i]; n++) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = (c < 0x80) ? 1 : ((c >> 5) == 6) ? 2 : ((c >> 4) == 14) ? 3 : ((c >> 3) == 30) ? 4 : 1;
        i += adv;
    }
    return n;
}

/* Byte offset of code-point index `idx` (clamped to the end). */
static size_t oar_cpoff(const char *s, size_t idx) {
    size_t i = 0, n = 0;
    while (s && s[i] && n < idx) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = (c < 0x80) ? 1 : ((c >> 5) == 6) ? 2 : ((c >> 4) == 14) ? 3 : ((c >> 3) == 30) ? 4 : 1;
        i += adv;
        n++;
    }
    return i;
}

/* Python s[:keep] over code points; a negative keep counts from the end. */
static char *oar_cp_prefix(const char *s, long keep) {
    if (!s) return oar_dup("");
    size_t len = oar_cplen(s);
    long end = keep;
    if (end < 0) { end = (long)len + end; if (end < 0) end = 0; }
    if (end > (long)len) end = (long)len;
    return oar_dupn(s, oar_cpoff(s, (size_t)end));
}

/* Python repr() for the value classes this port encounters. */
static char *oar_repr(const json_t *v) {
    if (!v) return oar_dup("None");
    switch (v->type) {
        case JSON_NULL: return oar_dup("None");
        case JSON_BOOL: return oar_dup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return oar_numstr(v->num_val);
        case JSON_STRING: {
            oar_sb b;
            oar_sb_init(&b);
            oar_sb_putc(&b, '\'');
            for (const char *p = v->str_val ? v->str_val : ""; *p; p++) {
                if (*p == '\'' || *p == '\\') { oar_sb_putc(&b, '\\'); oar_sb_putc(&b, *p); }
                else if (*p == '\n') oar_sb_puts(&b, "\\n");
                else if (*p == '\r') oar_sb_puts(&b, "\\r");
                else if (*p == '\t') oar_sb_puts(&b, "\\t");
                else oar_sb_putc(&b, *p);
            }
            oar_sb_putc(&b, '\'');
            return oar_sb_take(&b);
        }
        case JSON_ARRAY: {
            oar_sb b;
            oar_sb_init(&b);
            oar_sb_putc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) oar_sb_puts(&b, ", ");
                char *e = oar_repr(v->c.items[i]);
                oar_sb_puts(&b, e);
                free(e);
            }
            oar_sb_putc(&b, ']');
            return oar_sb_take(&b);
        }
        case JSON_OBJECT: {
            oar_sb b;
            oar_sb_init(&b);
            oar_sb_putc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) oar_sb_puts(&b, ", ");
                json_t key;
                memset(&key, 0, sizeof key);
                key.type = JSON_STRING;
                key.str_val = v->c.keys[i];
                char *k = oar_repr(&key);
                oar_sb_puts(&b, k);
                free(k);
                oar_sb_puts(&b, ": ");
                char *e = oar_repr(v->c.items[i]);
                oar_sb_puts(&b, e);
                free(e);
            }
            oar_sb_putc(&b, '}');
            return oar_sb_take(&b);
        }
        default: return oar_dup("None");
    }
}

/* Python str(value). */
static char *oar_pystr(const json_t *v) {
    if (oar_is_str(v)) return oar_dup(v->str_val ? v->str_val : "");
    return oar_repr(v);
}

/* str(value or ""). */
static char *oar_pystr_or_empty(const json_t *v) {
    if (!oar_truthy(v)) return oar_dup("");
    return oar_pystr(v);
}

static char *oar_strip(const char *s) {
    if (!s) return oar_dup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return oar_dupn(s + a, b - a);
}

static char *oar_lower(char *s) {
    for (char *p = s; p && *p; p++) *p = (char)tolower((unsigned char)*p);
    return s;
}

static int oar_startswith(const char *s, const char *pfx) {
    if (!s || !pfx) return 0;
    return strncmp(s, pfx, strlen(pfx)) == 0;
}

static int oar_contains(const char *s, const char *needle) {
    return s && needle && strstr(s, needle) != NULL;
}

static json_t *oar_jstr(const char *s) { return json_string(s ? s : ""); }
static json_t *oar_jbool(int b) { return json_bool(b ? true : false); }

/* ------------------------------------------------------------------ */
/* Python-compatible json.dumps(): ", " / ": " separators, ensure_ascii */
/* ------------------------------------------------------------------ */

static void oar_py_esc(oar_sb *b, const char *s) {
    oar_sb_putc(b, '"');
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        unsigned char c = *p;
        switch (c) {
            case '"':  oar_sb_puts(b, "\\\""); p++; continue;
            case '\\': oar_sb_puts(b, "\\\\"); p++; continue;
            case '\n': oar_sb_puts(b, "\\n"); p++; continue;
            case '\r': oar_sb_puts(b, "\\r"); p++; continue;
            case '\t': oar_sb_puts(b, "\\t"); p++; continue;
            case '\b': oar_sb_puts(b, "\\b"); p++; continue;
            case '\f': oar_sb_puts(b, "\\f"); p++; continue;
            default: break;
        }
        if (c < 0x20) {
            char esc[8];
            snprintf(esc, sizeof esc, "\\u%04x", c);
            oar_sb_puts(b, esc);
            p++;
            continue;
        }
        if (c < 0x80) { oar_sb_putc(b, (char)c); p++; continue; }
        unsigned int cp = 0;
        size_t adv = 1;
        if ((c >> 5) == 6 && p[1]) {
            cp = ((unsigned)(c & 0x1Fu) << 6) | (unsigned)(p[1] & 0x3Fu);
            adv = 2;
        } else if ((c >> 4) == 14 && p[1] && p[2]) {
            cp = ((unsigned)(c & 0x0Fu) << 12) | ((unsigned)(p[1] & 0x3Fu) << 6) |
                 (unsigned)(p[2] & 0x3Fu);
            adv = 3;
        } else if ((c >> 3) == 30 && p[1] && p[2] && p[3]) {
            cp = ((unsigned)(c & 0x07u) << 18) | ((unsigned)(p[1] & 0x3Fu) << 12) |
                 ((unsigned)(p[2] & 0x3Fu) << 6) | (unsigned)(p[3] & 0x3Fu);
            adv = 4;
        }
        char esc[20];
        if (cp <= 0xFFFFu) {
            snprintf(esc, sizeof esc, "\\u%04x", cp);
            oar_sb_puts(b, esc);
        } else {
            cp -= 0x10000u;
            snprintf(esc, sizeof esc, "\\u%04x\\u%04x", 0xD800u + (cp >> 10),
                     0xDC00u + (cp & 0x3FFu));
            oar_sb_puts(b, esc);
        }
        p += adv;
    }
    oar_sb_putc(b, '"');
}

static void oar_py_dump(const json_t *v, oar_sb *b) {
    if (!v) { oar_sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_NULL: oar_sb_puts(b, "null"); return;
        case JSON_BOOL: oar_sb_puts(b, v->bool_val ? "true" : "false"); return;
        case JSON_NUMBER: {
            char *n = oar_numstr(v->num_val);
            oar_sb_puts(b, n);
            free(n);
            return;
        }
        case JSON_STRING: oar_py_esc(b, v->str_val); return;
        case JSON_ARRAY:
            oar_sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) oar_sb_puts(b, ", ");
                oar_py_dump(v->c.items[i], b);
            }
            oar_sb_putc(b, ']');
            return;
        case JSON_OBJECT:
            oar_sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) oar_sb_puts(b, ", ");
                oar_py_esc(b, v->c.keys[i]);
                oar_sb_puts(b, ": ");
                oar_py_dump(v->c.items[i], b);
            }
            oar_sb_putc(b, '}');
            return;
        default: oar_sb_puts(b, "null"); return;
    }
}

static char *oar_py_dumps(const json_t *v) {
    oar_sb b;
    oar_sb_init(&b);
    oar_py_dump(v, &b);
    return oar_sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* gate p1 — turn verdicts and status algebra                           */
/* ------------------------------------------------------------------ */

json_t *w27_apiservero_1a4ac2_p1_result_flags(const json_t *result) {
    json_t *out = json_array();
    if (!oar_is_obj(result)) {
        json_append(out, oar_jbool(1));
        json_append(out, oar_jbool(0));
        json_append(out, oar_jbool(0));
        json_append(out, json_null());
        return out;
    }
    json_t *completed_v = oar_get(result, "completed");
    int completed = completed_v ? oar_truthy(completed_v) : 1;
    if (oar_truthy(oar_get(result, "interrupted"))) completed = 0;
    json_append(out, oar_jbool(completed));
    json_append(out, oar_jbool(oar_truthy(oar_get(result, "partial"))));
    json_append(out, oar_jbool(oar_truthy(oar_get(result, "failed"))));
    json_t *err = oar_get(result, "error");
    json_append(out, err ? json_copy(err) : json_null());
    return out;
}

json_t *w27_apiservero_1a4ac2_p1_budget_exhausted(const json_t *result) {
    char *reason = oar_is_obj(result)
                       ? oar_pystr_or_empty(oar_get(result, "turn_exit_reason"))
                       : oar_dup("");
    int hit = oar_startswith(reason, "budget_exhausted")
           || oar_startswith(reason, "max_iterations_reached(");
    free(reason);
    return oar_jbool(hit);
}

const char *w27_apiservero_1a4ac2_p1_finish_reason(int completed, int is_partial, int is_failed,
                                                   const char *err_msg, int has_agent_error,
                                                   int budget_exhausted) {
    if (is_partial && err_msg && *err_msg) {
        char *low = oar_lower(oar_dup(err_msg));
        int trunc = oar_contains(low, "truncat");
        free(low);
        if (trunc) return "length";
    }
    if (has_agent_error || is_failed || (!completed && err_msg && *err_msg)) return "error";
    if (budget_exhausted) return "length";
    return completed ? "stop" : "cancelled";
}

json_t *w27_apiservero_1a4ac2_p1_response_status(const json_t *result) {
    if (oar_truthy(oar_get(result, "interrupted"))) return oar_jstr("cancelled");
    if (oar_truthy(oar_get(result, "failed")) || oar_truthy(oar_get(result, "error")))
        return oar_jstr("failed");
    json_t *completed = oar_get(result, "completed");
    if (completed && completed->type == JSON_BOOL && !completed->bool_val)
        return oar_jstr("incomplete");
    return oar_jstr("completed");
}

json_t *w27_apiservero_1a4ac2_p1_hermes_extras(int completed, int is_partial, int is_failed,
                                               const json_t *err_msg, const char *finish_reason,
                                               int budget_exhausted) {
    int fr_length = finish_reason && strcmp(finish_reason, "length") == 0;
    const char *code = (budget_exhausted && fr_length) ? "iteration_budget_exhausted"
                       : fr_length ? "output_truncated"
                                   : "agent_error";
    json_t *out = json_object();
    json_set(out, "completed", oar_jbool(completed));
    json_set(out, "partial", oar_jbool(is_partial));
    json_set(out, "failed", oar_jbool(is_failed));
    json_set(out, "error", err_msg ? json_copy(err_msg) : json_null());
    json_set(out, "error_code", oar_jstr(code));
    return out;
}

json_t *w27_apiservero_1a4ac2_p1_settled(const json_t *record) {
    if (!oar_is_obj(record)) return oar_jbool(0);
    json_t *resp = oar_get(record, "response");
    return oar_jbool(!oar_is_null(resp));
}

static const char *const OAR_FINGERPRINT_KEYS[] = {
    "input", "instructions", "previous_response_id", "conversation", "conversation_history",
    "model", "provider", "model_options", "tools",
};

json_t *w27_apiservero_1a4ac2_p1_fingerprint_keys(const json_t *body) {
    json_t *out = json_array();
    for (size_t i = 0; i < sizeof OAR_FINGERPRINT_KEYS / sizeof OAR_FINGERPRINT_KEYS[0]; i++)
        json_append(out, oar_jstr(OAR_FINGERPRINT_KEYS[i]));
    json_t *trunc = oar_get(body, "truncation");
    if (!oar_is_null(trunc)) json_append(out, oar_jstr("truncation"));
    return out;
}

/* ------------------------------------------------------------------ */
/* gate p2 — text shaping and transcript hygiene                        */
/* ------------------------------------------------------------------ */

json_t *w27_apiservero_1a4ac2_p2_cap_text(const char *text, long keep) {
    const char *src = text ? text : "";
    char *head = oar_cp_prefix(src, keep);
    long more = (long)oar_cplen(src) - keep;
    char *out = oar_vfmt("%s...[%ld more chars]", head, more);
    free(head);
    json_t *r = oar_jstr(out);
    free(out);
    return r;
}

json_t *w27_apiservero_1a4ac2_p2_message_item(const json_t *text) {
    json_t *part = json_object();
    json_set(part, "type", oar_jstr("output_text"));
    json_set(part, "text", text ? json_copy(text) : json_null());
    json_t *content = json_array();
    json_append(content, part);
    json_t *out = json_object();
    json_set(out, "type", oar_jstr("message"));
    json_set(out, "role", oar_jstr("assistant"));
    json_set(out, "content", content);
    return out;
}

json_t *w27_apiservero_1a4ac2_p2_post_stream_transform(const json_t *result) {
    json_t *out = json_array();
    if (!oar_is_obj(result) || !oar_truthy(oar_get(result, "response_transformed"))) {
        json_append(out, oar_jstr(""));
        json_append(out, oar_jbool(0));
        return out;
    }
    char *final = oar_pystr_or_empty(oar_get(result, "final_response"));
    char *original = oar_pystr_or_empty(oar_get(result, "pre_transform_response"));
    if (*original && oar_startswith(final, original)) {
        size_t off = oar_cpoff(final, oar_cplen(original));
        json_append(out, oar_jstr(final + off));
        json_append(out, oar_jbool(1));
    } else {
        json_append(out, oar_jstr(final));
        json_append(out, oar_jbool(0));
    }
    free(final);
    free(original);
    return out;
}

json_t *w27_apiservero_1a4ac2_p2_is_reasoning_input_item(const json_t *item) {
    json_t *type = oar_get(item, "type");
    return oar_jbool(oar_is_str(type) && strcmp(type->str_val, "reasoning") == 0);
}

static const char *const OAR_TRIM_ARG_KEYS[] = {
    "content", "query", "pattern", "old_string", "new_string"
};

/* _trim_tool_items: function_call branch (mutates `item` in place). */
static void oar_trim_function_call(json_t *item) {
    json_t *raw = oar_get(item, "arguments");
    json_t *args = NULL;
    int owned = 0;
    if (oar_is_str(raw)) {
        args = json_parse(raw->str_val, NULL);
        if (!args) return; /* json.loads raised -> the live code swallows it */
        owned = 1;
    } else if (raw) {
        args = raw;
    }
    if (!oar_is_obj(args)) {
        if (owned) json_free(args);
        return;
    }
    for (size_t i = 0; i < sizeof OAR_TRIM_ARG_KEYS / sizeof OAR_TRIM_ARG_KEYS[0]; i++) {
        json_t *v = oar_get(args, OAR_TRIM_ARG_KEYS[i]);
        if (!oar_is_str(v)) continue;
        size_t len = oar_cplen(v->str_val ? v->str_val : "");
        if (len <= 500) continue;
        /* the live marker embeds U+2014 (em dash); dumps() re-encodes it as the
         * backslash-u escape inside the stored JSON text. */
        char *marker = oar_vfmt("[%zu chars \xE2\x80\x94 truncated for response.completed]", len);
        json_set(args, OAR_TRIM_ARG_KEYS[i], oar_jstr(marker));
        free(marker);
    }
    char *text = oar_py_dumps(args);
    json_set(item, "arguments", oar_jstr(text));
    free(text);
    if (owned) json_free(args);
}

/* _trim_tool_items: function_call_output branch (mutates `item` in place). */
static void oar_trim_function_call_output(json_t *item) {
    json_t *output = oar_get(item, "output");
    if (!oar_is_arr(output) || json_len(output) == 0) return;
    json_t *first = json_get(output, 0);
    if (!oar_is_obj(first)) return;
    json_t *ftype = oar_get(first, "type");
    if (!oar_is_str(ftype) || strcmp(ftype->str_val, "input_text") != 0) return;
    json_t *textv = oar_get(first, "text");
    const char *text = oar_is_str(textv) ? textv->str_val : "";
    if (oar_cplen(text) <= 1000) return;
    json_set(first, "text", w27_apiservero_1a4ac2_p2_cap_text(text, 500));
    json_t *wrapped = json_array();
    json_append(wrapped, json_copy(first));
    json_set(item, "output", wrapped);
}

json_t *w27_apiservero_1a4ac2_p2_trim_tool_items(const json_t *items) {
    json_t *out = json_array();
    if (!oar_is_arr(items)) return out;
    for (size_t i = 0; i < json_len(items); i++) {
        json_t *item = json_copy(json_get(items, i));
        if (!item) continue;
        json_t *type = oar_get(item, "type");
        if (oar_is_str(type) && strcmp(type->str_val, "function_call") == 0) {
            oar_trim_function_call(item);
        } else if (oar_is_str(type) && strcmp(type->str_val, "function_call_output") == 0) {
            oar_trim_function_call_output(item);
        }
        json_append(out, item);
    }
    return out;
}

/* One assistant turn's tool_calls blob, capped when it carries long arguments. */
static json_t *oar_cap_tool_calls(const json_t *tool_calls, long max_chars) {
    json_t *capped_calls = json_array();
    for (size_t c = 0; c < json_len(tool_calls); c++) {
        json_t *call = json_get(tool_calls, c);
        json_t *fn = oar_get(call, "function");
        json_t *raw = oar_get(fn, "arguments");
        if (oar_is_str(raw) && oar_cplen(raw->str_val ? raw->str_val : "") > (size_t)max_chars) {
            json_t *args = json_parse(raw->str_val, NULL);
            if (args && oar_is_obj(args)) {
                for (size_t k = 0; k < json_object_size(args); k++) {
                    const char *key = json_object_get_key_at(args, k);
                    json_t *v = json_object_get_at(args, k);
                    if (!key || !oar_is_str(v)) continue;
                    if (oar_cplen(v->str_val ? v->str_val : "") > (size_t)max_chars)
                        json_set(args, key, w27_apiservero_1a4ac2_p2_cap_text(v->str_val, max_chars));
                }
                char *text = oar_py_dumps(args);
                json_t *newfn = json_copy(fn);
                json_set(newfn, "arguments", oar_jstr(text));
                free(text);
                json_t *newcall = json_copy(call);
                json_set(newcall, "function", newfn);
                json_append(capped_calls, newcall);
                json_free(args);
                continue;
            }
            if (args) json_free(args);
        }
        json_append(capped_calls, json_copy(call));
    }
    return capped_calls;
}

json_t *w27_apiservero_1a4ac2_p2_cap_history_tool_outputs(const json_t *history, long max_chars) {
    if (!oar_is_arr(history)) return json_array();
    if (max_chars <= 0) return json_copy(history);
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(history); i++) {
        json_t *orig = json_get(history, i);
        if (!oar_is_obj(orig)) {
            json_append(out, json_copy(orig));
            continue;
        }
        json_t *msg = json_copy(orig);
        json_t *role = oar_get(msg, "role");
        int is_tool = oar_is_str(role) && strcmp(role->str_val, "tool") == 0;
        int is_assistant = oar_is_str(role) && strcmp(role->str_val, "assistant") == 0;
        json_t *content = oar_get(msg, "content");
        if (is_tool && oar_is_str(content) &&
            oar_cplen(content->str_val ? content->str_val : "") > (size_t)max_chars) {
            json_set(msg, "content", w27_apiservero_1a4ac2_p2_cap_text(content->str_val, max_chars));
        }
        json_t *tool_calls = oar_get(msg, "tool_calls");
        if (is_assistant && oar_is_arr(tool_calls)) {
            json_set(msg, "tool_calls", oar_cap_tool_calls(tool_calls, max_chars));
        }
        json_append(out, msg);
    }
    return out;
}

/* ------------------------------------------------------------------ */
/* gate p3 — wire parsing (OpenAI content normalisation)                */
/* ------------------------------------------------------------------ */

#define OAR_MAX_NORMALIZED_TEXT 65536
#define OAR_MAX_CONTENT_LIST 1000
#define OAR_MAX_DEPTH 10

typedef struct { char *raw; } oar_verr; /* "<code>:<message>" as the ValueError carries it */

static int oar_text_type(const char *t) {
    return t && (!strcmp(t, "text") || !strcmp(t, "input_text") || !strcmp(t, "output_text"));
}

static int oar_image_type(const char *t) {
    return t && (!strcmp(t, "image_url") || !strcmp(t, "input_image"));
}

static int oar_file_type(const char *t) {
    return t && (!strcmp(t, "file") || !strcmp(t, "input_file"));
}

/* api_server._cap_text: hard 64 KB code-point cap, no marker. */
static char *oar_cap_ws(const char *s) {
    size_t n = oar_cplen(s ? s : "");
    if (n <= OAR_MAX_NORMALIZED_TEXT) return oar_dup(s ? s : "");
    return oar_cp_prefix(s, OAR_MAX_NORMALIZED_TEXT);
}

/* str(value or "").strip().lower() — the part-type spelling normaliser. */
static char *oar_part_type(const json_t *raw) {
    char *s = oar_pystr_or_empty(raw);
    char *stripped = oar_strip(s);
    free(s);
    return oar_lower(stripped);
}

static json_t *oar_raise(oar_verr *err, const char *fmt, ...) {
    if (err && !err->raw) {
        va_list ap, ap2;
        va_start(ap, fmt);
        va_copy(ap2, ap);
        int need = vsnprintf(NULL, 0, fmt, ap);
        va_end(ap);
        if (need >= 0) {
            char *p = (char *)malloc((size_t)need + 1);
            if (p) vsnprintf(p, (size_t)need + 1, fmt, ap2);
            err->raw = p;
        }
        va_end(ap2);
    }
    return NULL;
}

/* _normalize_chat_content: flatten a chat content value into one plain string. */
static char *oar_normalize_chat_content(const json_t *content, int depth) {
    if (depth > OAR_MAX_DEPTH || oar_is_null(content)) return oar_dup("");
    if (oar_is_str(content)) return oar_cap_ws(content->str_val);
    if (oar_is_arr(content)) {
        oar_sb joined;
        oar_sb_init(&joined);
        size_t total = 0, n = json_len(content);
        if (n > OAR_MAX_CONTENT_LIST) n = OAR_MAX_CONTENT_LIST;
        int first = 1;
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(content, i);
            char *part = NULL;
            if (oar_is_str(item)) {
                part = oar_dup(item->str_val);
            } else if (oar_is_obj(item)) {
                char *pt = oar_part_type(oar_get(item, "type"));
                if (oar_text_type(pt)) {
                    json_t *text = oar_get(item, "text");
                    if (oar_truthy(text)) part = oar_pystr(text);
                }
                free(pt);
            } else if (oar_is_arr(item)) {
                part = oar_normalize_chat_content(item, depth + 1);
            }
            if (part && *part) {
                char *capped = oar_cap_ws(part);
                free(part);
                if (!first) oar_sb_puts(&joined, "\n");
                oar_sb_puts(&joined, capped);
                total += oar_cplen(capped);
                free(capped);
                first = 0;
                if (total >= OAR_MAX_NORMALIZED_TEXT) break;
            } else {
                free(part);
            }
        }
        char *s = oar_sb_take(&joined);
        char *out = oar_cap_ws(s);
        free(s);
        return out;
    }
    char *s = oar_pystr(content);
    char *out = oar_cap_ws(s);
    free(s);
    return out;
}

/* _normalize_image_part -> canonical {"type":"image_url","image_url":{...}}, or NULL + err. */
static json_t *oar_normalize_image_part(const json_t *part, oar_verr *err) {
    json_t *detail_v = oar_get(part, "detail");
    json_t *image_ref = oar_get(part, "image_url");
    json_t *detail_eff = detail_v;
    json_t *url_v;
    if (oar_is_obj(image_ref)) {
        url_v = oar_get(image_ref, "url");
        json_t *inner_detail = oar_get(image_ref, "detail");
        if (inner_detail) detail_eff = inner_detail;
    } else {
        url_v = image_ref;
    }
    char *url_check = oar_strip(oar_is_str(url_v) ? url_v->str_val : "");
    int url_ok = url_check[0] != '\0';
    free(url_check);
    if (!url_ok)
        return oar_raise(err, "invalid_image_url:Image parts must include a non-empty image URL.");

    char *url = oar_strip(url_v->str_val);
    char *lowered = oar_lower(oar_dup(url));
    int bad_url = 0;
    if (oar_startswith(lowered, "data:")) {
        if (!oar_startswith(lowered, "data:image/") || !oar_contains(url, ",")) {
            free(url);
            free(lowered);
            return oar_raise(err, "unsupported_content_type:Only image data URLs are supported. "
                                  "Non-image data payloads are not supported.");
        }
    } else if (!oar_startswith(lowered, "http://") && !oar_startswith(lowered, "https://")) {
        bad_url = 1;
    }
    free(lowered);
    if (bad_url) {
        free(url);
        return oar_raise(err, "invalid_image_url:Image inputs must use http(s) URLs or data:image/... URLs.");
    }

    json_t *inner = json_object();
    json_set(inner, "url", oar_jstr(url));
    free(url);
    if (!oar_is_null(detail_eff)) {
        char *d = oar_strip(oar_is_str(detail_eff) ? detail_eff->str_val : "");
        if (!d[0]) {
            free(d);
            json_free(inner);
            return oar_raise(err, "invalid_content_part:Image detail must be a non-empty string when provided.");
        }
        json_set(inner, "detail", oar_jstr(d));
        free(d);
    }
    json_t *out = json_object();
    json_set(out, "type", oar_jstr("image_url"));
    json_set(out, "image_url", inner);
    return out;
}

/* _normalize_multimodal_content -> canonical string or part list, NULL + err on ValueError. */
static json_t *oar_normalize_multimodal(const json_t *content, oar_verr *err) {
    if (oar_is_null(content)) return oar_jstr("");
    if (oar_is_str(content)) return oar_jstr(oar_cap_ws(content->str_val));
    if (!oar_is_arr(content)) return oar_jstr(oar_normalize_chat_content(content, 0));
    json_t *parts = json_array();
    size_t n = json_len(content);
    if (n > OAR_MAX_CONTENT_LIST) n = OAR_MAX_CONTENT_LIST;
    for (size_t i = 0; i < n; i++) {
        json_t *part = json_get(content, i);
        if (oar_is_str(part)) {
            if (part->str_val && part->str_val[0]) {
                json_t *p = json_object();
                json_set(p, "type", oar_jstr("text"));
                char *capped = oar_cap_ws(part->str_val);
                json_set(p, "text", oar_jstr(capped));
                free(capped);
                json_append(parts, p);
            }
            continue;
        }
        if (!oar_is_obj(part)) continue;
        char *pt = oar_part_type(oar_get(part, "type"));
        if (oar_text_type(pt)) {
            json_t *text = oar_get(part, "text");
            if (!oar_is_null(text)) {
                char *s = oar_pystr(text);
                if (*s) {
                    json_t *p = json_object();
                    json_set(p, "type", oar_jstr("text"));
                    char *capped = oar_cap_ws(s);
                    json_set(p, "text", oar_jstr(capped));
                    free(capped);
                    json_append(parts, p);
                }
                free(s);
            }
        } else if (oar_image_type(pt)) {
            json_t *img = oar_normalize_image_part(part, err);
            if (!img) { free(pt); json_free(parts); return NULL; }
            json_append(parts, img);
        } else if (oar_file_type(pt)) {
            free(pt);
            json_free(parts);
            oar_raise(err, "unsupported_content_type:Inline image inputs are supported, "
                           "but uploaded files and document inputs are not supported on this endpoint.");
            return NULL;
        } else {
            char *repr = oar_repr(oar_get(part, "type"));
            free(pt);
            json_free(parts);
            oar_raise(err, "unsupported_content_type:Unsupported content part type %s. "
                           "Only text and image_url/input_image parts are supported.", repr);
            free(repr);
            return NULL;
        }
        free(pt);
    }
    if (json_len(parts) == 0) { json_free(parts); return oar_jstr(""); }
    int all_text = 1;
    for (size_t i = 0; i < json_len(parts); i++) {
        json_t *t = oar_get(json_get(parts, i), "type");
        if (!oar_is_str(t) || strcmp(t->str_val, "text") != 0) { all_text = 0; break; }
    }
    if (all_text) {
        oar_sb b;
        oar_sb_init(&b);
        int first = 1;
        for (size_t i = 0; i < json_len(parts); i++) {
            json_t *t = oar_get(json_get(parts, i), "text");
            if (!oar_is_str(t) || !oar_truthy(t)) continue;
            if (!first) oar_sb_puts(&b, "\n");
            oar_sb_puts(&b, t->str_val);
            first = 0;
        }
        json_free(parts);
        char *s = oar_sb_take(&b);
        json_t *out = oar_jstr(s);
        free(s);
        return out;
    }
    return parts;
}

/* _multimodal_validation_error(exc, param) -> {"status":400,"body":{...}} */
static json_t *oar_validation_error_json(const oar_verr *err, const char *param) {
    const char *raw = (err && err->raw) ? err->raw : "";
    const char *colon = strchr(raw, ':');
    char *code;
    const char *message;
    if (colon && colon[1]) {
        code = oar_dupn(raw, (size_t)(colon - raw));
        message = colon + 1;
    } else {
        code = oar_dup("invalid_content_part");
        message = raw;
    }
    json_t *e = json_object();
    json_set(e, "message", oar_jstr(message));
    json_set(e, "type", oar_jstr("invalid_request_error"));
    json_set(e, "param", oar_jstr(param));
    json_set(e, "code", oar_jstr(code));
    free(code);
    json_t *body = json_object();
    json_set(body, "error", e);
    json_t *out = json_object();
    json_set(out, "status", json_number(400));
    json_set(out, "body", body);
    return out;
}

/* _error_response(message, 400) -> {"status":400,"body":{...}} (param/code None). */
static json_t *oar_error_json(const char *message) {
    json_t *e = json_object();
    json_set(e, "message", oar_jstr(message));
    json_set(e, "type", oar_jstr("invalid_request_error"));
    json_set(e, "param", json_null());
    json_set(e, "code", json_null());
    json_t *body = json_object();
    json_set(body, "error", e);
    json_t *out = json_object();
    json_set(out, "status", json_number(400));
    json_set(out, "body", body);
    return out;
}

json_t *w27_apiservero_1a4ac2_p3_parse_chat_messages(const json_t *messages) {
    json_t *out = json_object();
    json_t *sys = NULL;
    json_t *conv = json_array();
    size_t n = oar_is_arr(messages) ? json_len(messages) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *msg = json_get(messages, i);
        json_t *role = oar_get(msg, "role");
        const char *role_s = oar_is_str(role) ? role->str_val : "";
        json_t *raw = oar_get(msg, "content");
        if (strcmp(role_s, "system") == 0) {
            char *content = oar_normalize_chat_content(raw, 0);
            if (!sys) {
                sys = oar_jstr(content);
            } else {
                size_t need = strlen(sys->str_val) + 1 + strlen(content) + 1;
                char *joined = (char *)malloc(need);
                if (joined) {
                    snprintf(joined, need, "%s\n%s", sys->str_val, content);
                    json_free(sys);
                    sys = oar_jstr(joined);
                    free(joined);
                }
            }
            free(content);
        } else if (strcmp(role_s, "user") == 0 || strcmp(role_s, "assistant") == 0) {
            oar_verr err = { NULL };
            json_t *content = oar_normalize_multimodal(raw, &err);
            if (!content) {
                char *param = oar_vfmt("messages[%zu].content", i);
                json_set(out, "system_prompt", json_null());
                json_set(out, "messages", json_null());
                json_set(out, "error", oar_validation_error_json(&err, param));
                free(param);
                free(err.raw);
                json_free(conv);
                if (sys) json_free(sys);
                return out;
            }
            json_t *m = json_object();
            json_set(m, "role", oar_jstr(role_s));
            json_set(m, "content", content);
            json_append(conv, m);
        }
    }
    json_set(out, "system_prompt", sys ? sys : json_null());
    json_set(out, "messages", conv);
    json_set(out, "error", json_null());
    return out;
}

json_t *w27_apiservero_1a4ac2_p3_parse_responses_input(const json_t *raw_input) {
    json_t *out = json_object();
    json_t *msgs = json_array();
    if (oar_is_str(raw_input)) {
        json_t *m = json_object();
        json_set(m, "role", oar_jstr("user"));
        json_set(m, "content", oar_jstr(raw_input->str_val));
        json_append(msgs, m);
        json_set(out, "messages", msgs);
        json_set(out, "error", json_null());
        return out;
    }
    if (!oar_is_arr(raw_input)) {
        json_free(msgs);
        json_set(out, "messages", json_null());
        json_set(out, "error", oar_error_json("'input' must be a string or array"));
        return out;
    }
    for (size_t i = 0; i < json_len(raw_input); i++) {
        json_t *item = json_get(raw_input, i);
        if (oar_is_str(item)) {
            json_t *m = json_object();
            json_set(m, "role", oar_jstr("user"));
            json_set(m, "content", oar_jstr(item->str_val));
            json_append(msgs, m);
            continue;
        }
        json_t *item_type = oar_get(item, "type");
        if (oar_is_str(item_type) && strcmp(item_type->str_val, "reasoning") == 0) continue;
        if (oar_is_obj(item)) {
            oar_verr err = { NULL };
            json_t *content = oar_normalize_multimodal(oar_get(item, "content"), &err);
            if (!content) {
                char *param = oar_vfmt("input[%zu].content", i);
                json_set(out, "messages", json_null());
                json_set(out, "error", oar_validation_error_json(&err, param));
                free(param);
                free(err.raw);
                json_free(msgs);
                return out;
            }
            json_t *role = oar_get(item, "role");
            json_t *m = json_object();
            json_set(m, "role", oar_is_null(role) ? oar_jstr("user") : json_copy(role));
            json_set(m, "content", content);
            json_append(msgs, m);
        }
    }
    json_set(out, "messages", msgs);
    json_set(out, "error", json_null());
    return out;
}

json_t *w27_apiservero_1a4ac2_p3_parse_conversation_history(const json_t *raw_history) {
    json_t *out = json_object();
    json_t *hist = json_array();
    if (!oar_is_arr(raw_history)) {
        json_free(hist);
        json_set(out, "history", json_null());
        json_set(out, "error",
                 oar_error_json("'conversation_history' must be an array of message objects"));
        return out;
    }
    for (size_t i = 0; i < json_len(raw_history); i++) {
        json_t *entry = json_get(raw_history, i);
        json_t *type = oar_get(entry, "type");
        if (oar_is_str(type) && strcmp(type->str_val, "reasoning") == 0) continue;
        if (!oar_is_obj(entry) || !oar_get(entry, "role") || !oar_get(entry, "content")) {
            char *msg = oar_vfmt("conversation_history[%zu] must have 'role' and 'content' fields", i);
            json_set(out, "history", json_null());
            json_set(out, "error", oar_error_json(msg));
            free(msg);
            json_free(hist);
            return out;
        }
        oar_verr err = { NULL };
        json_t *content = oar_normalize_multimodal(oar_get(entry, "content"), &err);
        if (!content) {
            char *param = oar_vfmt("conversation_history[%zu].content", i);
            json_set(out, "history", json_null());
            json_set(out, "error", oar_validation_error_json(&err, param));
            free(param);
            free(err.raw);
            json_free(hist);
            return out;
        }
        char *role = oar_pystr(oar_get(entry, "role"));
        json_t *m = json_object();
        json_set(m, "role", oar_jstr(role));
        json_set(m, "content", content);
        free(role);
        json_append(hist, m);
    }
    json_set(out, "history", hist);
    json_set(out, "error", json_null());
    return out;
}

/* --- _ResponsesStream value surfaces ------------------------------ */

json_t *w27_apiservero_1a4ac2_p3_stream_envelope(const char *response_id, double created_at,
                                                 const char *model, const char *status) {
    json_t *env = json_object();
    json_set(env, "id", oar_jstr(response_id));
    json_set(env, "object", oar_jstr("response"));
    json_set(env, "status", oar_jstr(status));
    json_set(env, "created_at", json_number(created_at));
    json_set(env, "model", oar_jstr(model));
    return env;
}

json_t *w27_apiservero_1a4ac2_p3_terminal_envelope(const char *response_id, double created_at,
                                                   const char *model, const char *status,
                                                   const json_t *output, const json_t *error,
                                                   const json_t *usage, int durable) {
    json_t *env = w27_apiservero_1a4ac2_p3_stream_envelope(response_id, created_at, model, status);
    json_set(env, "output", output ? json_copy(output) : json_array());
    if (!oar_is_null(error)) {
        json_t *e = json_object();
        if (durable) {
            json_set(e, "code", oar_jstr("agent_error"));
            json_set(e, "message", json_copy(error));
        } else {
            json_set(e, "message", json_copy(error));
            json_set(e, "type", oar_jstr("server_error"));
        }
        json_set(env, "error", e);
    }
    json_t *u = json_object();
    static const char *const keys[] = { "input_tokens", "output_tokens", "total_tokens" };
    for (size_t i = 0; i < 3; i++) {
        json_t *v = oar_get(usage, keys[i]);
        json_set(u, keys[i], v ? json_copy(v) : json_number(0));
    }
    json_set(env, "usage", u);
    return env;
}

json_t *w27_apiservero_1a4ac2_p3_history_with_user(const json_t *history, const json_t *user_message) {
    json_t *out = oar_is_arr(history) ? json_copy(history) : json_array();
    json_t *m = json_object();
    json_set(m, "role", oar_jstr("user"));
    json_set(m, "content", user_message ? json_copy(user_message) : json_null());
    json_append(out, m);
    return out;
}

json_t *w27_apiservero_1a4ac2_p3_final_items(const json_t *emitted_items, const json_t *final_response_text,
                                             const json_t *agent_error) {
    json_t *items = w27_apiservero_1a4ac2_p2_trim_tool_items(emitted_items);
    const char *text = "";
    char *tmp = NULL;
    if (oar_is_str(final_response_text) && oar_truthy(final_response_text)) {
        text = final_response_text->str_val;
    } else if (oar_truthy(agent_error)) {
        /* caller boundary: agent_error has already been redacted upstream */
        tmp = oar_pystr(agent_error);
        text = tmp;
    }
    json_append(items, w27_apiservero_1a4ac2_p2_message_item(oar_jstr(text)));
    free(tmp);
    return items;
}
