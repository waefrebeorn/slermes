/*
 * signal_adapter_flow.c — Signal adapter FLOW cores and leaves of
 * plugins/platforms/signal/adapter.py (hermes-agent).
 *
 * Ports the DECISION SEQUENCES of the inbound SSE-line dispatch / sync-message
 * unwrap / group-mention gating / attachment-collection cluster, the outbound
 * recipient-routing / send-RPC / image-resolution / rate-limited attachment-batch
 * chain, and the sent-timestamp cache + send-validation + quote-author + mime
 * leaves. Call order, short-circuit branches, early returns, OrderedDict
 * mutation history (recent-sent TTL eviction + sent-message FIFO cap) and
 * exception propagation mirror the live Python.
 *
 * 18 ported functions: the 12 UNCLAIMED flow methods (_close_client,
 * _handle_sse_line, _unwrap_sync_message, _apply_group_mention_rules,
 * _collect_attachments, _group_allowed, _with_target, _rpc_send,
 * _resolve_image_path, _send_attachment_batch, _send_file, _send_reaction_rpc)
 * plus 6 leaf helpers that were only stub-claimed elsewhere and are upgraded to
 * real behaviour here (_validate_send_result, _track_sent_timestamp,
 * _remember_sent_message_timestamp, _consume_sent_timestamp,
 * _extract_quote_author, _ext_to_mime). This mirrors the anthropic_convert_flow2
 * precedent (a flow port re-claiming stub-only leaves with real bodies).
 *
 * Universal seam: every SEAMED sibling / foreign call enters through ONE
 * callback sig_call(ctx, name, args). The harness wires ctx->io to the case's
 * scripted results; the oracle installs recording stubs emitting the identical
 * event shape {"name","args","ret"} from the SAME scripts. args is ADOPTED into
 * the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":str(exc),...} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 * BaseException kinds (SystemExit / KeyboardInterrupt / CancelledError /
 * GeneratorExit) pass through every catch site: the only catches here are
 * _handle_sse_line's except json.JSONDecodeError / except Exception,
 * _collect_attachments's except Exception, _resolve_image_path's except
 * Exception and _send_attachment_batch's except SignalRateLimitError — all
 * narrower than BaseException, so scripted BaseException raises always
 * propagate.
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL (run inline, direct C call): every ported function below. A REAL
 *    function calls its REAL children directly (the _c cores) and its SEAMED
 *    children via sig_call.
 *  SEAMED (sig_call, scripted): the claimed sibling methods _rpc,
 *    _handle_envelope, _fetch_attachment, _resolve_recipient; the foreign
 *    collaborators client.aclose, time.time, time.monotonic, json.loads,
 *    cache_image_from_url, Path.exists, Path.stat, _signal_send_timeout,
 *    scheduler.acquire / scheduler.feedback / scheduler.report_rpc_duration,
 *    asyncio.sleep; and logger.debug / warning / error / info / exception.
 *    _ext_to_mime inlines the media_cache.mime_for_ext DEFAULT_EXT_TO_MIME
 *    table (no overrides rung) so it needs no seam.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and sig_-prefixed.
 *
 * Representation contract (shared with tests/oracle_signal_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}; a SignalRateLimitError
 *   raise additionally carries "retry_after" (read by _send_attachment_batch).
 * - The adapter self double is ctx.state (a JSON object of live attributes);
 *   methods read/write it in place. A DIRECT self._attr read with the key
 *   ABSENT raises AttributeError "'SignalAdapter' object has no attribute 'X'"
 *   (the oracle builds a real type("SignalAdapter", (), clsdict) double).
 * - _recent_sent_timestamps is an OrderedDict[int, float]: the oracle
 *   materializes int keys, the C port uses str(int) keys; both project to the
 *   same string-keyed JSON object under canon. _sent_message_timestamps is an
 *   OrderedDict[str, None] (string keys, null values). popitem(last=False)
 *   removes the FIRST key; pop+re-set moves a key to the end.
 * - group_allow_from / dm_allow_from are sets in live but _group_allowed only
 *   uses membership + truthiness, so the oracle keeps them as lists (fixture
 *   order) and the C port scans the JSON array — ret/trace/state all agree.
 * - tuple returns project to arrays; SendResult projects via dataclasses.asdict
 *   to ALL 8 fields {success, message_id, error, raw_response, retryable,
 *   retry_after, continuation_message_ids ([] for the () default), error_kind}.
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral floats).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - ASCII scope: text / chat ids / names / paths are ASCII; the only whitespace
 *   used is the ASCII set {09,0a,0b,0c,0d,1c,1d,1e,1f,20}. str.strip() uses that
 *   set on py3.14. str()-computed scalars carry str/int/bool/None only.
 * - json.loads is SEAMED (scripted parsed value or JSONDecodeError raise), so
 *   the libjson-lenient / json.loads-strict divergence is out of domain.
 * - unquote (file:// branch) is transcribed for the ASCII %XX (<0x80) domain;
 *   '+' is NOT converted to space (unquote, not unquote_plus).
 * - Path.exists / Path.stat are SEAMED (no real filesystem); Path.stat returns
 *   {"st_size": n}. cache_image_from_url returns a path string or raises.
 * - _recent_sent_timestamps values are numbers (floats); keys are integer
 *   timestamps. ts args to _consume_sent_timestamp / _track_sent_timestamp are
 *   ints. _sent_message_timestamps keys are str(timestamp).
 * - scheduler is a seamed double (acquire/feedback/report_rpc_duration); the
 *   fixture passes a marker object. time.monotonic is a scripted sequence.
 * - SignalRateLimitError raises carry retry_after (number or null).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct sig_ctx {
    json_t *state;      /* owned self double; mutated in place; emitted as out.state.self */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sig_ctx_t;

/* ── basic helpers ── */
static bool sig_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *sig_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool sig_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sig_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool sig_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *sig_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool sig_str_eq(const json_t *v, const char *s) {
    const char *t = sig_str(v);
    return t && s && strcmp(t, s) == 0;
}
/* Python scalar == (str/num/bool/null); different types are unequal. */
static bool sig_scalar_eq(const json_t *a, const json_t *b) {
    if (sig_is_none(a) && sig_is_none(b)) return true;
    if (sig_is_none(a) || sig_is_none(b)) return false;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    return false;
}
static json_t *sig_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *sig_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sig_err(type, msg);
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys[i];
            if (strcmp(k, "type") == 0 || strcmp(k, "message") == 0) continue;
            json_set(e, k, json_copy(spec->c.items[i]));
        }
    }
    return e;
}
static json_t *sig_attr_err(const char *key) {
    char msg[160];
    snprintf(msg, sizeof msg, "'SignalAdapter' object has no attribute '%s'", key);
    return sig_err("AttributeError", msg);
}
/* The one universal seam. args ADOPTED; return fresh owned node. */
static json_t *sig_call(sig_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sig_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sig_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sig_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}
static json_t *sig_args(void) { return json_object(); }
static void sig_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void sig_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void sig_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void sig_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void sig_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *sig_self(const sig_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}

/* ── string helpers ── */
static char *sig_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* str.strip() ASCII whitespace set (py3.14 includes 1c-1f). */
static bool sig_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
static char *sig_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sig_is_ws((unsigned char)s[a])) a++;
    while (b > a && sig_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
static char *sig_lower(const char *s) {
    char *o = sig_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* Python str() projection for scalar nodes (containers out of domain). */
static char *sig_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sig_strdup("None");
    if (v->type == JSON_BOOL) return sig_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return sig_strdup(buf);
    }
    if (v->type == JSON_STRING) return sig_strdup(v->str_val ? v->str_val : "");
    return sig_strdup("");
}
/* replace all literal occurrences of needle in hay with repl (single pass). */
static char *sig_replace_all(const char *hay, const char *needle, const char *repl) {
    size_t nl = strlen(needle), rl = strlen(repl);
    if (nl == 0) return sig_strdup(hay);
    size_t cap = strlen(hay) + 1, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = hay;
    while (*p) {
        if (strncmp(p, needle, nl) == 0) {
            while (len + rl + 1 > cap) { cap *= 2; o = realloc(o, cap); }
            memcpy(o + len, repl, rl); len += rl; p += nl;
        } else {
            if (len + 2 > cap) { cap *= 2; o = realloc(o, cap); }
            o[len++] = *p++;
        }
    }
    o[len] = '\0';
    return o;
}
/* codepoint-aware prefix slice (ASCII corpus: byte slice). */
static char *sig_slice(const char *s, size_t cap_cp) {
    if (!s) s = "";
    size_t i = 0, cp = 0;
    while (s[i] && cp < cap_cp) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        i += adv; cp++;
    }
    char *o = malloc(i + 1);
    if (o) { memcpy(o, s, i); o[i] = '\0'; }
    return o;
}
static int sig_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* urllib.parse.unquote for the ASCII %XX (<0x80) domain; '+' stays '+'. */
static char *sig_unquote(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n) {
            int hi = sig_hexval(s[i + 1]), lo = sig_hexval(s[i + 2]);
            if (hi >= 0 && lo >= 0) { o[j++] = (char)((hi << 4) | lo); i += 2; continue; }
        }
        o[j++] = s[i];
    }
    o[j] = '\0';
    return o;
}

/* ── ordered-map helpers (OrderedDict semantics) ── */
static void sig_pop_first(json_t *obj) {
    if (!obj || obj->type != JSON_OBJECT || obj->c.count == 0) return;
    free(obj->c.keys[0]);
    json_free(obj->c.items[0]);
    for (size_t j = 0; j + 1 < obj->c.count; j++) {
        obj->c.keys[j] = obj->c.keys[j + 1];
        obj->c.items[j] = obj->c.items[j + 1];
    }
    obj->c.count--;
}
static void sig_trim_to(json_t *obj, size_t cap) {
    while (obj && obj->type == JSON_OBJECT && obj->c.count > cap) sig_pop_first(obj);
}
/* array membership by scalar equality. */
static bool sig_arr_has(const json_t *arr, const json_t *target) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (sig_scalar_eq(json_get(arr, i), target)) return true;
    return false;
}
static bool sig_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0) return true;
    }
    return false;
}

/* ── SendResult (dataclasses.asdict, all 8 fields) ── */
static json_t *sig_send_result(bool success, const char *message_id, const char *error,
                               json_t *raw_response, bool retryable) {
    json_t *r = json_object();
    sig_set_b(r, "success", success);
    if (message_id) sig_set_s(r, "message_id", message_id); else sig_set_null(r, "message_id");
    if (error) sig_set_s(r, "error", error); else sig_set_null(r, "error");
    json_set(r, "raw_response", raw_response ? raw_response : json_null());
    sig_set_b(r, "retryable", retryable);
    sig_set_null(r, "retry_after");
    json_set(r, "continuation_message_ids", json_array());
    sig_set_null(r, "error_kind");
    return r;
}
/* logger.<level>(fmt, *args) seam; args recorded verbatim (no % formatting). */
static json_t *sig_log(sig_ctx_t *ctx, const char *level, const char *fmt, json_t *args) {
    json_t *a = sig_args();
    sig_set_s(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    char name[40];
    snprintf(name, sizeof name, "logger.%s", level);
    return sig_call(ctx, name, a);
}
/* _rpc seam args with the full live signature (defaults filled). */
static json_t *sig_rpc_args(const char *method, const json_t *params, const char *rpc_id,
                            bool log_failures, bool raise_on_rate_limit, double timeout) {
    json_t *a = sig_args();
    sig_set_s(a, "method", method);
    sig_set_copy(a, "params", params);
    if (rpc_id) sig_set_s(a, "rpc_id", rpc_id); else sig_set_null(a, "rpc_id");
    sig_set_b(a, "log_failures", log_failures);
    sig_set_b(a, "raise_on_rate_limit", raise_on_rate_limit);
    sig_set_n(a, "timeout", timeout);
    return a;
}

/* ================================================================
 * Leaves
 * ================================================================ */

/* PoP: sig__ext_to_mime @ plugins/platforms/signal/adapter.py:_ext_to_mime */
static json_t *sig_ext_to_mime_c(const char *ext) {
    /* mime_for_ext(ext, fallback="application/octet-stream"), no overrides:
     * key = (ext or "").strip().lower(); DEFAULT_EXT_TO_MIME.get(key, fallback). */
    char *stripped = sig_strip(ext ? ext : "");
    char *key = sig_lower(stripped);
    free(stripped);
    static const struct { const char *ext; const char *mime; } TAB[] = {
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
        {".gif", "image/gif"}, {".webp", "image/webp"}, {".ogg", "audio/ogg"},
        {".mp3", "audio/mpeg"}, {".wav", "audio/wav"}, {".m4a", "audio/mp4"},
        {".aac", "audio/aac"}, {".mp4", "video/mp4"}, {".pdf", "application/pdf"},
        {".zip", "application/zip"},
    };
    const char *mime = "application/octet-stream";
    for (size_t i = 0; i < sizeof(TAB) / sizeof(TAB[0]); i++) {
        if (strcmp(key, TAB[i].ext) == 0) { mime = TAB[i].mime; break; }
    }
    free(key);
    return json_string(mime);
}
static json_t *sig__ext_to_mime(sig_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ext = json_obj_get(args, "ext");
    const char *es = sig_truthy(ext) ? sig_str(ext) : NULL;
    return sig_ext_to_mime_c(es ? es : "");
}

/* PoP: sig__extract_quote_author @ plugins/platforms/signal/adapter.py:_extract_quote_author */
static json_t *sig_extract_quote_author_c(const json_t *quote_data) {
    if (!quote_data || quote_data->type != JSON_OBJECT) return json_null();
    static const char *KEYS[] = {
        "author", "authorNumber", "authorUuid", "authorAci",
        "authorServiceId", "authorServiceIdString",
    };
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        const json_t *v = json_obj_get(quote_data, KEYS[i]);
        if (sig_truthy(v)) {
            char *s = sig_pystr(v);
            json_t *r = json_string(s);
            free(s);
            return r;
        }
    }
    return json_null();
}
static json_t *sig__extract_quote_author(sig_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return sig_extract_quote_author_c(json_obj_get(args, "quote_data"));
}

/* PoP: sig__validate_send_result @ plugins/platforms/signal/adapter.py:_validate_send_result */
static json_t *sig_validate_send_result_c(const json_t *result) {
    const json_t *results = (result && result->type == JSON_OBJECT)
        ? json_obj_get(result, "results") : NULL;
    if (results && results->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(results); i++) {
            const json_t *r = json_get(results, i);
            if (!r || r->type != JSON_OBJECT) continue;
            const json_t *rtype = json_obj_get(r, "type");
            if (sig_truthy(rtype) && !sig_str_eq(rtype, "SUCCESS")) {
                char *s = sig_pystr(rtype);
                json_t *out = json_array();
                json_append(out, json_bool(false));
                json_append(out, json_string(s));
                free(s);
                return out;
            }
            if (json_has(r, "success") && !sig_truthy(json_obj_get(r, "success"))) {
                const json_t *fail = json_obj_get(r, "failure");
                char *s = sig_truthy(fail) ? sig_pystr(fail)
                                           : sig_strdup("Recipient delivery failed");
                json_t *out = json_array();
                json_append(out, json_bool(false));
                json_append(out, json_string(s));
                free(s);
                return out;
            }
        }
    }
    json_t *out = json_array();
    json_append(out, json_bool(true));
    json_append(out, json_null());
    return out;
}
static json_t *sig__validate_send_result(sig_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return sig_validate_send_result_c(json_obj_get(args, "result"));
}

/* PoP: sig__remember_sent_message_timestamp @ plugins/platforms/signal/adapter.py:_remember_sent_message_timestamp */
static json_t *sig_remember_sent_message_timestamp_c(sig_ctx_t *ctx, const json_t *timestamp) {
    if (sig_is_none(timestamp)) return json_null();
    char *key = sig_pystr(timestamp);
    if (!json_has(ctx->state, "_sent_message_timestamps")) { free(key); return sig_attr_err("_sent_message_timestamps"); }
    json_t *m = (json_t *)sig_self(ctx, "_sent_message_timestamps");
    json_obj_del(m, key);
    json_set(m, key, json_null());
    free(key);
    if (!json_has(ctx->state, "_max_sent_message_timestamps")) return sig_attr_err("_max_sent_message_timestamps");
    double maxn = json_get_num(ctx->state, "_max_sent_message_timestamps", 0);
    sig_trim_to(m, (size_t)maxn);
    return json_null();
}
static json_t *sig__remember_sent_message_timestamp(sig_ctx_t *ctx, const json_t *args) {
    return sig_remember_sent_message_timestamp_c(ctx, json_obj_get(args, "timestamp"));
}

/* PoP: sig__consume_sent_timestamp @ plugins/platforms/signal/adapter.py:_consume_sent_timestamp */
static json_t *sig_consume_sent_timestamp_c(sig_ctx_t *ctx, const json_t *ts) {
    if (!sig_truthy(ts)) return json_bool(false);
    if (!json_has(ctx->state, "_recent_sent_timestamps")) return sig_attr_err("_recent_sent_timestamps");
    json_t *recent = (json_t *)sig_self(ctx, "_recent_sent_timestamps");
    char *key = sig_pystr(ts);
    const json_t *v = json_obj_get(recent, key);
    bool hit = v && v->type != JSON_NULL;
    json_obj_del(recent, key);
    free(key);
    return json_bool(hit);
}
static json_t *sig__consume_sent_timestamp(sig_ctx_t *ctx, const json_t *args) {
    return sig_consume_sent_timestamp_c(ctx, json_obj_get(args, "ts"));
}

/* PoP: sig__track_sent_timestamp @ plugins/platforms/signal/adapter.py:_track_sent_timestamp */
static json_t *sig_track_sent_timestamp_c(sig_ctx_t *ctx, const json_t *rpc_result) {
    const json_t *ts = (rpc_result && rpc_result->type == JSON_OBJECT)
        ? json_obj_get(rpc_result, "timestamp") : NULL;
    if (!sig_truthy(ts)) return json_null();
    json_t *rr = sig_remember_sent_message_timestamp_c(ctx, ts);
    if (sig_is_raise(rr)) return rr;
    json_free(rr);
    json_t *nowr = sig_call(ctx, "time.monotonic", sig_args());
    if (sig_is_raise(nowr)) return nowr;
    double now = (nowr && nowr->type == JSON_NUMBER) ? nowr->num_val : 0.0;
    json_free(nowr);
    if (!json_has(ctx->state, "_recent_sent_timestamps")) return sig_attr_err("_recent_sent_timestamps");
    json_t *recent = (json_t *)sig_self(ctx, "_recent_sent_timestamps");
    char *key = sig_pystr(ts);
    json_obj_del(recent, key);
    json_set(recent, key, json_number(now));
    free(key);
    if (!json_has(ctx->state, "_recent_sent_ttl_seconds")) return sig_attr_err("_recent_sent_ttl_seconds");
    double ttl = json_get_num(ctx->state, "_recent_sent_ttl_seconds", 0);
    double cutoff = now - ttl;
    while (recent->c.count > 0) {
        const json_t *fv = recent->c.items[0];
        double fval = (fv && fv->type == JSON_NUMBER) ? fv->num_val : 0.0;
        if (fval < cutoff) sig_pop_first(recent); else break;
    }
    if (!json_has(ctx->state, "_max_recent_timestamps")) return sig_attr_err("_max_recent_timestamps");
    double maxn = json_get_num(ctx->state, "_max_recent_timestamps", 0);
    sig_trim_to(recent, (size_t)maxn);
    return json_null();
}
static json_t *sig__track_sent_timestamp(sig_ctx_t *ctx, const json_t *args) {
    return sig_track_sent_timestamp_c(ctx, json_obj_get(args, "rpc_result"));
}

/* ================================================================
 * Outbound routing / send chain
 * ================================================================ */

/* PoP: sig__with_target @ plugins/platforms/signal/adapter.py:_with_target */
static json_t *sig_with_target_c(sig_ctx_t *ctx, json_t *params, const char *chat_id, bool resolve) {
    if (strncmp(chat_id, "group:", 6) == 0) {
        json_set_hdr(params, "groupId", chat_id + 6);
    } else {
        json_t *recip;
        if (resolve) {
            json_t *ra = sig_args();
            sig_set_s(ra, "chat_id", chat_id);
            recip = sig_call(ctx, "_resolve_recipient", ra);
            if (sig_is_raise(recip)) return recip;
        } else {
            recip = json_string(chat_id);
        }
        json_t *arr = json_array();
        json_append(arr, recip);
        json_set(params, "recipient", arr);
    }
    return json_copy(params);
}
static json_t *sig__with_target(sig_ctx_t *ctx, const json_t *args) {
    json_t *params = json_obj_get(args, "params");
    if (!params || params->type != JSON_OBJECT) params = json_object();
    const char *chat_id = json_get_str(args, "chat_id", "");
    bool resolve = json_get_bool(args, "resolve", true);
    return sig_with_target_c(ctx, params, chat_id, resolve);
}

/* PoP: sig__rpc_send @ plugins/platforms/signal/adapter.py:_rpc_send */
static json_t *sig_rpc_send_c(sig_ctx_t *ctx, const json_t *params, const char *fail_error) {
    json_t *result = sig_call(ctx, "_rpc", sig_rpc_args("send", params, NULL, true, false, 30.0));
    if (sig_is_raise(result)) return result;
    if (sig_is_none(result)) {
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, sig_send_result(false, NULL, fail_error, NULL, false));
        json_free(result);
        return out;
    }
    json_t *vr = sig_validate_send_result_c(result);
    if (sig_is_raise(vr)) { json_free(result); return vr; }
    const json_t *succ = json_get(vr, 0);
    const json_t *errn = json_get(vr, 1);
    bool success = succ && succ->type == JSON_BOOL && succ->bool_val;
    if (!success) {
        const char *errmsg = sig_str(errn);
        json_t *out = json_array();
        json_append(out, json_null());
        json_append(out, sig_send_result(false, NULL, errmsg, json_copy(result), false));
        json_free(vr);
        json_free(result);
        return out;
    }
    json_free(vr);
    json_t *tr = sig_track_sent_timestamp_c(ctx, result);
    if (sig_is_raise(tr)) { json_free(result); return tr; }
    json_free(tr);
    json_t *out = json_array();
    json_append(out, result);   /* adopt */
    json_append(out, json_null());
    return out;
}
static json_t *sig__rpc_send(sig_ctx_t *ctx, const json_t *args) {
    return sig_rpc_send_c(ctx, json_obj_get(args, "params"), json_get_str(args, "fail_error", ""));
}

/* PoP: sig__send_file @ plugins/platforms/signal/adapter.py:_send_file */
static json_t *sig__send_file(sig_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *file_path = json_get_str(args, "file_path", "");
    const json_t *caption = json_obj_get(args, "caption");
    const char *fail_error = json_get_str(args, "fail_error", "");
    if (!json_has(ctx->state, "account")) return sig_attr_err("account");
    const json_t *acct = sig_self(ctx, "account");
    json_t *params = json_object();
    json_set(params, "account", json_copy(acct));
    const char *msg = sig_truthy(caption) ? sig_str(caption) : "";
    sig_set_s(params, "message", msg ? msg : "");
    json_t *atts = json_array();
    json_append(atts, json_string(file_path));
    json_set(params, "attachments", atts);
    json_t *wr = sig_with_target_c(ctx, params, chat_id, true);
    if (sig_is_raise(wr)) { json_free(params); return wr; }
    json_free(params);
    json_t *rsr = sig_rpc_send_c(ctx, wr, fail_error);
    json_free(wr);
    if (sig_is_raise(rsr)) return rsr;
    const json_t *err = json_get(rsr, 1);
    json_t *ret = sig_truthy(err) ? json_copy(err) : sig_send_result(true, NULL, NULL, NULL, false);
    json_free(rsr);
    return ret;
}

/* PoP: sig__send_reaction_rpc @ plugins/platforms/signal/adapter.py:_send_reaction_rpc */
static json_t *sig__send_reaction_rpc(sig_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    json_t *params = json_obj_get(args, "params");
    if (!params || params->type != JSON_OBJECT) params = json_object();
    json_t *wr = sig_with_target_c(ctx, params, chat_id, false);
    if (sig_is_raise(wr)) return wr;
    json_free(wr);   /* return value discarded (statement position) */
    json_t *rr = sig_call(ctx, "_rpc", sig_rpc_args("sendReaction", params, NULL, true, false, 30.0));
    if (sig_is_raise(rr)) return rr;
    json_t *ret = json_bool(!sig_is_none(rr));
    json_free(rr);
    return ret;
}

/* PoP: sig__resolve_image_path @ plugins/platforms/signal/adapter.py:_resolve_image_path */
static json_t *sig__resolve_image_path(sig_ctx_t *ctx, const json_t *args) {
    const char *image_url = json_get_str(args, "image_url", "");
    char *file_path = NULL;
    bool owned_path = false;
    if (strncmp(image_url, "file://", 7) == 0) {
        file_path = sig_unquote(image_url + 7);
        owned_path = true;
    } else {
        json_t *ca = sig_args();
        sig_set_s(ca, "image_url", image_url);
        json_t *cr = sig_call(ctx, "cache_image_from_url", ca);
        if (sig_is_raise(cr)) {
            if (sig_is_base_exc(sig_kind(cr))) return cr;
            /* except Exception as e: return None, "download", e (str(e) == message) */
            const char *m = json_get_str(cr, "message", "");
            json_t *out = json_array();
            json_append(out, json_null());
            json_append(out, json_string("download"));
            json_append(out, json_string(m));
            json_free(cr);
            return out;
        }
        const char *fp = sig_str(cr);
        file_path = sig_strdup(fp ? fp : "");
        owned_path = true;
        json_free(cr);
    }
    json_t *out = json_array();
    if (!file_path || !file_path[0]) {
        /* not file_path -> missing (no Path.exists call) */
        json_append(out, json_null());
        json_append(out, json_string("missing"));
        json_append(out, json_null());
        if (owned_path) free(file_path);
        return out;
    }
    json_t *ea = sig_args();
    sig_set_s(ea, "path", file_path);
    json_t *er = sig_call(ctx, "Path.exists", ea);
    if (sig_is_raise(er)) { if (owned_path) free(file_path); return er; }
    bool exists = sig_truthy(er);
    json_free(er);
    if (!exists) {
        json_append(out, json_null());
        json_append(out, json_string("missing"));
        json_append(out, json_null());
        if (owned_path) free(file_path);
        return out;
    }
    json_t *sa = sig_args();
    sig_set_s(sa, "path", file_path);
    json_t *sr = sig_call(ctx, "Path.stat", sa);
    if (sig_is_raise(sr)) { if (owned_path) free(file_path); return sr; }
    double file_size = json_get_num(sr, "st_size", 0);
    json_free(sr);
    if (file_size > 100.0 * 1024 * 1024) {
        json_append(out, json_null());
        json_append(out, json_string("oversize"));
        json_append(out, json_number(file_size));
        if (owned_path) free(file_path);
        return out;
    }
    json_append(out, json_string(file_path));
    json_append(out, json_null());
    json_append(out, json_null());
    if (owned_path) free(file_path);
    return out;
}

/* PoP: sig__send_attachment_batch @ plugins/platforms/signal/adapter.py:_send_attachment_batch */
static json_t *sig__send_attachment_batch(sig_ctx_t *ctx, const json_t *args) {
    const json_t *params = json_obj_get(args, "params");
    double n = json_get_num(args, "n", 0);
    const char *label = json_get_str(args, "label", "");
    const json_t *scheduler = json_obj_get(args, "scheduler");
    /* _signal_send_timeout(n) seam */
    json_t *ta = sig_args();
    sig_set_n(ta, "num_attachments", n);
    json_t *tr = sig_call(ctx, "_signal_send_timeout", ta);
    if (sig_is_raise(tr)) return tr;
    double send_timeout = (tr && tr->type == JSON_NUMBER) ? tr->num_val : 0.0;
    json_free(tr);
    const double max_attempts = 2.0;   /* SIGNAL_RATE_LIMIT_MAX_ATTEMPTS */
    for (double attempt = 1; attempt <= max_attempts; attempt++) {
        if (sig_is_none(scheduler))
            return sig_err("AttributeError", "'NoneType' object has no attribute 'acquire'");
        json_t *aa = sig_args();
        sig_set_n(aa, "n", n);
        json_t *ar = sig_call(ctx, "scheduler.acquire", aa);
        if (sig_is_raise(ar)) return ar;
        json_free(ar);
        json_t *t0r = sig_call(ctx, "time.monotonic", sig_args());
        if (sig_is_raise(t0r)) return t0r;
        double t0 = (t0r && t0r->type == JSON_NUMBER) ? t0r->num_val : 0.0;
        json_free(t0r);
        json_t *result = sig_call(ctx, "_rpc",
            sig_rpc_args("send", params, NULL, true, true, send_timeout));
        if (sig_is_raise(result)) {
            if (strcmp(sig_kind(result), "SignalRateLimitError") != 0) return result;
            /* except SignalRateLimitError as e */
            const json_t *retry_after = json_obj_get(result, "retry_after");
            json_t *fa = sig_args();
            sig_set_copy(fa, "retry_after", retry_after);
            sig_set_n(fa, "n", n);
            json_t *fr = sig_call(ctx, "scheduler.feedback", fa);
            if (sig_is_raise(fr)) { json_free(result); return fr; }
            json_free(fr);
            char retry_buf[64];
            const char *retry_str;
            if (sig_truthy(retry_after)) {
                snprintf(retry_buf, sizeof retry_buf, "%.0fs", retry_after->num_val);
                retry_str = retry_buf;
            } else {
                retry_str = "unknown";
            }
            if (attempt >= max_attempts) {
                json_t *la = json_array();
                json_append(la, json_string(label));
                json_append(la, json_number(n));
                json_append(la, json_string(retry_str));
                json_t *lr = sig_log(ctx, "error",
                    "Signal: rate-limit retries exhausted on batch %s (%d attachments lost, "
                    "server retry_after=%s)", la);
                if (sig_is_raise(lr)) { json_free(result); return lr; }
                json_free(lr);
                json_free(result);
                return json_bool(false);
            }
            json_t *la = json_array();
            json_append(la, json_string(label));
            json_append(la, json_number(attempt));
            json_append(la, json_number(max_attempts));
            json_append(la, json_string(retry_str));
            json_t *lr = sig_log(ctx, "warning",
                "Signal: rate-limited on batch %s (attempt %d/%d, server retry_after=%s); "
                "scheduler will pace the retry", la);
            if (sig_is_raise(lr)) { json_free(result); return lr; }
            json_free(lr);
            json_free(result);
            continue;
        }
        json_t *t1r = sig_call(ctx, "time.monotonic", sig_args());
        if (sig_is_raise(t1r)) { json_free(result); return t1r; }
        double duration = ((t1r && t1r->type == JSON_NUMBER) ? t1r->num_val : 0.0) - t0;
        json_free(t1r);
        bool success = false;
        const char *err_msg = NULL;
        char *err_dup = NULL;
        if (!sig_is_none(result)) {
            json_t *vr = sig_validate_send_result_c(result);
            if (sig_is_raise(vr)) { json_free(result); return vr; }
            const json_t *succ = json_get(vr, 0);
            success = succ && succ->type == JSON_BOOL && succ->bool_val;
            const json_t *errn = json_get(vr, 1);
            if (!sig_is_none(errn)) { err_dup = sig_pystr(errn); err_msg = err_dup; }
            json_free(vr);
        }
        if (success) {
            json_t *tsr = sig_track_sent_timestamp_c(ctx, result);
            if (sig_is_raise(tsr)) { free(err_dup); json_free(result); return tsr; }
            json_free(tsr);
            json_t *ra = sig_args();
            sig_set_n(ra, "duration", duration);
            sig_set_n(ra, "n", n);
            json_t *rr = sig_call(ctx, "scheduler.report_rpc_duration", ra);
            if (sig_is_raise(rr)) { free(err_dup); json_free(result); return rr; }
            json_free(rr);
            json_t *la = json_array();
            json_append(la, json_string(label));
            json_append(la, json_number(n));
            json_append(la, json_number(duration));
            json_append(la, json_number(attempt));
            json_append(la, json_number(max_attempts));
            json_t *lr = sig_log(ctx, "info",
                "Signal batch %s: %d attachments sent in %.1fs (attempt %d/%d)", la);
            if (sig_is_raise(lr)) { free(err_dup); json_free(result); return lr; }
            json_free(lr);
            free(err_dup);
            json_free(result);
            return json_bool(true);
        }
        /* failure logger.error; suffix f": {err_msg}" if result is not None else "" */
        char *suffix = sig_strdup("");
        if (!sig_is_none(result)) {
            size_t sl = strlen(err_msg ? err_msg : "") + 3;
            suffix = malloc(sl);
            snprintf(suffix, sl, ": %s", err_msg ? err_msg : "");
        }
        json_t *la = json_array();
        json_append(la, json_string(label));
        json_append(la, json_number(n));
        json_append(la, json_number(attempt));
        json_append(la, json_number(max_attempts));
        json_append(la, json_number(duration));
        json_append(la, json_string(suffix));
        free(suffix);
        json_t *lr = sig_log(ctx, "error",
            "Signal: RPC send failed for batch %s (%d attachments, attempt %d/%d, rpc_duration=%.1fs)%s", la);
        if (sig_is_raise(lr)) { free(err_dup); json_free(result); return lr; }
        json_free(lr);
        if (attempt >= max_attempts) {
            free(err_dup);
            json_free(result);
            return json_bool(false);
        }
        double backoff = 1.0;
        for (double k = 0; k < attempt; k++) backoff *= 2.0;   /* 2.0 ** attempt */
        json_t *ba = json_array();
        json_append(ba, json_string(label));
        json_append(ba, json_number(backoff));
        json_t *bl = sig_log(ctx, "info", "Signal: retrying batch %s after %.1fs backoff", ba);
        if (sig_is_raise(bl)) { free(err_dup); json_free(result); return bl; }
        json_free(bl);
        json_t *slp = sig_args();
        sig_set_n(slp, "delay", backoff);
        json_t *sr = sig_call(ctx, "asyncio.sleep", slp);
        if (sig_is_raise(sr)) { free(err_dup); json_free(result); return sr; }
        json_free(sr);
        free(err_dup);
        json_free(result);
    }
    return json_bool(false);
}

/* ================================================================
 * Inbound SSE / envelope cluster
 * ================================================================ */

/* PoP: sig__close_client @ plugins/platforms/signal/adapter.py:_close_client */
static json_t *sig__close_client(sig_ctx_t *ctx, const json_t *args) {
    (void)args;
    if (!json_has(ctx->state, "client")) return sig_attr_err("client");
    const json_t *client = sig_self(ctx, "client");
    if (sig_truthy(client)) {
        json_t *ar = sig_call(ctx, "client.aclose", sig_args());
        if (sig_is_raise(ar)) return ar;
        json_free(ar);
        json_set(ctx->state, "client", json_null());
    }
    return json_null();
}

/* PoP: sig__handle_sse_line @ plugins/platforms/signal/adapter.py:_handle_sse_line */
static json_t *sig__handle_sse_line(sig_ctx_t *ctx, const json_t *args) {
    const char *line = json_get_str(args, "line", "");
    if (line[0] == ':') {
        json_t *tr = sig_call(ctx, "time.time", sig_args());
        if (sig_is_raise(tr)) return tr;
        json_set(ctx->state, "_last_sse_activity", tr);   /* adopt */
        return json_null();
    }
    if (strncmp(line, "data:", 5) == 0) {
        char *data_str = sig_strip(line + 5);
        if (data_str[0]) {
            json_t *tr = sig_call(ctx, "time.time", sig_args());
            if (sig_is_raise(tr)) { free(data_str); return tr; }
            json_set(ctx->state, "_last_sse_activity", tr);   /* adopt */
            json_t *ja = sig_args();
            sig_set_s(ja, "data", data_str);
            json_t *jr = sig_call(ctx, "json.loads", ja);
            json_t *exc = NULL;
            if (sig_is_raise(jr)) {
                exc = jr;
            } else {
                json_t *ha = sig_args();
                sig_set_copy(ha, "envelope", jr);
                json_free(jr);
                json_t *hr = sig_call(ctx, "_handle_envelope", ha);
                if (sig_is_raise(hr)) exc = hr; else json_free(hr);
            }
            if (exc) {
                const char *kind = sig_kind(exc);
                if (strcmp(kind, "JSONDecodeError") == 0) {
                    char *sl = sig_slice(data_str, 100);
                    json_t *la = json_array();
                    json_append(la, json_string(sl));
                    free(sl);
                    json_t *lr = sig_log(ctx, "debug", "Signal SSE: invalid JSON: %s", la);
                    json_free(exc);
                    free(data_str);
                    if (sig_is_raise(lr)) return lr;
                    json_free(lr);
                    return json_null();
                }
                if (sig_is_base_exc(kind)) { free(data_str); return exc; }
                json_t *lr = sig_log(ctx, "exception", "Signal SSE: error handling event", json_array());
                json_free(exc);
                free(data_str);
                if (sig_is_raise(lr)) return lr;
                json_free(lr);
                return json_null();
            }
        }
        free(data_str);
    }
    return json_null();
}

/* PoP: sig__unwrap_sync_message @ plugins/platforms/signal/adapter.py:_unwrap_sync_message */
static json_t *sig__unwrap_sync_message(sig_ctx_t *ctx, const json_t *args) {
    const json_t *env = json_obj_get(args, "envelope_data");
    const json_t *sync_msg = env ? json_obj_get(env, "syncMessage") : NULL;
    const json_t *sent_msg = (sync_msg && sync_msg->type == JSON_OBJECT)
        ? json_obj_get(sync_msg, "sentMessage") : NULL;
    if (!sig_truthy(sent_msg) || sent_msg->type != JSON_OBJECT) return json_null();
    const json_t *destn = json_obj_get(sent_msg, "destinationNumber");
    const json_t *dest = sig_truthy(destn) ? destn : json_obj_get(sent_msg, "destination");
    if (!json_has(ctx->state, "_account_normalized")) return sig_attr_err("_account_normalized");
    const json_t *acct = sig_self(ctx, "_account_normalized");
    const json_t *gi = json_obj_get(sent_msg, "groupInfo");
    const json_t *gid = (gi && gi->type == JSON_OBJECT) ? json_obj_get(gi, "groupId") : NULL;
    if (!sig_scalar_eq(dest, acct) && !sig_truthy(gid)) return json_null();
    json_t *ca = sig_args();
    sig_set_copy(ca, "ts", json_obj_get(sent_msg, "timestamp"));
    json_t *cr = sig_consume_sent_timestamp_c(ctx, json_obj_get(ca, "ts"));
    json_free(ca);
    if (sig_is_raise(cr)) return cr;
    bool consumed = cr && cr->type == JSON_BOOL && cr->bool_val;
    json_free(cr);
    if (consumed) return json_null();
    json_t *out = json_copy(env);
    json_set(out, "dataMessage", json_copy(sent_msg));
    return out;
}

/* PoP: sig__apply_group_mention_rules @ plugins/platforms/signal/adapter.py:_apply_group_mention_rules */
static json_t *sig__apply_group_mention_rules(sig_ctx_t *ctx, const json_t *args) {
    const json_t *textn = json_obj_get(args, "text");
    const json_t *dm = json_obj_get(args, "data_message");
    if (!json_has(ctx->state, "_account_normalized")) return sig_attr_err("_account_normalized");
    const json_t *acct = sig_self(ctx, "_account_normalized");
    bool acct_truthy = sig_truthy(acct);
    const char *an = sig_str(acct);
    const char *text_s = sig_str(textn);
    if (!json_has(ctx->state, "require_mention")) return sig_attr_err("require_mention");
    const json_t *rm = sig_self(ctx, "require_mention");
    if (sig_truthy(rm)) {
        bool mit = false;
        if (acct_truthy && an) {
            size_t al = strlen(an) + 2;
            char *at = malloc(al);
            snprintf(at, al, "@%s", an);
            mit = strstr(text_s ? text_s : "", at) != NULL;
            free(at);
        }
        bool mim = false;
        const json_t *mentions = dm ? json_obj_get(dm, "mentions") : NULL;
        if (sig_truthy(mentions) && mentions->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(mentions); i++) {
                const json_t *m = json_get(mentions, i);
                const json_t *num = m ? json_obj_get(m, "number") : NULL;
                const json_t *uuid = m ? json_obj_get(m, "uuid") : NULL;
                if (acct_truthy && (sig_scalar_eq(acct, num) || sig_scalar_eq(acct, uuid))) {
                    mim = true;
                    break;
                }
            }
        }
        if (!mit && !mim) {
            json_t *lr = sig_log(ctx, "debug",
                "Signal: ignoring group message (require_mention=true, bot not mentioned)", json_array());
            if (sig_is_raise(lr)) return lr;
            json_free(lr);
            json_t *out = json_array();
            json_append(out, json_bool(false));
            json_append(out, textn ? json_copy(textn) : json_null());
            return out;
        }
    }
    json_t *result_text = textn ? json_copy(textn) : json_null();
    if (sig_truthy(textn) && acct_truthy && text_s && an) {
        size_t al = strlen(an) + 2;
        char *at = malloc(al);
        snprintf(at, al, "@%s", an);
        char *t1 = sig_replace_all(text_s, at, "");
        free(at);
        if (!json_has(ctx->state, "_recipient_uuid_by_number")) {
            free(t1);
            json_free(result_text);
            return sig_attr_err("_recipient_uuid_by_number");
        }
        const json_t *rmap = sig_self(ctx, "_recipient_uuid_by_number");
        const json_t *bot_uuid = json_obj_get(rmap, an);
        char *t2 = t1;
        if (sig_truthy(bot_uuid)) {
            const char *bu = sig_str(bot_uuid);
            if (bu) {
                size_t bl = strlen(bu) + 2;
                char *at2 = malloc(bl);
                snprintf(at2, bl, "@%s", bu);
                t2 = sig_replace_all(t1, at2, "");
                free(at2);
                free(t1);
            }
        }
        char *t3 = sig_replace_all(t2, "  ", " ");
        free(t2);
        char *t4 = sig_strip(t3);
        free(t3);
        json_free(result_text);
        result_text = json_string(t4);
        free(t4);
    }
    json_t *out = json_array();
    json_append(out, json_bool(true));
    json_append(out, result_text);
    return out;
}

/* PoP: sig__collect_attachments @ plugins/platforms/signal/adapter.py:_collect_attachments */
static json_t *sig__collect_attachments(sig_ctx_t *ctx, const json_t *args) {
    const json_t *atts = json_obj_get(args, "attachments_data");
    json_t *media_urls = json_array();
    json_t *media_types = json_array();
    if (atts && atts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(atts); i++) {
            const json_t *att = json_get(atts, i);
            const json_t *att_id = att ? json_obj_get(att, "id") : NULL;
            double att_size = att ? json_get_num(att, "size", 0) : 0;
            if (!sig_truthy(att_id)) continue;
            if (att_size > 100.0 * 1024 * 1024) {
                json_t *la = json_array();
                json_append(la, json_number(att_size));
                json_t *lr = sig_log(ctx, "warning",
                    "Signal: attachment too large (%d bytes), skipping", la);
                if (sig_is_raise(lr)) { json_free(media_urls); json_free(media_types); return lr; }
                json_free(lr);
                continue;
            }
            json_t *fa = sig_args();
            sig_set_copy(fa, "attachment_id", att_id);
            json_t *fr = sig_call(ctx, "_fetch_attachment", fa);
            if (sig_is_raise(fr)) {
                if (sig_is_base_exc(sig_kind(fr))) {
                    json_free(media_urls); json_free(media_types);
                    return fr;
                }
                const char *ids = sig_str(att_id);
                json_t *la = json_array();
                json_append(la, json_string(ids ? ids : ""));
                json_t *lr = sig_log(ctx, "exception", "Signal: failed to fetch attachment %s", la);
                json_free(fr);
                if (sig_is_raise(lr)) { json_free(media_urls); json_free(media_types); return lr; }
                json_free(lr);
                continue;
            }
            /* cached_path, ext = fr (a 2-array) */
            const json_t *cached_path = json_get(fr, 0);
            const json_t *ext = json_get(fr, 1);
            if (sig_truthy(cached_path)) {
                json_append(media_urls, json_copy(cached_path));
                const json_t *ct = att ? json_obj_get(att, "contentType") : NULL;
                if (sig_truthy(ct)) {
                    json_append(media_types, json_copy(ct));
                } else {
                    const char *es = sig_str(ext);
                    json_append(media_types, sig_ext_to_mime_c(es ? es : ""));
                }
            }
            json_free(fr);
        }
    }
    json_t *out = json_array();
    json_append(out, media_urls);
    json_append(out, media_types);
    return out;
}

/* PoP: sig__group_allowed @ plugins/platforms/signal/adapter.py:_group_allowed */
static json_t *sig__group_allowed(sig_ctx_t *ctx, const json_t *args) {
    const json_t *gid = json_obj_get(args, "group_id");
    if (!json_has(ctx->state, "group_allow_from")) return sig_attr_err("group_allow_from");
    const json_t *gaf = sig_self(ctx, "group_allow_from");
    if (!sig_truthy(gaf)) {
        json_t *lr = sig_log(ctx, "debug",
            "Signal: ignoring group message (no SIGNAL_GROUP_ALLOWED_USERS)", json_array());
        if (sig_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    if (!sig_arr_has_str(gaf, "*") && !sig_arr_has(gaf, gid)) {
        const char *gids = sig_str(gid);
        char *sl = sig_truthy(gid) ? sig_slice(gids ? gids : "", 8) : sig_strdup("?");
        json_t *la = json_array();
        json_append(la, json_string(sl));
        free(sl);
        json_t *lr = sig_log(ctx, "debug", "Signal: group %s not in allowlist", la);
        if (sig_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(false);
    }
    return json_bool(true);
}