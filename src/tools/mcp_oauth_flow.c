/* mcp_oauth_flow.c — FLOW + FUNCTION port of tools/mcp_oauth.py (MCP OAuth 2.1).
 *
 * Ports the OAuth refresh-fence acquisition/release decision chains, lazy SDK
 * discovery, callback-port reservation/eviction, cached-registration redirect
 * selection, on-disk model loading (device-metadata branch, fixup, validation
 * error paths), token expiry rebase / issuer stamping / refresh-token stripping,
 * the callback result race (claim under lock), callback-server lifecycle
 * ordering (adopt-parked vs bind), authorization outcome branches, and the
 * interactive-login connect bound. All network/file/clock/lock/logging I/O is
 * INJECTED through the mof_io_t seam dispatcher; the decision logic around each
 * seam matches the live Python exactly.
 *
 * Seam model mirrors port_credential_pool_flow: every live I/O call goes through
 * mof_io_t.call(io, name, args); the dispatcher appends `name` to io->trace
 * (ordered seam-name sequence), returns the scripted result, and models a raised
 * exception with {"__raise__": "<Type>", "message": "...", ...}. Pure siblings
 * (_result_taken, _coerce_secret_auth_method, _refresh_lock_path, the URL parser)
 * are ported directly and perform no I/O. io->state is a json_t OBJECT mirroring
 * the live module globals / storage attributes (_SDK_CLASSES, _OAUTH_AVAILABLE,
 * _reserved_sockets, loaded_issuer, _server_name, ...).
 *
 * Seam name -> live call (args are informational; the trace records names only):
 *   mkdir_under_hermes_home  hermes_constants.mkdir_under_hermes_home(parent)
 *   secure_parent_dir        hermes_constants.secure_parent_dir(lock_path)
 *   os.open                  os.open(lock_path, O_RDWR|O_CREAT, 0o600) -> fd
 *   os.close                 os.close(fd)
 *   time.monotonic           time.monotonic() -> float
 *   time.time                time.time() -> float
 *   fcntl.flock              fcntl.flock(fd, op)            [platform posix]
 *   msvcrt.locking           msvcrt.locking(fd, mode, 1)    [platform windows]
 *   asyncio.sleep            await asyncio.sleep(0.05)
 *   import_client_auth       from mcp.client import auth   -> {class: marker}
 *   import_shared_auth       from mcp.shared import auth   -> {class: marker}
 *   import_device_metadata   from tools.mcp_oauth_device import DeviceOAuthMetadata
 *   logger.debug/warning/info  logger.<level>(fmt, ...)
 *   socket.socket            socket.socket(AF_INET, SOCK_STREAM) -> sock id
 *   sock.bind                sock.bind(("127.0.0.1", port))
 *   sock.close               sock.close()
 *   sock.getsockname         sock.getsockname() -> [host, port]
 *   storage._client_info_path  storage._client_info_path() -> path str
 *   storage._tokens_path       storage._tokens_path() -> path str
 *   _read_json               _read_json(path) -> parsed JSON | None
 *   _write_json              _write_json(path, data)
 *   path.stat                path.stat() -> {"st_mtime": float}
 *   _sdk_class               _sdk_class(sdk_name) -> class marker | None
 *   fixup                    fixup(data)  (load_model callback)
 *   model_validate           cls.model_validate(data) -> model dict | raise
 *   lock.__enter__/__exit__  with lock:  (threading.Lock double)
 *   CallbackServer           _CallbackServer(addr, handler, bind_and_activate=False)
 *   server.socket.close      server.socket.close()
 *   reserved.getsockname     reserved.getsockname() -> [host, port]
 *   server.server_bind       server.server_bind()
 *   server.server_activate   server.server_activate()
 *   authorization_code_result  _authorization_code_result(code, state, iss)
 *
 * Scripted exception kinds carry the live class name; BaseException-only kinds
 * (SystemExit, KeyboardInterrupt, GeneratorExit, asyncio.CancelledError) PASS
 * THROUGH every `except Exception`/OSError/... site, matching live propagation.
 * errno values are pinned to the Linux constants (EWOULDBLOCK=EAGAIN=11,
 * EACCES=13, EDEADLK=35); the oracle seeds an errno double with the same values
 * so _FENCE_CONTENTION_ERRNOS is host-independent.
 *
 * Deterministic subset: paths are POSIX-style single-slash strings (no drive
 * letters / UNC / leading '//'); URL fixtures are ASCII (the C url parser models
 * the ASCII subset of urllib.parse, verified against the live parser); container
 * str() reprs are excluded from redirect fixtures. Integral doubles serialize
 * int-like; the oracle normalizes integral floats to int on both sides.
 *
 * Ownership: seam results returned by call are owned by the caller (json_free).
 * io->trace / io->state are owned by the harness. seam() frees its args.
 * json_set ADOPTS its value and frees the replaced node — borrowed pointers to a
 * replaced key are copied first.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to tools/mcp_oauth.py) ─────────── */
#define MO_REFRESH_FENCE_TIMEOUT_SECONDS 60.0
#define MO_MAX_RESERVED_SOCKETS 8
#define MO_USER_SKIPPED_SENTINEL "__hermes_user_skipped__"
static const int MO_CIMD_PORTS[] = {27890, 27891, 27892, 27893, 27894};
static const char *MO_SDK_CLASS_NAMES[] = {
    "OAuthClientProvider", "OAuthClientInformationFull", "OAuthClientMetadata",
    "OAuthMetadata", "OAuthToken", NULL,
};
/* _FENCE_CONTENTION_ERRNOS = {EWOULDBLOCK, EAGAIN, EACCES, EDEADLK} (Linux pins) */
static bool mo_fence_contention_errno(long e) {
    return e == 11 || e == 13 || e == 35;
}

/* ── generic seam dispatcher + trace + attribute state ─────────────────── */
typedef struct mof_io_t {
    json_t *(*call)(struct mof_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} mof_io_t;

static void mof_trace(mof_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *mof_call(mof_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool mof_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

static json_t *seam(mof_io_t *io, const char *name, json_t *args) {
    mof_trace(io, name);
    json_t *r = mof_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}

static json_t *mof_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

static const char *mof_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = mof_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}

static bool mof_truthy(const json_t *v) {
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

/* `x is not None` — absent (NULL) and JSON_NULL both model None. */
static bool mof_not_none(const json_t *v) {
    return v && v->type != JSON_NULL;
}

static json_t *mof_state_get(mof_io_t *io, const char *key) { return mof_get(io->state, key); }
static void mof_state_set(mof_io_t *io, const char *key, json_t *val) {
    if (io && io->state && io->state->type == JSON_OBJECT) json_set(io->state, key, val);
    else if (val) json_free(val);
}

static void mof_log(mof_io_t *io, const char *level) {
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = seam(io, name, NULL);
    json_free(r);
}

/* ── exception model ───────────────────────────────────────────────────── */
static json_t *mof_raise(const char *kind, const char *message) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(message ? message : ""));
    return o;
}

static const char *mof_exc_kind(const json_t *r) {
    return mof_get_str(r, "__raise__", "");
}

/* BaseException-only kinds pass through every `except Exception` site. */
static bool mof_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0
        || strcmp(kind, "BaseException") == 0;
}

static bool mo_in_list(const char *kind, const char *const *list) {
    for (size_t i = 0; list[i]; i++)
        if (strcmp(kind, list[i]) == 0) return true;
    return false;
}

/* Small live class hierarchy for `except <Base>` membership. */
static bool mof_exc_is(const json_t *r, const char *base) {
    if (!mof_raised(r)) return false;
    const char *k = mof_exc_kind(r);
    static const char *oserror[] = {"OSError", "FileNotFoundError", "PermissionError",
        "FileExistsError", "IsADirectoryError", "NotADirectoryError", "InterruptedError",
        "BlockingIOError", "ConnectionError", "ConnectionAbortedError",
        "ConnectionRefusedError", "ConnectionResetError", "BrokenPipeError",
        "TimeoutError", NULL};
    static const char *valueerror[] = {"ValueError", "JSONDecodeError",
        "UnicodeDecodeError", "UnicodeEncodeError", NULL};
    static const char *importerror[] = {"ImportError", "ModuleNotFoundError", NULL};
    static const char *runtimeerror[] = {"RuntimeError", "RefreshFenceTimeout",
        "OAuthNonInteractiveError", "NotImplementedError", NULL};
    if (strcmp(base, "BaseException") == 0) return true;
    if (strcmp(base, "Exception") == 0) return !mof_is_base_exc(k);
    if (strcmp(base, "OSError") == 0) return mo_in_list(k, oserror);
    if (strcmp(base, "ValueError") == 0) return mo_in_list(k, valueerror);
    if (strcmp(base, "ImportError") == 0) return mo_in_list(k, importerror);
    if (strcmp(base, "RuntimeError") == 0) return mo_in_list(k, runtimeerror);
    return strcmp(k, base) == 0;  /* TypeError, KeyError, AttributeError, ... */
}

/* str(exc) for a scripted raise: KeyError str is repr(key); the dispatcher
 * pre-reprs the key into "message", so every kind uses message verbatim. */
static const char *mof_exc_msg(const json_t *r) { return mof_get_str(r, "message", ""); }

/* ── Python str() / repr() for scalar JSON values ──────────────────────── */
static char *mof_pystr(const json_t *v) {
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
    char *ser = json_serialize(v);  /* containers excluded from fixtures */
    return ser ? ser : strdup("");
}

/* repr() of a scalar (KeyError str). ASCII subset: single quotes, escape
 * backslash / quote / \n \r \t; other bytes kept verbatim. */
static char *mof_py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return mof_pystr(v);
    if (v->type != JSON_STRING) return mof_pystr(v);
    const char *s = v->str_val ? v->str_val : "";
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == '\'') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else out[k++] = (char)c;
    }
    out[k++] = '\'';
    out[k] = '\0';
    return out;
}

/* ── POSIX path normalization (Path() subset) + with_suffix append rule ── */
/* Path(p) collapses repeated slashes, drops '.' components and trailing
 * slashes, keeps '..'. For every named path, with_suffix(suffix + ".refresh.lock")
 * == normalized + ".refresh.lock" (verified against live pathlib). Empty-name
 * paths ('/', '.', '') raise ValueError in live; fixtures avoid them. */
static char *mof_path_normalize(const char *p) {
    const char *s = p ? p : "";
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    if (!out) return NULL;
    size_t k = 0;
    bool rooted = (n > 0 && s[0] == '/');
    if (rooted) out[k++] = '/';
    size_t i = 0;
    bool first = true;
    while (i < n) {
        while (i < n && s[i] == '/') i++;
        size_t start = i;
        while (i < n && s[i] != '/') i++;
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

/* ── urllib.parse subset: scheme / netloc / path / hostname / port ─────── */
typedef struct {
    char *scheme;
    char *netloc;
    char *path;
    char *hostname;   /* NULL == None (empty host) */
    long  port;
    bool  has_port;
    bool  err;        /* ValueError raised (parse-eager or lazy host/port) */
    char  errmsg[160];
} mo_url_t;

static void mo_url_free(mo_url_t *u) {
    free(u->scheme); free(u->netloc); free(u->path); free(u->hostname);
    u->scheme = u->netloc = u->path = u->hostname = NULL;
}

static char *mo_lower_ascii(const char *s) {
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}

/* IPv4 dotted-quad (ipaddress: no leading zeros, 0-255). */
static bool mo_ipv4_valid(const char *s, size_t len) {
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

/* Count ':'-separated hex groups (IPv4 tail counts as 2). tail_allowed permits
 * an IPv4 dotted-quad as the final segment. Returns -1 on malformed input. */
static int mo_seg_count(const char *s, bool tail_allowed, bool *bad) {
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
            if (!tail_allowed || !is_last || !mo_ipv4_valid(start, len)) { *bad = true; return -1; }
            count += 2;
        } else {
            if (dots != 0 || len > 4) { *bad = true; return -1; }
            for (size_t i = 0; i < len; i++)
                if (!isxdigit((unsigned char)start[i])) { *bad = true; return -1; }
            count += 1;
        }
        if (*p == ':') {
            p++;
            if (*p == '\0') { *bad = true; return -1; }  /* trailing single ':' */
        }
    }
    return count;
}

/* IPv6 validation: hex groups with at most one '::' compression, optional IPv4
 * tail, total 8 groups (compression fills >= 1). */
static bool mo_ipv6_groups(const char *s) {
    const char *dc = strstr(s, "::");
    if (dc && strstr(dc + 2, "::")) return false;
    bool bad = false;
    if (dc) {
        size_t llen = (size_t)(dc - s);
        char *left = malloc(llen + 1);
        if (!left) return false;
        memcpy(left, s, llen); left[llen] = '\0';
        int lc = mo_seg_count(left, false, &bad);
        free(left);
        if (bad) return false;
        int rc = mo_seg_count(dc + 2, true, &bad);
        if (bad) return false;
        return lc + rc <= 7;
    }
    int c = mo_seg_count(s, true, &bad);
    if (bad) return false;
    return c == 8;
}

/* 0 = valid bracketed host; 1 = not an IPv6/IPvFuture address; 2 = IPv4 in
 * brackets (live: "An IPv4 address cannot be in brackets"). */
static int mo_bracketed_host_check(const char *h) {
    if (mo_ipv4_valid(h, strlen(h))) return 2;
    /* IPvFuture: v<hex>.<unreserved/sub-delims/':'>+ */
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
    /* scope id (%...) is allowed by ipaddress for IPv6 */
    char *base = strdup(h);
    if (!base) return 1;
    char *pct = strchr(base, '%');
    if (pct) *pct = '\0';
    bool ok = mo_ipv6_groups(base);
    free(base);
    return ok ? 0 : 1;
}

/* Parse the ASCII subset of urllib.parse.urlparse. Errors mirror live:
 * unclosed/stray brackets -> "Invalid IPv6 URL" (eager); bad bracketed host ->
 * "'<h>' does not appear to be an IPv4 or IPv6 address" (eager); port cast /
 * range errors are lazy (hostname/port access) but both surface as ValueError
 * inside _cached_redirect's try. */
static void mo_urlparse(const char *url, mo_url_t *u) {
    memset(u, 0, sizeof *u);
    u->port = -1;
    /* urlsplit strips ASCII \n \r \t before parsing */
    size_t n = strlen(url);
    char *clean = malloc(n + 1);
    if (!clean) { u->err = true; snprintf(u->errmsg, sizeof u->errmsg, "oom"); return; }
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char c = url[i];
        if (c == '\n' || c == '\r' || c == '\t') continue;
        clean[k++] = c;
    }
    clean[k] = '\0';
    const char *rest = clean;
    /* scheme: [A-Za-z][A-Za-z0-9+.-]* ':' */
    size_t si = 0;
    if (k > 0 && isalpha((unsigned char)clean[0])) {
        si = 1;
        while (si < k && (isalnum((unsigned char)clean[si]) || clean[si] == '+'
               || clean[si] == '.' || clean[si] == '-')) si++;
        if (si < k && clean[si] == ':') {
            char *sch = malloc(si + 1);
            memcpy(sch, clean, si); sch[si] = '\0';
            u->scheme = mo_lower_ascii(sch);
            free(sch);
            rest = clean + si + 1;
        } else {
            u->scheme = strdup("");
        }
    } else {
        u->scheme = strdup("");
    }
    /* netloc */
    const char *pathstart = rest;
    if (rest[0] == '/' && rest[1] == '/') {
        const char *nl = rest + 2;
        const char *end = nl;
        while (*end && *end != '/' && *end != '?' && *end != '#') end++;
        size_t nlen = (size_t)(end - nl);
        u->netloc = malloc(nlen + 1);
        memcpy(u->netloc, nl, nlen); u->netloc[nlen] = '\0';
        pathstart = end;
    } else {
        u->netloc = strdup("");
    }
    /* path up to '?' / '#' */
    const char *pe = pathstart;
    while (*pe && *pe != '?' && *pe != '#') pe++;
    size_t plen = (size_t)(pe - pathstart);
    u->path = malloc(plen + 1);
    memcpy(u->path, pathstart, plen); u->path[plen] = '\0';
    /* hostname / port from netloc */
    const char *nl = u->netloc;
    if (!*nl) { free(clean); return; }  /* hostname None, port None */
    /* stray brackets anywhere -> Invalid IPv6 URL (eager) */
    const char *lb = strchr(nl, '[');
    const char *rb = strchr(nl, ']');
    const char *hostinfo = nl;
    const char *at = strrchr(nl, '@');
    if (at) hostinfo = at + 1;
    if (hostinfo[0] == '[') {
        const char *close = strchr(hostinfo, ']');
        if (!close) { u->err = true; snprintf(u->errmsg, sizeof u->errmsg, "Invalid IPv6 URL"); free(clean); return; }
        size_t hlen = (size_t)(close - hostinfo - 1);
        char *h = malloc(hlen + 1);
        memcpy(h, hostinfo + 1, hlen); h[hlen] = '\0';
        int rc = mo_bracketed_host_check(h);
        if (rc != 0) {
            u->err = true;
            if (rc == 2) {
                snprintf(u->errmsg, sizeof u->errmsg, "An IPv4 address cannot be in brackets");
            } else {
                /* repr of the host string */
                json_t *hs = json_string(h);
                char *rr = mof_py_repr(hs);
                json_free(hs);
                snprintf(u->errmsg, sizeof u->errmsg, "%s does not appear to be an IPv4 or IPv6 address", rr ? rr : "''");
                free(rr);
            }
            free(h); free(clean); return;
        }
        u->hostname = mo_lower_ascii(h);
        free(h);
        const char *after = close + 1;
        if (*after == '\0') { free(clean); return; }
        if (*after != ':') { u->err = true; snprintf(u->errmsg, sizeof u->errmsg, "Invalid IPv6 URL"); free(clean); return; }
        const char *portstr = after + 1;
        if (*portstr == '\0') { free(clean); return; }  /* empty port -> None */
        /* port cast: ASCII digits only */
        bool digits = true;
        for (const char *q = portstr; *q; q++)
            if (*q < '0' || *q > '9') { digits = false; break; }
        if (!digits) {
            u->err = true;
            snprintf(u->errmsg, sizeof u->errmsg, "Port could not be cast to integer value as '%s'", portstr);
            free(clean); return;
        }
        long pv = strtol(portstr, NULL, 10);
        if (pv < 0 || pv > 65535) { u->err = true; snprintf(u->errmsg, sizeof u->errmsg, "Port out of range 0-65535"); free(clean); return; }
        u->port = pv; u->has_port = true;
        free(clean); return;
    }
    if (lb || rb) { u->err = true; snprintf(u->errmsg, sizeof u->errmsg, "Invalid IPv6 URL"); free(clean); return; }
    /* non-bracketed: split at FIRST ':' (live partition) */
    const char *colon = strchr(hostinfo, ':');
    const char *hostend = colon ? colon : hostinfo + strlen(hostinfo);
    size_t hlen = (size_t)(hostend - hostinfo);
    if (hlen == 0) {
        u->hostname = NULL;  /* empty host -> None */
    } else {
        char *h = malloc(hlen + 1);
        memcpy(h, hostinfo, hlen); h[hlen] = '\0';
        u->hostname = mo_lower_ascii(h);
        free(h);
    }
    if (colon) {
        const char *portstr = colon + 1;
        if (*portstr == '\0') { free(clean); return; }  /* empty -> None */
        bool digits = true;
        for (const char *q = portstr; *q; q++)
            if (*q < '0' || *q > '9') { digits = false; break; }
        if (!digits) {
            u->err = true;
            snprintf(u->errmsg, sizeof u->errmsg, "Port could not be cast to integer value as '%s'", portstr);
            free(clean); return;
        }
        long pv = strtol(portstr, NULL, 10);
        if (pv < 0 || pv > 65535) { u->err = true; snprintf(u->errmsg, sizeof u->errmsg, "Port out of range 0-65535"); free(clean); return; }
        u->port = pv; u->has_port = true;
    }
    free(clean);
}
/* ── _refresh_lock_path: path.with_suffix(path.suffix + ".refresh.lock") ── */
/* PoP: mof_refresh_lock_path @ tools/mcp_oauth.py:_refresh_lock_path */
json_t *mof_refresh_lock_path(const char *path) {
    char *norm = mof_path_normalize(path);
    if (!norm) return mof_raise("ValueError", "path normalization failed");
    /* empty name ('/', '.', '') raises in live; fixtures avoid those shapes */
    const char *slash = strrchr(norm, '/');
    const char *name = slash ? slash + 1 : norm;
    if (name[0] == '\0' || strcmp(name, ".") == 0) {
        free(norm);
        return mof_raise("ValueError", "path has an empty name");
    }
    size_t n = strlen(norm);
    char *out = malloc(n + 16);
    if (!out) { free(norm); return mof_raise("ValueError", "oom"); }
    snprintf(out, n + 16, "%s.refresh.lock", norm);
    free(norm);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* close(fd) then propagate orig; a raising close REPLACES orig (live bare
 * `raise` is never reached when os.close itself raises). */
static json_t *mof_close_propagate(mof_io_t *io, json_t *orig) {
    json_t *c = seam(io, "os.close", NULL);
    if (mof_raised(c)) { json_free(orig); return c; }
    json_free(c);
    return orig;
}

/* initial-try OSError -> RefreshFenceTimeout("refresh fence unavailable (name): exc") */
static json_t *mof_fence_unavail(json_t *r, const char *lname) {
    char msg[512];
    snprintf(msg, sizeof msg, "refresh fence unavailable (%s): %s", lname, mof_exc_msg(r));
    json_free(r);
    return mof_raise("RefreshFenceTimeout", msg);
}

/* ── acquire_refresh_fence: fail-closed fence across read -> POST -> persist ── */
/* PoP: mof_acquire_refresh_fence @ tools/mcp_oauth.py:acquire_refresh_fence */
json_t *mof_acquire_refresh_fence(mof_io_t *io, const char *path, double timeout, const char *platform) {
    json_t *lp = mof_refresh_lock_path(path);
    if (mof_raised(lp)) return lp;
    const char *lock_path = lp->str_val ? lp->str_val : "";
    const char *slash = strrchr(lock_path, '/');
    /* copy the basename to a stack buffer so it survives json_free(lp) */
    char lname[256];
    snprintf(lname, sizeof lname, "%s", slash ? slash + 1 : lock_path);

    /* try: mkdir / secure_parent_dir / os.open — except OSError -> unavailable.
     * BaseException/other propagates WITHOUT close (fd not opened yet). */
    json_t *r = seam(io, "mkdir_under_hermes_home", NULL);
    if (mof_raised(r)) {
        json_free(lp);
        if (mof_exc_is(r, "OSError")) return mof_fence_unavail(r, lname);
        return r;
    }
    json_free(r);
    r = seam(io, "secure_parent_dir", NULL);
    if (mof_raised(r)) {
        json_free(lp);
        if (mof_exc_is(r, "OSError")) return mof_fence_unavail(r, lname);
        return r;
    }
    json_free(r);
    r = seam(io, "os.open", NULL);
    if (mof_raised(r)) {
        json_free(lp);
        if (mof_exc_is(r, "OSError")) return mof_fence_unavail(r, lname);
        return r;
    }
    long fd = (r->type == JSON_NUMBER) ? (long)r->num_val : 0;
    json_free(r);

    /* deadline = time.monotonic() + timeout (outer BaseException -> close) */
    r = seam(io, "time.monotonic", NULL);
    if (mof_raised(r)) { json_free(lp); return mof_close_propagate(io, r); }
    double deadline = (r->type == JSON_NUMBER ? r->num_val : 0.0) + timeout;
    json_free(r);

    for (;;) {
        json_t *lk;
        if (strcmp(platform, "windows") == 0) {
            lk = seam(io, "msvcrt.locking", NULL);
        } else if (strcmp(platform, "posix") == 0) {
            lk = seam(io, "fcntl.flock", NULL);
        } else {
            /* no advisory locking primitive: RefreshFenceTimeout is NOT OSError,
             * so the inner except skips it -> outer close + raise. */
            json_free(lp);
            json_t *unsup = mof_raise("RefreshFenceTimeout",
                "refresh fence unsupported: no flock/msvcrt on this platform");
            return mof_close_propagate(io, unsup);
        }
        if (!mof_raised(lk)) {
            json_free(lk); json_free(lp);
            return json_number((double)fd);
        }
        if (mof_exc_is(lk, "OSError")) {
            long en = (long)json_get_num(lk, "errno", 0);
            if (!mo_fence_contention_errno(en)) {
                char msg[512];
                snprintf(msg, sizeof msg, "refresh fence unavailable on this filesystem: %s", mof_exc_msg(lk));
                json_free(lk); json_free(lp);
                return mof_close_propagate(io, mof_raise("RefreshFenceTimeout", msg));
            }
            json_free(lk);
            json_t *now = seam(io, "time.monotonic", NULL);
            if (mof_raised(now)) { json_free(lp); return mof_close_propagate(io, now); }
            bool expired = (now->type == JSON_NUMBER ? now->num_val : 0.0) >= deadline;
            json_free(now);
            if (expired) {
                char msg[512];
                snprintf(msg, sizeof msg, "refresh fence held by a peer for %.0fs (%s)", timeout, lname);
                json_free(lp);
                return mof_close_propagate(io, mof_raise("RefreshFenceTimeout", msg));
            }
            json_t *sl = seam(io, "asyncio.sleep", NULL);
            if (mof_raised(sl)) { json_free(lp); return mof_close_propagate(io, sl); }
            json_free(sl);
            continue;
        }
        /* non-OSError from the lock -> outer close + propagate */
        json_free(lp);
        return mof_close_propagate(io, lk);
    }
}

/* ── release_refresh_fence: unlock then close, never raises OSError ────── */
/* PoP: mof_release_refresh_fence @ tools/mcp_oauth.py:release_refresh_fence */
json_t *mof_release_refresh_fence(mof_io_t *io, long fd, const char *platform) {
    (void)fd;
    json_t *r = NULL;
    if (strcmp(platform, "windows") == 0) r = seam(io, "msvcrt.locking", NULL);
    else if (strcmp(platform, "posix") == 0) r = seam(io, "fcntl.flock", NULL);
    /* platform "none": no lock primitive -> no lock call, finally still closes */
    if (r && mof_raised(r)) {
        if (mof_exc_is(r, "OSError")) {
            json_free(r);
        } else {
            /* finally: suppress(OSError) close, then propagate the original */
            json_t *c = seam(io, "os.close", NULL);
            if (mof_raised(c)) {
                if (mof_exc_is(c, "OSError")) json_free(c);
                else { json_free(r); return c; }
            } else {
                json_free(c);
            }
            return r;
        }
    } else if (r) {
        json_free(r);
    }
    json_t *c = seam(io, "os.close", NULL);
    if (mof_raised(c)) {
        if (mof_exc_is(c, "OSError")) { json_free(c); return json_null(); }
        return c;
    }
    json_free(c);
    return json_null();
}

/* ── _sdk_class: lazy SDK probe, cached; broken SDK probed once ────────── */
/* PoP: mof_sdk_class @ tools/mcp_oauth.py:_sdk_class */
json_t *mof_sdk_class(mof_io_t *io, const char *name) {
    json_t *classes = mof_state_get(io, "_SDK_CLASSES");
    if (!classes || classes->type != JSON_OBJECT) {
        classes = json_object();
        mof_state_set(io, "_SDK_CLASSES", classes);
    }
    if (classes->c.count == 0) {
        bool failed = false;
        json_t *ca = seam(io, "import_client_auth", NULL);
        json_t *sa = NULL;
        if (mof_raised(ca)) {
            if (mof_exc_is(ca, "ImportError") || mof_exc_is(ca, "AttributeError")) failed = true;
            else { return ca; }  /* ownership passes to the caller — do NOT free */
        } else {
            sa = seam(io, "import_shared_auth", NULL);
            if (mof_raised(sa)) {
                if (mof_exc_is(sa, "ImportError") || mof_exc_is(sa, "AttributeError")) failed = true;
                else { json_free(ca); return sa; }  /* sa ownership passes to caller */
            } else {
                json_t *prov = mof_get(ca, "OAuthClientProvider");
                if (!prov) {
                    failed = true;  /* AttributeError on _client_auth.OAuthClientProvider */
                } else {
                    json_set(classes, "OAuthClientProvider", json_copy(prov));
                    for (size_t i = 1; MO_SDK_CLASS_NAMES[i]; i++) {
                        json_t *v = mof_get(sa, MO_SDK_CLASS_NAMES[i]);
                        if (!v) { failed = true; break; }
                        json_set(classes, MO_SDK_CLASS_NAMES[i], json_copy(v));
                    }
                }
            }
        }
        if (ca) json_free(ca);
        if (sa) json_free(sa);
        if (failed) {
            /* _SDK_CLASSES.update(dict.fromkeys(names)); _OAUTH_AVAILABLE = False */
            for (size_t i = 0; MO_SDK_CLASS_NAMES[i]; i++)
                json_set(classes, MO_SDK_CLASS_NAMES[i], json_null());
            mof_state_set(io, "_OAUTH_AVAILABLE", json_bool(false));
            mof_log(io, "debug");
        }
    }
    json_t *v = mof_get(classes, name);
    return v ? json_copy(v) : json_null();
}

/* ── _bind_reserved: bind + park, cap evicts ephemeral parks only ──────── */
/* PoP: mof_bind_reserved @ tools/mcp_oauth.py:_bind_reserved */
json_t *mof_bind_reserved(mof_io_t *io, long port) {
    json_t *s = seam(io, "socket.socket", NULL);
    if (mof_raised(s)) return s;  /* socket.socket() is outside the try */
    json_t *b = seam(io, "sock.bind", NULL);
    if (mof_raised(b)) {
        if (mof_exc_is(b, "OSError")) {
            json_t *c = seam(io, "sock.close", NULL);
            if (mof_raised(c)) { json_free(b); json_free(s); return c; }
            json_free(c);
            if (port) { json_free(b); json_free(s); return json_null(); }
            json_free(s);
            return b;  /* port 0: re-raise the original bind error */
        }
        json_free(s);
        return b;  /* non-OSError: no close */
    }
    json_free(b);
    json_t *g = seam(io, "sock.getsockname", NULL);
    if (mof_raised(g)) { json_free(s); return g; }
    json_t *bp = json_get(g, 1);
    long bound = (bp && bp->type == JSON_NUMBER) ? (long)bp->num_val : 0;
    json_free(g);

    json_t *reserves = mof_state_get(io, "_reserved_sockets");
    if (!reserves || reserves->type != JSON_OBJECT) {
        reserves = json_object();
        mof_state_set(io, "_reserved_sockets", reserves);
    }
    while (reserves->c.count >= MO_MAX_RESERVED_SOCKETS) {
        const char *stale_key = NULL;
        for (size_t i = 0; i < reserves->c.count; i++) {
            long p = strtol(reserves->c.keys[i], NULL, 10);
            bool cimd = false;
            for (size_t j = 0; j < sizeof MO_CIMD_PORTS / sizeof MO_CIMD_PORTS[0]; j++)
                if (MO_CIMD_PORTS[j] == p) cimd = true;
            if (!cimd) { stale_key = reserves->c.keys[i]; break; }
        }
        if (!stale_key) break;  /* only pinned sockets remain — never evict */
        json_obj_del(reserves, stale_key);  /* pop(stale_port) */
        json_t *c = seam(io, "sock.close", NULL);  /* suppress(OSError) */
        if (mof_raised(c)) {
            if (mof_exc_is(c, "OSError")) json_free(c);
            else { json_free(s); return c; }
        } else {
            json_free(c);
        }
    }
    char key[32];
    snprintf(key, sizeof key, "%ld", bound);
    json_set(reserves, key, json_copy(s));
    json_free(s);
    return json_number((double)bound);
}

/* ── _cached_client_info: on-disk registration or None ─────────────────── */
/* PoP: mof_cached_client_info @ tools/mcp_oauth.py:_cached_client_info */
json_t *mof_cached_client_info(mof_io_t *io, const json_t *storage) {
    if (!mof_not_none(storage)) return json_null();
    /* a non-storage object lacks _client_info_path -> AttributeError caught */
    if (storage->type != JSON_OBJECT || mof_truthy(mof_get(storage, "broken")))
        return json_null();
    json_t *p = seam(io, "storage._client_info_path", NULL);
    if (mof_raised(p)) {
        if (mof_exc_is(p, "AttributeError") || mof_exc_is(p, "TypeError") || mof_exc_is(p, "ValueError")) {
            json_free(p);
            return json_null();
        }
        return p;
    }
    json_free(p);
    json_t *info = seam(io, "_read_json", NULL);
    if (mof_raised(info)) {
        if (mof_exc_is(info, "AttributeError") || mof_exc_is(info, "TypeError") || mof_exc_is(info, "ValueError")) {
            json_free(info);
            return json_null();
        }
        return info;
    }
    if (info && info->type == JSON_OBJECT) return info;  /* isinstance(info, dict) */
    if (info) json_free(info);
    return json_null();
}

/* ── _cached_redirect: (https proxy URI, loopback port) from registration ── */
/* PoP: mof_cached_redirect @ tools/mcp_oauth.py:_cached_redirect */
json_t *mof_cached_redirect(mof_io_t *io, const json_t *storage) {
    json_t *info = mof_cached_client_info(io, storage);
    if (mof_raised(info)) return info;
    json_t *uri = NULL, *port = NULL;
    json_t *uris = (info && info->type == JSON_OBJECT) ? mof_get(info, "redirect_uris") : NULL;
    if (uris && uris->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(uris); i++) {
            json_t *raw = json_get(uris, i);
            char *s = mof_pystr(raw);  /* str(raw) */
            mo_url_t u;
            mo_urlparse(s, &u);
            if (u.err) { free(s); mo_url_free(&u); continue; }  /* (TypeError, ValueError) */
            if (!uri && strcmp(u.scheme, "https") == 0 && u.netloc[0] != '\0')
                uri = json_string(s);
            bool loopback = strcmp(u.scheme, "http") == 0 && strcmp(u.path, "/callback") == 0
                && u.hostname
                && (strcmp(u.hostname, "127.0.0.1") == 0 || strcmp(u.hostname, "localhost") == 0);
            if (!port && loopback && u.has_port)
                port = json_number((double)u.port);  /* int(parsed_port) */
            free(s);
            mo_url_free(&u);
        }
    }
    if (info) json_free(info);
    json_t *out = json_array();
    json_append(out, uri ? uri : json_null());
    json_append(out, port ? port : json_null());
    return out;
}
/* ── Python numeric coercion (ASCII subset, verified against live) ─────── */
static const char *mof_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

static bool mo_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static bool mo_word_ci(const char *p, const char *word) {
    size_t i = 0;
    for (; word[i]; i++)
        if (tolower((unsigned char)p[i]) != (unsigned char)word[i]) return false;
    return p[i] == '\0';
}

/* float(str): whitespace, sign, inf/infinity/nan, digits with underscores
 * (only between digits), optional fraction / exponent. Writes a strtod-clean
 * copy. Returns false on ValueError. */
static bool mo_float_parse(const char *s, double *out) {
    if (!s) return false;
    size_t n = strlen(s);
    char *clean = malloc(n + 16);
    if (!clean) return false;
    size_t k = 0;
    const char *p = s;
    while (mo_isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { clean[k++] = *p; p++; }
    if (mo_word_ci(p, "infinity") || mo_word_ci(p, "inf")) {
        strcpy(clean + k, "inf");
        *out = strtod(clean, NULL);
        free(clean);
        return true;
    }
    if (mo_word_ci(p, "nan")) {
        strcpy(clean + k, "nan");
        *out = strtod(clean, NULL);
        free(clean);
        return true;
    }
    bool digit_seen = false, prev_digit = false;
    while (*p) {
        if (isdigit((unsigned char)*p)) { clean[k++] = *p; digit_seen = true; prev_digit = true; p++; }
        else if (*p == '_' && prev_digit && isdigit((unsigned char)p[1])) { prev_digit = false; p++; }
        else break;
    }
    if (*p == '.') {
        clean[k++] = *p; p++;
        prev_digit = false;
        while (*p) {
            if (isdigit((unsigned char)*p)) { clean[k++] = *p; digit_seen = true; prev_digit = true; p++; }
            else if (*p == '_' && prev_digit && isdigit((unsigned char)p[1])) { prev_digit = false; p++; }
            else break;
        }
    }
    if (!digit_seen) { free(clean); return false; }
    if (*p == 'e' || *p == 'E') {
        clean[k++] = 'e'; p++;
        if (*p == '+' || *p == '-') { clean[k++] = *p; p++; }
        bool exp_digit = false;
        prev_digit = false;
        while (*p) {
            if (isdigit((unsigned char)*p)) { clean[k++] = *p; exp_digit = true; prev_digit = true; p++; }
            else if (*p == '_' && prev_digit && isdigit((unsigned char)p[1])) { prev_digit = false; p++; }
            else break;
        }
        if (!exp_digit) { free(clean); return false; }
    }
    while (mo_isspace((unsigned char)*p)) p++;
    if (*p != '\0') { free(clean); return false; }
    clean[k] = '\0';
    *out = strtod(clean, NULL);
    free(clean);
    return true;
}

/* _seconds(value, default): max(0.0, float(value)); TypeError/ValueError -> default */
static double mof_seconds(const json_t *v, double def) {
    double x;
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) x = v->bool_val ? 1.0 : 0.0;
    else if (v->type == JSON_NUMBER) x = v->num_val;
    else if (v->type == JSON_STRING) {
        if (!mo_float_parse(v->str_val, &x)) return def;
    } else return def;
    return x > 0.0 ? x : 0.0;  /* max(0.0, x): nan -> 0.0, -0.0 -> 0.0 */
}

/* int(value): trunc toward zero; str base-10 with sign/underscores; bool 0/1.
 * Returns a number node or a __raise__ (ValueError/TypeError/OverflowError). */
static json_t *mof_to_int(const json_t *v) {
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isnan(d)) return mof_raise("ValueError", "cannot convert float NaN to integer");
        if (isinf(d)) return mof_raise("OverflowError", "cannot convert float infinity to integer");
        return json_number((double)(long long)d);
    }
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (mo_isspace((unsigned char)*p)) p++;
        const char *start = p;
        if (*p == '+' || *p == '-') p++;
        bool digit = false, prev_digit = false;
        long long val = 0;
        bool neg = (*start == '-');
        const char *q = p;
        while (*q) {
            if (isdigit((unsigned char)*q)) {
                val = val * 10 + (*q - '0');
                digit = true; prev_digit = true; q++;
            } else if (*q == '_' && prev_digit && isdigit((unsigned char)q[1])) {
                prev_digit = false; q++;
            } else break;
        }
        const char *end = q;
        while (mo_isspace((unsigned char)*end)) end++;
        if (digit && *end == '\0')
            return json_number((double)(neg ? -val : val));
        char *rr = mof_py_repr(v);
        char msg[256];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", rr ? rr : "''");
        free(rr);
        return mof_raise("ValueError", msg);
    }
    char msg[160];
    snprintf(msg, sizeof msg,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        mof_typename(v));
    return mof_raise("TypeError", msg);
}

/* suppress(OSError, TypeError, ValueError) membership */
static bool mof_suppress_otv(const json_t *r) {
    return mof_exc_is(r, "OSError") || mof_exc_is(r, "TypeError") || mof_exc_is(r, "ValueError");
}

/* ── _rebase_expires_in: absolute expiry -> seconds remaining ──────────── */
/* PoP: mof_rebase_expires_in @ tools/mcp_oauth.py:_rebase_expires_in */
json_t *mof_rebase_expires_in(mof_io_t *io, json_t *data) {
    json_t *abs_node = mof_get(data, "expires_at");
    json_t *abs_copy = abs_node ? json_copy(abs_node) : NULL;
    json_obj_del(data, "expires_at");  /* data.pop("expires_at", None) */
    if (mof_not_none(abs_copy)) {
        json_t *t = seam(io, "time.time", NULL);
        if (mof_raised(t)) { json_free(abs_copy); return t; }
        double now = (t->type == JSON_NUMBER) ? t->num_val : 0.0;
        /* int-like doubles model Python ints (harness serializes them int-like,
         * so the live right operand is an int) — name the type, don't pin it. */
        const char *tname = mof_typename(t);
        json_free(t);
        if (abs_copy->type != JSON_NUMBER && abs_copy->type != JSON_BOOL) {
            char msg[128];
            snprintf(msg, sizeof msg, "unsupported operand type(s) for -: '%s' and '%s'",
                mof_typename(abs_copy), tname);
            json_free(abs_copy);
            return mof_raise("TypeError", msg);
        }
        double ae = (abs_copy->type == JSON_BOOL) ? (abs_copy->bool_val ? 1.0 : 0.0) : abs_copy->num_val;
        json_free(abs_copy);
        double v = ae - now;
        if (v < 0.0) v = 0.0;  /* max(..., 0) */
        json_set(data, "expires_in", json_number((double)(long long)v));  /* int() */
        return json_null();
    }
    if (abs_copy) json_free(abs_copy);
    json_t *ein = mof_get(data, "expires_in");
    if (mof_not_none(ein)) {
        /* with suppress(OSError, TypeError, ValueError):
         *   implied = self._tokens_path().stat().st_mtime + int(data["expires_in"])
         *   data["expires_in"] = int(max(implied - time.time(), 0)) */
        json_t *p = seam(io, "storage._tokens_path", NULL);
        if (mof_raised(p)) {
            if (mof_suppress_otv(p)) { json_free(p); return json_null(); }
            return p;
        }
        json_free(p);
        json_t *st = seam(io, "path.stat", NULL);
        if (mof_raised(st)) {
            if (mof_suppress_otv(st)) { json_free(st); return json_null(); }
            return st;
        }
        double mtime = json_get_num(st, "st_mtime", 0.0);
        json_free(st);
        json_t *iv = mof_to_int(mof_get(data, "expires_in"));
        if (mof_raised(iv)) {
            if (mof_suppress_otv(iv)) { json_free(iv); return json_null(); }
            return iv;
        }
        double implied = mtime + iv->num_val;
        json_free(iv);
        json_t *t = seam(io, "time.time", NULL);
        if (mof_raised(t)) {
            if (mof_suppress_otv(t)) { json_free(t); return json_null(); }
            return t;
        }
        double v = implied - (t->type == JSON_NUMBER ? t->num_val : 0.0);
        json_free(t);
        if (v < 0.0) v = 0.0;
        json_set(data, "expires_in", json_number((double)(long long)v));
    }
    return json_null();
}

/* ── _fixup_loaded_tokens: pop issuer bookkeeping, then rebase ─────────── */
/* PoP: mof_fixup_loaded_tokens @ tools/mcp_oauth.py:_fixup_loaded_tokens */
json_t *mof_fixup_loaded_tokens(mof_io_t *io, json_t *data) {
    json_t *iss = mof_get(data, "hermes_issuer");
    json_t *iss_copy = iss ? json_copy(iss) : json_null();
    json_obj_del(data, "hermes_issuer");
    mof_state_set(io, "loaded_issuer", iss_copy);  /* pop absent -> None */
    return mof_rebase_expires_in(io, data);
}

/* ── stamp_issuer: backfill hermes_issuer onto a pre-binding token file ── */
/* PoP: mof_stamp_issuer @ tools/mcp_oauth.py:stamp_issuer */
json_t *mof_stamp_issuer(mof_io_t *io, const char *issuer) {
    json_t *p1 = seam(io, "storage._tokens_path", NULL);
    if (mof_raised(p1)) return p1;
    json_free(p1);
    json_t *data = seam(io, "_read_json", NULL);
    if (mof_raised(data)) return data;
    if (!mof_not_none(data)) { json_free(data); return json_null(); }
    if (data->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", mof_typename(data));
        json_free(data);
        return mof_raise("AttributeError", msg);
    }
    if (mof_truthy(mof_get(data, "hermes_issuer"))) { json_free(data); return json_null(); }
    json_set(data, "hermes_issuer", json_string(issuer));  /* str(issuer) */
    /* try: _write_json(self._tokens_path(), data) — path call is INSIDE try */
    json_t *p2 = seam(io, "storage._tokens_path", NULL);
    if (mof_raised(p2)) {
        if (mof_exc_is(p2, "OSError")) { json_free(p2); json_free(data); mof_log(io, "debug"); return json_null(); }
        json_free(data);
        return p2;
    }
    json_free(p2);
    json_t *w = seam(io, "_write_json", NULL);
    if (mof_raised(w)) {
        if (mof_exc_is(w, "OSError")) { json_free(w); json_free(data); mof_log(io, "debug"); return json_null(); }
        json_free(data);
        return w;
    }
    json_free(w);
    json_free(data);
    mof_state_set(io, "loaded_issuer", json_string(issuer));
    return json_null();
}

/* ── strip_refresh_token: drop refresh token + issuer, keep access token ── */
/* PoP: mof_strip_refresh_token @ tools/mcp_oauth.py:strip_refresh_token */
json_t *mof_strip_refresh_token(mof_io_t *io) {
    json_t *p1 = seam(io, "storage._tokens_path", NULL);
    if (mof_raised(p1)) return p1;
    json_free(p1);
    json_t *data = seam(io, "_read_json", NULL);
    if (mof_raised(data)) return data;
    if (!mof_not_none(data)) { json_free(data); return json_null(); }
    if (data->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", mof_typename(data));
        json_free(data);
        return mof_raise("AttributeError", msg);
    }
    if (!mof_truthy(mof_get(data, "refresh_token"))) { json_free(data); return json_null(); }
    json_obj_del(data, "refresh_token");
    json_obj_del(data, "hermes_issuer");
    mof_state_set(io, "loaded_issuer", json_null());
    json_t *p2 = seam(io, "storage._tokens_path", NULL);
    if (mof_raised(p2)) {
        if (mof_exc_is(p2, "OSError")) { json_free(p2); json_free(data); mof_log(io, "warning"); return json_null(); }
        json_free(data);
        return p2;
    }
    json_free(p2);
    json_t *w = seam(io, "_write_json", NULL);
    if (mof_raised(w)) {
        if (mof_exc_is(w, "OSError")) { json_free(w); json_free(data); mof_log(io, "warning"); return json_null(); }
        json_free(data);
        return w;
    }
    json_free(w);
    json_free(data);
    mof_log(io, "info");
    return json_null();
}

/* ── _coerce_secret_auth_method: secret present, method unset -> post ──── */
/* PoP: mof_coerce_secret_auth_method @ tools/mcp_oauth.py:_coerce_secret_auth_method */
json_t *mof_coerce_secret_auth_method(json_t *data) {
    json_t *secret = mof_get(data, "client_secret");
    json_t *method = mof_get(data, "token_endpoint_auth_method");
    bool method_unset = !method || method->type == JSON_NULL
        || (method->type == JSON_STRING && method->str_val
            && (strcmp(method->str_val, "none") == 0 || strcmp(method->str_val, "") == 0));
    if (mof_truthy(secret) && method_unset) {
        json_set(data, "token_endpoint_auth_method", json_string("client_secret_post"));
        return json_bool(true);
    }
    return json_bool(false);
}

/* ── _load_model: read -> shape gate -> SDK class -> device branch -> fixup
 *    -> validate, with the corrupt-file / validation-error logging paths ── */
/* PoP: mof_load_model @ tools/mcp_oauth.py:_load_model */
json_t *mof_load_model(mof_io_t *io, const char *path, const char *sdk_name,
                       const char *label, bool fixup) {
    (void)path; (void)label;
    json_t *data = seam(io, "_read_json", NULL);
    if (mof_raised(data)) return data;
    bool have_data = mof_not_none(data);
    if (have_data && data->type != JSON_OBJECT) {
        /* non-object payload: warning + None (before any SDK probe) */
        mof_log(io, "warning");
        json_free(data);
        return json_null();
    }
    json_t *cls = NULL;  /* class marker node (owned) */
    if (have_data) {
        cls = seam(io, "_sdk_class", NULL);
        if (mof_raised(cls)) { json_free(data); return cls; }
        if (!mof_not_none(cls)) { json_free(cls); cls = NULL; }
    }
    if (cls && strcmp(sdk_name, "OAuthMetadata") == 0 && data->type == JSON_OBJECT
        && mof_truthy(mof_get(data, "device_authorization_endpoint"))) {
        json_t *dm = seam(io, "import_device_metadata", NULL);
        if (mof_raised(dm)) { json_free(cls); json_free(data); return dm; }
        json_free(cls);
        if (mof_not_none(dm)) cls = dm;
        else { json_free(dm); cls = NULL; }
    }
    if (!cls) { if (data) json_free(data); return json_null(); }
    if (fixup) {
        json_t *f = seam(io, "fixup", NULL);
        if (mof_raised(f)) { json_free(cls); json_free(data); return f; }
        json_free(f);
    }
    /* try: cls.model_validate(data) except (ValueError, TypeError, KeyError) */
    json_t *mv = seam(io, "model_validate", NULL);
    if (mof_raised(mv)) {
        if (mof_exc_is(mv, "ValueError") || mof_exc_is(mv, "TypeError") || mof_exc_is(mv, "KeyError")) {
            /* detail = exc, or "validation failed for <locs>" when exc.errors;
             * logger.warning carries it (trace records the level only). */
            mof_log(io, "warning");
            json_free(mv); json_free(cls); json_free(data);
            return json_null();
        }
        json_free(cls); json_free(data);
        return mv;
    }
    json_free(cls);
    json_free(data);
    return mv;
}
/* ── _result_taken: a terminal result is already present ───────────────── */
/* PoP: mof_result_taken @ tools/mcp_oauth.py:_result_taken */
json_t *mof_result_taken(const json_t *result) {
    json_t *a = mof_get(result, "auth_code");
    json_t *e = mof_get(result, "error");
    return json_bool(mof_not_none(a) || mof_not_none(e));
}

/* ── _claim_result: write values unless terminal; atomic under lock ────── */
/* PoP: mof_claim_result @ tools/mcp_oauth.py:_claim_result */
json_t *mof_claim_result(mof_io_t *io, json_t *result, const json_t *values) {
    json_t *e = seam(io, "lock.__enter__", NULL);
    if (mof_raised(e)) return e;  /* __enter__ raise: __exit__ never runs */
    json_free(e);
    json_t *taken = mof_result_taken(result);
    bool won = !taken->bool_val;
    json_free(taken);
    if (won && values && values->type == JSON_OBJECT) {
        for (size_t i = 0; i < values->c.count; i++)
            json_set(result, values->c.keys[i], json_copy(values->c.items[i]));
    }
    json_t *x = seam(io, "lock.__exit__", NULL);
    if (mof_raised(x)) return x;
    json_free(x);
    return json_bool(won);
}

/* OSError inside the server try -> OAuthNonInteractiveError("...already in use...") */
static json_t *mof_server_in_use(json_t *r, long port) {
    char msg[512];
    snprintf(msg, sizeof msg,
        "OAuth callback port %ld is already in use (%s). Close any other in-progress login, "
        "or set a free `oauth.redirect_port` in the server config, then retry.",
        port, mof_exc_msg(r));
    json_free(r);
    return mof_raise("OAuthNonInteractiveError", msg);
}

/* ── _start_callback_server: adopt parked socket OR bind+activate ──────── */
/* PoP: mof_start_callback_server @ tools/mcp_oauth.py:_start_callback_server */
json_t *mof_start_callback_server(mof_io_t *io, long port, const char *handler) {
    (void)handler;
    json_t *ret = NULL;
    json_t *addr = NULL;
    json_t *srv = seam(io, "CallbackServer", NULL);
    if (mof_raised(srv)) {
        if (mof_exc_is(srv, "OSError")) return mof_server_in_use(srv, port);
        return srv;
    }
    char *server_id = strdup(mof_get_str(srv, "server", ""));
    char *fresh_sock = strdup(mof_get_str(srv, "socket", ""));
    json_t *reserves = mof_state_get(io, "_reserved_sockets");
    char key[32];
    snprintf(key, sizeof key, "%ld", port);
    json_t *reserved = reserves ? mof_get(reserves, key) : NULL;
    bool have_reserved = mof_not_none(reserved);
    /* snapshot the parked sock id BEFORE pop frees the node */
    char *reserved_id = have_reserved ? strdup(reserved->str_val ? reserved->str_val : "") : NULL;
    if (have_reserved) json_obj_del(reserves, key);  /* _reserved_sockets.pop(port, None) */
    const char *final_sock = fresh_sock;
    /* HTTPServer.allow_reuse_address = 1; the else branch sets it True */
    if (have_reserved) {
        json_t *c = seam(io, "server.socket.close", NULL);
        if (mof_raised(c)) {
            ret = mof_exc_is(c, "OSError") ? mof_server_in_use(c, port) : c;
            goto done;
        }
        json_free(c);
        json_t *g = seam(io, "reserved.getsockname", NULL);
        if (mof_raised(g)) {
            ret = mof_exc_is(g, "OSError") ? mof_server_in_use(g, port) : g;
            goto done;
        }
        addr = g;  /* server.server_address = reserved.getsockname() */
        final_sock = reserved_id;
    } else {
        json_t *b = seam(io, "server.server_bind", NULL);
        if (mof_raised(b)) {
            ret = mof_exc_is(b, "OSError") ? mof_server_in_use(b, port) : b;
            goto done;
        }
        json_free(b);
        addr = json_array();
        json_append(addr, json_string("127.0.0.1"));
        json_append(addr, json_number((double)port));
    }
    /* server.server_activate() runs in BOTH branches (after the if/else) */
    json_t *a = seam(io, "server.server_activate", NULL);
    if (mof_raised(a)) {
        ret = mof_exc_is(a, "OSError") ? mof_server_in_use(a, port) : a;
        goto done;
    }
    json_free(a);
    ret = json_object();
    json_set(ret, "server", json_string(server_id ? server_id : ""));
    json_set(ret, "socket", json_string(final_sock ? final_sock : ""));
    json_set(ret, "server_address", addr);
    addr = NULL;  /* ownership transferred into ret */
    json_set(ret, "allow_reuse_address", json_bool(true));
done:
    free(server_id);
    free(fresh_sock);
    free(reserved_id);
    json_free(srv);
    if (addr) json_free(addr);
    return ret;
}

/* ── _callback_outcome: skip / error / timeout / success branches ──────── */
/* PoP: mof_callback_outcome @ tools/mcp_oauth.py:_callback_outcome */
json_t *mof_callback_outcome(mof_io_t *io, const json_t *result, const char *cimd_url) {
    json_t *err = mof_get(result, "error");
    if (!err) return mof_raise("KeyError", "'error'");
    if (err->type == JSON_STRING && err->str_val
        && strcmp(err->str_val, MO_USER_SKIPPED_SENTINEL) == 0)
        return mof_raise("OAuthNonInteractiveError", "user_skipped");
    if (mof_truthy(err)) {
        char *es = mof_pystr(err);
        char msg[512];
        snprintf(msg, sizeof msg, "OAuth authorization failed: %s", es ? es : "");
        free(es);
        return mof_raise("RuntimeError", msg);
    }
    json_t *code = mof_get(result, "auth_code");
    if (!code) return mof_raise("KeyError", "'auth_code'");
    if (!mof_not_none(code)) {
        char msg[1024];
        if (cimd_url && cimd_url[0] != '\0') {
            snprintf(msg, sizeof msg,
                "OAuth callback timed out \xe2\x80\x94 no authorization code received. "
                "Ensure you completed the browser authorization flow."
                " If the browser showed an invalid-client error instead of an approval prompt, "
                "the authorization server rejected Hermes' Client ID Metadata Document (%s); "
                "set ``cimd: false`` under that server's ``oauth:`` block in config.yaml to "
                "authorize via dynamic client registration instead.",
                cimd_url);
        } else {
            snprintf(msg, sizeof msg,
                "OAuth callback timed out \xe2\x80\x94 no authorization code received. "
                "Ensure you completed the browser authorization flow.");
        }
        return mof_raise("OAuthNonInteractiveError", msg);
    }
    json_t *state_v = mof_get(result, "state");
    if (!state_v) return mof_raise("KeyError", "'state'");
    /* iss = result.get("iss") — absent is None, never raises */
    return seam(io, "authorization_code_result", NULL);
}

/* ── login_connect_timeout: max(connect_timeout, oauth.timeout + 15) ───── */
/* PoP: mof_login_connect_timeout @ tools/mcp_oauth.py:login_connect_timeout */
json_t *mof_login_connect_timeout(const json_t *config) {
    json_t *oauth = mof_get(config, "oauth");
    json_t *empty = NULL;
    const json_t *ocfg;
    if (mof_truthy(oauth)) {
        ocfg = oauth;
    } else {
        empty = json_object();
        ocfg = empty;
    }
    double s1 = mof_seconds(mof_get(config, "connect_timeout"), 0.0);
    if (ocfg->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", mof_typename(ocfg));
        if (empty) json_free(empty);
        return mof_raise("AttributeError", msg);
    }
    double s2 = mof_seconds(mof_get(ocfg, "timeout"), 300.0) + 15.0;
    if (empty) json_free(empty);
    double r = s2 > s1 ? s2 : s1;  /* max(s1, s2) */
    return json_number(r);
}
