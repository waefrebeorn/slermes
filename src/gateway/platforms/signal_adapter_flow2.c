/*
 * signal_adapter_flow2.c — Signal adapter FLOW remainder of
 * plugins/platforms/signal/adapter.py (hermes-agent).
 *
 * Ports the DECISION SEQUENCES of the unclaimed remainder left after
 * signal_adapter_flow.c took the inbound SSE / sync-unwrap / group-mention /
 * attachment-collection cluster and the outbound routing / send-RPC /
 * image-resolution / rate-limited-batch chain. This second flow port takes:
 *   - identity / config leaves: _is_signal_service_id, validate_signal_config,
 *     _remember_recipient_identifiers, _extract_contact_uuid,
 *     _quote_references_own_message, _extract_reaction_target, format_message;
 *   - the UTF-16 chunking cluster: _utf16_offsets, _styles_for_chunk,
 *     _split_signal_formatted_message;
 *   - the reaction-hook cluster: _reactions_enabled, send_reaction,
 *     remove_reaction, on_processing_start, on_processing_complete;
 *   - the typing-backoff cluster: _stop_typing_indicator, stop_typing,
 *     send_typing;
 *   - the recipient-resolution / JSON-RPC / send-path cluster: _resolve_recipient,
 *     _rpc, get_chat_info, send_image, _send_attachment, send.
 *
 * 24 ported functions. Every one was only STUB-claimed elsewhere (printf /
 * return-constant / strstr / system() heuristics in src/plugins/platforms/
 * signal/{signal.c,signal_ports.c,signal_helpers2.c,signal_gaps.c}); this flow
 * port re-claims them with real behaviour, mirroring the signal_adapter_flow.c
 * precedent (a flow port re-claiming stub-only leaves with real bodies). The 18
 * REAL flow siblings claimed by signal_adapter_flow.c are SEAMED here, never
 * re-ported: _with_target, _rpc_send, _send_file, _send_reaction_rpc,
 * _resolve_image_path (the ones this cluster calls).
 *
 * Universal seam: every SEAMED sibling / foreign call enters through ONE
 * callback sig2_call(ctx, name, args). The harness wires ctx->io to the case's
 * scripted results; the oracle installs recording stubs emitting the identical
 * event shape {"name","args","ret"} from the SAME scripts. args is ADOPTED into
 * the trace event; the return is a NEW owned json_t (NULL == Python None;
 * {"__raise__":cls,"message":str(exc),...} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 * BaseException kinds (SystemExit / KeyboardInterrupt / CancelledError /
 * GeneratorExit) pass through every catch site: the catches here are
 * _is_signal_service_id's suppress(ValueError,AttributeError,TypeError),
 * _styles_for_chunk's except (TypeError,ValueError), _send_attachment's except
 * FileNotFoundError, _stop_typing_indicator's suppress(Exception), _rpc's
 * except SignalRateLimitError / except Exception, and send_image's implicit
 * reason navigation — all narrower than BaseException, so scripted
 * BaseException raises always propagate.
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL (run inline, direct C call): every ported function below. A REAL
 *    function calls its REAL children directly (the _c cores) and its SEAMED
 *    children via sig2_call. REAL child edges: _is_signal_service_id <-
 *    {_remember_recipient_identifiers,_extract_contact_uuid};
 *    {_extract_contact_uuid,_remember_recipient_identifiers} <- _resolve_recipient;
 *    {_utf16_offsets,_styles_for_chunk} <- _split_signal_formatted_message;
 *    _stop_typing_indicator <- {stop_typing,send,send_image,_send_attachment};
 *    {_reactions_enabled,_extract_reaction_target,send_reaction,remove_reaction}
 *    <- {on_processing_start,on_processing_complete}; _rpc <-
 *    {_stop_typing_indicator,send_typing,get_chat_info,_resolve_recipient};
 *    _split_signal_formatted_message <- send.
 *  SEAMED (sig2_call, scripted): the claimed siblings _with_target, _rpc_send,
 *    _send_file, _send_reaction_rpc, _resolve_image_path; the foreign
 *    collaborators client.post, resp.raise_for_status, resp.json, time.time,
 *    time.monotonic, cancel_task, _sig_secret, uuid.UUID, markdown_to_signal,
 *    Path.stat, _is_sender_authorized, _looks_like_e164_number,
 *    _is_signal_rate_limit_error, _extract_retry_after_seconds; and
 *    logger.debug / warning / error / info / exception / log.
 *  INLINED pure foreign helper (no seam, mirrors the _ext_to_mime precedent):
 *    utf16_len (base.py) — UTF-8 -> UTF-16 code-unit count, transcribed.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and sig2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_signal_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}; a SignalRateLimitError
 *   raise additionally carries "retry_after" (number or null).
 * - The adapter self double is ctx.state (a JSON object of live attributes);
 *   methods read/write it in place. A DIRECT self._attr read with the key
 *   ABSENT raises AttributeError "'SignalAdapter' object has no attribute 'X'"
 *   (the oracle builds a real type("SignalAdapter", (), clsdict) double).
 * - The MessageEvent double (on_processing_* / _extract_reaction_target) is a
 *   JSON object; a DIRECT event.<attr> read with the key absent raises
 *   AttributeError "'MessageEvent' object has no attribute 'X'". Its mutation
 *   (event._signal_eyes_sent) is emitted as out.state.event. source is a nested
 *   object double; getattr(event,"source",None) / getattr(source,"user_id",None)
 *   are presence-safe (absent -> None), direct .chat_id reads are not.
 * - _sent_message_timestamps is an OrderedDict[str, None] (string keys, null
 *   values); membership only. _recipient_uuid_by_number / _recipient_number_by_uuid
 *   are plain str->str dicts. _typing_tasks / _typing_failures / _typing_skip_until
 *   are plain dicts mutated in place (pop / set). group/dm_allow_from are sets in
 *   live but only membership + truthiness is used, so the oracle keeps them as
 *   lists (fixture order) and the C port scans the JSON array.
 * - tuple returns project to arrays; SendResult projects via dataclasses.asdict
 *   to ALL 8 fields {success, message_id, error, raw_response, retryable,
 *   retry_after, continuation_message_ids ([] for the () default), error_kind}.
 * - ProcessingOutcome is materialized from its string value ("success" /
 *   "failure" / "cancelled"); _OUTCOME_REACTION maps success->"✅", failure->"❌"
 *   (opaque UTF-8 literals passed verbatim to seams; cancelled has no entry).
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral floats).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - ASCII scope: text / chat ids / names / paths / style strings are ASCII. The
 *   only non-ASCII literals are the reaction emoji "👀"/"✅"/"❌", which are
 *   OPAQUE pass-through values (never sliced / lowered / compared bytewise against
 *   fixture text). str.strip() uses the ASCII set {09,0a,0b,0c,0d,1c,1d,1e,1f,20}.
 *   str()-computed scalars carry str/int/bool/None only.
 * - utf16_len is INLINED (UTF-8 -> UTF-16 units; ASCII char = 1, astral = 2), so
 *   chunking fixtures use ASCII bodies (offsets == codepoint indices).
 * - int() parsing in _styles_for_chunk is restricted to clean [+-]?[0-9]+ tokens
 *   (the fixture domain); malformed tokens raise ValueError -> logger.debug.
 *   repr() for the logger.debug %r arg is transcribed for the ASCII string domain.
 * - Path.stat is SEAMED (no real filesystem); returns {"st_size": n} or raises
 *   FileNotFoundError. client.post returns a seamed marker; resp.raise_for_status
 *   / resp.json are seamed (the resp double is implicit). uuid.UUID is seamed
 *   (scripted parse marker or ValueError/AttributeError/TypeError raise).
 * - time.time / time.monotonic are scripted numbers (integral in the corpus).
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
typedef struct sig2_ctx {
    json_t *state;      /* owned self double; mutated in place; emitted as out.state.self */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    json_t *event;      /* owned MessageEvent double (on_processing_*); emitted as out.state.event */
} sig2_ctx_t;

/* ── basic helpers ── */
static bool sig2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *sig2_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool sig2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool sig2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool sig2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *sig2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool sig2_str_eq(const json_t *v, const char *s) {
    const char *t = sig2_str(v);
    return t && s && strcmp(t, s) == 0;
}
/* Python scalar == (str/num/bool/null); different types are unequal. */
static bool sig2_scalar_eq(const json_t *a, const json_t *b) {
    if (sig2_is_none(a) && sig2_is_none(b)) return true;
    if (sig2_is_none(a) || sig2_is_none(b)) return false;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    return false;
}
static json_t *sig2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *sig2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = sig2_err(type, msg);
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys[i];
            if (strcmp(k, "type") == 0 || strcmp(k, "message") == 0) continue;
            json_set(e, k, json_copy(spec->c.items[i]));
        }
    }
    return e;
}
static json_t *sig2_attr_err(const char *key) {
    char msg[160];
    snprintf(msg, sizeof msg, "'SignalAdapter' object has no attribute '%s'", key);
    return sig2_err("AttributeError", msg);
}
static json_t *sig2_event_attr_err(const char *key) {
    char msg[160];
    snprintf(msg, sizeof msg, "'MessageEvent' object has no attribute '%s'", key);
    return sig2_err("AttributeError", msg);
}
/* The one universal seam. args ADOPTED; return fresh owned node. */
static json_t *sig2_call(sig2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sig2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sig2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sig2_make_exc(raise);
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
static json_t *sig2_args(void) { return json_object(); }
static void sig2_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void sig2_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void sig2_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void sig2_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void sig2_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *sig2_self(const sig2_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}

/* ── string helpers ── */
static char *sig2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* str.strip() ASCII whitespace set (py3.14 includes 1c-1f). */
static bool sig2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
static char *sig2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sig2_is_ws((unsigned char)s[a])) a++;
    while (b > a && sig2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
static char *sig2_lower(const char *s) {
    char *o = sig2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* Python str() projection for scalar nodes (containers out of domain). */
static char *sig2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sig2_strdup("None");
    if (v->type == JSON_BOOL) return sig2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return sig2_strdup(buf);
    }
    if (v->type == JSON_STRING) return sig2_strdup(v->str_val ? v->str_val : "");
    return sig2_strdup("");
}
/* codepoint-aware prefix slice (ASCII corpus: byte slice). */
static char *sig2_slice(const char *s, size_t cap_cp) {
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
/* Python repr() for the ASCII string domain (logger.debug %r arg). Prefers
 * single quotes; switches to double quotes when the string contains ' but no ";
 * escapes backslash, the active quote, and the common control characters. */
static char *sig2_repr_ascii(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *o = malloc(n * 4 + 3);
    if (!o) return NULL;
    size_t j = 0;
    o[j++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') { o[j++] = '\\'; o[j++] = (char)c; }
        else if (c == '\n') { o[j++] = '\\'; o[j++] = 'n'; }
        else if (c == '\r') { o[j++] = '\\'; o[j++] = 'r'; }
        else if (c == '\t') { o[j++] = '\\'; o[j++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            j += (size_t)snprintf(o + j, 5, "\\x%02x", c);
        } else o[j++] = (char)c;
    }
    o[j++] = q;
    o[j] = '\0';
    return o;
}
/* array membership by scalar equality. */
static bool sig2_arr_has(const json_t *arr, const json_t *target) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (sig2_scalar_eq(json_get(arr, i), target)) return true;
    return false;
}
static bool sig2_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0) return true;
    }
    return false;
}
/* number at an array index (def if absent / non-number). */
static double sig2_arr_num(const json_t *arr, size_t idx, double def) {
    const json_t *v = json_get(arr, idx);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}
/* dict pop(key, default): remove key, return its value (or null). Mutates obj. */
static json_t *sig2_dict_pop(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return json_null();
    const json_t *v = json_obj_get(obj, key);
    json_t *out = v ? json_copy(v) : json_null();
    json_obj_del(obj, key);
    return out;
}

/* ── SendResult (dataclasses.asdict, all 8 fields) ── */
static json_t *sig2_send_result(bool success, const char *message_id, const char *error,
                                json_t *raw_response, bool retryable) {
    json_t *r = json_object();
    sig2_set_b(r, "success", success);
    if (message_id) sig2_set_s(r, "message_id", message_id); else sig2_set_null(r, "message_id");
    if (error) sig2_set_s(r, "error", error); else sig2_set_null(r, "error");
    json_set(r, "raw_response", raw_response ? raw_response : json_null());
    sig2_set_b(r, "retryable", retryable);
    sig2_set_null(r, "retry_after");
    json_set(r, "continuation_message_ids", json_array());
    sig2_set_null(r, "error_kind");
    return r;
}
/* logger.<level>(fmt, *args) seam; args recorded verbatim (no % formatting). */
static json_t *sig2_log(sig2_ctx_t *ctx, const char *level, const char *fmt, json_t *args) {
    json_t *a = sig2_args();
    sig2_set_s(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    char name[40];
    snprintf(name, sizeof name, "logger.%s", level);
    return sig2_call(ctx, name, a);
}
/* logger.log(level_int, fmt, *args) seam (_rpc fail_level path). */
static json_t *sig2_log_level(sig2_ctx_t *ctx, double level, const char *fmt, json_t *args) {
    json_t *a = sig2_args();
    sig2_set_n(a, "level", level);
    sig2_set_s(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    return sig2_call(ctx, "logger.log", a);
}
/* _sig_secret(name, default) seam (get_scoped_secret). */
static json_t *sig2_sig_secret(sig2_ctx_t *ctx, const char *name, const char *dflt) {
    json_t *a = sig2_args();
    sig2_set_s(a, "name", name);
    sig2_set_s(a, "default", dflt);
    return sig2_call(ctx, "_sig_secret", a);
}

/* ── utf16_len (base.py) INLINED: UTF-8 -> UTF-16 code units ── */
static double sig2_utf16_len(const char *s) {
    if (!s) return 0.0;
    double units = 0.0;
    size_t i = 0, n = strlen(s);
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp;
        size_t adv;
        if (c < 0x80) { cp = c; adv = 1; }
        else if (c < 0xE0) { cp = c & 0x1F; adv = 2; }
        else if (c < 0xF0) { cp = c & 0x0F; adv = 3; }
        else { cp = c & 0x07; adv = 4; }
        for (size_t k = 1; k < adv && i + k < n; k++)
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        units += (cp > 0xFFFF) ? 2.0 : 1.0;
        i += adv;
    }
    return units;
}
/* codepoint count of a UTF-8 string (ASCII corpus: strlen). */
static size_t sig2_cp_count(const char *s) {
    if (!s) return 0;
    size_t i = 0, n = strlen(s), cp = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        i += adv; cp++;
    }
    return cp;
}
/* codepoint-indexed substring [a,b) of a UTF-8 string (ASCII: byte slice). */
static char *sig2_cp_slice(const char *s, size_t a, size_t b) {
    if (!s) s = "";
    size_t i = 0, cp = 0, n = strlen(s);
    /* Defaults: a beyond length -> start=n; b beyond length -> end=n.
     * a==0 / b==0 are boundary 0 (the loop's cp starts at 1, so handle here). */
    size_t start = (a == 0) ? 0 : n;
    size_t end = (b == 0) ? 0 : n;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        i += adv; cp++;
        if (cp == a) start = i;
        if (cp == b) { end = i; break; }
    }
    if (end < start) end = start;
    char *o = malloc(end - start + 1);
    if (o) { memcpy(o, s + start, end - start); o[end - start] = '\0'; }
    return o;
}

/* ================================================================
 * Identity / config / quote leaves
 * ================================================================ */

/* PoP: sig2_format_message @ plugins/platforms/signal/adapter.py:format_message */
static json_t *sig2_format_message_c(const json_t *content) {
    /* Plain-text fallback: return content verbatim (identity). */
    return content ? json_copy(content) : json_null();
}
static json_t *sig2__format_message(sig2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return sig2_format_message_c(json_obj_get(args, "content"));
}

/* PoP: sig2__extract_reaction_target @ plugins/platforms/signal/adapter.py:_extract_reaction_target */
static json_t *sig2_extract_reaction_target_c(sig2_ctx_t *ctx, const json_t *event) {
    if (!event || event->type != JSON_OBJECT) return sig2_event_attr_err("raw_message");
    if (!json_has(event, "raw_message")) return sig2_event_attr_err("raw_message");
    const json_t *raw = json_obj_get(event, "raw_message");
    bool ok = raw && raw->type == JSON_OBJECT
        && sig2_truthy(json_obj_get(raw, "sender"))
        && sig2_truthy(json_obj_get(raw, "timestamp_ms"));
    if (!ok) return json_null();
    json_t *out = json_array();
    json_append(out, json_copy(json_obj_get(raw, "sender")));
    json_append(out, json_copy(json_obj_get(raw, "timestamp_ms")));
    return out;
}
static json_t *sig2__extract_reaction_target(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_extract_reaction_target_c(ctx, json_obj_get(args, "event"));
}

/* PoP: sig2__quote_references_own_message @ plugins/platforms/signal/adapter.py:_quote_references_own_message */
static json_t *sig2__quote_references_own_message(sig2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "reply_to_id");
    const json_t *rauthor = json_obj_get(args, "reply_to_author");
    if (sig2_truthy(rid)) {
        if (!json_has(ctx->state, "_sent_message_timestamps")) return sig2_attr_err("_sent_message_timestamps");
        const json_t *sent = sig2_self(ctx, "_sent_message_timestamps");
        char *key = sig2_pystr(rid);
        bool hit = sent && sent->type == JSON_OBJECT && json_has(sent, key);
        free(key);
        if (hit) return json_bool(true);
    }
    if (!sig2_truthy(rauthor)) return json_bool(false);
    if (!json_has(ctx->state, "_account_normalized")) return sig2_attr_err("_account_normalized");
    const json_t *acct = sig2_self(ctx, "_account_normalized");
    char *author_raw = sig2_pystr(rauthor);
    char *author = sig2_strip(author_raw);
    free(author_raw);
    bool result = false;
    if (sig2_truthy(acct)) {
        const char *an = sig2_str(acct);
        if (an && strcmp(author, an) == 0) {
            result = true;
        } else {
            if (!json_has(ctx->state, "_recipient_uuid_by_number")) { free(author); return sig2_attr_err("_recipient_uuid_by_number"); }
            const json_t *umap = sig2_self(ctx, "_recipient_uuid_by_number");
            const json_t *bot_uuid = an ? json_obj_get(umap, an) : NULL;
            if (bot_uuid && bot_uuid->type == JSON_STRING && bot_uuid->str_val
                && strcmp(author, bot_uuid->str_val) == 0) {
                result = true;
            } else {
                if (!json_has(ctx->state, "_recipient_number_by_uuid")) { free(author); return sig2_attr_err("_recipient_number_by_uuid"); }
                const json_t *nmap = sig2_self(ctx, "_recipient_number_by_uuid");
                const json_t *num = json_obj_get(nmap, author);
                if (sig2_scalar_eq(num, acct)) result = true;
            }
        }
    }
    free(author);
    return json_bool(result);
}

/* PoP: sig2__is_signal_service_id @ plugins/platforms/signal/adapter.py:_is_signal_service_id */
static json_t *sig2_is_signal_service_id_c(sig2_ctx_t *ctx, const json_t *value) {
    if (!sig2_truthy(value)) return json_bool(false);
    const char *vs = sig2_str(value);
    if (!vs) return json_bool(false);
    if (strncmp(vs, "PNI:", 4) == 0 || strncmp(vs, "u:", 2) == 0) return json_bool(true);
    json_t *a = sig2_args();
    sig2_set_s(a, "value", vs);
    json_t *r = sig2_call(ctx, "uuid.UUID", a);
    if (sig2_is_raise(r)) {
        const char *kind = sig2_kind(r);
        /* suppress(ValueError, AttributeError, TypeError) */
        if (strcmp(kind, "ValueError") == 0 || strcmp(kind, "AttributeError") == 0
            || strcmp(kind, "TypeError") == 0) {
            json_free(r);
            return json_bool(false);
        }
        return r;   /* any other kind (incl. BaseException) propagates */
    }
    json_free(r);
    return json_bool(true);
}
static json_t *sig2__is_signal_service_id(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_is_signal_service_id_c(ctx, json_obj_get(args, "value"));
}

/* PoP: sig2__remember_recipient_identifiers @ plugins/platforms/signal/adapter.py:_remember_recipient_identifiers */
static json_t *sig2_remember_recipient_identifiers_c(sig2_ctx_t *ctx, const json_t *number, const json_t *service_id) {
    if (sig2_truthy(number) && sig2_truthy(service_id)) {
        json_t *vr = sig2_is_signal_service_id_c(ctx, service_id);
        if (sig2_is_raise(vr)) return vr;
        bool valid = vr && vr->type == JSON_BOOL && vr->bool_val;
        json_free(vr);
        if (valid) {
            if (!json_has(ctx->state, "_recipient_uuid_by_number")) return sig2_attr_err("_recipient_uuid_by_number");
            if (!json_has(ctx->state, "_recipient_number_by_uuid")) return sig2_attr_err("_recipient_number_by_uuid");
            const char *ns = sig2_str(number);
            const char *ss = sig2_str(service_id);
            json_t *umap = (json_t *)sig2_self(ctx, "_recipient_uuid_by_number");
            json_t *nmap = (json_t *)sig2_self(ctx, "_recipient_number_by_uuid");
            json_set_hdr(umap, ns ? ns : "", ss ? ss : "");
            json_set_hdr(nmap, ss ? ss : "", ns ? ns : "");
        }
    }
    return json_null();
}
static json_t *sig2__remember_recipient_identifiers(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_remember_recipient_identifiers_c(ctx, json_obj_get(args, "number"), json_obj_get(args, "service_id"));
}

/* PoP: sig2__extract_contact_uuid @ plugins/platforms/signal/adapter.py:_extract_contact_uuid */
static json_t *sig2_extract_contact_uuid_c(sig2_ctx_t *ctx, const json_t *contact, const json_t *phone_number) {
    if (!contact || contact->type != JSON_OBJECT) return json_null();
    const json_t *uuid = json_obj_get(contact, "uuid");
    const json_t *sid = json_obj_get(contact, "serviceId");
    const json_t *service_id = sig2_truthy(uuid) ? uuid : sid;
    const json_t *profile = json_obj_get(contact, "profile");
    if (!sig2_truthy(service_id) && profile && profile->type == JSON_OBJECT) {
        const json_t *psid = json_obj_get(profile, "serviceId");
        const json_t *puuid = json_obj_get(profile, "uuid");
        service_id = sig2_truthy(psid) ? psid : puuid;
    }
    if (sig2_truthy(service_id)) {
        json_t *vr = sig2_is_signal_service_id_c(ctx, service_id);
        if (sig2_is_raise(vr)) return vr;
        bool valid = vr && vr->type == JSON_BOOL && vr->bool_val;
        json_free(vr);
        if (valid) {
            const json_t *cnum = json_obj_get(contact, "number");
            const json_t *crecip = json_obj_get(contact, "recipient");
            if (sig2_scalar_eq(phone_number, cnum) || sig2_scalar_eq(phone_number, crecip))
                return json_copy(service_id);
        }
    }
    return json_null();
}
static json_t *sig2__extract_contact_uuid(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_extract_contact_uuid_c(ctx, json_obj_get(args, "contact"), json_obj_get(args, "phone_number"));
}

/* PoP: sig2_validate_signal_config @ plugins/platforms/signal/adapter.py:validate_signal_config */
static json_t *sig2_validate_signal_config_c(sig2_ctx_t *ctx, const json_t *config) {
    /* extra = getattr(config, "extra", {}) or {} */
    const json_t *extra = (config && config->type == JSON_OBJECT) ? json_obj_get(config, "extra") : NULL;
    if (!sig2_truthy(extra)) extra = NULL;   /* `or {}` -> empty dict */
    const json_t *hu = extra ? json_obj_get(extra, "http_url") : NULL;
    const json_t *ac = extra ? json_obj_get(extra, "account") : NULL;
    /* http_url = (extra.get("http_url","") or _sig_secret("SIGNAL_HTTP_URL","")).strip() */
    char *http_url = NULL;
    if (sig2_truthy(hu)) {
        http_url = sig2_pystr(hu);
    } else {
        json_t *sr = sig2_sig_secret(ctx, "SIGNAL_HTTP_URL", "");
        if (sig2_is_raise(sr)) return sr;
        http_url = sig2_pystr(sr);
        json_free(sr);
    }
    char *account = NULL;
    if (sig2_truthy(ac)) {
        account = sig2_pystr(ac);
    } else {
        json_t *sr = sig2_sig_secret(ctx, "SIGNAL_ACCOUNT", "");
        if (sig2_is_raise(sr)) { free(http_url); return sr; }
        account = sig2_pystr(sr);
        json_free(sr);
    }
    char *hs = sig2_strip(http_url); free(http_url);
    char *as = sig2_strip(account); free(account);
    bool ok = hs[0] != '\0' && as[0] != '\0';
    free(hs); free(as);
    return json_bool(ok);
}
static json_t *sig2__validate_signal_config(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_validate_signal_config_c(ctx, json_obj_get(args, "config"));
}

/* ================================================================
 * UTF-16 chunking cluster
 * ================================================================ */

/* PoP: sig2__utf16_offsets @ plugins/platforms/signal/adapter.py:_utf16_offsets */
static json_t *sig2_utf16_offsets_c(const char *text) {
    /* [0, *accumulate(utf16_len(char) for char in text)] */
    json_t *out = json_array();
    json_append(out, json_number(0));
    if (!text) return out;
    double acc = 0.0;
    size_t i = 0, n = strlen(text);
    while (i < n) {
        unsigned char c = (unsigned char)text[i];
        size_t adv = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        char tmp[8];
        size_t k = 0;
        for (; k < adv && i + k < n; k++) tmp[k] = text[i + k];
        tmp[k] = '\0';
        acc += sig2_utf16_len(tmp);
        json_append(out, json_number(acc));
        i += adv;
    }
    return out;
}
static json_t *sig2__utf16_offsets(sig2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *text = json_obj_get(args, "text");
    return sig2_utf16_offsets_c(sig2_str(text));
}

/* clean [+-]?[0-9]+ int parse (fixture domain); returns false on malformed. */
static bool sig2_parse_int(const char *s, long long *out) {
    if (!s) return false;
    const char *p = s;
    while (*p && sig2_is_ws((unsigned char)*p)) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    if (!(*p >= '0' && *p <= '9')) return false;
    while (*p >= '0' && *p <= '9') p++;
    const char *end = p;
    while (*p && sig2_is_ws((unsigned char)*p)) p++;
    if (*p != '\0') return false;
    char buf[64];
    size_t len = (size_t)(end - start);
    if (len >= sizeof buf) return false;
    memcpy(buf, start, len); buf[len] = '\0';
    *out = strtoll(buf, NULL, 10);
    return true;
}

/* PoP: sig2__styles_for_chunk @ plugins/platforms/signal/adapter.py:_styles_for_chunk */
static json_t *sig2_styles_for_chunk_c(sig2_ctx_t *ctx, const json_t *text_styles,
                                       double chunk_start, double chunk_end) {
    json_t *adjusted = json_array();
    if (!text_styles || text_styles->type != JSON_ARRAY) return adjusted;
    for (size_t i = 0; i < json_len(text_styles); i++) {
        const json_t *sn = json_get(text_styles, i);
        const char *ss = sig2_str(sn);
        bool malformed = false;
        long long style_start = 0, style_end = 0;
        char *style_type = NULL;
        if (!ss) {
            /* non-string: .split raises AttributeError (NOT caught) -> propagate */
            return sig2_err("AttributeError", "'int' object has no attribute 'split'");
        }
        /* style_string.split(":", 2) -> exactly 3 parts */
        const char *c1 = strchr(ss, ':');
        if (!c1) { malformed = true; }
        else {
            const char *c2 = strchr(c1 + 1, ':');
            if (!c2) { malformed = true; }
            else {
                size_t l1 = (size_t)(c1 - ss);
                size_t l2 = (size_t)(c2 - (c1 + 1));
                char *start_s = malloc(l1 + 1); memcpy(start_s, ss, l1); start_s[l1] = '\0';
                char *length_s = malloc(l2 + 1); memcpy(length_s, c1 + 1, l2); length_s[l2] = '\0';
                style_type = sig2_strdup(c2 + 1);
                long long st = 0, ln = 0;
                if (!sig2_parse_int(start_s, &st) || !sig2_parse_int(length_s, &ln)) {
                    malformed = true;
                } else {
                    style_start = st;
                    style_end = st + ln;
                }
                free(start_s); free(length_s);
            }
        }
        if (malformed) {
            json_t *la = json_array();
            json_append(la, json_string(ss));   /* %r applied at render time (seamed); arg is raw */
            json_t *lr = sig2_log(ctx, "debug", "[Signal] Ignoring malformed textStyle range: %r", la);
            free(style_type);
            if (sig2_is_raise(lr)) { json_free(adjusted); return lr; }
            json_free(lr);
            continue;
        }
        double overlap_start = (style_start > chunk_start) ? (double)style_start : chunk_start;
        double overlap_end = (style_end < chunk_end) ? (double)style_end : chunk_end;
        if (overlap_start < overlap_end) {
            char buf[128];
            snprintf(buf, sizeof buf, "%.0f:%.0f:%s",
                     overlap_start - chunk_start, overlap_end - overlap_start,
                     style_type ? style_type : "");
            json_append(adjusted, json_string(buf));
        }
        free(style_type);
    }
    return adjusted;
}
static json_t *sig2__styles_for_chunk(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_styles_for_chunk_c(ctx, json_obj_get(args, "text_styles"),
                                   json_get_num(args, "chunk_start", 0),
                                   json_get_num(args, "chunk_end", 0));
}

/* PoP: sig2__split_signal_formatted_message @ plugins/platforms/signal/adapter.py:_split_signal_formatted_message */
static json_t *sig2_split_signal_formatted_message_c(sig2_ctx_t *ctx, const char *plain_text,
                                                     const json_t *text_styles, double max_length) {
    if (!plain_text) plain_text = "";
    double total = sig2_utf16_len(plain_text);
    if (total <= max_length) {
        json_t *out = json_array();
        json_t *pair = json_array();
        json_append(pair, json_string(plain_text));
        json_append(pair, text_styles ? json_copy(text_styles) : json_array());
        json_append(out, pair);
        return out;
    }
    double body_limit = max_length - 10.0;
    if (body_limit < 1.0) body_limit = 1.0;
    json_t *offsets = sig2_utf16_offsets_c(plain_text);
    size_t noff = json_len(offsets);
    double total_u16 = sig2_arr_num(offsets, noff - 1, 0);
    json_t *chunks = json_array();   /* each: [text, styles] */
    size_t start_idx = 0;
    while (sig2_arr_num(offsets, start_idx, 0) < total_u16) {
        double start_off = sig2_arr_num(offsets, start_idx, 0);
        double end_budget = total_u16 < start_off + body_limit ? total_u16 : start_off + body_limit;
        size_t end_idx = start_idx + 1;
        while (end_idx < noff && sig2_arr_num(offsets, end_idx, 0) <= end_budget) end_idx++;
        if (end_idx - 1 < start_idx + 1) end_idx = start_idx + 1;
        else end_idx = end_idx - 1;
        json_t *cs = sig2_styles_for_chunk_c(ctx, text_styles, start_off, sig2_arr_num(offsets, end_idx, 0));
        if (sig2_is_raise(cs)) { json_free(offsets); json_free(chunks); return cs; }
        char *txt = sig2_cp_slice(plain_text, start_idx, end_idx);
        json_t *pair = json_array();
        json_append(pair, json_string(txt));
        json_append(pair, cs);
        free(txt);
        json_append(chunks, pair);
        start_idx = end_idx;
    }
    json_free(offsets);
    size_t nchunks = json_len(chunks);
    if (nchunks == 1) return chunks;
    json_t *out = json_array();
    for (size_t i = 0; i < nchunks; i++) {
        const json_t *pair = json_get(chunks, i);
        const char *txt = sig2_str(json_get(pair, 0));
        char buf[64];
        snprintf(buf, sizeof buf, "%s (%zu/%zu)", txt ? txt : "", i + 1, nchunks);
        json_t *np = json_array();
        json_append(np, json_string(buf));
        json_append(np, json_copy(json_get(pair, 1)));
        json_append(out, np);
    }
    json_free(chunks);
    return out;
}
static json_t *sig2__split_signal_formatted_message(sig2_ctx_t *ctx, const json_t *args) {
    const json_t *pt = json_obj_get(args, "plain_text");
    return sig2_split_signal_formatted_message_c(ctx, sig2_str(pt),
                                                 json_obj_get(args, "text_styles"),
                                                 json_get_num(args, "max_length", 0));
}

/* forward decls for REAL children defined in later sections */
static json_t *sig2_rpc_c(sig2_ctx_t *ctx, const char *method, const json_t *params,
                          const char *rpc_id, bool log_failures, bool raise_on_rate_limit, double timeout);

/* ================================================================
 * Reaction-hook cluster
 * ================================================================ */

static json_t *sig2_source_attr_err(const char *key) {
    char msg[160];
    snprintf(msg, sizeof msg, "'MessageSource' object has no attribute '%s'", key);
    return sig2_err("AttributeError", msg);
}
/* _is_sender_authorized(sender, chat_type, chat_id, is_bot=...) seam; the live
 * gate is `... is not False` (only the bool False singleton yields False). */
static json_t *sig2_is_sender_authorized(sig2_ctx_t *ctx, const json_t *sender,
                                         const json_t *chat_type, const json_t *chat_id, bool is_bot) {
    json_t *a = sig2_args();
    sig2_set_copy(a, "sender", sender);
    sig2_set_copy(a, "chat_type", chat_type);
    sig2_set_copy(a, "chat_id", chat_id);
    sig2_set_b(a, "is_bot", is_bot);
    return sig2_call(ctx, "_is_sender_authorized", a);
}

/* PoP: sig2__reactions_enabled @ plugins/platforms/signal/adapter.py:_reactions_enabled */
static json_t *sig2_reactions_enabled_c(sig2_ctx_t *ctx, const json_t *event) {
    json_t *sr = sig2_sig_secret(ctx, "SIGNAL_REACTIONS", "true");
    if (sig2_is_raise(sr)) return sr;
    char *raw = sig2_pystr(sr);
    json_free(sr);
    char *low = sig2_lower(raw);
    free(raw);
    bool off = strcmp(low, "false") == 0 || strcmp(low, "0") == 0 || strcmp(low, "no") == 0;
    free(low);
    if (off) return json_bool(false);
    /* source = getattr(event, "source", None); sender = getattr(source, "user_id", None) */
    const json_t *source = (event && event->type == JSON_OBJECT) ? json_obj_get(event, "source") : NULL;
    const json_t *sender = (source && source->type == JSON_OBJECT) ? json_obj_get(source, "user_id") : NULL;
    if (!sig2_truthy(sender)) return json_bool(true);
    if (!json_has(ctx->state, "dm_allow_from")) return sig2_attr_err("dm_allow_from");
    const json_t *dm = sig2_self(ctx, "dm_allow_from");
    if (!sig2_arr_has_str(dm, "*") && !sig2_arr_has(dm, sender)) return json_bool(false);
    /* source.chat_type / source.chat_id are DIRECT reads (absent -> AttributeError) */
    if (!source || source->type != JSON_OBJECT || !json_has(source, "chat_type"))
        return sig2_source_attr_err("chat_type");
    if (!json_has(source, "chat_id")) return sig2_source_attr_err("chat_id");
    const json_t *chat_type = json_obj_get(source, "chat_type");
    const json_t *chat_id = json_obj_get(source, "chat_id");
    bool is_bot = sig2_truthy(json_obj_get(source, "is_bot"));
    json_t *ar = sig2_is_sender_authorized(ctx, sender, chat_type, chat_id, is_bot);
    if (sig2_is_raise(ar)) return ar;
    bool not_false = !(ar && ar->type == JSON_BOOL && !ar->bool_val);
    json_free(ar);
    return json_bool(not_false);
}
static json_t *sig2__reactions_enabled(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_reactions_enabled_c(ctx, json_obj_get(args, "event"));
}

/* PoP: sig2_send_reaction @ plugins/platforms/signal/adapter.py:send_reaction */
static json_t *sig2_send_reaction_c(sig2_ctx_t *ctx, const char *chat_id, const char *emoji,
                                    const json_t *target_author, const json_t *target_timestamp) {
    if (!json_has(ctx->state, "account")) return sig2_attr_err("account");
    const json_t *acct = sig2_self(ctx, "account");
    json_t *params = json_object();
    json_set(params, "account", json_copy(acct));
    sig2_set_s(params, "emoji", emoji);
    sig2_set_copy(params, "targetAuthor", target_author);
    sig2_set_copy(params, "targetTimestamp", target_timestamp);
    json_t *a = sig2_args();
    sig2_set_s(a, "chat_id", chat_id);
    json_set(a, "params", params);   /* adopt */
    json_t *ok = sig2_call(ctx, "_send_reaction_rpc", a);
    if (sig2_is_raise(ok)) return ok;
    if (!sig2_truthy(ok)) {
        char *sl = sig2_slice(chat_id, 20);
        json_t *la = json_array();
        json_append(la, json_string(sl));
        json_append(la, json_string(emoji ? emoji : ""));
        free(sl);
        json_t *lr = sig2_log(ctx, "debug", "Signal: sendReaction failed (chat=%s, emoji=%s)", la);
        if (sig2_is_raise(lr)) { json_free(ok); return lr; }
        json_free(lr);
    }
    return ok;
}
static json_t *sig2__send_reaction(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_send_reaction_c(ctx, json_get_str(args, "chat_id", ""),
                                json_get_str(args, "emoji", ""),
                                json_obj_get(args, "target_author"),
                                json_obj_get(args, "target_timestamp"));
}

/* PoP: sig2_remove_reaction @ plugins/platforms/signal/adapter.py:remove_reaction */
static json_t *sig2_remove_reaction_c(sig2_ctx_t *ctx, const char *chat_id,
                                      const json_t *target_author, const json_t *target_timestamp) {
    if (!json_has(ctx->state, "account")) return sig2_attr_err("account");
    const json_t *acct = sig2_self(ctx, "account");
    json_t *params = json_object();
    json_set(params, "account", json_copy(acct));
    sig2_set_s(params, "emoji", "");
    sig2_set_copy(params, "targetAuthor", target_author);
    sig2_set_copy(params, "targetTimestamp", target_timestamp);
    sig2_set_b(params, "remove", true);
    json_t *a = sig2_args();
    sig2_set_s(a, "chat_id", chat_id);
    json_set(a, "params", params);   /* adopt */
    return sig2_call(ctx, "_send_reaction_rpc", a);
}
static json_t *sig2__remove_reaction(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_remove_reaction_c(ctx, json_get_str(args, "chat_id", ""),
                                  json_obj_get(args, "target_author"),
                                  json_obj_get(args, "target_timestamp"));
}

/* read event.source.chat_id (DIRECT reads; absent -> AttributeError). */
static json_t *sig2_event_chat_id(sig2_ctx_t *ctx, const json_t *event, char **out) {
    (void)ctx;
    if (!event || event->type != JSON_OBJECT || !json_has(event, "source"))
        return sig2_event_attr_err("source");
    const json_t *source = json_obj_get(event, "source");
    if (!source || source->type != JSON_OBJECT || !json_has(source, "chat_id"))
        return sig2_source_attr_err("chat_id");
    const json_t *cid = json_obj_get(source, "chat_id");
    *out = sig2_pystr(cid);
    return NULL;
}

/* PoP: sig2_on_processing_start @ plugins/platforms/signal/adapter.py:on_processing_start */
static json_t *sig2__on_processing_start(sig2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *event = ctx->event;
    json_t *er = sig2_reactions_enabled_c(ctx, event);
    if (sig2_is_raise(er)) return er;
    bool enabled = er && er->type == JSON_BOOL && er->bool_val;
    json_free(er);
    if (!enabled) return json_null();
    json_t *target = sig2_extract_reaction_target_c(ctx, event);
    if (sig2_is_raise(target)) return target;
    if (!sig2_truthy(target)) { json_free(target); return json_null(); }
    char *chat_id = NULL;
    json_t *ce = sig2_event_chat_id(ctx, event, &chat_id);
    if (ce) { json_free(target); return ce; }
    const json_t *author = json_get(target, 0);
    const json_t *ts = json_get(target, 1);
    json_t *sr = sig2_send_reaction_c(ctx, chat_id, "\xF0\x9F\x91\x80", author, ts);
    free(chat_id);
    json_free(target);
    if (sig2_is_raise(sr)) return sr;
    /* event._signal_eyes_sent = <result> */
    if (event && event->type == JSON_OBJECT) json_set(event, "_signal_eyes_sent", sr);
    else json_free(sr);
    return json_null();
}

/* PoP: sig2_on_processing_complete @ plugins/platforms/signal/adapter.py:on_processing_complete */
static json_t *sig2__on_processing_complete(sig2_ctx_t *ctx, const json_t *args) {
    const char *outcome = json_get_str(args, "outcome", "");
    json_t *event = ctx->event;
    if (strcmp(outcome, "cancelled") == 0) return json_null();
    bool eyes = sig2_truthy(event ? json_obj_get(event, "_signal_eyes_sent") : NULL);
    if (!eyes) return json_null();
    json_t *target = sig2_extract_reaction_target_c(ctx, event);
    if (sig2_is_raise(target)) return target;
    if (!sig2_truthy(target)) { json_free(target); return json_null(); }
    char *chat_id = NULL;
    json_t *ce = sig2_event_chat_id(ctx, event, &chat_id);
    if (ce) { json_free(target); return ce; }
    const json_t *author = json_get(target, 0);
    const json_t *ts = json_get(target, 1);
    json_t *rr = sig2_remove_reaction_c(ctx, chat_id, author, ts);
    if (sig2_is_raise(rr)) { free(chat_id); json_free(target); return rr; }
    json_free(rr);
    json_t *er = sig2_reactions_enabled_c(ctx, event);
    if (sig2_is_raise(er)) { free(chat_id); json_free(target); return er; }
    bool enabled = er && er->type == JSON_BOOL && er->bool_val;
    json_free(er);
    if (enabled) {
        const char *emoji = NULL;
        if (strcmp(outcome, "success") == 0) emoji = "\xE2\x9C\x85";
        else if (strcmp(outcome, "failure") == 0) emoji = "\xE2\x9D\x8C";
        if (emoji) {
            json_t *sr = sig2_send_reaction_c(ctx, chat_id, emoji, author, ts);
            if (sig2_is_raise(sr)) { free(chat_id); json_free(target); return sr; }
            json_free(sr);
        }
    }
    free(chat_id);
    json_free(target);
    return json_null();
}

/* ================================================================
 * Typing-backoff cluster
 * ================================================================ */

/* PoP: sig2__stop_typing_indicator @ plugins/platforms/signal/adapter.py:_stop_typing_indicator */
static json_t *sig2_stop_typing_indicator_c(sig2_ctx_t *ctx, const char *chat_id) {
    if (!json_has(ctx->state, "_typing_tasks")) return sig2_attr_err("_typing_tasks");
    json_t *tasks = (json_t *)sig2_self(ctx, "_typing_tasks");
    json_t *task = sig2_dict_pop(tasks, chat_id);
    json_t *ca = sig2_args();
    json_set(ca, "task", task);   /* adopt */
    json_t *cr = sig2_call(ctx, "cancel_task", ca);
    if (sig2_is_raise(cr)) return cr;
    json_free(cr);
    /* with suppress(Exception): account read + _with_target + _rpc */
    json_t *suppressed = NULL;
    if (!json_has(ctx->state, "account")) {
        suppressed = sig2_attr_err("account");   /* AttributeError swallowed */
    } else {
        const json_t *acct = sig2_self(ctx, "account");
        json_t *p0 = json_object();
        json_set(p0, "account", json_copy(acct));
        json_t *wa = sig2_args();
        sig2_set_s(wa, "chat_id", chat_id);
        sig2_set_b(wa, "resolve", true);
        json_set(wa, "params", p0);   /* adopt */
        json_t *wr = sig2_call(ctx, "_with_target", wa);
        if (sig2_is_raise(wr)) {
            if (sig2_is_base_exc(sig2_kind(wr))) return wr;
            suppressed = wr;
        } else {
            json_set(wr, "stop", json_bool(true));
            json_t *rr = sig2_rpc_c(ctx, "sendTyping", wr, "typing-stop", false, false, 30.0);
            json_free(wr);
            if (sig2_is_raise(rr)) {
                if (sig2_is_base_exc(sig2_kind(rr))) return rr;
                suppressed = rr;
            } else json_free(rr);
        }
    }
    if (suppressed) json_free(suppressed);
    /* backoff cleanup happens even when the RPC block was suppressed */
    if (!json_has(ctx->state, "_typing_failures")) return sig2_attr_err("_typing_failures");
    if (!json_has(ctx->state, "_typing_skip_until")) return sig2_attr_err("_typing_skip_until");
    json_t *fails = (json_t *)sig2_self(ctx, "_typing_failures");
    json_t *skip = (json_t *)sig2_self(ctx, "_typing_skip_until");
    json_obj_del(fails, chat_id);
    json_obj_del(skip, chat_id);
    return json_null();
}
static json_t *sig2__stop_typing_indicator(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_stop_typing_indicator_c(ctx, json_get_str(args, "chat_id", ""));
}

/* PoP: sig2_stop_typing @ plugins/platforms/signal/adapter.py:stop_typing */
static json_t *sig2__stop_typing(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_stop_typing_indicator_c(ctx, json_get_str(args, "chat_id", ""));
}

/* PoP: sig2_send_typing @ plugins/platforms/signal/adapter.py:send_typing */
static json_t *sig2__send_typing(sig2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    json_t *nowr = sig2_call(ctx, "time.monotonic", sig2_args());
    if (sig2_is_raise(nowr)) return nowr;
    double now = (nowr && nowr->type == JSON_NUMBER) ? nowr->num_val : 0.0;
    json_free(nowr);
    if (!json_has(ctx->state, "_typing_skip_until")) return sig2_attr_err("_typing_skip_until");
    json_t *skip = (json_t *)sig2_self(ctx, "_typing_skip_until");
    double until = json_get_num(skip, chat_id, 0.0);
    if (now < until) return json_null();
    if (!json_has(ctx->state, "account")) return sig2_attr_err("account");
    const json_t *acct = sig2_self(ctx, "account");
    json_t *p0 = json_object();
    json_set(p0, "account", json_copy(acct));
    json_t *wa = sig2_args();
    sig2_set_s(wa, "chat_id", chat_id);
    sig2_set_b(wa, "resolve", true);
    json_set(wa, "params", p0);   /* adopt */
    json_t *params = sig2_call(ctx, "_with_target", wa);
    if (sig2_is_raise(params)) return params;
    if (!json_has(ctx->state, "_typing_failures")) { json_free(params); return sig2_attr_err("_typing_failures"); }
    json_t *fails = (json_t *)sig2_self(ctx, "_typing_failures");
    double fn = json_get_num(fails, chat_id, 0.0);
    json_t *result = sig2_rpc_c(ctx, "sendTyping", params, "typing", (fn == 0.0), false, 30.0);
    json_free(params);
    if (sig2_is_raise(result)) return result;
    if (!sig2_is_none(result)) {
        json_free(result);
        json_obj_del(fails, chat_id);
        json_obj_del(skip, chat_id);
        return json_null();
    }
    json_free(result);
    double newfails = fn + 1.0;
    json_set(fails, chat_id, json_number(newfails));
    if (newfails >= 3.0) {
        double backoff = 16.0;
        for (double k = 0; k < newfails - 3.0; k++) backoff *= 2.0;
        if (backoff > 60.0) backoff = 60.0;
        json_set(skip, chat_id, json_number(now + backoff));
    }
    return json_null();
}

/* ================================================================
 * JSON-RPC core / recipient resolution / send path
 * ================================================================ */

/* A raise inside _rpc's try propagates iff BaseException or SignalRateLimitError
 * (except SignalRateLimitError: raise); every other Exception is swallowed by
 * except Exception -> logger.log + return None. */
static bool sig2_rpc_propagates(const json_t *r) {
    const char *k = sig2_kind(r);
    return sig2_is_base_exc(k) || strcmp(k, "SignalRateLimitError") == 0;
}

/* PoP: sig2__rpc @ plugins/platforms/signal/adapter.py:_rpc */
static json_t *sig2_rpc_c(sig2_ctx_t *ctx, const char *method, const json_t *params,
                          const char *rpc_id, bool log_failures, bool raise_on_rate_limit, double timeout) {
    if (!json_has(ctx->state, "client")) return sig2_attr_err("client");
    const json_t *client = sig2_self(ctx, "client");
    if (!sig2_truthy(client)) {
        json_t *lr = sig2_log(ctx, "warning", "Signal: RPC called but client not connected", json_array());
        if (sig2_is_raise(lr)) return lr;
        json_free(lr);
        return json_null();
    }
    /* payload (built BEFORE the try; time.time raise propagates uncaught) */
    json_t *payload = json_object();
    sig2_set_s(payload, "jsonrpc", "2.0");
    sig2_set_s(payload, "method", method);
    sig2_set_copy(payload, "params", params);
    if (rpc_id) {
        sig2_set_s(payload, "id", rpc_id);
    } else {
        json_t *tr = sig2_call(ctx, "time.time", sig2_args());
        if (sig2_is_raise(tr)) { json_free(payload); return tr; }
        double t = (tr && tr->type == JSON_NUMBER) ? tr->num_val : 0.0;
        json_free(tr);
        long long ms = (long long)(t * 1000.0);
        char idbuf[160];
        snprintf(idbuf, sizeof idbuf, "%s_%lld", method, ms);
        sig2_set_s(payload, "id", idbuf);
    }
    double fail_level = log_failures ? 30.0 : 10.0;
    json_t *caught = NULL;
    json_t *data = NULL;
    /* try: */
    if (!json_has(ctx->state, "http_url")) { caught = sig2_attr_err("http_url"); goto except_handler; }
    const char *hu = sig2_str(sig2_self(ctx, "http_url"));
    size_t ulen = strlen(hu ? hu : "") + 16;
    char *url = malloc(ulen);
    snprintf(url, ulen, "%s/api/v1/rpc", hu ? hu : "");
    json_t *pa = sig2_args();
    sig2_set_s(pa, "url", url);
    free(url);
    json_set(pa, "json", payload);   /* adopt payload */
    sig2_set_n(pa, "timeout", timeout);
    json_t *pr = sig2_call(ctx, "client.post", pa);
    if (sig2_is_raise(pr)) {
        if (sig2_rpc_propagates(pr)) return pr;
        caught = pr; goto except_handler;
    }
    json_free(pr);
    json_t *rr = sig2_call(ctx, "resp.raise_for_status", sig2_args());
    if (sig2_is_raise(rr)) {
        if (sig2_rpc_propagates(rr)) return rr;
        caught = rr; goto except_handler;
    }
    json_free(rr);
    data = sig2_call(ctx, "resp.json", sig2_args());
    if (sig2_is_raise(data)) {
        if (sig2_rpc_propagates(data)) return data;
        caught = data; data = NULL; goto except_handler;
    }
    if (data && data->type == JSON_OBJECT && json_has(data, "error")) {
        const json_t *err = json_obj_get(data, "error");
        if (raise_on_rate_limit) {
            json_t *ia = sig2_args();
            sig2_set_copy(ia, "err", err);
            json_t *ir = sig2_call(ctx, "_is_signal_rate_limit_error", ia);
            if (sig2_is_raise(ir)) {
                json_free(data);
                if (sig2_rpc_propagates(ir)) return ir;
                caught = ir; goto except_handler;
            }
            bool is_rl = sig2_truthy(ir);
            json_free(ir);
            if (is_rl) {
                char *err_msg;
                if (err && err->type == JSON_OBJECT) {
                    if (json_has(err, "message")) err_msg = sig2_pystr(json_obj_get(err, "message"));
                    else err_msg = sig2_strdup("");
                } else err_msg = sig2_pystr(err);
                json_t *ea = sig2_args();
                sig2_set_copy(ea, "err", err);
                json_t *er = sig2_call(ctx, "_extract_retry_after_seconds", ea);
                if (sig2_is_raise(er)) {
                    free(err_msg); json_free(data);
                    if (sig2_rpc_propagates(er)) return er;
                    caught = er; goto except_handler;
                }
                json_t *spec = sig2_err("SignalRateLimitError", err_msg);
                json_set(spec, "retry_after", er);   /* adopt */
                free(err_msg);
                json_free(data);
                return spec;
            }
        }
        json_t *la = json_array();
        json_append(la, json_string(method));
        json_append(la, err ? json_copy(err) : json_null());
        json_t *lr = sig2_log_level(ctx, fail_level, "Signal RPC error (%s): %s", la);
        if (sig2_is_raise(lr)) {
            json_free(data);
            if (sig2_rpc_propagates(lr)) return lr;
            caught = lr; goto except_handler;
        }
        json_free(lr);
        json_free(data);
        return json_null();
    }
    {
        const json_t *result = (data && data->type == JSON_OBJECT) ? json_obj_get(data, "result") : NULL;
        if (result && result->type == JSON_OBJECT && raise_on_rate_limit) {
            const json_t *results = json_obj_get(result, "results");
            if (results && results->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len(results); i++) {
                    const json_t *r = json_get(results, i);
                    if (r && r->type == JSON_OBJECT && sig2_str_eq(json_obj_get(r, "type"), "RATE_LIMIT_FAILURE")) {
                        const json_t *retry = json_obj_get(r, "retryAfterSeconds");
                        json_t *spec = sig2_err("SignalRateLimitError", "Rate limit exceeded for recipient");
                        json_set(spec, "retry_after", retry ? json_copy(retry) : json_null());
                        json_free(data);
                        return spec;
                    }
                }
            }
        }
        json_t *ret = result ? json_copy(result) : json_null();
        json_free(data);
        return ret;
    }
except_handler:
    /* except Exception as e: logger.log(fail_level, "Signal RPC %s failed: %s", method, e); return None */
    if (sig2_is_base_exc(sig2_kind(caught))) return caught;
    {
        const char *emsg = json_get_str(caught, "message", "");
        json_t *la = json_array();
        json_append(la, json_string(method));
        json_append(la, json_string(emsg));
        json_t *lr = sig2_log_level(ctx, fail_level, "Signal RPC %s failed: %s", la);
        json_free(caught);
        if (sig2_is_raise(lr)) return lr;
        json_free(lr);
        return json_null();
    }
}
static json_t *sig2__rpc(sig2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rpc_id");
    return sig2_rpc_c(ctx, json_get_str(args, "method", ""), json_obj_get(args, "params"),
                      sig2_is_none(rid) ? NULL : sig2_str(rid),
                      json_get_bool(args, "log_failures", true),
                      json_get_bool(args, "raise_on_rate_limit", false),
                      json_get_num(args, "timeout", 30.0));
}

/* PoP: sig2__resolve_recipient @ plugins/platforms/signal/adapter.py:_resolve_recipient */
static json_t *sig2_resolve_recipient_c(sig2_ctx_t *ctx, const json_t *chat_id_node) {
    const char *chat_id = sig2_str(chat_id_node);
    if (!chat_id) chat_id = "";
    if (!sig2_truthy(chat_id_node) || strncmp(chat_id, "group:", 6) == 0)
        return chat_id_node ? json_copy(chat_id_node) : json_null();
    json_t *la = sig2_args();
    sig2_set_s(la, "value", chat_id);
    json_t *lr = sig2_call(ctx, "_looks_like_e164_number", la);
    if (sig2_is_raise(lr)) return lr;
    bool e164 = sig2_truthy(lr);
    json_free(lr);
    if (!e164) return json_copy(chat_id_node);
    if (!json_has(ctx->state, "_recipient_uuid_by_number")) return sig2_attr_err("_recipient_uuid_by_number");
    json_t *umap = (json_t *)sig2_self(ctx, "_recipient_uuid_by_number");
    const json_t *cached = json_obj_get(umap, chat_id);
    if (sig2_truthy(cached)) return json_copy(cached);
    /* async with self._recipient_cache_lock: (inert; no observable state) */
    cached = json_obj_get(umap, chat_id);
    if (sig2_truthy(cached)) return json_copy(cached);
    if (!json_has(ctx->state, "account")) return sig2_attr_err("account");
    const json_t *acct = sig2_self(ctx, "account");
    json_t *p0 = json_object();
    json_set(p0, "account", json_copy(acct));
    sig2_set_b(p0, "allRecipients", true);
    json_t *contacts = sig2_rpc_c(ctx, "listContacts", p0, NULL, true, false, 30.0);
    json_free(p0);
    if (sig2_is_raise(contacts)) return contacts;
    if (contacts && contacts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(contacts); i++) {
            const json_t *contact = json_get(contacts, i);
            const json_t *number = (contact && contact->type == JSON_OBJECT) ? json_obj_get(contact, "number") : NULL;
            json_t *service_id = sig2_extract_contact_uuid_c(ctx, contact, chat_id_node);
            if (sig2_is_raise(service_id)) { json_free(contacts); return service_id; }
            if (sig2_truthy(number) && sig2_truthy(service_id)) {
                json_t *rr = sig2_remember_recipient_identifiers_c(ctx, number, service_id);
                if (sig2_is_raise(rr)) { json_free(service_id); json_free(contacts); return rr; }
                json_free(rr);
            }
            json_free(service_id);
        }
    }
    json_free(contacts);
    const json_t *final = json_obj_get(umap, chat_id);
    return final ? json_copy(final) : json_copy(chat_id_node);
}
static json_t *sig2__resolve_recipient(sig2_ctx_t *ctx, const json_t *args) {
    return sig2_resolve_recipient_c(ctx, json_obj_get(args, "chat_id"));
}

/* PoP: sig2_get_chat_info @ plugins/platforms/signal/adapter.py:get_chat_info */
static json_t *sig2__get_chat_info(sig2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    if (strncmp(chat_id, "group:", 6) == 0) {
        json_t *out = json_object();
        sig2_set_s(out, "name", chat_id);
        sig2_set_s(out, "type", "group");
        sig2_set_s(out, "chat_id", chat_id);
        return out;
    }
    if (!json_has(ctx->state, "account")) return sig2_attr_err("account");
    const json_t *acct = sig2_self(ctx, "account");
    json_t *p0 = json_object();
    json_set(p0, "account", json_copy(acct));
    sig2_set_s(p0, "contactAddress", chat_id);
    json_t *result = sig2_rpc_c(ctx, "getContact", p0, NULL, true, false, 30.0);
    json_free(p0);
    if (sig2_is_raise(result)) return result;
    const json_t *name = NULL;
    if (result && result->type == JSON_OBJECT) {
        const json_t *n = json_obj_get(result, "name");
        const json_t *pn = json_obj_get(result, "profileName");
        name = sig2_truthy(n) ? n : pn;
    }
    json_t *out = json_object();
    if (sig2_truthy(name)) json_set(out, "name", json_copy(name));
    else sig2_set_s(out, "name", chat_id);
    sig2_set_s(out, "type", "dm");
    sig2_set_s(out, "chat_id", chat_id);
    json_free(result);
    return out;
}

/* PoP: sig2_send_image @ plugins/platforms/signal/adapter.py:send_image */
static json_t *sig2__send_image(sig2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *image_url = json_get_str(args, "image_url", "");
    const json_t *caption = json_obj_get(args, "caption");
    json_t *sr = sig2_stop_typing_indicator_c(ctx, chat_id);
    if (sig2_is_raise(sr)) return sr;
    json_free(sr);
    json_t *ra = sig2_args();
    sig2_set_s(ra, "image_url", image_url);
    json_t *rr = sig2_call(ctx, "_resolve_image_path", ra);
    if (sig2_is_raise(rr)) return rr;
    const json_t *file_path = json_get(rr, 0);
    const json_t *reason = json_get(rr, 1);
    const json_t *detail = json_get(rr, 2);
    if (sig2_str_eq(reason, "download")) {
        json_t *la = json_array();
        json_append(la, detail ? json_copy(detail) : json_null());
        json_t *lr = sig2_log(ctx, "warning", "Signal: failed to download image: %s", la);
        if (sig2_is_raise(lr)) { json_free(rr); return lr; }
        json_free(lr);
    }
    if (sig2_truthy(reason)) {
        const char *rs = sig2_str(reason);
        char *error = NULL;
        if (rs && strcmp(rs, "download") == 0) error = sig2_pystr(detail);
        else if (rs && strcmp(rs, "missing") == 0) error = sig2_strdup("Image file not found");
        else if (rs && strcmp(rs, "oversize") == 0) {
            char *ds = sig2_pystr(detail);
            size_t n = strlen(ds) + 32;
            error = malloc(n);
            snprintf(error, n, "Image too large (%s bytes)", ds);
            free(ds);
        } else {
            /* {...}[reason] -> KeyError(repr(reason)) */
            char *rp = sig2_repr_ascii(rs ? rs : "");
            json_t *spec = sig2_err("KeyError", rp ? rp : "");
            free(rp);
            json_free(rr);
            return spec;
        }
        json_t *out = sig2_send_result(false, NULL, error, NULL, false);
        free(error);
        json_free(rr);
        return out;
    }
    json_t *fa = sig2_args();
    sig2_set_s(fa, "chat_id", chat_id);
    sig2_set_copy(fa, "file_path", file_path);
    sig2_set_copy(fa, "caption", caption);
    sig2_set_s(fa, "fail_error", "RPC send with attachment failed");
    json_free(rr);
    return sig2_call(ctx, "_send_file", fa);
}

/* PoP: sig2__send_attachment @ plugins/platforms/signal/adapter.py:_send_attachment */
static json_t *sig2__send_attachment(sig2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *file_path = json_get_str(args, "file_path", "");
    const char *media_label = json_get_str(args, "media_label", "");
    const json_t *caption = json_obj_get(args, "caption");
    json_t *sr = sig2_stop_typing_indicator_c(ctx, chat_id);
    if (sig2_is_raise(sr)) return sr;
    json_free(sr);
    json_t *sa = sig2_args();
    sig2_set_s(sa, "path", file_path);
    json_t *stat = sig2_call(ctx, "Path.stat", sa);
    if (sig2_is_raise(stat)) {
        if (strcmp(sig2_kind(stat), "FileNotFoundError") == 0) {
            size_t n = strlen(media_label) + strlen(file_path) + 32;
            char *error = malloc(n);
            snprintf(error, n, "%s file not found: %s", media_label, file_path);
            json_t *out = sig2_send_result(false, NULL, error, NULL, false);
            free(error);
            json_free(stat);
            return out;
        }
        return stat;   /* any other kind (incl. BaseException) propagates */
    }
    double file_size = json_get_num(stat, "st_size", 0);
    json_free(stat);
    if (file_size > 100.0 * 1024 * 1024) {
        char fs[64];
        if (file_size == (double)(long long)file_size) snprintf(fs, sizeof fs, "%.0f", file_size);
        else snprintf(fs, sizeof fs, "%.17g", file_size);
        size_t n = strlen(media_label) + strlen(fs) + 32;
        char *error = malloc(n);
        snprintf(error, n, "%s too large (%s bytes)", media_label, fs);
        json_t *out = sig2_send_result(false, NULL, error, NULL, false);
        free(error);
        return out;
    }
    char *low = sig2_lower(media_label);
    size_t fn = strlen(low) + 24;
    char *fail_error = malloc(fn);
    snprintf(fail_error, fn, "RPC send %s failed", low);
    free(low);
    json_t *fa = sig2_args();
    sig2_set_s(fa, "chat_id", chat_id);
    sig2_set_s(fa, "file_path", file_path);
    sig2_set_copy(fa, "caption", caption);
    json_set_hdr(fa, "fail_error", fail_error);
    free(fail_error);
    return sig2_call(ctx, "_send_file", fa);
}

/* PoP: sig2_send @ plugins/platforms/signal/adapter.py:send */
static json_t *sig2__send(sig2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const json_t *content = json_obj_get(args, "content");
    json_t *sr = sig2_stop_typing_indicator_c(ctx, chat_id);
    if (sig2_is_raise(sr)) return sr;
    json_free(sr);
    const char *cs = sig2_str(content);
    char *stripped = sig2_strip(cs ? cs : "");
    bool empty = !sig2_truthy(content) || stripped[0] == '\0';
    free(stripped);
    if (empty) return sig2_send_result(true, NULL, NULL, NULL, false);
    if (!json_has(ctx->state, "account")) return sig2_attr_err("account");
    const json_t *acct = sig2_self(ctx, "account");
    json_t *p0 = json_object();
    json_set(p0, "account", json_copy(acct));
    json_t *wa = sig2_args();
    sig2_set_s(wa, "chat_id", chat_id);
    sig2_set_b(wa, "resolve", true);
    json_set(wa, "params", p0);   /* adopt */
    json_t *base_params = sig2_call(ctx, "_with_target", wa);
    if (sig2_is_raise(base_params)) return base_params;
    json_t *ma = sig2_args();
    sig2_set_s(ma, "text", cs ? cs : "");
    json_t *mr = sig2_call(ctx, "markdown_to_signal", ma);
    if (sig2_is_raise(mr)) { json_free(base_params); return mr; }
    const json_t *plain = json_get(mr, 0);
    const json_t *styles = json_get(mr, 1);
    if (!json_has(ctx->state, "MAX_MESSAGE_LENGTH")) { json_free(mr); json_free(base_params); return sig2_attr_err("MAX_MESSAGE_LENGTH"); }
    double max_length = json_get_num(ctx->state, "MAX_MESSAGE_LENGTH", 0);
    json_t *chunks = sig2_split_signal_formatted_message_c(ctx, sig2_str(plain), styles, max_length);
    if (sig2_is_raise(chunks)) { json_free(mr); json_free(base_params); return chunks; }
    size_t nchunks = json_len(chunks);
    json_t *last_result = json_null();
    for (size_t i = 0; i < nchunks; i++) {
        const json_t *pair = json_get(chunks, i);
        const json_t *plain_text = json_get(pair, 0);
        const json_t *text_styles = json_get(pair, 1);
        json_t *params = json_copy(base_params);
        sig2_set_copy(params, "message", plain_text);
        size_t nstyles = (text_styles && text_styles->type == JSON_ARRAY) ? json_len(text_styles) : 0;
        if (nstyles == 1) {
            json_set(params, "textStyle", json_copy(json_get(text_styles, 0)));
        } else if (nstyles > 0) {
            json_set(params, "textStyles", json_copy(text_styles));
        }
        const char *pts = sig2_str(plain_text);
        json_t *la = json_array();
        json_append(la, json_number((double)(i + 1)));
        json_append(la, json_number((double)nchunks));
        json_append(la, json_number((double)sig2_cp_count(pts ? pts : "")));
        json_append(la, json_string(chat_id));
        json_t *lr = sig2_log(ctx, "info", "[Signal] Sending response chunk %d/%d (%d chars) to %s", la);
        if (sig2_is_raise(lr)) { json_free(params); json_free(last_result); json_free(chunks); json_free(mr); json_free(base_params); return lr; }
        json_free(lr);
        json_t *rpa = sig2_args();
        json_set(rpa, "params", params);   /* adopt */
        sig2_set_s(rpa, "fail_error", "RPC send failed");
        json_t *rsr = sig2_call(ctx, "_rpc_send", rpa);
        if (sig2_is_raise(rsr)) { json_free(last_result); json_free(chunks); json_free(mr); json_free(base_params); return rsr; }
        const json_t *err = json_get(rsr, 1);
        if (sig2_truthy(err)) {
            json_t *out = json_copy(err);
            json_free(rsr); json_free(last_result); json_free(chunks); json_free(mr); json_free(base_params);
            return out;
        }
        json_free(last_result);
        const json_t *res = json_get(rsr, 0);
        last_result = res ? json_copy(res) : json_null();
        json_free(rsr);
    }
    json_free(chunks);
    json_free(mr);
    json_free(base_params);
    return sig2_send_result(true, NULL, NULL, last_result, false);   /* adopt last_result */
}