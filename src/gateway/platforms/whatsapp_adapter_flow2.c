/*
 * whatsapp_adapter_flow2.c -- WhatsApp adapter bridge session/token/auth/env
 * FLOW remainder of plugins/platforms/whatsapp/adapter.py (hermes-agent).
 *
 * Ports the DECISION SEQUENCES of the UNCLAIMED second-wave cluster (the 30
 * bridge-lifecycle siblings live in whatsapp_adapter_flow.c and are SEAMED
 * here): the session-dir resolution, owner-only log open/rotate, the
 * process-tree terminate flow (Windows taskkill / POSIX psutil), the
 * cache-dir path allowlist, the crash-released token file lock (msvcrt /
 * fcntl branches) and the mint-under-lock token flow, the aiohttp import
 * gate + ensure_aiohttp PM retry, the _needs_bridge guard decorator, the
 * challenge/proof authenticated health probe, the bridge spawn env builder,
 * adoption fingerprint match, bridge-died / close-log / terminate /
 * managed-exit lifecycle methods, document text injection, quoted-media
 * resolution, the POST send wrapper, the YAML config bridge and the two
 * inbound gate/log leaves. Call order, short-circuit branches, suppress()
 * regions, CM enter/exit ordering, early returns and exception propagation
 * mirror the live Python.
 *
 * Universal seam: every SEAMED sibling call enters through ONE callback
 * wa2_call(ctx, name, args). The harness wires ctx->io to a recording stub
 * fed the case's scripted results; the oracle installs recording stubs
 * emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL == Python None; {"__raise__":cls,"message":str(exc)} == a
 * raised exception). A missing/exhausted script is a fail-closed
 * RuntimeError on both sides. BaseException kinds (SystemExit /
 * KeyboardInterrupt / CancelledError / GeneratorExit) pass through EVERY
 * suppress()/except site (full builtin exception-parent table below).
 *
 * Mode table (how a function is invoked BY ITS CALLERS):
 *  REAL-ALWAYS (run inline / passed as a value, never seamed):
 *    _owner_only_opener (only passed as open(opener=) value), _bridge_url
 *    (transcribed inline in _bridge_req; unclaimed leaf, no PoP re-claim).
 *  REAL-TARGET / SEAMED-CALLER (wa2_call when called by another ported
 *    function; real body when the case target): _session_dir,
 *    _open_bridge_log, _terminate_bridge_process, _is_allowed_bridge_path,
 *    _bridge_token_lock, _load_or_create_bridge_token, _import_aiohttp,
 *    ensure_aiohttp, _needs_bridge, _bridge_req, _authenticated_bridge_health,
 *    _bridge_package_hash, _bridge_env, _adopted_config_matches, _bridge_died,
 *    _close_bridge_log, _terminate_bridge, _check_managed_bridge_exit,
 *    _inject_document_text, _quoted_media, _post_bridge_message,
 *    _apply_yaml_config, _may_transcribe_at_gate, _log_delivery_status.
 *  SEAMED claimed flow1 siblings: _wenv, _cache_dirs, _file_content_hash,
 *    _bridge_config_fingerprint, _bridge_auth_proof, _bridge_auth_headers.
 *  SEAMED foreign calls: get_hermes_dir, expanduser / abspath (os.path),
 *    os.open / os.fchmod / os.replace / os.unlink / os.fdopen / os.fsync,
 *    open (builtin), stat / read_text / mkdir / resolve / is_relative_to
 *    (PathDouble), handle.seek / handle.write / handle.flush /
 *    handle.fileno / handle.close, fh.close, msvcrt.locking, fcntl.flock,
 *    secrets.token_urlsafe, tempfile.mkstemp, proc.poll / proc.kill /
 *    proc.terminate, psutil.Process / psutil.children / child.kill /
 *    child.terminate, subprocess.run, print, logger.info / logger.error,
 *    voice_mention_gate_enabled, _apply_yaml_bridge, hermes_subprocess_env,
 *    with_hermes_node_path, repo_root, lazy_installs_allowed,
 *    running_from_selected_environment, restart_needed, ensure_import,
 *    ClientTimeout, session.<method>, resp.enter / resp.json / resp.text /
 *    resp.exit, token_lock.enter / token_lock.exit, method (the wrapped
 *    coroutine), _bridge_unavailable, _set_fatal_error, _notify_fatal_error,
 *    lookup_media (gateway.rich_sent_store).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and wa2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_whatsapp_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 *   json_set(obj,k,NULL) is a no-op in libjson, so None is set with json_null().
 * - A raise is {"__raise__": cls, "message": str(exc)}.
 * - Paths are NORMALIZED POSIX strings (PurePosixPath lexical model: "."
 *   components dropped, exactly-two leading slashes kept, trailing slash
 *   removed except root, "" -> "."); the oracle drives PathDouble
 *   (PurePosixPath) doubles. resolve()/is_relative_to() are SEAMS (no real
 *   filesystem); os.path is posixpath (isabs = leading "/").
 * - read_text seams use encoding="utf-8-sig": one leading BOM (U+FEFF) is
 *   stripped from the scripted string on BOTH sides; the errors kwarg rides
 *   the args (null when the live call omits it).
 * - Attribute doubles (proc / stat result / subprocess.run result / handles /
 *   session) are JSON objects whose keys are the live attributes; a missing
 *   key models a missing attribute (AttributeError with the py3.14
 *   'types.SimpleNamespace' prefix); null models None ('NoneType').
 *   proc doubles carry pid only; handle doubles project back to their
 *   scripted spec in echoes ({"__handle__": spec} internally).
 * - _IS_WINDOWS / os.name / hasattr(os,"fchmod") are fixture-driven
 *   (args.is_windows / args.os_name / args.has_fchmod). Constants modeled:
 *   SEEK_END=2, msvcrt.LK_LOCK=1 / LK_UNLCK=0, fcntl.LOCK_EX=2 / LOCK_UN=8,
 *   PASSKEY_REQUIRED_EXIT=78, _BRIDGE_LOG_ROTATE_BYTES=5242880,
 *   _MAX_TEXT_INJECT_BYTES=102400, subprocess.DEVNULL=-3.
 * - SendResult projects to its full dataclass field dict (success,
 *   message_id, error, raw_response, retryable, retry_after,
 *   continuation_message_ids, error_kind); the empty tuple () -> [].
 * - The aiohttp module double projects to {"__module__":"aiohttp"};
 *   ClientTimeout results ride verbatim as the timeout seam arg.
 * - Method targets mutate ctx.state["self"] (materialized from args.self by
 *   the harness); out.state echoes ONLY the fixture-present self keys, on
 *   success AND on raise (partial mutation history). Module targets emit {}.
 * - Integral floats == ints (libjson %.0f; oracle canon folds integral
 *   floats to int). Tuples project to lists. bytes args (handle.write of
 *   b"\0") project to {"__bytes__":[0]} (libjson strings cannot embed NUL).
 *
 * Fixture corpus restrictions (documented; generator honours them):
 * - str()-computed positions carry str / int / bool / None only -- no
 *   containers, no non-integral floats (Python str(5.0)=="5.0" vs C "5").
 * - int values fit |v| < 2^53; ASCII-only in sorted()/lower()/path positions.
 * - auth proof / challenge / token strings are ASCII (compare_digest is
 *   byte equality; surrogatepass encodes are out of domain).
 * - _bridge_auth_proof seam results are str (the .encode() AttributeError
 *   guard is fail-closed parity, not a fixture family).
 * - resp.exit scripts are null on body-raise cases (exit-suppress of a body
 *   raise leaves live locals unbound -- out of domain).
 * - tempfile.mkstemp results are [fd, tmp] pairs; _cache_dirs results are
 *   4-element path lists; extra / config / data carriers are objects|null.
 * - logger format args carry the raw values (no %s/%d transformation).
 * - _import_aiohttp aiohttp.__path__ is a str list or absent (present-null
 *   would TypeError in list() -- out of domain).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct wa2_ctx {
    json_t *state;      /* owned; {"self": ...} for method targets, {} else */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} wa2_ctx_t;

/* -- basic helpers -- */
static bool wa2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *wa2_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool wa2_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "CancelledError") == 0 || strcmp(kind, "GeneratorExit") == 0;
}
/* FULL builtin exception-parent hierarchy (py3.14) + the stub families the
 * oracle registers (psutil.NoSuchProcess, aiohttp.ClientError tree,
 * subprocess.SubprocessError tree). Walk parents until base matches.
 * SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError parent ONLY to
 * BaseException, so they pass through every except Exception site. */
typedef struct { const char *kind; const char *parent; } wa2_exc_parent_t;
static const wa2_exc_parent_t WA2_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"ZipImportError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    /* py3.14 aliases / groups / finalization */
    {"IOError", "OSError"},
    {"EnvironmentError", "OSError"},
    {"WindowsError", "OSError"},
    {"ExceptionGroup", "Exception"},
    {"BaseExceptionGroup", "BaseException"},
    {"PythonFinalizationError", "RuntimeError"},
    {"_IncompleteInputError", "SyntaxError"},
    /* stub families (oracle-registered classes) */
    {"NoSuchProcess", "Exception"},
    {"SubprocessError", "Exception"},
    {"TimeoutExpired", "SubprocessError"},
    {"CalledProcessError", "SubprocessError"},
    {"ClientError", "Exception"},
    {"ClientConnectorError", "ClientError"},
    {"ClientResponseError", "ClientError"},
    {"ServerTimeoutError", "ClientError"},
    {"ClientPayloadError", "ClientError"},
};
static bool wa2_kind_is(const char *kind, const char *base) {
    if (!kind || !base) return false;
    if (strcmp(kind, base) == 0) return true;
    char cur[128];
    snprintf(cur, sizeof cur, "%s", kind);
    for (int hops = 0; hops < 16; hops++) {
        const char *parent = NULL;
        for (size_t i = 0; i < sizeof(WA2_EXC_PARENTS) / sizeof(WA2_EXC_PARENTS[0]); i++) {
            if (strcmp(WA2_EXC_PARENTS[i].kind, cur) == 0) { parent = WA2_EXC_PARENTS[i].parent; break; }
        }
        if (!parent) return false;
        if (strcmp(parent, base) == 0) return true;
        snprintf(cur, sizeof cur, "%s", parent);
    }
    return false;
}
/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool wa2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *wa2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *wa2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static json_t *wa2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return wa2_err(type, msg);
}
/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *wa2_call(wa2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = wa2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = wa2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = wa2_make_exc(raise);
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
static void wa2_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void wa2_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void wa2_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void wa2_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void wa2_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* -- string / utf-8 helpers -- */
static char *wa2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static uint32_t *wa2_utf8_decode(const char *s, size_t *n) {
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
static void wa2_utf8_putcp(char **q, uint32_t cp) {
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
static char *wa2_utf8_encode(const uint32_t *cps, size_t n) {
    char *o = malloc(n * 4 + 1);
    if (!o) return NULL;
    char *q = o;
    for (size_t i = 0; i < n; i++) wa2_utf8_putcp(&q, cps[i]);
    *q = '\0';
    return o;
}
/* Python str.strip() White_Space set. */
static bool wa2_is_ws_cp(uint32_t c) {
    if (c == 0x20 || (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x1F)) return true;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return true;
    if (c >= 0x2000 && c <= 0x200A) return true;
    if (c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000) return true;
    return false;
}
static char *wa2_strip(const char *s) {
    size_t n = 0;
    uint32_t *cps = wa2_utf8_decode(s, &n);
    size_t a = 0, b = n;
    while (a < b && wa2_is_ws_cp(cps[a])) a++;
    while (b > a && wa2_is_ws_cp(cps[b-1])) b--;
    char *o = wa2_utf8_encode(cps + a, b - a);
    free(cps);
    return o;
}
/* One leading U+FEFF BOM strip (utf-8-sig read_text seam contract). */
static char *wa2_bom_strip(const char *s) {
    if (!s) s = "";
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        return wa2_strdup(s + 3);
    return wa2_strdup(s);
}
/* ASCII lower (fixture corpus for .lower() positions is ASCII). */
static char *wa2_lower_ascii(const char *s) {
    char *o = wa2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
/* str.split(delim, maxsplit) -- maxsplit < 0 == unlimited. */
static char **wa2_split_char(const char *s, char delim, int maxsplit, size_t *n) {
    if (!s) s = "";
    size_t len = strlen(s), cap = 8, k = 0;
    char **parts = malloc(cap * sizeof(char *));
    size_t start = 0;
    int splits = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == delim && (maxsplit < 0 || splits < maxsplit)) {
            if (k == cap) { cap *= 2; parts = realloc(parts, cap * sizeof(char *)); }
            parts[k] = malloc(i - start + 1);
            memcpy(parts[k], s + start, i - start);
            parts[k][i - start] = '\0';
            k++; splits++; start = i + 1;
        }
    }
    if (k == cap) { cap *= 2; parts = realloc(parts, cap * sizeof(char *)); }
    parts[k] = wa2_strdup(s + start);
    k++;
    *n = k;
    return parts;
}
static void wa2_free_parts(char **parts, size_t n) {
    for (size_t i = 0; i < n; i++) free(parts[i]);
    free(parts);
}
/* -- lexical PurePosixPath model -- */
/* Normalize: drop ""/"." components, keep exactly-two leading slashes,
 * no trailing slash except root, empty -> ".". */
static char *wa2_posix_norm(const char *p) {
    if (!p) p = "";
    size_t len = strlen(p);
    size_t lead = 0;
    while (lead < len && p[lead] == '/') lead++;
    const char *root = (lead == 2) ? "//" : (lead >= 1 ? "/" : "");
    size_t cap = len + 4, k = 0;
    char *o = malloc(cap);
    k += (size_t)snprintf(o + k, cap - k, "%s", root);
    size_t i = lead;
    bool first = true;
    while (i < len) {
        size_t j = i;
        while (j < len && p[j] != '/') j++;
        if (j > i && !(j - i == 1 && p[i] == '.')) {
            if (!first) o[k++] = '/';
            memcpy(o + k, p + i, j - i);
            k += j - i;
            first = false;
        }
        i = j;
        while (i < len && p[i] == '/') i++;
    }
    if (first && root[0] == '\0') { o[k++] = '.'; }
    o[k] = '\0';
    return o;
}
/* PurePosixPath.name: last component ("" for root AND "."). Input normalized. */
static char *wa2_path_name(const char *p) {
    char *n = wa2_posix_norm(p);
    size_t len = strlen(n);
    if (len == 0) { free(n); return wa2_strdup(""); }
    if (strcmp(n, "/") == 0 || strcmp(n, "//") == 0 || strcmp(n, ".") == 0) { free(n); return wa2_strdup(""); }
    size_t start = len;
    while (start > 0 && n[start-1] != '/') start--;
    char *o = wa2_strdup(n + start);
    free(n);
    return o;
}
/* PurePosixPath.parent: drop the last component (root stays root,
 * "a" -> "."). Input normalized. */
static char *wa2_path_parent(const char *p) {
    char *n = wa2_posix_norm(p);
    size_t len = strlen(n);
    if (strcmp(n, "/") == 0 || strcmp(n, "//") == 0 || strcmp(n, ".") == 0) return n;
    size_t end = len;
    while (end > 0 && n[end-1] != '/') end--;
    if (end == 0) { free(n); return wa2_strdup("."); }
    while (end > 1 && n[end-1] == '/') end--;
    if (end == 1 && n[0] == '/') { n[1] = '\0'; return n; }
    if (end == 2 && n[0] == '/' && n[1] == '/') { n[2] = '\0'; return n; }
    n[end] = '\0';
    return n;
}
/* PurePosixPath.with_name: replace the last component (root -> ValueError
 * out of domain; fixture paths always carry a name). The result is
 * normalized ("." parents drop: with_name("b","c") == "c"). */
static char *wa2_with_name(const char *p, const char *name) {
    char *par = wa2_path_parent(p);
    size_t pl = strlen(par);
    char *o = malloc(pl + strlen(name) + 2);
    memcpy(o, par, pl);
    size_t k = pl;
    if (pl > 0 && par[pl-1] != '/') o[k++] = '/';
    strcpy(o + k, name);
    free(par);
    char *n = wa2_posix_norm(o);
    free(o);
    return n;
}
/* PurePosixPath join (base may be unnormalized; result normalized). */
static char *wa2_path_join(const char *base, const char *name) {
    if (!base) base = "";
    size_t bl = strlen(base);
    char *o = malloc(bl + strlen(name) + 2);
    memcpy(o, base, bl);
    size_t k = bl;
    if (k > 0 && o[k-1] != '/') o[k++] = '/';
    strcpy(o + k, name);
    char *n = wa2_posix_norm(o);
    free(o);
    return n;
}
/* PurePosixPath.suffix: extension of the name incl dot ("" when none). */
static char *wa2_path_suffix(const char *p) {
    char *name = wa2_path_name(p);
    size_t len = strlen(name);
    size_t dot = len;
    while (dot > 0 && name[dot-1] != '.') dot--;
    if (dot == 0 || dot == len) { free(name); return wa2_strdup(""); }
    /* leading dots of the name are not a suffix (".hidden" -> "") */
    size_t first_non_dot = 0;
    while (first_non_dot < len && name[first_non_dot] == '.') first_non_dot++;
    if (dot - 1 < first_non_dot) { free(name); return wa2_strdup(""); }
    char *o = wa2_strdup(name + dot - 1);
    free(name);
    return o;
}
/* Python str() projection for the fixture domain (str/int/bool/None). */
static char *wa2_str_proj(const json_t *v) {
    if (!v || v->type == JSON_NULL) return wa2_strdup("None");
    if (v->type == JSON_BOOL) return wa2_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof buf, "%.0f", v->num_val);
        return wa2_strdup(buf);
    }
    if (v->type == JSON_STRING) return wa2_strdup(v->str_val ? v->str_val : "");
    return wa2_strdup("");  /* containers out of domain */
}
/* Python type name for error messages (integral double -> int). */
static const char *wa2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.0e15) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* Attribute read on a scripted double: borrowed node, or *err set to the
 * exact py3.14 AttributeError (null -> NoneType, object missing key ->
 * types.SimpleNamespace, other scalar -> its type name). */
static const json_t *wa2_attr(const json_t *obj, const char *field, json_t **err) {
    *err = NULL;
    if (!obj || obj->type == JSON_NULL) {
        char msg[256];
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", field);
        *err = wa2_err("AttributeError", msg);
        return NULL;
    }
    if (obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, field);
        if (!v) {
            char msg[256];
            snprintf(msg, sizeof msg, "'types.SimpleNamespace' object has no attribute '%s'", field);
            *err = wa2_err("AttributeError", msg);
            return NULL;
        }
        return v;
    }
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", wa2_type_name(obj), field);
    *err = wa2_err("AttributeError", msg);
    return NULL;
}
/* Strict Python == for the fixture domain (int/float/str/bool/null). */
static bool wa2_py_eq(const json_t *a, const json_t *b) {
    bool an = (!a || a->type == JSON_NULL), bn = (!b || b->type == JSON_NULL);
    if (an || bn) return an && bn;
    bool anum = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    bool bnum = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (anum && bnum) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return false;
}
/* Python list repr for a str-list (the _import_aiohttp __path__ branch).
 * repr quote rule: double quotes when the string contains ' but no ";
 * escape backslash and the chosen quote char. Printable-ASCII domain. */
static char *wa2_list_repr(const json_t *arr) {
    size_t cap = 64, k = 0;
    char *o = malloc(cap);
    o[k++] = '[';
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *it = json_get(arr, i);
        const char *s = wa2_str(it);
        if (!s) s = "";
        bool has_sq = strchr(s, '\'') != NULL;
        bool has_dq = strchr(s, '"') != NULL;
        char q = (has_sq && !has_dq) ? '"' : '\'';
        size_t need = strlen(s) * 2 + 8;
        while (k + need >= cap) { cap *= 2; o = realloc(o, cap); }
        if (i) { o[k++] = ','; o[k++] = ' '; }
        o[k++] = q;
        for (const char *p = s; *p; p++) {
            if (*p == '\\' || *p == q) o[k++] = '\\';
            o[k++] = *p;
        }
        o[k++] = q;
    }
    o[k++] = ']';
    o[k] = '\0';
    return o;
}
/* sorted(ids) joined with "," -- ASCII str corpus (strcmp == codepoint). */
static int wa2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static char *wa2_sorted_join(const json_t *ids) {
    size_t n = json_len(ids);
    char **parts = malloc((n ? n : 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) parts[i] = wa2_strdup(wa2_str(json_get(ids, i)));
    qsort(parts, n, sizeof(char *), wa2_cmp_str);
    size_t cap = 64, k = 0;
    char *o = malloc(cap);
    o[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        size_t need = strlen(parts[i]) + 2;
        while (k + need >= cap) { cap *= 2; o = realloc(o, cap); }
        if (i) o[k++] = ',';
        strcpy(o + k, parts[i]);
        k += strlen(parts[i]);
    }
    o[k] = '\0';
    wa2_free_parts(parts, n);
    return o;
}
/* SendResult full-field projection (dataclass field order). */
static json_t *wa2_send_result(bool success, const json_t *message_id,
                               const json_t *error, const json_t *raw_response) {
    json_t *r = json_object();
    wa2_set_b(r, "success", success);
    json_set(r, "message_id", message_id ? json_copy(message_id) : json_null());
    json_set(r, "error", error ? json_copy(error) : json_null());
    json_set(r, "raw_response", raw_response ? json_copy(raw_response) : json_null());
    wa2_set_b(r, "retryable", false);
    wa2_set_null(r, "retry_after");
    json_set(r, "continuation_message_ids", json_array());
    wa2_set_null(r, "error_kind");
    return r;
}
/* Borrowed self object for method targets (harness materializes it). */
static json_t *wa2_self(wa2_ctx_t *ctx) {
    json_t *s = json_obj_get(ctx->state, "self");
    return (s && s->type == JSON_OBJECT) ? s : NULL;
}
/* CM exit resolution: fire the exit seam with the pending kind; an exit
 * raise overrides, a truthy exit ret suppresses the pending raise.
 * Returns the final raise (owned) or NULL; *suppressed flags a swallow. */
static json_t *wa2_cm_exit(wa2_ctx_t *ctx, const char *name, json_t *pending, bool *suppressed) {
    *suppressed = false;
    json_t *a = json_object();
    if (pending) wa2_set_s(a, "exc", wa2_kind(pending));
    else wa2_set_null(a, "exc");
    json_t *x = wa2_call(ctx, name, a);
    if (wa2_is_raise(x)) {
        if (pending) json_free(pending);
        return x;
    }
    if (pending && wa2_truthy(x)) {
        json_free(pending);
        json_free(x);
        *suppressed = true;
        return NULL;
    }
    json_free(x);
    return pending;
}

/* -- module-level flows -- */

/* PoP: wa2__session_dir @ plugins/platforms/whatsapp/adapter.py:_session_dir */
static json_t *wa2__session_dir(wa2_ctx_t *ctx, const json_t *args) {
    const json_t *extra = json_obj_get(args, "extra");
    /* .get default evaluates ALWAYS (before the .get call). */
    json_t *ga = json_object();
    wa2_set_s(ga, "name", "platforms/whatsapp/session");
    wa2_set_s(ga, "fallback", "whatsapp/session");
    json_t *gh = wa2_call(ctx, "get_hermes_dir", ga);
    if (wa2_is_raise(gh)) return gh;
    const json_t *val = NULL;
    if (wa2_truthy(extra) && extra->type == JSON_OBJECT)
        val = json_obj_get(extra, "session_path");
    if (!val) val = gh;
    char *s = wa2_str_proj(val);
    json_t *ea = json_object();
    wa2_set_s(ea, "path", s);
    free(s);
    json_t *eu = wa2_call(ctx, "expanduser", ea);
    if (wa2_is_raise(eu)) { json_free(gh); return eu; }
    json_t *aa = json_object();
    wa2_set_s(aa, "path", wa2_str(eu));
    json_t *ab = wa2_call(ctx, "abspath", aa);
    json_free(eu);
    json_free(gh);
    if (wa2_is_raise(ab)) return ab;
    char *norm = wa2_posix_norm(wa2_str(ab));
    json_t *ret = json_string(norm);
    free(norm);
    json_free(ab);
    return ret;
}

/* PoP: wa2__owner_only_opener @ plugins/platforms/whatsapp/adapter.py:_owner_only_opener */
static json_t *wa2__owner_only_opener(wa2_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    wa2_set_s(a, "path", json_get_str(args, "path", ""));
    wa2_set_n(a, "flags", json_get_num(args, "flags", 0));
    wa2_set_n(a, "mode", 384.0);  /* 0o600 */
    json_t *fd = wa2_call(ctx, "os.open", a);
    if (wa2_is_raise(fd)) return fd;
    if (json_get_bool(args, "has_fchmod", false)) {
        json_t *fa = json_object();
        wa2_set_copy(fa, "fd", fd);
        wa2_set_n(fa, "mode", 384.0);
        json_t *fr = wa2_call(ctx, "os.fchmod", fa);
        if (wa2_is_raise(fr)) {
            if (wa2_kind_is(wa2_kind(fr), "OSError") && !wa2_is_base_exc(wa2_kind(fr))) {
                json_free(fr);  /* suppress(OSError) */
            } else {
                json_free(fd);
                return fr;
            }
        } else {
            json_free(fr);
        }
    }
    return fd;
}

/* PoP: wa2__open_bridge_log @ plugins/platforms/whatsapp/adapter.py:_open_bridge_log */
static json_t *wa2__open_bridge_log(wa2_ctx_t *ctx, const json_t *args) {
    const char *path = json_get_str(args, "path", "");
    bool priv = json_get_bool(args, "private", false);
    /* with suppress(OSError): rotate block */
    json_t *sa = json_object();
    wa2_set_s(sa, "path", path);
    json_t *st = wa2_call(ctx, "stat", sa);
    bool rotate = false;
    if (wa2_is_raise(st)) {
        if (wa2_kind_is(wa2_kind(st), "OSError") && !wa2_is_base_exc(wa2_kind(st))) {
            json_free(st);
        } else {
            return st;
        }
    } else {
        json_t *err = NULL;
        const json_t *sz = wa2_attr(st, "st_size", &err);
        if (err) { json_free(st); return err; }  /* AttributeError NOT suppressed */
        if (sz->type != JSON_NUMBER && sz->type != JSON_BOOL) {
            char msg[256];
            snprintf(msg, sizeof msg, "'>' not supported between instances of '%s' and 'int'",
                     wa2_type_name(sz));
            json_free(st);
            return wa2_err("TypeError", msg);
        }
        double szv = sz->type == JSON_BOOL ? (sz->bool_val ? 1.0 : 0.0) : sz->num_val;
        rotate = szv > 5242880.0;
        json_free(st);
    }
    if (rotate) {
        char *name = wa2_path_name(path);
        size_t nl = strlen(name);
        char *rot = malloc(nl + 3);
        memcpy(rot, name, nl);
        strcpy(rot + nl, ".1");
        char *dst = wa2_with_name(path, rot);
        free(rot);
        free(name);
        json_t *ra = json_object();
        wa2_set_s(ra, "src", path);
        wa2_set_s(ra, "dst", dst);
        free(dst);
        json_t *rp = wa2_call(ctx, "os.replace", ra);
        if (wa2_is_raise(rp)) {
            if (wa2_kind_is(wa2_kind(rp), "OSError") && !wa2_is_base_exc(wa2_kind(rp))) {
                json_free(rp);
            } else {
                return rp;
            }
        } else {
            json_free(rp);
        }
    }
    json_t *oa = json_object();
    wa2_set_s(oa, "path", path);
    wa2_set_s(oa, "mode", "a");
    wa2_set_s(oa, "encoding", "utf-8");
    if (priv) wa2_set_s(oa, "opener", "owner_only_opener");
    else wa2_set_null(oa, "opener");
    return wa2_call(ctx, "open", oa);
}

/* PoP: wa2__terminate_bridge_process @ plugins/platforms/whatsapp/adapter.py:_terminate_bridge_process */
static json_t *wa2__terminate_bridge_process(wa2_ctx_t *ctx, const json_t *args) {
    const json_t *proc = json_obj_get(args, "proc");
    bool force = json_get_bool(args, "force", false);
    bool isw = json_get_bool(args, "is_windows", true);
    const char *action = force ? "kill" : "terminate";
    json_t *err = NULL;
    const json_t *pidv = wa2_attr(proc, "pid", &err);
    if (err) return err;
    char *pids = wa2_str_proj(pidv);
    if (isw) {
        json_t *argv = json_array();
        json_append(argv, json_string("taskkill"));
        json_append(argv, json_string("/PID"));
        json_append(argv, json_string(pids));
        json_append(argv, json_string("/T"));
        if (force) json_append(argv, json_string("/F"));
        json_t *a = json_object();
        json_set(a, "argv", argv);
        wa2_set_n(a, "stdin", -3.0);  /* subprocess.DEVNULL */
        wa2_set_b(a, "capture_output", true);
        wa2_set_b(a, "text", true);
        wa2_set_s(a, "encoding", "utf-8");
        wa2_set_s(a, "errors", "replace");
        wa2_set_n(a, "timeout", 10.0);
        wa2_set_b(a, "check", false);
        json_t *r = wa2_call(ctx, "subprocess.run", a);
        if (wa2_is_raise(r)) {
            if (wa2_kind_is(wa2_kind(r), "FileNotFoundError") && !wa2_is_base_exc(wa2_kind(r))) {
                json_free(r);
                json_t *fa = json_object();
                wa2_set_copy(fa, "pid", pidv);
                char nm[64];
                snprintf(nm, sizeof nm, "proc.%s", action);
                json_t *fr = wa2_call(ctx, nm, fa);
                free(pids);
                return fr;  /* return getattr(proc, action)() */
            }
            free(pids);
            return r;
        }
        const json_t *rc = wa2_attr(r, "returncode", &err);
        if (err) { json_free(r); free(pids); return err; }
        /* returncode != 0: only number/bool compare equal to 0 (str "0"
         * is != 0 in Python); non-numeric non-bool -> failed. */
        bool rc_zero = false;
        if (rc->type == JSON_NUMBER) rc_zero = (rc->num_val == 0.0);
        else if (rc->type == JSON_BOOL) rc_zero = !rc->bool_val;
        bool failed = !rc_zero;
        if (failed) {
            const json_t *se = wa2_attr(r, "stderr", &err);
            if (err) { json_free(r); free(pids); return err; }
            const json_t *so = wa2_attr(r, "stdout", &err);
            if (err) { json_free(r); free(pids); return err; }
            const json_t *pick = wa2_truthy(se) ? se : (wa2_truthy(so) ? so : NULL);
            char *stripped = wa2_strip(pick ? wa2_str(pick) : "");
            char msg[512];
            if (stripped[0]) snprintf(msg, sizeof msg, "%s", stripped);
            else snprintf(msg, sizeof msg, "taskkill failed for PID %s", pids);
            free(stripped);
            json_free(r);
            free(pids);
            return wa2_err("OSError", msg);
        }
        json_free(r);
        free(pids);
        return NULL;
    }
    /* POSIX: psutil process-tree semantics, suppress(NoSuchProcess) regions */
    free(pids);
    json_t *pa = json_object();
    wa2_set_copy(pa, "pid", pidv);
    json_t *parent = wa2_call(ctx, "psutil.Process", pa);
    if (wa2_is_raise(parent)) {
        if (wa2_kind_is(wa2_kind(parent), "NoSuchProcess") && !wa2_is_base_exc(wa2_kind(parent))) {
            json_free(parent);
            return NULL;
        }
        return parent;
    }
    json_t *ca = json_object();
    wa2_set_copy(ca, "pid", pidv);
    wa2_set_b(ca, "recursive", true);
    json_t *children = wa2_call(ctx, "psutil.children", ca);
    if (wa2_is_raise(children)) {
        json_free(parent);
        if (wa2_kind_is(wa2_kind(children), "NoSuchProcess") && !wa2_is_base_exc(wa2_kind(children))) {
            json_free(children);
            return NULL;
        }
        return children;
    }
    for (size_t i = 0; i < json_len(children); i++) {
        const json_t *child = json_get(children, i);
        /* live getattr(child, action) resolves the METHOD first: a non-object
         * child raises the action-named AttributeError; an object child's
         * pid rides the recorder closure (absent -> null, no raise). */
        if (!child || child->type != JSON_OBJECT) {
            char msg[256];
            snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                     wa2_type_name(child), action);
            json_free(children);
            json_free(parent);
            return wa2_err("AttributeError", msg);
        }
        const json_t *cpid = json_obj_get(child, "pid");
        json_t *xa = json_object();
        wa2_set_copy(xa, "pid", cpid);
        char nm[64];
        snprintf(nm, sizeof nm, "child.%s", action);
        json_t *xr = wa2_call(ctx, nm, xa);
        if (wa2_is_raise(xr)) {
            if (wa2_kind_is(wa2_kind(xr), "NoSuchProcess") && !wa2_is_base_exc(wa2_kind(xr))) {
                json_free(xr);  /* inner suppress */
                continue;
            }
            json_free(children);
            json_free(parent);
            return xr;
        }
        json_free(xr);
    }
    json_free(children);
    json_t *ka = json_object();
    wa2_set_copy(ka, "pid", pidv);
    char nm[64];
    snprintf(nm, sizeof nm, "proc.%s", action);
    json_t *kr = wa2_call(ctx, nm, ka);
    json_free(parent);
    if (wa2_is_raise(kr)) {
        if (wa2_kind_is(wa2_kind(kr), "NoSuchProcess") && !wa2_is_base_exc(wa2_kind(kr))) {
            json_free(kr);
            return NULL;
        }
        return kr;
    }
    json_free(kr);
    return NULL;
}

/* PoP: wa2__is_allowed_bridge_path @ plugins/platforms/whatsapp/adapter.py:_is_allowed_bridge_path */
static json_t *wa2__is_allowed_bridge_path(wa2_ctx_t *ctx, const json_t *args) {
    const char *url = json_get_str(args, "url", "");
    json_t *ra = json_object();
    wa2_set_s(ra, "path", url);
    json_t *rs = wa2_call(ctx, "resolve", ra);
    if (wa2_is_raise(rs)) {
        const char *k = wa2_kind(rs);
        if ((wa2_kind_is(k, "OSError") || wa2_kind_is(k, "ValueError")) && !wa2_is_base_exc(k)) {
            json_free(rs);
            return json_bool(false);
        }
        return rs;
    }
    char *resolved = wa2_strdup(wa2_str(rs));
    json_free(rs);
    json_t *cd = wa2_call(ctx, "_cache_dirs", json_object());
    if (wa2_is_raise(cd)) { free(resolved); return cd; }
    for (size_t i = 0; i < json_len(cd); i++) {
        const json_t *root = json_get(cd, i);
        json_t *rra = json_object();
        wa2_set_s(rra, "path", wa2_str(root));
        json_t *rr = wa2_call(ctx, "resolve", rra);
        if (wa2_is_raise(rr)) {
            const char *k = wa2_kind(rr);
            if ((wa2_kind_is(k, "OSError") || wa2_kind_is(k, "ValueError")) && !wa2_is_base_exc(k)) {
                json_free(rr);
                continue;
            }
            json_free(cd);
            free(resolved);
            return rr;
        }
        json_t *ia = json_object();
        wa2_set_s(ia, "path", resolved);
        wa2_set_s(ia, "other", wa2_str(rr));
        json_t *ir = wa2_call(ctx, "is_relative_to", ia);
        json_free(rr);
        if (wa2_is_raise(ir)) {
            const char *k = wa2_kind(ir);
            if ((wa2_kind_is(k, "OSError") || wa2_kind_is(k, "ValueError")) && !wa2_is_base_exc(k)) {
                json_free(ir);
                continue;
            }
            json_free(cd);
            free(resolved);
            return ir;
        }
        bool rel = wa2_truthy(ir);
        json_free(ir);
        if (rel) {
            json_free(cd);
            free(resolved);
            return json_bool(true);
        }
    }
    json_free(cd);
    free(resolved);
    return json_bool(false);
}

/* PoP: wa2__bridge_token_lock @ plugins/platforms/whatsapp/adapter.py:_bridge_token_lock
 * The harness drives the whole `with` (enter steps, optional args.body_raise,
 * finally steps, with-open close); composed callers seam the factory plus
 * token_lock.enter / token_lock.exit instead. */
static json_t *wa2__bridge_token_lock(wa2_ctx_t *ctx, const json_t *args) {
    const char *lock_path = json_get_str(args, "lock_path", "");
    const char *os_name = json_get_str(args, "os_name", "nt");
    json_t *pending = NULL;
    json_t *oa = json_object();
    wa2_set_s(oa, "path", lock_path);
    wa2_set_s(oa, "mode", "a+b");
    wa2_set_null(oa, "encoding");
    wa2_set_null(oa, "opener");
    json_t *handle = wa2_call(ctx, "open", oa);
    if (wa2_is_raise(handle)) return handle;
    json_free(handle);
    bool nt = strcmp(os_name, "nt") == 0;
    json_t *fn = NULL;
    /* enter steps; a raise here skips the finally (only with-open close). */
    if (nt) {
        json_t *s1a = json_object();
        wa2_set_n(s1a, "offset", 0);
        wa2_set_n(s1a, "whence", 2);  /* os.SEEK_END */
        json_t *s1 = wa2_call(ctx, "handle.seek", s1a);
        if (wa2_is_raise(s1)) { pending = s1; goto close; }
        bool empty = (s1->type == JSON_NUMBER && s1->num_val == 0.0);
        json_free(s1);
        if (empty) {
            /* b"\0" projects to {"__bytes__":[0]} (libjson strings cannot
             * embed U+0000; the oracle ARGMAP projects bytes the same way). */
            json_t *wra = json_object();
            json_t *bytes = json_array();
            json_append(bytes, json_number(0));
            json_set(wra, "data", bytes);
            json_t *wr = wa2_call(ctx, "handle.write", wra);
            if (wa2_is_raise(wr)) { pending = wr; goto close; }
            json_free(wr);
            json_t *fl = wa2_call(ctx, "handle.flush", json_object());
            if (wa2_is_raise(fl)) { pending = fl; goto close; }
            json_free(fl);
        }
        json_t *s2a = json_object();
        wa2_set_n(s2a, "offset", 0);
        wa2_set_n(s2a, "whence", 0);
        json_t *s2 = wa2_call(ctx, "handle.seek", s2a);
        if (wa2_is_raise(s2)) { pending = s2; goto close; }
        json_free(s2);
        fn = wa2_call(ctx, "handle.fileno", json_object());
        if (wa2_is_raise(fn)) { pending = fn; fn = NULL; goto close; }
        json_t *la = json_object();
        wa2_set_copy(la, "fd", fn);
        wa2_set_n(la, "mode", 1);  /* msvcrt.LK_LOCK */
        wa2_set_n(la, "nbytes", 1);
        json_t *lk = wa2_call(ctx, "msvcrt.locking", la);
        if (wa2_is_raise(lk)) { pending = lk; goto close; }
        json_free(lk);
    } else {
        fn = wa2_call(ctx, "handle.fileno", json_object());
        if (wa2_is_raise(fn)) { pending = fn; fn = NULL; goto close; }
        json_t *fa = json_object();
        wa2_set_copy(fa, "fd", fn);
        wa2_set_n(fa, "operation", 2);  /* fcntl.LOCK_EX */
        json_t *fl = wa2_call(ctx, "fcntl.flock", fa);
        if (wa2_is_raise(fl)) { pending = fl; goto close; }
        json_free(fl);
    }
    /* yield body */
    {
        const json_t *br = json_obj_get(args, "body_raise");
        if (br && br->type == JSON_OBJECT) pending = wa2_make_exc(br);
    }
    /* finally steps (first raise wins over the body raise); the unlock
     * argument re-evaluates handle.fileno() (a SECOND fileno seam event) */
    if (nt) {
        json_t *s3a = json_object();
        wa2_set_n(s3a, "offset", 0);
        wa2_set_n(s3a, "whence", 0);
        json_t *s3 = wa2_call(ctx, "handle.seek", s3a);
        if (wa2_is_raise(s3)) {
            if (pending) json_free(pending);
            pending = s3;
            goto close;
        }
        json_free(s3);
        json_t *fn2 = wa2_call(ctx, "handle.fileno", json_object());
        if (wa2_is_raise(fn2)) {
            if (pending) json_free(pending);
            pending = fn2;
            goto close;
        }
        json_t *ua = json_object();
        wa2_set_copy(ua, "fd", fn2);
        wa2_set_n(ua, "mode", 0);  /* msvcrt.LK_UNLCK */
        wa2_set_n(ua, "nbytes", 1);
        json_free(fn2);
        json_t *ul = wa2_call(ctx, "msvcrt.locking", ua);
        if (wa2_is_raise(ul)) {
            if (pending) json_free(pending);
            pending = ul;
        } else {
            json_free(ul);
        }
    } else {
        json_t *fn2 = wa2_call(ctx, "handle.fileno", json_object());
        if (wa2_is_raise(fn2)) {
            if (pending) json_free(pending);
            pending = fn2;
            goto close;
        }
        json_t *fa = json_object();
        wa2_set_copy(fa, "fd", fn2);
        wa2_set_n(fa, "operation", 8);  /* fcntl.LOCK_UN */
        json_free(fn2);
        json_t *fu = wa2_call(ctx, "fcntl.flock", fa);
        if (wa2_is_raise(fu)) {
            if (pending) json_free(pending);
            pending = fu;
        } else {
            json_free(fu);
        }
    }
close:
    if (fn) json_free(fn);
    {
        json_t *hc = wa2_call(ctx, "handle.close", json_object());
        if (wa2_is_raise(hc)) {
            if (pending) json_free(pending);
            pending = hc;
        } else {
            json_free(hc);
        }
    }
    return pending;  /* NULL == normal completion */
}

/* PoP: wa2__load_or_create_bridge_token @ plugins/platforms/whatsapp/adapter.py:_load_or_create_bridge_token */
static json_t *wa2__load_or_create_bridge_token(wa2_ctx_t *ctx, const json_t *args) {
    const char *session_path = json_get_str(args, "session_path", "");
    bool rotate = json_get_bool(args, "rotate", false);
    json_t *ma = json_object();
    wa2_set_s(ma, "path", session_path);
    wa2_set_b(ma, "parents", true);
    wa2_set_b(ma, "exist_ok", true);
    json_t *mr = wa2_call(ctx, "mkdir", ma);
    if (wa2_is_raise(mr)) return mr;
    json_free(mr);
    char *token_path = wa2_path_join(session_path, ".bridge-token");
    char *lock_path = wa2_with_name(token_path, ".bridge-token.lock");
    json_t *la = json_object();
    wa2_set_s(la, "lock_path", lock_path);
    free(lock_path);
    json_t *cm = wa2_call(ctx, "_bridge_token_lock", la);
    if (wa2_is_raise(cm)) { free(token_path); return cm; }
    json_free(cm);
    json_t *en = wa2_call(ctx, "token_lock.enter", json_object());
    if (wa2_is_raise(en)) { free(token_path); return en; }
    json_free(en);
    json_t *pending = NULL;
    json_t *ret = NULL;
    /* with suppress(OSError): existing-token read */
    bool have_existing = false;
    char *existing = NULL;
    if (rotate) {
        existing = wa2_strdup("");
        have_existing = true;
    } else {
        json_t *ra = json_object();
        wa2_set_s(ra, "path", token_path);
        wa2_set_s(ra, "encoding", "utf-8-sig");
        wa2_set_null(ra, "errors");
        json_t *rt = wa2_call(ctx, "read_text", ra);
        if (wa2_is_raise(rt)) {
            const char *k = wa2_kind(rt);
            if (wa2_kind_is(k, "OSError") && !wa2_is_base_exc(k)) {
                json_free(rt);
            } else {
                pending = rt;
                goto exit;
            }
        } else {
            char *bom = wa2_bom_strip(wa2_str(rt));
            existing = wa2_strip(bom);
            free(bom);
            have_existing = true;
            json_free(rt);
        }
    }
    if (have_existing) {
        size_t n = 0;
        uint32_t *cps = wa2_utf8_decode(existing, &n);
        free(cps);
        if (n >= 32) {
            ret = json_string(existing);
            free(existing);
            existing = NULL;
            goto exit;
        }
    }
    if (existing) { free(existing); existing = NULL; }
    /* mint */
    {
        json_t *ta = json_object();
        wa2_set_n(ta, "n", 32);
        json_t *tk = wa2_call(ctx, "secrets.token_urlsafe", ta);
        if (wa2_is_raise(tk)) { pending = tk; goto exit; }
        char *token = wa2_strdup(wa2_str(tk));
        json_free(tk);
        json_t *mk = wa2_call(ctx, "tempfile.mkstemp", ({
            json_t *a = json_object();
            wa2_set_s(a, "prefix", ".bridge-token.");
            wa2_set_s(a, "dir", session_path);
            a;
        }));
        if (wa2_is_raise(mk)) { free(token); pending = mk; goto exit; }
        const json_t *fdv = json_get(mk, 0);
        const json_t *tmpv = json_get(mk, 1);
        char *tmp = wa2_strdup(wa2_str(tmpv));
        /* inner try: fdopen handle write/flush/fsync/close + replace */
        json_t *inner = NULL;
        json_t *fa = json_object();
        wa2_set_copy(fa, "fd", fdv);
        wa2_set_s(fa, "mode", "w");
        wa2_set_s(fa, "encoding", "utf-8");
        json_t *fh = wa2_call(ctx, "os.fdopen", fa);
        if (wa2_is_raise(fh)) { inner = fh; goto inner_done; }
        json_free(fh);
        {
            json_t *wa = json_object();
            wa2_set_s(wa, "data", token);
            json_t *wr = wa2_call(ctx, "handle.write", wa);
            if (wa2_is_raise(wr)) { inner = wr; goto inner_close; }
            json_free(wr);
        }
        {
            json_t *fl = wa2_call(ctx, "handle.flush", json_object());
            if (wa2_is_raise(fl)) { inner = fl; goto inner_close; }
            json_free(fl);
        }
        {
            json_t *fn = wa2_call(ctx, "handle.fileno", json_object());
            if (wa2_is_raise(fn)) { inner = fn; goto inner_close; }
            json_t *fsa = json_object();
            wa2_set_copy(fsa, "fd", fn);
            json_free(fn);
            json_t *fs = wa2_call(ctx, "os.fsync", fsa);
            if (wa2_is_raise(fs)) { inner = fs; goto inner_close; }
            json_free(fs);
        }
inner_close:
        {
            json_t *hc = wa2_call(ctx, "handle.close", json_object());
            if (wa2_is_raise(hc)) {
                if (inner) json_free(inner);
                inner = hc;
            } else {
                json_free(hc);
            }
        }
inner_done:
        if (!inner) {
            json_t *rpa = json_object();
            wa2_set_s(rpa, "src", tmp);
            wa2_set_s(rpa, "dst", token_path);
            json_t *rp = wa2_call(ctx, "os.replace", rpa);
            if (wa2_is_raise(rp)) inner = rp;
            else json_free(rp);
        }
        if (inner) {
            /* except BaseException: suppress(OSError) unlink(tmp); raise */
            json_t *ua = json_object();
            wa2_set_s(ua, "path", tmp);
            json_t *ul = wa2_call(ctx, "os.unlink", ua);
            if (wa2_is_raise(ul)) {
                const char *k = wa2_kind(ul);
                if (wa2_kind_is(k, "OSError") && !wa2_is_base_exc(k)) {
                    json_free(ul);
                } else {
                    json_free(inner);
                    inner = ul;
                }
            } else {
                json_free(ul);
            }
            pending = inner;
        } else {
            ret = json_string(token);
        }
        free(tmp);
        free(token);
        json_free(mk);
    }
exit:
    if (existing) free(existing);
    {
        bool suppressed = false;
        json_t *final = wa2_cm_exit(ctx, "token_lock.exit", pending, &suppressed);
        if (final) {
            if (ret) json_free(ret);
            free(token_path);
            return final;
        }
        if (suppressed) {
            if (ret) json_free(ret);
            ret = NULL;  /* with-body raise suppressed -> implicit None */
        }
    }
    free(token_path);
    return ret;
}

/* PoP: wa2__import_aiohttp @ plugins/platforms/whatsapp/adapter.py:_import_aiohttp */
static json_t *wa2__import_aiohttp(wa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ah = json_obj_get(args, "aiohttp");
    if (!ah || ah->type == JSON_NULL) {
        return wa2_err("ModuleNotFoundError", "import of aiohttp halted; None in sys.modules");
    }
    if (json_get_bool(ah, "client_session", false)) {
        json_t *m = json_object();
        wa2_set_s(m, "__module__", "aiohttp");
        return m;
    }
    const json_t *file = json_obj_get(ah, "file");
    const json_t *path = json_obj_get(ah, "path");
    char *where;
    if (wa2_truthy(file)) {
        where = wa2_str_proj(file);
    } else if (path && path->type == JSON_ARRAY) {
        where = wa2_list_repr(path);
    } else {
        where = wa2_strdup("[]");
    }
    char msg[512];
    snprintf(msg, sizeof msg, "aiohttp at %s is an incomplete install", where);
    free(where);
    return wa2_err("ImportError", msg);
}

/* PoP: wa2_ensure_aiohttp @ plugins/platforms/whatsapp/adapter.py:ensure_aiohttp */
static json_t *wa2_ensure_aiohttp(wa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *ia = wa2_call(ctx, "_import_aiohttp", json_object());
    if (!wa2_is_raise(ia)) { json_free(ia); return NULL; }
    const char *k = wa2_kind(ia);
    if (wa2_kind_is(k, "ImportError") && !wa2_is_base_exc(k)) {
        if (strcmp(k, "ModuleNotFoundError") != 0) {
            json_t *r = json_array();
            json_append(r, json_string(json_get_str(ia, "message", "")));
            json_append(r, json_bool(false));
            json_free(ia);
            return r;
        }
        json_free(ia);  /* ModuleNotFoundError: fall through to the PM path */
    } else {
        return ia;
    }
    json_t *root = wa2_call(ctx, "repo_root", json_object());
    if (wa2_is_raise(root)) return root;
    json_t *la = wa2_call(ctx, "lazy_installs_allowed", json_object());
    if (wa2_is_raise(la)) { json_free(root); return la; }
    bool installable = false;
    if (wa2_truthy(la)) {
        json_t *ra = json_object();
        wa2_set_copy(ra, "root", root);
        json_t *rs = wa2_call(ctx, "running_from_selected_environment", ra);
        if (wa2_is_raise(rs)) { json_free(la); json_free(root); return rs; }
        if (wa2_truthy(rs)) {
            installable = true;
        } else {
            json_t *rna = json_object();
            wa2_set_copy(rna, "root", root);
            json_t *rn = wa2_call(ctx, "restart_needed", rna);
            if (wa2_is_raise(rn)) { json_free(rs); json_free(la); json_free(root); return rn; }
            installable = (rn && rn->type != JSON_NULL);  /* is not None */
            json_free(rn);
        }
        json_free(rs);
    }
    json_free(la);
    json_free(root);
    json_t *ea = json_object();
    wa2_set_s(ea, "extra", "sms");
    json_t *ei = wa2_call(ctx, "ensure_import", ea);
    if (wa2_is_raise(ei)) {
        const char *ek = wa2_kind(ei);
        if ((wa2_kind_is(ek, "RuntimeError") || wa2_kind_is(ek, "OSError") || wa2_kind_is(ek, "ValueError"))
            && !wa2_is_base_exc(ek)) {
            const char *msg = json_get_str(ei, "message", "");
            json_t *r = json_array();
            json_append(r, json_string(msg[0] ? msg : ek));  /* str(exc) or type name */
            json_append(r, json_bool(installable));
            json_free(ei);
            return r;
        }
        return ei;
    }
    json_free(ei);
    json_t *ia2 = wa2_call(ctx, "_import_aiohttp", json_object());
    if (!wa2_is_raise(ia2)) { json_free(ia2); return NULL; }
    const char *k2 = wa2_kind(ia2);
    if (wa2_kind_is(k2, "ImportError") && !wa2_is_base_exc(k2)) {
        char msg[512];
        snprintf(msg, sizeof msg, "%s after installing the sms extra", json_get_str(ia2, "message", ""));
        json_t *r = json_array();
        json_append(r, json_string(msg));
        json_append(r, json_bool(false));
        json_free(ia2);
        return r;
    }
    return ia2;
}

/* -- decorator + aiohttp request flows -- */

/* PoP: wa2__needs_bridge @ plugins/platforms/whatsapp/adapter.py:_needs_bridge
 * PoP: wa2__guarded @ plugins/platforms/whatsapp/adapter.py:_guarded
 * The harness drives the factory AND the returned guarded coroutine: the
 * wrapped method is the async "method" seam (self dropped from its args),
 * self._bridge_unavailable the async "_bridge_unavailable" seam. */
static json_t *wa2__needs_bridge(wa2_ctx_t *ctx, const json_t *args) {
    json_t *un = wa2_call(ctx, "_bridge_unavailable", json_object());
    if (wa2_is_raise(un)) return un;
    if (wa2_truthy(un)) {
        json_t *r = wa2_send_result(false, NULL, un, NULL);
        json_free(un);
        return r;
    }
    json_free(un);
    json_t *ma = json_object();
    wa2_set_copy(ma, "args", json_obj_get(args, "call_args"));
    wa2_set_copy(ma, "kwargs", json_obj_get(args, "call_kwargs"));
    return wa2_call(ctx, "method", ma);
}

/* PoP: wa2__bridge_req @ plugins/platforms/whatsapp/adapter.py:_bridge_req */
static json_t *wa2__bridge_req(wa2_ctx_t *ctx, const json_t *args) {
    json_t *slf = wa2_self(ctx);
    const char *method = json_get_str(args, "method", "");
    const char *path = json_get_str(args, "path", "");
    /* getattr(self._http_session, method) resolves BEFORE the arguments:
     * an unknown method raises AttributeError with no seam event. */
    const json_t *sms = json_obj_get(args, "session_methods");
    bool known = false;
    for (size_t i = 0; i < json_len(sms); i++) {
        if (strcmp(wa2_str(json_get(sms, i)), method) == 0) { known = true; break; }
    }
    if (!known) {
        char msg[256];
        snprintf(msg, sizeof msg, "'types.SimpleNamespace' object has no attribute '%s'", method);
        return wa2_err("AttributeError", msg);
    }
    /* url = self._bridge_url(path) (transcribed leaf, no seam) */
    json_t *err = NULL;
    const json_t *port = wa2_attr(slf, "_bridge_port", &err);
    if (err) return err;
    char *ports = wa2_str_proj(port);
    size_t ul = strlen(ports) + strlen(path) + 32;
    char *url = malloc(ul);
    snprintf(url, ul, "http://127.0.0.1:%s/%s", ports, path);
    free(ports);
    /* timeout=aiohttp.ClientTimeout(total=timeout) evaluates before the call */
    json_t *ta = json_object();
    wa2_set_copy(ta, "total", json_obj_get(args, "timeout"));
    json_t *ct = wa2_call(ctx, "ClientTimeout", ta);
    if (wa2_is_raise(ct)) { free(url); return ct; }
    json_t *a = json_object();
    wa2_set_s(a, "url", url);
    free(url);
    const json_t *kw = json_obj_get(args, "kwargs");
    if (kw && kw->type == JSON_OBJECT) {
        for (size_t i = 0; i < kw->c.count; i++)
            json_set(a, kw->c.keys[i], json_copy(kw->c.items[i]));
    }
    json_set(a, "timeout", ct);
    char nm[128];
    snprintf(nm, sizeof nm, "session.%s", method);
    return wa2_call(ctx, nm, a);
}

/* PoP: wa2__authenticated_bridge_health @ plugins/platforms/whatsapp/adapter.py:_authenticated_bridge_health
 * Async CM model: session.get seam -> resp.enter -> body (resp.json) ->
 * resp.exit {exc}. An exit raise overrides the body raise; a truthy exit
 * ret suppresses it (fixture domain: null exit on body-raise cases). */
static json_t *wa2__authenticated_bridge_health(wa2_ctx_t *ctx, const json_t *args) {
    const char *base_url = json_get_str(args, "base_url", "");
    const char *token = json_get_str(args, "token", "");
    const json_t *timeout = json_obj_get(args, "timeout");
    json_t *ca = json_object();
    wa2_set_n(ca, "n", 24);
    json_t *ch = wa2_call(ctx, "secrets.token_urlsafe", ca);
    if (wa2_is_raise(ch)) return ch;
    char *challenge = wa2_strdup(wa2_str(ch));
    json_free(ch);
    /* first async with: GET /auth-proof */
    size_t u1l = strlen(base_url) + 16;
    char *url1 = malloc(u1l);
    snprintf(url1, u1l, "%s/auth-proof", base_url);
    json_t *h1 = json_object();
    wa2_set_s(h1, "X-Hermes-Bridge-Challenge", challenge);
    json_t *g1a = json_object();
    wa2_set_s(g1a, "url", url1);
    free(url1);
    json_set(g1a, "headers", h1);
    wa2_set_copy(g1a, "timeout", timeout);
    json_t *g1 = wa2_call(ctx, "session.get", g1a);
    if (wa2_is_raise(g1)) { free(challenge); return g1; }
    json_t *e1 = wa2_call(ctx, "resp.enter", json_object());
    if (wa2_is_raise(e1)) { json_free(g1); free(challenge); return e1; }
    json_free(e1);
    json_t *pending = NULL;
    json_t *bodyret = NULL;   /* early (False, None) return */
    bool fallthrough = false; /* proof extracted, continue after the with */
    json_t *proof = NULL;     /* borrowed from the resp.json result */
    json_t *pj_holder = NULL; /* owned resp.json result */
    {
        json_t *err = NULL;
        const json_t *status = wa2_attr(g1, "status", &err);
        if (err) { pending = err; goto exit1; }
        if (!(status->type == JSON_NUMBER && status->num_val == 200.0)) {
            bodyret = json_array();
            json_append(bodyret, json_bool(false));
            json_append(bodyret, json_null());
            goto exit1;
        }
        json_t *pj = wa2_call(ctx, "resp.json", json_object());
        if (wa2_is_raise(pj)) {
            const char *k = wa2_kind(pj);
            if (wa2_kind_is(k, "TimeoutError")) {
                pending = pj;  /* except TimeoutError: raise */
            } else if ((wa2_kind_is(k, "ClientError") || wa2_kind_is(k, "ValueError")
                        || wa2_kind_is(k, "AttributeError")) && !wa2_is_base_exc(k)) {
                json_free(pj);
                bodyret = json_array();
                json_append(bodyret, json_bool(false));
                json_append(bodyret, json_null());
            } else {
                pending = pj;
            }
            goto exit1;
        }
        /* (await resp.json()).get("authProof") -- non-object .get raises
         * AttributeError, CAUGHT by the same except -> (False, None). */
        if (!pj || pj->type != JSON_OBJECT) {
            json_free(pj);
            bodyret = json_array();
            json_append(bodyret, json_bool(false));
            json_append(bodyret, json_null());
            goto exit1;
        }
        pj_holder = pj;
        proof = json_obj_get(pj, "authProof");
        fallthrough = true;
    }
exit1:
    {
        bool suppressed = false;
        json_t *final = wa2_cm_exit(ctx, "resp.exit", pending, &suppressed);
        if (final) {
            if (bodyret) json_free(bodyret);
            if (pj_holder) json_free(pj_holder);
            json_free(g1);
            free(challenge);
            return final;
        }
        if (suppressed) {
            /* body raise suppressed: live continues with unbound locals
             * (out of fixture domain); model the NameError faithfully. */
            if (bodyret) json_free(bodyret);
            if (pj_holder) json_free(pj_holder);
            json_free(g1);
            free(challenge);
            return wa2_err("NameError", "name 'proof' is not defined");
        }
        pending = NULL;
    }
    if (bodyret) {
        if (pj_holder) json_free(pj_holder);
        json_free(g1);
        free(challenge);
        return bodyret;
    }
    (void)fallthrough;
    /* expected = _bridge_auth_proof(token, challenge).encode("utf-8") */
    json_t *apa = json_object();
    wa2_set_s(apa, "token", token);
    wa2_set_s(apa, "challenge", challenge);
    json_t *ap = wa2_call(ctx, "_bridge_auth_proof", apa);
    if (wa2_is_raise(ap)) {
        if (pj_holder) json_free(pj_holder);
        json_free(g1);
        free(challenge);
        return ap;
    }
    if (!wa2_str(ap)) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'encode'", wa2_type_name(ap));
        json_free(ap);
        if (pj_holder) json_free(pj_holder);
        json_free(g1);
        free(challenge);
        return wa2_err("AttributeError", msg);
    }
    /* isinstance(proof, str) and compare_digest(proof.encode(), expected):
     * ASCII fixture domain -> byte equality. */
    bool proof_ok = false;
    if (proof && proof->type == JSON_STRING)
        proof_ok = strcmp(proof->str_val ? proof->str_val : "", ap->str_val) == 0;
    json_free(ap);
    if (pj_holder) json_free(pj_holder);
    json_free(g1);
    if (!proof_ok) {
        free(challenge);
        json_t *r = json_array();
        json_append(r, json_bool(false));
        json_append(r, json_null());
        return r;
    }
    /* second async with: GET /health with the bearer */
    json_t *ha = wa2_call(ctx, "_bridge_auth_headers", ({
        json_t *a = json_object();
        wa2_set_s(a, "token", token);
        a;
    }));
    if (wa2_is_raise(ha)) { free(challenge); return ha; }
    size_t u2l = strlen(base_url) + 16;
    char *url2 = malloc(u2l);
    snprintf(url2, u2l, "%s/health", base_url);
    json_t *g2a = json_object();
    wa2_set_s(g2a, "url", url2);
    free(url2);
    json_set(g2a, "headers", ha);
    wa2_set_copy(g2a, "timeout", timeout);
    json_t *g2 = wa2_call(ctx, "session.get", g2a);
    free(challenge);
    if (wa2_is_raise(g2)) return g2;
    json_t *e2 = wa2_call(ctx, "resp.enter", json_object());
    if (wa2_is_raise(e2)) { json_free(g2); return e2; }
    json_free(e2);
    pending = NULL;
    bodyret = NULL;
    {
        json_t *err = NULL;
        const json_t *status = wa2_attr(g2, "status", &err);
        if (err) { pending = err; goto exit2; }
        if (!(status->type == JSON_NUMBER && status->num_val == 200.0)) {
            bodyret = json_array();
            json_append(bodyret, json_bool(false));
            json_append(bodyret, json_null());
            goto exit2;
        }
        json_t *pj2 = wa2_call(ctx, "resp.json", json_object());
        if (wa2_is_raise(pj2)) {
            const char *k = wa2_kind(pj2);
            if (wa2_kind_is(k, "TimeoutError")) {
                pending = pj2;
            } else if ((wa2_kind_is(k, "ClientError") || wa2_kind_is(k, "ValueError"))
                       && !wa2_is_base_exc(k)) {
                json_free(pj2);
                bodyret = json_array();
                json_append(bodyret, json_bool(true));
                json_append(bodyret, json_null());
            } else {
                pending = pj2;
            }
            goto exit2;
        }
        bodyret = json_array();
        json_append(bodyret, json_bool(true));
        json_append(bodyret, pj2);
    }
exit2:
    {
        bool suppressed = false;
        json_t *final = wa2_cm_exit(ctx, "resp.exit", pending, &suppressed);
        if (final) {
            if (bodyret) json_free(bodyret);
            json_free(g2);
            return final;
        }
        if (suppressed) {
            if (bodyret) json_free(bodyret);
            json_free(g2);
            return NULL;  /* suppressed body raise -> implicit None */
        }
    }
    json_free(g2);
    return bodyret;
}

/* -- adapter method flows (self rides ctx.state["self"]) -- */

/* PoP: wa2__bridge_package_hash @ plugins/platforms/whatsapp/adapter.py:_bridge_package_hash */
static json_t *wa2__bridge_package_hash(wa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *slf = wa2_self(ctx);
    json_t *err = NULL;
    const json_t *script = wa2_attr(slf, "_bridge_script", &err);
    if (err) return err;
    char *par = wa2_path_parent(wa2_str(script));
    char *pkg = wa2_path_join(par, "package.json");
    free(par);
    json_t *a = json_object();
    wa2_set_s(a, "path", pkg);
    free(pkg);
    return wa2_call(ctx, "_file_content_hash", a);
}

/* PoP: wa2__bridge_env @ plugins/platforms/whatsapp/adapter.py:_bridge_env */
static const char *WA2_PASSTHROUGH_ENV[] = {
    "WHATSAPP_ALLOWED_USERS", "WHATSAPP_ALLOW_FROM", "WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY",
    "WHATSAPP_GROUP_ALLOWED_USERS", "WHATSAPP_GROUP_ALLOW_FROM", "WHATSAPP_REQUIRE_MENTION",
    "WHATSAPP_MENTION_PATTERNS", "WHATSAPP_FREE_RESPONSE_CHATS", "WHATSAPP_DEBUG",
    "WHATSAPP_FORWARD_OWNER_MESSAGES", "WHATSAPP_MAX_MESSAGE_LENGTH",
    "WHATSAPP_CHUNK_DELAY_MS", "WHATSAPP_SEND_TIMEOUT_MS",
};
static json_t *wa2__bridge_env(wa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *slf = wa2_self(ctx);
    json_t *hse = wa2_call(ctx, "hermes_subprocess_env", json_object());
    if (wa2_is_raise(hse)) return hse;
    json_t *wa = json_object();
    json_set(wa, "env", hse);
    json_t *env = wa2_call(ctx, "with_hermes_node_path", wa);
    if (wa2_is_raise(env)) return env;
    if (env->type != JSON_OBJECT) {
        /* live dict ops on a non-dict raise AttributeError; out of domain
         * except as a fail-closed guard. */
        json_free(env);
        return wa2_err("AttributeError", "'object' object has no attribute 'pop'");
    }
    json_t *err = NULL;
    /* reply_prefix = _wenv("WHATSAPP_REPLY_PREFIX") */
    json_t *rpa = json_object();
    wa2_set_s(rpa, "name", "WHATSAPP_REPLY_PREFIX");
    wa2_set_s(rpa, "default", "");
    json_t *rp = wa2_call(ctx, "_wenv", rpa);
    if (wa2_is_raise(rp)) { json_free(env); return rp; }
    if (wa2_truthy(rp)) {
        json_set(env, "WHATSAPP_REPLY_PREFIX", json_copy(rp));
    } else {
        const json_t *rpf = wa2_attr(slf, "_reply_prefix", &err);
        if (err) { json_free(rp); json_free(env); return err; }
        if (rpf->type != JSON_NULL) json_set(env, "WHATSAPP_REPLY_PREFIX", json_copy(rpf));
        else json_obj_del(env, "WHATSAPP_REPLY_PREFIX");
    }
    json_free(rp);
    const json_t *srr = wa2_attr(slf, "_send_read_receipts", &err);
    if (err) { json_free(env); return err; }
    wa2_set_s(env, "WHATSAPP_SEND_READ_RECEIPTS", wa2_truthy(srr) ? "true" : "false");
    const json_t *tok = wa2_attr(slf, "_bridge_token", &err);
    if (err) { json_free(env); return err; }
    json_set(env, "HERMES_WHATSAPP_BRIDGE_TOKEN", json_copy(tok));
    /* the list literal evaluates ALL _wenv calls eagerly, in order */
    json_t *vals = json_array();
    {
        json_t *ma = json_object();
        wa2_set_s(ma, "name", "WHATSAPP_MODE");
        wa2_set_s(ma, "default", "self-chat");
        json_t *mv = wa2_call(ctx, "_wenv", ma);
        if (wa2_is_raise(mv)) { json_free(vals); json_free(env); return mv; }
        json_append(vals, mv);
    }
    for (size_t i = 0; i < sizeof(WA2_PASSTHROUGH_ENV) / sizeof(WA2_PASSTHROUGH_ENV[0]); i++) {
        json_t *ka = json_object();
        wa2_set_s(ka, "name", WA2_PASSTHROUGH_ENV[i]);
        wa2_set_s(ka, "default", "");
        json_t *v = wa2_call(ctx, "_wenv", ka);
        if (wa2_is_raise(v)) { json_free(vals); json_free(env); return v; }
        json_append(vals, v);
    }
    {
        json_t *mv = json_get(vals, 0);
        if (wa2_truthy(mv)) json_set(env, "WHATSAPP_MODE", json_copy(mv));
        else json_obj_del(env, "WHATSAPP_MODE");
        for (size_t i = 0; i < sizeof(WA2_PASSTHROUGH_ENV) / sizeof(WA2_PASSTHROUGH_ENV[0]); i++) {
            json_t *v = json_get(vals, i + 1);
            if (wa2_truthy(v)) json_set(env, WA2_PASSTHROUGH_ENV[i], json_copy(v));
            else json_obj_del(env, WA2_PASSTHROUGH_ENV[i]);
        }
    }
    json_free(vals);
    const json_t *dm = wa2_attr(slf, "_dm_policy", &err);
    if (err) { json_free(env); return err; }
    json_set(env, "WHATSAPP_DM_POLICY", json_copy(dm));
    const json_t *gp = wa2_attr(slf, "_group_policy", &err);
    if (err) { json_free(env); return err; }
    json_set(env, "WHATSAPP_GROUP_POLICY", json_copy(gp));
    const json_t *allow = wa2_attr(slf, "_allow_from", &err);
    if (err) { json_free(env); return err; }
    if (wa2_truthy(allow)) {
        char *joined = wa2_sorted_join(allow);
        wa2_set_s(env, "WHATSAPP_ALLOWED_USERS", joined);
        free(joined);
    } else {
        json_obj_del(env, "WHATSAPP_ALLOWED_USERS");
    }
    const json_t *gallow = wa2_attr(slf, "_group_allow_from", &err);
    if (err) { json_free(env); return err; }
    if (wa2_truthy(gallow)) {
        char *joined = wa2_sorted_join(gallow);
        wa2_set_s(env, "WHATSAPP_GROUP_ALLOWED_USERS", joined);
        free(joined);
    } else {
        json_obj_del(env, "WHATSAPP_GROUP_ALLOWED_USERS");
    }
    json_t *cd = wa2_call(ctx, "_cache_dirs", json_object());
    if (wa2_is_raise(cd)) { json_free(env); return cd; }
    char *img = wa2_str_proj(json_get(cd, 0));
    char *aud = wa2_str_proj(json_get(cd, 1));
    char *doc = wa2_str_proj(json_get(cd, 3));
    wa2_set_s(env, "HERMES_IMAGE_CACHE_DIR", img);
    wa2_set_s(env, "HERMES_AUDIO_CACHE_DIR", aud);
    wa2_set_s(env, "HERMES_DOCUMENT_CACHE_DIR", doc);
    free(img); free(aud); free(doc);
    json_free(cd);
    return env;
}

/* PoP: wa2__adopted_config_matches @ plugins/platforms/whatsapp/adapter.py:_adopted_config_matches */
static json_t *wa2__adopted_config_matches(wa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *slf = wa2_self(ctx);
    json_t *err = NULL;
    const json_t *sp = wa2_attr(slf, "_session_path", &err);
    if (err) return err;
    char *fp_path = wa2_path_join(wa2_str(sp), "bridge.env-fingerprint");
    char *recorded = wa2_strdup("");
    json_t *ra = json_object();
    wa2_set_s(ra, "path", fp_path);
    wa2_set_s(ra, "encoding", "utf-8-sig");
    wa2_set_null(ra, "errors");
    free(fp_path);
    json_t *rt = wa2_call(ctx, "read_text", ra);
    if (wa2_is_raise(rt)) {
        const char *k = wa2_kind(rt);
        if (wa2_kind_is(k, "OSError") && !wa2_is_base_exc(k)) {
            json_free(rt);
        } else {
            free(recorded);
            return rt;
        }
    } else {
        free(recorded);
        char *bom = wa2_bom_strip(wa2_str(rt));
        recorded = wa2_strip(bom);
        free(bom);
        json_free(rt);
    }
    if (!recorded[0]) {
        free(recorded);
        return json_bool(false);  /* bool(recorded) short-circuits */
    }
    json_t *be = wa2_call(ctx, "_bridge_env", json_object());
    if (wa2_is_raise(be)) { free(recorded); return be; }
    json_t *ph = wa2_call(ctx, "_bridge_package_hash", json_object());
    if (wa2_is_raise(ph)) { json_free(be); free(recorded); return ph; }
    json_t *fa = json_object();
    json_set(fa, "bridge_env", be);
    json_set(fa, "package_hash", ph);
    json_t *fp = wa2_call(ctx, "_bridge_config_fingerprint", fa);
    if (wa2_is_raise(fp)) { free(recorded); return fp; }
    bool match = wa2_str(fp) && strcmp(recorded, wa2_str(fp)) == 0;
    free(recorded);
    json_free(fp);
    return json_bool(match);
}

/* PoP: wa2__bridge_died @ plugins/platforms/whatsapp/adapter.py:_bridge_died */
static json_t *wa2__bridge_died(wa2_ctx_t *ctx, const json_t *args) {
    json_t *slf = wa2_self(ctx);
    char *detail = wa2_strdup(json_get_str(args, "detail", ""));
    char *code = wa2_strdup(json_get_str(args, "code", "whatsapp_bridge_exited"));
    json_t *err = NULL;
    const json_t *proc = wa2_attr(slf, "_bridge_process", &err);
    if (err) { free(detail); free(code); return err; }
    bool needs_passkey = false;
    if (proc && proc->type != JSON_NULL) {
        const json_t *pid = wa2_attr(proc, "pid", &err);
        if (err) { free(detail); free(code); return err; }
        json_t *pa = json_object();
        wa2_set_copy(pa, "pid", pid);
        json_t *pc = wa2_call(ctx, "proc.poll", pa);
        if (wa2_is_raise(pc)) { free(detail); free(code); return pc; }
        needs_passkey = (pc->type == JSON_NUMBER && pc->num_val == 78.0);
        json_free(pc);
    }
    if (needs_passkey) {
        free(code);
        code = wa2_strdup("whatsapp_passkey_required");
        free(detail);
        detail = wa2_strdup("WhatsApp requires passkey verification. Run `hermes whatsapp --passkey-browser EXECUTABLE` in a local interactive terminal, then restart the gateway.");
    }
    const json_t *name = wa2_attr(slf, "name", &err);
    if (err) { free(detail); free(code); return err; }
    char *names = wa2_str_proj(name);
    size_t ml = strlen(names) + strlen(detail) + 128;
    char *msg = malloc(ml);
    snprintf(msg, ml, "[%s] %s", names, detail);
    json_t *p1 = json_object();
    wa2_set_s(p1, "text", msg);
    json_t *pr1 = wa2_call(ctx, "print", p1);
    if (wa2_is_raise(pr1)) { free(msg); free(names); free(detail); free(code); return pr1; }
    json_free(pr1);
    /* self._bridge_log is read at the SECOND print (f-string evaluation) */
    const json_t *blog = wa2_attr(slf, "_bridge_log", &err);
    if (err) { free(msg); free(names); free(detail); free(code); return err; }
    char *blogs = wa2_str_proj(blog);
    ml = strlen(names) + strlen(detail) + strlen(blogs) + 64;
    msg = realloc(msg, ml);
    snprintf(msg, ml, "[%s] Check log: %s", names, blogs);
    json_t *p2 = json_object();
    wa2_set_s(p2, "text", msg);
    json_t *pr2 = wa2_call(ctx, "print", p2);
    if (wa2_is_raise(pr2)) { free(msg); free(names); free(blogs); free(detail); free(code); return pr2; }
    json_free(pr2);
    json_t *cl = wa2_call(ctx, "_close_bridge_log", json_object());
    if (wa2_is_raise(cl)) { free(msg); free(names); free(blogs); free(detail); free(code); return cl; }
    json_free(cl);
    snprintf(msg, ml, "%s (see %s)", detail, blogs);
    json_t *fa = json_object();
    wa2_set_s(fa, "code", code);
    wa2_set_s(fa, "message", msg);
    wa2_set_b(fa, "retryable", !needs_passkey);
    json_t *fe = wa2_call(ctx, "_set_fatal_error", fa);
    free(msg); free(names); free(blogs); free(detail); free(code);
    if (wa2_is_raise(fe)) return fe;
    json_free(fe);
    return json_bool(false);
}

/* PoP: wa2__close_bridge_log @ plugins/platforms/whatsapp/adapter.py:_close_bridge_log */
static json_t *wa2__close_bridge_log(wa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *slf = wa2_self(ctx);
    json_t *err = NULL;
    const json_t *fh = wa2_attr(slf, "_bridge_log_fh", &err);
    if (err) return err;
    if (wa2_truthy(fh)) {
        json_t *cr = wa2_call(ctx, "fh.close", json_object());
        if (wa2_is_raise(cr)) {
            const char *k = wa2_kind(cr);
            if (wa2_kind_is(k, "Exception") && !wa2_is_base_exc(k)) {
                json_free(cr);  /* suppress(Exception) */
            } else {
                return cr;  /* BaseException passes; fh NOT cleared */
            }
        } else {
            json_free(cr);
        }
        json_set(slf, "_bridge_log_fh", json_null());
    }
    return NULL;
}

/* PoP: wa2__terminate_bridge @ plugins/platforms/whatsapp/adapter.py:_terminate_bridge */
static json_t *wa2__terminate_bridge(wa2_ctx_t *ctx, const json_t *args) {
    json_t *slf = wa2_self(ctx);
    bool force = json_get_bool(args, "force", false);
    json_t *err = NULL;
    const json_t *proc = wa2_attr(slf, "_bridge_process", &err);
    if (err) return err;
    json_t *ta = json_object();
    wa2_set_copy(ta, "proc", proc);
    wa2_set_b(ta, "force", force);
    json_t *tp = wa2_call(ctx, "_terminate_bridge_process", ta);
    if (wa2_is_raise(tp)) {
        const char *k = wa2_kind(tp);
        if ((wa2_kind_is(k, "ProcessLookupError") || wa2_kind_is(k, "PermissionError"))
            && !wa2_is_base_exc(k)) {
            json_free(tp);
            /* getattr(proc, action) resolves the METHOD first: a non-object
             * proc raises the action-named AttributeError (no pid read);
             * an object proc's pid is read by the recorder (missing ->
             * 'pid'-named AttributeError). */
            if (!proc || proc->type != JSON_OBJECT) {
                char msg[256];
                snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
                         wa2_type_name(proc), force ? "kill" : "terminate");
                return wa2_err("AttributeError", msg);
            }
            const json_t *pid = wa2_attr(proc, "pid", &err);
            if (err) return err;
            json_t *fa = json_object();
            wa2_set_copy(fa, "pid", pid);
            char nm[64];
            snprintf(nm, sizeof nm, "proc.%s", force ? "kill" : "terminate");
            json_t *fr = wa2_call(ctx, nm, fa);
            if (wa2_is_raise(fr)) return fr;
            json_free(fr);
            return NULL;  /* statement-position call */
        }
        return tp;
    }
    json_free(tp);
    return NULL;
}

/* PoP: wa2__check_managed_bridge_exit @ plugins/platforms/whatsapp/adapter.py:_check_managed_bridge_exit */
static json_t *wa2__check_managed_bridge_exit(wa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *slf = wa2_self(ctx);
    json_t *err = NULL;
    const json_t *proc = wa2_attr(slf, "_bridge_process", &err);
    if (err) return err;
    json_t *rc = NULL;
    if (proc && proc->type != JSON_NULL) {
        const json_t *pid = wa2_attr(proc, "pid", &err);
        if (err) return err;
        json_t *pa = json_object();
        wa2_set_copy(pa, "pid", pid);
        rc = wa2_call(ctx, "proc.poll", pa);
        if (wa2_is_raise(rc)) return rc;
    }
    if (!rc || rc->type == JSON_NULL) {
        if (rc) json_free(rc);
        return NULL;
    }
    /* getattr(getattr(self, "gateway_runner", None), "_stop_requested_by_signal", False) */
    const json_t *gr = json_obj_get(slf, "gateway_runner");
    bool runner_stop = false;
    if (gr && gr->type == JSON_OBJECT) {
        const json_t *rs = json_obj_get(gr, "_stop_requested_by_signal");
        runner_stop = rs ? wa2_truthy(rs) : false;
    }
    const json_t *sd = json_obj_get(slf, "_shutting_down");
    bool shutting = sd ? wa2_truthy(sd) : false;
    double rcv = rc->type == JSON_NUMBER ? rc->num_val : (rc->type == JSON_BOOL ? (rc->bool_val ? 1.0 : 0.0) : 1.0);
    bool clean_code = (rcv == 0.0 || rcv == -2.0 || rcv == -15.0);
    const json_t *name = wa2_attr(slf, "name", &err);
    if (err) { json_free(rc); return err; }
    char *names = wa2_str_proj(name);
    if ((shutting || runner_stop) && clean_code) {
        json_t *la = json_object();
        wa2_set_s(la, "msg", "[%s] Bridge exited during shutdown (code %d).");
        json_t *arr = json_array();
        json_append(arr, json_string(names));
        json_append(arr, json_copy(rc));
        json_set(la, "args", arr);
        json_t *li = wa2_call(ctx, "logger.info", la);
        free(names);
        json_free(rc);
        if (wa2_is_raise(li)) return li;
        json_free(li);
        return NULL;
    }
    char *rcs = wa2_str_proj(rc);
    size_t ml = strlen(rcs) + 64;
    char *message = malloc(ml);
    snprintf(message, ml, "WhatsApp bridge process exited unexpectedly (code %s).", rcs);
    free(rcs);
    const json_t *hfe = wa2_attr(slf, "has_fatal_error", &err);
    if (err) { free(message); free(names); json_free(rc); return err; }
    if (!wa2_truthy(hfe)) {
        json_t *la = json_object();
        wa2_set_s(la, "msg", "[%s] %s");
        json_t *arr = json_array();
        json_append(arr, json_string(names));
        json_append(arr, json_string(message));
        json_set(la, "args", arr);
        json_t *le = wa2_call(ctx, "logger.error", la);
        if (wa2_is_raise(le)) { free(message); free(names); json_free(rc); return le; }
        json_free(le);
        json_t *ba = json_object();
        wa2_set_s(ba, "detail", message);
        wa2_set_s(ba, "code", "whatsapp_bridge_exited");
        json_t *bd = wa2_call(ctx, "_bridge_died", ba);
        if (wa2_is_raise(bd)) { free(message); free(names); json_free(rc); return bd; }
        json_free(bd);
        json_t *nf = wa2_call(ctx, "_notify_fatal_error", json_object());
        if (wa2_is_raise(nf)) { free(message); free(names); json_free(rc); return nf; }
        json_free(nf);
    }
    const json_t *fem = wa2_attr(slf, "fatal_error_message", &err);
    if (err) { free(message); free(names); json_free(rc); return err; }
    json_t *ret = wa2_truthy(fem) ? json_copy(fem) : json_string(message);
    free(message);
    free(names);
    json_free(rc);
    return ret;
}

/* PoP: wa2__inject_document_text @ plugins/platforms/whatsapp/adapter.py:_inject_document_text */
static const char *WA2_TEXT_INJECT_EXTS[] = {
    ".txt", ".md", ".csv", ".json", ".xml", ".yaml", ".yml", ".log",
    ".py", ".js", ".ts", ".html", ".css",
};
static json_t *wa2__inject_document_text(wa2_ctx_t *ctx, const json_t *args) {
    json_t *slf = wa2_self(ctx);
    const json_t *urls = json_obj_get(args, "cached_urls");
    char *body = wa2_strdup(json_get_str(args, "body", ""));
    json_t *err = NULL;
    const json_t *name = wa2_attr(slf, "name", &err);
    if (err) { free(body); return err; }
    char *names = wa2_str_proj(name);
    size_t n = json_len(urls);
    json_t *inlined = json_array();
    for (size_t i = 0; i < n; i++) json_append(inlined, json_bool(false));
    for (size_t i = 0; i < n; i++) {
        const char *doc_path = wa2_str(json_get(urls, i));
        char *suf = wa2_path_suffix(doc_path);
        char *sufl = wa2_lower_ascii(suf);
        free(suf);
        bool injectable = false;
        for (size_t e = 0; e < sizeof(WA2_TEXT_INJECT_EXTS) / sizeof(WA2_TEXT_INJECT_EXTS[0]); e++) {
            if (strcmp(sufl, WA2_TEXT_INJECT_EXTS[e]) == 0) { injectable = true; break; }
        }
        free(sufl);
        if (!injectable) continue;
        /* try body: any Exception -> print failed; BaseException propagates */
        json_t *failure = NULL;
        char *content = NULL;
        json_t *st = wa2_call(ctx, "stat", ({
            json_t *a = json_object();
            wa2_set_s(a, "path", doc_path);
            a;
        }));
        if (wa2_is_raise(st)) { failure = st; goto handled; }
        {
            const json_t *sz = wa2_attr(st, "st_size", &err);
            if (err) { json_free(st); failure = err; goto handled; }
            double szv = sz->type == JSON_NUMBER ? sz->num_val : (sz->type == JSON_BOOL ? (sz->bool_val ? 1.0 : 0.0) : 0.0);
            if (sz->type != JSON_NUMBER && sz->type != JSON_BOOL) {
                char msg[256];
                snprintf(msg, sizeof msg, "'>' not supported between instances of '%s' and 'int'", wa2_type_name(sz));
                json_free(st);
                failure = wa2_err("TypeError", msg);
                goto handled;
            }
            if (szv > 102400.0) {
                char *szs = wa2_str_proj(sz);
                size_t pl = strlen(names) + strlen(doc_path) + strlen(szs) + 96;
                char *pt = malloc(pl);
                snprintf(pt, pl, "[%s] Skipping text injection for %s (%s bytes > 102400)", names, doc_path, szs);
                free(szs);
                json_t *pa = json_object();
                wa2_set_s(pa, "text", pt);
                wa2_set_b(pa, "flush", true);
                free(pt);
                json_t *pr = wa2_call(ctx, "print", pa);
                json_free(st);
                if (wa2_is_raise(pr)) { failure = pr; goto handled; }
                json_free(pr);
                continue;
            }
            json_free(st);
        }
        {
            json_t *rt = wa2_call(ctx, "read_text", ({
                json_t *a = json_object();
                wa2_set_s(a, "path", doc_path);
                wa2_set_s(a, "encoding", "utf-8-sig");
                wa2_set_s(a, "errors", "replace");
                a;
            }));
            if (wa2_is_raise(rt)) { failure = rt; goto handled; }
            content = wa2_bom_strip(wa2_str(rt));
            json_free(rt);
        }
        {
            char *pname = wa2_path_name(doc_path);
            size_t np = 0;
            char **parts = wa2_split_char(pname, '_', 2, &np);
            const char *display = np >= 3 ? parts[2] : pname;
            size_t il = strlen(display) + strlen(content) + 32;
            char *injection = malloc(il);
            snprintf(injection, il, "[Content of %s]:\n%s", display, content);
            wa2_free_parts(parts, np);
            free(pname);
            free(content);
            content = NULL;
            char *newbody;
            if (body[0]) {
                size_t bl = strlen(injection) + strlen(body) + 4;
                newbody = malloc(bl);
                snprintf(newbody, bl, "%s\n\n%s", injection, body);
            } else {
                newbody = wa2_strdup(injection);
            }
            free(injection);
            free(body);
            body = newbody;
            json_array_set(inlined, i, json_bool(true));
            size_t pl = strlen(names) + strlen(doc_path) + 64;
            char *pt = malloc(pl);
            snprintf(pt, pl, "[%s] Injected text content from: %s", names, doc_path);
            json_t *pa = json_object();
            wa2_set_s(pa, "text", pt);
            wa2_set_b(pa, "flush", true);
            free(pt);
            json_t *pr = wa2_call(ctx, "print", pa);
            if (wa2_is_raise(pr)) { failure = pr; goto handled; }
            json_free(pr);
        }
handled:
        if (failure) {
            const char *k = wa2_kind(failure);
            if (wa2_kind_is(k, "Exception") && !wa2_is_base_exc(k)) {
                size_t pl = strlen(names) + strlen(json_get_str(failure, "message", "")) + 64;
                char *pt = malloc(pl);
                snprintf(pt, pl, "[%s] Failed to read document text: %s", names, json_get_str(failure, "message", ""));
                json_free(failure);
                json_t *pa = json_object();
                wa2_set_s(pa, "text", pt);
                wa2_set_b(pa, "flush", true);
                free(pt);
                json_t *pr = wa2_call(ctx, "print", pa);
                if (wa2_is_raise(pr)) { free(body); free(names); json_free(inlined); return pr; }
                json_free(pr);
            } else {
                free(body);
                free(names);
                json_free(inlined);
                return failure;
            }
        }
    }
    free(names);
    json_t *ret = json_array();
    json_append(ret, json_string(body));
    json_append(ret, inlined);
    free(body);
    return ret;
}

/* PoP: wa2__quoted_media @ plugins/platforms/whatsapp/adapter.py:_quoted_media */
static const char *wa2_quoted_mime(const char *kind) {
    if (strcmp(kind, "image") == 0) return "image/jpeg";
    if (strcmp(kind, "video") == 0 || strcmp(kind, "gif") == 0) return "video/mp4";
    if (strcmp(kind, "audio") == 0 || strcmp(kind, "ptt") == 0) return "audio/ogg";
    if (strcmp(kind, "document") == 0) return "application/octet-stream";
    if (strcmp(kind, "sticker") == 0) return "image/webp";
    return "application/octet-stream";
}
static json_t *wa2__quoted_media(wa2_ctx_t *ctx, const json_t *args) {
    json_t *slf = wa2_self(ctx);
    const json_t *data = json_obj_get(args, "data");
    const json_t *raw_reply_id = json_obj_get(args, "raw_reply_id");
    json_t *err = NULL;
    const json_t *name = wa2_attr(slf, "name", &err);
    if (err) return err;
    char *names = wa2_str_proj(name);
    const json_t *qmt = data ? json_obj_get(data, "quotedMediaType") : NULL;
    char *qmts = wa2_truthy(qmt) ? wa2_str_proj(qmt) : wa2_strdup("");
    char *quoted_type = wa2_strip(qmts);
    free(qmts);
    json_t *accepted = json_array();
    const json_t *urls = data ? json_obj_get(data, "quotedMediaUrls") : NULL;
    if (!wa2_truthy(urls)) urls = NULL;
    for (size_t i = 0; urls && i < json_len(urls); i++) {
        const json_t *path = json_get(urls, i);
        bool ok = false;
        if (path && path->type == JSON_STRING) {
            const char *ps = path->str_val ? path->str_val : "";
            if (ps[0] == '/') {  /* posixpath.isabs */
                json_t *aa = json_object();
                wa2_set_s(aa, "url", ps);
                json_t *ab = wa2_call(ctx, "_is_allowed_bridge_path", aa);
                if (wa2_is_raise(ab)) { free(names); free(quoted_type); json_free(accepted); return ab; }
                ok = wa2_truthy(ab);
                json_free(ab);
            }
        }
        if (!ok) {
            char *ps = wa2_str_proj(path);
            size_t pl = strlen(names) + strlen(ps) + 80;
            char *pt = malloc(pl);
            snprintf(pt, pl, "[%s] Rejected quoted-media path outside cache dir: %s", names, ps);
            free(ps);
            json_t *pa = json_object();
            wa2_set_s(pa, "text", pt);
            wa2_set_b(pa, "flush", true);
            free(pt);
            json_t *pr = wa2_call(ctx, "print", pa);
            if (wa2_is_raise(pr)) { free(names); free(quoted_type); json_free(accepted); return pr; }
            json_free(pr);
            continue;
        }
        json_t *pair = json_array();
        json_append(pair, json_copy(path));
        json_append(pair, json_string(wa2_quoted_mime(quoted_type)));
        json_append(accepted, pair);
    }
    if (json_len(accepted) == 0 && raw_reply_id && raw_reply_id->type != JSON_NULL) {
        const json_t *cid = data ? json_obj_get(data, "chatId") : NULL;
        char *rid = wa2_str_proj(raw_reply_id);
        json_t *la = json_object();
        wa2_set_s(la, "chat_id", cid ? wa2_str(cid) : "");
        wa2_set_s(la, "reply_id", rid);
        free(rid);
        json_t *lm = wa2_call(ctx, "lookup_media", la);
        if (wa2_is_raise(lm)) { free(names); free(quoted_type); json_free(accepted); return lm; }
        json_free(accepted);
        accepted = lm;
    }
    for (size_t i = 0; i < json_len(accepted); i++) {
        const json_t *pair = json_get(accepted, i);
        char *ps = wa2_str_proj(json_get(pair, 0));
        size_t pl = strlen(names) + strlen(ps) + 64;
        char *pt = malloc(pl);
        snprintf(pt, pl, "[%s] Attached quoted-reply media: %s", names, ps);
        free(ps);
        json_t *pa = json_object();
        wa2_set_s(pa, "text", pt);
        wa2_set_b(pa, "flush", true);
        free(pt);
        json_t *pr = wa2_call(ctx, "print", pa);
        if (wa2_is_raise(pr)) { free(names); free(quoted_type); json_free(accepted); return pr; }
        json_free(pr);
    }
    free(names);
    free(quoted_type);
    return accepted;
}

/* PoP: wa2__post_bridge_message @ plugins/platforms/whatsapp/adapter.py:_post_bridge_message */
static json_t *wa2__post_bridge_message(wa2_ctx_t *ctx, const json_t *args) {
    const char *path = json_get_str(args, "path", "");
    const json_t *payload = json_obj_get(args, "payload");
    const json_t *timeout = json_obj_get(args, "timeout");
    json_t *pending = NULL;
    json_t *ret = NULL;
    json_t *kw = json_object();
    json_set(kw, "json", payload ? json_copy(payload) : json_null());
    json_t *ba = json_object();
    wa2_set_s(ba, "method", "post");
    wa2_set_s(ba, "path", path);
    wa2_set_copy(ba, "timeout", timeout);
    json_set(ba, "kwargs", kw);
    json_t *br = wa2_call(ctx, "_bridge_req", ba);
    if (wa2_is_raise(br)) { pending = br; goto handled; }
    {
        json_t *en = wa2_call(ctx, "resp.enter", json_object());
        if (wa2_is_raise(en)) { pending = en; goto cm_exit; }
        json_free(en);
        json_t *err = NULL;
        const json_t *status = wa2_attr(br, "status", &err);
        if (err) { pending = err; goto cm_exit; }
        if (status->type == JSON_NUMBER && status->num_val == 200.0) {
            json_t *dj = wa2_call(ctx, "resp.json", json_object());
            if (wa2_is_raise(dj)) { pending = dj; goto cm_exit; }
            if (!dj || dj->type != JSON_OBJECT) {
                char msg[256];
                snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", wa2_type_name(dj));
                json_free(dj);
                pending = wa2_err("AttributeError", msg);
                goto cm_exit;
            }
            ret = wa2_send_result(true, json_obj_get(dj, "messageId"), NULL, dj);
            json_free(dj);
        } else {
            json_t *tx = wa2_call(ctx, "resp.text", json_object());
            if (wa2_is_raise(tx)) { pending = tx; goto cm_exit; }
            ret = wa2_send_result(false, NULL, tx, NULL);
            json_free(tx);
        }
cm_exit:
        {
            bool suppressed = false;
            json_t *final = wa2_cm_exit(ctx, "resp.exit", pending, &suppressed);
            if (final) {
                if (ret) json_free(ret);
                json_free(br);
                return final;
            }
            if (suppressed) {
                if (ret) json_free(ret);
                json_free(br);
                return NULL;
            }
            pending = NULL;
        }
    }
    json_free(br);
    return ret;
handled:
    /* except Exception as e: SendResult(success=False, error=str(e)) */
    {
        const char *k = wa2_kind(pending);
        if (wa2_kind_is(k, "Exception") && !wa2_is_base_exc(k)) {
            json_t *r = wa2_send_result(false, NULL, pending, NULL);
            /* error carries str(e) == the message string */
            json_set(r, "error", json_string(json_get_str(pending, "message", "")));
            json_free(pending);
            return r;
        }
        return pending;
    }
}

/* PoP: wa2__apply_yaml_config @ plugins/platforms/whatsapp/adapter.py:_apply_yaml_config */
static json_t *wa2__apply_yaml_config(wa2_ctx_t *ctx, const json_t *args) {
    static const char *YK[] = {
        "require_mention", "dm_policy", "group_policy", "mention_patterns",
        "free_response_chats", "allow_from", "group_allow_from",
    };
    static const char *YE[] = {
        "WHATSAPP_REQUIRE_MENTION", "WHATSAPP_DM_POLICY", "WHATSAPP_GROUP_POLICY",
        "WHATSAPP_MENTION_PATTERNS", "WHATSAPP_FREE_RESPONSE_CHATS",
        "WHATSAPP_ALLOWED_USERS", "WHATSAPP_GROUP_ALLOWED_USERS",
    };
    static const char *YT[] = { "lower", "lower", "lower", "json", "csv", "csv", "csv" };
    json_t *table = json_array();
    for (size_t i = 0; i < 7; i++) {
        json_t *row = json_array();
        json_append(row, json_string(YK[i]));
        json_append(row, json_string(YE[i]));
        json_append(row, json_string(YT[i]));
        json_append(table, row);
    }
    json_t *a = json_object();
    wa2_set_copy(a, "whatsapp_cfg", json_obj_get(args, "whatsapp_cfg"));
    json_set(a, "yaml_bridge", table);
    return wa2_call(ctx, "_apply_yaml_bridge", a);
}

/* PoP: wa2__may_transcribe_at_gate @ plugins/platforms/whatsapp/adapter.py:_may_transcribe_at_gate */
static json_t *wa2__may_transcribe_at_gate(wa2_ctx_t *ctx, const json_t *args) {
    json_t *slf = wa2_self(ctx);
    const json_t *data = json_obj_get(args, "data");
    const json_t *ig = data ? json_obj_get(data, "isGroup") : NULL;
    if (!wa2_truthy(ig)) return json_bool(false);
    const json_t *mt = data ? json_obj_get(data, "mediaType") : NULL;
    char *mts = wa2_truthy(mt) ? wa2_str_proj(mt) : wa2_strdup("");
    bool ptt = strcmp(mts, "ptt") == 0;
    free(mts);
    if (!ptt) return json_bool(false);
    json_t *err = NULL;
    const json_t *config = wa2_attr(slf, "config", &err);
    if (err) return err;
    const json_t *extra = wa2_attr(config, "extra", &err);
    if (err) return err;
    json_t *va = json_object();
    wa2_set_copy(va, "extra", extra);
    json_t *vg = wa2_call(ctx, "voice_mention_gate_enabled", va);
    if (wa2_is_raise(vg)) return vg;
    bool r = wa2_truthy(vg);
    json_free(vg);
    return json_bool(r);
}

/* PoP: wa2__log_delivery_status @ plugins/platforms/whatsapp/adapter.py:_log_delivery_status */
static json_t *wa2__log_delivery_status(wa2_ctx_t *ctx, const json_t *args) {
    json_t *slf = wa2_self(ctx);
    const json_t *data = json_obj_get(args, "data");
    json_t *err = NULL;
    const json_t *name = wa2_attr(slf, "name", &err);
    if (err) return err;
    const json_t *rid = data ? json_obj_get(data, "recipientId") : NULL;
    const json_t *er = data ? json_obj_get(data, "error") : NULL;
    char *recip = wa2_strdup("");
    if (wa2_truthy(rid)) {
        char *rs = wa2_str_proj(rid);
        size_t rl = strlen(rs) + 16;
        recip = malloc(rl);
        snprintf(recip, rl, " recipient=%s", rs);
        free(rs);
    }
    char *errpart = wa2_strdup("");
    if (wa2_truthy(er)) {
        char *es = wa2_str_proj(er);
        size_t el = strlen(es) + 16;
        errpart = malloc(el);
        snprintf(errpart, el, " error=%s", es);
        free(es);
    }
    json_t *a = json_object();
    wa2_set_s(a, "msg", "[%s] WhatsApp delivery status: message=%s chat=%s status=%s%s at=%s%s");
    json_t *arr = json_array();
    json_append(arr, json_copy(name));
    json_append(arr, json_copy(data ? json_obj_get(data, "messageId") : NULL));
    json_append(arr, json_copy(data ? json_obj_get(data, "chatId") : NULL));
    json_append(arr, json_copy(data ? json_obj_get(data, "status") : NULL));
    json_append(arr, json_string(recip));
    json_append(arr, json_copy(data ? json_obj_get(data, "timestamp") : NULL));
    json_append(arr, json_string(errpart));
    json_set(a, "args", arr);
    free(recip);
    free(errpart);
    json_t *li = wa2_call(ctx, "logger.info", a);
    if (wa2_is_raise(li)) return li;
    json_free(li);
    return NULL;
}