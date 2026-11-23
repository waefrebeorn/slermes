/* mcp_oauth_flow2.c — FLOW port of the UNCLAIMED remainder of tools/mcp_oauth.py
 * (MCP OAuth 2.1): the stdin console probe, pydantic model JSON dump, storage
 * path derivation / state-path tuple / issuer binding / credential stat stamp,
 * redirect-query parsing, the callback HTTP handler trio (do_GET / log_request /
 * log_message), the authorization-URL announcement, the redirect-handler and
 * callback-waiter coroutine bodies, the callback-server close lifecycle
 * (settle / close / _close_callback_server) and the login-timeout seconds
 * coercion. Claimed siblings (_safe_filename, _get_token_dir, _read_json,
 * _write_json, _is_interactive, _raise_if_non_interactive, _can_open_browser,
 * _result_taken, _claim_result, _make_callback_handler, _start_callback_server,
 * _callback_outcome, _authorization_code_result, the storage partialmethods,
 * get_dashboard_oauth_flow) are SEAMED, never re-ported.
 *
 * Seam model mirrors mcp_oauth_flow: every live I/O call goes through
 * mo2_io_t.call(io, name, args); the dispatcher appends `name` to io->trace
 * (ordered seam-name sequence), returns the scripted result, and models a
 * raised exception with {"__raise__": "<Type>", "message": "..."}. Pure
 * unclaimed siblings (_parse_redirect_query, _seconds, the %-format render,
 * html.escape, the URL/query parser) are ported directly and perform no I/O.
 * io->state is a json_t OBJECT mirroring the storage attributes
 * (_server_name, _hermes_home, _bound_issuer, ...).
 *
 * Seam name -> live call (args informational; the trace records names only):
 *   sys.stdin.isatty / sys.stdin.fileno   sys.stdin.<m>()
 *   msvcrt.get_osfhandle                  msvcrt.get_osfhandle(fileno)
 *   ctypes.c_ulong / c_void_p / byref     ctypes constructors
 *   ctypes.windll.kernel32.GetConsoleMode GetConsoleMode(handle, &mode)
 *   model_dump                            model.model_dump(mode="json", exclude_none=True)
 *   get_token_dir                         _get_token_dir(self._hermes_home)  [claimed]
 *   storage._tokens_path / _client_info_path / _meta_path  partialmethods [claimed]
 *   path.stat                             path.stat() -> {st_mtime_ns, st_size, st_ino}
 *   claim_result                          _claim_result(result, lock, **values) [claimed]
 *   send_response / send_header / end_headers / wfile.write  handler I/O
 *   logger.debug                          logger.debug(fmt, ...)
 *   print                                 print(text, file=sys.stderr[, flush=True])
 *   os.getenv                             os.getenv(key)
 *   can_open_browser                      _can_open_browser()  [claimed]
 *   webbrowser.open                       webbrowser.open(url)
 *   get_dashboard_oauth_flow              get_dashboard_oauth_flow()  [imported]
 *   dashboard_flow.publish_authorization_url / wait_for_callback  await dashboard I/O
 *   raise_if_non_interactive              _raise_if_non_interactive(lead)  [claimed]
 *   is_interactive                        _is_interactive()  [claimed]
 *   make_callback_handler                 _make_callback_handler() -> {handler, result} [claimed]
 *   start_callback_server                 _start_callback_server(port, handler) [claimed]
 *   result_taken                          _result_taken(result)  [claimed]
 *   asyncio.sleep / asyncio.shield / asyncio.get_running_loop
 *   loop.create_future / loop.call_soon_threadsafe
 *   future.done / future.set_result
 *   threading.Thread / thread.start       Thread(...).start()
 *   server.shutdown / server.server_close
 *   authorization_code_result             _authorization_code_result(*args) [claimed]
 *   callback_outcome                      _callback_outcome(result, cimd_url) [claimed]
 *
 * Deterministic threading model: the close-thread (name "mcp-oauth-cb-close")
 * runs close() synchronously at thread.start; loop.call_soon_threadsafe(settle)
 * runs settle() inline. The waiter's serve_forever / paste-reader threads are
 * inert (recorded, never run). A raise escaping close() in the PARENT drive
 * propagates (fixtures reserve escaping close raises for direct close drives;
 * live would strand them on the daemon thread).
 *
 * contextlib.suppress(Exception) around webbrowser.open is modeled as catch
 * semantics only (no CM seam events), matching mcp_oauth_flow's suppress model.
 *
 * Scripted exception kinds carry the live class name; BaseException-only kinds
 * (SystemExit, KeyboardInterrupt, GeneratorExit, asyncio.CancelledError) PASS
 * THROUGH every `except Exception`/OSError/... site. The exception-parent table
 * covers the FULL builtin hierarchy (plus RefreshFenceTimeout /
 * OAuthNonInteractiveError -> RuntimeError, JSONDecodeError -> ValueError,
 * CancelledError -> BaseException); unknown scripted kinds parent to Exception
 * (the oracle's make_exc builds them as Exception subclasses).
 *
 * Deterministic subset: paths are POSIX-style strings (no drive letters / UNC;
 * exactly-two leading slashes preserved like PurePosixPath); URL/query
 * fixtures are ASCII (the C parsers model the ASCII subset of urllib.parse,
 * verified against the live parser); %XX escapes in fixtures decode to ASCII
 * (the decoder implements full UTF-8-with-replacement regardless). Integral
 * JSON numbers read as int live: type-name message positions (model carrier,
 * float() argument) and str()-computed positions avoid integral floats in
 * fixtures; the oracle normalizes integral floats to int on both sides.
 * Integral doubles with |x| >= 1e15 are also excluded from str()-computed
 * positions (JSON numbers lose the int/float distinction there).
 * log_message fixtures use the %s/%r/%d/%% subset (flags/width/precision and
 * non-trivial float reprs excluded). _seconds fixtures avoid inf/nan returns.
 * do_GET/log_request URL fixtures are origin-form or simple absolute URLs;
 * bracketed-host fixtures cover only the eager ValueError branches.
 *
 * Ownership: seam results returned by call are owned by the caller (json_free).
 * io->trace / io->state are owned by the harness. mo2_seam() frees its args.
 * json_set ADOPTS its value and frees the replaced node.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to tools/mcp_oauth.py) ─────────── */
static const char MO2_SSH_HINT_PROXY[] =
    "  Remote session detected. After you authorize, the provider redirects to\n"
    "    {redirect_uri}\n"
    "  which forwards to the callback listener on this machine — no SSH tunnel needed.\n";
static const char MO2_SSH_HINT_LOOPBACK[] =
    "  Remote session detected. After you authorize, the provider redirects to\n"
    "    http://{host}:{port}/callback\n"
    "  which only the listener on THIS machine can receive. Two options:\n"
    "\n"
    "    1. Easiest — when your browser shows a connection error after\n"
    "       authorizing, copy the full URL from the address bar and paste\n"
    "       it at the prompt below. The pasted ``code=...&state=...`` is\n"
    "       enough to complete the flow.\n"
    "\n"
    "    2. Or forward the port first in a separate terminal:\n"
    "         ssh -N -L {port}:127.0.0.1:{port} <user>@<this-host>\n"
    "       then open the URL above and let it redirect normally.\n"
    "\n"
    "  See: https://hermes-agent.nousresearch.com/docs/guides/oauth-over-ssh\n";
static const char MO2_REDIRECT_LEAD[] =
    "MCP OAuth requires browser authorization but no interactive session is available "
    "(non-interactive/background context).";
static const char MO2_WAIT_LEAD[] =
    "OAuth callback requires an interactive session but none is available "
    "(non-interactive/background context); skipping browser authorization without "
    "binding a callback listener.";
static const char MO2_PASTE_PROMPT[] =
    "\n  Or paste the redirect URL here (or the ``?code=...&state=...`` portion) and press Enter. "
    "Type ``skip`` + Enter to continue without this server:";
static const char MO2_NOTE_HEADLESS[] =
    "Headless environment detected — open the URL manually.";
static const char MO2_NOTE_OPENED[] = "Browser opened automatically.";
static const char MO2_NOTE_NOT_OPENED[] =
    "Could not open browser — please open the URL manually.";

/* ── generic seam dispatcher + trace + attribute state ─────────────────── */
typedef struct mo2_io_t {
    json_t *(*call)(struct mo2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} mo2_io_t;

static void mo2_trace(mo2_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *mo2_call(mo2_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool mo2_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

static json_t *mo2_seam(mo2_io_t *io, const char *name, json_t *args) {
    mo2_trace(io, name);
    json_t *r = mo2_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}

static json_t *mo2_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

static const char *mo2_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = mo2_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}

static bool mo2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static bool mo2_not_none(const json_t *v) {
    return v && v->type != JSON_NULL;
}

static json_t *mo2_state_get(mo2_io_t *io, const char *key) { return mo2_get(io->state, key); }
static void mo2_state_set(mo2_io_t *io, const char *key, json_t *val) {
    if (io && io->state && io->state->type == JSON_OBJECT) json_set(io->state, key, val);
    else if (val) json_free(val);
}

/* ── exception model (FULL builtin hierarchy) ──────────────────────────── */
static json_t *mo2_raise(const char *kind, const char *message) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

static const char *mo2_exc_kind(const json_t *r) { return mo2_get_str(r, "__raise__", ""); }
static const char *mo2_exc_msg(const json_t *r) { return mo2_get_str(r, "message", ""); }

static const char *mo2_exc_parent(const char *k) {
    if (strcmp(k, "BaseException") == 0) return NULL;
    if (strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0)
        return "BaseException";
    if (strcmp(k, "Exception") == 0) return "BaseException";
    if (strcmp(k, "ExceptionGroup") == 0) return "Exception";
    if (strcmp(k, "ArithmeticError") == 0 || strcmp(k, "AssertionError") == 0
        || strcmp(k, "AttributeError") == 0 || strcmp(k, "BufferError") == 0
        || strcmp(k, "EOFError") == 0 || strcmp(k, "ImportError") == 0
        || strcmp(k, "LookupError") == 0 || strcmp(k, "MemoryError") == 0
        || strcmp(k, "NameError") == 0 || strcmp(k, "OSError") == 0
        || strcmp(k, "ReferenceError") == 0 || strcmp(k, "RuntimeError") == 0
        || strcmp(k, "StopAsyncIteration") == 0 || strcmp(k, "StopIteration") == 0
        || strcmp(k, "SyntaxError") == 0 || strcmp(k, "SystemError") == 0
        || strcmp(k, "TypeError") == 0 || strcmp(k, "ValueError") == 0
        || strcmp(k, "Warning") == 0)
        return "Exception";
    if (strcmp(k, "FloatingPointError") == 0 || strcmp(k, "OverflowError") == 0
        || strcmp(k, "ZeroDivisionError") == 0) return "ArithmeticError";
    if (strcmp(k, "ModuleNotFoundError") == 0) return "ImportError";
    if (strcmp(k, "IndexError") == 0 || strcmp(k, "KeyError") == 0) return "LookupError";
    if (strcmp(k, "UnboundLocalError") == 0) return "NameError";
    if (strcmp(k, "BlockingIOError") == 0 || strcmp(k, "ChildProcessError") == 0
        || strcmp(k, "ConnectionError") == 0 || strcmp(k, "FileExistsError") == 0
        || strcmp(k, "FileNotFoundError") == 0 || strcmp(k, "InterruptedError") == 0
        || strcmp(k, "IsADirectoryError") == 0 || strcmp(k, "NotADirectoryError") == 0
        || strcmp(k, "PermissionError") == 0 || strcmp(k, "ProcessLookupError") == 0
        || strcmp(k, "TimeoutError") == 0) return "OSError";
    if (strcmp(k, "BrokenPipeError") == 0 || strcmp(k, "ConnectionAbortedError") == 0
        || strcmp(k, "ConnectionRefusedError") == 0 || strcmp(k, "ConnectionResetError") == 0)
        return "ConnectionError";
    if (strcmp(k, "NotImplementedError") == 0 || strcmp(k, "RecursionError") == 0
        || strcmp(k, "RefreshFenceTimeout") == 0 || strcmp(k, "OAuthNonInteractiveError") == 0)
        return "RuntimeError";
    if (strcmp(k, "IndentationError") == 0) return "SyntaxError";
    if (strcmp(k, "TabError") == 0) return "IndentationError";
    if (strcmp(k, "UnicodeError") == 0) return "ValueError";
    if (strcmp(k, "UnicodeDecodeError") == 0 || strcmp(k, "UnicodeEncodeError") == 0
        || strcmp(k, "UnicodeTranslateError") == 0) return "UnicodeError";
    if (strcmp(k, "JSONDecodeError") == 0) return "ValueError";
    if (strcmp(k, "DeprecationWarning") == 0 || strcmp(k, "PendingDeprecationWarning") == 0
        || strcmp(k, "RuntimeWarning") == 0 || strcmp(k, "SyntaxWarning") == 0
        || strcmp(k, "UserWarning") == 0 || strcmp(k, "FutureWarning") == 0
        || strcmp(k, "ImportWarning") == 0 || strcmp(k, "UnicodeWarning") == 0
        || strcmp(k, "BytesWarning") == 0 || strcmp(k, "EncodingWarning") == 0
        || strcmp(k, "ResourceWarning") == 0) return "Warning";
    /* unknown scripted kind: the oracle's make_exc builds Exception subclasses */
    return "Exception";
}

static bool mo2_exc_is(const json_t *r, const char *base) {
    if (!mo2_raised(r)) return false;
    const char *k = mo2_exc_kind(r);
    for (int depth = 0; k && depth < 32; depth++) {
        if (strcmp(k, base) == 0) return true;
        k = mo2_exc_parent(k);
    }
    return false;
}

/* BaseException-only kinds pass through every `except Exception` site. */
static bool mo2_is_base_exc(const json_t *r) {
    return mo2_raised(r) && !mo2_exc_is(r, "Exception");
}

/* ── Python str() / repr() for scalar JSON values ──────────────────────── */
static char *mo2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    char *ser = json_serialize(v);  /* containers excluded from str() fixtures */
    return ser ? ser : strdup("");
}

/* repr() of a scalar: Python quote selection (single quotes unless the string
 * contains ' and no ", then double quotes). ASCII subset. */
static char *mo2_py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return mo2_pystr(v);
    if (v->type != JSON_STRING) return mo2_pystr(v);
    const char *s = v->str_val ? v->str_val : "";
    size_t n = strlen(s);
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    char *out = malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (unsigned char)q) { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            k += (size_t)snprintf(out + k, 5, "\\x%02x", c);
        }
        else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

/* type(v).__name__ for attribute-error messages (integral JSON numbers read
 * as int live; fixtures keep integral floats out of type-name positions). */
static const char *mo2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
                                 ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "NoneType";
    }
}

static json_t *mo2_attr_raise(const char *type_name, const char *attr) {
    char buf[256];
    snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", type_name, attr);
    return mo2_raise("AttributeError", buf);
}

/* ── string builder ────────────────────────────────────────────────────── */
typedef struct { char *s; size_t len, cap; } mo2_sb;
static void mo2_sb_init(mo2_sb *b) { b->cap = 64; b->len = 0; b->s = malloc(b->cap); if (b->s) b->s[0] = '\0'; }
static void mo2_sb_add(mo2_sb *b, const char *s) {
    if (!b->s || !s) return;
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2 + n;
        char *ns = realloc(b->s, nc);
        if (!ns) return;
        b->s = ns; b->cap = nc;
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}
static void mo2_sb_addch(mo2_sb *b, char c) {
    char tmp[2] = {c, '\0'};
    mo2_sb_add(b, tmp);
}
static char *mo2_sb_finish(mo2_sb *b) {
    char *s = b->s ? b->s : strdup("");
    b->s = NULL; b->len = b->cap = 0;
    return s;
}

/* replace ALL occurrences of needle in hay with val (str.format subset) */
static char *mo2_replace_all(const char *hay, const char *needle, const char *val) {
    mo2_sb b; mo2_sb_init(&b);
    size_t nl = strlen(needle);
    const char *p = hay;
    while (*p) {
        const char *hit = strstr(p, needle);
        if (!hit) { mo2_sb_add(&b, p); break; }
        size_t pre = (size_t)(hit - p);
        for (size_t i = 0; i < pre; i++) mo2_sb_addch(&b, p[i]);
        mo2_sb_add(&b, val);
        p = hit + nl;
    }
    return mo2_sb_finish(&b);
}

/* ── POSIX path normalization (Path() subset) + join ───────────────────── */
/* PurePosixPath keeps EXACTLY two leading slashes ('//host' implementation-
 * defined root); three or more collapse to one. '.' components dropped,
 * '..' kept, repeated slashes collapsed. */
static char *mo2_path_normalize(const char *p) {
    const char *s = p ? p : "";
    size_t n = strlen(s);
    char *out = malloc(n + 3);
    if (!out) return NULL;
    size_t k = 0;
    /* Windows seam paths use '\' as a separator too; a trailing separator is
     * dropped (PureWindowsPath("sC:\\") -> "sC:"). */
    bool rooted = (n > 0 && (s[0] == '/' || s[0] == '\\'));
    bool double_root = rooted && n > 1 && (s[1] == '/' || s[1] == '\\')
                       && !(n > 2 && (s[2] == '/' || s[2] == '\\'));
    if (rooted) out[k++] = '/';
    if (double_root) out[k++] = '/';
    size_t i = 0;
    bool first = true;
    while (i < n) {
        while (i < n && (s[i] == '/' || s[i] == '\\')) i++;
        size_t start = i;
        while (i < n && s[i] != '/' && s[i] != '\\') i++;
        size_t len = i - start;
        if (len == 0) continue;
        if (len == 1 && s[start] == '.') continue;
        if (!first || !rooted) {
            if (k > 0 && out[k - 1] != '/') out[k++] = '/';
        }
        memcpy(out + k, s + start, len);
        k += len;
        first = false;
    }
    if (k == 0) { out[k++] = '.'; }
    out[k] = '\0';
    return out;
}

/* PurePath.__truediv__: an absolute rhs (leading '/' or '\') resets the path. */
static char *mo2_path_join(const char *dir, const char *name) {
    if (name && (name[0] == '/' || name[0] == '\\')) return mo2_path_normalize(name);
    char *dn = mo2_path_normalize(dir);
    if (!dn) return NULL;
    mo2_sb b; mo2_sb_init(&b);
    if (strcmp(dn, ".") != 0) {
        mo2_sb_add(&b, dn);
        size_t dl = strlen(dn);
        if (dl == 0 || dn[dl - 1] != '/') mo2_sb_addch(&b, '/');
    }
    mo2_sb_add(&b, name ? name : "");
    free(dn);
    char *joined = mo2_sb_finish(&b);
    char *norm = mo2_path_normalize(joined);
    free(joined);
    return norm;
}

/* ── urllib.parse ASCII subset: path / query extraction ────────────────── */
/* IPv4 dotted-quad (no leading zeros, 0-255). */
static bool mo2_ipv4_valid(const char *s, size_t len) {
    int parts = 0;
    size_t i = 0;
    while (i < len) {
        size_t start = i;
        int v = 0;
        while (i < len && s[i] != '.') {
            if (s[i] < '0' || s[i] > '9') return false;
            v = v * 10 + (s[i] - '0');
            if (v > 255) return false;
            i++;
        }
        size_t dlen = i - start;
        if (dlen == 0 || (dlen > 1 && s[start] == '0')) return false;
        parts++;
        if (i < len && s[i] == '.') { i++; if (i == len) return false; }
    }
    return parts == 4;
}

static int mo2_seg_count(const char *s, bool tail_allowed, bool *bad) {
    if (*s == '\0') return 0;
    int count = 0;
    const char *p = s;
    while (*p) {
        const char *start = p;
        while (*p && *p != ':') p++;
        size_t len = (size_t)(p - start);
        if (len == 0) { *bad = true; return -1; }
        bool is_last = (*p == '\0');
        int dots = 0;
        for (size_t i = 0; i < len; i++) if (start[i] == '.') dots++;
        if (dots == 3) {
            if (!tail_allowed || !is_last || !mo2_ipv4_valid(start, len)) { *bad = true; return -1; }
            count += 2;
        } else {
            if (dots != 0 || len > 4) { *bad = true; return -1; }
            for (size_t i = 0; i < len; i++)
                if (!isxdigit((unsigned char)start[i])) { *bad = true; return -1; }
            count += 1;
        }
        if (*p == ':') {
            p++;
            if (*p == '\0') { *bad = true; return -1; }
        }
    }
    return count;
}

static bool mo2_ipv6_groups(const char *s) {
    const char *dc = strstr(s, "::");
    if (dc && strstr(dc + 2, "::")) return false;
    bool bad = false;
    if (dc) {
        size_t llen = (size_t)(dc - s);
        char *left = malloc(llen + 1);
        if (!left) return false;
        memcpy(left, s, llen); left[llen] = '\0';
        int lc = mo2_seg_count(left, false, &bad);
        free(left);
        if (bad) return false;
        int rc = mo2_seg_count(dc + 2, true, &bad);
        if (bad) return false;
        return lc + rc <= 7;
    }
    int c = mo2_seg_count(s, true, &bad);
    if (bad) return false;
    return c == 8;
}

/* 0 = valid bracketed host; 1 = not IPv6/IPvFuture; 2 = IPv4 in brackets. */
static int mo2_bracketed_host_check(const char *h) {
    if (mo2_ipv4_valid(h, strlen(h))) return 2;
    if (h[0] == 'v' || h[0] == 'V') {
        const char *p = h + 1;
        int hd = 0;
        while (isxdigit((unsigned char)*p)) { p++; hd++; }
        if (hd > 0 && *p == '.') {
            p++;
            if (*p == '\0') return 1;
            for (; *p; p++) {
                unsigned char c = (unsigned char)*p;
                if (isalnum(c) || strchr("-._~!$&'()*+,;=:", c)) continue;
                return 1;
            }
            return 0;
        }
        return 1;
    }
    char *base = strdup(h);
    if (!base) return 1;
    char *pct = strchr(base, '%');
    if (pct) *pct = '\0';
    bool ok = mo2_ipv6_groups(base);
    free(base);
    return ok ? 0 : 1;
}

/* urlsplit ASCII subset: returns malloc'd path/query; err kind/message on the
 * eager ValueError branches (bracket mismatch / bracketed host). */
static void mo2_urlparse(const char *url, char **path_out, char **query_out,
                         char *errmsg, size_t errcap) {
    *path_out = NULL; *query_out = NULL;
    errmsg[0] = '\0';
    const char *u = url ? url : "";
    /* urlsplit removes ASCII \t \r \n anywhere */
    size_t n = strlen(u);
    char *clean = malloc(n + 1);
    if (!clean) { snprintf(errmsg, errcap, "oom"); return; }
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = u[i];
        if (c == '\t' || c == '\r' || c == '\n') continue;
        clean[k++] = c;
    }
    clean[k] = '\0';
    /* fragment first */
    char *hash = strchr(clean, '#');
    if (hash) *hash = '\0';
    char *rest = clean;
    /* scheme: [A-Za-z][A-Za-z0-9+.-]*: */
    if (isalpha((unsigned char)clean[0])) {
        size_t j = 1;
        while (clean[j] && (isalnum((unsigned char)clean[j]) || clean[j] == '+'
               || clean[j] == '.' || clean[j] == '-')) j++;
        if (clean[j] == ':') rest = clean + j + 1;
    }
    /* netloc: a leading '//' starts it even WITHOUT a scheme (urlsplit) */
    if (rest[0] == '/' && rest[1] == '/') {
        char *netloc = rest + 2;
        char *end = netloc;
        while (*end && *end != '/' && *end != '?') end++;
        size_t nl = (size_t)(end - netloc);
        char *nlbuf = malloc(nl + 1);
        if (!nlbuf) { snprintf(errmsg, errcap, "oom"); free(clean); return; }
        memcpy(nlbuf, netloc, nl); nlbuf[nl] = '\0';
        bool has_open = strchr(nlbuf, '[') != NULL;
        bool has_close = strchr(nlbuf, ']') != NULL;
        if (has_open != has_close) {
            snprintf(errmsg, errcap, "Invalid IPv6 URL");
            free(nlbuf); free(clean); return;
        }
        if (nl > 0 && nlbuf[0] == '[') {
            char *cb = strchr(nlbuf, ']');
            if (cb) {
                *cb = '\0';
                char *host = nlbuf + 1;
                int chk = mo2_bracketed_host_check(host);
                if (chk == 2) {
                    snprintf(errmsg, errcap, "An IPv4 address cannot be in brackets");
                    free(nlbuf); free(clean); return;
                }
                if (chk == 1) {
                    snprintf(errmsg, errcap,
                             "'%s' does not appear to be an IPv4 or IPv6 address", host);
                    free(nlbuf); free(clean); return;
                }
            }
        }
        free(nlbuf);
        rest = end;
    }
    /* query split on the remainder */
    char *qmark = strchr(rest, '?');
    char *path;
    char *query;
    if (qmark) {
        *qmark = '\0';
        path = strdup(rest);
        query = strdup(qmark + 1);
    } else {
        path = strdup(rest);
        query = strdup("");
    }
    free(clean);
    *path_out = path;
    *query_out = query;
}

/* ── unquote_plus (UTF-8 with replacement) + parse_qs ASCII subset ─────── */
static void mo2_utf8_emit(mo2_sb *b, unsigned int cp) {
    if (cp < 0x80) mo2_sb_addch(b, (char)cp);
    else if (cp < 0x800) {
        mo2_sb_addch(b, (char)(0xC0 | (cp >> 6)));
        mo2_sb_addch(b, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        mo2_sb_addch(b, (char)(0xE0 | (cp >> 12)));
        mo2_sb_addch(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        mo2_sb_addch(b, (char)(0x80 | (cp & 0x3F)));
    } else {
        mo2_sb_addch(b, (char)(0xF0 | (cp >> 18)));
        mo2_sb_addch(b, (char)(0x80 | ((cp >> 12) & 0x3F)));
        mo2_sb_addch(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        mo2_sb_addch(b, (char)(0x80 | (cp & 0x3F)));
    }
}
static void mo2_emit_replacement(mo2_sb *b) { mo2_utf8_emit(b, 0xFFFD); }

/* decode a run of %XX bytes as UTF-8 with errors='replace' */
static void mo2_decode_bytes(mo2_sb *b, const unsigned char *buf, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = buf[i];
        if (c < 0x80) { mo2_sb_addch(b, (char)c); i++; continue; }
        int need; unsigned int cp; unsigned int lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) {
            need = 2; cp = c & 0x0F;
            if (c == 0xE0) lo = 0xA0;
            if (c == 0xED) hi = 0x9F;
        }
        else if (c >= 0xF0 && c <= 0xF4) {
            need = 3; cp = c & 0x07;
            if (c == 0xF0) lo = 0x90;
            if (c == 0xF4) hi = 0x8F;
        }
        else { mo2_emit_replacement(b); i++; continue; }
        size_t j = i + 1;
        bool ok = true;
        for (int t = 0; t < need; t++) {
            if (j >= n) { ok = false; break; }
            unsigned char cc = buf[j];
            unsigned char l = (t == 0) ? (unsigned char)lo : 0x80;
            unsigned char h = (t == 0) ? (unsigned char)hi : 0xBF;
            if (cc < l || cc > h) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
            j++;
        }
        if (!ok) { mo2_emit_replacement(b); i++; continue; }
        mo2_utf8_emit(b, cp);
        i = j;
    }
}

static int mo2_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *mo2_unquote_plus(const char *s) {
    mo2_sb b; mo2_sb_init(&b);
    size_t n = strlen(s);
    unsigned char *run = malloc(n + 1);
    size_t rn = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '+') {
            if (rn) { mo2_decode_bytes(&b, run, rn); rn = 0; }
            mo2_sb_addch(&b, ' ');
        } else if (c == '%' && i + 2 < n
                   && mo2_hexval(s[i + 1]) >= 0 && mo2_hexval(s[i + 2]) >= 0) {
            run[rn++] = (unsigned char)(mo2_hexval(s[i + 1]) * 16 + mo2_hexval(s[i + 2]));
            i += 2;
        } else {
            if (rn) { mo2_decode_bytes(&b, run, rn); rn = 0; }
            mo2_sb_addch(&b, c);
        }
    }
    if (rn) mo2_decode_bytes(&b, run, rn);
    free(run);
    return mo2_sb_finish(&b);
}

/* parse_qs(query) with keep_blank_values=False, separator='&' */
static json_t *mo2_parse_qs(const char *query) {
    json_t *out = json_object();
    const char *q = query ? query : "";
    const char *p = q;
    while (*p) {
        const char *amp = strchr(p, '&');
        size_t len = amp ? (size_t)(amp - p) : strlen(p);
        if (len > 0) {
            char *piece = malloc(len + 1);
            if (piece) {
                memcpy(piece, p, len); piece[len] = '\0';
                char *eq = strchr(piece, '=');
                if (eq) {
                    *eq = '\0';
                    const char *rawval = eq + 1;
                    if (rawval[0] != '\0') {  /* blank values dropped */
                        char *name = mo2_unquote_plus(piece);
                        char *val = mo2_unquote_plus(rawval);
                        json_t *arr = mo2_get(out, name);
                        if (!arr || arr->type != JSON_ARRAY) {
                            arr = json_array();
                            json_set(out, name, arr);
                        }
                        json_append(arr, json_string(val));
                        free(name); free(val);
                    }
                }
                free(piece);
            }
        }
        if (!amp) break;
        p = amp + 1;
    }
    return out;
}

/* ── html.escape (quote=True) ──────────────────────────────────────────── */
static char *mo2_html_escape(const char *s) {
    mo2_sb b; mo2_sb_init(&b);
    for (const char *p = s ? s : ""; *p; p++) {
        switch (*p) {
            case '&': mo2_sb_add(&b, "&amp;"); break;
            case '<': mo2_sb_add(&b, "&lt;"); break;
            case '>': mo2_sb_add(&b, "&gt;"); break;
            case '"': mo2_sb_add(&b, "&quot;"); break;
            case '\'': mo2_sb_add(&b, "&#x27;"); break;
            default: mo2_sb_addch(&b, *p); break;
        }
    }
    return mo2_sb_finish(&b);
}

/* ── float() coercion (ASCII string subset) ────────────────────────────── */
static bool mo2_ascii_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* digits with underscores ONLY between digits; returns end pointer or NULL */
static const char *mo2_float_digits(const char *p, bool *any) {
    *any = false;
    bool prev_digit = false;
    while (*p) {
        if (*p >= '0' && *p <= '9') { *any = true; prev_digit = true; p++; }
        else if (*p == '_') {
            if (!prev_digit) return NULL;
            if (!(p[1] >= '0' && p[1] <= '9')) return NULL;
            prev_digit = false;
            p++;
        }
        else break;
    }
    return p;
}

/* grammar-validate the unsigned numeric body (no sign, no whitespace) */
static bool mo2_float_grammar(const char *s) {
    const char *p = s;
    bool int_any = false, frac_any = false;
    p = mo2_float_digits(p, &int_any);
    if (!p) return false;
    if (*p == '.') {
        p++;
        p = mo2_float_digits(p, &frac_any);
        if (!p) return false;
    }
    if (!int_any && !frac_any) return false;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        bool exp_any = false;
        p = mo2_float_digits(p, &exp_any);
        if (!p || !exp_any) return false;
    }
    return *p == '\0';
}

/* float(v): ok=false carries a TypeError/ValueError spec in *err. */
static bool mo2_float_value(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = mo2_raise("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
        return false;
    }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return true; }
    if (v->type != JSON_STRING) {
        char buf[160];
        snprintf(buf, sizeof buf,
                 "float() argument must be a string or a real number, not '%s'",
                 mo2_type_name(v));
        *err = mo2_raise("TypeError", buf);
        return false;
    }
    const char *s = v->str_val ? v->str_val : "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && mo2_ascii_ws(s[a])) a++;
    while (b > a && mo2_ascii_ws(s[b - 1])) b--;
    char *body = malloc(b - a + 1);
    if (!body) { *err = mo2_raise("MemoryError", "oom"); return false; }
    memcpy(body, s + a, b - a); body[b - a] = '\0';
    char *p = body;
    double sign = 1.0;
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1.0; p++; }
    /* inf / infinity / nan (case-insensitive) */
    char low[16];
    size_t rl = strlen(p);
    if (rl < sizeof low) {
        for (size_t i = 0; i <= rl; i++) low[i] = (char)tolower((unsigned char)p[i]);
        if (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0) {
            *out = sign * INFINITY; free(body); return true;
        }
        if (strcmp(low, "nan") == 0) { *out = NAN; free(body); return true; }
    }
    bool ok = mo2_float_grammar(p);
    if (ok) {
        /* strip underscores for strtod */
        char *clean = malloc(strlen(p) + 1);
        if (!clean) { free(body); *err = mo2_raise("MemoryError", "oom"); return false; }
        size_t ck = 0;
        for (const char *t = p; *t; t++) if (*t != '_') clean[ck++] = *t;
        clean[ck] = '\0';
        *out = sign * strtod(clean, NULL);
        free(clean); free(body);
        return true;
    }
    free(body);
    char *rp = mo2_py_repr(v);
    mo2_sb msg; mo2_sb_init(&msg);
    mo2_sb_add(&msg, "could not convert string to float: ");
    mo2_sb_add(&msg, rp ? rp : "''");
    free(rp);
    char *m = mo2_sb_finish(&msg);
    *err = mo2_raise("ValueError", m);
    free(m);
    return false;
}

/* ── %-format subset (%s %r %d %%) ─────────────────────────────────────── */
static char *mo2_percent_format(const char *fmt, const json_t *args, json_t **err) {
    *err = NULL;
    mo2_sb b; mo2_sb_init(&b);
    size_t nargs = (args && args->type == JSON_ARRAY) ? args->c.count : 0;
    size_t used = 0;
    const char *p = fmt ? fmt : "";
    while (*p) {
        if (*p != '%') { mo2_sb_addch(&b, *p); p++; continue; }
        p++;
        if (*p == '%') { mo2_sb_addch(&b, '%'); p++; continue; }
        if (*p == '\0') {
            /* live: ValueError "incomplete format" */
            *err = mo2_raise("ValueError", "incomplete format");
            free(mo2_sb_finish(&b));
            return NULL;
        }
        char spec = *p;
        if (spec != 's' && spec != 'r' && spec != 'd') {
            char msg[128];
            snprintf(msg, sizeof msg,
                     "unsupported format character '%c' (0x%x) at index %zu",
                     spec, (unsigned char)spec, (size_t)(p - fmt));
            *err = mo2_raise("ValueError", msg);
            free(mo2_sb_finish(&b));
            return NULL;
        }
        if (used >= nargs) {
            *err = mo2_raise("TypeError", "not enough arguments for format string");
            free(mo2_sb_finish(&b));
            return NULL;
        }
        const json_t *arg = json_get(args, used);
        used++;
        char *piece = NULL;
        if (spec == 's') piece = mo2_pystr(arg);
        else if (spec == 'r') piece = mo2_py_repr(arg);
        else {  /* %d: a real number required */
            if (!arg || (arg->type != JSON_NUMBER && arg->type != JSON_BOOL)) {
                char msg[160];
                snprintf(msg, sizeof msg, "%%d format: a real number is required, not %s",
                         mo2_type_name(arg));
                *err = mo2_raise("TypeError", msg);
                free(mo2_sb_finish(&b));
                return NULL;
            }
            double d = arg->type == JSON_BOOL ? (arg->bool_val ? 1.0 : 0.0) : arg->num_val;
            char buf[64];
            snprintf(buf, sizeof buf, "%lld", (long long)d);
            piece = strdup(buf);
        }
        mo2_sb_add(&b, piece ? piece : "");
        free(piece);
        p++;
    }
    if (used < nargs) {
        *err = mo2_raise("TypeError", "not all arguments converted during string formatting");
        free(mo2_sb_finish(&b));
        return NULL;
    }
    return mo2_sb_finish(&b);
}

/* ── seam helpers ──────────────────────────────────────────────────────── */
/* ADOPTS v1 (json_set semantics); pass json_copy for borrowed nodes. */
static json_t *mo2_args1(const char *k1, json_t *v1) {
    json_t *a = json_object();
    json_set(a, k1, v1 ? v1 : json_null());
    return a;
}

/* fire a seam; on raise return it (caller propagates), else free and NULL */
static json_t *mo2_seam_prop(mo2_io_t *io, const char *name, json_t *args) {
    json_t *r = mo2_seam(io, name, args);
    if (mo2_raised(r)) return r;
    json_free(r);
    return NULL;
}

static json_t *mo2_print(mo2_io_t *io, const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text));
    json_set(a, "file", json_string("stderr"));
    return mo2_seam_prop(io, "print", a);
}

/* ══ PoP: mo2_stdin_is_console @ tools/mcp_oauth.py:_stdin_is_console ═════ */
static json_t *mo2_stdin_is_console(mo2_io_t *io, const json_t *in) {
    /* NB: 'stdin' is a stdio macro on the Windows gate host — name it stdin_v */
    const json_t *stdin_v = mo2_get(in, "stdin");
    /* sys.stdin None: the isatty attribute access raises AttributeError,
     * caught by the first except (AttributeError, ValueError) -> False */
    if (!mo2_not_none(stdin_v))
        return json_bool(false);
    /* try: if not sys.stdin.isatty(): return False
     * except (AttributeError, ValueError): return False */
    json_t *r = mo2_seam(io, "sys.stdin.isatty", NULL);
    if (mo2_raised(r)) {
        if (mo2_exc_is(r, "AttributeError") || mo2_exc_is(r, "ValueError")) {
            json_free(r);
            return json_bool(false);
        }
        return r;
    }
    bool atty = mo2_truthy(r);
    json_free(r);
    if (!atty) return json_bool(false);
    const char *platform = mo2_get_str(in, "platform", "posix");
    if (strcmp(platform, "nt") != 0) return json_bool(true);
    /* import ctypes / import msvcrt: a None sys.modules entry halts the import
     * (ModuleNotFoundError, caught by except Exception -> False, no seam). */
    if (json_get_bool(in, "ctypes_missing", false)) return json_bool(false);
    if (json_get_bool(in, "msvcrt_missing", false)) return json_bool(false);
    /* handle = msvcrt.get_osfhandle(sys.stdin.fileno()) */
    json_t *fd = mo2_seam(io, "sys.stdin.fileno", NULL);
    if (mo2_raised(fd)) {
        if (mo2_is_base_exc(fd)) return fd;
        json_free(fd);
        return json_bool(false);
    }
    json_t *hargs = mo2_args1("fileno", fd);
    json_t *handle = mo2_seam(io, "msvcrt.get_osfhandle", hargs);
    if (mo2_raised(handle)) {
        if (mo2_is_base_exc(handle)) return handle;
        json_free(handle);
        return json_bool(false);
    }
    /* mode = ctypes.c_ulong() */
    json_t *mode = mo2_seam(io, "ctypes.c_ulong", NULL);
    if (mo2_raised(mode)) {
        if (mo2_is_base_exc(mode)) return mode;
        json_free(mode);
        return json_bool(false);
    }
    /* GetConsoleMode(ctypes.c_void_p(handle), ctypes.byref(mode)) */
    json_t *vp = mo2_seam(io, "ctypes.c_void_p", mo2_args1("handle", handle));
    if (mo2_raised(vp)) {
        json_free(mode);   /* not yet adopted into any args object */
        if (mo2_is_base_exc(vp)) return vp;
        json_free(vp);
        return json_bool(false);
    }
    json_t *br = mo2_seam(io, "ctypes.byref", mo2_args1("mode", mode));
    json_free(vp);
    if (mo2_raised(br)) {
        if (mo2_is_base_exc(br)) return br;
        json_free(br);
        return json_bool(false);
    }
    json_free(br);
    json_t *gc = mo2_seam(io, "ctypes.windll.kernel32.GetConsoleMode", NULL);
    if (mo2_raised(gc)) {
        if (mo2_is_base_exc(gc)) return gc;
        json_free(gc);
        return json_bool(false);
    }
    bool console = mo2_truthy(gc);
    json_free(gc);
    return json_bool(console);
}

/* ══ PoP: mo2_model_json @ tools/mcp_oauth.py:_model_json ═════════════════ */
static json_t *mo2_model_json(mo2_io_t *io, const json_t *model) {
    /* model.model_dump(...) — the receiver attribute resolves first */
    if (!model || model->type != JSON_STRING)
        return mo2_attr_raise(mo2_type_name(model), "model_dump");
    json_t *a = json_object();
    json_set(a, "model", json_copy(model));
    json_set(a, "mode", json_string("json"));
    json_set(a, "exclude_none", json_bool(true));
    return mo2_seam(io, "model_dump", a);
}

/* ══ PoP: mo2_path @ tools/mcp_oauth.py:_path ═════════════════════════════ */
static json_t *mo2_path(mo2_io_t *io, const char *suffix) {
    /* _get_token_dir(self._hermes_home) / f"{self._server_name}{suffix}" */
    if (!json_has(io->state, "_hermes_home"))
        return mo2_attr_raise("HermesTokenStorage", "_hermes_home");
    json_t *home = mo2_state_get(io, "_hermes_home");
    json_t *dir = mo2_seam(io, "get_token_dir", mo2_args1("hermes_home", json_copy(home)));
    if (mo2_raised(dir)) return dir;
    if (!mo2_not_none(dir)) {
        json_free(dir);
        return mo2_raise("TypeError", "unsupported operand type(s) for /: 'NoneType' and 'str'");
    }
    if (dir->type != JSON_STRING) {
        char buf[160];
        snprintf(buf, sizeof buf, "unsupported operand type(s) for /: '%s' and 'str'",
                 mo2_type_name(dir));
        json_t *e = mo2_raise("TypeError", buf);
        json_free(dir);
        return e;
    }
    if (!json_has(io->state, "_server_name")) {
        json_free(dir);
        return mo2_attr_raise("HermesTokenStorage", "_server_name");
    }
    char *sn = mo2_pystr(mo2_state_get(io, "_server_name"));
    mo2_sb name; mo2_sb_init(&name);
    mo2_sb_add(&name, sn ? sn : "");
    mo2_sb_add(&name, suffix ? suffix : "");
    free(sn);
    char *nm = mo2_sb_finish(&name);
    char *joined = mo2_path_join(dir->str_val, nm);
    free(nm);
    json_free(dir);
    json_t *ret = json_string(joined ? joined : "");
    free(joined);
    return ret;
}

/* ══ PoP: mo2_state_paths @ tools/mcp_oauth.py:_state_paths ═══════════════ */
static json_t *mo2_state_paths(mo2_io_t *io) {
    static const char *seams[] = {
        "storage._tokens_path", "storage._client_info_path", "storage._meta_path", NULL,
    };
    json_t *arr = json_array();
    for (size_t i = 0; seams[i]; i++) {
        json_t *r = mo2_seam(io, seams[i], NULL);
        if (mo2_raised(r)) { json_free(arr); return r; }
        json_append(arr, json_copy(r));
        json_free(r);
    }
    return arr;
}

/* ══ PoP: mo2_bind_issuer @ tools/mcp_oauth.py:bind_issuer ════════════════ */
static json_t *mo2_bind_issuer(mo2_io_t *io, const json_t *issuer) {
    json_t *val;
    if (mo2_truthy(issuer)) {
        char *s = mo2_pystr(issuer);
        val = json_string(s ? s : "");
        free(s);
    } else {
        val = json_null();
    }
    mo2_state_set(io, "_bound_issuer", val);
    return json_null();
}

/* ══ PoP: mo2_credential_stamp @ tools/mcp_oauth.py:credential_stamp ══════ */
static json_t *mo2_credential_stamp(mo2_io_t *io) {
    static const char *fields[] = { "st_mtime_ns", "st_size", "st_ino", NULL };
    /* live: for path in (self._tokens_path(), self._client_info_path()):
     * the tuple literal evaluates BOTH path seams before the loop body */
    json_t *paths[2];
    static const char *seams[] = { "storage._tokens_path", "storage._client_info_path" };
    for (size_t i = 0; i < 2; i++) {
        paths[i] = mo2_seam(io, seams[i], NULL);
        if (mo2_raised(paths[i])) {
            for (size_t j = 0; j < i; j++) json_free(paths[j]);
            return paths[i];
        }
    }
    json_t *stamp = json_array();
    for (size_t i = 0; i < 2; i++) {
        json_t *p = paths[i];
        if (!p || p->type != JSON_STRING) {
            json_t *e = mo2_attr_raise(mo2_type_name(p), "stat");
            json_free(p);
            for (size_t j = i + 1; j < 2; j++) json_free(paths[j]);
            json_free(stamp);
            return e;
        }
        json_t *st = mo2_seam(io, "path.stat", mo2_args1("path", p));
        if (mo2_raised(st)) {
            if (mo2_exc_is(st, "OSError")) {
                json_free(st);
                json_append(stamp, json_null());
                continue;
            }
            for (size_t j = i + 1; j < 2; j++) json_free(paths[j]);
            json_free(stamp);
            return st;
        }
        if (!st || st->type != JSON_OBJECT) {
            json_t *e = mo2_attr_raise(mo2_type_name(st), "st_mtime_ns");
            json_free(st);
            for (size_t j = i + 1; j < 2; j++) json_free(paths[j]);
            json_free(stamp);
            return e;
        }
        json_t *triple = json_array();
        for (size_t f = 0; fields[f]; f++) {
            json_t *fv = mo2_get(st, fields[f]);
            if (!fv) {
                char buf[200];
                snprintf(buf, sizeof buf,
                         "'types.SimpleNamespace' object has no attribute '%s'", fields[f]);
                json_t *e = mo2_raise("AttributeError", buf);
                json_free(triple); json_free(st); json_free(stamp);
                for (size_t j = i + 1; j < 2; j++) json_free(paths[j]);
                return e;
            }
            json_append(triple, json_copy(fv));
        }
        json_free(st);
        json_append(stamp, triple);
    }
    return stamp;
}

/* ══ PoP: mo2_parse_redirect_query @ tools/mcp_oauth.py:_parse_redirect_query ══ */
static json_t *mo2_parse_redirect_query(const char *query) {
    json_t *params = mo2_parse_qs(query);
    static const char *keys[] = { "code", "state", "error", "iss", NULL };
    json_t *out = json_object();
    for (size_t i = 0; keys[i]; i++) {
        json_t *arr = mo2_get(params, keys[i]);
        json_t *val = (arr && arr->type == JSON_ARRAY && arr->c.count > 0)
                      ? json_copy(json_get(arr, 0)) : json_null();
        json_set(out, keys[i], val);
    }
    json_free(params);
    return out;
}

/* ══ PoP: mo2_do_get @ tools/mcp_oauth.py:do_GET ══════════════════════════ */
static json_t *mo2_do_get(mo2_io_t *io, const char *path) {
    char *upath = NULL, *uquery = NULL;
    char errmsg[200];
    mo2_urlparse(path, &upath, &uquery, errmsg, sizeof errmsg);
    free(upath);
    if (errmsg[0]) {
        free(uquery);
        return mo2_raise("ValueError", errmsg);
    }
    json_t *parsed = mo2_parse_redirect_query(uquery);
    free(uquery);
    json_t *code = mo2_get(parsed, "code");
    json_t *error = mo2_get(parsed, "error");
    long status = 200;
    char *body = NULL;
    if (!mo2_truthy(code) && !mo2_truthy(error)) {
        status = 404;
        body = strdup("<h2>Not Found</h2>");
    } else {
        json_t *a = json_object();
        json_set(a, "auth_code", code ? json_copy(code) : json_null());
        json_set(a, "state", json_copy(mo2_get(parsed, "state")));
        json_set(a, "error", error ? json_copy(error) : json_null());
        json_set(a, "iss", json_copy(mo2_get(parsed, "iss")));
        json_t *cr = mo2_seam(io, "claim_result", a);
        if (mo2_raised(cr)) { json_free(parsed); return cr; }
        bool won = mo2_truthy(cr);
        json_free(cr);
        if (!won) {
            body = strdup("<h2>Authorization already received</h2>"
                          "<p>You can close this tab and return to Hermes.</p>");
        } else if (mo2_truthy(code)) {
            body = strdup("<h2>Authorization Successful</h2>"
                          "<p>You can close this tab and return to Hermes.</p>");
        } else {
            const char *errstr = mo2_truthy(error) ? error->str_val : "unknown";
            char *esc = mo2_html_escape(errstr);
            mo2_sb b; mo2_sb_init(&b);
            mo2_sb_add(&b, "<h2>Authorization Failed</h2><p>Error: ");
            mo2_sb_add(&b, esc ? esc : "");
            mo2_sb_add(&b, "</p>");
            free(esc);
            body = mo2_sb_finish(&b);
        }
    }
    json_free(parsed);
    json_t *e;
    /* mo2_args1 ADOPTS the status node; mo2_seam frees the args object.
     * Do NOT free it again here (was a double-free -> 0xC0000374). */
    e = mo2_seam_prop(io, "send_response",
                      mo2_args1("status", json_number((double)status)));
    if (e) { free(body); return e; }
    json_t *ha = json_object();
    json_set(ha, "name", json_string("Content-Type"));
    json_set(ha, "value", json_string("text/html; charset=utf-8"));
    e = mo2_seam_prop(io, "send_header", ha);
    if (e) { free(body); return e; }
    e = mo2_seam_prop(io, "end_headers", NULL);
    if (e) { free(body); return e; }
    mo2_sb wb; mo2_sb_init(&wb);
    mo2_sb_add(&wb, "<html><body>");
    mo2_sb_add(&wb, body ? body : "");
    mo2_sb_add(&wb, "</body></html>");
    free(body);
    char *write_text = mo2_sb_finish(&wb);
    e = mo2_seam_prop(io, "wfile.write", mo2_args1("data", json_string(write_text)));
    free(write_text);
    if (e) return e;
    return json_null();
}

/* ══ PoP: mo2_log_request @ tools/mcp_oauth.py:log_request ════════════════ */
static json_t *mo2_log_request(mo2_io_t *io, const char *path, const json_t *command) {
    /* logger.debug("OAuth callback: %s %s", self.command, urlparse(self.path).path) */
    if (!command)
        return mo2_attr_raise("_Handler", "command");
    char *upath = NULL, *uquery = NULL;
    char errmsg[200];
    mo2_urlparse(path, &upath, &uquery, errmsg, sizeof errmsg);
    free(uquery);
    if (errmsg[0]) {
        free(upath);
        return mo2_raise("ValueError", errmsg);
    }
    free(upath);
    return mo2_seam_prop(io, "logger.debug", NULL);
}

/* ══ PoP: mo2_log_message @ tools/mcp_oauth.py:log_message ════════════════ */
static json_t *mo2_log_message(mo2_io_t *io, const char *fmt, const json_t *args) {
    json_t *err = NULL;
    char *rendered = mo2_percent_format(fmt, args, &err);
    if (err) return err;
    free(rendered);
    return mo2_seam_prop(io, "logger.debug", NULL);
}

/* ══ PoP: mo2_announce_authorization_url @ tools/mcp_oauth.py:_announce_authorization_url ══ */
static json_t *mo2_announce_authorization_url(mo2_io_t *io, const char *authorization_url,
                                              const json_t *port, const json_t *redirect_uri,
                                              const json_t *redirect_host) {
    mo2_sb t1; mo2_sb_init(&t1);
    mo2_sb_add(&t1, "\n  MCP OAuth: authorization required.\n  Open this URL in your browser:\n\n    ");
    mo2_sb_add(&t1, authorization_url ? authorization_url : "");
    mo2_sb_add(&t1, "\n");
    char *text1 = mo2_sb_finish(&t1);
    json_t *e = mo2_print(io, text1);
    free(text1);
    if (e) return e;
    /* os.getenv("SSH_CLIENT") or os.getenv("SSH_TTY") — short-circuit */
    json_t *g1 = mo2_seam(io, "os.getenv", mo2_args1("key", json_string("SSH_CLIENT")));
    if (mo2_raised(g1)) return g1;
    bool ssh = mo2_truthy(g1);
    json_free(g1);
    if (!ssh) {
        json_t *g2 = mo2_seam(io, "os.getenv", mo2_args1("key", json_string("SSH_TTY")));
        if (mo2_raised(g2)) return g2;
        ssh = mo2_truthy(g2);
        json_free(g2);
    }
    if (ssh) {
        if (mo2_truthy(redirect_uri)) {
            char *uri = mo2_pystr(redirect_uri);
            char *hint = mo2_replace_all(MO2_SSH_HINT_PROXY, "{redirect_uri}", uri ? uri : "");
            free(uri);
            e = mo2_print(io, hint);
            free(hint);
            if (e) return e;
        } else if (mo2_truthy(port)) {
            char *host;
            if (mo2_truthy(redirect_host)) host = mo2_pystr(redirect_host);
            else host = strdup("127.0.0.1");
            char *portstr = mo2_pystr(port);
            char *h1 = mo2_replace_all(MO2_SSH_HINT_LOOPBACK, "{host}", host ? host : "");
            char *hint = mo2_replace_all(h1, "{port}", portstr ? portstr : "");
            free(host); free(portstr); free(h1);
            e = mo2_print(io, hint);
            free(hint);
            if (e) return e;
        }
    }
    json_t *cob = mo2_seam(io, "can_open_browser", NULL);
    if (mo2_raised(cob)) return cob;
    bool can = mo2_truthy(cob);
    json_free(cob);
    const char *note;
    if (!can) {
        note = MO2_NOTE_HEADLESS;
    } else {
        bool opened = false;
        json_t *wo = mo2_seam(io, "webbrowser.open",
                              mo2_args1("url", json_string(authorization_url ? authorization_url : "")));
        if (mo2_raised(wo)) {
            /* contextlib.suppress(Exception): BaseException passes through */
            if (mo2_is_base_exc(wo)) return wo;
            json_free(wo);
        } else {
            opened = mo2_truthy(wo);
            json_free(wo);
        }
        note = opened ? MO2_NOTE_OPENED : MO2_NOTE_NOT_OPENED;
    }
    mo2_sb t2; mo2_sb_init(&t2);
    mo2_sb_add(&t2, "  (");
    mo2_sb_add(&t2, note);
    mo2_sb_add(&t2, ")\n");
    char *text2 = mo2_sb_finish(&t2);
    e = mo2_print(io, text2);
    free(text2);
    if (e) return e;
    return json_null();
}

/* ══ PoP: mo2_redirect_handler @ tools/mcp_oauth.py:_redirect_handler ═════ */
static json_t *mo2_redirect_handler(mo2_io_t *io, const char *authorization_url,
                                    const json_t *port, const json_t *redirect_uri,
                                    const json_t *redirect_host) {
    json_t *df = mo2_seam(io, "get_dashboard_oauth_flow", NULL);
    if (mo2_raised(df)) return df;
    if (mo2_not_none(df)) {
        json_free(df);
        json_t *r = mo2_seam(io, "dashboard_flow.publish_authorization_url",
                             mo2_args1("url", json_string(authorization_url ? authorization_url : "")));
        if (mo2_raised(r)) return r;
        json_free(r);
        return json_null();
    }
    json_free(df);
    json_t *e = mo2_seam_prop(io, "raise_if_non_interactive",
                              mo2_args1("lead", json_string(MO2_REDIRECT_LEAD)));
    if (e) return e;
    return mo2_announce_authorization_url(io, authorization_url, port, redirect_uri, redirect_host);
}

/* ══ PoP: mo2_settle @ tools/mcp_oauth.py:settle ══════════════════════════ */
static json_t *mo2_settle(mo2_io_t *io) {
    json_t *d = mo2_seam(io, "future.done", NULL);
    if (mo2_raised(d)) return d;
    bool done = mo2_truthy(d);
    json_free(d);
    if (!done) {
        json_t *e = mo2_seam_prop(io, "future.set_result", mo2_args1("value", json_null()));
        if (e) return e;
    }
    return json_null();
}

/* ══ PoP: mo2_close @ tools/mcp_oauth.py:close ════════════════════════════ */
static json_t *mo2_close(mo2_io_t *io) {
    json_t *pending = NULL;
    /* try: server.shutdown() finally: server.server_close() */
    json_t *r = mo2_seam(io, "server.shutdown", NULL);
    if (mo2_raised(r)) pending = r;
    else json_free(r);
    json_t *r2 = mo2_seam(io, "server.server_close", NULL);
    if (mo2_raised(r2)) {
        if (pending) json_free(pending);
        pending = r2;
    } else {
        json_free(r2);
    }
    /* except Exception: logger.debug("...", exc_info=True) */
    if (pending && mo2_exc_is(pending, "Exception")) {
        json_free(pending);
        pending = NULL;
        json_t *lg = mo2_seam(io, "logger.debug", mo2_args1("exc_info", json_bool(true)));
        if (mo2_raised(lg)) pending = lg;
        else json_free(lg);
    }
    /* finally: try: loop.call_soon_threadsafe(settle) except RuntimeError: pass
     * (settle runs inline in the deterministic model; its raise is subject to
     * the same RuntimeError catch — fixtures keep settle non-raising here) */
    json_t *cserr = NULL;
    json_t *cs = mo2_seam(io, "loop.call_soon_threadsafe", NULL);
    if (mo2_raised(cs)) {
        cserr = cs;
    } else {
        json_free(cs);
        json_t *st = mo2_settle(io);
        if (mo2_raised(st)) cserr = st;
        else json_free(st);
    }
    if (cserr) {
        if (mo2_exc_is(cserr, "RuntimeError")) {
            json_free(cserr);
        } else {
            if (pending) json_free(pending);
            pending = cserr;
        }
    }
    if (pending) return pending;
    return json_null();
}

/* ══ PoP: mo2_close_callback_server @ tools/mcp_oauth.py:_close_callback_server ══ */
static json_t *mo2_close_callback_server(mo2_io_t *io) {
    json_t *e;
    e = mo2_seam_prop(io, "asyncio.get_running_loop", NULL);
    if (e) return e;
    e = mo2_seam_prop(io, "loop.create_future", NULL);
    if (e) return e;
    json_t *ta = json_object();
    json_set(ta, "name", json_string("mcp-oauth-cb-close"));
    json_set(ta, "daemon", json_bool(true));
    e = mo2_seam_prop(io, "threading.Thread", ta);
    if (e) return e;
    e = mo2_seam_prop(io, "thread.start", NULL);
    if (e) return e;
    /* the close-thread runs synchronously in the deterministic model */
    json_t *cl = mo2_close(io);
    if (mo2_raised(cl)) return cl;
    json_free(cl);
    /* while not closed.done(): await asyncio.shield(closed); CancelledError
     * is remembered and the wait continues; re-raised after the future settles */
    json_t *cancelled = NULL;
    for (;;) {
        json_t *d = mo2_seam(io, "future.done", NULL);
        if (mo2_raised(d)) {
            if (cancelled) json_free(cancelled);
            return d;
        }
        bool done = mo2_truthy(d);
        json_free(d);
        if (done) break;
        json_t *sh = mo2_seam(io, "asyncio.shield", NULL);
        if (mo2_raised(sh)) {
            if (mo2_exc_is(sh, "CancelledError")) {
                if (cancelled) json_free(cancelled);
                cancelled = sh;
                continue;
            }
            if (cancelled) json_free(cancelled);
            return sh;
        }
        json_free(sh);
    }
    if (cancelled) return cancelled;
    return json_null();
}

/* ══ PoP: mo2_wait @ tools/mcp_oauth.py:_wait ═════════════════════════════ */
static json_t *mo2_wait(mo2_io_t *io, const json_t *port, const json_t *cimd_url,
                        double timeout) {
    json_t *df = mo2_seam(io, "get_dashboard_oauth_flow", NULL);
    if (mo2_raised(df)) return df;
    bool has_df = mo2_not_none(df);
    if (has_df && !mo2_truthy(port)) {
        json_free(df);
        json_t *wc = mo2_seam(io, "dashboard_flow.wait_for_callback", NULL);
        if (mo2_raised(wc)) return wc;
        if (!wc || wc->type != JSON_ARRAY) {
            char buf[160];
            snprintf(buf, sizeof buf,
                     "argument after * must be an iterable, not %s", mo2_type_name(wc));
            json_free(wc);
            return mo2_raise("TypeError", buf);
        }
        json_t *a = json_array();
        for (size_t i = 0; i < wc->c.count; i++)
            json_append(a, json_copy(json_get(wc, i)));
        json_free(wc);
        json_t *acr = mo2_seam(io, "authorization_code_result", a);
        return acr;
    }
    json_free(df);
    json_t *e = mo2_seam_prop(io, "raise_if_non_interactive",
                              mo2_args1("lead", json_string(MO2_WAIT_LEAD)));
    if (e) return e;
    json_t *mh = mo2_seam(io, "make_callback_handler", NULL);
    if (mo2_raised(mh)) return mh;
    if (!mh || mh->type != JSON_OBJECT) {
        char buf[160];
        if (!mo2_not_none(mh))
            snprintf(buf, sizeof buf, "cannot unpack non-iterable NoneType object");
        else
            snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object", mo2_type_name(mh));
        json_free(mh);
        return mo2_raise("TypeError", buf);
    }
    json_t *handler = mo2_get(mh, "handler");
    json_t *sa = json_object();
    json_set(sa, "port", port ? json_copy(port) : json_null());
    json_set(sa, "handler", handler ? json_copy(handler) : json_null());
    json_t *server = mo2_seam(io, "start_callback_server", sa);
    json_free(mh);
    if (mo2_raised(server)) return server;
    json_free(server);
    /* serve_forever thread: recorded, inert (never runs the listener loop) */
    json_t *ta = json_object();
    json_set(ta, "target", json_string("serve_forever"));
    json_set(ta, "poll_interval", json_number(0.25));
    json_set(ta, "daemon", json_bool(true));
    e = mo2_seam_prop(io, "threading.Thread", ta);
    if (e) return e;
    e = mo2_seam_prop(io, "thread.start", NULL);
    if (e) return e;
    json_t *ii = mo2_seam(io, "is_interactive", NULL);
    if (mo2_raised(ii)) return ii;
    bool inter = mo2_truthy(ii);
    json_free(ii);
    if (inter && !has_df) {
        json_t *pa = json_object();
        json_set(pa, "text", json_string(MO2_PASTE_PROMPT));
        json_set(pa, "file", json_string("stderr"));
        json_set(pa, "flush", json_bool(true));
        e = mo2_seam_prop(io, "print", pa);
        if (e) return e;
        json_t *ta2 = json_object();
        json_set(ta2, "target", json_string("paste_callback_reader"));
        json_set(ta2, "daemon", json_bool(true));
        e = mo2_seam_prop(io, "threading.Thread", ta2);
        if (e) return e;
        e = mo2_seam_prop(io, "thread.start", NULL);
        if (e) return e;
    }
    /* try: while elapsed < timeout and not _result_taken(result):
     *          await asyncio.sleep(0.5); elapsed += 0.5
     * finally: await _close_callback_server(server) */
    double elapsed = 0.0;
    json_t *looperr = NULL;
    for (;;) {
        if (!(elapsed < timeout)) break;
        json_t *rt = mo2_seam(io, "result_taken", NULL);
        if (mo2_raised(rt)) { looperr = rt; break; }
        bool taken = mo2_truthy(rt);
        json_free(rt);
        if (taken) break;
        json_t *sl = mo2_seam(io, "asyncio.sleep", mo2_args1("seconds", json_number(0.5)));
        if (mo2_raised(sl)) { looperr = sl; break; }
        json_free(sl);
        elapsed += 0.5;
    }
    json_t *cc = mo2_close_callback_server(io);
    if (mo2_raised(cc)) {
        if (looperr) json_free(looperr);
        looperr = cc;
    } else {
        json_free(cc);
    }
    if (looperr) return looperr;
    json_t *oa = json_object();
    json_set(oa, "cimd_url", cimd_url ? json_copy(cimd_url) : json_null());
    return mo2_seam(io, "callback_outcome", oa);
}

/* ══ PoP: mo2_seconds @ tools/mcp_oauth.py:_seconds ═══════════════════════ */
static json_t *mo2_seconds(const json_t *value, double default_value) {
    double out = 0.0;
    json_t *err = NULL;
    if (!mo2_float_value(value, &out, &err)) {
        /* except (TypeError, ValueError): return default */
        if (mo2_exc_is(err, "TypeError") || mo2_exc_is(err, "ValueError")) {
            json_free(err);
            return json_number(default_value);
        }
        return err;
    }
    /* max(0.0, x): nan compares false -> 0.0 */
    return json_number(out > 0.0 ? out : 0.0);
}