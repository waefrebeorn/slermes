/*
 * docker_env_flow.c — container-lifecycle FLOW cores of
 * tools/environments/docker.py (Docker execution environment).
 *
 * Ports the DECISION sequences (call order, branch conditions, early returns,
 * exception -> fallback / suppress / re-raise paths, mutation order) of the
 * live module: the attach/reuse decision chain, image pull gates, docker-run
 * spawn ordering + orphan cleanup, mount/env resolution branches, resource
 * gates and the probe/query error paths. All live I/O (docker CLI captures,
 * subprocess.run, uuid, os.path probes, makedirs, sandbox-dir discovery,
 * foreign module helpers and logger emission) enters through the universal
 * seam below; the cores perform no I/O.
 *
 * Sequences are observable through a def_trace_t: the harness wires the seam
 * to a recording stub that appends one JSON event per call (arguments +
 * scripted result) in call order; tests/oracle_docker_env_flow.py drives the
 * LIVE AST-extracted Python with recording monkeypatch stubs fed the SAME
 * scripts and compares ordered trace + ret + propagated exception + mutated
 * self state.
 *
 * ── Models ──────────────────────────────────────────────────────────────────
 * self:        JSON object of the DockerEnvironment data fields (no methods).
 *              Mutated in place (json_set keeps an existing key's position).
 *              Python None fields are JSON null nodes (json_set with a NULL
 *              value is a no-op in libjson, so None writes use json_null()).
 *              Fixtures supply every field the live body reads; a missing
 *              field reads as None here where live would AttributeError.
 * run_capture result: {"returncode": <int>, "stdout": <str>, "stderr": <str>}
 *              (subprocess.CompletedProcess projection; the oracle stub
 *              returns a real CompletedProcess and projects it back).
 * _labels:     JSON object; insertion order is semantic (_run_command
 *              iterates .items() in order).
 * _sandbox_mounts: [[host_dir, container_path], ...] (list of 2-tuples).
 *
 * ── Universal seam ──────────────────────────────────────────────────────────
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * name identifies the live call (table below); args is a BORROWED object;
 * the result is a NEW json_t the caller frees. NULL == Python None. A
 * scripted exception is {"__raise__": "<Kind>", "msg": ...}; def_raise()
 * detects it. The harness records {ev:name, args:<copy>, ret:<script>} per
 * call; a scripted JSON null returns a NULL pointer (NOT a json_null node).
 * def_call_raw takes OWNERSHIP of args (frees them); def_call additionally
 * parks a raise spec into *exc and returns NULL.
 *
 * Script semantics (harness + oracle identical): io[name] is a single value
 * reused for every call, OR a bare array consumed one element per call (last
 * element repeats when exhausted). A constant LIST result is wrapped as
 * {"__script__": true, "ret": [...]}.
 *
 * None-valued argument keys are OMITTED on both sides (drop_none). Logger
 * args are recorded VERBATIM (format specifiers apply only at emission, never
 * to the recorded args); exception args are normalized on BOTH sides to
 * {"__exc__": "<Kind>", "msg": str(e)}.
 *
 * Exception handlers are transcribed exactly; BaseException-only kinds
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError /
 * BaseException) pass through every catch site. OSError-family and
 * SubprocessError-family matching cover the subclasses the fixtures script.
 *
 * Seam name -> live call (args keys; ret):
 *   run_capture        remote_common.run_capture {argv,timeout,check,env?};
 *                      CompletedProcess projection (may raise)
 *   subprocess_run     subprocess.run {argv,capture_output,timeout,stdin,
 *                      check}; null (may raise). stdin is DEVNULL == -3.
 *   client_env_with    remote_common.client_env_with (via the live
 *                      _docker_client_env delegation) {values?}; dict|null
 *   uuid4_hex          uuid.uuid4().hex {}; str (full 32-hex; core slices [:8])
 *   log_debug / log_info / log_warning / log_error
 *                      logger.<level> {msg,args:[...]}; null (may raise)
 *   isdir              os.path.isdir {path}; bool
 *   makedirs           os.makedirs {path,exist_ok}; null (may raise)
 *   expanduser         os.path.expanduser {path}; str
 *   abspath            os.path.abspath {path}; str
 *   is_windows_drive_path  terminal_tool_config._is_windows_drive_path
 *                      {path}; bool
 *   host_path_key      terminal_tool_config._host_path_key {path}; str
 *   get_sandbox_dir    base.get_sandbox_dir {}; str (clean absolute POSIX
 *                      root; the oracle stub returns PurePosixPath so str()
 *                      keeps forward slashes — Path.as_posix normalization)
 *   sanitize_task_id_for_path  path_utils.sanitize_task_id_for_path (the
 *                      _sandbox_dir_name alias) {task_id}; str
 *   cgroup_limits_available  docker._cgroup_limits_available {image}; bool
 *   extra_args_set_shm_size  docker._extra_args_set_shm_size {extra_args?};
 *                      bool
 *   storage_opt_supported  DockerEnvironment._storage_opt_supported {}; bool
 *   resolve_host_user_spec  docker._resolve_host_user_spec {}; str|null
 *   find_reusable_container  DockerEnvironment._find_reusable_container
 *                      {task_label,profile_label,egress_label};
 *                      [cid,state]|null
 *   container_network_mode  DockerEnvironment._container_network_mode
 *                      {container_id}; str|null
 *   egress_proxy_args_for_docker  docker_egress._egress_proxy_args_for_docker
 *                      {}; [volume_args,env_overrides,host_args]
 *   egress_reuse_fingerprint  docker_egress._egress_reuse_fingerprint
 *                      {volume_args,env_overrides,host_args}; str
 *   egress_enforce_on_docker  docker_egress._egress_enforce_on_docker {}; bool
 *   critical_egress_env_names  docker_egress._critical_egress_env_names
 *                      {env_overrides}; [str,...] (set modeled as list)
 *   check_forward_env_collisions  docker_egress.check_forward_env_collisions
 *                      {forward_env,critical,enforce}; null (may raise)
 *   check_docker_env_collisions  docker_egress.check_docker_env_collisions
 *                      {user_env,egress_env,enforce}; null (may raise)
 *   check_extra_args_collisions  docker_egress.check_extra_args_collisions
 *                      {extra_args,critical,enforce}; null (may raise)
 *   merge_egress_env   docker_egress.merge_egress_env
 *                      {user_env,egress_env,enforce}; dict
 *
 * Claimed siblings (PoP ports elsewhere: env_docker_wrappers.c /
 * environments_docker_ports.c) stay SEAMED, never re-ported:
 * _find_reusable_container, _container_network_mode, _storage_opt_supported,
 * _cgroup_limits_available, _extra_args_set_shm_size, _resolve_host_user_spec.
 * _name_only_env_args is an unclaimed internal helper (no PoP line).
 *
 * String seams are scripted with strings; fixture paths/ids are ASCII except
 * where a case probes the Unicode strip set. str(cpu) uses Python float repr
 * (integral floats keep ".0"); memory/disk are ints. cid[:12] / hex[:8] slice
 * bytes (fixture ids are ASCII). JSON values are libjson json_t.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "libjson/json.h"

/* ── trace recorder (harness infrastructure, no Python counterpart) ── */
typedef struct def_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} def_trace_t;

static void def_trace_add(def_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

/* ── universal seam ── */
typedef struct def_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} def_seams_t;

/* ── raise spec ── */
static const char *def_raise(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}
static const char *def_raise_msg(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return "";
    return json_get_str(r, "msg", "");
}
static json_t *def_raise_spec(const char *kind, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(kind));
    json_set(e, "msg", json_string(msg ? msg : ""));
    return e;
}
/* {"__exc__": kind, "msg": str(e)} — the normalized exception VALUE used for
 * returned exceptions (_start_container) and exception log args. */
static json_t *def_exc_norm(const json_t *spec) {
    json_t *e = json_object();
    json_set(e, "__exc__", json_string(def_raise(spec)));
    json_set(e, "msg", json_string(def_raise_msg(spec)));
    return e;
}

/* ── exception hierarchy (scripted kinds) ── */
static bool def_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0
                 || strcmp(kind, "KeyboardInterrupt") == 0
                 || strcmp(kind, "GeneratorExit") == 0
                 || strcmp(kind, "CancelledError") == 0
                 || strcmp(kind, "BaseException") == 0);
}
static bool def_is_oserror_kind(const char *kind) {
    static const char *fam[] = {
        "OSError", "ProcessLookupError", "PermissionError", "FileNotFoundError",
        "FileExistsError", "NotADirectoryError", "IsADirectoryError",
        "InterruptedError", "TimeoutError", "BlockingIOError", "ChildProcessError",
        "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
        "ConnectionRefusedError", "ConnectionResetError",
    };
    if (!kind) return false;
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(kind, fam[i]) == 0) return true;
    return false;
}
static bool def_is_subprocess_kind(const char *kind) {
    return kind && (strcmp(kind, "SubprocessError") == 0
                 || strcmp(kind, "CalledProcessError") == 0
                 || strcmp(kind, "TimeoutExpired") == 0);
}
/* handler is one of: "Exception", "OSError", "TimeoutExpired",
 * "CalledProcessError", "SubprocessError", "TimeoutExpired|OSError",
 * "SubprocessError|OSError", "CalledProcessError|TimeoutExpired". */
static bool def_exc_matches(const char *kind, const char *handler) {
    if (!kind) return false;
    if (strcmp(handler, "Exception") == 0) return !def_is_base_exc(kind);
    if (strcmp(handler, "OSError") == 0) return def_is_oserror_kind(kind);
    if (strcmp(handler, "TimeoutExpired") == 0) return strcmp(kind, "TimeoutExpired") == 0;
    if (strcmp(handler, "CalledProcessError") == 0) return strcmp(kind, "CalledProcessError") == 0;
    if (strcmp(handler, "SubprocessError") == 0) return def_is_subprocess_kind(kind);
    if (strcmp(handler, "TimeoutExpired|OSError") == 0)
        return strcmp(kind, "TimeoutExpired") == 0 || def_is_oserror_kind(kind);
    if (strcmp(handler, "SubprocessError|OSError") == 0)
        return def_is_subprocess_kind(kind) || def_is_oserror_kind(kind);
    if (strcmp(handler, "CalledProcessError|TimeoutExpired") == 0)
        return strcmp(kind, "CalledProcessError") == 0 || strcmp(kind, "TimeoutExpired") == 0;
    return strcmp(kind, handler) == 0;
}

/* ── Python truthiness / None ── */
static bool def_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool def_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

/* ── dynamic string buffer ── */
typedef struct { char *buf; size_t len, cap; } def_sb_t;
static void def_sb_init(def_sb_t *sb) { sb->buf = NULL; sb->len = 0; sb->cap = 0; }
static void def_sb_grow(def_sb_t *sb, size_t need) {
    if (sb->len + need + 1 <= sb->cap) return;
    size_t nc = sb->cap ? sb->cap : 64;
    while (nc < sb->len + need + 1) nc *= 2;
    char *nb = realloc(sb->buf, nc);
    if (!nb) return;
    sb->buf = nb; sb->cap = nc;
}
static void def_sb_addn(def_sb_t *sb, const char *s, size_t n) {
    if (!s || !n) return;
    def_sb_grow(sb, n);
    if (!sb->buf) return;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}
static void def_sb_add(def_sb_t *sb, const char *s) { if (s) def_sb_addn(sb, s, strlen(s)); }
static void def_sb_addch(def_sb_t *sb, char c) { def_sb_addn(sb, &c, 1); }
static char *def_sb_take(def_sb_t *sb) {
    char *r = sb->buf ? sb->buf : strdup("");
    sb->buf = NULL; sb->len = 0; sb->cap = 0;
    return r;
}

static char *def_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}
static bool def_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* ── Unicode White_Space strip (str.strip() no-arg set) ── */
static size_t def_utf8_next(const char *s, size_t len, size_t i, unsigned *cp) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *cp = c; return i + 1; }
    unsigned n; unsigned v;
    if ((c & 0xE0) == 0xC0) { n = 2; v = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 3; v = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 4; v = c & 0x07; }
    else { *cp = c; return i + 1; }
    if (i + n > len) { *cp = c; return i + 1; }
    for (unsigned k = 1; k < n; k++) {
        unsigned char cc = (unsigned char)s[i + k];
        if ((cc & 0xC0) != 0x80) { *cp = c; return i + 1; }
        v = (v << 6) | (cc & 0x3F);
    }
    *cp = v;
    return i + n;
}
static bool def_is_ws_cp(unsigned cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F))
        return true;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000)
        return true;
    return false;
}
static char *def_strip(const char *s) {
    if (!s) return def_xstrdup("");
    size_t len = strlen(s);
    size_t a = 0, b = len;
    while (a < b) {
        unsigned cp;
        size_t nx = def_utf8_next(s, len, a, &cp);
        if (!def_is_ws_cp(cp)) break;
        a = nx;
    }
    while (b > a) {
        /* find the codepoint start preceding b */
        size_t st = b - 1;
        while (st > a && ((unsigned char)s[st] & 0xC0) == 0x80) st--;
        unsigned cp;
        def_utf8_next(s, len, st, &cp);
        if (!def_is_ws_cp(cp)) break;
        b = st;
    }
    char *d = malloc(b - a + 1);
    if (d) { memcpy(d, s + a, b - a); d[b - a] = '\0'; }
    return d;
}
/* str.rstrip("/") — ASCII char set, both... only the right end. */
static char *def_rstrip_slash(const char *s) {
    if (!s) return def_xstrdup("");
    size_t n = strlen(s);
    while (n && s[n - 1] == '/') n--;
    char *d = malloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = '\0'; }
    return d;
}

/* ── Python repr() for str (ASCII fixture domain; printable non-ASCII kept) ── */
static char *def_py_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    def_sb_t sb; def_sb_init(&sb);
    def_sb_addch(&sb, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { def_sb_addch(&sb, '\\'); def_sb_addch(&sb, (char)c); }
        else if (c == '\n') def_sb_add(&sb, "\\n");
        else if (c == '\r') def_sb_add(&sb, "\\r");
        else if (c == '\t') def_sb_add(&sb, "\\t");
        else if (c < 0x20 || c == 0x7F) {
            char hx[8];
            snprintf(hx, sizeof(hx), "\\x%02x", c);
            def_sb_add(&sb, hx);
        } else def_sb_addch(&sb, (char)c);
    }
    def_sb_addch(&sb, q);
    return def_sb_take(&sb);
}

/* ── Python str(float): integral floats keep ".0"; else shortest round-trip ── */
static char *def_py_float_str(double v) {
    char buf[64];
    if (v == trunc(v) && fabs(v) < 1e16) {
        snprintf(buf, sizeof(buf), "%.1f", v);
        return def_xstrdup(buf);
    }
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, v);
        if (strtod(buf, NULL) == v) break;
    }
    return def_xstrdup(buf);
}

/* str slice [:n] (byte slice; fixture ids/hex are ASCII). */
static char *def_first_n(const char *s, size_t n) {
    if (!s) return def_xstrdup("");
    size_t l = strlen(s);
    if (n < l) l = n;
    char *d = malloc(l + 1);
    if (d) { memcpy(d, s, l); d[l] = '\0'; }
    return d;
}

/* ── seam call helpers ── */
/* Takes ownership of args (frees them); returns the owned seam result. */
static json_t *def_call_raw(const def_seams_t *s, const char *name, json_t *args) {
    json_t *a = args ? args : json_object();
    json_t *r = s->call(s->ctx, name, a);
    json_free(a);
    return r;
}
/* Uncaught site: a raise spec parks into *exc and the call reads as NULL. */
static json_t *def_call(const def_seams_t *s, const char *name, json_t *args, json_t **exc) {
    json_t *r = def_call_raw(s, name, args);
    if (def_raise(r)) { *exc = r; return NULL; }
    return r;
}
static void def_arg_str(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void def_arg_node(json_t *a, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(a, k, json_copy(v));
}
/* logger.<level>(msg, *args) — args recorded verbatim; takes ownership of
 * args_array (NULL == empty). Returns false when the seam raised (*exc set). */
static bool def_log(const def_seams_t *s, json_t **exc, const char *level,
                    const char *msg, json_t *args_array) {
    char name[32];
    snprintf(name, sizeof(name), "log_%s", level);
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg));
    json_set(a, "args", args_array ? args_array : json_array());
    json_t *r = def_call(s, name, a, exc);
    json_free(r);
    return *exc == NULL;
}
/* fail_args tuple + one trailing value (exception norm / rc / stderr). */
static json_t *def_args_plus(const json_t *fail_args, json_t *last) {
    json_t *a = json_array();
    if (fail_args && fail_args->type == JSON_ARRAY)
        for (size_t i = 0; i < fail_args->c.count; i++)
            json_append(a, json_copy(fail_args->c.items[i]));
    if (last) json_append(a, last);
    return a;
}
static json_t *def_str_array(void) { return json_array(); }
static void def_push_str(json_t *arr, const char *s) {
    json_append(arr, json_string(s ? s : ""));
}

/* ── docker CLI arg scans / mount-spec parsing (pure leaves) ── */

/* PoP: def_extra_args_network_mode @ tools/environments/docker.py:_extra_args_network_mode */
json_t *def_extra_args_network_mode(const json_t *extra_args) {
    /* args = [a for a in (extra_args or []) if isinstance(a, str)] */
    const char *args[256];
    size_t n = 0;
    if (extra_args && extra_args->type == JSON_ARRAY) {
        for (size_t i = 0; i < extra_args->c.count && n < 256; i++) {
            const json_t *el = extra_args->c.items[i];
            if (el && el->type == JSON_STRING && el->str_val) args[n++] = el->str_val;
        }
    }
    static const char *flags[] = { "--network", "--net" };
    for (size_t i = 0; i < n; i++) {
        for (size_t f = 0; f < 2; f++) {
            if (strcmp(args[i], flags[f]) == 0)
                return json_string(i + 1 < n ? args[i + 1] : "");
            char eq[32];
            snprintf(eq, sizeof(eq), "%s=", flags[f]);
            if (def_starts_with(args[i], eq))
                return json_string(args[i] + strlen(eq));
        }
    }
    return NULL;
}

/* PoP: def_split_volume_spec @ tools/environments/docker.py:_split_volume_spec */
json_t *def_split_volume_spec(const json_t *spec) {
    /* _VOLUME_SPEC_RE = ^(?P<host>.+):(?P<container>/[^:]+)(?::[^:]*)?$
     * on spec.strip(). host: greedy .+ (no newline); container: '/' plus at
     * least one non-colon; optional ':' + mode (no colon). Greedy host ==
     * rightmost colon that satisfies the tail; scan colons right-to-left. */
    if (!spec || spec->type != JSON_STRING || !spec->str_val) return NULL;
    char *s = def_strip(spec->str_val);
    if (!s) return NULL;
    size_t len = strlen(s);
    json_t *ret = NULL;
    for (size_t p = len; p-- > 0; ) {
        if (s[p] != ':' || p == 0) continue;
        if (memchr(s, '\n', p)) continue;              /* .+ excludes newline */
        if (p + 1 >= len || s[p + 1] != '/') continue;
        size_t q = p + 1;
        while (q < len && s[q] != ':') q++;
        if (q - (p + 1) < 2) continue;                 /* /[^:]+ needs >= 2 */
        if (q < len && memchr(s + q + 1, ':', len - q - 1)) continue;
        char *host = malloc(p + 1);
        if (host) { memcpy(host, s, p); host[p] = '\0'; }
        char *cont = malloc(q - p);
        if (cont) { memcpy(cont, s + p + 1, q - p - 1); cont[q - p - 1] = '\0'; }
        ret = json_array();
        json_append(ret, json_string(host ? host : ""));
        json_append(ret, json_string(cont ? cont : ""));
        free(host); free(cont);
        break;
    }
    free(s);
    return ret;
}

/* PoP: def_container_mount_taken @ tools/environments/docker.py:_container_mount_taken */
bool def_container_mount_taken(const json_t *volume_args, const char *mount) {
    char *t = def_rstrip_slash(mount);
    const char *target = (t && t[0]) ? t : "/";
    bool taken = false;
    if (volume_args && volume_args->type == JSON_ARRAY) {
        for (size_t i = 0; i < volume_args->c.count && !taken; i++) {
            json_t *parsed = def_split_volume_spec(volume_args->c.items[i]);
            if (!parsed) continue;
            /* parsed is [host, container]: read element 1 directly */
            const json_t *cn = json_get(parsed, 1);
            const char *cont = (cn && cn->type == JSON_STRING) ? cn->str_val : "";
            char *c = def_rstrip_slash(cont);
            const char *cc = (c && c[0]) ? c : "/";
            if (strcmp(cc, target) == 0) taken = true;
            free(c);
            json_free(parsed);
        }
    }
    free(t);
    return taken;
}

/* PoP: def_free_host_cwd_mount @ tools/environments/docker.py:_free_host_cwd_mount */
json_t *def_free_host_cwd_mount(const json_t *volume_args) {
    static const char *candidates[] = { "/host-cwd", "/host-cwd-2", "/host-cwd-3" };
    for (size_t i = 0; i < 3; i++)
        if (!def_container_mount_taken(volume_args, candidates[i]))
            return json_string(candidates[i]);
    return json_string(candidates[2]);
}

/* _name_only_env_args — unclaimed internal helper (no PoP): sorted names ->
 * ["-e", k1, "-e", k2, ...]. names is a dict (keys) or list; fixture lists
 * are homogeneous (str or int). */
static int def_name_cmp(const void *a, const void *b) {
    const json_t *x = *(json_t *const *)a, *y = *(json_t *const *)b;
    if (x->type == JSON_STRING && y->type == JSON_STRING)
        return strcmp(x->str_val, y->str_val);
    if (x->type == JSON_NUMBER && y->type == JSON_NUMBER)
        return (x->num_val > y->num_val) - (x->num_val < y->num_val);
    return 0;
}
static json_t *def_name_only_env_args(const json_t *names) {
    json_t *keys = json_array();
    if (names && names->type == JSON_OBJECT) {
        for (size_t i = 0; i < names->c.count; i++)
            json_append(keys, json_string(names->c.keys[i]));
    } else if (names && names->type == JSON_ARRAY) {
        for (size_t i = 0; i < names->c.count; i++)
            json_append(keys, json_copy(names->c.items[i]));
    }
    if (keys->c.count > 1)
        qsort(keys->c.items, keys->c.count, sizeof(json_t *), def_name_cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < keys->c.count; i++) {
        def_push_str(out, "-e");
        json_append(out, json_copy(keys->c.items[i]));
    }
    json_free(keys);
    return out;
}

/* ── host-path resolution branches ── */

/* PoP: def_existing_host_mount @ tools/environments/docker.py:_existing_host_mount */
json_t *def_existing_host_mount(const json_t *volume_args, const char *host_cwd_abs,
                                const def_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    def_arg_str(a, "path", host_cwd_abs);
    json_t *want = def_call(s, "host_path_key", a, exc);
    if (*exc) return NULL;
    if (!def_truthy(want)) { json_free(want); return NULL; }
    json_t *ret = NULL;
    if (volume_args && volume_args->type == JSON_ARRAY) {
        for (size_t i = 0; i < volume_args->c.count && !ret && !*exc; i++) {
            json_t *parsed = def_split_volume_spec(volume_args->c.items[i]);
            if (!parsed) continue;
            const json_t *hn = json_get(parsed, 0);
            json_t *ka = json_object();
            def_arg_str(ka, "path", (hn && hn->type == JSON_STRING) ? hn->str_val : "");
            json_t *key = def_call(s, "host_path_key", ka, exc);
            if (*exc) { json_free(parsed); break; }
            if (key && key->type == JSON_STRING && want->type == JSON_STRING
                    && strcmp(key->str_val, want->str_val) == 0) {
                const json_t *cn = json_get(parsed, 1);
                ret = json_string((cn && cn->type == JSON_STRING) ? cn->str_val : "");
            }
            json_free(key);
            json_free(parsed);
        }
    }
    json_free(want);
    return ret;
}

/* PoP: def_abs_host_cwd @ tools/environments/docker.py:_abs_host_cwd */
json_t *def_abs_host_cwd(const char *host_cwd, const char *os_name,
                         const def_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    def_arg_str(a, "path", host_cwd);
    json_t *expanded = def_call(s, "expanduser", a, exc);
    if (*exc) return NULL;
    const char *ex = (expanded && expanded->type == JSON_STRING) ? expanded->str_val : "";
    json_t *da = json_object();
    def_arg_str(da, "path", ex);
    json_t *drive = def_call(s, "is_windows_drive_path", da, exc);
    if (*exc) { json_free(expanded); return NULL; }
    bool is_drive = drive && drive->type == JSON_BOOL && drive->bool_val;
    json_free(drive);
    if (is_drive && strcmp(os_name ? os_name : "posix", "nt") != 0)
        return expanded;   /* ownership transfers to the caller */
    json_t *aa = json_object();
    def_arg_str(aa, "path", ex);
    json_t *abs = def_call(s, "abspath", aa, exc);
    json_free(expanded);
    return abs;   /* NULL when *exc set */
}

/* PoP: def_host_user_args @ tools/environments/docker.py:_host_user_args */
json_t *def_host_user_args(bool run_as_host_user, const def_seams_t *s, json_t **exc) {
    if (!run_as_host_user) return json_array();
    json_t *spec = def_call(s, "resolve_host_user_spec", json_object(), exc);
    if (*exc) return NULL;
    if (spec) {   /* is not None — an empty string still passes */
        const char *sv = (spec->type == JSON_STRING) ? spec->str_val : "";
        json_t *la = json_array();
        json_append(la, json_string(sv));
        if (!def_log(s, exc, "info", "Docker: running container as host user %s", la)) {
            json_free(spec);
            return NULL;
        }
        json_t *out = json_array();
        def_push_str(out, "--user");
        def_push_str(out, sv);
        json_free(spec);
        return out;
    }
    if (!def_log(s, exc, "warning",
                 "docker_run_as_host_user is enabled but this platform does "
                 "not expose POSIX uid/gid; container will start as its image default user.",
                 NULL))
        return NULL;
    return json_array();
}

/* ── probe / query error paths ── */

/* PoP: def_docker_query @ tools/environments/docker.py:_docker_query */
json_t *def_docker_query(const json_t *argv, double timeout, const char *fail,
                         const json_t *fail_args, const char *nonzero,
                         bool subprocess_handlers, const def_seams_t *s, json_t **exc) {
    json_t *a = json_object();
    def_arg_node(a, "argv", argv);
    json_set(a, "timeout", json_number(timeout));
    json_set(a, "check", json_bool(false));
    json_t *r = def_call_raw(s, "run_capture", a);
    const char *rk = def_raise(r);
    if (rk) {
        const char *handler = subprocess_handlers ? "SubprocessError|OSError"
                                                  : "TimeoutExpired|OSError";
        if (def_exc_matches(rk, handler)) {
            def_log(s, exc, "debug", fail, def_args_plus(fail_args, def_exc_norm(r)));
            json_free(r);
            return NULL;
        }
        *exc = r;   /* not in exc_types — propagates (BaseException included) */
        return NULL;
    }
    if (nonzero && json_get_num(r, "returncode", 0) != 0) {
        char *errtxt = def_strip(json_get_str(r, "stderr", ""));
        json_t *la = def_args_plus(fail_args, json_number(json_get_num(r, "returncode", 0)));
        json_append(la, json_string(errtxt ? errtxt : ""));
        free(errtxt);
        def_log(s, exc, "debug", nonzero, la);
        json_free(r);
        return NULL;
    }
    return r;
}

/* PoP: def_container_image @ tools/environments/docker.py:_container_image */
json_t *def_container_image(json_t *self, const char *container_id,
                            const def_seams_t *s, json_t **exc) {
    json_t *argv = json_array();
    def_push_str(argv, json_get_str(self, "_docker_exe", ""));
    def_push_str(argv, "inspect");
    def_push_str(argv, "--format");
    def_push_str(argv, "{{.Config.Image}}");
    def_push_str(argv, container_id);
    json_t *r = def_docker_query(argv, 10, "docker inspect Image failed: %s", NULL,
                                 "docker inspect Image returned %d: %s", false, s, exc);
    json_free(argv);
    if (*exc || !r) return NULL;
    char *out = def_strip(json_get_str(r, "stdout", ""));
    json_t *ret = (out && out[0]) ? json_string(out) : NULL;   /* strip() or None */
    free(out);
    json_free(r);
    return ret;
}

/* PoP: def_is_idle_workspace_container @ tools/environments/docker.py:_is_idle_workspace_container */
bool def_is_idle_workspace_container(const char *docker_exe, const char *container_id,
                                     const def_seams_t *s, json_t **exc) {
    char *cid12 = def_first_n(container_id, 12);
    json_t *argv = json_array();
    def_push_str(argv, docker_exe);
    def_push_str(argv, "inspect");
    def_push_str(argv, "--format");
    def_push_str(argv, "{{index .Config.Labels \"hermes-workspace\"}}");
    def_push_str(argv, container_id);
    json_t *fail_args = json_array();
    json_append(fail_args, json_string(cid12 ? cid12 : ""));
    json_t *r = def_docker_query(argv, 10, "orphan reaper docker inspect %s failed: %s",
                                 fail_args, NULL, false, s, exc);
    json_free(argv);
    json_free(fail_args);
    free(cid12);
    if (*exc || !r) return false;
    if (json_get_num(r, "returncode", 0) != 0) { json_free(r); return false; }
    char *out = def_strip(json_get_str(r, "stdout", ""));
    bool ok = out && strlen(out) == 16;
    if (ok)
        for (size_t i = 0; i < 16; i++) {
            char c = out[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { ok = false; break; }
        }
    free(out);
    json_free(r);
    return ok;
}

/* ── start / pull gates ── */

/* PoP: def_start_container @ tools/environments/docker.py:_start_container */
json_t *def_start_container(json_t *self, const char *container_id,
                            const def_seams_t *s, json_t **exc) {
    json_t *argv = json_array();
    def_push_str(argv, json_get_str(self, "_docker_exe", ""));
    def_push_str(argv, "start");
    def_push_str(argv, container_id);
    json_t *a = json_object();
    def_arg_node(a, "argv", argv);
    json_free(argv);
    json_set(a, "timeout", json_number(30));
    json_set(a, "check", json_bool(true));
    json_t *r = def_call_raw(s, "run_capture", a);
    const char *rk = def_raise(r);
    if (rk) {
        if (def_exc_matches(rk, "CalledProcessError|TimeoutExpired")) {
            json_t *ret = def_exc_norm(r);   /* returns the failure, not raises */
            json_free(r);
            return ret;
        }
        *exc = r;
        return NULL;
    }
    json_free(r);
    return NULL;   /* None on success */
}

/* except (subprocess.SubprocessError, OSError) site shared by the probe and
 * the pull: log "could not pull" and degrade to False. */
static bool def_pull_gate_catch(const def_seams_t *s, json_t **exc,
                                const char *image, json_t *spec) {
    json_t *la = json_array();
    json_append(la, json_string(image ? image : ""));
    json_append(la, def_exc_norm(spec));
    return def_log(s, exc, "warning", "Docker: could not pull %s: %s", la);
}

/* PoP: def_image_available_locally @ tools/environments/docker.py:_image_available_locally */
bool def_image_available_locally(json_t *self, const def_seams_t *s, json_t **exc) {
    const char *exe = json_get_str(self, "_docker_exe", "");
    const char *image = json_get_str(self, "_image", "");
    json_t *argv = json_array();
    def_push_str(argv, exe);
    def_push_str(argv, "image");
    def_push_str(argv, "inspect");
    def_push_str(argv, image);
    def_push_str(argv, "--format");
    def_push_str(argv, "{{.Id}}");
    json_t *a = json_object();
    def_arg_node(a, "argv", argv);
    json_free(argv);
    json_set(a, "timeout", json_number(30));
    json_set(a, "check", json_bool(false));
    json_t *probe = def_call_raw(s, "run_capture", a);
    const char *pk = def_raise(probe);
    if (pk) {
        if (def_exc_matches(pk, "SubprocessError|OSError")) {
            def_pull_gate_catch(s, exc, image, probe);
            json_free(probe);
            return false;
        }
        *exc = probe;
        return false;
    }
    if (json_get_num(probe, "returncode", 0) == 0) { json_free(probe); return true; }
    json_free(probe);
    json_t *pargv = json_array();
    def_push_str(pargv, exe);
    def_push_str(pargv, "pull");
    def_push_str(pargv, image);
    json_t *pa = json_object();
    def_arg_node(pa, "argv", pargv);
    json_free(pargv);
    json_set(pa, "timeout", json_number(900));
    json_set(pa, "check", json_bool(false));
    json_t *pull = def_call_raw(s, "run_capture", pa);
    const char *lk = def_raise(pull);
    if (lk) {
        if (def_exc_matches(lk, "SubprocessError|OSError")) {
            def_pull_gate_catch(s, exc, image, pull);
            json_free(pull);
            return false;
        }
        *exc = pull;
        return false;
    }
    bool ok = json_get_num(pull, "returncode", 0) == 0;
    json_free(pull);
    return ok;
}

/* ── spawn ordering / orphan cleanup / resource gates ── */

/* PoP: def_run_command @ tools/environments/docker.py:_run_command */
json_t *def_run_command(json_t *self, const char *name, const char *workdir) {
    json_t *out = json_array();
    def_push_str(out, json_get_str(self, "_docker_exe", ""));
    def_push_str(out, "run");
    def_push_str(out, "-d");
    /* [] if self._image_uses_s6_init or self._snap_compat else ["--init"] */
    if (!json_get_bool(self, "_image_uses_s6_init", false)
            && !json_get_bool(self, "_snap_compat", false))
        def_push_str(out, "--init");
    def_push_str(out, "--name");
    def_push_str(out, name);
    const json_t *labels = json_obj_get(self, "_labels");
    if (labels && labels->type == JSON_OBJECT) {
        for (size_t i = 0; i < labels->c.count; i++) {
            def_push_str(out, "--label");
            const json_t *v = labels->c.items[i];
            def_sb_t sb; def_sb_init(&sb);
            def_sb_add(&sb, labels->c.keys[i]);
            def_sb_addch(&sb, '=');
            def_sb_add(&sb, (v && v->type == JSON_STRING) ? v->str_val : "");
            char *kv = def_sb_take(&sb);
            def_push_str(out, kv);
            free(kv);
        }
    }
    def_push_str(out, "-w");
    def_push_str(out, workdir);
    const json_t *all = json_obj_get(self, "_all_run_args");
    if (all && all->type == JSON_ARRAY)
        for (size_t i = 0; i < all->c.count; i++)
            json_append(out, json_copy(all->c.items[i]));
    def_push_str(out, json_get_str(self, "_image", ""));
    def_push_str(out, "sleep");
    def_push_str(out, "infinity");
    return out;
}

static const char *def_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == trunc(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* PoP: def_docker_run @ tools/environments/docker.py:_docker_run */
json_t *def_docker_run(json_t *self, const char *cwd, const def_seams_t *s, json_t **exc) {
    json_t *hex = def_call(s, "uuid4_hex", json_object(), exc);
    if (*exc) return NULL;
    if (!hex || hex->type != JSON_STRING) {
        /* uuid.uuid4().hex[:8] on a non-str scripted hex */
        char msg[96];
        snprintf(msg, sizeof(msg), "'%s' object is not subscriptable", def_type_name(hex));
        *exc = def_raise_spec("TypeError", msg);
        json_free(hex);
        return NULL;
    }
    char *first8 = def_first_n(hex->str_val, 8);
    json_free(hex);
    def_sb_t nb; def_sb_init(&nb);
    def_sb_add(&nb, "hermes-");
    def_sb_add(&nb, first8);
    free(first8);
    char *container_name = def_sb_take(&nb);

    json_t *run_cmd = def_run_command(self, container_name, cwd);
    def_sb_t jb; def_sb_init(&jb);
    for (size_t i = 0; i < run_cmd->c.count; i++) {
        if (i) def_sb_addch(&jb, ' ');
        const json_t *el = run_cmd->c.items[i];
        def_sb_add(&jb, (el && el->type == JSON_STRING) ? el->str_val : "");
    }
    char *joined = def_sb_take(&jb);
    json_t *da = json_array();
    json_append(da, json_string(joined));
    free(joined);
    if (!def_log(s, exc, "debug", "Starting container: %s", da)) {
        json_free(run_cmd);
        free(container_name);
        return NULL;
    }

    /* kwarg evaluation order: env=self._docker_client_env(...) fires BEFORE
     * run_capture (client_env_with seam via the live delegation). */
    json_t *ca = json_object();
    def_arg_node(ca, "values", json_obj_get(self, "_run_env_values"));
    json_t *client_env = def_call(s, "client_env_with", ca, exc);
    if (*exc) { json_free(run_cmd); free(container_name); return NULL; }

    json_t *ra = json_object();
    def_arg_node(ra, "argv", run_cmd);
    json_free(run_cmd);
    json_set(ra, "timeout", json_number(120));
    json_set(ra, "check", json_bool(true));
    def_arg_node(ra, "env", client_env);
    json_t *r = def_call_raw(s, "run_capture", ra);
    const char *rk = def_raise(r);
    if (rk) {
        if (def_exc_matches(rk, "CalledProcessError|TimeoutExpired")) {
            json_t *wa = json_array();
            json_append(wa, json_string(container_name));
            json_append(wa, def_exc_norm(r));
            if (!def_log(s, exc, "warning",
                         "docker run failed for %s, cleaning up orphaned container: %s", wa)) {
                json_free(r);
                json_free(client_env);
                free(container_name);
                return NULL;
            }
            json_t *sa = json_object();
            json_t *sargv = json_array();
            def_push_str(sargv, json_get_str(self, "_docker_exe", ""));
            def_push_str(sargv, "rm");
            def_push_str(sargv, "-f");
            def_push_str(sargv, container_name);
            def_arg_node(sa, "argv", sargv);
            json_free(sargv);
            json_set(sa, "capture_output", json_bool(true));
            json_set(sa, "timeout", json_number(10));
            json_set(sa, "stdin", json_number(-3));   /* subprocess.DEVNULL */
            json_set(sa, "check", json_bool(false));
            json_t *sr = def_call_raw(s, "subprocess_run", sa);
            if (def_raise(sr)) {   /* cleanup failure REPLACES the in-flight raise */
                *exc = sr;
                json_free(r);
                json_free(client_env);
                free(container_name);
                return NULL;
            }
            json_free(sr);
            *exc = r;   /* bare `raise` re-raises the original */
            json_free(client_env);
            free(container_name);
            return NULL;
        }
        *exc = r;   /* OSError etc. propagate without cleanup */
        json_free(client_env);
        free(container_name);
        return NULL;
    }
    char *cid = def_strip(json_get_str(r, "stdout", ""));
    json_free(r);
    char *cid12 = def_first_n(cid, 12);
    json_t *ia = json_array();
    json_append(ia, json_string(container_name));
    json_append(ia, json_string(cid12 ? cid12 : ""));
    free(cid12);
    if (!def_log(s, exc, "info", "Started container %s (%s)", ia)) {
        free(cid);
        json_free(client_env);
        free(container_name);
        return NULL;
    }
    json_t *ret = json_string(cid ? cid : "");
    free(cid);
    json_free(client_env);
    free(container_name);
    return ret;
}

/* PoP: def_resource_args @ tools/environments/docker.py:_resource_args */
json_t *def_resource_args(json_t *self, const char *image, double cpu, long memory,
                          long disk, bool network, const json_t *shm_size,
                          const json_t *extra_args, const char *platform,
                          const def_seams_t *s, json_t **exc) {
    (void)self;   /* only self._storage_opt_supported (seamed) is read */
    json_t *args = json_array();
    json_t *cga = json_object();
    def_arg_str(cga, "image", image);
    json_t *cg = def_call(s, "cgroup_limits_available", cga, exc);
    if (*exc) { json_free(args); return NULL; }
    bool cgroup_ok = cg && cg->type == JSON_BOOL && cg->bool_val;
    json_free(cg);
    if (cgroup_ok) {
        if (cpu > 0) {
            char *cs = def_py_float_str(cpu);
            def_push_str(args, "--cpus");
            def_push_str(args, cs);
            free(cs);
        }
        if (memory > 0) {
            char mb[32];
            snprintf(mb, sizeof(mb), "%ldm", memory);
            def_push_str(args, "--memory");
            def_push_str(args, mb);
        }
        def_push_str(args, "--pids-limit");
        def_push_str(args, "2048");   /* _DEFAULT_PIDS_LIMIT */
    }
    /* shm = str(shm_size or "").strip() — fixtures script str|null */
    char *shm = NULL;
    if (def_truthy(shm_size) && shm_size->type == JSON_STRING)
        shm = def_strip(shm_size->str_val);
    else
        shm = def_xstrdup("");
    if (shm[0] && strcmp(shm, "0") != 0) {
        json_t *sa = json_object();
        def_arg_node(sa, "extra_args", extra_args);
        json_t *set = def_call(s, "extra_args_set_shm_size", sa, exc);
        if (*exc) { free(shm); json_free(args); return NULL; }
        bool already = set && set->type == JSON_BOOL && set->bool_val;
        json_free(set);
        if (!already) {
            def_push_str(args, "--shm-size");
            def_push_str(args, shm);
        }
    }
    if (disk > 0 && strcmp(platform ? platform : "linux", "darwin") != 0) {
        json_t *ok = def_call(s, "storage_opt_supported", json_object(), exc);
        if (*exc) { free(shm); json_free(args); return NULL; }
        bool supported = ok && ok->type == JSON_BOOL && ok->bool_val;
        json_free(ok);
        if (supported) {
            char db[48];
            snprintf(db, sizeof(db), "size=%ldm", disk);
            def_push_str(args, "--storage-opt");
            def_push_str(args, db);
        } else if (!def_log(s, exc, "warning",
                            "Docker storage driver does not support per-container disk limits "
                            "(requires overlay2 on XFS with pquota). Container will run without disk quota.",
                            NULL)) {
            free(shm);
            json_free(args);
            return NULL;
        }
    }
    if (!network) {
        json_t *en = def_extra_args_network_mode(extra_args);
        if (!en) {
            def_push_str(args, "--network=none");
        } else if (strcmp(en->str_val, "none") != 0) {
            char *rp = def_py_repr_str(en->str_val);
            def_sb_t mb; def_sb_init(&mb);
            def_sb_add(&mb, "terminal.docker_network is false (air-gapped) but terminal.docker_extra_args "
                            "requests --network=");
            def_sb_add(&mb, rp);
            def_sb_add(&mb, ". Remove the --network entry from docker_extra_args, or set "
                            "docker_network: true to opt into that network.");
            char *msg = def_sb_take(&mb);
            free(rp);
            *exc = def_raise_spec("RuntimeError", msg);
            free(msg);
            json_free(en);
            free(shm);
            json_free(args);
            return NULL;
        }
        json_free(en);
        /* extra_network == "none": same intent stated twice; no flag added */
    }
    free(shm);
    return args;
}

/* ── mount resolution branches ── */

/* PoP: def_mount_args @ tools/environments/docker.py:_mount_args */
json_t *def_mount_args(json_t *self, const json_t *volumes, const char *host_cwd,
                       bool auto_mount_cwd, const char *task_id, const char *os_name,
                       const def_seams_t *s, json_t **exc) {
    json_t *volume_args = json_array();
    json_t *writable_args = NULL;
    char *host_cwd_abs = NULL, *existing_mount = NULL, *bind_target = NULL;
    json_t *ret = NULL;

    if (volumes && volumes->type == JSON_ARRAY) {
        for (size_t i = 0; i < volumes->c.count; i++) {
            const json_t *vol = volumes->c.items[i];
            if (!vol || vol->type != JSON_STRING) {
                json_t *la = json_array();
                json_append(la, json_copy(vol));
                if (!def_log(s, exc, "warning", "Docker volume entry is not a string: %r", la))
                    goto fail;
                continue;
            }
            char *v = def_strip(vol->str_val);
            if (!v[0]) { free(v); continue; }
            if (!strchr(v, ':')) {
                json_t *la = json_array();
                json_append(la, json_string(v));
                if (!def_log(s, exc, "warning", "Docker volume '%s' missing colon, skipping", la)) {
                    free(v);
                    goto fail;
                }
                free(v);
                continue;
            }
            def_push_str(volume_args, "-v");
            def_push_str(volume_args, v);
            free(v);
        }
    }
    bool ws_explicit = false;
    for (size_t i = 0; i < volume_args->c.count; i++) {
        const json_t *el = volume_args->c.items[i];
        if (el && el->type == JSON_STRING && strstr(el->str_val, ":/workspace")) {
            ws_explicit = true;
            break;
        }
    }

    host_cwd_abs = def_xstrdup("");
    if (host_cwd && host_cwd[0]) {
        json_t *abs = def_abs_host_cwd(host_cwd, os_name, s, exc);
        if (*exc) goto fail;
        free(host_cwd_abs);
        host_cwd_abs = def_xstrdup((abs && abs->type == JSON_STRING) ? abs->str_val : "");
        json_free(abs);
    }
    /* windows_cwd = drive(host_cwd or "") or drive(host_cwd_abs) — short-circuit */
    json_t *wa = json_object();
    def_arg_str(wa, "path", (host_cwd && host_cwd[0]) ? host_cwd : "");
    json_t *w1 = def_call(s, "is_windows_drive_path", wa, exc);
    if (*exc) goto fail;
    bool windows_cwd = w1 && w1->type == JSON_BOOL && w1->bool_val;
    json_free(w1);
    if (!windows_cwd) {
        json_t *wa2 = json_object();
        def_arg_str(wa2, "path", host_cwd_abs);
        json_t *w2 = def_call(s, "is_windows_drive_path", wa2, exc);
        if (*exc) goto fail;
        windows_cwd = w2 && w2->type == JSON_BOOL && w2->bool_val;
        json_free(w2);
    }
    bool host_dir_exists = false;
    if (host_cwd_abs[0]) {
        json_t *ia = json_object();
        def_arg_str(ia, "path", host_cwd_abs);
        json_t *id = def_call(s, "isdir", ia, exc);
        if (*exc) goto fail;
        host_dir_exists = id && id->type == JSON_BOOL && id->bool_val;
        json_free(id);
    }
    bool should_bind = host_dir_exists && (auto_mount_cwd || windows_cwd);
    if ((auto_mount_cwd || windows_cwd) && host_cwd && host_cwd[0] && !host_dir_exists) {
        json_t *la = json_array();
        json_append(la, json_string(host_cwd));
        if (!def_log(s, exc, "debug",
                     "Skipping docker cwd mount: host_cwd is not a valid directory: %s", la))
            goto fail;
    }

    if (should_bind) {
        json_t *em = def_existing_host_mount(volume_args, host_cwd_abs, s, exc);
        if (*exc) goto fail;
        if (em) {
            existing_mount = def_xstrdup((em->type == JSON_STRING) ? em->str_val : "");
            json_free(em);
        }
    }
    if (existing_mount && existing_mount[0]) {
        json_set(self, "host_cwd", json_string(host_cwd_abs));
        json_set(self, "host_cwd_mount", json_string(existing_mount));
    } else if (should_bind) {
        if (!ws_explicit) {
            bind_target = def_xstrdup("/workspace");
        } else {
            json_t *fm = def_free_host_cwd_mount(volume_args);
            bind_target = def_xstrdup(fm->str_val);
            json_free(fm);
        }
        json_set(self, "host_cwd", json_string(host_cwd_abs));
        json_set(self, "host_cwd_mount", json_string(bind_target));
    } else {
        json_set(self, "host_cwd", json_null());
        json_set(self, "host_cwd_mount", json_null());
    }
    bool bind_at_ws = bind_target && strcmp(bind_target, "/workspace") == 0;
    bool mount_workspace = !bind_at_ws && !ws_explicit;

    writable_args = json_array();
    if (json_get_bool(self, "_persistent", false)) {
        json_t *root = def_call(s, "get_sandbox_dir", json_object(), exc);
        if (*exc) goto fail;
        json_t *ta = json_object();
        def_arg_str(ta, "task_id", task_id);
        json_t *dn = def_call(s, "sanitize_task_id_for_path", ta, exc);
        if (*exc) { json_free(root); goto fail; }
        /* sandbox = get_sandbox_dir() / "docker" / _sandbox_dir_name(task_id);
         * the oracle stub returns PurePosixPath (as_posix normalization).
         * Fixture roots are clean absolute POSIX paths; trailing '/' folds. */
        char *root_trim = def_rstrip_slash((root && root->type == JSON_STRING) ? root->str_val : "");
        const char *dns = (dn && dn->type == JSON_STRING) ? dn->str_val : "";
        def_sb_t sb; def_sb_init(&sb);
        def_sb_add(&sb, root_trim);
        def_sb_add(&sb, "/docker/");
        def_sb_add(&sb, dns);
        char *sandbox = def_sb_take(&sb);
        free(root_trim);
        json_free(root);
        json_free(dn);
        def_sb_t hb; def_sb_init(&hb);
        def_sb_add(&hb, sandbox);
        def_sb_add(&hb, "/home");
        char *home = def_sb_take(&hb);
        json_set(self, "_home_dir", json_string(home));
        json_t *ma = json_object();
        def_arg_str(ma, "path", home);
        json_set(ma, "exist_ok", json_bool(true));
        json_t *mr = def_call(s, "makedirs", ma, exc);
        json_free(mr);
        if (*exc) { free(sandbox); free(home); goto fail; }
        json_t *sm = json_array();
        json_t *pair = json_array();
        def_push_str(pair, home);
        def_push_str(pair, "/root");
        json_append(sm, pair);
        json_set(self, "_sandbox_mounts", sm);
        if (mount_workspace) {
            def_sb_t wb; def_sb_init(&wb);
            def_sb_add(&wb, sandbox);
            def_sb_add(&wb, "/workspace");
            char *ws = def_sb_take(&wb);
            json_set(self, "_workspace_dir", json_string(ws));
            json_t *ma2 = json_object();
            def_arg_str(ma2, "path", ws);
            json_set(ma2, "exist_ok", json_bool(true));
            json_t *mr2 = def_call(s, "makedirs", ma2, exc);
            json_free(mr2);
            if (*exc) { free(ws); free(sandbox); free(home); goto fail; }
            json_t *pair2 = json_array();
            def_push_str(pair2, ws);
            def_push_str(pair2, "/workspace");
            json_append(sm, pair2);   /* sm borrowed from self; no setter on the key */
            free(ws);
        }
        for (size_t i = 0; i < sm->c.count; i++) {
            const json_t *p = sm->c.items[i];
            const json_t *h = json_get(p, 0);
            const json_t *c = json_get(p, 1);
            def_sb_t vb; def_sb_init(&vb);
            def_sb_add(&vb, (h && h->type == JSON_STRING) ? h->str_val : "");
            def_sb_addch(&vb, ':');
            def_sb_add(&vb, (c && c->type == JSON_STRING) ? c->str_val : "");
            char *spec = def_sb_take(&vb);
            def_push_str(writable_args, "-v");
            def_push_str(writable_args, spec);
            free(spec);
        }
        free(sandbox);
        free(home);
    } else {
        if (mount_workspace) {
            def_push_str(writable_args, "--tmpfs");
            def_push_str(writable_args, "/workspace:rw,exec,size=10g");
        }
        def_push_str(writable_args, "--tmpfs");
        def_push_str(writable_args, "/home:rw,exec,size=1g");
        def_push_str(writable_args, "--tmpfs");
        def_push_str(writable_args, "/root:rw,exec,size=1g");
    }

    if (bind_target) {
        json_t *la = json_array();
        json_append(la, json_string(bind_target));
        json_append(la, json_string(host_cwd_abs));
        if (!def_log(s, exc, "info", "Mounting configured host cwd to %s: %s", la))
            goto fail;
        json_t *nv = json_array();
        def_push_str(nv, "-v");
        def_sb_t bb; def_sb_init(&bb);
        def_sb_add(&bb, host_cwd_abs);
        def_sb_addch(&bb, ':');
        def_sb_add(&bb, bind_target);
        char *spec = def_sb_take(&bb);
        def_push_str(nv, spec);
        free(spec);
        for (size_t i = 0; i < volume_args->c.count; i++)
            json_append(nv, json_copy(volume_args->c.items[i]));
        json_free(volume_args);
        volume_args = nv;
    } else if (ws_explicit && !existing_mount) {
        if (!def_log(s, exc, "debug",
                     "Skipping docker cwd mount: /workspace already mounted by user config", NULL))
            goto fail;
    }
    ret = json_array();
    json_append(ret, volume_args);
    json_append(ret, writable_args);
    volume_args = NULL;
    writable_args = NULL;
    goto out;
fail:
    json_free(volume_args);
    json_free(writable_args);
out:
    free(host_cwd_abs);
    free(existing_mount);
    free(bind_target);
    return ret;
}

/* ── egress / env resolution branches ── */

/* PoP: def_egress_and_env_args @ tools/environments/docker.py:_egress_and_env_args */
json_t *def_egress_and_env_args(json_t *self, const json_t *extra_args,
                                const def_seams_t *s, json_t **exc) {
    json_t *eg = def_call(s, "egress_proxy_args_for_docker", json_object(), exc);
    if (*exc) return NULL;
    if (!eg || eg->type != JSON_ARRAY || eg->c.count != 3) {
        char msg[96];
        if (!eg || eg->type == JSON_NULL)
            snprintf(msg, sizeof(msg), "cannot unpack non-iterable NoneType object");
        else if (eg->type == JSON_ARRAY)
            snprintf(msg, sizeof(msg), "%s values to unpack (expected 3, got %zu)",
                     eg->c.count < 3 ? "not enough" : "too many", eg->c.count);
        else
            snprintf(msg, sizeof(msg), "cannot unpack non-iterable %s object", def_type_name(eg));
        *exc = def_raise_spec(eg && eg->type == JSON_ARRAY ? "ValueError" : "TypeError", msg);
        json_free(eg);
        return NULL;
    }
    json_t *eg_vols = json_copy(json_get(eg, 0));
    json_t *eg_over = json_copy(json_get(eg, 1));
    json_t *eg_hosts = json_copy(json_get(eg, 2));
    json_free(eg);
    json_t *label = NULL, *critical = NULL, *merged = NULL, *validated = NULL, *ret = NULL;

    json_t *fa = json_object();
    def_arg_node(fa, "volume_args", eg_vols);
    def_arg_node(fa, "env_overrides", eg_over);
    def_arg_node(fa, "host_args", eg_hosts);
    label = def_call(s, "egress_reuse_fingerprint", fa, exc);
    if (*exc) goto fail;
    bool has_over = def_truthy(eg_over);
    bool enforce = true;   /* _egress_enforce_on_docker() only when overrides */
    if (has_over) {
        json_t *en = def_call(s, "egress_enforce_on_docker", json_object(), exc);
        if (*exc) goto fail;
        enforce = en && en->type == JSON_BOOL && en->bool_val;
        json_free(en);
    }
    json_t *ca = json_object();
    def_arg_node(ca, "env_overrides", eg_over);
    critical = def_call(s, "critical_egress_env_names", ca, exc);
    if (*exc) goto fail;
    if (has_over) {
        json_t *cfa = json_object();
        def_arg_node(cfa, "forward_env", json_obj_get(self, "_forward_env"));
        def_arg_node(cfa, "critical", critical);
        json_set(cfa, "enforce", json_bool(enforce));
        json_t *r1 = def_call(s, "check_forward_env_collisions", cfa, exc);
        json_free(r1);
        if (*exc) goto fail;
        json_t *cda = json_object();
        def_arg_node(cda, "user_env", json_obj_get(self, "_env"));
        def_arg_node(cda, "egress_env", eg_over);
        json_set(cda, "enforce", json_bool(enforce));
        json_t *r2 = def_call(s, "check_docker_env_collisions", cda, exc);
        json_free(r2);
        if (*exc) goto fail;
    }
    json_t *ma = json_object();
    def_arg_node(ma, "user_env", json_obj_get(self, "_env"));
    def_arg_node(ma, "egress_env", eg_over);
    json_set(ma, "enforce", json_bool(enforce));
    merged = def_call(s, "merge_egress_env", ma, exc);
    if (*exc) goto fail;
    if (!merged || merged->type != JSON_OBJECT) {
        /* self._run_env_values = dict(merged_env) */
        char msg[96];
        snprintf(msg, sizeof(msg), "'%s' object is not iterable", def_type_name(merged));
        *exc = def_raise_spec("TypeError", msg);
        goto fail;
    }
    json_set(self, "_run_env_values", json_copy(merged));

    validated = json_array();
    if (extra_args && extra_args->type == JSON_ARRAY) {
        for (size_t i = 0; i < extra_args->c.count; i++) {
            const json_t *arg = extra_args->c.items[i];
            if (!arg || arg->type != JSON_STRING) {
                json_t *la = json_array();
                json_append(la, json_copy(arg));
                if (!def_log(s, exc, "warning", "Ignoring non-string docker_extra_args entry: %r", la))
                    goto fail;
                continue;
            }
            json_append(validated, json_copy(arg));
        }
    }
    if (has_over) {
        json_t *cea = json_object();
        def_arg_node(cea, "extra_args", validated);
        def_arg_node(cea, "critical", critical);
        json_set(cea, "enforce", json_bool(enforce));
        json_t *r3 = def_call(s, "check_extra_args_collisions", cea, exc);
        json_free(r3);
        if (*exc) goto fail;
    }
    ret = json_array();
    json_append(ret, label ? label : json_null());
    label = NULL;
    json_append(ret, eg_vols);
    eg_vols = NULL;
    json_append(ret, eg_hosts);
    eg_hosts = NULL;
    json_append(ret, def_name_only_env_args(merged));
    json_append(ret, validated);
    validated = NULL;
fail:
    json_free(eg_vols);
    json_free(eg_over);
    json_free(eg_hosts);
    json_free(label);
    json_free(critical);
    json_free(merged);
    json_free(validated);
    return ret;
}

/* ── the attach / reuse decision chain ── */

/* docker rm -f site shared by the image-mismatch and network-guard branches.
 * Returns 0 when the removal completed or its failure was caught+logged,
 * nonzero when an exception propagated (*exc set). */
static int def_attach_rm(const def_seams_t *s, json_t **exc, const char *exe,
                         const char *cid, const char *cid12) {
    json_t *ra = json_object();
    json_t *argv = json_array();
    def_push_str(argv, exe);
    def_push_str(argv, "rm");
    def_push_str(argv, "-f");
    def_push_str(argv, cid);
    def_arg_node(ra, "argv", argv);
    json_free(argv);
    json_set(ra, "timeout", json_number(30));
    json_set(ra, "check", json_bool(false));
    json_t *r = def_call_raw(s, "run_capture", ra);
    const char *rk = def_raise(r);
    if (rk) {
        if (def_exc_matches(rk, "TimeoutExpired|OSError")) {
            json_t *la = json_array();
            json_append(la, json_string(cid12));
            json_append(la, def_exc_norm(r));
            json_free(r);
            if (!def_log(s, exc, "warning", "Failed to remove mismatched container %s: %s", la))
                return 1;
            return 0;
        }
        *exc = r;
        return 1;
    }
    json_free(r);
    return 0;
}

/* PoP: def_attach_existing_container @ tools/environments/docker.py:_attach_existing_container */
bool def_attach_existing_container(json_t *self, const char *task_label,
                                   const char *profile_name, const char *egress_label,
                                   bool network, const def_seams_t *s, json_t **exc) {
    json_t *fa = json_object();
    def_arg_str(fa, "task_label", task_label);
    def_arg_str(fa, "profile_label", profile_name);
    def_arg_str(fa, "egress_label", egress_label);
    json_t *existing = def_call(s, "find_reusable_container", fa, exc);
    if (*exc) return false;
    if (!existing) return false;
    if (existing->type != JSON_ARRAY || existing->c.count != 2) {
        char msg[96];
        if (existing->type == JSON_ARRAY)
            snprintf(msg, sizeof(msg), "%s values to unpack (expected 2, got %zu)",
                     existing->c.count < 2 ? "not enough" : "too many", existing->c.count);
        else
            snprintf(msg, sizeof(msg), "cannot unpack non-iterable %s object", def_type_name(existing));
        *exc = def_raise_spec(existing->type == JSON_ARRAY ? "ValueError" : "TypeError", msg);
        json_free(existing);
        return false;
    }
    const json_t *c0 = json_get(existing, 0);
    const json_t *c1 = json_get(existing, 1);
    char *cid = def_xstrdup((c0 && c0->type == JSON_STRING) ? c0->str_val : "");
    char *state = def_xstrdup((c1 && c1->type == JSON_STRING) ? c1->str_val : "");
    json_free(existing);
    char *cid12 = def_first_n(cid, 12);
    const char *image = json_get_str(self, "_image", "");
    const char *exe = json_get_str(self, "_docker_exe", "");
    bool pinned = json_get_bool(self, "_image_pinned", false);
    bool ok = false;

    json_t *actual = def_container_image(self, cid, s, exc);
    if (*exc) goto out;
    if (actual && actual->type == JSON_STRING && strcmp(actual->str_val, image) != 0) {
        const char *ai = actual->str_val;
        if (!pinned) {
            json_t *la = json_array();
            json_append(la, json_string(cid12));
            json_append(la, json_string(ai));
            json_append(la, json_string(image));
            json_append(la, json_string(image));
            json_append(la, json_string(task_label));
            json_append(la, json_string(profile_name));
            if (!def_log(s, exc, "warning",
                         "Existing container %s runs image %s; the default docker_image is now %s. "
                         "Keeping the existing sandbox \xe2\x80\x94" " approve the switch with "
                         "`hermes config set terminal.docker_image %s` (files in /root and /workspace "
                         "carry over) or pin the current image to stop this notice (task=%s, profile=%s).",
                         la))
                goto out;
        } else {
            bool avail = def_image_available_locally(self, s, exc);
            if (*exc) goto out;
            if (!avail) {
                json_t *la = json_array();
                json_append(la, json_string(cid12));
                json_append(la, json_string(ai));
                json_append(la, json_string(image));
                json_append(la, json_string(task_label));
                json_append(la, json_string(profile_name));
                if (!def_log(s, exc, "warning",
                             "Existing container %s runs image %s; docker_image is %s but that image "
                             "could not be pulled \xe2\x80\x94" " keeping the current sandbox until "
                             "it can (task=%s, profile=%s).", la))
                    goto out;
            } else {
                json_t *la = json_array();
                json_append(la, json_string(cid12));
                json_append(la, json_string(ai));
                json_append(la, json_string(image));
                json_append(la, json_string(task_label));
                json_append(la, json_string(profile_name));
                if (!def_log(s, exc, "warning",
                             "Existing container %s runs image %s but docker_image is %s \xe2\x80\x94"
                             " removing it and starting fresh (task=%s, profile=%s).", la))
                    goto out;
                if (def_attach_rm(s, exc, exe, cid, cid12)) goto out;
                goto out;   /* return False — fresh container path */
            }
        }
    }
    json_free(actual);
    actual = NULL;
    if (!network) {
        json_t *na = json_object();
        def_arg_str(na, "container_id", cid);
        json_t *mode = def_call(s, "container_network_mode", na, exc);
        if (*exc) goto out;
        const char *ms = (mode && mode->type == JSON_STRING) ? mode->str_val : NULL;
        if (!ms || strcmp(ms, "none") != 0) {
            json_t *la = json_array();
            json_append(la, json_string(cid12));
            json_append(la, json_string((ms && ms[0]) ? ms : "unknown"));
            json_append(la, json_string(task_label));
            json_append(la, json_string(profile_name));
            if (!def_log(s, exc, "warning",
                         "Existing container %s has NetworkMode=%s but docker_network=false requests "
                         "an air-gapped container \xe2\x80\x94" " removing it and starting fresh "
                         "(task=%s, profile=%s).", la)) {
                json_free(mode);
                goto out;
            }
            json_free(mode);
            if (def_attach_rm(s, exc, exe, cid, cid12)) goto out;
            goto out;   /* return False */
        }
        json_free(mode);
    }
    if (strcmp(state, "running") != 0) {
        json_t *err = def_start_container(self, cid, s, exc);
        if (*exc) goto out;
        if (err) {
            json_t *la = json_array();
            json_append(la, json_string(cid12));
            json_append(la, json_string(state));
            json_append(la, err);   /* already {"__exc__", "msg"}; adopted */
            if (!def_log(s, exc, "warning",
                         "Failed to start existing container %s (state=%s): %s \xe2\x80\x94"
                         " falling back to a fresh container.", la))
                goto out;
            goto out;   /* return False */
        }
    }
    json_set(self, "_container_id", json_string(cid));
    {
        json_t *la = json_array();
        json_append(la, json_string(cid12));
        json_append(la, json_string(task_label));
        json_append(la, json_string(profile_name));
        json_append(la, json_string(state));
        if (!def_log(s, exc, "info",
                     "Reusing container %s (task=%s, profile=%s, prior state=%s)", la))
            goto out;
    }
    ok = true;
out:
    json_free(actual);
    free(cid);
    free(state);
    free(cid12);
    return ok;
}
