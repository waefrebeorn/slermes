/*
 * photon_adapter_flow2.c — Photon Spectrum (iMessage) adapter FLOW remainder
 * of plugins/platforms/photon/adapter.py (hermes-agent). Ports the UNCLAIMED
 * second-wave clusters left after photon_adapter_flow.c (36 funcs) and
 * managed_tool_gateway.c (_parse_timestamp):
 *
 *  A. Sidecar runtime-record lifecycle: _runtime_record_path /
 *     _legacy_runtime_record_path / _current_runtime_record_path /
 *     _write_runtime_record / _read_runtime_record / _delete_runtime_record
 *     (PurePosixPath join/parent/name algebra + suppress(OSError) unlink +
 *     atomic_json_write / read_text seams + json.loads dict gate).
 *  B. Liveness / error projection: _sidecar_pid_alive (int() coercion +
 *     gateway.status._pid_exists + os.name/os.kill fallback),
 *     _sidecar_error_from_response (PhotonSidecarError field projection),
 *     _is_timeout_error (TimeoutError / httpx timeout isinstance + name gate).
 *  C. Config / env enablement: validate_config / is_connected /
 *     _env_enablement (short-circuit credential ladder + seed_extra_from_env).
 *  D. Sent-text index + mime: _sent_text_key / _record_sent_text_async /
 *     _lookup_sent_text / _guess_mime (rich_sent_store + mimetypes seams).
 *  E. Standalone (out-of-process) send helpers: _standalone_error /
 *     _standalone_token_from_record (resp.json + send_error + record reuse).
 *  F. Adapter methods: _sidecar_url / _sidecar_headers /
 *     _note_upstream_activity / _cancel_pending_fffc / _gate_stt_chat /
 *     _add_reaction / _remove_reaction / add_reaction / remove_reaction /
 *     on_processing_start / get_chat_info / format_message / send_typing /
 *     stop_typing / _send_plain_fallback.
 *
 * Universal seam: every SEAMED sibling / foreign call enters ONE callback
 * ph2_call(ctx, name, args). The harness wires ctx->io to a recording stub fed
 * the case's scripted results; the oracle installs recording stubs emitting the
 * identical event shape {"name","args","ret"} from the SAME scripts. args is
 * ADOPTED into the trace event; the return is a NEW owned json_t (NULL == Python
 * None; {"__raise__":cls,"message":str(exc),...} == a raised exception). A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 * BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
 * BaseExceptionGroup) pass through every `except Exception` / suppress site.
 *
 * Mode table:
 *  REAL (inline C call): ALL 33 ported functions call their REAL ported children
 *    directly (_current -> _legacy -> _runtime; is_connected -> validate_config;
 *    _standalone_token_from_record -> _read_runtime_record + _sidecar_pid_alive;
 *    add_reaction -> _add_reaction -> sidecar_try seam; _send_plain_fallback ->
 *    format_message; _record_sent_text_async / _lookup_sent_text -> _sent_text_key).
 *  SEAMED (ph2_call, scripted): the CLAIMED flow1 siblings _normalize_chat_key
 *    (normalize_chat_key), _reply_inner (reply_inner), _carries_voice
 *    (carries_voice), _reactions_enabled (reactions_enabled), _markdown_enabled
 *    (markdown_enabled), _sidecar_try (sidecar_try), _sidecar_send (sidecar_send);
 *    and the foreign imports get_hermes_home, atomic_json_write, read_text,
 *    unlink, pid_exists (gateway.status._pid_exists), os.kill,
 *    load_project_credentials, get_scoped_secret (_get_scoped_secret),
 *    seed_extra_from_env (_seed_extra_from_env), coerce_port (_coerce_port),
 *    send_error, strip_markdown, voice_mention_gate_enabled,
 *    rich_sent_store.record_async, rich_sent_store.lookup, guess_type
 *    (mimetypes.guess_type), resp.json, time.monotonic, time.time, task.done,
 *    task.cancel.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and ph2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_photon_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}; extra spec keys preserved.
 * - Paths project to POSIX strings (PurePosixPath str). get_hermes_home returns a
 *   home path string; the C joins with PurePosixPath algebra (exactly-two leading
 *   slashes preserved, trailing slashes normalized, '.' components dropped).
 * - PhotonSidecarError (returned, not raised, by _sidecar_error_from_response)
 *   projects to {"__exc__":"PhotonSidecarError","message":str(e),"path",
 *   "status_code","error","error_class","retryable"}.
 * - Tuples project to JSON arrays: load_project_credentials -> [id,sec];
 *   _gate_stt_chat -> [space_id,transcribes]; _standalone_token_from_record ->
 *   [token,port,error]; guess_type -> [mime,encoding].
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral floats).
 * - Logger calls are SILENT on both sides (no trace events): the error-path
 *   logger.warning/debug side effects do not change ret/trace/state.
 * - os.name (_sidecar_pid_alive) and HTTPX_AVAILABLE (_is_timeout_error) are
 *   per-case CONFIG inputs (args.os_name default "nt", args.httpx_available
 *   default true), NOT traced seams.
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - json.loads positions (_read_runtime_record read_text, _sidecar_error_from_
 *   response text) carry CANONICAL JSON (no leading-zero numbers, no trailing
 *   commas, no bare '-'/'1.'/'1e', no invalid escapes, no raw control chars, no
 *   NaN/Infinity) plus invalid forms BOTH parsers reject ("", "notjson", "{",
 *   "[1,", "tru", '{"a":1,}'). libjson json_parse is lenient outside this domain.
 * - str()-computed slots (error/error_class/pid/token, data.get("error")) carry
 *   str/int/bool/None only — no containers, no non-integral floats.
 * - lower()/name-check text is ASCII. int() coercion (_sidecar_pid_alive pid)
 *   uses the NARROW int strip set (White_Space minus \x1c-\x1f); pid strings are
 *   ASCII digits with optional sign/whitespace, no underscores.
 * - Home paths are simple POSIX ("/h", "/", "//", "/a/b", "h", "/h/") with no
 *   '.'/'..' components and no internal double slashes (except a leading "//").
 * - SendResult / sidecar_send / sidecar_try success scripts are dicts; raise
 *   scripts carry the uniform {type,message} shape.
 * - Direct attribute reads (state._sidecar_bind/_sidecar_port/_sidecar_token/
 *   _typing_last_sent/_pending_fffc/_last_inbound_by_chat, cfg.extra,
 *   event.source) are PRESENT in fixtures (present-null models None); an absent
 *   attribute raises AttributeError live and is out of the fixture domain.
 * - _cancel_pending_fffc task entries are null or a task-id STRING (materialized
 *   to an always-truthy TaskDouble whose done()/cancel() fire task.done/task.cancel).
 * - Seam raise propagation: every consumer checks ph2_is_raise and propagates;
 *   suppress(OSError)/except(OSError,ValueError)/except(Exception) sites swallow
 *   only their family and let BaseException pass.
 *
 * PoP annotations credit plugins/platforms/photon/adapter.py per function.
 */
#include "hermes_json.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── universal seam context ── */
typedef struct ph2_ctx {
    json_t *state;      /* owned; adapter attributes, mutated in place */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} ph2_ctx_t;

/* ── exception hierarchy (FULL py3.14 builtin table; first parent) ── */
static const struct { const char *name; const char *parent; } PH2_EXC[] = {
    {"BaseException", NULL},
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"OSError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
};
static const char *ph2_exc_norm(const char *kind) {
    if (!kind) return "RuntimeError";
    /* OSError aliases + asyncio.TimeoutError alias (py3.11+) */
    if (strcmp(kind, "IOError") == 0 || strcmp(kind, "EnvironmentError") == 0
        || strcmp(kind, "WindowsError") == 0) return "OSError";
    if (strcmp(kind, "asyncio.TimeoutError") == 0) return "TimeoutError";
    return kind;
}
static const char *ph2_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(PH2_EXC) / sizeof(PH2_EXC[0]); i++)
        if (strcmp(PH2_EXC[i].name, kind) == 0) return PH2_EXC[i].parent;
    return "Exception"; /* unknown scripted kind == type(kind,(Exception,)) */
}
static bool ph2_exc_is(const char *kind, const char *ancestor) {
    const char *cur = ph2_exc_norm(kind);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, ancestor) == 0) return true;
        const char *p = ph2_exc_parent(cur);
        if (!p) return false;
        cur = p;
    }
    return false;
}
static bool ph2_is_exception(const char *k) { return ph2_exc_is(k, "Exception"); }
static bool ph2_is_oserror(const char *k) { return ph2_exc_is(k, "OSError"); }
static bool ph2_is_valueerror(const char *k) { return ph2_exc_is(k, "ValueError"); }
static bool ph2_is_permissionerror(const char *k) { return ph2_exc_is(k, "PermissionError"); }
static bool ph2_is_base_exc(const char *k) { return !ph2_is_exception(k); }

/* ── basic helpers ── */
static bool ph2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ph2_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool ph2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *ph2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool ph2_str_eq(const json_t *v, const char *s) {
    const char *t = ph2_str(v);
    return t && s && strcmp(t, s) == 0;
}
static json_t *ph2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *ph2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = ph2_err(type, msg);
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys[i];
            if (strcmp(k, "type") == 0 || strcmp(k, "message") == 0) continue;
            json_set(e, k, json_copy(spec->c.items[i]));
        }
    }
    return e;
}
/* The one universal seam. args ADOPTED; return fresh owned node. */
static json_t *ph2_call(ph2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ph2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ph2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ph2_make_exc(raise);
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
static json_t *ph2_args(void) { return json_object(); }
static void ph2_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void ph2_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void ph2_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void ph2_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void ph2_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static json_t *ph2_attr_absent(const char *k) {
    char msg[160];
    snprintf(msg, sizeof msg, "'PhotonAdapter' object has no attribute '%s'", k);
    return ph2_err("AttributeError", msg);
}
/* ── string helpers ── */
static char *ph2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static uint32_t *ph2_utf8_decode(const char *s, size_t *n) {
    if (!s) s = "";
    size_t len = strlen(s);
    uint32_t *cps = malloc((len + 1) * sizeof(uint32_t));
    size_t k = 0;
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; size_t adv;
        if (c < 0x80) { cp = c; adv = 1; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < len) {
            cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i+1] & 0x3Fu); adv = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < len) {
            cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i+1] & 0x3Fu) << 6)
               | ((unsigned char)s[i+2] & 0x3Fu); adv = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < len) {
            cp = ((c & 0x07u) << 18) | (((unsigned char)s[i+1] & 0x3Fu) << 12)
               | (((unsigned char)s[i+2] & 0x3Fu) << 6) | ((unsigned char)s[i+3] & 0x3Fu); adv = 4;
        } else { cp = 0xFFFD; adv = 1; }
        cps[k++] = cp; i += adv;
    }
    *n = k;
    return cps;
}
static void ph2_utf8_putcp(char **q, uint32_t cp) {
    char *o = *q;
    if (cp < 0x80) { *o++ = (char)cp; }
    else if (cp < 0x800) {
        *o++ = (char)(0xC0 | (cp >> 6)); *o++ = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        *o++ = (char)(0xE0 | (cp >> 12)); *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *o++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *o++ = (char)(0xF0 | (cp >> 18)); *o++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *o++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *o++ = (char)(0x80 | (cp & 0x3F));
    }
    *q = o;
}
static char *ph2_utf8_encode(const uint32_t *cps, size_t n) {
    char *o = malloc(n * 4 + 1);
    if (!o) return NULL;
    char *q = o;
    for (size_t i = 0; i < n; i++) ph2_utf8_putcp(&q, cps[i]);
    *q = '\0';
    return o;
}
static size_t ph2_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static char *ph2_utf8_slice(const char *s, size_t limit) {
    size_t n = 0;
    uint32_t *cps = ph2_utf8_decode(s, &n);
    if (limit > n) limit = n;
    char *o = ph2_utf8_encode(cps, limit);
    free(cps);
    return o;
}
/* Python str.strip() White_Space set (ASCII corpus). */
static bool ph2_is_ws_cp(uint32_t c) {
    if (c == 0x20 || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) return true;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return true;
    if (c >= 0x2000 && c <= 0x200A) return true;
    if (c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000) return true;
    return false;
}
/* int() strips a NARROWER set: White_Space minus \x1c-\x1f. */
static bool ph2_is_int_ws_cp(uint32_t c) {
    if (c >= 0x1C && c <= 0x1F) return false;
    return ph2_is_ws_cp(c);
}
static char *ph2_strip(const char *s) {
    size_t n = 0;
    uint32_t *cps = ph2_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && ph2_is_ws_cp(cps[a])) a++;
    while (b > a && ph2_is_ws_cp(cps[b-1])) b--;
    char *o = ph2_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
static char *ph2_lower_ascii(const char *s) {
    char *o = ph2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static bool ph2_contains(const char *s, const char *sub) {
    return s && sub && strstr(s, sub) != NULL;
}
/* Python str() of a scalar (null/bool/int/str). Containers / non-integral floats
 * are OUT of the fixture domain. */
static char *ph2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ph2_strdup("None");
    if (v->type == JSON_BOOL) return ph2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof buf, "%lld", (long long)d);
        else snprintf(buf, sizeof buf, "%g", d);
        return ph2_strdup(buf);
    }
    if (v->type == JSON_STRING) return ph2_strdup(v->str_val ? v->str_val : "");
    return ph2_strdup("");
}
static json_t *ph2_str_ret(char *s) {
    if (!s) return json_null();
    json_t *r = json_string(s);
    free(s);
    return r;
}
/* int() coercion. Returns 0 ok (value out), 1 TypeError, 2 ValueError.
 * pid strings: narrow-strip + optional sign + ASCII digits (no underscores). */
static int ph2_coerce_int(const json_t *v, long long *out) {
    *out = 0;
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 0; }
    if (v->type == JSON_STRING) {
        size_t n = 0;
        uint32_t *cps = ph2_utf8_decode(v->str_val, &n);
        size_t a = 0, b = n;
        while (a < b && ph2_is_int_ws_cp(cps[a])) a++;
        while (b > a && ph2_is_int_ws_cp(cps[b-1])) b--;
        bool neg = false;
        if (a < b && (cps[a] == '+' || cps[a] == '-')) { neg = cps[a] == '-'; a++; }
        if (a >= b) { free(cps); return 2; }
        long long val = 0;
        for (size_t i = a; i < b; i++) {
            if (cps[i] < '0' || cps[i] > '9') { free(cps); return 2; }
            val = val * 10 + (cps[i] - '0');
        }
        free(cps);
        *out = neg ? -val : val;
        return 0;
    }
    return 1; /* array / object -> TypeError */
}
/* ── PurePosixPath algebra (component model) ── */
/* Split into leading-slash count + components (drop empty + '.'). */
typedef struct { int leading; char **comps; size_t n; } ph2_path_t;
static ph2_path_t ph2_path_parse(const char *s) {
    ph2_path_t p; p.leading = 0; p.comps = NULL; p.n = 0;
    if (!s) s = "";
    size_t i = 0;
    while (s[i] == '/') { p.leading++; i++; }
    if (p.leading > 2) p.leading = 1; /* /// -> / ; exactly two kept */
    size_t cap = 8;
    p.comps = malloc(cap * sizeof(char *));
    while (s[i]) {
        size_t start = i;
        while (s[i] && s[i] != '/') i++;
        size_t len = i - start;
        if (len == 1 && s[start] == '.') { /* drop '.' */ }
        else if (len > 0) {
            if (p.n >= cap) { cap *= 2; p.comps = realloc(p.comps, cap * sizeof(char *)); }
            char *c = malloc(len + 1); memcpy(c, s + start, len); c[len] = '\0';
            p.comps[p.n++] = c;
        }
        while (s[i] == '/') i++;
    }
    return p;
}
static void ph2_path_free(ph2_path_t *p) {
    for (size_t i = 0; i < p->n; i++) free(p->comps[i]);
    free(p->comps);
}
static char *ph2_path_str(const ph2_path_t *p) {
    size_t total = 3;
    for (size_t i = 0; i < p->n; i++) total += strlen(p->comps[i]) + 1;
    char *o = malloc(total + 1);
    char *q = o;
    if (p->leading == 2) { *q++ = '/'; *q++ = '/'; }
    else if (p->leading == 1) { *q++ = '/'; }
    for (size_t i = 0; i < p->n; i++) {
        if (i) *q++ = '/';
        size_t l = strlen(p->comps[i]); memcpy(q, p->comps[i], l); q += l;
    }
    *q = '\0';
    if (o[0] == '\0') { free(o); return ph2_strdup(p->leading == 2 ? "//" : (p->leading ? "/" : ".")); }
    return o;
}
/* join a single component name (no slashes in fixtures). */
static char *ph2_path_join(const char *base, const char *name) {
    ph2_path_t p = ph2_path_parse(base);
    size_t cap = p.n + 2;
    char **nc = malloc(cap * sizeof(char *));
    for (size_t i = 0; i < p.n; i++) nc[i] = p.comps[i];
    nc[p.n] = ph2_strdup(name);
    ph2_path_t j; j.leading = p.leading; j.comps = nc; j.n = p.n + 1;
    char *o = ph2_path_str(&j);
    free(nc[p.n]); free(nc);
    ph2_path_free(&p);
    return o;
}
static char *ph2_path_parent(const char *s) {
    ph2_path_t p = ph2_path_parse(s);
    if (p.n > 0) p.n--; /* drop last component */
    char *o = ph2_path_str(&p);
    ph2_path_free(&p);
    return o;
}
static char *ph2_path_name(const char *s) {
    ph2_path_t p = ph2_path_parse(s);
    char *o = p.n ? ph2_strdup(p.comps[p.n - 1]) : ph2_strdup("");
    ph2_path_free(&p);
    return o;
}
/* ── json.loads (domain-restricted; libjson json_parse) ── */
static json_t *ph2_json_loads(const char *text) {
    if (!text) return NULL;
    return json_parse(text, NULL); /* NULL on grammar failure (ValueError) */
}
/* ── projections ── */
#define PH2_TARGET_NOT_ALLOWED \
    "shared/free-tier Photon lines cannot initiate outbound sends to new " \
    "targets \xe2\x80\x94 upgrade to a dedicated line or use another delivery channel"
#define PH2_RUNTIME_RECORD_NAME "photon-sidecar.json"
#define PH2_EYE_EMOJI "\xf0\x9f\x91\x80" /* U+1F440 */

static json_t *ph2_pser(const char *path, double status_code, const char *error,
                        const char *error_class, bool retryable) {
    char msg[1024];
    snprintf(msg, sizeof msg, "Photon sidecar %s returned %lld (%s, retryable=%s): %s",
             path ? path : "", (long long)status_code, error_class,
             retryable ? "True" : "False", error ? error : "");
    json_t *r = json_object();
    ph2_set_s(r, "__exc__", "PhotonSidecarError");
    ph2_set_s(r, "message", msg);
    ph2_set_s(r, "path", path ? path : "");
    ph2_set_n(r, "status_code", status_code);
    ph2_set_s(r, "error", error ? error : "");
    ph2_set_s(r, "error_class", error_class);
    ph2_set_b(r, "retryable", retryable);
    return r;
}
/* str(x or "") for a scalar slot: falsy collapses to "" (NOT "None"). */
static char *ph2_str_or_empty(const json_t *v) {
    return ph2_truthy(v) ? ph2_pystr(v) : ph2_strdup("");
}
/* data.get(k) or default -> str() (scalar domain). */
static char *ph2_get_str_or(const json_t *obj, const char *k, const char *def) {
    const json_t *v = json_obj_get(obj, k);
    if (ph2_truthy(v)) return ph2_pystr(v);
    return ph2_strdup(def);
}

/* ── A. runtime-record lifecycle (REAL) ── */
static json_t *ph2_runtime_record_path(ph2_ctx_t *ctx) {
    json_t *r = ph2_call(ctx, "get_hermes_home", ph2_args());
    if (ph2_is_raise(r)) return r;
    char *home = ph2_strdup(ph2_str(r) ? ph2_str(r) : "");
    json_free(r);
    char *p = ph2_path_join(home, PH2_RUNTIME_RECORD_NAME);
    free(home);
    return ph2_str_ret(p);
}
static json_t *ph2_legacy_runtime_record_path(ph2_ctx_t *ctx) {
    json_t *rp = ph2_runtime_record_path(ctx);
    if (ph2_is_raise(rp)) return rp;
    const char *rps = ph2_str(rp);
    char *parent = ph2_path_parent(rps);
    char *j1 = ph2_path_join(parent, "runtime");
    char *name = ph2_path_name(rps);
    char *legacy = ph2_path_join(j1, name);
    free(parent); free(j1); free(name);
    json_free(rp);
    return ph2_str_ret(legacy);
}
static json_t *ph2_current_runtime_record_path(ph2_ctx_t *ctx) {
    json_t *lp = ph2_legacy_runtime_record_path(ctx);
    if (ph2_is_raise(lp)) {
        if (ph2_is_oserror(ph2_kind(lp))) { json_free(lp); /* suppressed; skip unlink */ }
        else return lp;
    } else {
        json_t *a = ph2_args();
        ph2_set_s(a, "path", ph2_str(lp));
        ph2_set_b(a, "missing_ok", false);
        json_t *ur = ph2_call(ctx, "unlink", a);
        json_free(lp);
        if (ph2_is_raise(ur)) {
            if (ph2_is_oserror(ph2_kind(ur))) json_free(ur);
            else return ur;
        } else json_free(ur);
    }
    return ph2_runtime_record_path(ctx);
}
static json_t *ph2_write_runtime_record(ph2_ctx_t *ctx, double port, const char *token, double pid) {
    json_t *path = ph2_current_runtime_record_path(ctx);
    if (ph2_is_raise(path)) {
        if (ph2_is_exception(ph2_kind(path))) { json_free(path); return json_null(); }
        return path;
    }
    json_t *data = json_object();
    ph2_set_n(data, "port", port);
    ph2_set_s(data, "token", token);
    ph2_set_n(data, "pid", pid);
    json_t *a = ph2_args();
    ph2_set_s(a, "path", ph2_str(path));
    json_set(a, "data", data);
    ph2_set_null(a, "indent");
    ph2_set_n(a, "mode", 384); /* 0o600 */
    json_t *r = ph2_call(ctx, "atomic_json_write", a);
    json_free(path);
    if (ph2_is_raise(r)) {
        if (ph2_is_exception(ph2_kind(r))) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}
static json_t *ph2_read_runtime_record(ph2_ctx_t *ctx) {
    json_t *path = ph2_current_runtime_record_path(ctx);
    if (ph2_is_raise(path)) {
        const char *k = ph2_kind(path);
        if (ph2_is_oserror(k) || ph2_is_valueerror(k)) { json_free(path); return json_null(); }
        return path;
    }
    json_t *a = ph2_args();
    ph2_set_s(a, "path", ph2_str(path));
    ph2_set_s(a, "encoding", "utf-8-sig");
    json_t *rt = ph2_call(ctx, "read_text", a);
    json_free(path);
    if (ph2_is_raise(rt)) {
        const char *k = ph2_kind(rt);
        if (ph2_is_oserror(k) || ph2_is_valueerror(k)) { json_free(rt); return json_null(); }
        return rt;
    }
    json_t *parsed = ph2_json_loads(ph2_str(rt));
    json_free(rt);
    if (parsed && parsed->type == JSON_OBJECT) return parsed;
    if (parsed) json_free(parsed);
    return json_null();
}
static json_t *ph2_delete_runtime_record(ph2_ctx_t *ctx) {
    json_t *rp = ph2_runtime_record_path(ctx);
    if (ph2_is_raise(rp)) return rp;
    json_t *lp = ph2_legacy_runtime_record_path(ctx);
    if (ph2_is_raise(lp)) { json_free(rp); return lp; }
    const char *paths[2] = { ph2_str(rp), ph2_str(lp) };
    for (int i = 0; i < 2; i++) {
        json_t *a = ph2_args();
        ph2_set_s(a, "path", paths[i]);
        ph2_set_b(a, "missing_ok", true);
        json_t *ur = ph2_call(ctx, "unlink", a);
        if (ph2_is_raise(ur)) {
            if (ph2_is_oserror(ph2_kind(ur))) json_free(ur);
            else { json_free(rp); json_free(lp); return ur; }
        } else json_free(ur);
    }
    json_free(rp); json_free(lp);
    return json_null();
}
/* ── B. liveness / error projection (REAL) ── */
static json_t *ph2_sidecar_pid_alive(ph2_ctx_t *ctx, const json_t *pid, const char *os_name) {
    long long pid_int = 0;
    int rc = ph2_coerce_int(pid, &pid_int);
    if (rc != 0) return json_bool(false); /* TypeError / ValueError */
    if (pid_int <= 0) return json_bool(false);
    json_t *a = ph2_args();
    ph2_set_n(a, "pid", (double)pid_int);
    json_t *pe = ph2_call(ctx, "pid_exists", a);
    if (ph2_is_raise(pe)) {
        if (ph2_is_exception(ph2_kind(pe))) { json_free(pe); /* fall through */ }
        else return pe;
    } else {
        bool alive = ph2_truthy(pe);
        json_free(pe);
        return json_bool(alive);
    }
    if (strcmp(os_name, "posix") != 0) return json_bool(true);
    json_t *ka = ph2_args();
    ph2_set_n(ka, "pid", (double)pid_int);
    ph2_set_n(ka, "sig", 0);
    json_t *kr = ph2_call(ctx, "os.kill", ka);
    if (ph2_is_raise(kr)) {
        const char *k = ph2_kind(kr);
        if (ph2_is_permissionerror(k)) { json_free(kr); return json_bool(true); }
        if (ph2_is_oserror(k)) { json_free(kr); return json_bool(false); }
        return kr;
    }
    json_free(kr);
    return json_bool(true);
}
static json_t *ph2_sidecar_error_from_response(ph2_ctx_t *ctx, const char *path,
                                               double status_code, const char *text,
                                               const json_t *data_in) {
    (void)ctx;
    json_t *data = NULL;
    bool owned = false;
    if (!data_in || data_in->type == JSON_NULL) {
        json_t *parsed = ph2_json_loads(text);
        if (parsed && parsed->type == JSON_OBJECT) { data = parsed; owned = true; }
        else { if (parsed) json_free(parsed); data = json_object(); owned = true; }
    } else {
        data = (json_t *)data_in;
    }
    char *t200 = ph2_utf8_slice(text ? text : "", 200);
    /* error = str(data.get("error") or text[:200] or "sidecar error") */
    const json_t *ev = json_obj_get(data, "error");
    char *error;
    if (ph2_truthy(ev)) error = ph2_pystr(ev);
    else if (t200[0]) error = ph2_strdup(t200);
    else error = ph2_strdup("sidecar error");
    free(t200);
    char *error_class = ph2_get_str_or(data, "error_class", "sidecar_error");
    bool retryable = ph2_truthy(json_obj_get(data, "retryable"));
    if (strcmp(error_class, "target_not_allowed") == 0) {
        free(error);
        error = ph2_strdup(PH2_TARGET_NOT_ALLOWED);
        retryable = false;
    }
    json_t *r = ph2_pser(path, status_code, error, error_class, retryable);
    free(error); free(error_class);
    if (owned) json_free(data);
    return r;
}
static json_t *ph2_is_timeout_error(ph2_ctx_t *ctx, const json_t *exc, bool httpx_available) {
    (void)ctx;
    const char *kind = json_get_str(exc, "type", "RuntimeError");
    const char *norm = ph2_exc_norm(kind);
    if (strcmp(norm, "TimeoutError") == 0) return json_bool(true);
    if (httpx_available &&
        (strcmp(kind, "TimeoutException") == 0 || strcmp(kind, "ConnectTimeout") == 0
         || strcmp(kind, "ReadTimeout") == 0 || strcmp(kind, "WriteTimeout") == 0
         || strcmp(kind, "PoolTimeout") == 0))
        return json_bool(true);
    char *lk = ph2_lower_ascii(norm);
    bool hit = ph2_contains(lk, "timeout");
    free(lk);
    return json_bool(hit);
}
/* ── C. config / env enablement (REAL) ── */
static json_t *ph2_validate_config(ph2_ctx_t *ctx, const json_t *cfg) {
    const json_t *extra = json_obj_get(cfg, "extra");
    if (!ph2_truthy(extra)) extra = NULL;
    bool pid_ok;
    if (ph2_truthy(json_obj_get(extra, "project_id"))) pid_ok = true;
    else {
        json_t *a = ph2_args();
        ph2_set_s(a, "name", "PHOTON_PROJECT_ID");
        ph2_set_null(a, "default");
        json_t *r = ph2_call(ctx, "get_scoped_secret", a);
        if (ph2_is_raise(r)) return r;
        pid_ok = ph2_truthy(r);
        json_free(r);
    }
    if (pid_ok) {
        bool sec_ok;
        if (ph2_truthy(json_obj_get(extra, "project_secret"))) sec_ok = true;
        else {
            json_t *a = ph2_args();
            ph2_set_s(a, "name", "PHOTON_PROJECT_SECRET");
            ph2_set_null(a, "default");
            json_t *r = ph2_call(ctx, "get_scoped_secret", a);
            if (ph2_is_raise(r)) return r;
            sec_ok = ph2_truthy(r);
            json_free(r);
        }
        if (sec_ok) return json_bool(true);
    }
    json_t *cr = ph2_call(ctx, "load_project_credentials", ph2_args());
    if (ph2_is_raise(cr)) return cr;
    bool ok = ph2_truthy(json_get(cr, 0)) && ph2_truthy(json_get(cr, 1));
    json_free(cr);
    return json_bool(ok);
}
static json_t *ph2_is_connected(ph2_ctx_t *ctx, const json_t *cfg) {
    return ph2_validate_config(ctx, cfg);
}
static json_t *ph2_env_enablement(ph2_ctx_t *ctx) {
    json_t *cr = ph2_call(ctx, "load_project_credentials", ph2_args());
    if (ph2_is_raise(cr)) return cr;
    const json_t *id = json_get(cr, 0);
    const json_t *sec = json_get(cr, 1);
    if (!(ph2_truthy(id) && ph2_truthy(sec))) { json_free(cr); return json_null(); }
    json_t *a = ph2_args();
    json_set(a, "spec", json_array());
    ph2_set_s(a, "home_env", "PHOTON_HOME_CHANNEL");
    json_t *seed = ph2_call(ctx, "seed_extra_from_env", a);
    if (ph2_is_raise(seed)) { json_free(cr); return seed; }
    json_t *r = json_object();
    ph2_set_copy(r, "project_id", id);
    ph2_set_copy(r, "project_secret", sec);
    if (seed && seed->type == JSON_OBJECT) {
        for (size_t i = 0; i < seed->c.count; i++)
            json_set(r, seed->c.keys[i], json_copy(seed->c.items[i]));
    }
    json_free(cr); json_free(seed);
    return r;
}
/* ── D. sent-text index + mime (REAL) ── */
static json_t *ph2_sent_text_key(ph2_ctx_t *ctx, const json_t *chat_id) {
    if (!ph2_truthy(chat_id)) return json_null();
    json_t *a = ph2_args();
    ph2_set_s(a, "chat_id", ph2_str(chat_id) ? ph2_str(chat_id) : "");
    return ph2_call(ctx, "normalize_chat_key", a);
}
static json_t *ph2_record_sent_text_async(ph2_ctx_t *ctx, const json_t *chat_id,
                                          const json_t *message_id, const json_t *text) {
    json_t *key = ph2_sent_text_key(ctx, chat_id);
    if (ph2_is_raise(key)) {
        if (ph2_is_exception(ph2_kind(key))) { json_free(key); return json_null(); }
        return key;
    }
    json_t *a = ph2_args();
    ph2_set_copy(a, "chat_id", key);
    ph2_set_copy(a, "message_id", message_id);
    ph2_set_copy(a, "text", text);
    json_t *r = ph2_call(ctx, "rich_sent_store.record_async", a);
    json_free(key);
    if (ph2_is_raise(r)) {
        if (ph2_is_exception(ph2_kind(r))) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}
static json_t *ph2_lookup_sent_text(ph2_ctx_t *ctx, const json_t *chat_id, const json_t *message_id) {
    json_t *key = ph2_sent_text_key(ctx, chat_id);
    if (ph2_is_raise(key)) {
        if (ph2_is_exception(ph2_kind(key))) { json_free(key); return json_null(); }
        return key;
    }
    json_t *a = ph2_args();
    ph2_set_copy(a, "chat_id", key);
    ph2_set_copy(a, "message_id", message_id);
    json_t *r = ph2_call(ctx, "rich_sent_store.lookup", a);
    json_free(key);
    if (ph2_is_raise(r)) {
        if (ph2_is_exception(ph2_kind(r))) { json_free(r); return json_null(); }
        return r;
    }
    return r;
}
static json_t *ph2_guess_mime(ph2_ctx_t *ctx, const char *path) {
    json_t *a = ph2_args();
    ph2_set_s(a, "path", path);
    json_t *r = ph2_call(ctx, "guess_type", a);
    if (ph2_is_raise(r)) return r;
    const json_t *mime = json_get(r, 0);
    json_t *o = ph2_truthy(mime) ? json_copy(mime) : json_null();
    json_free(r);
    return o;
}
/* ── E. standalone helpers (REAL) ── */
static json_t *ph2_standalone_error(ph2_ctx_t *ctx, const json_t *resp) {
    json_t *data = json_object();
    json_t *rj = ph2_call(ctx, "resp.json", ph2_args());
    if (ph2_is_raise(rj)) {
        if (ph2_is_exception(ph2_kind(rj))) { json_free(rj); /* data stays {} */ }
        else { json_free(data); return rj; }
    } else {
        if (ph2_truthy(rj) && rj->type == JSON_OBJECT) { json_free(data); data = rj; }
        else json_free(rj);
    }
    char *error_class = ph2_get_str_or(data, "error_class", "sidecar_error");
    double status = json_get_num(resp, "status_code", 200);
    const char *text = json_get_str(resp, "text", "");
    char *error;
    if (strcmp(error_class, "target_not_allowed") == 0) {
        error = ph2_strdup(PH2_TARGET_NOT_ALLOWED);
    } else if (status != 200) {
        char *t200 = ph2_utf8_slice(text, 200);
        error = malloc(strlen(t200) + 64);
        sprintf(error, "sidecar returned %lld: %s", (long long)status, t200);
        free(t200);
    } else {
        error = ph2_get_str_or(data, "error", "sidecar reported failure");
    }
    json_t *sa = ph2_args();
    ph2_set_s(sa, "message", error);
    json_t *se = ph2_call(ctx, "send_error", sa);
    if (ph2_is_raise(se)) { free(error); free(error_class); json_free(data); return se; }
    json_t *r = json_object();
    if (se && se->type == JSON_OBJECT)
        for (size_t i = 0; i < se->c.count; i++)
            json_set(r, se->c.keys[i], json_copy(se->c.items[i]));
    ph2_set_s(r, "error_class", error_class);
    ph2_set_b(r, "retryable", false);
    free(error); free(error_class); json_free(data); json_free(se);
    return r;
}
static json_t *ph2_standalone_token_from_record(ph2_ctx_t *ctx, double port, const char *os_name) {
    json_t *record = ph2_read_runtime_record(ctx);
    if (ph2_is_raise(record)) return record;
    char *stale_hint = ph2_strdup("");
    const json_t *token = record ? json_obj_get(record, "token") : NULL;
    if (ph2_truthy(record) && ph2_truthy(token)) {
        json_t *alive = ph2_sidecar_pid_alive(ctx, json_obj_get(record, "pid"), os_name);
        if (ph2_is_raise(alive)) { free(stale_hint); json_free(record); return alive; }
        bool is_alive = ph2_truthy(alive);
        json_free(alive);
        if (is_alive) {
            json_t *ca = ph2_args();
            ph2_set_copy(ca, "value", json_obj_get(record, "port"));
            ph2_set_n(ca, "default", port);
            json_t *cp = ph2_call(ctx, "coerce_port", ca);
            if (ph2_is_raise(cp)) { free(stale_hint); json_free(record); return cp; }
            json_t *r = json_array();
            char *ts = ph2_pystr(token);
            json_append(r, json_string(ts)); free(ts);
            json_append(r, cp);
            json_append(r, json_string(""));
            free(stale_hint); json_free(record);
            return r;
        }
        free(stale_hint);
        char *ps = ph2_pystr(json_obj_get(record, "pid"));
        stale_hint = malloc(strlen(ps) + 160);
        sprintf(stale_hint,
                " A stale sidecar runtime record was found (pid %s is not running)"
                " \xe2\x80\x94 the gateway appears to be down.", ps);
        free(ps);
    }
    const char *base_err =
        "Photon standalone send requires a running sidecar. Start the Hermes gateway (which spawns "
        "the sidecar and records its address in <hermes-home>/" PH2_RUNTIME_RECORD_NAME "), "
        "or set PHOTON_SIDECAR_TOKEN in this process's environment.";
    char *error = malloc(strlen(base_err) + strlen(stale_hint) + 1);
    sprintf(error, "%s%s", base_err, stale_hint);
    json_t *r = json_array();
    json_append(r, json_null());
    json_append(r, json_number(port));
    json_append(r, json_string(error));
    free(error); free(stale_hint);
    if (record) json_free(record);
    return r;
}
/* ── F. adapter methods (REAL; state in ctx->state) ── */
static json_t *ph2_sidecar_url(ph2_ctx_t *ctx, const char *path) {
    const json_t *bind = json_obj_get(ctx->state, "_sidecar_bind");
    const json_t *port = json_obj_get(ctx->state, "_sidecar_port");
    if (!bind) return ph2_attr_absent("_sidecar_bind");
    if (!port) return ph2_attr_absent("_sidecar_port");
    char *o = malloc(strlen(ph2_str(bind)) + strlen(path) + 64);
    sprintf(o, "http://%s:%lld%s", ph2_str(bind), (long long)json_get_num(ctx->state, "_sidecar_port", 0), path);
    return ph2_str_ret(o);
}
static json_t *ph2_sidecar_headers(ph2_ctx_t *ctx) {
    const json_t *token = json_obj_get(ctx->state, "_sidecar_token");
    if (!token) return ph2_attr_absent("_sidecar_token");
    json_t *r = json_object();
    ph2_set_copy(r, "X-Hermes-Sidecar-Token", token);
    return r;
}
static json_t *ph2_note_upstream_activity(ph2_ctx_t *ctx) {
    json_t *t = ph2_call(ctx, "time.monotonic", ph2_args());
    if (ph2_is_raise(t)) return t;
    json_set(ctx->state, "_last_upstream_activity", t);
    json_set(ctx->state, "_probe_failures", json_number(0));
    return json_null();
}
static json_t *ph2_cancel_pending_fffc(ph2_ctx_t *ctx, const char *chat_key) {
    json_t *store = json_obj_get(ctx->state, "_pending_fffc");
    if (!store) return ph2_attr_absent("_pending_fffc");
    json_t *prev = json_obj_get(store, chat_key);
    json_t *prev_copy = prev ? json_copy(prev) : NULL;
    json_obj_del(store, chat_key);
    bool live = false;
    char *task_id = NULL;
    if (ph2_truthy(prev_copy)) {
        const json_t *task = json_get(prev_copy, 1);
        if (ph2_truthy(task)) {
            task_id = ph2_strdup(ph2_str(task) ? ph2_str(task) : "");
            json_t *da = ph2_args();
            ph2_set_s(da, "task", task_id);
            json_t *dr = ph2_call(ctx, "task.done", da);
            if (ph2_is_raise(dr)) { free(task_id); json_free(prev_copy); return dr; }
            live = !ph2_truthy(dr);
            json_free(dr);
        }
    }
    if (live) {
        json_t *ca = ph2_args();
        ph2_set_s(ca, "task", task_id);
        json_t *cr = ph2_call(ctx, "task.cancel", ca);
        if (ph2_is_raise(cr)) { free(task_id); json_free(prev_copy); return cr; }
        json_free(cr);
    }
    free(task_id);
    if (prev_copy) json_free(prev_copy);
    return json_bool(live);
}
static json_t *ph2_gate_stt_chat(ph2_ctx_t *ctx, const json_t *event) {
    const json_t *space = json_obj_get(event, "space");
    if (!ph2_truthy(space)) space = NULL;
    const json_t *craw = json_obj_get(event, "content");
    json_t *craw_def = NULL;
    if (!ph2_truthy(craw)) { craw_def = json_object(); craw = craw_def; }
    json_t *ca = ph2_args();
    ph2_set_copy(ca, "content", craw);
    json_t *content = ph2_call(ctx, "reply_inner", ca);
    if (ph2_is_raise(content)) { if (craw_def) json_free(craw_def); return content; }
    if (craw_def) json_free(craw_def);
    char *space_id = ph2_get_str_or(space, "id", "");
    bool transcribes = false;
    if (ph2_str_eq(json_obj_get(space, "type"), "group")
        && ph2_truthy(json_obj_get(ctx->state, "require_mention"))) {
        json_t *cva = ph2_args();
        ph2_set_copy(cva, "content", content);
        json_t *cv = ph2_call(ctx, "carries_voice", cva);
        if (ph2_is_raise(cv)) { free(space_id); json_free(content); return cv; }
        bool carries = ph2_truthy(cv);
        json_free(cv);
        if (carries) {
            const json_t *config = json_obj_get(ctx->state, "config");
            const json_t *extra = config ? json_obj_get(config, "extra") : NULL;
            json_t *va = ph2_args();
            ph2_set_copy(va, "extra", extra);
            json_t *vm = ph2_call(ctx, "voice_mention_gate_enabled", va);
            if (ph2_is_raise(vm)) { free(space_id); json_free(content); return vm; }
            transcribes = ph2_truthy(vm);
            json_free(vm);
        }
    }
    json_free(content);
    json_t *r = json_array();
    json_append(r, json_string(space_id));
    json_append(r, json_bool(transcribes));
    free(space_id);
    return r;
}
static json_t *ph2_add_reaction(ph2_ctx_t *ctx, const char *chat_id, const char *message_id,
                                const char *emoji) {
    json_t *body = json_object();
    ph2_set_s(body, "spaceId", chat_id);
    ph2_set_s(body, "messageId", message_id);
    ph2_set_s(body, "emoji", emoji);
    json_t *a = ph2_args();
    ph2_set_s(a, "path", "/react");
    json_set(a, "body", body);
    ph2_set_s(a, "what", "add_reaction");
    return ph2_call(ctx, "sidecar_try", a);
}
static json_t *ph2_remove_reaction(ph2_ctx_t *ctx, const char *chat_id, const char *message_id) {
    json_t *body = json_object();
    ph2_set_s(body, "spaceId", chat_id);
    ph2_set_s(body, "messageId", message_id);
    json_t *a = ph2_args();
    ph2_set_s(a, "path", "/unreact");
    json_set(a, "body", body);
    ph2_set_s(a, "what", "remove_reaction");
    return ph2_call(ctx, "sidecar_try", a);
}
static json_t *ph2_reaction_target(ph2_ctx_t *ctx, const json_t *chat_id, const json_t *message_id) {
    /* target = message_id or self._last_inbound_by_chat.get(self._normalize_chat_key(chat_id)) */
    if (ph2_truthy(message_id)) return json_copy(message_id);
    json_t *na = ph2_args();
    ph2_set_s(na, "chat_id", ph2_str(chat_id) ? ph2_str(chat_id) : "");
    json_t *nk = ph2_call(ctx, "normalize_chat_key", na);
    if (ph2_is_raise(nk)) return nk;
    json_t *store = json_obj_get(ctx->state, "_last_inbound_by_chat");
    if (!store) { json_free(nk); return ph2_attr_absent("_last_inbound_by_chat"); }
    const json_t *t = json_obj_get(store, ph2_str(nk) ? ph2_str(nk) : "");
    json_t *o = t ? json_copy(t) : json_null();
    json_free(nk);
    return o;
}
static json_t *ph2_add_reaction_public(ph2_ctx_t *ctx, const json_t *chat_id, const char *emoji,
                                       const json_t *message_id) {
    json_t *target = ph2_reaction_target(ctx, chat_id, message_id);
    if (ph2_is_raise(target)) return target;
    if (!ph2_truthy(target)) {
        json_free(target);
        json_t *r = json_object();
        ph2_set_b(r, "success", false);
        ph2_set_s(r, "error", "no message to react to \xe2\x80\x94 pass message_id (no "
                  "inbound message seen in this chat since the gateway started)");
        return r;
    }
    json_t *ar = ph2_add_reaction(ctx, ph2_str(chat_id), ph2_str(target), emoji);
    if (ph2_is_raise(ar)) { json_free(target); return ar; }
    bool ok = ph2_truthy(ar);
    json_free(ar);
    json_t *r = json_object();
    if (!ok) {
        ph2_set_b(r, "success", false);
        ph2_set_s(r, "error", "reaction failed (see gateway debug log)");
    } else {
        ph2_set_b(r, "success", true);
        ph2_set_copy(r, "message_id", target);
    }
    json_free(target);
    return r;
}
static json_t *ph2_remove_reaction_public(ph2_ctx_t *ctx, const json_t *chat_id, const json_t *message_id) {
    json_t *target = ph2_reaction_target(ctx, chat_id, message_id);
    if (ph2_is_raise(target)) return target;
    if (!ph2_truthy(target)) {
        json_free(target);
        json_t *r = json_object();
        ph2_set_b(r, "success", false);
        ph2_set_s(r, "error", "no message to unreact \xe2\x80\x94 pass message_id");
        return r;
    }
    json_t *rr = ph2_remove_reaction(ctx, ph2_str(chat_id), ph2_str(target));
    if (ph2_is_raise(rr)) { json_free(target); return rr; }
    bool ok = ph2_truthy(rr);
    json_free(rr);
    json_t *r = json_object();
    if (!ok) {
        ph2_set_b(r, "success", false);
        ph2_set_s(r, "error", "unreact failed (see gateway debug log)");
    } else {
        ph2_set_b(r, "success", true);
        ph2_set_copy(r, "message_id", target);
    }
    json_free(target);
    return r;
}
static json_t *ph2_on_processing_start(ph2_ctx_t *ctx, const json_t *event) {
    json_t *re = ph2_call(ctx, "reactions_enabled", ph2_args());
    if (ph2_is_raise(re)) return re;
    bool enabled = ph2_truthy(re);
    json_free(re);
    if (!enabled) return json_null();
    const json_t *source = json_obj_get(event, "source");
    const json_t *chat_id = (source && source->type == JSON_OBJECT) ? json_obj_get(source, "chat_id") : NULL;
    const json_t *message_id = json_obj_get(event, "message_id");
    if (ph2_truthy(chat_id) && ph2_truthy(message_id)) {
        json_t *ar = ph2_add_reaction(ctx, ph2_str(chat_id), ph2_str(message_id), PH2_EYE_EMOJI);
        if (ph2_is_raise(ar)) return ar;
        json_free(ar);
    }
    return json_null();
}
static json_t *ph2_get_chat_info(ph2_ctx_t *ctx, const char *chat_id) {
    (void)ctx;
    json_t *r = json_object();
    ph2_set_s(r, "name", chat_id);
    ph2_set_s(r, "type", "dm");
    ph2_set_s(r, "id", chat_id);
    return r;
}
static json_t *ph2_format_message(ph2_ctx_t *ctx, const json_t *content) {
    json_t *me = ph2_call(ctx, "markdown_enabled", ph2_args());
    if (ph2_is_raise(me)) return me;
    bool enabled = ph2_truthy(me);
    json_free(me);
    if (enabled) return content ? json_copy(content) : json_null();
    json_t *a = ph2_args();
    ph2_set_s(a, "text", ph2_str(content) ? ph2_str(content) : "");
    ph2_set_b(a, "keep_link_targets", true);
    return ph2_call(ctx, "strip_markdown", a);
}
static json_t *ph2_send_typing(ph2_ctx_t *ctx, const char *chat_id) {
    json_t *t = ph2_call(ctx, "time.time", ph2_args());
    if (ph2_is_raise(t)) return t;
    double now = (t && t->type == JSON_NUMBER) ? t->num_val : 0.0;
    json_t *store = json_obj_get(ctx->state, "_typing_last_sent");
    if (!store) { json_free(t); return ph2_attr_absent("_typing_last_sent"); }
    const json_t *last = json_obj_get(store, chat_id);
    double lastv = (last && last->type == JSON_NUMBER) ? last->num_val : 0.0;
    if (now - lastv < 5.0) { json_free(t); return json_null(); }
    json_set(store, chat_id, t);
    json_t *body = json_object();
    ph2_set_s(body, "spaceId", chat_id);
    ph2_set_s(body, "state", "start");
    json_t *a = ph2_args();
    ph2_set_s(a, "path", "/typing");
    json_set(a, "body", body);
    ph2_set_s(a, "what", "send_typing");
    json_t *r = ph2_call(ctx, "sidecar_try", a);
    if (ph2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}
static json_t *ph2_stop_typing(ph2_ctx_t *ctx, const char *chat_id) {
    json_t *store = json_obj_get(ctx->state, "_typing_last_sent");
    if (!store) return ph2_attr_absent("_typing_last_sent");
    json_obj_del(store, chat_id);
    json_t *body = json_object();
    ph2_set_s(body, "spaceId", chat_id);
    ph2_set_s(body, "state", "stop");
    json_t *a = ph2_args();
    ph2_set_s(a, "path", "/typing");
    json_set(a, "body", body);
    ph2_set_s(a, "what", "stop_typing");
    json_t *r = ph2_call(ctx, "sidecar_try", a);
    if (ph2_is_raise(r)) return r;
    json_free(r);
    return json_null();
}
static json_t *ph2_send_plain_fallback(ph2_ctx_t *ctx, const char *chat_id, const json_t *content) {
    json_t *fm = ph2_format_message(ctx, content);
    if (ph2_is_raise(fm)) return fm;
    double maxl = json_get_num(ctx->state, "MAX_MESSAGE_LENGTH", 8000);
    char *text = ph2_utf8_slice(ph2_str(fm) ? ph2_str(fm) : "", (size_t)maxl);
    json_free(fm);
    json_t *a = ph2_args();
    ph2_set_s(a, "space_id", chat_id);
    ph2_set_s(a, "text", text);
    ph2_set_b(a, "richlink", false);
    ph2_set_b(a, "markdown", false);
    json_t *r = ph2_call(ctx, "sidecar_send", a);
    free(text);
    return r;
}
/* ── case-target wrappers (uniform ph2__name(ctx, args) signature) ── */
static const char *ph2_arg_str(const json_t *a, const char *k, const char *def) {
    return json_get_str(a, k, def);
}
static const json_t *ph2_arg_node(const json_t *a, const char *k) {
    return json_obj_get(a, k);
}
static bool ph2_arg_bool(const json_t *a, const char *k, bool def) {
    return json_get_bool(a, k, def);
}
static double ph2_arg_num(const json_t *a, const char *k, double def) {
    return json_get_num(a, k, def);
}
/* PoP: ph2__runtime_record_path @ plugins/platforms/photon/adapter.py:_runtime_record_path */
static json_t *ph2__runtime_record_path(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_runtime_record_path(ctx);
}
/* PoP: ph2__legacy_runtime_record_path @ plugins/platforms/photon/adapter.py:_legacy_runtime_record_path */
static json_t *ph2__legacy_runtime_record_path(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_legacy_runtime_record_path(ctx);
}
/* PoP: ph2__current_runtime_record_path @ plugins/platforms/photon/adapter.py:_current_runtime_record_path */
static json_t *ph2__current_runtime_record_path(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_current_runtime_record_path(ctx);
}
/* PoP: ph2__write_runtime_record @ plugins/platforms/photon/adapter.py:_write_runtime_record */
static json_t *ph2__write_runtime_record(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_write_runtime_record(ctx, ph2_arg_num(args, "port", 0),
                                    ph2_arg_str(args, "token", ""), ph2_arg_num(args, "pid", 0));
}
/* PoP: ph2__read_runtime_record @ plugins/platforms/photon/adapter.py:_read_runtime_record */
static json_t *ph2__read_runtime_record(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_read_runtime_record(ctx);
}
/* PoP: ph2__delete_runtime_record @ plugins/platforms/photon/adapter.py:_delete_runtime_record */
static json_t *ph2__delete_runtime_record(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_delete_runtime_record(ctx);
}
/* PoP: ph2__sidecar_pid_alive @ plugins/platforms/photon/adapter.py:_sidecar_pid_alive */
static json_t *ph2__sidecar_pid_alive(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_sidecar_pid_alive(ctx, ph2_arg_node(args, "pid"), ph2_arg_str(args, "os_name", "nt"));
}
/* PoP: ph2__sidecar_error_from_response @ plugins/platforms/photon/adapter.py:_sidecar_error_from_response */
static json_t *ph2__sidecar_error_from_response(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_sidecar_error_from_response(ctx, ph2_arg_str(args, "path", ""),
                                           ph2_arg_num(args, "status_code", 0),
                                           ph2_arg_str(args, "text", ""),
                                           ph2_arg_node(args, "data"));
}
/* PoP: ph2__is_timeout_error @ plugins/platforms/photon/adapter.py:_is_timeout_error */
static json_t *ph2__is_timeout_error(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_is_timeout_error(ctx, ph2_arg_node(args, "exc"), ph2_arg_bool(args, "httpx_available", true));
}
/* PoP: ph2__validate_config @ plugins/platforms/photon/adapter.py:validate_config */
static json_t *ph2__validate_config(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_validate_config(ctx, ph2_arg_node(args, "cfg"));
}
/* PoP: ph2__is_connected @ plugins/platforms/photon/adapter.py:is_connected */
static json_t *ph2__is_connected(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_is_connected(ctx, ph2_arg_node(args, "cfg"));
}
/* PoP: ph2__env_enablement @ plugins/platforms/photon/adapter.py:_env_enablement */
static json_t *ph2__env_enablement(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_env_enablement(ctx);
}
/* PoP: ph2__sent_text_key @ plugins/platforms/photon/adapter.py:_sent_text_key */
static json_t *ph2__sent_text_key(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_sent_text_key(ctx, ph2_arg_node(args, "chat_id"));
}
/* PoP: ph2__record_sent_text_async @ plugins/platforms/photon/adapter.py:_record_sent_text_async */
static json_t *ph2__record_sent_text_async(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_record_sent_text_async(ctx, ph2_arg_node(args, "chat_id"),
                                      ph2_arg_node(args, "message_id"), ph2_arg_node(args, "text"));
}
/* PoP: ph2__lookup_sent_text @ plugins/platforms/photon/adapter.py:_lookup_sent_text */
static json_t *ph2__lookup_sent_text(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_lookup_sent_text(ctx, ph2_arg_node(args, "chat_id"), ph2_arg_node(args, "message_id"));
}
/* PoP: ph2__guess_mime @ plugins/platforms/photon/adapter.py:_guess_mime */
static json_t *ph2__guess_mime(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_guess_mime(ctx, ph2_arg_str(args, "path", ""));
}
/* PoP: ph2__standalone_error @ plugins/platforms/photon/adapter.py:_standalone_error */
static json_t *ph2__standalone_error(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_standalone_error(ctx, ph2_arg_node(args, "resp"));
}
/* PoP: ph2__standalone_token_from_record @ plugins/platforms/photon/adapter.py:_standalone_token_from_record */
static json_t *ph2__standalone_token_from_record(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_standalone_token_from_record(ctx, ph2_arg_num(args, "port", 8789),
                                            ph2_arg_str(args, "os_name", "nt"));
}
/* PoP: ph2__sidecar_url @ plugins/platforms/photon/adapter.py:_sidecar_url */
static json_t *ph2__sidecar_url(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_sidecar_url(ctx, ph2_arg_str(args, "path", ""));
}
/* PoP: ph2__sidecar_headers @ plugins/platforms/photon/adapter.py:_sidecar_headers */
static json_t *ph2__sidecar_headers(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_sidecar_headers(ctx);
}
/* PoP: ph2__note_upstream_activity @ plugins/platforms/photon/adapter.py:_note_upstream_activity */
static json_t *ph2__note_upstream_activity(ph2_ctx_t *ctx, const json_t *args) {
    (void)args; return ph2_note_upstream_activity(ctx);
}
/* PoP: ph2__cancel_pending_fffc @ plugins/platforms/photon/adapter.py:_cancel_pending_fffc */
static json_t *ph2__cancel_pending_fffc(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_cancel_pending_fffc(ctx, ph2_arg_str(args, "chat_key", ""));
}
/* PoP: ph2__gate_stt_chat @ plugins/platforms/photon/adapter.py:_gate_stt_chat */
static json_t *ph2__gate_stt_chat(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_gate_stt_chat(ctx, ph2_arg_node(args, "event"));
}
/* PoP: ph2__add_reaction @ plugins/platforms/photon/adapter.py:_add_reaction */
static json_t *ph2__add_reaction(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_add_reaction(ctx, ph2_arg_str(args, "chat_id", ""), ph2_arg_str(args, "message_id", ""),
                            ph2_arg_str(args, "emoji", ""));
}
/* PoP: ph2__remove_reaction @ plugins/platforms/photon/adapter.py:_remove_reaction */
static json_t *ph2__remove_reaction(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_remove_reaction(ctx, ph2_arg_str(args, "chat_id", ""), ph2_arg_str(args, "message_id", ""));
}
/* PoP: ph2__add_reaction_public @ plugins/platforms/photon/adapter.py:add_reaction */
static json_t *ph2__add_reaction_public(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_add_reaction_public(ctx, ph2_arg_node(args, "chat_id"), ph2_arg_str(args, "emoji", ""),
                                   ph2_arg_node(args, "message_id"));
}
/* PoP: ph2__remove_reaction_public @ plugins/platforms/photon/adapter.py:remove_reaction */
static json_t *ph2__remove_reaction_public(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_remove_reaction_public(ctx, ph2_arg_node(args, "chat_id"), ph2_arg_node(args, "message_id"));
}
/* PoP: ph2__on_processing_start @ plugins/platforms/photon/adapter.py:on_processing_start */
static json_t *ph2__on_processing_start(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_on_processing_start(ctx, ph2_arg_node(args, "event"));
}
/* PoP: ph2__get_chat_info @ plugins/platforms/photon/adapter.py:get_chat_info */
static json_t *ph2__get_chat_info(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_get_chat_info(ctx, ph2_arg_str(args, "chat_id", ""));
}
/* PoP: ph2__format_message @ plugins/platforms/photon/adapter.py:format_message */
static json_t *ph2__format_message(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_format_message(ctx, ph2_arg_node(args, "content"));
}
/* PoP: ph2__send_typing @ plugins/platforms/photon/adapter.py:send_typing */
static json_t *ph2__send_typing(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_send_typing(ctx, ph2_arg_str(args, "chat_id", ""));
}
/* PoP: ph2__stop_typing @ plugins/platforms/photon/adapter.py:stop_typing */
static json_t *ph2__stop_typing(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_stop_typing(ctx, ph2_arg_str(args, "chat_id", ""));
}
/* PoP: ph2__send_plain_fallback @ plugins/platforms/photon/adapter.py:_send_plain_fallback */
static json_t *ph2__send_plain_fallback(ph2_ctx_t *ctx, const json_t *args) {
    return ph2_send_plain_fallback(ctx, ph2_arg_str(args, "chat_id", ""), ph2_arg_node(args, "content"));
}