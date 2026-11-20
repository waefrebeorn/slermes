/*
 * buzz_adapter_flow2.c — Buzz (Block Nostr relay) adapter FLOW remainder of
 * plugins/platforms/buzz/adapter.py (hermes-agent): the media-ref / attachment
 * cluster (_escape_unresolved_presentation_mention, _safe_attachment_filename,
 * _attachment_origin, _effective_port, _is_relay_media_url,
 * _find_relay_media_refs, _replace_media_refs, _parse_imeta_attachments,
 * _imeta_attachments), the identity / CLI-resolution cluster (hex_to_npub,
 * _resolve_cli_path, _configured_cli_path, _resolve_auth_tag,
 * _get_scoped_secret), the websocket URL / task leaves (_websocket_url,
 * _consume_ws_read_task), the send-result / connect-failure leaves
 * (_send_result, _connect_failed) and the async CLI plumbing flows
 * (_run_cli, _cli_json, send_reaction, edit_message, delete_message,
 * get_chat_info, _resolve_user_name).
 *
 * The 43 flow1 siblings stay CLAIMED in buzz_adapter_flow.c; every flow1
 * sibling called here is SEAMED (scripted) through the universal seam. The
 * flow2 siblings below call each other REAL (inline C), mirroring flow1's
 * mode table. Foreign collaborators (urllib urlsplit/urlunsplit, shutil.which,
 * Path ops, _nostr_auth.parse_auth_tag, _shared_scoped_secret, logger,
 * asyncio task ops, _exec_buzz subprocess) are SEAMED.
 *
 * Universal seam: every SEAMED call enters through ONE callback
 * ba2_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts. args
 * is ADOPTED into the trace event; the return is a NEW owned json_t (NULL ==
 * Python None; {"__raise__":cls,"message":str(exc)} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 * BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
 * CancelledError / BaseExceptionGroup) pass through every catch site
 * (contextlib.suppress(Exception), except ValueError, except (TypeError,
 * ValueError)); the full builtin hierarchy is tabulated below.
 *
 * Async methods (_run_cli / _cli_json / send_reaction / edit_message /
 * delete_message / get_chat_info / _resolve_user_name) are driven by the
 * oracle with asyncio.run over async recording stubs; the C port is synchronous
 * (the seams model the awaited results). Trace order is identical.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and ba2_-prefixed.
 *
 * Seam name table (name -> live call):
 *   _shared_scoped_secret   gateway.platforms._shared.get_scoped_secret(name, default, external_fallback=True)
 *   urlsplit                urllib.parse.urlsplit(url) -> parsed double (see below)
 *   urlunsplit              urllib.parse.urlunsplit((scheme,netloc,path,query,fragment))
 *   shutil.which            shutil.which("buzz")
 *   expanduser              Path(configured).expanduser() -> str
 *   is_file                 Path.is_file() -> bool
 *   Path.home               Path.home() -> str
 *   parse_auth_tag          _nostr_auth.parse_auth_tag(raw, "Buzz auth tag")
 *   task.cancelled          task.cancelled() -> bool
 *   task.exception          task.exception() -> value / raise
 *   logger.error / logger.debug   logger.<level>(fmt, *args) -> {fmt, args}
 *   _exec_buzz              module _exec_buzz(cli_path, args, relay_url=, private_key=, auth_tag=, input_text=)
 *   _scoped_platform_setting / _cli_error_message / _parse_send_receipt /
 *   _json_or / _parse_json_list / _resolve_private_key /
 *   _resolve_credentials_data / _configured_credentials_file
 *                           flow1-claimed module siblings (scripted)
 *   self._mark_seen / self._set_fatal_error
 *                           flow1-claimed / base adapter methods (scripted)
 *
 * Representation contract (shared with tests/oracle_buzz_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - The adapter self double is ctx.state (a JSON object of live attributes);
 *   methods read/write it in place. A DIRECT self._attr read with the key
 *   ABSENT raises AttributeError "'BuzzAdapter' object has no attribute 'X'".
 * - urlsplit seam results are parsed doubles: {scheme, netloc, path, query,
 *   fragment, hostname, username, password, port, port_error?}. port_error (a
 *   str) models the LAZY ValueError on SplitResult.port access ("Port could
 *   not be cast to integer value as 'abc'" / "Port out of range 0-65535");
 *   when present, port is null and every .port read raises ValueError. A
 *   urlsplit that raises at split time (invalid IPv6) is a scripted raise.
 *   The oracle materializes a ParsedDouble whose .port property raises; the
 *   generator computes the fields with the REAL urlsplit. _effective_port's
 *   args.parsed is the same double shape (fixture input, no seam).
 * - redact_path travels as a {path, name} dict (or null) straight through the
 *   _cli_error_message seam (no Path materialization: the callee is seamed).
 * - SendResult projects to its FULL dataclass field dict: {success,
 *   message_id, error, raw_response, retryable, retry_after,
 *   continuation_message_ids, error_kind} (defaults null/false/[]).
 * - tuple returns project to arrays; _find_relay_media_refs returns
 *   [urls, [[start,end,repl],...]]; _parse_imeta_attachments [attachments,
 *   rejected]; _parse_send_receipt seam ret [event_id, error]; _exec_buzz /
 *   _run_cli ret [rc, out, err].
 * - Integral floats == ints (libjson %.0f; oracle canon folds). str()-computed
 *   positions carry str/int/bool/None only.
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - ASCII scope: text / names / paths / URLs are ASCII; the only whitespace is
 *   the ASCII set {09,0a,0b,0c,0d,1c,1d,1e,1f,20}. str.strip() and re \s share
 *   that set on py3.14; int() strips the NARROWER {09,0a,0b,0c,0d,20};
 *   bytes.fromhex skips {09,0a,0b,0c,0d,20} BETWEEN byte pairs only (not
 *   between the two digits of a pair). The ZWSP separator U+200B and the
 *   em-dash U+2014 in fixed literals are the only non-ASCII bytes.
 * - Paths are pre-normalized POSIX (no '.'/'..' components, no duplicate or
 *   trailing slashes, no leading '//'); PurePosixPath str() is identity on
 *   this corpus. Path.home results are absolute.
 * - json.loads text (delete_message) is json.dumps-generated canonical JSON or
 *   an obviously-invalid string; leading-zero / trailing-dot / bare-exponent
 *   numbers, NaN/Infinity, lone surrogates, duplicate keys and raw control
 *   chars are out of domain (libjson json_parse matches json.loads here).
 * - parse_auth_tag seam results are ASCII str/int/bool/null/dict/list (NO
 *   floats: json.dumps ensure_ascii + repr(float) vs libjson %.17g diverge).
 *   json.dumps(..., separators=(",",":")) == libjson json_serialize on ASCII.
 * - _resolve_credentials_data / _json_or (edit/get_chat_info) seam results are
 *   dicts; _parse_json_list results are lists of dicts; _exec_buzz / _run_cli
 *   results are 3-arrays [int, str, str]; _parse_send_receipt results are
 *   2-arrays. str()-computed scalars carry str/int/bool/None only.
 * - int()-coerced size strings are clean ASCII decimals (optional sign,
 *   narrower-ws padding, no underscores) or printable-ASCII invalid tokens
 *   without quotes/backslashes.
 * - bytes.fromhex inputs are str; invalid forms are pure-hex odd-length,
 *   non-hex printable ASCII, or ASCII-ws-separated pairs.
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
typedef struct ba2_ctx {
    json_t *state;      /* owned self double; mutated in place; emitted as out.state.self */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ba2_ctx_t;

/* ── basic helpers ── */
static bool ba2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ba2_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
/* Full builtin exception hierarchy (py3.14). BaseException-only kinds pass
 * through every except-Exception / suppress(Exception) site; everything else
 * below Exception is swallowed by those catches. Unknown scripted kinds are
 * Exception subclasses (the oracle's make_exc builds type(kind,(Exception,))).
 *
 * BaseException
 *   SystemExit, KeyboardInterrupt, GeneratorExit, BaseExceptionGroup
 *   asyncio.CancelledError
 *   Exception
 *     StopIteration, StopAsyncIteration
 *     ArithmeticError: FloatingPointError, OverflowError, ZeroDivisionError
 *     AssertionError, AttributeError, BufferError, EOFError
 *     ImportError: ModuleNotFoundError
 *     LookupError: IndexError, KeyError
 *     MemoryError
 *     NameError: UnboundLocalError
 *     OSError: BlockingIOError, ChildProcessError,
 *       ConnectionError: BrokenPipeError, ConnectionAbortedError,
 *         ConnectionRefusedError, ConnectionResetError
 *       FileExistsError, FileNotFoundError, InterruptedError,
 *       IsADirectoryError, NotADirectoryError, PermissionError,
 *       ProcessLookupError, TimeoutError
 *     ReferenceError
 *     RuntimeError: NotImplementedError, RecursionError
 *     SyntaxError: IndentationError: TabError
 *     SystemError
 *     TypeError
 *     ValueError: UnicodeError: UnicodeDecodeError, UnicodeEncodeError,
 *       UnicodeTranslateError
 *       json.JSONDecodeError
 *     Warning: DeprecationWarning, PendingDeprecationWarning, RuntimeWarning,
 *       SyntaxWarning, UserWarning, FutureWarning, ImportWarning,
 *       UnicodeWarning, BytesWarning, EncodingWarning, ResourceWarning
 *     ExceptionGroup
 */
static const char *BA2_BASE_KINDS[] = {
    "BaseException", "SystemExit", "KeyboardInterrupt", "GeneratorExit",
    "CancelledError", "BaseExceptionGroup",
};
static bool ba2_is_base_exc(const char *kind) {
    for (size_t i = 0; i < sizeof(BA2_BASE_KINDS) / sizeof(BA2_BASE_KINDS[0]); i++)
        if (strcmp(kind, BA2_BASE_KINDS[i]) == 0) return true;
    return false;
}
/* ValueError family (except ValueError catches subclasses). */
static bool ba2_is_valueerror(const char *kind) {
    return strcmp(kind, "ValueError") == 0 || strcmp(kind, "UnicodeError") == 0
        || strcmp(kind, "UnicodeDecodeError") == 0 || strcmp(kind, "UnicodeEncodeError") == 0
        || strcmp(kind, "UnicodeTranslateError") == 0 || strcmp(kind, "JSONDecodeError") == 0;
}
static bool ba2_is_typeerror(const char *kind) { return strcmp(kind, "TypeError") == 0; }
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool ba2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ba2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ba2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool ba2_str_eq(const json_t *v, const char *s) {
    const char *t = ba2_str(v);
    return t && s && strcmp(t, s) == 0;
}
static const char *ba2_arr_str(const json_t *arr, size_t i, const char *def) {
    const json_t *v = json_get(arr, i);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static double ba2_arr_num(const json_t *arr, size_t i, double def) {
    const json_t *v = json_get(arr, i);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}
static json_t *ba2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ba2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return ba2_err(type, msg);
}
static json_t *ba2_attr_err(const char *key) {
    char msg[160];
    snprintf(msg, sizeof msg, "'BuzzAdapter' object has no attribute '%s'", key);
    return ba2_err("AttributeError", msg);
}
/* Python type(x).__name__ for AttributeError messages (integral number==int). */
static const char *ba2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static json_t *ba2_no_attr(const json_t *v, const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", ba2_type_name(v), attr);
    return ba2_err("AttributeError", msg);
}
/* The one universal seam. args ADOPTED; return fresh owned node. */
static json_t *ba2_call(ba2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ba2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ba2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ba2_make_exc(raise);
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
static json_t *ba2_args(void) { return json_object(); }
static void ba2_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void ba2_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void ba2_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void ba2_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void ba2_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── string helpers ── */
static char *ba2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* str.strip() / re \s ASCII whitespace set (py3.14 includes 1c-1f). */
static bool ba2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'
        || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}
/* int() strips a NARROWER set (NOT 1c-1f). */
static bool ba2_is_int_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static char *ba2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && ba2_is_ws((unsigned char)s[a])) a++;
    while (b > a && ba2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}
/* rstrip of a literal char set (e.g. " ." or "."). */
static char *ba2_rstrip_set(const char *s, const char *set) {
    size_t n = strlen(s);
    while (n > 0 && strchr(set, s[n - 1]) != NULL) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static char *ba2_lower(const char *s) {
    char *o = ba2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* Python str() projection for scalar nodes (containers out of domain). */
static char *ba2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ba2_strdup("None");
    if (v->type == JSON_BOOL) return ba2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return ba2_strdup(buf);
    }
    if (v->type == JSON_STRING) return ba2_strdup(v->str_val ? v->str_val : "");
    return ba2_strdup("");
}
/* str(x or "") — falsy x collapses to "" before str(). */
static char *ba2_pystr_or_empty(const json_t *v) {
    if (!ba2_truthy(v)) return ba2_strdup("");
    return ba2_pystr(v);
}
static bool ba2_hex64(const char *s) {
    if (!s || strlen(s) != 64) return false;
    for (const char *p = s; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) return false;
    return true;
}
static bool ba2_hex_digit(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
/* literal case-insensitive match of needle at hay[pos]. */
static bool ba2_ieq_at(const char *hay, size_t pos, const char *needle) {
    size_t n = strlen(needle);
    if (strlen(hay + pos) < n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)hay[pos + i]) != tolower((unsigned char)needle[i])) return false;
    return true;
}
/* codepoint-aware prefix slice (ASCII corpus: byte slice). */
static char *ba2_slice(const char *s, size_t cap_cp) {
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
/* PurePosixPath join on the pre-normalized corpus: home / "bin/buzz". */
static char *ba2_path_join(const char *home, const char *rest) {
    size_t hl = strlen(home);
    bool sep = (hl > 0 && home[hl - 1] != '/');
    char *o = malloc(hl + strlen(rest) + 2);
    if (o) sprintf(o, "%s%s%s", home, sep ? "/" : "", rest);
    return o;
}

/* ── int() coercion (size / returncode positions) ── */
static bool ba2_int_of(const json_t *v, long long *out, char **errmsg) {
    *errmsg = NULL;
    if (!v || v->type == JSON_NULL) { *out = 0; return true; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return true; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && ba2_is_int_ws((unsigned char)s[a])) a++;
        while (b > a && ba2_is_int_ws((unsigned char)s[b - 1])) b--;
        size_t i = a;
        bool neg = false;
        if (i < b && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
        if (i >= b) goto bad;
        long long val = 0;
        for (; i < b; i++) {
            if (s[i] < '0' || s[i] > '9') goto bad;
            val = val * 10 + (s[i] - '0');
        }
        *out = neg ? -val : val;
        return true;
    bad: {
            char *msg = malloc(n + 80);
            snprintf(msg, n + 80, "invalid literal for int() with base 10: '%s'", s);
            *errmsg = msg;
            return false;
        }
    }
    {
        char *msg = malloc(128);
        snprintf(msg, 128, "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 ba2_type_name(v));
        *errmsg = msg;
        return false;
    }
}

/* ── libjson parse wrapper (json.loads on the documented corpus) ── */
static json_t *ba2_json_loads(const char *text) {
    if (!text) return NULL;
    return json_parse(text, NULL);
}

/* self double attribute reader (borrowed). */
static const json_t *ba2_self(const ba2_ctx_t *ctx, const char *key) {
    return ctx->state ? json_obj_get(ctx->state, key) : NULL;
}
/* direct self._attr read: absent -> AttributeError. */
static const json_t *ba2_self_req(ba2_ctx_t *ctx, const char *key, json_t **err) {
    if (!json_has(ctx->state, key)) { *err = ba2_attr_err(key); return NULL; }
    *err = NULL;
    return ba2_self(ctx, key);
}

/* ── parsed (urlsplit) double helpers ── */
/* .port access: port_error present -> ValueError raise; else the port value. */
static json_t *ba2_port_access(const json_t *parsed) {
    const json_t *pe = parsed ? json_obj_get(parsed, "port_error") : NULL;
    if (pe && pe->type == JSON_STRING)
        return ba2_err("ValueError", pe->str_val ? pe->str_val : "");
    const json_t *p = parsed ? json_obj_get(parsed, "port") : NULL;
    return p ? json_copy(p) : json_null();
}
/* _effective_port: try .port (ValueError -> None), else scheme default map. */
static json_t *ba2_effective_port(const json_t *parsed) {
    json_t *port = ba2_port_access(parsed);
    if (ba2_is_raise(port)) { json_free(port); return json_null(); }
    if (!ba2_is_none(port)) return port;
    json_free(port);
    const char *scheme = ba2_str(parsed ? json_obj_get(parsed, "scheme") : NULL);
    if (scheme) {
        if (strcmp(scheme, "https") == 0 || strcmp(scheme, "wss") == 0) return json_number(443);
        if (strcmp(scheme, "http") == 0 || strcmp(scheme, "ws") == 0) return json_number(80);
    }
    return json_null();
}
static json_t *ba2_urlsplit(ba2_ctx_t *ctx, const char *url) {
    json_t *a = ba2_args();
    ba2_set_s(a, "url", url);
    return ba2_call(ctx, "urlsplit", a);
}
/* _MEDIA_PATH_RE.fullmatch: ^/media/[0-9a-f]{64}(\.[a-z0-9]{1,10})?/?$ (I). */
static bool ba2_media_path_fullmatch(const char *path) {
    if (!path) return false;
    size_t n = strlen(path);
    if (!ba2_ieq_at(path, 0, "/media/")) return false;
    size_t h = 7;
    if (n < h + 64) return false;
    for (size_t i = 0; i < 64; i++)
        if (!ba2_hex_digit((unsigned char)path[h + i])) return false;
    size_t e = h + 64;
    if (e < n && path[e] == '.') {
        size_t j = e + 1, cnt = 0;
        while (j < n && cnt < 10 &&
               (isalnum((unsigned char)path[j]))) { j++; cnt++; }
        if (cnt == 0) return false;
        e = j;
    }
    if (e < n && path[e] == '/') e++;
    return e == n;
}

/* ── media URL scanner (shared by markdown + bare) ──
 * Pattern: https?://[^\s<>\[\]()]+/media/[0-9a-f]{64}(?:\.[a-z0-9]{1,10})?(?:\?[^\s<>\[\]()]*)?
 * IGNORECASE. Greedy backtracking order: longest host run k first, then ext
 * present (longest..1) before absent, then query present (maximal) before
 * absent. tail_ok(url_end) decides the markdown continuation; for the bare
 * pattern it always succeeds. Returns the URL end (and match end via tail). */
static bool ba2_url_char(unsigned char c) {
    return !ba2_is_ws(c) && c != '<' && c != '>' && c != '[' && c != ']' && c != '(' && c != ')';
}
static bool ba2_alnum(unsigned char c) { return isalnum(c) != 0; }
typedef bool (*ba2_tail_fn)(const char *text, size_t url_end, size_t *match_end, void *ud);
static bool ba2_tail_bare(const char *text, size_t url_end, size_t *match_end, void *ud) {
    (void)text; (void)ud;
    *match_end = url_end;
    return true;
}
static bool ba2_match_media_url(const char *text, size_t start, ba2_tail_fn tail, void *ud,
                                size_t *url_end_out, size_t *match_end_out) {
    size_t n = strlen(text);
    /* scheme: http + optional s + ://  (greedy s first, backtrack to no-s) */
    for (int with_s = 1; with_s >= 0; with_s--) {
        size_t p = start;
        if (!ba2_ieq_at(text, p, "http")) continue;
        p += 4;
        if (with_s) {
            if (p >= n || (text[p] != 's' && text[p] != 'S')) continue;
            p += 1;
        }
        if (!ba2_ieq_at(text, p, "://")) continue;
        size_t u = p + 3;
        /* maximal run of url chars */
        size_t r = u;
        while (r < n && ba2_url_char((unsigned char)text[r])) r++;
        if (r == u) continue; /* + needs >= 1 */
        /* backtrack k from r down to u+1 looking for /media/ + hex64 */
        for (size_t k = r; k >= u + 1; k--) {
            if (!ba2_ieq_at(text, k, "/media/")) continue;
            size_t h = k + 7;
            if (n < h + 64) continue;
            bool hexok = true;
            for (size_t i = 0; i < 64; i++)
                if (!ba2_hex_digit((unsigned char)text[h + i])) { hexok = false; break; }
            if (!hexok) continue;
            size_t e = h + 64;
            /* ext choices: present lengths L..1 (if '.'), then absent */
            size_t ext_max = 0;
            if (e < n && text[e] == '.') {
                size_t j = e + 1;
                while (j < n && ext_max < 10 && ba2_alnum((unsigned char)text[j])) { j++; ext_max++; }
            }
            /* ext choices in backtrack order: present lengths ext_max..1
             * (only when '.'), then absent (L=0). */
            size_t q_cands[12];
            size_t qn = 0;
            if (ext_max > 0)
                for (size_t L = ext_max; L >= 1; L--) q_cands[qn++] = e + 1 + L;
            q_cands[qn++] = e;
            for (size_t ci = 0; ci < qn; ci++) {
                size_t q = q_cands[ci];
                /* query choices: present (maximal) then absent */
                for (int q_present = 1; q_present >= 0; q_present--) {
                    size_t qe = q;
                    if (q_present) {
                        if (q >= n || text[q] != '?') continue;
                        qe = q + 1;
                        while (qe < n && ba2_url_char((unsigned char)text[qe])) qe++;
                    }
                    size_t me = 0;
                    if (tail(text, qe, &me, ud)) {
                        if (url_end_out) *url_end_out = qe;
                        if (match_end_out) *match_end_out = me;
                        return true;
                    }
                }
            }
        }
        return false; /* scheme matched at start; no k succeeded */
    }
    return false;
}
/* markdown tail: (?:\s+["'][^"']*["'])?\s*\)  — greedy, only maximal runs
 * can succeed (shortening leaves a char the next literal cannot match). */
static bool ba2_markdown_tail(const char *text, size_t url_end, size_t *match_end, void *ud) {
    (void)ud;
    size_t n = strlen(text);
    /* title present: \s+ (>=1, maximal) quote [^"']* (maximal) quote \s* (maximal) ')' */
    size_t w = url_end;
    while (w < n && ba2_is_ws((unsigned char)text[w])) w++;
    if (w > url_end && w < n && (text[w] == '"' || text[w] == '\'')) {
        size_t t = w + 1;
        while (t < n && text[t] != '"' && text[t] != '\'') t++;
        if (t < n) { /* closing quote (either kind) */
            size_t w2 = t + 1;
            while (w2 < n && ba2_is_ws((unsigned char)text[w2])) w2++;
            if (w2 < n && text[w2] == ')') { *match_end = w2 + 1; return true; }
        }
    }
    /* title absent: \s* (maximal) ')' */
    if (w < n && text[w] == ')') { *match_end = w + 1; return true; }
    return false;
}
/* _MARKDOWN_MEDIA_RE full match at start: !\[alt\]\(\s* URL tail */
static bool ba2_match_markdown(const char *text, size_t start, size_t *match_end,
                               size_t *url_start, size_t *url_end, size_t *alt_start, size_t *alt_end) {
    size_t n = strlen(text);
    if (start + 1 >= n || text[start] != '!' || text[start + 1] != '[') return false;
    size_t a = start + 2;
    size_t ae = a;
    while (ae < n && text[ae] != ']') ae++;
    if (ae >= n) return false;
    if (ae + 1 >= n || text[ae + 1] != '(') return false;
    size_t p = ae + 2;
    while (p < n && ba2_is_ws((unsigned char)text[p])) p++;
    size_t ue = 0, me = 0;
    if (!ba2_match_media_url(text, p, ba2_markdown_tail, NULL, &ue, &me)) return false;
    *match_end = me;
    if (url_start) *url_start = p;
    if (url_end) *url_end = ue;
    if (alt_start) *alt_start = a;
    if (alt_end) *alt_end = ae;
    return true;
}

/* ── bech32 (BIP-173) npub encode (gateway/authz_mixin helpers, transcribed) ── */
static const char BA2_BECH32_CHARSET[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
static const uint32_t BA2_BECH32_GEN[5] = {0x3B6A57B2u, 0x26508E6Du, 0x1EA119FAu, 0x3D4233DDu, 0x2A1462B3u};
static uint32_t ba2_bech32_polymod(const int *values, size_t n) {
    uint32_t chk = 1;
    for (size_t i = 0; i < n; i++) {
        uint32_t top = chk >> 25;
        chk = ((chk & 0x1FFFFFFu) << 5) ^ (uint32_t)values[i];
        for (int j = 0; j < 5; j++)
            if ((top >> j) & 1) chk ^= BA2_BECH32_GEN[j];
    }
    return chk;
}
/* bytes.fromhex on the documented ASCII corpus; exact ValueError messages. */
static unsigned char *ba2_fromhex(const char *s, size_t *out_len, char **errmsg) {
    *errmsg = NULL; *out_len = 0;
    size_t n = strlen(s);
    unsigned char *o = malloc(n / 2 + 1);
    if (!o) return NULL;
    size_t i = 0, len = 0;
    for (;;) {
        while (i < n && ba2_is_int_ws((unsigned char)s[i])) i++;
        if (i >= n) break;
        if (!ba2_hex_digit((unsigned char)s[i])) goto nonhex;
        unsigned char hi = (unsigned char)s[i];
        i++;
        if (i >= n) goto odd;
        if (!ba2_hex_digit((unsigned char)s[i])) goto nonhex;
        unsigned char lo = (unsigned char)s[i];
        i++;
        int hv = (hi <= '9') ? hi - '0' : (tolower(hi) - 'a' + 10);
        int lv = (lo <= '9') ? lo - '0' : (tolower(lo) - 'a' + 10);
        o[len++] = (unsigned char)((hv << 4) | lv);
    }
    *out_len = len;
    return o;
nonhex: {
        char *msg = malloc(n + 96);
        snprintf(msg, n + 96, "non-hexadecimal number found in fromhex() arg at position %zu", i);
        *errmsg = msg; free(o); return NULL;
    }
odd:
    *errmsg = ba2_strdup("fromhex() arg must contain an even number of hexadecimal digits");
    free(o);
    return NULL;
}

/* ── SendResult full dataclass projection ── */
static json_t *ba2_send_result(bool success, const json_t *message_id, const json_t *error,
                               const json_t *raw_response, bool retryable) {
    json_t *r = json_object();
    ba2_set_b(r, "success", success);
    ba2_set_copy(r, "message_id", message_id);
    ba2_set_copy(r, "error", error);
    ba2_set_copy(r, "raw_response", raw_response);
    ba2_set_b(r, "retryable", retryable);
    ba2_set_null(r, "retry_after");
    json_set(r, "continuation_message_ids", json_array());
    ba2_set_null(r, "error_kind");
    return r;
}

/* ================================================================
 * Group A — media-ref / attachment leaves
 * ================================================================ */

/* _UNRESOLVED_MENTION_ERROR_RE.search: mention '@(?P<name>[^']+)' does not
 * match a current channel member  (case-sensitive, leftmost). */
static char *ba2_unresolved_name(const char *error) {
    const char *lit1 = "mention '@";
    const char *lit2 = "' does not match a current channel member";
    size_t n = strlen(error), l1 = strlen(lit1), l2 = strlen(lit2);
    for (size_t s = 0; s < n; s++) {
        if (strncmp(error + s, lit1, l1) != 0) continue;
        size_t a = s + l1, b = a;
        while (b < n && error[b] != '\'') b++;
        if (b >= n || b == a) continue; /* [^']+ needs >=1 and a closing quote */
        if (strncmp(error + b, lit2, l2) != 0) continue;
        char *name = malloc(b - a + 1);
        if (name) { memcpy(name, error + a, b - a); name[b - a] = '\0'; }
        return name;
    }
    return NULL;
}
/* token char set for the escape lookahead: [A-Za-z0-9._-] */
static bool ba2_token_char(unsigned char c) {
    return ba2_alnum(c) || c == '.' || c == '_' || c == '-';
}
/* PoP: ba2__escape_unresolved_presentation_mention @ plugins/platforms/buzz/adapter.py:_escape_unresolved_presentation_mention */
static json_t *ba2__escape_unresolved_presentation_mention(ba2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *content = json_get_str(args, "content", "");
    const json_t *errv = json_obj_get(args, "error");
    char *err = ba2_pystr_or_empty(errv); /* error or "" */
    char *name = ba2_unresolved_name(err);
    free(err);
    if (!name || !name[0]) { free(name); return json_null(); }
    size_t nl = strlen(name), n = strlen(content);
    size_t cap = n * 4 + 64, len = 0;
    char *o = malloc(cap);
    if (!o) { free(name); return json_null(); }
    size_t count = 0, pos = 0;
    while (pos < n) {
        bool before_ok = (pos == 0) || ba2_is_ws((unsigned char)content[pos - 1]);
        if (before_ok && content[pos] == '@' && ba2_ieq_at(content, pos + 1, name)) {
            size_t e = pos + 1 + nl;
            unsigned char after = (unsigned char)content[e];
            bool after_ok = (after == '\0') || !ba2_token_char(after);
            if (after_ok) {
                /* "@" + ZWSP + original-case name */
                while (len + nl + 8 > cap) { cap *= 2; o = realloc(o, cap); }
                o[len++] = '@';
                o[len++] = (char)0xE2; o[len++] = (char)0x80; o[len++] = (char)0x8B;
                memcpy(o + len, content + pos + 1, nl); len += nl;
                count++;
                pos = e;
                continue;
            }
        }
        if (len + 2 > cap) { cap *= 2; o = realloc(o, cap); }
        o[len++] = content[pos++];
    }
    o[len] = '\0';
    free(name);
    if (!count) { free(o); return json_null(); }
    json_t *r = json_string(o);
    free(o);
    return r;
}

/* Path(name).suffix (py3.14): name.lstrip('.') then rfind('.'); if found,
 * the suffix is name[dot:] (trailing-dot counts; leading dots stripped). */
static char *ba2_path_suffix(const char *name) {
    size_t n = strlen(name);
    size_t s = 0;
    while (s < n && name[s] == '.') s++;
    size_t i = n;
    while (i > s && name[i - 1] != '.') i--;
    if (i == s) return ba2_strdup("");
    return ba2_strdup(name + (i - 1));
}
/* PoP: ba2__safe_attachment_filename @ plugins/platforms/buzz/adapter.py:_safe_attachment_filename */
static json_t *ba2__safe_attachment_filename(ba2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *base = ba2_pystr_or_empty(json_obj_get(args, "value"));
    /* replace "\\" with "/" then rsplit("/",1)[-1] */
    for (char *p = base; *p; p++) if (*p == '\\') *p = '/';
    char *slash = strrchr(base, '/');
    char *name0 = ba2_strdup(slash ? slash + 1 : base);
    free(base);
    /* filter ord(c) >= 32 and c != 0x7f, then strip */
    size_t n = strlen(name0), len = 0;
    char *filt = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name0[i];
        if (c >= 32 && c != 0x7f) filt[len++] = (char)c;
    }
    filt[len] = '\0';
    free(name0);
    char *name = ba2_strip(filt);
    free(filt);
    if (strcmp(name, "") == 0 || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        free(name);
        return json_string("attachment.bin");
    }
    char *suffix = ba2_path_suffix(name);
    if (strlen(suffix) > 20) { free(suffix); suffix = ba2_strdup(""); }
    size_t sl = strlen(suffix), nl = strlen(name);
    char *stem = ba2_strdup(suffix[0] ? name : name);
    if (sl) stem[nl - sl] = '\0';
    size_t budget = 120 - sl;
    char *cut = ba2_slice(stem, budget); /* ASCII: byte slice; decode ignore no-op */
    char *safe_stem = ba2_rstrip_set(cut, " .");
    free(cut); free(stem);
    const char *use = safe_stem[0] ? safe_stem : "attachment";
    char *o = malloc(strlen(use) + sl + 1);
    if (o) sprintf(o, "%s%s", use, suffix);
    json_t *r = json_string(o ? o : "");
    free(o); free(safe_stem); free(suffix); free(name);
    return r;
}

/* PoP: ba2__attachment_origin @ plugins/platforms/buzz/adapter.py:_attachment_origin */
static json_t *ba2__attachment_origin(ba2_ctx_t *ctx, const json_t *args) {
    char *raw0 = ba2_pystr_or_empty(json_obj_get(args, "value"));
    char *raw = ba2_strip(raw0);
    free(raw0);
    if (!raw[0]) { free(raw); return json_null(); }
    char *url = strstr(raw, "://") ? ba2_strdup(raw) : malloc(strlen(raw) + 3);
    if (!strstr(raw, "://")) sprintf(url, "//%s", raw);
    json_t *parsed = ba2_urlsplit(ctx, url);
    free(url); free(raw);
    if (ba2_is_raise(parsed)) {
        /* except ValueError -> None; other kinds propagate */
        if (ba2_is_valueerror(ba2_kind(parsed))) { json_free(parsed); return json_null(); }
        return parsed;
    }
    /* host = (hostname or "").lower().rstrip(".") */
    char *host0 = ba2_pystr_or_empty(json_obj_get(parsed, "hostname"));
    char *hostl = ba2_lower(host0); free(host0);
    char *host = ba2_rstrip_set(hostl, "."); free(hostl);
    /* port = parsed.port or 443  (ValueError -> None via the try) */
    json_t *port = ba2_port_access(parsed);
    if (ba2_is_raise(port)) {
        json_free(parsed); free(host);
        if (ba2_is_valueerror(ba2_kind(port))) { json_free(port); return json_null(); }
        return port;
    }
    long long pv = ba2_truthy(port) ? (long long)port->num_val : 443;
    json_free(port);
    const json_t *scheme = json_obj_get(parsed, "scheme");
    bool scheme_bad = ba2_truthy(scheme) && !ba2_str_eq(scheme, "https") && !ba2_str_eq(scheme, "wss");
    json_free(parsed);
    if (scheme_bad || !host[0]) { free(host); return json_null(); }
    json_t *r = json_array();
    json_append(r, json_string(host));
    json_append(r, json_number((double)pv));
    free(host);
    return r;
}

/* PoP: ba2__effective_port @ plugins/platforms/buzz/adapter.py:_effective_port */
static json_t *ba2__effective_port(ba2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ba2_effective_port(json_obj_get(args, "parsed"));
}

/* _is_relay_media_url core (fires the two urlsplit seams). */
static json_t *ba2_is_relay_media_url_c(ba2_ctx_t *ctx, const char *url, const char *relay_url) {
    json_t *candidate = ba2_urlsplit(ctx, url);
    if (ba2_is_raise(candidate)) return candidate;
    json_t *relay = ba2_urlsplit(ctx, relay_url);
    if (ba2_is_raise(relay)) { json_free(candidate); return relay; }
    bool ok = false;
    const json_t *cs = json_obj_get(candidate, "scheme");
    if (ba2_str_eq(cs, "http") || ba2_str_eq(cs, "https")) {
        const json_t *ch = json_obj_get(candidate, "hostname");
        const json_t *rh = json_obj_get(relay, "hostname");
        if (ba2_truthy(ch) && ba2_truthy(rh)) {
            char *cl = ba2_lower(ba2_str(ch) ? ba2_str(ch) : "");
            char *rl = ba2_lower(ba2_str(rh) ? ba2_str(rh) : "");
            if (strcmp(cl, rl) == 0) {
                json_t *cp = ba2_effective_port(candidate);
                json_t *rp = ba2_effective_port(relay);
                bool eq = (ba2_is_none(cp) && ba2_is_none(rp)) ||
                    (!ba2_is_none(cp) && !ba2_is_none(rp) &&
                     cp->type == JSON_NUMBER && rp->type == JSON_NUMBER &&
                     cp->num_val == rp->num_val);
                json_free(cp); json_free(rp);
                if (eq) {
                    const char *path = ba2_str(json_obj_get(candidate, "path"));
                    ok = ba2_media_path_fullmatch(path ? path : "");
                }
            }
            free(cl); free(rl);
        }
    }
    json_free(candidate); json_free(relay);
    return json_bool(ok);
}
/* PoP: ba2__is_relay_media_url @ plugins/platforms/buzz/adapter.py:_is_relay_media_url */
static json_t *ba2__is_relay_media_url(ba2_ctx_t *ctx, const json_t *args) {
    return ba2_is_relay_media_url_c(ctx, json_get_str(args, "url", ""), json_get_str(args, "relay_url", ""));
}

/* PoP: ba2__find_relay_media_refs @ plugins/platforms/buzz/adapter.py:_find_relay_media_refs */
static json_t *ba2__find_relay_media_refs(ba2_ctx_t *ctx, const json_t *args) {
    const char *text = json_get_str(args, "text", "");
    const char *relay_url = json_get_str(args, "relay_url", "");
    size_t n = strlen(text);
    json_t *urls = json_array();
    json_t *repls = json_array();
    /* markdown spans (start,end) for the bare containment skip */
    size_t spans_cap = 16, spans_n = 0;
    size_t (*spans)[2] = malloc(spans_cap * sizeof *spans);
    size_t pos = 0;
    while (pos < n) {
        size_t me = 0, us = 0, ue = 0, as = 0, ae = 0;
        if (ba2_match_markdown(text, pos, &me, &us, &ue, &as, &ae)) {
            char *url = ba2_slice(text + us, ue - us);
            json_t *isr = ba2_is_relay_media_url_c(ctx, url, relay_url);
            if (ba2_is_raise(isr)) {
                free(url); json_free(urls); json_free(repls); free(spans);
                return isr;
            }
            bool yes = isr && isr->type == JSON_BOOL && isr->bool_val;
            json_free(isr);
            if (yes) {
                if (spans_n == spans_cap) { spans_cap *= 2; spans = realloc(spans, spans_cap * sizeof *spans); }
                spans[spans_n][0] = pos; spans[spans_n][1] = me; spans_n++;
                char *alt = ba2_slice(text + as, ae - as);
                char *alts = ba2_strip(alt); free(alt);
                json_t *r = json_array();
                json_append(r, json_number((double)pos));
                json_append(r, json_number((double)me));
                json_append(r, json_string(alts));
                free(alts);
                json_append(repls, r);
                bool dup = false;
                for (size_t i = 0; i < json_len(urls); i++)
                    if (strcmp(ba2_str(json_get(urls, i)) ? ba2_str(json_get(urls, i)) : "", url) == 0) { dup = true; break; }
                if (!dup) json_append(urls, json_string(url));
            }
            free(url);
            pos = me;
        } else {
            pos++;
        }
    }
    /* bare finditer */
    pos = 0;
    while (pos < n) {
        size_t ue = 0, me = 0;
        if (ba2_match_media_url(text, pos, ba2_tail_bare, NULL, &ue, &me)) {
            bool contained = false;
            for (size_t i = 0; i < spans_n; i++)
                if (spans[i][0] <= pos && me <= spans[i][1]) { contained = true; break; }
            if (!contained) {
                char *url = ba2_slice(text + pos, ue - pos);
                json_t *isr = ba2_is_relay_media_url_c(ctx, url, relay_url);
                if (ba2_is_raise(isr)) {
                    free(url); json_free(urls); json_free(repls); free(spans);
                    return isr;
                }
                bool yes = isr && isr->type == JSON_BOOL && isr->bool_val;
                json_free(isr);
                if (yes) {
                    json_t *r = json_array();
                    json_append(r, json_number((double)pos));
                    json_append(r, json_number((double)me));
                    json_append(r, json_string(""));
                    json_append(repls, r);
                    bool dup = false;
                    for (size_t i = 0; i < json_len(urls); i++)
                        if (strcmp(ba2_str(json_get(urls, i)) ? ba2_str(json_get(urls, i)) : "", url) == 0) { dup = true; break; }
                    if (!dup) json_append(urls, json_string(url));
                }
                free(url);
            }
            pos = me;
        } else {
            pos++;
        }
    }
    free(spans);
    json_t *out = json_array();
    json_append(out, urls);
    json_append(out, repls);
    return out;
}

/* stable sort of replacements descending by (start, end, repl). */
typedef struct { long long start, end; char *repl; } ba2_repl_t;
static void ba2_sort_repls(ba2_repl_t *r, size_t n) {
    for (size_t i = 1; i < n; i++) {
        ba2_repl_t key = r[i];
        size_t j = i;
        while (j > 0) {
            ba2_repl_t *p = &r[j - 1];
            bool less = (p->start < key.start) ||
                (p->start == key.start && p->end < key.end) ||
                (p->start == key.start && p->end == key.end && strcmp(p->repl, key.repl) < 0);
            if (!less) break;
            r[j] = r[j - 1]; j--;
        }
        r[j] = key;
    }
}
/* PoP: ba2__replace_media_refs @ plugins/platforms/buzz/adapter.py:_replace_media_refs */
static json_t *ba2__replace_media_refs(ba2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *text = ba2_strdup(json_get_str(args, "text", ""));
    const json_t *repls = json_obj_get(args, "replacements");
    size_t rn = (repls && repls->type == JSON_ARRAY) ? json_len(repls) : 0;
    ba2_repl_t *rs = malloc((rn ? rn : 1) * sizeof *rs);
    for (size_t i = 0; i < rn; i++) {
        const json_t *r = json_get(repls, i);
        rs[i].start = (long long)ba2_arr_num(r, 0, 0);
        rs[i].end = (long long)ba2_arr_num(r, 1, 0);
        rs[i].repl = ba2_strdup(ba2_arr_str(r, 2, ""));
    }
    ba2_sort_repls(rs, rn);
    for (size_t i = 0; i < rn; i++) {
        size_t tl = strlen(text);
        long long s = rs[i].start, e = rs[i].end;
        if (s < 0) s = 0; if ((size_t)s > tl) s = (long long)tl;
        if (e < 0) e = 0; if ((size_t)e > tl) e = (long long)tl;
        size_t rl = strlen(rs[i].repl);
        char *o = malloc(tl + rl + 1);
        if (o) {
            memcpy(o, text, (size_t)s);
            memcpy(o + s, rs[i].repl, rl);
            memcpy(o + s + rl, text + e, tl - (size_t)e);
            o[s + rl + tl - (size_t)e] = '\0';
        }
        free(text); text = o;
        free(rs[i].repl);
    }
    free(rs);
    /* re.sub(r"[ \t]+\n", "\n", text) */
    size_t tl = strlen(text), len = 0;
    char *t1 = malloc(tl + 1);
    for (size_t i = 0; i < tl; ) {
        if (text[i] == ' ' || text[i] == '\t') {
            size_t j = i;
            while (j < tl && (text[j] == ' ' || text[j] == '\t')) j++;
            if (j < tl && text[j] == '\n') { t1[len++] = '\n'; i = j + 1; continue; }
            memcpy(t1 + len, text + i, j - i); len += j - i; i = j;
        } else {
            t1[len++] = text[i++];
        }
    }
    t1[len] = '\0';
    free(text);
    /* re.sub(r"\n{3,}", "\n\n", t1) */
    tl = len; len = 0;
    char *t2 = malloc(tl + 1);
    for (size_t i = 0; i < tl; ) {
        if (t1[i] == '\n') {
            size_t j = i;
            while (j < tl && t1[j] == '\n') j++;
            size_t run = j - i;
            size_t keep = run >= 3 ? 2 : run;
            for (size_t k = 0; k < keep; k++) t2[len++] = '\n';
            i = j;
        } else {
            t2[len++] = t1[i++];
        }
    }
    t2[len] = '\0';
    free(t1);
    char *t3 = ba2_strip(t2);
    free(t2);
    json_t *r = json_string(t3);
    free(t3);
    return r;
}

/* ================================================================
 * Group B — identity / CLI-resolution cluster
 * ================================================================ */

/* PoP: ba2_hex_to_npub @ plugins/platforms/buzz/adapter.py:hex_to_npub */
static json_t *ba2_hex_to_npub_c(ba2_ctx_t *ctx, const char *pubkey_hex) {
    (void)ctx;
    size_t raw_len = 0;
    char *errmsg = NULL;
    unsigned char *raw = ba2_fromhex(pubkey_hex ? pubkey_hex : "", &raw_len, &errmsg);
    if (!raw) {
        /* except ValueError -> None */
        free(errmsg);
        return json_null();
    }
    if (raw_len != 32) { free(raw); return json_null(); }
    /* _convertbits(raw, 8, 5) — always succeeds for bytes */
    int data[64];
    size_t dn = 0;
    uint32_t acc = 0; int bits = 0;
    for (size_t i = 0; i < raw_len; i++) {
        acc = (acc << 8) | raw[i];
        bits += 8;
        while (bits >= 5) { bits -= 5; data[dn++] = (int)((acc >> bits) & 31); }
    }
    if (bits) data[dn++] = (int)((acc << (5 - bits)) & 31);
    free(raw);
    /* hrp_expand("npub") + data + [0]*6 */
    int values[128];
    size_t vn = 0;
    const char *hrp = "npub";
    for (const char *p = hrp; *p; p++) values[vn++] = (unsigned char)*p >> 5;
    values[vn++] = 0;
    for (const char *p = hrp; *p; p++) values[vn++] = (unsigned char)*p & 31;
    for (size_t i = 0; i < dn; i++) values[vn++] = data[i];
    for (int i = 0; i < 6; i++) values[vn++] = 0;
    uint32_t polymod = ba2_bech32_polymod(values, vn) ^ 1u;
    char out[5 + 64 + 6 + 1];
    memcpy(out, "npub1", 5);
    size_t on = 5;
    for (size_t i = 0; i < dn; i++) out[on++] = BA2_BECH32_CHARSET[data[i]];
    for (int i = 0; i < 6; i++) out[on++] = BA2_BECH32_CHARSET[(polymod >> (5 * (5 - i))) & 31];
    out[on] = '\0';
    return json_string(out);
}
static json_t *ba2__hex_to_npub(ba2_ctx_t *ctx, const json_t *args) {
    return ba2_hex_to_npub_c(ctx, json_get_str(args, "pubkey_hex", ""));
}

/* _resolve_cli_path core. */
static json_t *ba2_resolve_cli_path_c(ba2_ctx_t *ctx, const char *configured) {
    if (configured && configured[0]) {
        json_t *a = ba2_args();
        ba2_set_s(a, "path", configured);
        json_t *expanded = ba2_call(ctx, "expanduser", a);
        if (ba2_is_raise(expanded)) return expanded;
        char *pstr = ba2_pystr(expanded); /* str double result */
        json_free(expanded);
        json_t *a2 = ba2_args();
        ba2_set_s(a2, "path", pstr);
        json_t *isf = ba2_call(ctx, "is_file", a2);
        if (ba2_is_raise(isf)) { free(pstr); return isf; }
        bool yes = ba2_truthy(isf);
        json_free(isf);
        json_t *r = yes ? json_string(pstr) : json_string("");
        free(pstr);
        return r;
    }
    json_t *a3 = ba2_args();
    ba2_set_s(a3, "name", "buzz");
    json_t *found = ba2_call(ctx, "shutil.which", a3);
    if (ba2_is_raise(found)) return found;
    if (ba2_truthy(found)) return found;
    json_free(found);
    json_t *a4 = ba2_args();
    json_t *home = ba2_call(ctx, "Path.home", a4);
    if (ba2_is_raise(home)) return home;
    char *homes = ba2_pystr(home);
    json_free(home);
    char *fallback = ba2_path_join(homes, "bin/buzz");
    free(homes);
    json_t *a5 = ba2_args();
    ba2_set_s(a5, "path", fallback);
    json_t *isf2 = ba2_call(ctx, "is_file", a5);
    if (ba2_is_raise(isf2)) { free(fallback); return isf2; }
    bool yes2 = ba2_truthy(isf2);
    json_free(isf2);
    json_t *r = yes2 ? json_string(fallback) : json_string("");
    free(fallback);
    return r;
}
/* PoP: ba2__resolve_cli_path @ plugins/platforms/buzz/adapter.py:_resolve_cli_path */
static json_t *ba2__resolve_cli_path(ba2_ctx_t *ctx, const json_t *args) {
    return ba2_resolve_cli_path_c(ctx, json_get_str(args, "configured", ""));
}

/* PoP: ba2__configured_cli_path @ plugins/platforms/buzz/adapter.py:_configured_cli_path */
static json_t *ba2__configured_cli_path(ba2_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    json_t *a = ba2_args();
    ba2_set_s(a, "env_name", "BUZZ_CLI_PATH");
    ba2_set_copy(a, "extra", extra);
    ba2_set_s(a, "key", "cli_path");
    json_t *raw = ba2_call(ctx, "_scoped_platform_setting", a);
    if (ba2_is_raise(raw)) return raw;
    char *s1 = ba2_pystr_or_empty(raw);
    json_free(raw);
    char *s1s = ba2_strip(s1); free(s1);
    char *configured = NULL;
    if (s1s[0]) {
        configured = s1s;
    } else {
        free(s1s);
        if (!extra || extra->type != JSON_OBJECT)
            return ba2_no_attr(extra, "get");
        char *s2 = ba2_pystr_or_empty(json_obj_get(extra, "cli_path"));
        configured = s2;
    }
    json_t *r = ba2_resolve_cli_path_c(ctx, configured);
    free(configured);
    return r;
}

/* _get_scoped_secret core (REAL; wraps the _shared_scoped_secret seam). */
static json_t *ba2_get_scoped_secret_c(ba2_ctx_t *ctx, const char *name, const json_t *dflt) {
    json_t *a = ba2_args();
    ba2_set_s(a, "name", name);
    ba2_set_copy(a, "default", dflt);
    ba2_set_b(a, "external_fallback", true);
    return ba2_call(ctx, "_shared_scoped_secret", a);
}
/* PoP: ba2__get_scoped_secret @ plugins/platforms/buzz/adapter.py:_get_scoped_secret */
static json_t *ba2__get_scoped_secret(ba2_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    const json_t *dflt = json_obj_get(args, "default");
    return ba2_get_scoped_secret_c(ctx, name, dflt);
}

/* _resolve_auth_tag core (REAL). */
static json_t *ba2_resolve_auth_tag_c(ba2_ctx_t *ctx, const json_t *extra) {
    json_t *dflt = json_string("");
    json_t *sec = ba2_get_scoped_secret_c(ctx, "BUZZ_AUTH_TAG", dflt);
    json_free(dflt);
    if (ba2_is_raise(sec)) return sec;
    char *raw0 = ba2_pystr_or_empty(sec);
    json_free(sec);
    char *raw = ba2_strip(raw0); free(raw0);
    if (!raw[0]) {
        free(raw);
        json_t *d2 = json_string("");
        json_t *pk = ba2_get_scoped_secret_c(ctx, "BUZZ_PRIVATE_KEY", d2);
        json_free(d2);
        if (ba2_is_raise(pk)) return pk;
        char *pks = ba2_pystr_or_empty(pk);
        json_free(pk);
        char *pkss = ba2_strip(pks); free(pks);
        bool has_key = pkss[0] != '\0';
        free(pkss);
        if (has_key) {
            json_t *a = ba2_args();
            ba2_set_copy(a, "extra", extra);
            json_t *cf = ba2_call(ctx, "_configured_credentials_file", a);
            if (ba2_is_raise(cf)) return cf;
            bool no_file = !ba2_truthy(cf);
            json_free(cf);
            if (no_file) return json_string("");
        }
        json_t *a2 = ba2_args();
        ba2_set_copy(a2, "extra", extra);
        json_t *data = ba2_call(ctx, "_resolve_credentials_data", a2);
        if (ba2_is_raise(data)) return data;
        if (!data || data->type != JSON_OBJECT || !json_has(data, "auth_tag")) {
            json_free(data);
            return json_string("");
        }
        json_t *tag = json_copy(json_obj_get(data, "auth_tag"));
        json_free(data);
        json_t *a3 = ba2_args();
        ba2_set_copy(a3, "raw", tag);
        ba2_set_s(a3, "label", "Buzz auth tag");
        json_free(tag);
        json_t *parsed = ba2_call(ctx, "parse_auth_tag", a3);
        if (ba2_is_raise(parsed)) return parsed;
        char *s = json_serialize(parsed); /* json.dumps(..., separators=(",",":")) on ASCII */
        json_free(parsed);
        json_t *r = json_string(s ? s : "null");
        free(s);
        return r;
    }
    json_t *a3 = ba2_args();
    ba2_set_s(a3, "raw", raw);
    ba2_set_s(a3, "label", "Buzz auth tag");
    free(raw);
    json_t *parsed = ba2_call(ctx, "parse_auth_tag", a3);
    if (ba2_is_raise(parsed)) return parsed;
    char *s = json_serialize(parsed);
    json_free(parsed);
    json_t *r = json_string(s ? s : "null");
    free(s);
    return r;
}
/* PoP: ba2__resolve_auth_tag @ plugins/platforms/buzz/adapter.py:_resolve_auth_tag */
static json_t *ba2__resolve_auth_tag(ba2_ctx_t *ctx, const json_t *args) {
    return ba2_resolve_auth_tag_c(ctx, json_obj_get(args, "extra"));
}

/* ================================================================
 * Group C — websocket / task leaves
 * ================================================================ */

/* PoP: ba2__consume_ws_read_task @ plugins/platforms/buzz/adapter.py:_consume_ws_read_task */
static json_t *ba2__consume_ws_read_task(ba2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *c = ba2_call(ctx, "task.cancelled", ba2_args());
    if (ba2_is_raise(c)) return c;
    bool cancelled = ba2_truthy(c);
    json_free(c);
    if (cancelled) return json_null();
    /* with contextlib.suppress(Exception): task.exception() */
    json_t *e = ba2_call(ctx, "task.exception", ba2_args());
    if (ba2_is_raise(e)) {
        if (ba2_is_base_exc(ba2_kind(e))) return e; /* BaseException passes through */
        json_free(e); /* Exception family swallowed */
        return json_null();
    }
    json_free(e);
    return json_null();
}

/* PoP: ba2__websocket_url @ plugins/platforms/buzz/adapter.py:_websocket_url */
static json_t *ba2__websocket_url(ba2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *err = NULL;
    const json_t *relay = ba2_self_req(ctx, "relay_url", &err);
    if (err) return err;
    if (!relay || relay->type != JSON_STRING) return ba2_no_attr(relay, "strip");
    char *stripped = ba2_strip(relay->str_val ? relay->str_val : "");
    json_t *parsed = ba2_urlsplit(ctx, stripped);
    free(stripped);
    if (ba2_is_raise(parsed)) return parsed;
    const char *ps = ba2_str(json_obj_get(parsed, "scheme"));
    const char *scheme = ps ? ps : "";
    if (strcmp(scheme, "http") == 0) scheme = "ws";
    else if (strcmp(scheme, "https") == 0) scheme = "wss";
    const json_t *netloc = json_obj_get(parsed, "netloc");
    if ((strcmp(scheme, "ws") != 0 && strcmp(scheme, "wss") != 0) || !ba2_truthy(netloc)) {
        json_free(parsed);
        return ba2_err("ValueError", "Buzz relay URL must use http(s) or ws(s)");
    }
    const json_t *path = json_obj_get(parsed, "path");
    const json_t *query = json_obj_get(parsed, "query");
    json_t *a = ba2_args();
    json_t *parts = json_array();
    json_append(parts, json_string(scheme));
    json_append(parts, json_string(ba2_str(netloc) ? ba2_str(netloc) : ""));
    json_append(parts, json_string(ba2_truthy(path) ? (ba2_str(path) ? ba2_str(path) : "") : ""));
    json_append(parts, json_string(ba2_str(query) ? ba2_str(query) : ""));
    json_append(parts, json_string(""));
    json_set(a, "parts", parts);
    json_free(parsed);
    return ba2_call(ctx, "urlunsplit", a);
}

/* ================================================================
 * Group D — imeta attachment parsing
 * ================================================================ */

/* _safe_attachment_filename core on a C string. */
static char *ba2_safe_filename_c(const char *value) {
    json_t *a = ba2_args();
    ba2_set_s(a, "value", value);
    json_t *r = ba2__safe_attachment_filename(NULL, a);
    json_free(a);
    char *s = ba2_strdup(ba2_str(r) ? ba2_str(r) : "");
    json_free(r);
    return s;
}
/* _parse_imeta_attachments core. */
static json_t *ba2_parse_imeta_c(ba2_ctx_t *ctx, const json_t *event) {
    if (!event || event->type != JSON_OBJECT) return ba2_no_attr(event, "get");
    const json_t *tags = json_obj_get(event, "tags");
    json_t *attachments = json_array();
    long long rejected = 0, total = 0;
    if (!tags || tags->type != JSON_ARRAY) {
        json_t *r = json_array();
        json_append(r, attachments);
        json_append(r, json_number(0));
        return r;
    }
    for (size_t ti = 0; ti < json_len(tags); ti++) {
        const json_t *tag = json_get(tags, ti);
        if (!tag || tag->type != JSON_ARRAY || json_len(tag) == 0) continue;
        if (!ba2_str_eq(json_get(tag, 0), "imeta")) continue;
        if (json_len(attachments) >= 4) { rejected++; continue; }
        /* fields: first-wins partition(" ") on str entries */
        json_t *fields = json_object();
        for (size_t fi = 1; fi < json_len(tag); fi++) {
            const json_t *f = json_get(tag, fi);
            const char *fs = ba2_str(f);
            if (!fs) continue;
            const char *sp = strchr(fs, ' ');
            if (!sp) continue;
            size_t kl = (size_t)(sp - fs);
            char *key = malloc(kl + 1);
            if (!key) continue;
            memcpy(key, fs, kl); key[kl] = '\0';
            if (!json_has(fields, key)) {
                char *val = ba2_strip(sp + 1);
                json_set_hdr(fields, key, val);
                free(val);
            }
            free(key);
        }
        const char *url = json_get_str(fields, "url", "");
        char *digest0 = ba2_lower(json_get_str(fields, "x", ""));
        /* try: size = int(...); parsed = urlsplit(url); hostname; port */
        const json_t *sizev = json_obj_get(fields, "size");
        json_t *sizenode = sizev ? (json_t *)sizev : json_string("");
        long long size = 0;
        char *emsg = NULL;
        bool size_ok = ba2_int_of(sizenode, &size, &emsg);
        if (!sizev) json_free(sizenode);
        if (!size_ok) {
            /* except (TypeError, ValueError) -> rejected */
            free(emsg); free(digest0); json_free(fields);
            rejected++;
            continue;
        }
        json_t *parsed = ba2_urlsplit(ctx, url);
        if (ba2_is_raise(parsed)) {
            const char *k = ba2_kind(parsed);
            if (ba2_is_valueerror(k) || ba2_is_typeerror(k)) {
                json_free(parsed); free(digest0); json_free(fields);
                rejected++;
                continue;
            }
            free(digest0); json_free(fields);
            return parsed; /* BaseException / other propagates */
        }
        json_t *port = ba2_port_access(parsed);
        if (ba2_is_raise(port)) {
            const char *k = ba2_kind(port);
            if (ba2_is_valueerror(k) || ba2_is_typeerror(k)) {
                json_free(port); json_free(parsed); free(digest0); json_free(fields);
                rejected++;
                continue;
            }
            json_free(parsed); free(digest0); json_free(fields);
            return port;
        }
        json_free(port);
        const json_t *scheme = json_obj_get(parsed, "scheme");
        const json_t *hostname = json_obj_get(parsed, "hostname");
        const json_t *username = json_obj_get(parsed, "username");
        const json_t *password = json_obj_get(parsed, "password");
        const json_t *fragment = json_obj_get(parsed, "fragment");
        bool bad = !ba2_str_eq(scheme, "https") || !ba2_truthy(hostname)
            || ba2_truthy(username) || ba2_truthy(password) || ba2_truthy(fragment)
            || !ba2_hex64(digest0) || !(size > 0 && size <= 20971520)
            || (total + size > 20971520);
        json_free(parsed);
        if (bad) { free(digest0); json_free(fields); rejected++; continue; }
        total += size;
        char *fname = ba2_safe_filename_c(json_get_str(fields, "filename", ""));
        char *mime0 = ba2_strdup(json_get_str(fields, "m", ""));
        char *mime = ba2_slice(mime0, 255);
        free(mime0);
        json_t *att = json_object();
        ba2_set_s(att, "url", url);
        ba2_set_s(att, "sha256", digest0);
        ba2_set_n(att, "size", (double)size);
        ba2_set_s(att, "filename", fname);
        ba2_set_s(att, "mime_type", mime);
        json_append(attachments, att);
        free(fname); free(mime); free(digest0); json_free(fields);
    }
    json_t *r = json_array();
    json_append(r, attachments);
    json_append(r, json_number((double)rejected));
    return r;
}
/* PoP: ba2__parse_imeta_attachments @ plugins/platforms/buzz/adapter.py:_parse_imeta_attachments */
static json_t *ba2__parse_imeta_attachments(ba2_ctx_t *ctx, const json_t *args) {
    return ba2_parse_imeta_c(ctx, json_obj_get(args, "event"));
}
/* PoP: ba2__imeta_attachments @ plugins/platforms/buzz/adapter.py:_imeta_attachments */
static json_t *ba2__imeta_attachments(ba2_ctx_t *ctx, const json_t *args) {
    json_t *r = ba2_parse_imeta_c(ctx, json_obj_get(args, "event"));
    if (ba2_is_raise(r)) return r;
    json_t *list = json_copy(json_get(r, 0));
    json_free(r);
    return list;
}

/* ================================================================
 * Group E — send-result / connect-failure leaves
 * ================================================================ */

/* PoP: ba2__send_result @ plugins/platforms/buzz/adapter.py:_send_result */
static json_t *ba2__send_result(ba2_ctx_t *ctx, const json_t *args) {
    long long code = (long long)json_get_num(args, "code", 0);
    const char *out = json_get_str(args, "out", "");
    const char *err = json_get_str(args, "err", "");
    const json_t *redact = json_obj_get(args, "redact_path");
    if (code != 0) {
        json_t *a = ba2_args();
        ba2_set_s(a, "stderr", err);
        ba2_set_n(a, "returncode", (double)code);
        ba2_set_copy(a, "redact_path", redact);
        json_t *msg = ba2_call(ctx, "_cli_error_message", a);
        if (ba2_is_raise(msg)) return msg;
        json_t *r = ba2_send_result(false, NULL, msg, NULL, code == 2);
        json_free(msg);
        return r;
    }
    json_t *a2 = ba2_args();
    ba2_set_s(a2, "stdout", out);
    json_t *receipt = ba2_call(ctx, "_parse_send_receipt", a2);
    if (ba2_is_raise(receipt)) return receipt;
    const json_t *eid = json_get(receipt, 0);
    const json_t *rerr = json_get(receipt, 1);
    if (ba2_truthy(rerr)) {
        json_t *r = ba2_send_result(false, NULL, rerr, NULL, false);
        json_free(receipt);
        return r;
    }
    if (ba2_is_none(eid)) {
        json_free(receipt);
        return ba2_err("AssertionError", ""); /* assert event_id is not None */
    }
    char *cid = ba2_pystr(json_obj_get(args, "chat_id"));
    json_t *a3 = ba2_args();
    ba2_set_s(a3, "channel_id", cid);
    ba2_set_copy(a3, "event_id", eid);
    free(cid);
    json_t *ms = ba2_call(ctx, "self._mark_seen", a3);
    if (ba2_is_raise(ms)) { json_free(receipt); return ms; }
    json_free(ms);
    json_t *r = ba2_send_result(true, eid, NULL, NULL, false);
    json_free(receipt);
    return r;
}

/* PoP: ba2__connect_failed @ plugins/platforms/buzz/adapter.py:_connect_failed */
static json_t *ba2__connect_failed(ba2_ctx_t *ctx, const json_t *args) {
    const char *code = json_get_str(args, "code", "");
    const char *detail = json_get_str(args, "detail", "");
    const char *log = json_get_str(args, "log", "");
    const json_t *log_args = json_obj_get(args, "log_args");
    bool retryable = json_get_bool(args, "retryable", false);
    json_t *a = ba2_args();
    ba2_set_s(a, "fmt", log);
    json_set(a, "args", log_args ? json_copy(log_args) : json_array());
    json_t *lr = ba2_call(ctx, "logger.error", a);
    if (ba2_is_raise(lr)) return lr;
    json_free(lr);
    json_t *a2 = ba2_args();
    ba2_set_s(a2, "code", code);
    ba2_set_s(a2, "detail", detail);
    ba2_set_b(a2, "retryable", retryable);
    json_t *sf = ba2_call(ctx, "self._set_fatal_error", a2);
    if (ba2_is_raise(sf)) return sf;
    json_free(sf);
    return json_bool(false);
}

/* ================================================================
 * Group F — async CLI plumbing flows (synchronous seam model)
 * ================================================================ */

/* _run_cli core (REAL). */
static json_t *ba2_run_cli_c(ba2_ctx_t *ctx, const json_t *args_list, const json_t *input_text) {
    json_t *err = NULL;
    const json_t *pk = ba2_self_req(ctx, "_private_key", &err);
    if (err) return err;
    if (!ba2_truthy(pk)) {
        const json_t *extra = ba2_self_req(ctx, "_extra", &err);
        if (err) return err;
        json_t *a = ba2_args();
        ba2_set_copy(a, "extra", extra);
        json_t *key = ba2_call(ctx, "_resolve_private_key", a);
        if (ba2_is_raise(key)) return key;
        json_set(ctx->state, "_private_key", json_copy(key));
        json_free(key);
        json_t *tag = ba2_resolve_auth_tag_c(ctx, extra);
        if (ba2_is_raise(tag)) return tag;
        json_set(ctx->state, "_auth_tag", json_copy(tag));
        json_free(tag);
    }
    /* _exec_buzz(self.cli_path, args, relay_url=, private_key=, auth_tag=, input_text=) */
    const json_t *cli = ba2_self_req(ctx, "cli_path", &err);
    if (err) return err;
    const json_t *relay = ba2_self_req(ctx, "relay_url", &err);
    if (err) return err;
    const json_t *pk2 = ba2_self_req(ctx, "_private_key", &err);
    if (err) return err;
    const json_t *at = ba2_self_req(ctx, "_auth_tag", &err);
    if (err) return err;
    json_t *a2 = ba2_args();
    ba2_set_copy(a2, "cli_path", cli);
    json_set(a2, "args", args_list ? json_copy(args_list) : json_array());
    ba2_set_copy(a2, "relay_url", relay);
    ba2_set_copy(a2, "private_key", pk2);
    ba2_set_copy(a2, "auth_tag", at);
    ba2_set_copy(a2, "input_text", input_text);
    return ba2_call(ctx, "_exec_buzz", a2);
}
/* PoP: ba2__run_cli @ plugins/platforms/buzz/adapter.py:_run_cli */
static json_t *ba2__run_cli(ba2_ctx_t *ctx, const json_t *args) {
    return ba2_run_cli_c(ctx, json_obj_get(args, "args"), json_obj_get(args, "input_text"));
}

/* _cli_json core (REAL). */
static json_t *ba2_cli_json_c(ba2_ctx_t *ctx, const json_t *args_list, const json_t *dflt) {
    json_t *res = ba2_run_cli_c(ctx, args_list, NULL);
    if (ba2_is_raise(res)) return res;
    const json_t *code = json_get(res, 0);
    const json_t *out = json_get(res, 1);
    bool zero = code && code->type == JSON_NUMBER && code->num_val == 0.0;
    if (!zero) {
        json_free(res);
        return dflt ? json_copy(dflt) : json_null();
    }
    json_t *a = ba2_args();
    ba2_set_s(a, "text", ba2_str(out) ? ba2_str(out) : "");
    ba2_set_copy(a, "default", dflt);
    json_t *parsed = ba2_call(ctx, "_json_or", a);
    json_free(res);
    return parsed;
}
/* PoP: ba2__cli_json @ plugins/platforms/buzz/adapter.py:_cli_json */
static json_t *ba2__cli_json(ba2_ctx_t *ctx, const json_t *args) {
    return ba2_cli_json_c(ctx, json_obj_get(args, "args"), json_obj_get(args, "default"));
}

/* logger.debug seam helper: {fmt, args}. */
static json_t *ba2_log_debug(ba2_ctx_t *ctx, const char *fmt, json_t *args_arr) {
    json_t *a = ba2_args();
    ba2_set_s(a, "fmt", fmt);
    json_set(a, "args", args_arr ? args_arr : json_array());
    return ba2_call(ctx, "logger.debug", a);
}
/* _cli_error_message seam (redact_path null). */
static json_t *ba2_cli_err_msg(ba2_ctx_t *ctx, const char *err, long long code) {
    json_t *a = ba2_args();
    ba2_set_s(a, "stderr", err ? err : "");
    ba2_set_n(a, "returncode", (double)code);
    ba2_set_null(a, "redact_path");
    return ba2_call(ctx, "_cli_error_message", a);
}
/* _json_or seam (flow1-claimed sibling). */
static json_t *ba2_json_or_seam(ba2_ctx_t *ctx, const char *text, const json_t *dflt) {
    json_t *a = ba2_args();
    ba2_set_s(a, "text", text ? text : "");
    ba2_set_copy(a, "default", dflt);
    return ba2_call(ctx, "_json_or", a);
}

/* PoP: ba2_send_reaction @ plugins/platforms/buzz/adapter.py:send_reaction */
static json_t *ba2_send_reaction(ba2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *message_id = json_get_str(args, "message_id", "");
    const char *emoji = json_get_str(args, "emoji", "");
    json_t *err = NULL;
    const json_t *cli = ba2_self_req(ctx, "cli_path", &err);
    if (err) return err;
    if (!ba2_truthy(cli) || !emoji[0] || !message_id[0]) return json_bool(false);
    char *mid = ba2_pystr(json_obj_get(args, "message_id"));
    json_t *cargs = json_array();
    json_append(cargs, json_string("reactions"));
    json_append(cargs, json_string("add"));
    json_append(cargs, json_string("--event"));
    json_append(cargs, json_string(mid));
    json_append(cargs, json_string("--emoji"));
    json_append(cargs, json_string(emoji));
    free(mid);
    json_t *res = ba2_run_cli_c(ctx, cargs, NULL);
    json_free(cargs);
    if (ba2_is_raise(res)) return res;
    long long code = (long long)ba2_arr_num(res, 0, 0);
    if (code != 0) {
        const char *errs = ba2_arr_str(res, 2, "");
        json_t *cem = ba2_cli_err_msg(ctx, errs, code);
        if (ba2_is_raise(cem)) { json_free(res); return cem; }
        char *mid12 = ba2_slice(message_id, 12);
        json_t *la = json_array();
        json_append(la, json_string(mid12));
        json_append(la, json_string(chat_id));
        json_append(la, cem); /* adopted */
        free(mid12);
        json_t *lr = ba2_log_debug(ctx, "Buzz: reaction add failed for message %s in %s \xE2\x80\x94 %s", la);
        if (ba2_is_raise(lr)) { json_free(res); return lr; }
        json_free(lr);
    }
    json_free(res);
    return json_bool(code == 0);
}

/* PoP: ba2_edit_message @ plugins/platforms/buzz/adapter.py:edit_message */
static json_t *ba2_edit_message(ba2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *message_id = json_get_str(args, "message_id", "");
    const char *content = json_get_str(args, "content", "");
    if (!message_id[0]) {
        json_t *e = json_string("Buzz edit needs a message id");
        json_t *r = ba2_send_result(false, NULL, e, NULL, false);
        json_free(e);
        return r;
    }
    if (!content[0]) {
        json_t *e = json_string("Empty message");
        json_t *r = ba2_send_result(false, NULL, e, NULL, false);
        json_free(e);
        return r;
    }
    char *mid = ba2_pystr(json_obj_get(args, "message_id"));
    char *carg = malloc(strlen(content) + 16);
    if (carg) sprintf(carg, "--content=%s", content);
    json_t *cargs = json_array();
    json_append(cargs, json_string("messages"));
    json_append(cargs, json_string("edit"));
    json_append(cargs, json_string("--event"));
    json_append(cargs, json_string(mid));
    json_append(cargs, json_string(carg ? carg : ""));
    free(mid); free(carg);
    json_t *res = ba2_run_cli_c(ctx, cargs, NULL);
    json_free(cargs);
    if (ba2_is_raise(res)) return res;
    long long code = (long long)ba2_arr_num(res, 0, 0);
    if (code != 0) {
        json_t *cem = ba2_cli_err_msg(ctx, ba2_arr_str(res, 2, ""), code);
        if (ba2_is_raise(cem)) { json_free(res); return cem; }
        json_t *r = ba2_send_result(false, NULL, cem, NULL, code == 2);
        json_free(cem); json_free(res);
        return r;
    }
    json_t *dflt = json_object();
    json_t *data = ba2_json_or_seam(ctx, ba2_arr_str(res, 1, ""), dflt);
    json_free(dflt);
    json_free(res);
    if (ba2_is_raise(data)) return data;
    if (!data || data->type != JSON_OBJECT) return ba2_no_attr(data, "get");
    const json_t *eid = json_obj_get(data, "event_id");
    if (ba2_truthy(eid)) {
        char *cid = ba2_pystr(json_obj_get(args, "chat_id"));
        char *eids = ba2_pystr(eid);
        json_t *a = ba2_args();
        ba2_set_s(a, "channel_id", cid);
        ba2_set_s(a, "event_id", eids);
        free(cid); free(eids);
        json_t *ms = ba2_call(ctx, "self._mark_seen", a);
        if (ba2_is_raise(ms)) { json_free(data); return ms; }
        json_free(ms);
    }
    const json_t *acc = json_obj_get(data, "accepted");
    bool success = acc ? ba2_truthy(acc) : true;
    char *mids = ba2_pystr(json_obj_get(args, "message_id"));
    json_t *midn = json_string(mids);
    free(mids);
    json_t *r = ba2_send_result(success, midn, NULL, data, false);
    json_free(midn); json_free(data);
    return r;
}

/* PoP: ba2_delete_message @ plugins/platforms/buzz/adapter.py:delete_message */
static json_t *ba2_delete_message(ba2_ctx_t *ctx, const json_t *args) {
    const char *chat_id = json_get_str(args, "chat_id", "");
    const char *message_id = json_get_str(args, "message_id", "");
    if (!message_id[0]) return json_bool(false);
    char *mid = ba2_pystr(json_obj_get(args, "message_id"));
    json_t *cargs = json_array();
    json_append(cargs, json_string("messages"));
    json_append(cargs, json_string("delete"));
    json_append(cargs, json_string("--event"));
    json_append(cargs, json_string(mid));
    free(mid);
    json_t *res = ba2_run_cli_c(ctx, cargs, NULL);
    json_free(cargs);
    if (ba2_is_raise(res)) return res;
    long long code = (long long)ba2_arr_num(res, 0, 0);
    if (code != 0) { json_free(res); return json_bool(false); }
    const char *out = ba2_arr_str(res, 1, "");
    json_free(res);
    /* try: data = json.loads(out or "{}") except ValueError: return True */
    json_t *data = ba2_json_loads(out && out[0] ? out : "{}");
    if (!data) return json_bool(true);
    if (data->type != JSON_OBJECT) {
        json_t *e = ba2_no_attr(data, "get");
        json_free(data);
        return e;
    }
    const json_t *eid = json_obj_get(data, "event_id");
    if (ba2_truthy(eid)) {
        char *cid = ba2_pystr(json_obj_get(args, "chat_id"));
        char *eids = ba2_pystr(eid);
        json_t *a = ba2_args();
        ba2_set_s(a, "channel_id", cid);
        ba2_set_s(a, "event_id", eids);
        free(cid); free(eids);
        json_t *ms = ba2_call(ctx, "self._mark_seen", a);
        if (ba2_is_raise(ms)) { json_free(data); return ms; }
        json_free(ms);
    }
    const json_t *acc = json_obj_get(data, "accepted");
    bool success = acc ? ba2_truthy(acc) : true;
    json_free(data);
    return json_bool(success);
}

/* PoP: ba2_get_chat_info @ plugins/platforms/buzz/adapter.py:get_chat_info */
static json_t *ba2_get_chat_info(ba2_ctx_t *ctx, const json_t *args) {
    char *cid = ba2_pystr(json_obj_get(args, "chat_id"));
    json_t *err = NULL;
    const json_t *cstate = ba2_self_req(ctx, "_channel_state", &err);
    if (err) { free(cid); return err; }
    if (!cstate || cstate->type != JSON_OBJECT) { json_t *e = ba2_no_attr(cstate, "get"); free(cid); return e; }
    const json_t *state = json_obj_get(cstate, cid);
    const json_t *cnames = ba2_self_req(ctx, "_channel_names", &err);
    if (err) { free(cid); return err; }
    if (!cnames || cnames->type != JSON_OBJECT) { json_t *e = ba2_no_attr(cnames, "get"); free(cid); return e; }
    const json_t *name = json_obj_get(cnames, cid);
    if (ba2_is_none(name)) {
        const json_t *cli = ba2_self_req(ctx, "cli_path", &err);
        if (err) { free(cid); return err; }
        if (ba2_truthy(cli)) {
            json_t *cargs = json_array();
            json_append(cargs, json_string("channels"));
            json_append(cargs, json_string("get"));
            json_append(cargs, json_string("--channel"));
            json_append(cargs, json_string(cid));
            json_t *dflt = json_object();
            json_t *data = ba2_cli_json_c(ctx, cargs, dflt);
            json_free(cargs); json_free(dflt);
            if (ba2_is_raise(data)) { free(cid); return data; }
            if (data && data->type == JSON_OBJECT) {
                const json_t *nm = json_obj_get(data, "name");
                if (ba2_truthy(nm)) {
                    char *nms = ba2_pystr(nm);
                    json_set_hdr((json_t *)cnames, cid, nms);
                    name = json_obj_get(cnames, cid);
                    free(nms);
                }
            }
            json_free(data);
        }
    }
    /* {"name": name or chat_id, "type": state["chat_type"] if state else "group", "chat_id": cid} */
    const char *type = "group";
    if (ba2_truthy(state)) {
        const json_t *ct = state ? json_obj_get(state, "chat_type") : NULL;
        if (!ct) { free(cid); return ba2_err("KeyError", "'chat_type'"); }
        type = ba2_str(ct) ? ba2_str(ct) : "";
    }
    json_t *r = json_object();
    ba2_set_s(r, "name", ba2_truthy(name) ? (ba2_str(name) ? ba2_str(name) : "") : cid);
    ba2_set_s(r, "type", type);
    ba2_set_s(r, "chat_id", cid);
    free(cid);
    return r;
}

/* PoP: ba2__resolve_user_name @ plugins/platforms/buzz/adapter.py:_resolve_user_name */
static json_t *ba2__resolve_user_name(ba2_ctx_t *ctx, const json_t *args) {
    const char *pubkey = json_get_str(args, "pubkey", "");
    json_t *err = NULL;
    const json_t *unames = ba2_self_req(ctx, "_user_names", &err);
    if (err) return err;
    if (!unames || unames->type != JSON_OBJECT) return ba2_no_attr(unames, "get");
    const json_t *cached = json_obj_get(unames, pubkey);
    if (!ba2_is_none(cached)) return json_copy(cached);
    json_t *cargs = json_array();
    json_append(cargs, json_string("users"));
    json_append(cargs, json_string("get"));
    json_append(cargs, json_string("--pubkey"));
    json_append(cargs, json_string(pubkey));
    json_t *res = ba2_run_cli_c(ctx, cargs, NULL);
    json_free(cargs);
    if (ba2_is_raise(res)) return res;
    long long code = (long long)ba2_arr_num(res, 0, 0);
    json_t *profiles = NULL;
    if (code == 0) {
        json_t *a = ba2_args();
        ba2_set_s(a, "stdout", ba2_arr_str(res, 1, ""));
        profiles = ba2_call(ctx, "_parse_json_list", a);
        if (ba2_is_raise(profiles)) { json_free(res); return profiles; }
    }
    json_free(res);
    char *name = ba2_strdup("");
    if (profiles && json_len(profiles) > 0) {
        const json_t *p0 = json_get(profiles, 0);
        if (!p0 || p0->type != JSON_OBJECT) {
            json_t *e = ba2_no_attr(p0, "get");
            json_free(profiles); free(name);
            return e;
        }
        char *dn = ba2_pystr_or_empty(json_obj_get(p0, "display_name"));
        char *dns = ba2_strip(dn); free(dn);
        free(name); name = dns;
    }
    json_free(profiles);
    if (!name[0]) {
        free(name);
        json_t *np = ba2_hex_to_npub_c(ctx, pubkey);
        const char *use = ba2_truthy(np) ? ba2_str(np) : pubkey;
        name = ba2_slice(use ? use : "", 16);
        json_free(np);
    }
    json_set_hdr((json_t *)unames, pubkey, name);
    json_t *r = json_string(name);
    free(name);
    return r;
}