/*
 * config_backend_flow.c — hermes_cli/config_backend.py FLOW cores + leaves (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/config_backend.py
 *
 * Self-contained, libjson-only. Ports the DECISION sequences of the config-backend
 * selection/routing wrappers (read/version/write/probe/destination flows), the
 * FileBackend path-level leaves, the _ExplicitFile override, the bootstrap-name /
 * deployment-env gates and the contextvar pin/lock managers. Every live effect
 * (stat/open/read, utils signature+yaml helpers, foreign-module imports, the
 * CLAIMED sibling functions, and the selected backend's method surface) enters
 * through ONE universal seam; the cores perform NO I/O.
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object
 * (the seam records a copy and must not free it); the result is a NEW json_t* the
 * caller frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg, ...} == a raised exception. The harness records
 * {"ev": name, "args": args, "ret": result} per call in order; the oracle's
 * recording stubs emit the identical shape driving LIVE Python with the same
 * scripts. BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 * CancelledError, and ConfigBackendUnavailable which subclasses SystemExit) PASS
 * THROUGH every `except OSError` / `except (OSError, NotImplementedError)` site;
 * only the SPECIFIC named except clauses swallow their own class. With no seam
 * installed the dispatcher is fail-closed (returns a RuntimeError raise spec).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. The flows
 * are stateless apart from the contextvar managers, which thread an injected
 * "cv" state object (working copy) and observe it through cm_body / cm_after seams.
 *
 * CLAIMED siblings (ported elsewhere in src/hermes_cli/config_migration_flow.c)
 * are SEAMED here, never re-ported: _route, get_config_backend, _backend_for,
 * _bootstrap, _ensure_bootstrap_env, read_config_doc, write_config_keys,
 * version_token, config_cache_version_or_none, FileBackend.write_path. The
 * unported sibling _refuse_redacted_sets is seamed inside write_changes.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   route                     _route(config_path)                 args{config_path} ret {backend,home}|raise
 *   get_config_backend        get_config_backend()                args{} ret backend-desc|raise
 *   backend_for               _backend_for(kind)                  args{kind} ret backend-desc|raise
 *   bootstrap                 _bootstrap(home,project_env,once=)  args{home,project_env,once} ret null|raise
 *   read_config_doc           read_config_doc(config_path)        args{config_path} ret doc|raise
 *   write_config_keys         write_config_keys(config_path,updates) args{config_path,updates} ret null|raise
 *   write_path                FileBackend.write_path(path,changes) args{path,changes} ret null|raise
 *   refuse_redacted_sets      _refuse_redacted_sets(config_path,sets) args{config_path,sets} ret null|raise
 *   backend_read_user_doc_readonly backend.read_user_doc_readonly(home) args{home} ret doc|raise
 *   backend_version           backend.version(home)               args{home} ret tuple|raise
 *   backend_cache_version     backend.cache_version(home,version) args{home,version} ret tuple|raise
 *   backend_probe_readable    backend.probe_readable(home)        args{home} ret null|raise
 *   backend_exists            backend.exists(home)                args{home} ret bool|raise
 *   backend_write_changes     backend.write_changes(home,changes) args{home,changes} ret null|raise
 *   backend_destination       backend.destination(home)           args{home} ret str|raise
 *   backend_deployment_env_names backend.deployment_env_names()   args{} ret list|raise
 *   backend_supports_file_tooling backend.supports_file_tooling() args{} ret bool|raise
 *   backend_bootstrap_env_names backend.bootstrap_env_names()     args{} ret list|raise
 *   backend_preserve_read_base backend.preserve_read_base()       args{} ret cm-desc|raise
 *   path_stat                 Path.stat()                         args{path} ret stat-obj|raise
 *   file_signature            utils.file_signature(st)            args{st} ret tuple|raise
 *   open                      builtin open(path,mode,encoding=)   args{path,mode,encoding?} ret file-token|raise
 *   fast_safe_load            utils.fast_safe_load(f)             args{file} ret doc|raise
 *   load_yaml_file_readonly   utils.load_yaml_file_readonly(path) args{path} ret doc|raise
 *   file_read                 f.read(size)                        args{file,size} ret data|raise
 *   path_exists               Path.exists()                       args{path} ret bool
 *   worker_config_snapshot    agent.safe_worker_policy.worker_config_snapshot() args{} ret snapshot|null
 *   racy_content_digest       utils.racy_content_digest(path,version) args{path,version} ret digest|raise
 *   get_managed_system        hermes_constants.get_managed_system() args{} ret str|null
 *   container_or_chmod_skipped hermes_constants._container_or_chmod_skipped() args{} ret bool
 *   os_chmod                  os.chmod(path,mode)                 args{path,mode} ret null|raise
 *   cm_body                   contextvar manager body observation args{refused,pinned} ret null
 *   cm_after                  contextvar manager post-reset observation args{refused,pinned} ret null
 *
 * Representation contract (fixture domain, identical on both sides):
 *  - ASCII text; POSIX lexical paths (no symlinks/resolve). Path inputs are
 *    normalized (backslash->slash, trailing slashes stripped) like pathlib.
 *  - integral JSON numbers read as int; a version is a JSON array (tuple).
 *  - a backend descriptor is a JSON object; .name is read from desc["name"].
 *    local_file_layer returns [ {"explicit":path,"name":"file"}, parent ] (a tuple).
 *  - a Changes object is {"document":dict|null,"set":dict,"unset":[str],
 *    "extra_content_on_create":str|null} (canonical key order; defaults filled).
 *  - a file token is the open seam's scripted return (a path string in the
 *    fixtures); fast_safe_load / file_read carry it as "file". The with-block
 *    __exit__ (file close) and preserve_read_base CM enter/exit are no-ops in
 *    this domain and are NOT seamed.
 *  - stat-obj carries st_mode (int) for drop_other_access; file_signature takes
 *    the whole stat-obj. frozenset()/() returns normalize to [] on both sides.
 *  - contextvar state "cv" = {"refuse":[str],"pinned":[str]}; managers append on
 *    enter and reset on exit (finally), observed via cm_body / cm_after.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool jtruth(const json_t *v) {
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
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static char *xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct cb_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cb_seams_t;

static json_t *cb_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool cb_is_raise(const json_t *r) {
    return is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *cb_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: cb_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *cb_call(const cb_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return cb_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *cb_args0(void) { return json_object(); }
static void cb_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void cb_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── exception class catch sets (scripted class names; subclasses folded in) ── */
static bool cb_cls_oserror(const char *c) {
    return !strcmp(c, "OSError") || !strcmp(c, "FileNotFoundError")
        || !strcmp(c, "PermissionError") || !strcmp(c, "IsADirectoryError")
        || !strcmp(c, "NotADirectoryError") || !strcmp(c, "InterruptedError")
        || !strcmp(c, "FileExistsError") || !strcmp(c, "BlockingIOError")
        || !strcmp(c, "TimeoutError") || !strcmp(c, "ConnectionError");
}
static bool cb_cls_notimplemented(const char *c) { return !strcmp(c, "NotImplementedError"); }
static bool cb_cls_backend_unavailable(const char *c) { return !strcmp(c, "ConfigBackendUnavailable"); }

/* ── POSIX lexical path helpers (mirror pathlib normalization) ── */
static char *cb_path_posix(const char *p) {
    if (!p) return strdup("");
    char *s = strdup(p);
    for (char *q = s; *q; q++) if (*q == '\\') *q = '/';
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') s[--n] = '\0';
    return s;
}
static char *cb_path_parent(const char *p) {
    if (!p || !*p) return strdup(".");
    char *tmp = cb_path_posix(p); size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = '\0';
    char *slash = strrchr(tmp, '/');
    char *out;
    if (!slash) out = strdup(".");
    else if (slash == tmp) out = strdup("/");
    else out = strndup(tmp, (size_t)(slash - tmp));
    free(tmp); return out;
}
static char *cb_path_join(const char *a, const char *b) {
    if (!b || !*b) return strdup(a ? a : "");
    if (b[0] == '/') return strdup(b);
    if (!a || !*a) return strdup(b);
    size_t na = strlen(a);
    if (a[na - 1] == '/') return xsf("%s%s", a, b);
    return xsf("%s/%s", a, b);
}
/* config_path(home) = Path(home) / CONFIG_FILENAME */
static char *cb_config_path(const char *home) {
    char *h = cb_path_posix(home ? home : "");
    char *j = cb_path_join(h, "config.yaml");
    free(h);
    return j;
}

/* Python repr() of a str (single-quote preference) or None. Fixture domain:
 * simple ASCII backend names; escapes backslash and the chosen quote. */
static char *cb_repr_name(const json_t *desc) {
    const json_t *n = desc ? json_obj_get(desc, "name") : NULL;
    if (!n || n->type == JSON_NULL) return strdup("None");
    if (!is_str(n)) return strdup("None");
    const char *s = n->str_val ? n->str_val : "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 2 + 3;
    char *out = malloc(cap);
    size_t o = 0;
    out[o++] = q;
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == q) out[o++] = '\\';
        out[o++] = *p;
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}

/* ── Changes canonicalization: {document,set,unset,extra_content_on_create} ── */
static json_t *cb_changes_canon(const json_t *document, const json_t *set,
                                const json_t *unset, const json_t *extra) {
    json_t *ch = json_object();
    json_set(ch, "document", (document && document->type != JSON_NULL) ? json_copy(document) : json_null());
    json_set(ch, "set", is_obj(set) ? json_copy(set) : json_object());
    json_set(ch, "unset", is_arr(unset) ? json_copy(unset) : json_array());
    json_set(ch, "extra_content_on_create", (extra && extra->type != JSON_NULL) ? json_copy(extra) : json_null());
    return ch;
}

/* ── shared route-then-backend-method flow ── */
static json_t *cb_via_route(const cb_seams_t *s, const char *config_path,
                            const char *backend_seam) {
    json_t *args = json_object();
    cb_set_str(args, "config_path", config_path);
    json_t *route = cb_call(s, "route", args);
    if (cb_is_raise(route)) return route;
    json_t *bargs = json_object();
    cb_set_str(bargs, "home", json_get_str(route, "home", ""));
    json_free(route);
    return cb_call(s, backend_seam, bargs);
}

/* ═══════════════════════ FileBackend leaves ═══════════════════════ */

/* PoP: cb_file_config_path @ hermes_cli/config_backend.py:config_path */
json_t *cb_file_config_path(const char *home) {
    char *p = cb_config_path(home);
    json_t *r = json_string(p);
    free(p);
    return r;
}

/* PoP: cb_file_version_path @ hermes_cli/config_backend.py:version_path */
json_t *cb_file_version_path(const cb_seams_t *s, const char *path) {
    char *posix = cb_path_posix(path);
    json_t *args = json_object();
    cb_set_str(args, "path", posix);
    free(posix);
    json_t *st = cb_call(s, "path_stat", args);
    if (cb_is_raise(st)) return st;
    json_t *fargs = json_object();
    json_set(fargs, "st", st ? st : json_null());  /* ownership transfer */
    return cb_call(s, "file_signature", fargs);
}

/* PoP: cb_file_read_path @ hermes_cli/config_backend.py:read_path */
json_t *cb_file_read_path(const cb_seams_t *s, const char *path) {
    char *posix = cb_path_posix(path);
    json_t *args = json_object();
    cb_set_str(args, "path", posix);
    cb_set_str(args, "mode", "r");
    cb_set_str(args, "encoding", "utf-8-sig");
    free(posix);
    json_t *f = cb_call(s, "open", args);
    if (cb_is_raise(f)) return f;
    json_t *largs = json_object();
    json_set(largs, "file", f ? f : json_null());  /* ownership transfer */
    return cb_call(s, "fast_safe_load", largs);
}

/* PoP: cb_file_read_path_readonly @ hermes_cli/config_backend.py:read_path_readonly */
json_t *cb_file_read_path_readonly(const cb_seams_t *s, const char *path) {
    char *posix = cb_path_posix(path);
    json_t *args = json_object();
    cb_set_str(args, "path", posix);
    free(posix);
    return cb_call(s, "load_yaml_file_readonly", args);
}

/* PoP: cb_file_read_user_doc @ hermes_cli/config_backend.py:read_user_doc */
json_t *cb_file_read_user_doc(const cb_seams_t *s, const char *home) {
    char *p = cb_config_path(home);
    json_t *r = cb_file_read_path(s, p);
    free(p);
    return r;
}

/* PoP: cb_file_read_user_doc_readonly @ hermes_cli/config_backend.py:read_user_doc_readonly */
json_t *cb_file_read_user_doc_readonly(const cb_seams_t *s, const char *home) {
    char *p = cb_config_path(home);
    json_t *r = cb_file_read_path_readonly(s, p);
    free(p);
    return r;
}

/* PoP: cb_file_version @ hermes_cli/config_backend.py:version */
json_t *cb_file_version(const cb_seams_t *s, const char *home) {
    char *p = cb_config_path(home);
    json_t *r = cb_file_version_path(s, p);
    free(p);
    return r;
}

/* PoP: cb_file_cache_version @ hermes_cli/config_backend.py:cache_version */
json_t *cb_file_cache_version(const cb_seams_t *s, const char *home, const json_t *version) {
    json_t *snap = cb_call(s, "worker_config_snapshot", cb_args0());
    if (cb_is_raise(snap)) return snap;
    bool frozen = (snap != NULL);  /* is not None */
    json_free(snap);
    json_t *out = is_arr(version) ? json_copy(version) : json_array();
    if (frozen) {
        json_append(out, json_string(""));
        return out;
    }
    char *p = cb_config_path(home);
    json_t *ver = cb_file_version(s, home);  /* self.version(home): the file's own signature */
    if (cb_is_raise(ver)) { free(p); json_free(out); return ver; }
    json_t *dargs = json_object();
    cb_set_str(dargs, "path", p);
    json_set(dargs, "version", ver);  /* ownership transfer */
    free(p);
    json_t *digest = cb_call(s, "racy_content_digest", dargs);
    if (cb_is_raise(digest)) { json_free(out); return digest; }
    json_append(out, digest ? digest : json_null());
    return out;
}

/* PoP: cb_file_exists @ hermes_cli/config_backend.py:exists */
json_t *cb_file_exists(const cb_seams_t *s, const char *home) {
    char *p = cb_config_path(home);
    json_t *args = json_object();
    cb_set_str(args, "path", p);
    free(p);
    return cb_call(s, "path_exists", args);
}

/* PoP: cb_file_probe_readable @ hermes_cli/config_backend.py:probe_readable */
json_t *cb_file_probe_readable(const cb_seams_t *s, const char *home) {
    char *p = cb_config_path(home);
    json_t *args = json_object();
    cb_set_str(args, "path", p);
    cb_set_str(args, "mode", "rb");
    free(p);
    json_t *f = cb_call(s, "open", args);
    if (cb_is_raise(f)) return f;
    json_t *rargs = json_object();
    json_set(rargs, "file", f ? f : json_null());  /* ownership transfer */
    json_set(rargs, "size", json_number(1));
    json_t *r = cb_call(s, "file_read", rargs);
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;  /* probe_readable returns None */
}

/* PoP: cb_file_write_changes @ hermes_cli/config_backend.py:write_changes */
json_t *cb_file_write_changes(const cb_seams_t *s, const char *home, const json_t *changes) {
    char *p = cb_config_path(home);
    json_t *args = json_object();
    cb_set_str(args, "path", p);
    free(p);
    json_set(args, "changes", cb_changes_canon(
        json_obj_get(changes, "document"), json_obj_get(changes, "set"),
        json_obj_get(changes, "unset"), json_obj_get(changes, "extra_content_on_create")));
    json_t *r = cb_call(s, "write_path", args);  /* write_path is CLAIMED -> seam */
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: cb_file_locked @ hermes_cli/config_backend.py:locked */
json_t *cb_file_locked(const char *home, const char *dotted) {
    (void)home; (void)dotted;
    return NULL;  /* a local file has no levels */
}

/* duplicate trivial concrete; single PoP claim lives on cb_supports_file_tooling_flow */
json_t *cb_file_supports_file_tooling(void) { return json_bool(true); }

/* PoP: cb_file_honors_managed_config @ hermes_cli/config_backend.py:honors_managed_config */
json_t *cb_file_honors_managed_config(void) { return json_bool(true); }

/* PoP: cb_file_boot @ hermes_cli/config_backend.py:boot */
json_t *cb_file_boot(const char *home) { (void)home; return NULL; }

/* PoP: cb_file_protected_env_names @ hermes_cli/config_backend.py:protected_env_names */
json_t *cb_file_protected_env_names(void) { return json_array(); }

/* PoP: cb_file_deployment_env_names @ hermes_cli/config_backend.py:deployment_env_names */
json_t *cb_file_deployment_env_names(void) { return json_array(); }

/* PoP: cb_file_bootstrap_env_names @ hermes_cli/config_backend.py:bootstrap_env_names */
json_t *cb_file_bootstrap_env_names(void) { return json_array(); }

/* PoP: cb_file_preserve_read_base @ hermes_cli/config_backend.py:preserve_read_base */
json_t *cb_file_preserve_read_base(void) {
    json_t *o = json_object();
    cb_set_str(o, "__cm__", "nullcontext");
    return o;
}

/* PoP: cb_file_doctor_warnings @ hermes_cli/config_backend.py:doctor_warnings */
json_t *cb_file_doctor_warnings(const char *home) { (void)home; return json_array(); }

/* PoP: cb_file_apply_in_memory @ hermes_cli/config_backend.py:apply_in_memory */
json_t *cb_file_apply_in_memory(const char *home, const json_t *doc) {
    (void)home; (void)doc;
    return cb_raise("NotImplementedError", "the file backend persists migrations to config.yaml");
}

/* PoP: cb_file_describe @ hermes_cli/config_backend.py:describe */
json_t *cb_file_describe(const char *home) {
    char *p = cb_config_path(home);
    json_t *r = json_string(p);
    free(p);
    return r;
}

/* PoP: cb_file_destination @ hermes_cli/config_backend.py:destination */
json_t *cb_file_destination(const char *home) {
    char *p = cb_config_path(home);
    json_t *r = json_string(p);
    free(p);
    return r;
}

/* ═══════════════════════ _ExplicitFile leaf ═══════════════════════ */

/* PoP: cb_explicit_config_path @ hermes_cli/config_backend.py:config_path */
json_t *cb_explicit_config_path(const char *path, const char *home) {
    (void)home;  /* _ExplicitFile.config_path ignores home, returns self._path */
    char *p = cb_path_posix(path);
    json_t *r = json_string(p);
    free(p);
    return r;
}

/* ═══════════════════════ module-level FLOW cores ═══════════════════════ */

/* PoP: cb_drop_other_access @ hermes_cli/config_backend.py:drop_other_access */
json_t *cb_drop_other_access(const cb_seams_t *s, const char *path) {
    json_t *managed = cb_call(s, "get_managed_system", cb_args0());
    if (cb_is_raise(managed)) return managed;
    bool skip = (managed != NULL);  /* get_managed_system() is not None */
    json_free(managed);
    if (!skip) {
        json_t *cont = cb_call(s, "container_or_chmod_skipped", cb_args0());
        if (cb_is_raise(cont)) return cont;
        skip = jtruth(cont);
        json_free(cont);
    }
    if (skip) return NULL;
    char *posix = cb_path_posix(path);
    json_t *sargs = json_object();
    cb_set_str(sargs, "path", posix);
    json_t *st = cb_call(s, "path_stat", sargs);
    if (cb_is_raise(st)) {
        const char *c = cb_raise_class(st);
        if (cb_cls_oserror(c) || cb_cls_notimplemented(c)) { json_free(st); free(posix); return NULL; }
        free(posix);
        return st;  /* BaseException / other passes through */
    }
    long mode = 0;
    if (is_obj(st)) {
        const json_t *m = json_obj_get(st, "st_mode");
        if (is_num(m)) mode = (long)m->num_val;
    }
    json_free(st);
    mode &= 07777;
    if (mode & 0007) {
        json_t *cargs = json_object();
        cb_set_str(cargs, "path", posix);
        json_set(cargs, "mode", json_number((double)(mode & ~0007)));
        json_t *r = cb_call(s, "os_chmod", cargs);
        if (cb_is_raise(r)) {
            const char *c = cb_raise_class(r);
            if (cb_cls_oserror(c) || cb_cls_notimplemented(c)) { json_free(r); free(posix); return NULL; }
            free(posix);
            return r;
        }
        json_free(r);
    }
    free(posix);
    return NULL;
}

/* PoP: cb_bootstrap_names @ hermes_cli/config_backend.py:_bootstrap_names */
json_t *cb_bootstrap_names(const cb_seams_t *s, const char *kind) {
    json_t *args = json_object();
    cb_set_str(args, "kind", kind);
    json_t *b = cb_call(s, "backend_for", args);
    if (cb_is_raise(b)) {
        if (cb_cls_backend_unavailable(cb_raise_class(b))) { json_free(b); return json_array(); }
        return b;  /* other (incl. BaseException) passes through */
    }
    json_free(b);
    return cb_call(s, "backend_bootstrap_env_names", cb_args0());
}

/* PoP: cb_bootstrap_deployment @ hermes_cli/config_backend.py:bootstrap_deployment */
json_t *cb_bootstrap_deployment(const cb_seams_t *s, const char *home, const char *project_env) {
    json_t *args = json_object();
    if (home) { char *p = cb_path_posix(home); cb_set_str(args, "home", p); free(p); }
    else json_set(args, "home", json_null());
    if (project_env) { char *p = cb_path_posix(project_env); cb_set_str(args, "project_env", p); free(p); }
    else json_set(args, "project_env", json_null());
    json_set(args, "once", json_bool(false));
    json_t *r = cb_call(s, "bootstrap", args);
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: cb_local_file_layer @ hermes_cli/config_backend.py:local_file_layer */
json_t *cb_local_file_layer(const char *config_path) {
    char *posix = cb_path_posix(config_path);
    char *parent = cb_path_parent(posix);
    json_t *desc = json_object();
    cb_set_str(desc, "explicit", posix);
    cb_set_str(desc, "name", "file");
    json_t *out = json_array();  /* live returns a (backend, home) tuple -> array */
    json_append(out, desc);
    json_append(out, json_string(parent));
    free(posix); free(parent);
    return out;
}

/* PoP: cb_read_config_doc_readonly @ hermes_cli/config_backend.py:read_config_doc_readonly */
json_t *cb_read_config_doc_readonly(const cb_seams_t *s, const char *config_path) {
    return cb_via_route(s, config_path, "backend_read_user_doc_readonly");
}

/* PoP: cb_config_version @ hermes_cli/config_backend.py:config_version */
json_t *cb_config_version(const cb_seams_t *s, const char *config_path) {
    return cb_via_route(s, config_path, "backend_version");
}

/* PoP: cb_config_cache_version @ hermes_cli/config_backend.py:config_cache_version */
json_t *cb_config_cache_version(const cb_seams_t *s, const char *config_path) {
    json_t *ver = cb_config_version(s, config_path);  /* the one stat; errors stay config_version's */
    if (cb_is_raise(ver)) return ver;
    json_t *args = json_object();
    cb_set_str(args, "config_path", config_path);
    json_t *route = cb_call(s, "route", args);
    if (cb_is_raise(route)) { json_free(ver); return route; }
    json_t *bargs = json_object();
    cb_set_str(bargs, "home", json_get_str(route, "home", ""));
    json_set(bargs, "version", ver);  /* ownership transfer */
    json_free(route);
    return cb_call(s, "backend_cache_version", bargs);
}

/* PoP: cb_config_version_or_none @ hermes_cli/config_backend.py:config_version_or_none */
json_t *cb_config_version_or_none(const cb_seams_t *s, const char *config_path) {
    json_t *r = cb_config_version(s, config_path);
    if (cb_is_raise(r)) {
        if (cb_cls_oserror(cb_raise_class(r))) { json_free(r); return NULL; }
        return r;  /* BaseException / other passes through */
    }
    return r;
}

/* PoP: cb_probe_config_readable @ hermes_cli/config_backend.py:probe_config_readable */
json_t *cb_probe_config_readable(const cb_seams_t *s, const char *config_path) {
    json_t *r = cb_via_route(s, config_path, "backend_probe_readable");
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: cb_config_exists @ hermes_cli/config_backend.py:config_exists */
json_t *cb_config_exists(const cb_seams_t *s, const char *config_path) {
    return cb_via_route(s, config_path, "backend_exists");
}

/* PoP: cb_write_config_document @ hermes_cli/config_backend.py:write_config_document */
json_t *cb_write_config_document(const cb_seams_t *s, const char *config_path,
                                 const json_t *document, const json_t *extra) {
    json_t *args = json_object();
    cb_set_str(args, "config_path", config_path);
    json_t *route = cb_call(s, "route", args);
    if (cb_is_raise(route)) return route;
    json_t *bargs = json_object();
    cb_set_str(bargs, "home", json_get_str(route, "home", ""));
    json_set(bargs, "changes", cb_changes_canon(document, NULL, NULL, extra));
    json_free(route);
    json_t *r = cb_call(s, "backend_write_changes", bargs);
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: cb_config_destination @ hermes_cli/config_backend.py:config_destination */
json_t *cb_config_destination(const cb_seams_t *s, const char *config_path) {
    return cb_via_route(s, config_path, "backend_destination");
}

/* PoP: cb_write_config_key @ hermes_cli/config_backend.py:write_config_key */
json_t *cb_write_config_key(const cb_seams_t *s, const char *config_path,
                            const char *key_path, const json_t *value) {
    json_t *updates = json_object();
    json_set(updates, key_path, value ? json_copy(value) : json_null());
    json_t *args = json_object();
    cb_set_str(args, "config_path", config_path);
    json_set(args, "updates", updates);  /* ownership transfer */
    json_t *r = cb_call(s, "write_config_keys", args);  /* CLAIMED -> seam */
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: cb_keeping_read_base @ hermes_cli/config_backend.py:keeping_read_base */
json_t *cb_keeping_read_base(const cb_seams_t *s, const char *config_path) {
    json_t *desc = cb_call(s, "get_config_backend", cb_args0());
    if (cb_is_raise(desc)) return desc;
    json_free(desc);
    json_t *cm = cb_call(s, "backend_preserve_read_base", cb_args0());
    if (cb_is_raise(cm)) return cm;
    json_free(cm);  /* CM enter/exit are no-ops in this domain */
    json_t *rargs = json_object();
    cb_set_str(rargs, "config_path", config_path);
    return cb_call(s, "read_config_doc", rargs);  /* read() == read_config_doc(config_path) */
}

/* PoP: cb_write_changes_flow @ hermes_cli/config_backend.py:write_changes */
json_t *cb_write_changes_flow(const cb_seams_t *s, const char *config_path, const json_t *changes) {
    json_t *sets = json_obj_get(changes, "set");
    json_t *rargs = json_object();
    cb_set_str(rargs, "config_path", config_path);
    json_set(rargs, "sets", is_obj(sets) ? json_copy(sets) : json_object());
    json_t *ref = cb_call(s, "refuse_redacted_sets", rargs);  /* unported sibling -> seam */
    if (cb_is_raise(ref)) return ref;
    json_free(ref);
    json_t *args = json_object();
    cb_set_str(args, "config_path", config_path);
    json_t *route = cb_call(s, "route", args);
    if (cb_is_raise(route)) return route;
    json_t *bargs = json_object();
    cb_set_str(bargs, "home", json_get_str(route, "home", ""));
    json_set(bargs, "changes", cb_changes_canon(
        json_obj_get(changes, "document"), json_obj_get(changes, "set"),
        json_obj_get(changes, "unset"), json_obj_get(changes, "extra_content_on_create")));
    json_free(route);
    json_t *r = cb_call(s, "backend_write_changes", bargs);
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: cb_deployment_env_names_flow @ hermes_cli/config_backend.py:deployment_env_names */
json_t *cb_deployment_env_names_flow(const cb_seams_t *s) {
    json_t *desc = cb_call(s, "get_config_backend", cb_args0());
    if (cb_is_raise(desc)) return desc;
    json_free(desc);
    return cb_call(s, "backend_deployment_env_names", cb_args0());
}

/* PoP: cb_supports_file_tooling_flow @ hermes_cli/config_backend.py:supports_file_tooling */
json_t *cb_supports_file_tooling_flow(const cb_seams_t *s) {
    json_t *desc = cb_call(s, "get_config_backend", cb_args0());
    if (cb_is_raise(desc)) return desc;
    json_free(desc);
    return cb_call(s, "backend_supports_file_tooling", cb_args0());
}

/* PoP: cb_require_file_tooling @ hermes_cli/config_backend.py:require_file_tooling */
json_t *cb_require_file_tooling(const cb_seams_t *s, const char *what) {
    json_t *r = cb_supports_file_tooling_flow(s);
    if (cb_is_raise(r)) return r;
    bool supported = jtruth(r);
    json_free(r);
    if (supported) return NULL;
    json_t *desc = cb_call(s, "get_config_backend", cb_args0());
    if (cb_is_raise(desc)) return desc;
    char *name = cb_repr_name(desc);
    json_free(desc);
    char *msg = xsf("%s copies the local config.yaml, and the %s config "
                    "backend has none. It is not available with this backend.",
                    what ? what : "", name);
    free(name);
    json_t *raise = cb_raise("ConfigBackendUnavailable", msg);
    free(msg);
    return raise;
}

/* ═══════════════════════ contextvar managers + getters ═══════════════════════ */

static json_t *cv_get(const json_t *cv, const char *key) {
    const json_t *v = cv ? json_obj_get(cv, key) : NULL;
    return is_arr(v) ? json_copy(v) : json_array();
}
static void cv_set(json_t *cv, const char *key, json_t *arr) {
    json_set(cv, key, arr);  /* ownership transfer */
}
static json_t *cv_concat(const json_t *base, const json_t *extra) {
    json_t *out = is_arr(base) ? json_copy(base) : json_array();
    if (is_arr(extra)) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_append(out, json_copy(extra->c.items[i]));
    }
    return out;
}
static json_t *cv_observe(const cb_seams_t *s, const char *ev, const json_t *cv) {
    json_t *args = json_object();
    json_set(args, "refused", cv_get(cv, "refuse"));
    json_set(args, "pinned", cv_get(cv, "pinned"));
    json_t *r = cb_call(s, ev, args);
    if (cb_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: cb_refusing_locked_omissions @ hermes_cli/config_backend.py:refusing_locked_omissions */
json_t *cb_refusing_locked_omissions(const cb_seams_t *s, const json_t *prefixes, json_t *cv) {
    json_t *old = cv_get(cv, "refuse");
    cv_set(cv, "refuse", cv_concat(old, prefixes));
    json_t *body = cv_observe(s, "cm_body", cv);
    /* finally: reset regardless of a body raise */
    cv_set(cv, "refuse", old);  /* ownership transfer of old */
    if (cb_is_raise(body)) return body;
    json_t *after = cv_observe(s, "cm_after", cv);
    if (cb_is_raise(after)) return after;
    return NULL;
}

/* PoP: cb_pinning_keys @ hermes_cli/config_backend.py:pinning_keys */
json_t *cb_pinning_keys(const cb_seams_t *s, const json_t *keys, json_t *cv) {
    json_t *old = cv_get(cv, "pinned");
    cv_set(cv, "pinned", cv_concat(old, keys));
    json_t *body = cv_observe(s, "cm_body", cv);
    cv_set(cv, "pinned", old);
    if (cb_is_raise(body)) return body;
    json_t *after = cv_observe(s, "cm_after", cv);
    if (cb_is_raise(after)) return after;
    return NULL;
}

/* PoP: cb_refused_locked_omission_prefixes @ hermes_cli/config_backend.py:refused_locked_omission_prefixes */
json_t *cb_refused_locked_omission_prefixes(const json_t *cv) {
    return cv_get(cv, "refuse");
}

/* PoP: cb_pinned_keys @ hermes_cli/config_backend.py:pinned_keys */
json_t *cb_pinned_keys(const json_t *cv) {
    return cv_get(cv, "pinned");
}