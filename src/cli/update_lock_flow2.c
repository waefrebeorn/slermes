/*
 * update_lock_flow2.c — FLOW-core port of the UNCLAIMED remainder of
 * hermes_cli/update_lock.py (LIVE authority). Sibling to port_update_lock_flow.c
 * (update_lock_flow), which claims the marker parse / incarnation / judge /
 * release / reclaim / claim state machine. This file ports the functions that
 * PoP grep shows are still unclaimed: the checkout-lock path + kernel-lock
 * state cluster, the marker I/O helpers, the identity/world adapters, and the
 * remaining UpdateLock methods.
 *
 * Self-contained C11, libjson-only. Every function is `static ul2_*` and takes
 * the universal seam io + a borrowed json_t *in, returning a NEW json_t*
 * (caller frees). The harness #includes this file by area-qualified name.
 *
 * SEAM DOCTRINE: every live effect (file reads/stats/iterdir/unlink, the clock,
 * os.getpid, the process probes, and EVERY sibling — claimed or not — such as
 * _parse_marker / _read_bytes / incarnation_live / _identity_live / _pid_alive /
 * _is_ancestor_pid / _handoff_pid / read_live_update / checkout_lock_held /
 * checkout_lock_path / _acquire_checkout / _release_checkout / _live_partners /
 * _bounded_int / _tmp_writer_gone / _compare_and_swap / _default_install_root /
 * update_marker_path / process_create_time / _own_create_time, plus the marker
 * methods marker.age / marker.live_pid and the runtime-host collaborators)
 * enters through ONE universal seam:
 *
 *   json_t *(*call)(ul2_io_t *io, const char *name, json_t *args)
 *
 * `name` identifies the live call; `args` is a BORROWED json object (the seam
 * copies what it keeps; ul2_call frees nothing — the caller frees args). The
 * result is a NEW json_t* the caller frees. NULL pointer == Python None. A
 * raised exception is {"__raise__": cls, "message": str, "errno": int?}.
 * ul2_call appends {"name","args"} to io->trace in call order BEFORE dispatch.
 *
 * Exception propagation: a seam raise is threaded up through the decision core
 * and returned as the function result unless a live except/suppress clause
 * catches it. All catches here are OSError-family (or (TypeError,ValueError) in
 * incarnation_live), so the BaseException trio (SystemExit / KeyboardInterrupt /
 * GeneratorExit) ALWAYS passes through. Fail-closed: a missing/empty seam script
 * yields a RuntimeError raise spec (recorded, then propagated/swallowed as live).
 *
 * Representation contract (shared with update_lock_flow + the oracle):
 * - Paths are POSIX strings; name/parent/with_name/join/normpath follow
 *   PurePosixPath / posixpath for the clean-path fixture domain.
 * - marker spec: {raw|raw_hex, pid, started_at, create_time, delegate_pid,
 *   delegate_create_time, in_flight, ct_text, delegate_ct_text, runs}. started_at
 *   is a JSON number or {"__bigint__": decimal} when > 2^53.
 * - holder spec: {pid, age_seconds, reason, held} (UpdateHolder).
 * - world: {pid, ct, now}.
 * - bytes travel as UTF-8 strings (ASCII fixture domain); None == null.
 * - Scripted OSError str(): with errno -> "[Errno N] message"; without ->
 *   "message" (both sides construct the exception identically; no filename).
 * - sys.platform is a case INPUT (in.platform), not a seam (constant read).
 * - logger calls are silent on both sides (not traced).
 *
 * Seam name table (name -> live call):
 *   module_file / path.resolve   _default_install_root's __file__.resolve()
 *   _default_install_root        _default_install_root()  [seamed sibling]
 *   _git_common_dir              _git_common_dir(root) -> path|null
 *   path.is_dir / path.is_file   Path.is_dir() / Path.is_file() -> bool
 *   path.read_text               Path.read_text(utf-8-sig) -> str (decoded)
 *   path.read_bytes              Path.read_bytes() -> bytes(str)
 *   path.stat                    Path.stat() -> {st_mtime}
 *   path.iterdir                 Path.iterdir() -> [entry paths]
 *   path.unlink                  Path.unlink()
 *   os.path.realpath             os.path.realpath(p) -> str
 *   os.getpid                    os.getpid() -> int
 *   os.close                     os.close(fd)
 *   os.open                      os.open(path, flags) -> fd
 *   os.replace                   os.replace(tmp, path)
 *   tmp.write                    open(tmp,'wb')+write+flush+fsync
 *   secrets.token_hex            secrets.token_hex(4) -> str
 *   time.time / time.monotonic   time.time() / time.monotonic() -> number
 *   time.sleep                   time.sleep(0.02)
 *   _own_create_time             _own_create_time() -> float|null  [claimed]
 *   process_create_time          process_create_time(pid) -> float|null
 *   _as_ct                       _as_ct(recorded) -> float|null  [claimed]
 *   _incarnation                 _incarnation(pid, recorded, w)  [claimed]
 *   _real_world                  _real_world() -> world
 *   incarnation_live             incarnation_live(pid, recorded_ct) -> bool|null
 *   _identity_live               _identity_live(...) -> bool  [claimed]
 *   _identity_line               _identity_line() -> "ct:<s>"|""
 *   _pid_alive                   _pid_alive(pid) -> bool  [claimed]
 *   _handoff_pid                 _handoff_pid() -> int|null  [claimed]
 *   _is_ancestor_pid             _is_ancestor_pid(pid) -> bool  [claimed]
 *   looks_like_gateway_command_line / _hermes_holder_subcommand  (_is_runtime_host)
 *   _runtime_host_below          _runtime_host_below(pid) -> bool
 *   _parse_marker                _parse_marker(raw, mtime=) -> marker spec
 *   _read_bytes                  _read_bytes(path) -> bytes|null
 *   _read_marker                 _read_marker(path) -> marker spec|null
 *   marker.age                   _Marker.age() -> number
 *   marker.live_pid              _Marker.live_pid() -> int|null
 *   _bounded_int                 _bounded_int(text, limit) -> int|null  [claimed]
 *   _tmp_sibling                 _tmp_sibling(path) -> path
 *   _tmp_writer_gone             _tmp_writer_gone(owner, entry) -> bool
 *   mutex.enter / mutex.exit     marker_mutex(path) enter/exit
 *   _publish_exclusive           _publish_exclusive(path, body) -> bool
 *   _compare_and_swap            _compare_and_swap(path, expected, new) -> bool
 *   _live_partners               _live_partners(marker) -> [pids]  [claimed]
 *   _release_decision            _release_decision(raw) -> {action,new}  [claimed]
 *   read_live_update             read_live_update(path=,install_root=) -> holder|null
 *   checkout_lock_path           checkout_lock_path(install_root) -> path
 *   checkout_lock_held           checkout_lock_held(install_root) -> bool
 *   holds_checkout_lock          holds_checkout_lock(install_root) -> bool
 *   _inherited_lock_fd           _inherited_lock_fd(path) -> fd|null
 *   _try_lock / _try_shared_lock _try_lock(fd) / _try_shared_lock(fd) -> bool
 *   _held_by_our_windows_ancestor _held_by_our_windows_ancestor(path) -> bool
 *   _unlock                      _unlock(fd, offset, count)
 *   _acquire_checkout            _acquire_checkout(install_root) -> holder|null
 *   _release_checkout            _release_checkout()
 *   legacy_profile_claims        legacy_profile_claims(root) -> [holders]
 *   self._is_partner             UpdateLock._is_partner(pid) -> bool
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h>
#include "hermes_json.h"

#define UL2_U32_MAX 4294967295ULL
#define UL2_U64_MAX 18446744073709551615ULL
#define UL2_BIGINT_SAFE 9007199254740992ULL  /* 2^53 */
#define UL2_MAX_AGE 1200.0
#define UL2_CT_TOL 2.0
#define UL2_OWN_EPS 0.005
#define UL2_WHOLE_CT 1.0
#define UL2_EMPTY_GRACE 5.0
#define UL2_TMP_STALE 60.0
#define UL2_CONTENTION_WAIT 3.0
#define UL2_WINDOWS_LOCK_OFFSET 1048576  /* 1 << 20 */
#define UL2_O_RDONLY 0
#define UL2_O_BINARY 32768

/* ── universal seam plumbing ─────────────────────────────────────────────── */
typedef struct ul2_io_t {
    json_t *(*call)(struct ul2_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ordered array of {"name","args"} */
    void   *ctx;
} ul2_io_t;

static json_t *ul2_call(ul2_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}
static bool ul2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *ul2_raise(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message ? message : "");
    return o;
}

/* ── Python truthiness / type tests ──────────────────────────────────────── */
static bool jtruth(const json_t *v) {
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool j_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *jstr(const json_t *v) { return j_is_str(v) ? v->str_val : ""; }
/* array element number reader (json_get_num is an OBJECT accessor only). */
static double arr_num(const json_t *arr, size_t i) {
    json_t *v = json_get(arr, i);
    return j_is_num(v) ? v->num_val : 0.0;
}

/* ── exception classification (OSError family as the live except clauses) ── */
static const char *exc_type(const json_t *r) { return json_get_str(r, "__raise__", ""); }
static bool exc_is_oserror(const json_t *r) {
    const char *t = exc_type(r);
    return strcmp(t, "OSError") == 0 || strcmp(t, "PermissionError") == 0
        || strcmp(t, "FileNotFoundError") == 0 || strcmp(t, "MarkerBusy") == 0
        || strcmp(t, "BlockingIOError") == 0 || strcmp(t, "FileExistsError") == 0
        || strcmp(t, "IsADirectoryError") == 0 || strcmp(t, "NotADirectoryError") == 0
        || strcmp(t, "InterruptedError") == 0 || strcmp(t, "TimeoutError") == 0;
}
static bool exc_is_fnf(const json_t *r) { return strcmp(exc_type(r), "FileNotFoundError") == 0; }
/* str(exc) parity: OSError(errno, msg) -> "[Errno N] msg"; OSError(msg) -> msg */
static char *exc_str(const json_t *r) {
    const char *msg = json_get_str(r, "message", "");
    json_t *en = json_obj_get(r, "errno");
    size_t cap = strlen(msg) + 48;
    char *out = malloc(cap);
    if (j_is_num(en)) snprintf(out, cap, "[Errno %.0f] %s", en->num_val, msg);
    else snprintf(out, cap, "%s", msg);
    return out;
}

/* ── u64 / bigint projection ─────────────────────────────────────────────── */
static json_t *u64_node(unsigned long long v) {
    if (v <= UL2_BIGINT_SAFE) return json_number((double)v);
    json_t *o = json_object();
    char buf[32];
    snprintf(buf, sizeof buf, "%" PRIu64, v);
    json_set_hdr(o, "__bigint__", buf);
    return o;
}
static bool node_to_u64(const json_t *v, unsigned long long *out) {
    if (j_is_num(v)) {
        if (v->num_val < 0 || v->num_val > 1.8446744073709552e19) return false;
        *out = (unsigned long long)v->num_val;
        return true;
    }
    if (j_is_obj(v)) {
        const char *s = json_get_str(v, "__bigint__", NULL);
        if (!s) return false;
        char *end = NULL;
        unsigned long long x = strtoull(s, &end, 10);
        if (!end || *end != '\0') return false;
        *out = x;
        return true;
    }
    return false;
}

/* ── POSIX path helpers (PurePosixPath / posixpath, clean-path domain) ───── */
static char *posix_parent(const char *p) {
    if (!p || !p[0]) return strdup(".");
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;
    if (n == 1 && p[0] == '/') return strdup("/");
    size_t last = 0; bool found = false;
    for (size_t i = 0; i < n; i++) if (p[i] == '/') { last = i; found = true; }
    if (!found) return strdup(".");
    if (last == 0) return strdup("/");
    char *out = malloc(last + 1);
    memcpy(out, p, last); out[last] = '\0';
    return out;
}
static char *posix_name(const char *p) {
    if (!p || !p[0]) return strdup("");
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;
    if (n == 1 && p[0] == '/') return strdup("");
    size_t start = 0;
    for (size_t i = 0; i < n; i++) if (p[i] == '/') start = i + 1;
    char *out = malloc(n - start + 1);
    memcpy(out, p + start, n - start); out[n - start] = '\0';
    return out;
}
/* PurePosixPath.with_name: replace the final component (parent must exist). */
static char *posix_with_name(const char *p, const char *newname) {
    char *par = posix_parent(p);
    size_t n = strlen(par) + strlen(newname) + 2;
    char *out = malloc(n);
    if (strcmp(par, ".") == 0) snprintf(out, n, "%s", newname);
    else if (strcmp(par, "/") == 0) snprintf(out, n, "/%s", newname);
    else snprintf(out, n, "%s/%s", par, newname);
    free(par);
    return out;
}
/* Path(a) / b : absolute b replaces a; otherwise join (keeps '..', no norm). */
static char *posix_join(const char *a, const char *b) {
    if (!b) b = "";
    if (b[0] == '/') return strdup(b);
    if (!a || !a[0]) return strdup(b);
    size_t na = strlen(a);
    bool slash = a[na - 1] == '/';
    size_t n = na + strlen(b) + 2;
    char *out = malloc(n);
    snprintf(out, n, "%s%s%s", a, slash ? "" : "/", b);
    return out;
}
/* posixpath.normpath (lexical). */
static char *posix_normpath(const char *path) {
    if (!path || !path[0]) return strdup(".");
    int initial = 0;
    if (path[0] == '/') {
        initial = 1;
        if (path[1] == '/' && path[2] != '/') initial = 2;
    }
    /* split into components, process */
    char **comps = NULL; size_t ncomps = 0, cap = 0;
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *s = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - s);
        if (len == 1 && s[0] == '.') continue;
        char *c = malloc(len + 1); memcpy(c, s, len); c[len] = '\0';
        if (strcmp(c, "..") == 0) {
            if ((!initial && ncomps == 0) || (ncomps > 0 && strcmp(comps[ncomps - 1], "..") == 0)) {
                if (ncomps == cap) { cap = cap ? cap * 2 : 8; comps = realloc(comps, cap * sizeof(char *)); }
                comps[ncomps++] = c;
            } else if (ncomps > 0) {
                free(comps[--ncomps]); free(c);
            } else {
                free(c);  /* initial slashes and empty: drop leading .. */
            }
        } else {
            if (ncomps == cap) { cap = cap ? cap * 2 : 8; comps = realloc(comps, cap * sizeof(char *)); }
            comps[ncomps++] = c;
        }
    }
    size_t total = (size_t)initial + 1;
    for (size_t i = 0; i < ncomps; i++) total += strlen(comps[i]) + 1;
    char *out = malloc(total + 1);
    size_t o = 0;
    for (int i = 0; i < initial; i++) out[o++] = '/';
    for (size_t i = 0; i < ncomps; i++) {
        if (i > 0) out[o++] = '/';
        size_t l = strlen(comps[i]); memcpy(out + o, comps[i], l); o += l;
        free(comps[i]);
    }
    free(comps);
    out[o] = '\0';
    if (o == 0) { out[0] = '.'; out[1] = '\0'; }
    return out;
}

/* ── holder construction / projection (UpdateHolder) ─────────────────────── */
static json_t *holder_make(const json_t *spec) {
    json_t *h = json_object();
    json_set(h, "pid", json_number(json_get_num(spec, "pid", 0)));
    json_set(h, "age_seconds", json_number(json_get_num(spec, "age_seconds", 0.0)));
    json_t *reason = json_obj_get(spec, "reason");
    json_set(h, "reason", (reason && reason->type != JSON_NULL) ? json_copy(reason) : json_null());
    json_set(h, "held", json_bool(json_get_bool(spec, "held", false)));
    return h;
}
static json_t *holder_default(void) {
    json_t *h = json_object();
    json_set(h, "pid", json_number(0));
    json_set(h, "age_seconds", json_number(0.0));
    json_set(h, "reason", json_null());
    json_set(h, "held", json_bool(false));
    return h;
}

/* ── marker spec field readers ───────────────────────────────────────────── */
static long long spec_pid(const json_t *m) { return (long long)json_get_num(m, "pid", -1); }
static bool spec_started(const json_t *m, unsigned long long *out) {
    json_t *st = json_obj_get(m, "started_at");
    if (j_is_null(st)) return false;
    return node_to_u64(st, out);
}
static bool spec_ct(const json_t *m, double *out) {
    json_t *ct = json_obj_get(m, "create_time");
    if (j_is_num(ct)) { *out = ct->num_val; return true; }
    return false;
}
/* bytes input: raw_hex (exact) or raw (UTF-8 string). Returns malloc'd bytes. */
static unsigned char *input_bytes(const json_t *in, size_t *len_out) {
    const char *hex = json_get_str(in, "raw_hex", NULL);
    if (hex) {
        size_t n = strlen(hex) / 2;
        unsigned char *out = malloc(n + 1);
        for (size_t i = 0; i < n; i++) {
            char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
            out[i] = (unsigned char)strtoul(b, NULL, 16);
        }
        out[n] = 0; *len_out = n; return out;
    }
    const char *raw = json_get_str(in, "raw", "");
    size_t n = strlen(raw);
    unsigned char *out = malloc(n + 1);
    memcpy(out, raw, n + 1); *len_out = n;
    return out;
}
/* base: first 3 byte-lines with ends (bytes splitlines: \n, \r\n, \r). */
static char *marker_base_hex(const unsigned char *raw, size_t raw_len) {
    size_t i = 0, lines = 0, end = 0;
    while (i < raw_len && lines < 3) {
        unsigned char c = raw[i];
        if (c == '\n') { i++; lines++; end = i; }
        else if (c == '\r') { i++; if (i < raw_len && raw[i] == '\n') i++; lines++; end = i; }
        else i++;
    }
    if (lines < 3) end = raw_len;
    char *hex = malloc(end * 2 + 1);
    static const char digits[] = "0123456789abcdef";
    for (size_t k = 0; k < end; k++) { hex[2 * k] = digits[raw[k] >> 4]; hex[2 * k + 1] = digits[raw[k] & 0xF]; }
    hex[end * 2] = '\0';
    return hex;
}

/* ════════════════════════════════════════════════════════════════════════ */
/* PoP: ul2_git_common_dir @ hermes_cli/update_lock.py:_git_common_dir */
static json_t *ul2_git_common_dir(ul2_io_t *io, json_t *in) {
    const char *root = json_get_str(in, "root", "");
    char *dot = posix_join(root, ".git");
    json_t *err = NULL;
    char *gitdir = NULL;
    /* try: if dot.is_dir() ... elif dot.is_file() ... else return None */
    json_t *a = json_object(); json_set_hdr(a, "path", dot);
    json_t *isdir = ul2_call(io, "path.is_dir", a); json_free(a);
    if (ul2_raised(isdir)) { err = isdir; goto done; }
    bool d = jtruth(isdir); json_free(isdir);
    if (d) {
        gitdir = strdup(dot);
    } else {
        a = json_object(); json_set_hdr(a, "path", dot);
        json_t *isfile = ul2_call(io, "path.is_file", a); json_free(a);
        if (ul2_raised(isfile)) { err = isfile; goto done; }
        bool f = jtruth(isfile); json_free(isfile);
        if (!f) { free(dot); return json_null(); }
        a = json_object(); json_set_hdr(a, "path", dot);
        json_t *txt = ul2_call(io, "path.read_text", a); json_free(a);
        if (ul2_raised(txt)) { err = txt; goto done; }
        const char *t = jstr(txt);
        /* .strip() then startswith("gitdir:") */
        char *stripped = strdup(t);
        /* strip whitespace both ends (Python str.strip White_Space) */
        size_t sn = strlen(stripped), sa = 0, sb = sn;
        while (sa < sb && (unsigned char)stripped[sa] <= 0x20 && strchr(" \t\n\r\v\f\x1c\x1d\x1e\x1f", stripped[sa])) sa++;
        while (sb > sa && strchr(" \t\n\r\v\f\x1c\x1d\x1e\x1f", stripped[sb - 1])) sb--;
        stripped[sb] = '\0';
        char *body = stripped + sa;
        if (strncmp(body, "gitdir:", 7) != 0) { free(stripped); json_free(txt); free(dot); return json_null(); }
        char *target = body + 7;
        /* target.strip() */
        size_t tn = strlen(target), ta = 0, tb = tn;
        while (ta < tb && strchr(" \t\n\r\v\f\x1c\x1d\x1e\x1f", target[ta])) ta++;
        while (tb > ta && strchr(" \t\n\r\v\f\x1c\x1d\x1e\x1f", target[tb - 1])) tb--;
        target[tb] = '\0';
        gitdir = posix_join(root, target + ta);
        free(stripped); json_free(txt);
    }
    /* common = gitdir / "commondir"; if common.is_file(): gitdir /= read_text */
    {
        char *common = posix_join(gitdir, "commondir");
        a = json_object(); json_set_hdr(a, "path", common);
        json_t *cf = ul2_call(io, "path.is_file", a); json_free(a);
        if (ul2_raised(cf)) { free(common); err = cf; goto done; }
        bool cfile = jtruth(cf); json_free(cf);
        if (cfile) {
            a = json_object(); json_set_hdr(a, "path", common);
            json_t *ct = ul2_call(io, "path.read_text", a); json_free(a);
            if (ul2_raised(ct)) { free(common); err = ct; goto done; }
            const char *s = jstr(ct);
            size_t cn = strlen(s), ca = 0, cb = cn;
            while (ca < cb && strchr(" \t\n\r\v\f\x1c\x1d\x1e\x1f", s[ca])) ca++;
            while (cb > ca && strchr(" \t\n\r\v\f\x1c\x1d\x1e\x1f", s[cb - 1])) cb--;
            char *tmp = malloc(cb - ca + 1); memcpy(tmp, s + ca, cb - ca); tmp[cb - ca] = '\0';
            char *ng = posix_join(gitdir, tmp);
            free(tmp); json_free(ct); free(gitdir); gitdir = ng;
        }
        free(common);
    }
done:
    free(dot);
    if (err) {
        free(gitdir);
        if (exc_is_oserror(err)) { json_free(err); return json_null(); }
        return err;
    }
    if (!gitdir) return json_null();
    char *norm = posix_normpath(gitdir);
    free(gitdir);
    json_t *r = json_string(norm);
    free(norm);
    return r;
}

/* PoP: ul2_checkout_lock_path @ hermes_cli/update_lock.py:checkout_lock_path */
static json_t *ul2_checkout_lock_path(ul2_io_t *io, json_t *in) {
    json_t *ir = json_obj_get(in, "install_root");
    char *root = NULL;
    /* root = Path(install_root or _default_install_root()) */
    if (jtruth(ir)) {
        root = strdup(jstr(ir));
    } else {
        json_t *d = ul2_call(io, "_default_install_root", NULL);
        if (ul2_raised(d)) return d;
        root = strdup(jstr(d));
        json_free(d);
    }
    json_t *a = json_object(); json_set_hdr(a, "root", root);
    json_t *common = ul2_call(io, "_git_common_dir", a); json_free(a);
    if (ul2_raised(common)) { free(root); return common; }
    json_t *r;
    if (j_is_null(common)) {
        char *j = posix_join(root, ".hermes-update.lock");
        r = json_string(j); free(j);
    } else {
        char *j = posix_join(jstr(common), "hermes-update.lock");
        r = json_string(j); free(j);
    }
    json_free(common);
    free(root);
    return r;
}

/* ── int(pid) coercion for incarnation_live (ASCII fixture domain) ───────── */
/* Returns 0 on success (value in *out), 1 on TypeError, 2 on ValueError. */
static int py_int_coerce(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return 1;             /* int(None) TypeError */
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return 2;                       /* int(inf/nan) ValueError */
        *out = (long long)d;                              /* trunc toward zero */
        return 0;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        /* int() strips White_Space minus \x1c-\x1f (narrower than str.strip) */
        const char *ws = " \t\n\r\v\f\xa0";
        size_t n = strlen(s), a = 0, b = n;
        while (a < b && strchr(ws, s[a])) a++;
        while (b > a && strchr(ws, s[b - 1])) b--;
        if (a == b) return 2;                             /* empty -> ValueError */
        size_t i = a; bool neg = false;
        if (s[i] == '+' || s[i] == '-') { neg = s[i] == '-'; i++; }
        if (i == b) return 2;
        long long val = 0;
        bool prev_digit = false;
        for (; i < b; i++) {
            char c = s[i];
            if (c >= '0' && c <= '9') { val = val * 10 + (c - '0'); prev_digit = true; }
            else if (c == '_') {
                /* int() allows a single underscore BETWEEN digits only */
                if (!prev_digit) return 2;
                if (i + 1 >= b || s[i + 1] < '0' || s[i + 1] > '9') return 2;
                prev_digit = false;
            } else return 2;
        }
        if (!prev_digit) return 2;
        *out = neg ? -val : val;
        return 0;
    }
    return 1;                                             /* containers TypeError */
}

/* PoP: ul2_incarnation_live @ hermes_cli/update_lock.py:incarnation_live */
static json_t *ul2_incarnation_live(ul2_io_t *io, json_t *in) {
    json_t *pidn = json_obj_get(in, "pid");
    long long pid;
    int rc = py_int_coerce(pidn, &pid);
    if (rc == 1 || rc == 2) return json_bool(false);   /* except (TypeError,ValueError) */
    /* _as_ct(recorded_ct) — claimed sibling, seamed */
    json_t *a = json_object();
    json_t *rec = json_obj_get(in, "recorded_ct");
    json_set(a, "recorded", rec ? json_copy(rec) : json_null());
    json_t *ct = ul2_call(io, "_as_ct", a); json_free(a);
    if (ul2_raised(ct)) return ct;
    /* _real_world() */
    json_t *w = ul2_call(io, "_real_world", NULL);
    if (ul2_raised(w)) { json_free(ct); return w; }
    /* _incarnation(pid, ct, world) — claimed sibling, seamed */
    a = json_object();
    json_set(a, "pid", json_number((double)pid));
    json_set(a, "recorded", ct ? json_copy(ct) : json_null());
    json_set(a, "world", w ? json_copy(w) : json_null());
    json_t *r = ul2_call(io, "_incarnation", a);
    json_free(a); json_free(ct); json_free(w);
    return r;
}

/* PoP: ul2_real_world @ hermes_cli/update_lock.py:_real_world */
static json_t *ul2_real_world(ul2_io_t *io, json_t *in) {
    (void)in;
    json_t *pid = ul2_call(io, "os.getpid", NULL);
    if (ul2_raised(pid)) return pid;
    json_t *ct = ul2_call(io, "_own_create_time", NULL);
    if (ul2_raised(ct)) { json_free(pid); return ct; }
    json_t *now = ul2_call(io, "time.time", NULL);
    if (ul2_raised(now)) { json_free(pid); json_free(ct); return now; }
    json_t *w = json_object();
    json_set(w, "pid", pid ? json_copy(pid) : json_null());
    json_set(w, "ct", ct ? json_copy(ct) : json_null());
    json_set(w, "now", now ? json_copy(now) : json_null());
    json_free(pid); json_free(ct); json_free(now);
    return w;
}

/* PoP: ul2_identity_line @ hermes_cli/update_lock.py:_identity_line */
static json_t *ul2_identity_line(ul2_io_t *io, json_t *in) {
    json_t *a = json_object();
    json_t *pid = json_obj_get(in, "pid");
    json_set(a, "pid", pid ? json_copy(pid) : json_null());
    json_t *ct = ul2_call(io, "process_create_time", a); json_free(a);
    if (ul2_raised(ct)) return ct;
    if (j_is_null(ct)) { json_free(ct); return json_string(""); }
    double v = j_is_num(ct) ? ct->num_val : 0.0;
    json_free(ct);
    char buf[64];
    snprintf(buf, sizeof buf, "ct:%.3f", v);
    return json_string(buf);
}

/* PoP: ul2_is_runtime_host @ hermes_cli/update_lock.py:_is_runtime_host */
static json_t *ul2_is_runtime_host(ul2_io_t *io, json_t *in) {
    json_t *cmd = json_obj_get(in, "cmdline");
    /* line = " ".join(cmdline) */
    size_t cap = 64, len = 0;
    char *line = malloc(cap);
    line[0] = '\0';
    if (cmd && cmd->type == JSON_ARRAY) {
        for (size_t i = 0; i < cmd->c.count; i++) {
            const char *s = jstr(cmd->c.items[i]);
            size_t sl = strlen(s);
            size_t need = len + sl + 2;
            if (need > cap) { while (need > cap) cap *= 2; line = realloc(line, cap); }
            if (i > 0) line[len++] = ' ';
            memcpy(line + len, s, sl); len += sl; line[len] = '\0';
        }
    }
    json_t *a = json_object(); json_set_hdr(a, "line", line);
    json_t *g = ul2_call(io, "looks_like_gateway_command_line", a); json_free(a);
    if (ul2_raised(g)) { free(line); return g; }
    if (jtruth(g)) { json_free(g); free(line); return json_bool(true); }
    json_free(g);
    a = json_object(); json_set_hdr(a, "line", line);
    json_t *h = ul2_call(io, "_hermes_holder_subcommand", a); json_free(a);
    free(line);
    if (ul2_raised(h)) return h;
    const char *s = jstr(h);
    bool in_set = strcmp(s, "serve") == 0 || strcmp(s, "dashboard") == 0;
    json_free(h);
    return json_bool(in_set);
}

/* PoP: ul2_read_bytes @ hermes_cli/update_lock.py:_read_bytes */
static json_t *ul2_read_bytes(ul2_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    json_t *a = json_object(); json_set_hdr(a, "path", path);
    json_t *r = ul2_call(io, "path.read_bytes", a); json_free(a);
    if (ul2_raised(r)) {
        if (exc_is_oserror(r)) { json_free(r); return json_null(); }
        return r;
    }
    return r;
}

/* PoP: ul2_read_marker @ hermes_cli/update_lock.py:_read_marker */
static json_t *ul2_read_marker(ul2_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    json_t *a = json_object(); json_set_hdr(a, "path", path);
    json_t *raw = ul2_call(io, "_read_bytes", a); json_free(a);
    if (ul2_raised(raw)) return raw;
    if (j_is_null(raw)) { json_free(raw); return json_null(); }
    /* if not raw: with suppress(OSError): mtime = path.stat().st_mtime */
    json_t *mtime = json_null();
    if (!jtruth(raw)) {
        a = json_object(); json_set_hdr(a, "path", path);
        json_t *st = ul2_call(io, "path.stat", a); json_free(a);
        if (ul2_raised(st)) {
            if (exc_is_oserror(st)) { json_free(st); }
            else { json_free(raw); json_free(mtime); return st; }
        } else {
            json_t *mt = json_obj_get(st, "st_mtime");
            if (mt) { json_free(mtime); mtime = json_copy(mt); }
            json_free(st);
        }
    }
    a = json_object();
    json_set(a, "raw", json_copy(raw));
    json_set(a, "mtime", mtime);
    json_t *r = ul2_call(io, "_parse_marker", a);
    json_free(a); json_free(raw);
    return r;
}

/* PoP: ul2_tmp_sibling @ hermes_cli/update_lock.py:_tmp_sibling */
static json_t *ul2_tmp_sibling(ul2_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    json_t *pid = ul2_call(io, "os.getpid", NULL);
    if (ul2_raised(pid)) return pid;
    json_t *a = json_object(); json_set(a, "n", json_number(4));
    json_t *tok = ul2_call(io, "secrets.token_hex", a); json_free(a);
    if (ul2_raised(tok)) { json_free(pid); return tok; }
    char *name = posix_name(path);
    long long p = j_is_num(pid) ? (long long)pid->num_val : 0;
    const char *t = jstr(tok);
    size_t n = strlen(name) + strlen(t) + 48;
    char *newname = malloc(n);
    snprintf(newname, n, "%s.%lld.%s.tmp", name, p, t);
    char *full = posix_with_name(path, newname);
    json_t *r = json_string(full);
    free(full); free(newname); free(name);
    json_free(pid); json_free(tok);
    return r;
}

/* PoP: ul2_tmp_writer_gone @ hermes_cli/update_lock.py:_tmp_writer_gone */
static json_t *ul2_tmp_writer_gone(ul2_io_t *io, json_t *in) {
    json_t *own = json_obj_get(in, "owner");
    if (j_is_null(own)) return json_bool(true);
    long long owner = (long long)json_get_num(in, "owner", 0);
    json_t *pid = ul2_call(io, "os.getpid", NULL);
    if (ul2_raised(pid)) return pid;
    long long mypid = j_is_num(pid) ? (long long)pid->num_val : 0;
    json_free(pid);
    if (owner != mypid) {
        json_t *a = json_object(); json_set(a, "pid", json_number((double)owner));
        json_t *alive = ul2_call(io, "_pid_alive", a); json_free(a);
        if (ul2_raised(alive)) return alive;
        bool r = !jtruth(alive);
        json_free(alive);
        return json_bool(r);
    }
    json_t *now = ul2_call(io, "time.time", NULL);
    if (ul2_raised(now)) return now;
    const char *entry = json_get_str(in, "entry", "");
    json_t *a = json_object(); json_set_hdr(a, "path", entry);
    json_t *st = ul2_call(io, "path.stat", a); json_free(a);
    if (ul2_raised(st)) { json_free(now); return st; }
    double mt = json_get_num(st, "st_mtime", 0.0);
    json_free(st);
    double t = j_is_num(now) ? now->num_val : 0.0;
    json_free(now);
    return json_bool((t - mt) > UL2_TMP_STALE);
}

/* PoP: ul2_marker_mutex_path @ hermes_cli/update_lock.py:marker_mutex_path */
static json_t *ul2_marker_mutex_path(ul2_io_t *io, json_t *in) {
    (void)io;
    const char *path = json_get_str(in, "path", "");
    char *name = posix_name(path);
    size_t n = strlen(name) + 8;
    char *newname = malloc(n);
    snprintf(newname, n, "%s.lock", name);
    char *full = posix_with_name(path, newname);
    json_t *r = json_string(full);
    free(full); free(newname); free(name);
    return r;
}

/* ── _INT_LINE fullmatch: [0-9]+ (ASCII) ─────────────────────────────────── */
static bool int_line_fullmatch(const char *s) {
    if (!s || !s[0]) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}

/* PoP: ul2_sweep_dead_tmp_siblings @ hermes_cli/update_lock.py:_sweep_dead_tmp_siblings */
static json_t *ul2_sweep_dead_tmp_siblings(ul2_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    char *name = posix_name(path);
    size_t pn = strlen(name) + 2;
    char *prefix = malloc(pn);
    snprintf(prefix, pn, "%s.", name);
    free(name);
    char *parent = posix_parent(path);
    json_t *a = json_object(); json_set_hdr(a, "path", parent);
    json_t *entries = ul2_call(io, "path.iterdir", a);
    json_free(a); free(parent);
    if (ul2_raised(entries)) {
        free(prefix);
        if (exc_is_oserror(entries)) { json_free(entries); return json_null(); }
        return entries;
    }
    json_t *err = NULL;
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < entries->c.count && !err; i++) {
            const char *entry = jstr(entries->c.items[i]);
            char *ename = posix_name(entry);
            /* owner = name[len(prefix):].split(".",1)[0] */
            size_t enl = strlen(ename), pl = strlen(prefix);
            const char *rest = enl > pl ? ename + pl : "";
            char *owner = strdup(rest);
            char *dotp = strchr(owner, '.');
            if (dotp) *dotp = '\0';
            bool pass = strncmp(ename, prefix, pl) == 0
                && enl >= 4 && strcmp(ename + enl - 4, ".tmp") == 0
                && int_line_fullmatch(owner);
            if (pass) {
                /* inner suppress(OSError): _tmp_writer_gone(_bounded_int(owner), entry) */
                a = json_object();
                json_set_hdr(a, "text", owner);
                json_set(a, "limit", json_number((double)UL2_U32_MAX));
                json_t *bi = ul2_call(io, "_bounded_int", a); json_free(a);
                if (ul2_raised(bi)) {
                    if (exc_is_oserror(bi)) { json_free(bi); }
                    else { err = bi; }
                } else {
                    a = json_object();
                    json_set(a, "owner", bi ? json_copy(bi) : json_null());
                    json_set_hdr(a, "entry", entry);
                    json_t *gone = ul2_call(io, "_tmp_writer_gone", a); json_free(a);
                    json_free(bi);
                    if (ul2_raised(gone)) {
                        if (exc_is_oserror(gone)) { json_free(gone); }
                        else { err = gone; }
                    } else {
                        if (jtruth(gone)) {
                            a = json_object(); json_set_hdr(a, "path", entry);
                            json_t *un = ul2_call(io, "path.unlink", a); json_free(a);
                            if (ul2_raised(un)) {
                                if (exc_is_oserror(un)) { json_free(un); }
                                else { err = un; }
                            } else json_free(un);
                        }
                        json_free(gone);
                    }
                }
            }
            free(owner); free(ename);
        }
    }
    json_free(entries); free(prefix);
    if (err) return err;
    return json_null();
}

/* PoP: ul2_compare_and_swap @ hermes_cli/update_lock.py:_compare_and_swap */
static json_t *ul2_compare_and_swap(ul2_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    const char *expected = json_get_str(in, "expected", "");
    const char *newb = json_get_str(in, "new", "");
    /* with marker_mutex(path): enter */
    json_t *a = json_object(); json_set_hdr(a, "path", path);
    json_t *enter = ul2_call(io, "mutex.enter", a); json_free(a);
    if (ul2_raised(enter)) return enter;
    json_free(enter);
    json_t *body_err = NULL;
    json_t *ret = NULL;
    char *tmp = NULL;
    bool in_try = false;
    /* if _read_bytes(path) != expected: return False */
    a = json_object(); json_set_hdr(a, "path", path);
    json_t *rb = ul2_call(io, "_read_bytes", a); json_free(a);
    if (ul2_raised(rb)) { body_err = rb; goto body_done; }
    if (strcmp(jstr(rb), expected) != 0) { json_free(rb); ret = json_bool(false); goto body_done; }
    json_free(rb);
    /* tmp = _tmp_sibling(path) */
    a = json_object(); json_set_hdr(a, "path", path);
    json_t *ts = ul2_call(io, "_tmp_sibling", a); json_free(a);
    if (ul2_raised(ts)) { body_err = ts; goto body_done; }
    tmp = strdup(jstr(ts)); json_free(ts);
    in_try = true;
    /* try: tmp.write + os.replace ; except OSError -> False */
    a = json_object(); json_set_hdr(a, "path", tmp); json_set_hdr(a, "body", newb);
    json_t *wr = ul2_call(io, "tmp.write", a); json_free(a);
    if (ul2_raised(wr)) {
        if (exc_is_oserror(wr)) { json_free(wr); ret = json_bool(false); goto finally; }
        body_err = wr; goto finally;
    }
    json_free(wr);
    a = json_object(); json_set_hdr(a, "tmp", tmp); json_set_hdr(a, "path", path);
    json_t *rep = ul2_call(io, "os.replace", a); json_free(a);
    if (ul2_raised(rep)) {
        if (exc_is_oserror(rep)) { json_free(rep); ret = json_bool(false); goto finally; }
        body_err = rep; goto finally;
    }
    json_free(rep);
    ret = json_bool(true);
finally:
    /* finally: with suppress(OSError): tmp.unlink() */
    if (in_try && tmp) {
        a = json_object(); json_set_hdr(a, "path", tmp);
        json_t *un = ul2_call(io, "path.unlink", a); json_free(a);
        if (ul2_raised(un)) {
            if (exc_is_oserror(un)) { json_free(un); }
            else { if (body_err) json_free(body_err); if (ret) json_free(ret); body_err = un; ret = NULL; }
        } else json_free(un);
    }
body_done:
    free(tmp);
    /* mutex.exit on every body exit */
    a = json_object(); json_set_hdr(a, "path", path);
    json_t *exit_r = ul2_call(io, "mutex.exit", a); json_free(a);
    if (ul2_raised(exit_r)) {
        if (body_err) json_free(body_err);
        if (ret) json_free(ret);
        return exit_r;
    }
    json_free(exit_r);
    if (body_err) { if (ret) json_free(ret); return body_err; }
    return ret ? ret : json_bool(false);
}

/* PoP: ul2_legacy_profile_claims @ hermes_cli/update_lock.py:legacy_profile_claims */
static json_t *ul2_legacy_profile_claims(ul2_io_t *io, json_t *in) {
    json_t *rootn = json_obj_get(in, "root");
    char *root = NULL;
    if (j_is_null(rootn)) {
        json_t *ump = ul2_call(io, "update_marker_path", NULL);
        if (ul2_raised(ump)) return ump;
        root = posix_parent(jstr(ump));
        json_free(ump);
    } else {
        root = strdup(jstr(rootn));
    }
    char *profiles = posix_join(root, "profiles");
    free(root);
    json_t *a = json_object(); json_set_hdr(a, "path", profiles);
    json_t *entries = ul2_call(io, "path.iterdir", a); json_free(a);
    free(profiles);
    json_t *claims = json_array();
    if (ul2_raised(entries)) {
        if (exc_is_oserror(entries)) { json_free(entries); return claims; }
        json_free(claims); return entries;
    }
    json_t *err = NULL;
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < entries->c.count && !err; i++) {
            const char *profile = jstr(entries->c.items[i]);
            char *mp = posix_join(profile, ".hermes-update-in-progress");
            a = json_object(); json_set_hdr(a, "path", mp);
            json_t *marker = ul2_call(io, "_read_marker", a); json_free(a);
            free(mp);
            if (ul2_raised(marker)) {
                if (exc_is_oserror(marker)) { json_free(marker); break; }
                err = marker; break;
            }
            if (j_is_null(marker)) { json_free(marker); continue; }
            if (json_get_bool(marker, "in_flight", false)) { json_free(marker); continue; }
            /* live = marker.live_pid() */
            a = json_object(); json_set(a, "marker", json_copy(marker));
            json_t *live = ul2_call(io, "marker.live_pid", a); json_free(a);
            if (ul2_raised(live)) {
                json_free(marker);
                if (exc_is_oserror(live)) { json_free(live); break; }
                err = live; break;
            }
            if (jtruth(live)) {
                /* age = marker.age() */
                a = json_object(); json_set(a, "marker", json_copy(marker));
                json_t *age = ul2_call(io, "marker.age", a); json_free(a);
                if (ul2_raised(age)) {
                    json_free(marker); json_free(live);
                    if (exc_is_oserror(age)) { json_free(age); break; }
                    err = age; break;
                }
                json_t *h = holder_default();
                json_set(h, "pid", json_copy(live));
                json_set(h, "age_seconds", age ? json_copy(age) : json_number(0.0));
                json_append(claims, h);
                json_free(age);
            }
            json_free(live); json_free(marker);
        }
    }
    json_free(entries);
    if (err) { json_free(claims); return err; }
    return claims;
}

/* PoP: ul2_lock_holder @ hermes_cli/update_lock.py:_lock_holder */
static json_t *ul2_lock_holder(ul2_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    json_t *a = json_object(); json_set_hdr(a, "path", path);
    json_t *raw = ul2_call(io, "_read_bytes", a); json_free(a);
    if (ul2_raised(raw)) return raw;
    /* raw or b"" */
    a = json_object();
    if (jtruth(raw)) json_set(a, "raw", json_copy(raw));
    else json_set_hdr(a, "raw", "");
    json_set(a, "mtime", json_null());
    json_t *parsed = ul2_call(io, "_parse_marker", a); json_free(a);
    json_free(raw);
    if (ul2_raised(parsed)) return parsed;
    /* age = parsed.age() if parsed.started_at else 0.0 */
    unsigned long long st;
    bool has_st = spec_started(parsed, &st);
    json_t *age = json_number(0.0);
    if (has_st && st != 0) {
        json_free(age);
        a = json_object(); json_set(a, "marker", json_copy(parsed));
        age = ul2_call(io, "marker.age", a); json_free(a);
        if (ul2_raised(age)) { json_free(parsed); return age; }
    }
    long long pid = spec_pid(parsed);
    json_t *h = holder_default();
    json_set(h, "age_seconds", age ? json_copy(age) : json_number(0.0));
    json_free(age);
    /* if parsed.pid > 0 and incarnation_live(...) is False: held holder */
    if (pid > 0) {
        double ct; bool has_ct = spec_ct(parsed, &ct);
        a = json_object();
        json_set(a, "pid", json_number((double)pid));
        json_set(a, "recorded_ct", has_ct ? json_number(ct) : json_null());
        json_t *il = ul2_call(io, "incarnation_live", a); json_free(a);
        if (ul2_raised(il)) { json_free(parsed); json_free(h); return il; }
        bool is_false = il && il->type == JSON_BOOL && !il->bool_val;
        json_free(il);
        if (is_false) {
            json_set(h, "pid", json_number(0));
            json_set(h, "held", json_bool(true));
            json_free(parsed);
            return h;
        }
    }
    json_set(h, "pid", json_number((double)(pid > 0 ? pid : 0)));
    json_free(parsed);
    return h;
}

/* PoP: ul2_lock_with_contention_wait @ hermes_cli/update_lock.py:_lock_with_contention_wait */
static json_t *ul2_lock_with_contention_wait(ul2_io_t *io, json_t *in) {
    double fd = json_get_num(in, "fd", 0);
    const char *path = json_get_str(in, "path", "");
    json_t *a = json_object(); json_set(a, "fd", json_number(fd));
    json_t *tl = ul2_call(io, "_try_lock", a); json_free(a);
    if (ul2_raised(tl)) return tl;
    if (jtruth(tl)) { json_free(tl); return json_bool(true); }
    json_free(tl);
    a = json_object(); json_set_hdr(a, "path", path);
    json_t *anc = ul2_call(io, "_held_by_our_windows_ancestor", a); json_free(a);
    if (ul2_raised(anc)) return anc;
    if (jtruth(anc)) { json_free(anc); return json_bool(false); }
    json_free(anc);
    json_t *m0 = ul2_call(io, "time.monotonic", NULL);
    if (ul2_raised(m0)) return m0;
    double deadline = (j_is_num(m0) ? m0->num_val : 0.0) + UL2_CONTENTION_WAIT;
    json_free(m0);
    for (;;) {
        json_t *m = ul2_call(io, "time.monotonic", NULL);
        if (ul2_raised(m)) return m;
        double now = j_is_num(m) ? m->num_val : 0.0;
        json_free(m);
        if (!(now < deadline)) break;
        a = json_object(); json_set(a, "seconds", json_number(0.02));
        json_t *sl = ul2_call(io, "time.sleep", a); json_free(a);
        if (ul2_raised(sl)) return sl;
        json_free(sl);
        a = json_object(); json_set(a, "fd", json_number(fd));
        json_t *t2 = ul2_call(io, "_try_lock", a); json_free(a);
        if (ul2_raised(t2)) return t2;
        if (jtruth(t2)) { json_free(t2); return json_bool(true); }
        json_free(t2);
    }
    return json_bool(false);
}

/* PoP: ul2_held_by_our_windows_ancestor @ hermes_cli/update_lock.py:_held_by_our_windows_ancestor */
static json_t *ul2_held_by_our_windows_ancestor(ul2_io_t *io, json_t *in) {
    const char *platform = json_get_str(in, "platform", "linux");
    if (strcmp(platform, "win32") != 0) return json_bool(false);
    const char *path = json_get_str(in, "path", "");
    json_t *a = json_object(); json_set_hdr(a, "path", path);
    json_t *raw = ul2_call(io, "_read_bytes", a); json_free(a);
    if (ul2_raised(raw)) return raw;
    a = json_object();
    if (jtruth(raw)) json_set(a, "raw", json_copy(raw));
    else json_set_hdr(a, "raw", "");
    json_set(a, "mtime", json_null());
    json_t *rec = ul2_call(io, "_parse_marker", a); json_free(a);
    json_free(raw);
    if (ul2_raised(rec)) return rec;
    unsigned long long st;
    bool has_st = spec_started(rec, &st);
    if (!has_st) { json_free(rec); return json_bool(false); }
    long long rpid = spec_pid(rec);
    json_t *pid = ul2_call(io, "os.getpid", NULL);
    if (ul2_raised(pid)) { json_free(rec); return pid; }
    long long mypid = j_is_num(pid) ? (long long)pid->num_val : 0;
    json_free(pid);
    if (rpid == mypid) { json_free(rec); return json_bool(false); }
    double ct; bool has_ct = spec_ct(rec, &ct);
    a = json_object();
    json_set(a, "pid", json_number((double)rpid));
    json_set(a, "recorded_ct", has_ct ? json_number(ct) : json_null());
    json_t *il = ul2_call(io, "incarnation_live", a); json_free(a);
    if (ul2_raised(il)) { json_free(rec); return il; }
    bool is_true = il && il->type == JSON_BOOL && il->bool_val;
    json_free(il);
    if (!is_true) { json_free(rec); return json_bool(false); }
    a = json_object(); json_set(a, "pid", json_number((double)rpid));
    json_t *anc = ul2_call(io, "_is_ancestor_pid", a); json_free(a);
    json_free(rec);
    return anc;
}

/* PoP: ul2_release_checkout @ hermes_cli/update_lock.py:_release_checkout */
static json_t *ul2_release_checkout(ul2_io_t *io, json_t *in) {
    json_t *held = json_obj_get(in, "held");
    if (j_is_null(held)) return json_null();
    double depth = json_get_num(held, "depth", 0) - 1;
    json_set(held, "depth", json_number(depth));
    if (depth > 0) return json_null();
    /* held, _HELD = _HELD, None  -> snapshot then clear input held */
    json_t *snap = json_copy(held);
    /* mark the input held as None by replacing with null sentinel: the harness
     * echoes pin.held; we set it to JSON null to model _HELD = None. */
    json_t *lease = json_obj_get(snap, "lease");
    json_t *err = NULL;
    if (jtruth(lease) && lease->type == JSON_ARRAY && lease->c.count >= 2) {
        double lfd = arr_num(lease, 0);
        double off = arr_num(lease, 1);
        json_t *a = json_object();
        json_set(a, "fd", json_number(lfd));
        json_set(a, "offset", json_number(off));
        json_set(a, "count", json_number(1));
        json_t *un = ul2_call(io, "_unlock", a); json_free(a);
        if (ul2_raised(un)) { err = un; goto done; }
        json_free(un);
        a = json_object(); json_set(a, "fd", json_number(lfd));
        json_t *cl = ul2_call(io, "os.close", a); json_free(a);
        if (ul2_raised(cl)) {
            if (exc_is_oserror(cl)) { json_free(cl); }
            else { err = cl; goto done; }
        } else json_free(cl);
    }
    if (json_get_bool(snap, "owned", false)) {
        double fd = json_get_num(snap, "fd", 0);
        json_t *a = json_object();
        json_set(a, "fd", json_number(fd));
        json_set(a, "offset", json_number((double)UL2_WINDOWS_LOCK_OFFSET));
        json_set(a, "count", json_number(1));
        json_t *un = ul2_call(io, "_unlock", a); json_free(a);
        if (ul2_raised(un)) { err = un; goto done; }
        json_free(un);
        a = json_object(); json_set(a, "fd", json_number(fd));
        json_t *cl = ul2_call(io, "os.close", a); json_free(a);
        if (ul2_raised(cl)) {
            if (exc_is_oserror(cl)) { json_free(cl); }
            else { err = cl; goto done; }
        } else json_free(cl);
    }
done:
    json_free(snap);
    /* _HELD = None regardless (the assignment happened before the lease/owned work) */
    json_obj_del(in, "held");
    json_set(in, "held", json_null());
    if (err) return err;
    return json_null();
}

/* PoP: ul2_checkout_lock_fds @ hermes_cli/update_lock.py:checkout_lock_fds */
static json_t *ul2_checkout_lock_fds(ul2_io_t *io, json_t *in) {
    const char *platform = json_get_str(in, "platform", "linux");
    if (strcmp(platform, "win32") == 0) return json_array();
    json_t *held = json_obj_get(in, "held");
    if (!j_is_null(held)) {
        json_t *fd = json_obj_get(held, "fd");
        json_t *r = json_array();
        if (!j_is_null(fd)) json_append(r, json_copy(fd));
        return r;
    }
    json_t *ir = json_obj_get(in, "install_root");
    json_t *a = json_object();
    json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *path = ul2_call(io, "checkout_lock_path", a); json_free(a);
    if (ul2_raised(path)) return path;
    a = json_object(); json_set_hdr(a, "path", jstr(path));
    json_t *fd = ul2_call(io, "_inherited_lock_fd", a); json_free(a);
    json_free(path);
    if (ul2_raised(fd)) return fd;
    json_t *r = json_array();
    if (!j_is_null(fd)) json_append(r, json_copy(fd));
    json_free(fd);
    return r;
}

/* PoP: ul2_custody_spawn_kwargs @ hermes_cli/update_lock.py:custody_spawn_kwargs */
static json_t *ul2_custody_spawn_kwargs(ul2_io_t *io, json_t *in) {
    (void)io;
    const char *platform = json_get_str(in, "platform", "linux");
    json_t *held = json_obj_get(in, "held");
    if (strcmp(platform, "win32") == 0 || j_is_null(held)) return json_object();
    json_t *fd = json_obj_get(held, "fd");
    if (j_is_null(fd)) return json_object();
    json_t *r = json_object();
    json_t *arr = json_array();
    json_append(arr, json_copy(fd));
    json_set(r, "pass_fds", arr);
    return r;
}

/* PoP: ul2_holds_checkout_lock @ hermes_cli/update_lock.py:holds_checkout_lock */
static json_t *ul2_holds_checkout_lock(ul2_io_t *io, json_t *in) {
    json_t *held = json_obj_get(in, "held");
    if (j_is_null(held)) return json_bool(false);
    json_t *ir = json_obj_get(in, "install_root");
    /* left: os.path.realpath(_HELD["path"]) */
    json_t *a = json_object(); json_set_hdr(a, "path", json_get_str(held, "path", ""));
    json_t *lp = ul2_call(io, "os.path.realpath", a); json_free(a);
    if (ul2_raised(lp)) return lp;
    /* right: checkout_lock_path(install_root) then realpath */
    a = json_object(); json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *cp = ul2_call(io, "checkout_lock_path", a); json_free(a);
    if (ul2_raised(cp)) { json_free(lp); return cp; }
    a = json_object(); json_set_hdr(a, "path", jstr(cp));
    json_t *rp = ul2_call(io, "os.path.realpath", a); json_free(a);
    json_free(cp);
    if (ul2_raised(rp)) { json_free(lp); return rp; }
    bool eq = strcmp(jstr(lp), jstr(rp)) == 0;
    json_free(lp); json_free(rp);
    return json_bool(eq);
}

/* PoP: ul2_update_in_progress @ hermes_cli/update_lock.py:update_in_progress */
static json_t *ul2_update_in_progress(ul2_io_t *io, json_t *in) {
    json_t *ir = json_obj_get(in, "install_root");
    json_t *a = json_object();
    json_set(a, "path", json_null());
    json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *rlu = ul2_call(io, "read_live_update", a); json_free(a);
    if (ul2_raised(rlu)) return rlu;
    if (!j_is_null(rlu)) { json_free(rlu); return json_bool(true); }
    json_free(rlu);
    a = json_object(); json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *held = ul2_call(io, "checkout_lock_held", a); json_free(a);
    if (ul2_raised(held)) return held;
    bool r = jtruth(held);
    json_free(held);
    return json_bool(r);
}

/* PoP: ul2_checkout_lock_held @ hermes_cli/update_lock.py:checkout_lock_held */
static json_t *ul2_checkout_lock_held(ul2_io_t *io, json_t *in) {
    json_t *ir = json_obj_get(in, "install_root");
    json_t *a = json_object(); json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *hcl = ul2_call(io, "holds_checkout_lock", a); json_free(a);
    if (ul2_raised(hcl)) return hcl;
    if (jtruth(hcl)) { json_free(hcl); return json_bool(true); }
    json_free(hcl);
    a = json_object(); json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *path = ul2_call(io, "checkout_lock_path", a); json_free(a);
    if (ul2_raised(path)) return path;
    /* fd = os.open(path, O_RDONLY | O_BINARY) */
    a = json_object();
    json_set_hdr(a, "path", jstr(path));
    json_set(a, "flags", json_number((double)(UL2_O_RDONLY | UL2_O_BINARY)));
    json_t *fd = ul2_call(io, "os.open", a); json_free(a);
    json_free(path);
    if (ul2_raised(fd)) {
        if (exc_is_fnf(fd)) { json_free(fd); return json_bool(false); }
        if (exc_is_oserror(fd)) { json_free(fd); return json_bool(true); }
        return fd;
    }
    double fdi = j_is_num(fd) ? fd->num_val : 0;
    json_free(fd);
    /* try: return not _try_shared_lock(fd) ; except OSError: return False ;
     * finally: os.close(fd) */
    json_t *pending = NULL;
    json_t *ret = NULL;
    a = json_object(); json_set(a, "fd", json_number(fdi));
    json_t *tsl = ul2_call(io, "_try_shared_lock", a); json_free(a);
    if (ul2_raised(tsl)) {
        if (exc_is_oserror(tsl)) { json_free(tsl); ret = json_bool(false); }
        else pending = tsl;
    } else {
        ret = json_bool(!jtruth(tsl));
        json_free(tsl);
    }
    /* finally: os.close(fd) — a raise here overrides */
    a = json_object(); json_set(a, "fd", json_number(fdi));
    json_t *cl = ul2_call(io, "os.close", a); json_free(a);
    if (ul2_raised(cl)) {
        if (pending) json_free(pending);
        if (ret) json_free(ret);
        return cl;
    }
    json_free(cl);
    if (pending) { if (ret) json_free(ret); return pending; }
    return ret;
}

/* PoP: ul2_marker_unwritable @ hermes_cli/update_lock.py:_marker_unwritable */
static json_t *ul2_marker_unwritable(ul2_io_t *io, json_t *in) {
    (void)io;
    const char *path = json_get_str(in, "path", "");
    json_t *exc = json_obj_get(in, "exc");
    char *es = exc_str(exc);
    size_t n = strlen(path) + strlen(es) + 200;
    char *reason = malloc(n);
    snprintf(reason, n, "%s is not writable (%s); the update records itself there so the Desktop app, "
             "gateways, installers and other updaters wait for it", path, es);
    free(es);
    json_t *r = json_string(reason);
    free(reason);
    return r;
}

/* PoP: ul2_release_marker @ hermes_cli/update_lock.py:_release_marker */
static json_t *ul2_release_marker(ul2_io_t *io, json_t *in) {
    const char *path = json_get_str(in, "path", "");
    json_t *a = json_object(); json_set_hdr(a, "path", path);
    json_t *enter = ul2_call(io, "mutex.enter", a); json_free(a);
    if (ul2_raised(enter)) return enter;
    json_free(enter);
    json_t *body_err = NULL;
    a = json_object(); json_set_hdr(a, "path", path);
    json_t *raw = ul2_call(io, "_read_bytes", a); json_free(a);
    if (ul2_raised(raw)) { body_err = raw; goto exit; }
    if (j_is_null(raw)) { json_free(raw); goto exit; }
    a = json_object(); json_set(a, "raw", json_copy(raw));
    json_t *dec = ul2_call(io, "_release_decision", a); json_free(a);
    if (ul2_raised(dec)) { json_free(raw); body_err = dec; goto exit; }
    const char *action = json_get_str(dec, "action", "");
    if (strcmp(action, "delete") == 0) {
        a = json_object(); json_set_hdr(a, "path", path);
        json_t *un = ul2_call(io, "path.unlink", a); json_free(a);
        if (ul2_raised(un)) {
            if (exc_is_fnf(un)) json_free(un);
            else body_err = un;
        } else json_free(un);
    } else if (strcmp(action, "rewrite") == 0) {
        json_t *newn = json_obj_get(dec, "new");
        a = json_object();
        json_set_hdr(a, "path", path);
        json_set(a, "expected", json_copy(raw));
        json_set_hdr(a, "new", jstr(newn));
        json_t *cas = ul2_call(io, "_compare_and_swap", a); json_free(a);
        if (ul2_raised(cas)) body_err = cas;
        else json_free(cas);
    }
    json_free(dec); json_free(raw);
exit:
    a = json_object(); json_set_hdr(a, "path", path);
    json_t *exit_r = ul2_call(io, "mutex.exit", a); json_free(a);
    if (ul2_raised(exit_r)) { if (body_err) json_free(body_err); return exit_r; }
    json_free(exit_r);
    if (body_err) return body_err;
    return json_null();
}

/* ── UpdateLock methods (self model injected as in.self) ─────────────────── */

/* PoP: ul2_acquire_checkout @ hermes_cli/update_lock.py:UpdateLock.acquire_checkout */
static json_t *ul2_acquire_checkout(ul2_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    if (json_get_bool(self, "_checkout", false)) return json_bool(true);
    json_t *ir = json_obj_get(in, "install_root");
    json_t *a = json_object(); json_set_hdr(a, "install_root", jstr(ir));
    json_t *refused = ul2_call(io, "_acquire_checkout", a); json_free(a);
    if (ul2_raised(refused)) return refused;
    if (!j_is_null(refused)) {
        json_set(self, "holder", holder_make(refused));
        json_free(refused);
        return json_bool(false);
    }
    json_free(refused);
    json_set(self, "_checkout", json_bool(true));
    return json_bool(true);
}

/* PoP: ul2_checkout_held_elsewhere @ hermes_cli/update_lock.py:UpdateLock._checkout_held_elsewhere */
static json_t *ul2_checkout_held_elsewhere(ul2_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    if (json_get_bool(self, "_checkout", false)) return json_bool(false);
    json_t *ir = json_obj_get(self, "install_root");
    json_t *a = json_object(); json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *path = ul2_call(io, "checkout_lock_path", a); json_free(a);
    if (ul2_raised(path)) return path;
    json_t *held = json_obj_get(in, "held");
    if (!j_is_null(held) && strcmp(json_get_str(held, "path", ""), jstr(path)) == 0) {
        json_free(path); return json_bool(false);
    }
    a = json_object(); json_set(a, "install_root", ir ? json_copy(ir) : json_null());
    json_t *clh = ul2_call(io, "checkout_lock_held", a); json_free(a);
    if (ul2_raised(clh)) { json_free(path); return clh; }
    if (!jtruth(clh)) { json_free(clh); json_free(path); return json_bool(false); }
    json_free(clh);
    a = json_object(); json_set_hdr(a, "path", jstr(path));
    json_t *inh = ul2_call(io, "_inherited_lock_fd", a); json_free(a);
    json_free(path);
    if (ul2_raised(inh)) return inh;
    bool r = j_is_null(inh);
    json_free(inh);
    return json_bool(r);
}

/* PoP: ul2_is_partner @ hermes_cli/update_lock.py:UpdateLock._is_partner */
static json_t *ul2_is_partner(ul2_io_t *io, json_t *in) {
    long long pid = (long long)json_get_num(in, "pid", 0);
    json_t *pidr = ul2_call(io, "os.getpid", NULL);
    if (ul2_raised(pidr)) return pidr;
    long long mypid = j_is_num(pidr) ? (long long)pidr->num_val : 0;
    json_free(pidr);
    if (pid == mypid) return json_bool(true);
    if (pid == 0) return json_number(0);   /* (pid and ...) -> pid when falsy */
    json_t *ho = ul2_call(io, "_handoff_pid", NULL);
    if (ul2_raised(ho)) return ho;
    long long handoff = j_is_num(ho) ? (long long)ho->num_val : -1;
    bool ho_present = !j_is_null(ho);
    json_free(ho);
    if (ho_present && pid == handoff) return json_bool(true);
    json_t *a = json_object(); json_set(a, "pid", json_number((double)pid));
    json_t *anc = ul2_call(io, "_is_ancestor_pid", a); json_free(a);
    return anc;
}

/* PoP: ul2_adopt_or_refuse @ hermes_cli/update_lock.py:UpdateLock._adopt_or_refuse */
static json_t *ul2_adopt_or_refuse(ul2_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    json_t *existing = json_obj_get(in, "marker");
    const char *path = json_get_str(self, "path", "");
    /* partners = _live_partners(existing) */
    json_t *a = json_object(); json_set(a, "marker", json_copy(existing));
    json_t *partners = ul2_call(io, "_live_partners", a); json_free(a);
    if (ul2_raised(partners)) return partners;
    /* any(self._is_partner(p) and (p==os.getpid() or not _runtime_host_below(p))) */
    bool any_match = false;
    json_t *err = NULL;
    if (partners && partners->type == JSON_ARRAY) {
        for (size_t i = 0; i < partners->c.count && !any_match && !err; i++) {
            double p = arr_num(partners, i);
            a = json_object(); json_set(a, "pid", json_number(p));
            json_t *ip = ul2_call(io, "self._is_partner", a); json_free(a);
            if (ul2_raised(ip)) { err = ip; break; }
            if (!jtruth(ip)) { json_free(ip); continue; }
            json_free(ip);
            json_t *pidr = ul2_call(io, "os.getpid", NULL);
            if (ul2_raised(pidr)) { err = pidr; break; }
            long long mypid = j_is_num(pidr) ? (long long)pidr->num_val : 0;
            json_free(pidr);
            if ((long long)p == mypid) { any_match = true; break; }
            a = json_object(); json_set(a, "pid", json_number(p));
            json_t *rhb = ul2_call(io, "_runtime_host_below", a); json_free(a);
            if (ul2_raised(rhb)) { err = rhb; break; }
            if (!jtruth(rhb)) any_match = true;
            json_free(rhb);
        }
    }
    if (err) { json_free(partners); return err; }
    if (!any_match) {
        /* self.holder = UpdateHolder(pid=partners[0], age=existing.age() if started_at else 0) */
        double p0 = arr_num(partners, 0);
        unsigned long long st;
        bool has_st = spec_started(existing, &st);
        json_t *age = json_number(0.0);
        if (has_st && st != 0) {
            json_free(age);
            a = json_object(); json_set(a, "marker", json_copy(existing));
            age = ul2_call(io, "marker.age", a); json_free(a);
            if (ul2_raised(age)) { json_free(partners); return age; }
        }
        json_t *h = holder_default();
        json_set(h, "pid", json_number(p0));
        json_set(h, "age_seconds", age ? json_copy(age) : json_number(0.0));
        json_free(age);
        json_set(self, "holder", h);
        json_free(partners);
        return json_bool(false);
    }
    /* own = _identity_line() */
    json_t *own = ul2_call(io, "_identity_line", NULL);
    if (ul2_raised(own)) { json_free(partners); return own; }
    /* os.getpid() not in partners and delegate_pid not in partners and ct is not None and own */
    json_t *pidr = ul2_call(io, "os.getpid", NULL);
    if (ul2_raised(pidr)) { json_free(partners); json_free(own); return pidr; }
    long long mypid = j_is_num(pidr) ? (long long)pidr->num_val : 0;
    json_free(pidr);
    bool pid_in = false, del_in = false;
    for (size_t i = 0; i < json_len(partners); i++) {
        double p = arr_num(partners, i);
        if ((long long)p == mypid) pid_in = true;
    }
    json_t *dp = json_obj_get(existing, "delegate_pid");
    if (!j_is_null(dp)) {
        double d = dp->num_val;
        for (size_t i = 0; i < json_len(partners); i++)
            if (arr_num(partners, i) == d) del_in = true;
    }
    double ct; bool has_ct = spec_ct(existing, &ct);
    bool own_truthy = jtruth(own);
    if (!pid_in && !del_in && has_ct && own_truthy) {
        /* base = existing.base if endswith \n else base + \n */
        size_t raw_len = 0;
        unsigned char *raw = input_bytes(existing, &raw_len);
        char *basehex = marker_base_hex(raw, raw_len);
        size_t bl = strlen(basehex) / 2;
        bool ends_nl = bl > 0 && raw[bl - 1] == '\n';
        /* delegated = base + f"delegate:{os.getpid()} {own}\n" + run lines.
         * The f-string re-reads os.getpid() (a fresh seam call). */
        json_t *pidr2 = ul2_call(io, "os.getpid", NULL);
        if (ul2_raised(pidr2)) { free(raw); free(basehex); json_free(partners); json_free(own); return pidr2; }
        long long dpid = j_is_num(pidr2) ? (long long)pidr2->num_val : 0;
        json_free(pidr2);
        const char *owns = jstr(own);
        size_t cap = bl + 64 + strlen(owns);
        json_t *runs = json_obj_get(existing, "runs");
        if (runs && runs->type == JSON_ARRAY)
            for (size_t i = 0; i < runs->c.count; i++) cap += strlen(jstr(runs->c.items[i])) + 8;
        char *deleg = malloc(cap);
        size_t o = 0;
        memcpy(deleg, raw, bl); o = bl;
        if (!ends_nl) deleg[o++] = '\n';
        o += (size_t)snprintf(deleg + o, cap - o, "delegate:%lld %s\n", dpid, owns);
        if (runs && runs->type == JSON_ARRAY)
            for (size_t i = 0; i < runs->c.count; i++)
                o += (size_t)snprintf(deleg + o, cap - o, "run:%s\n", jstr(runs->c.items[i]));
        deleg[o] = '\0';
        a = json_object();
        json_set_hdr(a, "path", path);
        json_set_hdr(a, "expected", (const char *)raw);
        json_set_hdr(a, "new", deleg);
        json_t *cas = ul2_call(io, "_compare_and_swap", a); json_free(a);
        free(deleg); free(basehex); free(raw);
        if (ul2_raised(cas)) { json_free(partners); json_free(own); return cas; }
        json_free(cas);
    }
    json_free(partners); json_free(own);
    return json_bool(true);
}

/* PoP: ul2_drop_checkout @ hermes_cli/update_lock.py:UpdateLock._drop_checkout */
static json_t *ul2_drop_checkout(ul2_io_t *io, json_t *in) {
    json_t *self = json_obj_get(in, "self");
    if (json_get_bool(self, "_checkout", false)) {
        json_set(self, "_checkout", json_bool(false));
        json_t *r = ul2_call(io, "_release_checkout", NULL);
        if (ul2_raised(r)) return r;
        json_free(r);
    }
    return json_null();
}
