/*
 * tui_methods_projects_flow.c -- FLOW + leaf port of tui_gateway/methods_projects.py
 * (hermes-agent): the Projects RPC surface -- per-profile multi-folder workspaces,
 * repo discovery and the sidebar-tree input gathering.
 *
 * Ports the DECISION SEQUENCES -- branch order, error-code/message composition,
 * seam call order, dict-literal evaluation order, module-global state transitions
 * (_DIR_EXISTS_CACHE / the deferred HandlerRegistry pending list) and exception
 * paths. Every live external call enters through ONE universal seam:
 * tmp_call(ctx, name, args). The harness wires it to a recording stub fed the
 * case's scripted results (in.io); the oracle installs recording stubs emitting
 * the identical event shape {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tmp_-prefixed.
 *
 * The module's functions are rebound onto server.py's globals at install time
 * (method_ctx.bind_module), so _err / _ok / _load_cfg / _completion_cwd /
 * _persistence_for / logger / git_probe / os / json / contextlib are bare free
 * variables. _err / _ok are CLAIMED server globals -- seamed, never re-ported.
 * os / json / contextlib are modeled as doubles: the PURE posixpath algorithms
 * (normcase/join/dirname/basename/abspath/normpath) run REAL on both sides
 * (POSIX fixture corpus, os.sep == "/"), while the filesystem probes
 * (realpath/expanduser/isdir) and os.walk are seams. json.dumps(sort_keys=True,
 * separators=(",",":")) is transcribed (tmp_policy_key) over the ASCII corpus;
 * contextlib.nullcontext is silent on both sides.
 *
 * Decorated handlers (def _ under @_projects_method) are ported BARE (registry
 * decorators stripped): tmp_projects_list / tmp_projects_get / tmp_projects_create
 * / tmp_projects_archive / tmp_projects_delete / tmp_projects_set_active /
 * tmp_projects_for_cwd, driven with (rid, params, conn) and a scripted pdb double.
 * The decorator factories themselves are ported as flows: tmp_projects_method
 * (name + wrapped-fn callback seam "projects_fn") and tmp_register_project_mutator
 * (suffix/fn_name/takes_path + kwargs_of callback seam); both append the deferred
 * registration [name, "handler"] to state._registry_pending exactly when the
 * live @method(name) decorator fires, then drive the produced handler (including
 * the _NoProject/ValueError/Exception -> 5062/5063/5061 error mapping).
 *
 * Ported siblings compose LIVE on both sides (each is exercised by its own
 * cases): _pick inside create; _require_project inside get/archive/delete/
 * set_active/mutator; _projects_payload inside list/archive/delete;
 * _non_workspace_dirs inside _is_repo_junk/_is_session_cwd_junk;
 * _repo_discovery_policy/_repo_discovery_policy_key inside is_default /
 * _project_tree_inputs / the scan's record call; _project_tree_row inside
 * _project_tree_inputs; _discover_repos_payload inside _project_tree_inputs;
 * _project_tree_inputs inside _build_project_tree.
 *
 * register(server) is install plumbing (bind_module over globals()) and is NOT
 * ported; the _NoProject class is extracted live in the oracle and modeled here
 * as the raise kind "_NoProject" (str(_NoProject()) == "").
 *
 * Seam name -> live call (args are informational projections):
 *   ok                          _ok(rid, result)  [claimed server global]
 *   err                         _err(rid, code, msg)  [claimed server global]
 *   load_cfg                    _load_cfg()  [server global]
 *   completion_cwd              _completion_cwd(arg_dict)  [server global]
 *   persistence_for             _persistence_for(db) -> YIELDED handle (CM silent)
 *   logger.debug                logger.debug(fmt, *args, exc_info=True|absent)
 *                               args = {"fmt", "args": [positionals],
 *                               "exc_info": true only when passed}
 *   git_probe.branch            git_probe.branch(cwd)
 *   git_probe.warm_roots        git_probe.warm_roots(iterable) -> {"roots": list}
 *   git_probe.common_repo_root  git_probe.common_repo_root(cwd)
 *   os.path.expanduser          os.path.expanduser(path)
 *   os.path.realpath            os.path.realpath(path)
 *   os.path.isdir               os.path.isdir(path)
 *   os.walk                     os.walk(root) -> scripted TREE node
 *                               {"dirs": {name: subtree}, "files": [names]};
 *                               the double walks it top-down honoring the
 *                               caller's dirnames[:] pruning exactly as os.walk
 *                               (yield order = dict insertion order). A scripted
 *                               raise fires at the os.walk(root) call site.
 *   get_hermes_home             hermes_constants.get_hermes_home()
 *   default_config              hermes_cli.config.DEFAULT_CONFIG (module-attr
 *                               import seam; the live import reads the attribute)
 *   connect_closing             pdb.connect_closing() -> YIELDED conn (CM silent)
 *   pdb.list_projects           pdb.list_projects(conn, include_archived=...)
 *   pdb.get_active_id           pdb.get_active_id(conn)
 *   pdb.get_project             pdb.get_project(conn, id) -> project double
 *   to_dict                     project_double.to_dict()  (args {})
 *   pdb.create_project          pdb.create_project(conn, name=, folders=, **pick)
 *   pdb.set_active              pdb.set_active(conn, id)
 *   pdb.archive_project         pdb.archive_project(conn, id)
 *   pdb.restore_project         pdb.restore_project(conn, id)
 *   pdb.delete_project          pdb.delete_project(conn, id)
 *   pdb.project_for_path        pdb.project_for_path(conn, cwd) -> project double
 *   pdb.mutator                 getattr(pdb, fn_name)(conn, id, *args, **kwargs)
 *                               args = {"fn_name", "conn", "id", "args": [path?],
 *                               "kwargs": kwargs_of(params)}
 *   pdb.record_discovered_repos pdb.record_discovered_repos(conn, pairs,
 *                               replace=, policy_key=)
 *   pdb.list_discovered_repos   pdb.list_discovered_repos(conn) -> list[dict]
 *   pdb.reconcile_discovered_repos_policy
 *                               pdb.reconcile_discovered_repos_policy(conn,
 *                               policy_key, preserve_unversioned=)
 *   record_feature_used         shared_metrics_signals.record_feature_used("projects")
 *   SessionQuery                SessionQuery(**kwargs) ctor; args = the kwargs;
 *                               the double projects back to them in list_sessions
 *   SessionWindow               SessionWindow(limit=, offset=) ctor
 *   persistence.list_sessions   persistence.sessions.list_sessions(query, window,
 *                               compact_rows=True) -> list of entry dicts
 *   persistence.distinct_session_cwds
 *                               persistence.sessions.distinct_session_cwds()
 *   persistence.backfill_repo_roots
 *                               persistence.sessions.backfill_repo_roots(map)
 *   project_tree.build_tree     project_tree.build_tree(projects, sessions,
 *                               discovered, git_probe.resolve, preview_limit=,
 *                               hydrate=, is_junk_root=, is_junk_cwd=, exists=)
 *                               [callables project to their name markers]
 *   projects_fn                 the fn wrapped by _projects_method:
 *                               fn(rid, params, pdb, conn) -> {"rid","params","conn"}
 *   kwargs_of                   the mutator kwargs callback: kwargs_of(params)
 *
 * Representation contract (shared with tests/oracle_tui_methods_projects_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException-only
 *   kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError,
 *   BaseExceptionGroup, BaseException) PASS THROUGH every except-Exception site
 *   (full builtin parent table below). Unknown custom kinds default to Exception
 *   subclasses, exactly as the oracle's make_exc builds them.
 * - Scripted raises of plain kinds carry str(exc) == message verbatim; KeyError
 *   messages are repr(key) exactly as str(KeyError(key)).
 * - Object doubles (project / entry / row / persistence) are scripted dicts read
 *   as attributes: a MISSING key raises AttributeError "'types.SimpleNamespace'
 *   object has no attribute '<key>'"; a null receiver raises the NoneType form.
 *   The persistence double ALWAYS carries its seamed sessions sub-object, so
 *   persistence scripts carry a "sessions": {} presence marker alongside
 *   read_only. An EMPTY dict project double is TRUTHY (SimpleNamespace(**{}) is
 *   truthy); only null is falsy for the `if proj` gates in create/for_cwd.
 * - Tuples project to lists; sets to sorted lists; integral floats to ints.
 * - str()-projected values (params id/name/path/cwd, row.cwd, entry root, folder
 *   path, get_hermes_home) are str|int|null in the fixture corpus; str() folds
 *   ASCII only. int()/float() coercion accepts numbers, bools and simple numeric
 *   strings (ASCII whitespace padding); the ValueError messages are transcribed.
 * - Fixture paths are absolute POSIX (no drive letters/UNC); posixpath.abspath
 *   is pure on them. normcase is the POSIX identity.
 * - Seam args drop None-valued keys on both sides (absent-vs-null parity).
 * - State: ctx.state mirrors _DIR_EXISTS_CACHE (dict; _dir_exists_cached /
 *   _build_project_tree) and _registry_pending (list of [name, "handler"];
 *   _projects_method / _register_project_mutator). Functions that touch them
 *   materialize their keys at entry; other functions leave state alone.
 *
 * Ownership: tmp_call ADOPTS args into the trace event; its return is a fresh
 * owned node. json_set ADOPTS its value and frees the replaced node -- borrowed
 * pointers to a replaced key are copied first. Seam results are caller-freed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct tmp_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tmp_ctx_t;

/* ── basic helpers ── */
static bool tmp_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *tmp_raise_kind(const json_t *v) {
    return tmp_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

static const char *tmp_raise_msg(const json_t *v) {
    return tmp_is_raise(v) ? json_get_str(v, "message", "") : "";
}

/* Full builtin exception parent table (py3.14). A kind is caught by
 * `except Exception` iff walking its parent chain reaches Exception.
 * BaseException-only kinds pass through. Unknown kinds default to Exception
 * (the oracle's make_exc builds type(kind, (Exception,), {})). */
typedef struct { const char *kind; const char *parent; } tmp_exc_parent_t;
static const tmp_exc_parent_t TMP_EXC_PARENTS[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"ExceptionGroup", "Exception"},
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
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"IOError", "OSError"},             /* py3.14 OSError aliases */
    {"EnvironmentError", "OSError"},
    {"WindowsError", "OSError"},
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
    {"PythonFinalizationError", "RuntimeError"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"_NoProject", "Exception"},
};

static const char *tmp_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(TMP_EXC_PARENTS) / sizeof(TMP_EXC_PARENTS[0]); i++)
        if (!strcmp(TMP_EXC_PARENTS[i].kind, kind)) return TMP_EXC_PARENTS[i].parent;
    return "Exception"; /* unknown custom kind: make_exc default */
}

/* walk the chain: does `except <ancestor>` catch this kind? */
static bool tmp_exc_is_a(const char *kind, const char *ancestor) {
    if (!kind) return false;
    const char *k = kind;
    for (int depth = 0; k && depth < 32; depth++) {
        if (!strcmp(k, ancestor)) return true;
        k = tmp_exc_parent(k);
    }
    return false;
}

static bool tmp_exc_caught(const char *kind) {
    return tmp_exc_is_a(kind, "Exception");
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tmp_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *tmp_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *tmp_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python type name for a JSON value (fixture scalar/container domain). */
static const char *tmp_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == (double)(long long)d) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static char *tmp_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* Python str() for the fixture scalar domain (str|int|null at str() sites). */
static char *tmp_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tmp_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tmp_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tmp_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return tmp_strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* Python repr() of an ASCII str (for KeyError / int / float messages). */
static char *tmp_pyrepr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *o = malloc(cap);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = quote;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)quote || c == '\\') { o[k++] = '\\'; o[k++] = (char)c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            k += (size_t)snprintf(o + k, cap - k, "\\x%02x", c);
        } else o[k++] = (char)c;
    }
    o[k++] = quote;
    o[k] = '\0';
    return o;
}

static bool tmp_is_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip(): ASCII whitespace both ends (fixtures fold ASCII only). */
static char *tmp_strip(const char *s) {
    if (!s) s = "";
    while (*s && tmp_is_ascii_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tmp_is_ascii_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.rstrip(chars): strip any trailing char in the set. */
static char *tmp_rstrip(const char *s, const char *chars) {
    if (!s) s = "";
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static bool tmp_startswith(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* ── POSIX path algorithms (pure; fixture paths are absolute POSIX) ── */
static bool tmp_posix_isabs(const char *s) { return s && s[0] == '/'; }

/* posixpath.normpath (POSIX): collapse //, ., ..; keep exactly-two leading /. */
static char *tmp_posix_normpath(const char *s) {
    if (!s || !*s) return tmp_strdup(".");
    bool absolute = s[0] == '/';
    bool double_lead = absolute && s[1] == '/' && s[2] != '/';
    /* split into components, resolve . / .. with a stack */
    size_t n = strlen(s);
    char **stack = malloc((n + 1) * sizeof(char *));
    if (!stack) return NULL;
    size_t top = 0;
    size_t i = 0;
    while (i < n) {
        while (i < n && s[i] == '/') i++;
        size_t j = i;
        while (j < n && s[j] != '/') j++;
        if (j == i) break;
        size_t len = j - i;
        if (len == 1 && s[i] == '.') { i = j; continue; }
        if (len == 2 && s[i] == '.' && s[i + 1] == '.') {
            if (top > 0 && !(stack[top - 1][0] == '.' && stack[top - 1][1] == '.'
                             && stack[top - 1][2] == '\0')) {
                free(stack[--top]);
            } else if (!absolute) {
                stack[top++] = tmp_strdup("..");
            }
            i = j;
            continue;
        }
        char *comp = malloc(len + 1);
        if (!comp) { for (size_t k = 0; k < top; k++) free(stack[k]); free(stack); return NULL; }
        memcpy(comp, s + i, len);
        comp[len] = '\0';
        stack[top++] = comp;
        i = j;
    }
    size_t cap = n + 3;
    char *o = malloc(cap);
    if (!o) { for (size_t k = 0; k < top; k++) free(stack[k]); free(stack); return NULL; }
    size_t k = 0;
    if (absolute) { o[k++] = '/'; if (double_lead) o[k++] = '/'; }
    for (size_t c = 0; c < top; c++) {
        if (c > 0) o[k++] = '/';
        size_t l = strlen(stack[c]);
        memcpy(o + k, stack[c], l);
        k += l;
        free(stack[c]);
    }
    free(stack);
    if (k == 0) o[k++] = '.';
    o[k] = '\0';
    return o;
}

/* posixpath.join for two components (the only arity the ported code uses). */
static char *tmp_posix_join2(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (tmp_posix_isabs(b)) return tmp_strdup(b);
    size_t la = strlen(a), lb = strlen(b);
    bool need_sep = la > 0 && a[la - 1] != '/';
    char *o = malloc(la + lb + 2);
    if (!o) return NULL;
    memcpy(o, a, la);
    size_t k = la;
    if (need_sep) o[k++] = '/';
    memcpy(o + k, b, lb + 1);
    return o;
}

/* posixpath.dirname (POSIX): head through the last slash; rstrip slashes
 * UNLESS the head is all slashes (then keep it verbatim, incl. "//"). */
static char *tmp_posix_dirname(const char *s) {
    if (!s) s = "";
    const char *slash = strrchr(s, '/');
    if (!slash) return tmp_strdup("");
    size_t i = (size_t)(slash - s) + 1;
    bool all_slashes = true;
    for (size_t k = 0; k < i; k++)
        if (s[k] != '/') { all_slashes = false; break; }
    size_t n = i;
    if (!all_slashes)
        while (n > 0 && s[n - 1] == '/') n--;
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* posixpath.basename (POSIX). */
static char *tmp_posix_basename(const char *s) {
    if (!s) s = "";
    const char *slash = strrchr(s, '/');
    return tmp_strdup(slash ? slash + 1 : s);
}

/* posixpath.abspath: pure on absolute fixture paths (normpath only). */
static char *tmp_posix_abspath(const char *s) {
    return tmp_posix_normpath(s);
}

/* ── the one universal seam (fail-closed) ── */
static json_t *tmp_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return tmp_err_spec(type, msg);
}

static json_t *tmp_call(tmp_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    /* drop null-valued arg keys on both sides (absent-vs-null parity) */
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tmp_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tmp_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = tmp_make_exc(raise);
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

/* args builders (values borrowed; copied into the args object) */
static json_t *tmp_args0(void) { return json_object(); }
static void tmp_aset(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static void tmp_asets(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tmp_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tmp_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* _ok(rid, result) -- result ADOPTED. */
static json_t *tmp_ok(tmp_ctx_t *ctx, const json_t *rid, json_t *result) {
    json_t *a = tmp_args0();
    tmp_aset(a, "rid", rid);
    json_set(a, "result", result ? result : json_null());
    return tmp_call(ctx, "ok", a);
}

/* _err(rid, code, msg). */
static json_t *tmp_err(tmp_ctx_t *ctx, const json_t *rid, int code, const char *msg) {
    json_t *a = tmp_args0();
    tmp_aset(a, "rid", rid);
    tmp_asetn(a, "code", (double)code);
    tmp_asets(a, "message", msg);
    return tmp_call(ctx, "err", a);
}

/* logger.debug(fmt, *args, exc_info=True) -- args array ADOPTED (NULL = none). */
static json_t *tmp_log_debug(tmp_ctx_t *ctx, const char *fmt, json_t *args, bool exc_info) {
    json_t *a = tmp_args0();
    tmp_asets(a, "fmt", fmt);
    json_set(a, "args", args ? args : json_array());
    if (exc_info) tmp_asetb(a, "exc_info", true);
    return tmp_call(ctx, "logger.debug", a);
}

/* ── dict subscript / attribute access with live error surfaces ── */
/* obj[key] on a dict double: KeyError repr(key) when missing; TypeError when
 * the receiver is not a dict. Returns a BORROWED node (NULL only on raise). */
static const json_t *tmp_subscript(const json_t *obj, const char *key, json_t **raise_out) {
    *raise_out = NULL;
    if (!obj || obj->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not subscriptable", tmp_type_name(obj));
        *raise_out = tmp_err_spec("TypeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) {
        char *rk = tmp_pyrepr_str(key);
        *raise_out = tmp_err_spec("KeyError", rk ? rk : "");
        free(rk);
        return NULL;
    }
    return v;
}

/* attribute read on an object double (SimpleNamespace): missing key raises
 * AttributeError; a null receiver raises the NoneType form. BORROWED result. */
static const json_t *tmp_attr(const json_t *obj, const char *key, json_t **raise_out) {
    *raise_out = NULL;
    if (obj && obj->type == JSON_OBJECT) {
        const json_t *v = json_obj_get(obj, key);
        if (!v) {
            char msg[192];
            snprintf(msg, sizeof msg,
                     "'types.SimpleNamespace' object has no attribute '%s'", key);
            *raise_out = tmp_err_spec("AttributeError", msg);
            return NULL;
        }
        return v;
    }
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tmp_type_name(obj), key);
    *raise_out = tmp_err_spec("AttributeError", msg);
    return NULL;
}

/* dict.get(key): non-dict receiver raises AttributeError 'get'; a missing key
 * returns NULL (== None). BORROWED result. */
static const json_t *tmp_dict_get(const json_t *obj, const char *key, json_t **raise_out) {
    *raise_out = NULL;
    if (!obj || obj->type != JSON_OBJECT) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmp_type_name(obj));
        *raise_out = tmp_err_spec("AttributeError", msg);
        return NULL;
    }
    return json_obj_get(obj, key);
}

/* ── int()/float() coercion (fixture domain: number|bool|numeric str|null) ── */
static json_t *tmp_py_int(const json_t *v, json_t **raise_out) {
    *raise_out = NULL;
    if (!v || v->type == JSON_NULL) return json_number(0);
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) return json_number((double)(long long)v->num_val);
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s;
        while (*p && tmp_is_ascii_ws((unsigned char)*p)) p++;
        const char *end = p + strlen(p);
        while (end > p && tmp_is_ascii_ws((unsigned char)end[-1])) end--;
        const char *q = p;
        if (q < end && (*q == '+' || *q == '-')) q++;
        bool ok = q < end;
        for (const char *d = q; d < end; d++)
            if (!isdigit((unsigned char)*d)) { ok = false; break; }
        if (ok) {
            char *tmp = malloc((size_t)(end - p) + 1);
            if (tmp) {
                memcpy(tmp, p, (size_t)(end - p));
                tmp[end - p] = '\0';
                double d = (double)strtoll(tmp, NULL, 10);
                free(tmp);
                return json_number(d);
            }
        }
        char *rk = tmp_pyrepr_str(s);
        char msg[256];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", rk ? rk : "''");
        free(rk);
        *raise_out = tmp_err_spec("ValueError", msg);
        return NULL;
    }
    char *rk = tmp_pyrepr_str(tmp_type_name(v));
    char msg[192];
    snprintf(msg, sizeof msg, "int() argument must be a string, a bytes-like object or a real number, not %s",
             rk ? rk : "");
    free(rk);
    *raise_out = tmp_err_spec("TypeError", msg);
    return NULL;
}

static json_t *tmp_py_float(const json_t *v, json_t **raise_out) {
    *raise_out = NULL;
    if (!v || v->type == JSON_NULL) return json_number(0);
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1 : 0);
    if (v->type == JSON_NUMBER) return json_number(v->num_val);
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *endp = NULL;
        double d = strtod(s, &endp);
        if (endp != s) {
            while (*endp && tmp_is_ascii_ws((unsigned char)*endp)) endp++;
            /* leading whitespace is accepted by float(); strtod skips it too */
            if (*endp == '\0') return json_number(d);
        }
        char *rk = tmp_pyrepr_str(s);
        char msg[256];
        snprintf(msg, sizeof msg, "could not convert string to float: %s", rk ? rk : "''");
        free(rk);
        *raise_out = tmp_err_spec("ValueError", msg);
        return NULL;
    }
    char msg[192];
    snprintf(msg, sizeof msg, "float() argument must be a string or a real number, not '%s'",
             tmp_type_name(v));
    *raise_out = tmp_err_spec("TypeError", msg);
    return NULL;
}

/* ── sorted string set (set -> sorted list projection) ── */
static int tmp_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* takes ownership of nothing; builds a fresh sorted-deduped array of copies */
static json_t *tmp_sorted_set(char **vals, size_t n) {
    json_t *out = json_array();
    if (n == 0) return out;
    qsort(vals, n, sizeof(char *), tmp_cmp_str);
    for (size_t i = 0; i < n; i++) {
        if (i > 0 && !strcmp(vals[i], vals[i - 1])) continue;
        json_append(out, json_string(vals[i]));
    }
    return out;
}

/* ── os / constants seams ── */
static json_t *tmp_expanduser(tmp_ctx_t *ctx, const char *path) {
    json_t *a = tmp_args0();
    tmp_asets(a, "path", path);
    return tmp_call(ctx, "os.path.expanduser", a);
}

static json_t *tmp_realpath(tmp_ctx_t *ctx, const char *path) {
    json_t *a = tmp_args0();
    tmp_asets(a, "path", path);
    return tmp_call(ctx, "os.path.realpath", a);
}

static json_t *tmp_isdir(tmp_ctx_t *ctx, const char *path) {
    json_t *a = tmp_args0();
    tmp_asets(a, "path", path);
    return tmp_call(ctx, "os.path.isdir", a);
}

static json_t *tmp_get_hermes_home(tmp_ctx_t *ctx) {
    return tmp_call(ctx, "get_hermes_home", tmp_args0());
}

/* hermes_cli.config.DEFAULT_CONFIG import -> the desktop defaults dict.
 * Returns an OWNED {"dc": node} pair via out params; caller frees both. */
static json_t *tmp_default_config(tmp_ctx_t *ctx, const json_t **defaults_out,
                                  json_t **raise_out) {
    *defaults_out = NULL;
    *raise_out = NULL;
    json_t *dc = tmp_call(ctx, "default_config", tmp_args0());
    if (tmp_is_raise(dc)) { *raise_out = dc; return NULL; }
    json_t *raise = NULL;
    const json_t *defaults = tmp_subscript(dc, "desktop", &raise);
    if (raise) { json_free(dc); *raise_out = raise; return NULL; }
    *defaults_out = defaults;
    return dc; /* keep alive: defaults borrows into it */
}

/* ── pdb seams ── */
static json_t *tmp_connect_closing(tmp_ctx_t *ctx) {
    return tmp_call(ctx, "connect_closing", tmp_args0());
}

static json_t *tmp_pdb_list_projects(tmp_ctx_t *ctx, const json_t *conn, bool include_archived) {
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    tmp_asetb(a, "include_archived", include_archived);
    return tmp_call(ctx, "pdb.list_projects", a);
}

static json_t *tmp_pdb_get_active_id(tmp_ctx_t *ctx, const json_t *conn) {
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    return tmp_call(ctx, "pdb.get_active_id", a);
}

static json_t *tmp_pdb_get_project(tmp_ctx_t *ctx, const json_t *conn, const json_t *id) {
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    tmp_aset(a, "id", id);
    return tmp_call(ctx, "pdb.get_project", a);
}

static json_t *tmp_to_dict(tmp_ctx_t *ctx) {
    return tmp_call(ctx, "to_dict", tmp_args0());
}

static json_t *tmp_pdb_set_active(tmp_ctx_t *ctx, const json_t *conn, const json_t *id) {
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    tmp_aset(a, "id", id);
    return tmp_call(ctx, "pdb.set_active", a);
}

static json_t *tmp_pdb_archive(tmp_ctx_t *ctx, const json_t *conn, const char *id, bool restore) {
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    tmp_asets(a, "id", id);
    return tmp_call(ctx, restore ? "pdb.restore_project" : "pdb.archive_project", a);
}

static json_t *tmp_pdb_delete(tmp_ctx_t *ctx, const json_t *conn, const char *id) {
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    tmp_asets(a, "id", id);
    return tmp_call(ctx, "pdb.delete_project", a);
}

static json_t *tmp_pdb_project_for_path(tmp_ctx_t *ctx, const json_t *conn, const char *cwd) {
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    tmp_asets(a, "cwd", cwd);
    return tmp_call(ctx, "pdb.project_for_path", a);
}

static json_t *tmp_warm_roots(tmp_ctx_t *ctx, json_t *roots) {
    json_t *a = tmp_args0();
    json_set(a, "roots", roots ? roots : json_array());
    return tmp_call(ctx, "git_probe.warm_roots", a);
}

/* project double .id attribute (borrowed -> copied string) */
static char *tmp_proj_id(const json_t *proj, json_t **raise_out) {
    const json_t *id = tmp_attr(proj, "id", raise_out);
    if (*raise_out) return NULL;
    return tmp_pystr(id);
}

/* ── module-global state mirror ── */
static void tmp_ensure_cache(tmp_ctx_t *ctx) {
    if (!json_obj_get(ctx->state, "_DIR_EXISTS_CACHE"))
        json_set(ctx->state, "_DIR_EXISTS_CACHE", json_object());
}

static void tmp_ensure_pending(tmp_ctx_t *ctx) {
    if (!json_obj_get(ctx->state, "_registry_pending"))
        json_set(ctx->state, "_registry_pending", json_array());
}

static void tmp_pending_append(tmp_ctx_t *ctx, const char *name) {
    tmp_ensure_pending(ctx);
    json_t *pair = json_array();
    json_append(pair, json_string(name));
    json_append(pair, json_string("handler"));
    json_append(json_obj_get(ctx->state, "_registry_pending"), pair);
}

/* ── pure leaves ── */

/* PoP: tmp_pick @ tui_gateway/methods_projects.py:_pick */
static json_t *tmp_pick(tmp_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *params = json_obj_get(args, "params");
    const json_t *keys = json_obj_get(args, "keys");
    json_t *raise = NULL;
    if (keys && keys->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(keys));
        return tmp_err_spec("TypeError", msg);
    }
    /* params.get(k) fires on the FIRST comprehension iteration only */
    if (keys && json_len(keys) > 0 && (!params || params->type != JSON_OBJECT)) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tmp_type_name(params));
        return tmp_err_spec("AttributeError", msg);
    }
    json_t *out = json_object();
    for (size_t i = 0; keys && i < json_len(keys); i++) {
        const char *k = tmp_str(json_get(keys, i));
        if (!k) {
            json_free(out);
            return tmp_err_spec("TypeError", "unhashable type in keys");
        }
        const json_t *v = tmp_dict_get(params, k, &raise);
        if (raise) { json_free(out); return raise; }
        json_set(out, k, v ? json_copy(v) : json_null());
    }
    return out;
}

/* PoP: tmp_project_tree_row @ tui_gateway/methods_projects.py:_project_tree_row */
static json_t *tmp_project_tree_row(tmp_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *entry = json_obj_get(args, "entry");
    json_t *raise = NULL;
    const json_t *s = tmp_attr(entry, "session", &raise);
    if (raise) return raise;
    /* dict literal evaluation order (live return shape) */
    const json_t *sid = tmp_attr(s, "session_id", &raise);
    if (raise) return raise;
    const json_t *lrid = tmp_attr(entry, "lineage_root_id", &raise);
    if (raise) return raise;
    const json_t *lids = tmp_attr(entry, "lineage_ids", &raise);
    if (raise) return raise;
    /* list(entry.lineage_ids) or None */
    json_t *lids_out = NULL;
    if (!lids || lids->type == JSON_NULL) {
        return tmp_err_spec("TypeError", "'NoneType' object is not iterable");
    }
    if (lids->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(lids));
        return tmp_err_spec("TypeError", msg);
    }
    lids_out = json_copy(lids);
    if (json_len(lids_out) == 0) { json_free(lids_out); lids_out = json_null(); }
    const json_t *psid = tmp_attr(s, "parent_session_id", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *rfrom = tmp_attr(s, "reset_from", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *bfrom = tmp_attr(s, "branched_from", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *title = tmp_attr(s, "title", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *preview = tmp_attr(entry, "preview", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *started = tmp_attr(s, "started_at", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *ended = tmp_attr(s, "ended_at", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *la = tmp_attr(entry, "last_active", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *source = tmp_attr(s, "source", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *archived = tmp_attr(s, "archived", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *mc = tmp_attr(s, "message_count", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *tcc = tmp_attr(s, "tool_call_count", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *it = tmp_attr(s, "input_tokens", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *ot = tmp_attr(s, "output_tokens", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *ac = tmp_attr(s, "actual_cost_usd", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *ec = tmp_attr(s, "estimated_cost_usd", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *model = tmp_attr(s, "model", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *cwd = tmp_attr(s, "cwd", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *gb = tmp_attr(s, "git_branch", &raise);
    if (raise) { json_free(lids_out); return raise; }
    const json_t *grr = tmp_attr(s, "git_repo_root", &raise);
    if (raise) { json_free(lids_out); return raise; }

    json_t *out = json_object();
    tmp_aset(out, "id", sid);
    tmp_aset(out, "_lineage_root_id", lrid);
    json_set(out, "_lineage_ids", lids_out); /* adopted */
    tmp_aset(out, "parent_session_id", psid);
    tmp_aset(out, "_reset_from", rfrom);
    tmp_aset(out, "_branched_from", bfrom);
    tmp_aset(out, "title", title);
    tmp_aset(out, "preview", preview);
    /* s.started_at or 0 */
    json_set(out, "started_at", tmp_truthy(started) ? json_copy(started) : json_number(0));
    tmp_aset(out, "ended_at", ended);
    /* entry.last_active or s.started_at or 0 */
    const json_t *la_v = tmp_truthy(la) ? la : (tmp_truthy(started) ? started : NULL);
    json_set(out, "last_active", la_v ? json_copy(la_v) : json_number(0));
    tmp_aset(out, "source", source);
    tmp_asetb(out, "archived", tmp_truthy(archived));
    json_set(out, "message_count", tmp_truthy(mc) ? json_copy(mc) : json_number(0));
    json_set(out, "tool_call_count", tmp_truthy(tcc) ? json_copy(tcc) : json_number(0));
    json_set(out, "input_tokens", tmp_truthy(it) ? json_copy(it) : json_number(0));
    json_set(out, "output_tokens", tmp_truthy(ot) ? json_copy(ot) : json_number(0));
    tmp_aset(out, "actual_cost_usd", ac);
    tmp_aset(out, "estimated_cost_usd", ec);
    tmp_aset(out, "model", model);
    tmp_asetb(out, "is_active", false);
    tmp_aset(out, "cwd", cwd);
    tmp_aset(out, "git_branch", gb);
    tmp_aset(out, "git_repo_root", grr);
    return out;
}

/* PoP: tmp_non_workspace_dirs @ tui_gateway/methods_projects.py:_non_workspace_dirs */
static json_t *tmp_non_workspace_dirs(tmp_ctx_t *ctx, json_t **raise_out) {
    *raise_out = NULL;
    json_t *home_raw = tmp_expanduser(ctx, "~");
    if (tmp_is_raise(home_raw)) { *raise_out = home_raw; return NULL; }
    json_t *home = tmp_realpath(ctx, tmp_str(home_raw) ? tmp_str(home_raw) : "");
    json_free(home_raw);
    if (tmp_is_raise(home)) { *raise_out = home; return NULL; }
    const char *home_s = tmp_str(home) ? tmp_str(home) : "";
    char *dn = tmp_posix_dirname(home_s);
    const char *candidates[5] = { "/", home_s, dn ? dn : "", "/home", "/Users" };
    char **vals = malloc(5 * sizeof(char *));
    size_t n = 0;
    json_t *raise = NULL;
    for (int i = 0; i < 5; i++) {
        if (!candidates[i][0]) continue; /* `if path` */
        json_t *rp = tmp_realpath(ctx, candidates[i]);
        if (tmp_is_raise(rp)) { raise = rp; break; }
        /* os.path.normcase is the POSIX identity */
        vals[n++] = tmp_strdup(tmp_str(rp) ? tmp_str(rp) : "");
        json_free(rp);
    }
    free(dn);
    json_free(home);
    if (raise) {
        for (size_t i = 0; i < n; i++) free(vals[i]);
        free(vals);
        *raise_out = raise;
        return NULL;
    }
    json_t *out = tmp_sorted_set(vals, n);
    for (size_t i = 0; i < n; i++) free(vals[i]);
    free(vals);
    return out;
}

/* membership in the sorted-set projection */
static bool tmp_in_str_array(const json_t *arr, const char *s) {
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = tmp_str(json_get(arr, i));
        if (v && !strcmp(v, s)) return true;
    }
    return false;
}

/* PoP: tmp_is_repo_junk @ tui_gateway/methods_projects.py:_is_repo_junk */
static json_t *tmp_is_repo_junk(tmp_ctx_t *ctx, const json_t *root) {
    if (!tmp_truthy(root)) return json_bool(true);
    const char *root_s = tmp_str(root);
    if (!root_s) {
        /* os.path.realpath(non-str) -- outside the fixture str domain */
        char msg[128];
        snprintf(msg, sizeof msg, "realpath() argument must be str, not '%s'", tmp_type_name(root));
        return tmp_err_spec("TypeError", msg);
    }
    json_t *real = tmp_realpath(ctx, root_s);
    if (tmp_is_raise(real)) return real;
    json_t *hh_val = tmp_get_hermes_home(ctx);
    if (tmp_is_raise(hh_val)) { json_free(real); return hh_val; }
    char *hh_s = tmp_pystr(hh_val);
    json_free(hh_val);
    json_t *hh = tmp_realpath(ctx, hh_s ? hh_s : "");
    free(hh_s);
    if (tmp_is_raise(hh)) { json_free(real); return hh; }
    json_t *raise = NULL;
    json_t *nwd = tmp_non_workspace_dirs(ctx, &raise);
    if (raise) { json_free(real); json_free(hh); return raise; }
    const char *real_s = tmp_str(real) ? tmp_str(real) : "";
    const char *hh_str = tmp_str(hh) ? tmp_str(hh) : "";
    bool junk = tmp_in_str_array(nwd, real_s); /* normcase(real) in set */
    if (!junk) junk = !strcmp(real_s, hh_str);
    if (!junk) {
        /* real.startswith(hermes_home + os.sep) */
        size_t n = strlen(hh_str);
        char *pfx = malloc(n + 2);
        if (pfx) {
            memcpy(pfx, hh_str, n);
            pfx[n] = '/';
            pfx[n + 1] = '\0';
            junk = tmp_startswith(real_s, pfx);
            free(pfx);
        }
    }
    json_free(nwd);
    json_free(real);
    json_free(hh);
    return json_bool(junk);
}

/* PoP: tmp_is_session_cwd_junk @ tui_gateway/methods_projects.py:_is_session_cwd_junk */
static json_t *tmp_is_session_cwd_junk(tmp_ctx_t *ctx, const json_t *cwd) {
    if (!tmp_truthy(cwd)) return json_bool(true);
    const char *cwd_s = tmp_str(cwd);
    if (!cwd_s) {
        char msg[128];
        snprintf(msg, sizeof msg, "realpath() argument must be str, not '%s'", tmp_type_name(cwd));
        return tmp_err_spec("TypeError", msg);
    }
    json_t *real = tmp_realpath(ctx, cwd_s);
    if (tmp_is_raise(real)) return real;
    json_t *hh_val = tmp_get_hermes_home(ctx);
    if (tmp_is_raise(hh_val)) { json_free(real); return hh_val; }
    char *hh_s = tmp_pystr(hh_val);
    json_free(hh_val);
    json_t *hh = tmp_realpath(ctx, hh_s ? hh_s : "");
    free(hh_s);
    if (tmp_is_raise(hh)) { json_free(real); return hh; }
    json_t *raise = NULL;
    json_t *nwd = tmp_non_workspace_dirs(ctx, &raise);
    if (raise) { json_free(real); json_free(hh); return raise; }
    /* normcase is the POSIX identity on both sides */
    const char *real_s = tmp_str(real) ? tmp_str(real) : "";
    const char *hh_str = tmp_str(hh) ? tmp_str(hh) : "";
    bool junk = tmp_in_str_array(nwd, real_s) || !strcmp(real_s, hh_str);
    json_free(nwd);
    json_free(real);
    json_free(hh);
    return json_bool(junk);
}

/* ── repo-discovery policy ── */
/* _get(short, long): source.get(short, source.get(long, defaults[long])) --
 * the default argument is EAGER: defaults[long] is subscripted FIRST. */
static const json_t *tmp_policy_get(const json_t *source, const json_t *defaults,
                                    const char *short_k, const char *long_k,
                                    json_t **raise_out) {
    *raise_out = NULL;
    const json_t *dflt = tmp_subscript(defaults, long_k, raise_out);
    if (*raise_out) return NULL;
    json_t *raise = NULL;
    const json_t *long_v = tmp_dict_get(source, long_k, &raise);
    if (raise) { *raise_out = raise; return NULL; }
    const json_t *inner = long_v ? long_v : dflt;
    const json_t *short_v = tmp_dict_get(source, short_k, &raise);
    if (raise) { *raise_out = raise; return NULL; }
    return short_v ? short_v : inner;
}

/* _paths(short, long): non-list -> list(defaults[long]); list -> stripped strs */
static json_t *tmp_policy_paths(const json_t *source, const json_t *defaults,
                                const char *short_k, const char *long_k,
                                json_t **raise_out) {
    *raise_out = NULL;
    const json_t *values = tmp_policy_get(source, defaults, short_k, long_k, raise_out);
    if (*raise_out) return NULL;
    if (!values || values->type != JSON_ARRAY) {
        json_t *raise = NULL;
        const json_t *dflt = tmp_subscript(defaults, long_k, &raise);
        if (raise) { *raise_out = raise; return NULL; }
        /* list(defaults[long]) */
        if (dflt && dflt->type == JSON_ARRAY) return json_copy(dflt);
        if (dflt && dflt->type == JSON_STRING) {
            json_t *out = json_array();
            for (const char *p = dflt->str_val; p && *p; p++) {
                char ch[2] = { *p, '\0' };
                json_append(out, json_string(ch));
            }
            return out;
        }
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(dflt));
        *raise_out = tmp_err_spec("TypeError", msg);
        return NULL;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(values); i++) {
        const json_t *v = json_get(values, i);
        if (!v || v->type != JSON_STRING) continue; /* isinstance(v, str) */
        char *st = tmp_strip(v->str_val);
        if (st && st[0]) json_append(out, json_string(st)); /* and v.strip() */
        free(st);
    }
    return out;
}

/* shared core of _repo_discovery_policy (raw == NULL -> the _load_cfg path) */
static json_t *tmp_policy_core(tmp_ctx_t *ctx, const json_t *raw, json_t **raise_out) {
    *raise_out = NULL;
    json_t *dc = NULL;
    const json_t *defaults = NULL;
    json_t *cfg = NULL;
    json_t *empty = NULL;
    json_t *out = NULL;
    json_t *raise = NULL;
    dc = tmp_default_config(ctx, &defaults, &raise);
    if (raise) { *raise_out = raise; return NULL; }
    const json_t *source;
    if (raw && raw->type == JSON_OBJECT) {
        source = raw;
    } else {
        cfg = tmp_call(ctx, "load_cfg", tmp_args0());
        if (tmp_is_raise(cfg)) { raise = cfg; cfg = NULL; goto cleanup; }
        const json_t *desktop = tmp_dict_get(cfg, "desktop", &raise);
        if (raise) goto cleanup;
        source = tmp_truthy(desktop) ? desktop : NULL;
        if (!source) {
            empty = json_object();
            source = empty;
        }
        if (source->type != JSON_OBJECT) {
            if (empty) { json_free(empty); empty = NULL; }
            empty = json_object();
            source = empty;
        }
    }
    /* enabled = _get("enabled", "repo_scan_enabled") */
    const json_t *enabled = tmp_policy_get(source, defaults, "enabled", "repo_scan_enabled", &raise);
    if (raise) goto cleanup;
    json_t *roots = tmp_policy_paths(source, defaults, "roots", "repo_scan_roots", &raise);
    if (raise) goto cleanup;
    json_t *excl = tmp_policy_paths(source, defaults, "exclude_paths", "repo_scan_exclude_paths", &raise);
    if (raise) { json_free(roots); goto cleanup; }
    out = json_object();
    if (enabled && enabled->type == JSON_BOOL) {
        tmp_aset(out, "enabled", enabled);
    } else {
        const json_t *den = tmp_subscript(defaults, "repo_scan_enabled", &raise);
        if (raise) { json_free(out); out = NULL; json_free(roots); json_free(excl); goto cleanup; }
        tmp_aset(out, "enabled", den);
    }
    json_set(out, "roots", roots);
    json_set(out, "exclude_paths", excl);
cleanup:
    json_free(dc);
    json_free(cfg);
    json_free(empty);
    if (raise) { *raise_out = raise; json_free(out); return NULL; }
    return out;
}

/* PoP: tmp_repo_discovery_policy @ tui_gateway/methods_projects.py:_repo_discovery_policy */
static json_t *tmp_repo_discovery_policy(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *raw = json_obj_get(args, "raw");
    json_t *raise = NULL;
    json_t *out = tmp_policy_core(ctx, raw, &raise);
    return raise ? raise : out;
}

/* _paths(values) inside _repo_discovery_policy_key: expanduser each value,
 * join under home, abspath, normcase, set, sorted. */
static json_t *tmp_key_paths(tmp_ctx_t *ctx, const json_t *values, json_t **raise_out) {
    *raise_out = NULL;
    if (!values || values->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(values));
        *raise_out = tmp_err_spec("TypeError", msg);
        return NULL;
    }
    json_t *home = tmp_expanduser(ctx, "~");
    if (tmp_is_raise(home)) { *raise_out = home; return NULL; }
    const char *home_s = tmp_str(home) ? tmp_str(home) : "";
    char **vals = malloc((json_len(values) + 1) * sizeof(char *));
    size_t n = 0;
    json_t *raise = NULL;
    for (size_t i = 0; i < json_len(values); i++) {
        const json_t *v = json_get(values, i);
        const char *vs = tmp_str(v);
        if (!vs) {
            /* os.path.expanduser(non-str) is outside the fixture domain */
            char msg[128];
            snprintf(msg, sizeof msg, "expanduser() argument must be str, not '%s'", tmp_type_name(v));
            raise = tmp_err_spec("TypeError", msg);
            break;
        }
        json_t *ev = tmp_expanduser(ctx, vs);
        if (tmp_is_raise(ev)) { raise = ev; break; }
        const char *evs = tmp_str(ev);
        if (!evs) {
            char msg[128];
            snprintf(msg, sizeof msg, "join() argument must be str, not '%s'", tmp_type_name(ev));
            json_free(ev);
            raise = tmp_err_spec("TypeError", msg);
            break;
        }
        char *joined = tmp_posix_join2(home_s, evs);
        char *abs = tmp_posix_abspath(joined ? joined : "");
        free(joined);
        json_free(ev);
        vals[n++] = abs ? abs : tmp_strdup("");
    }
    json_free(home);
    if (raise) {
        for (size_t i = 0; i < n; i++) free(vals[i]);
        free(vals);
        *raise_out = raise;
        return NULL;
    }
    json_t *out = tmp_sorted_set(vals, n);
    for (size_t i = 0; i < n; i++) free(vals[i]);
    free(vals);
    return out;
}

/* PoP: tmp_repo_discovery_policy_key @ tui_gateway/methods_projects.py:_repo_discovery_policy_key */
static json_t *tmp_repo_discovery_policy_key(tmp_ctx_t *ctx, const json_t *policy,
                                             json_t **raise_out) {
    *raise_out = NULL;
    json_t *raise = NULL;
    const json_t *enabled = tmp_subscript(policy, "enabled", &raise);
    if (raise) { *raise_out = raise; return NULL; }
    const json_t *roots = tmp_subscript(policy, "roots", &raise);
    if (raise) { *raise_out = raise; return NULL; }
    const json_t *excl = tmp_subscript(policy, "exclude_paths", &raise);
    if (raise) { *raise_out = raise; return NULL; }
    json_t *roots_c = tmp_key_paths(ctx, roots, &raise);
    if (raise) { *raise_out = raise; return NULL; }
    json_t *excl_c = tmp_key_paths(ctx, excl, &raise);
    if (raise) { json_free(roots_c); *raise_out = raise; return NULL; }
    /* canonical dict in sorted-key order (enabled < exclude_paths < roots);
     * json_serialize is compact ("," / ":") with Python-matching ASCII escapes */
    json_t *canonical = json_object();
    tmp_asetb(canonical, "enabled", tmp_truthy(enabled));
    json_set(canonical, "exclude_paths", excl_c);
    json_set(canonical, "roots", roots_c);
    char *s = json_serialize(canonical);
    json_free(canonical);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* PoP: tmp_repo_discovery_policy_is_default @ tui_gateway/methods_projects.py:_repo_discovery_policy_is_default */
static json_t *tmp_repo_discovery_policy_is_default(tmp_ctx_t *ctx, const json_t *policy) {
    json_t *raise = NULL;
    const json_t *defaults = NULL;
    json_t *dc = tmp_default_config(ctx, &defaults, &raise);
    if (raise) return raise;
    json_t *k1 = tmp_repo_discovery_policy_key(ctx, policy, &raise);
    if (raise) { json_free(dc); return raise; }
    json_t *p2 = tmp_policy_core(ctx, defaults, &raise);
    if (raise) { json_free(dc); json_free(k1); return raise; }
    json_t *k2 = tmp_repo_discovery_policy_key(ctx, p2, &raise);
    json_free(p2);
    if (raise) { json_free(dc); json_free(k1); return raise; }
    bool eq = !strcmp(tmp_str(k1) ? tmp_str(k1) : "", tmp_str(k2) ? tmp_str(k2) : "");
    json_free(k1);
    json_free(k2);
    json_free(dc);
    return json_bool(eq);
}

/* PoP: tmp_dir_exists_cached @ tui_gateway/methods_projects.py:_dir_exists_cached */
static json_t *tmp_dir_exists_cached(tmp_ctx_t *ctx, const json_t *args) {
    tmp_ensure_cache(ctx);
    const json_t *path = json_obj_get(args, "path");
    const char *path_s = tmp_str(path);
    json_t *cache = json_obj_get(ctx->state, "_DIR_EXISTS_CACHE");
    const json_t *hit = path_s ? json_obj_get(cache, path_s) : NULL;
    if (!hit || hit->type == JSON_NULL) { /* hit is None */
        json_t *d = tmp_isdir(ctx, path_s ? path_s : "");
        if (tmp_is_raise(d)) return d;
        if (path_s) json_set(cache, path_s, json_copy(d));
        return d;
    }
    return json_copy(hit);
}

/* ── projects helpers ── */

/* PoP: tmp_require_project @ tui_gateway/methods_projects.py:_require_project */
static json_t *tmp_require_project(tmp_ctx_t *ctx, const json_t *conn,
                                   const json_t *params, json_t **raise_out) {
    *raise_out = NULL;
    json_t *raise = NULL;
    const json_t *idv = tmp_dict_get(params, "id", &raise);
    if (raise) { *raise_out = raise; return NULL; }
    char *id_s = tmp_truthy(idv) ? tmp_pystr(idv) : tmp_strdup("");
    json_t *id_node = json_string(id_s ? id_s : "");
    json_t *proj = tmp_pdb_get_project(ctx, conn, id_node);
    json_free(id_node);
    free(id_s);
    if (tmp_is_raise(proj)) { *raise_out = proj; return NULL; }
    if (!proj || proj->type == JSON_NULL) {
        json_free(proj);
        *raise_out = tmp_err_spec("_NoProject", "");
        return NULL;
    }
    return proj;
}

/* PoP: tmp_projects_payload @ tui_gateway/methods_projects.py:_projects_payload */
static json_t *tmp_projects_payload(tmp_ctx_t *ctx, const json_t *conn, json_t **raise_out) {
    *raise_out = NULL;
    json_t *ps = tmp_pdb_list_projects(ctx, conn, true);
    if (tmp_is_raise(ps)) { *raise_out = ps; return NULL; }
    if (!ps || ps->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(ps));
        json_free(ps);
        *raise_out = tmp_err_spec("TypeError", msg);
        return NULL;
    }
    json_t *projects = json_array();
    for (size_t i = 0; i < json_len(ps); i++) {
        json_t *td = tmp_to_dict(ctx);
        if (tmp_is_raise(td)) {
            json_free(ps);
            json_free(projects);
            *raise_out = td;
            return NULL;
        }
        json_append(projects, td);
    }
    json_free(ps);
    json_t *aid = tmp_pdb_get_active_id(ctx, conn);
    if (tmp_is_raise(aid)) {
        json_free(projects);
        *raise_out = aid;
        return NULL;
    }
    json_t *out = json_object();
    json_set(out, "projects", projects);
    json_set(out, "active_id", aid);
    return out;
}

/* the _projects_method handler core: try fn(rid, params, pdb, conn) inside
 * `with pdb.connect_closing() as conn`, mapping _NoProject/ValueError/Exception
 * to 5062/5063/5061. fn_cb returns an owned value-or-raise. */
typedef json_t *(*tmp_fn_cb_t)(tmp_ctx_t *, const json_t *rid, const json_t *params,
                               const json_t *conn, void *data);

static json_t *tmp_projects_handler_core(tmp_ctx_t *ctx, const json_t *rid,
                                         const json_t *params, tmp_fn_cb_t fn_cb,
                                         void *data) {
    json_t *conn = tmp_connect_closing(ctx);
    json_t *ret = NULL;
    if (tmp_is_raise(conn)) {
        ret = conn;
    } else {
        ret = fn_cb(ctx, rid, params, conn, data);
        json_free(conn);
    }
    if (!tmp_is_raise(ret)) return ret;
    const char *kind = tmp_raise_kind(ret);
    const char *msg = tmp_raise_msg(ret);
    json_t *mapped = NULL;
    if (!strcmp(kind, "_NoProject")) {
        mapped = tmp_err(ctx, rid, 5062, "no such project");
    } else if (tmp_exc_is_a(kind, "ValueError")) {
        mapped = tmp_err(ctx, rid, 5063, msg);
    } else if (tmp_exc_caught(kind)) {
        mapped = tmp_err(ctx, rid, 5061, msg);
    } else {
        return ret; /* BaseException passthrough */
    }
    json_free(ret);
    return mapped;
}

/* the wrapped-fn callback for direct _projects_method cases */
static json_t *tmp_projects_fn_cb(tmp_ctx_t *ctx, const json_t *rid, const json_t *params,
                                  const json_t *conn, void *data) {
    (void)data;
    json_t *a = tmp_args0();
    tmp_aset(a, "rid", rid);
    tmp_aset(a, "params", params);
    tmp_aset(a, "conn", conn);
    return tmp_call(ctx, "projects_fn", a);
}

/* PoP: tmp_projects_method @ tui_gateway/methods_projects.py:_projects_method */
static json_t *tmp_projects_method(tmp_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    /* @method(name) fires when decorator(fn) is applied */
    tmp_pending_append(ctx, name);
    return tmp_projects_handler_core(ctx, json_obj_get(args, "rid"),
                                     json_obj_get(args, "params"),
                                     tmp_projects_fn_cb, NULL);
}

/* the mutator body: _require_project -> getattr(pdb, fn_name)(...) -> refreshed
 * project -> _ok. kwargs_of is a callback seam. */
typedef struct { const char *fn_name; bool takes_path; } tmp_mutator_data_t;

static json_t *tmp_mutator_cb(tmp_ctx_t *ctx, const json_t *rid, const json_t *params,
                              const json_t *conn, void *vp) {
    tmp_mutator_data_t *md = (tmp_mutator_data_t *)vp;
    json_t *raise = NULL;
    json_t *proj = tmp_require_project(ctx, conn, params, &raise);
    if (raise) return raise;
    char *pid = tmp_proj_id(proj, &raise);
    if (raise) { json_free(proj); return raise; }
    /* args = (str(params.get("path") or ""),) if takes_path else () */
    json_t *path_args = json_array();
    if (md->takes_path) {
        const json_t *pv = tmp_dict_get(params, "path", &raise);
        if (raise) { free(pid); json_free(proj); json_free(path_args); return raise; }
        char *ps = tmp_truthy(pv) ? tmp_pystr(pv) : tmp_strdup("");
        json_append(path_args, json_string(ps ? ps : ""));
        free(ps);
    }
    /* **kwargs_of(params) evaluated BEFORE the mutator call */
    json_t *ka = tmp_args0();
    tmp_aset(ka, "params", params);
    json_t *kwargs = tmp_call(ctx, "kwargs_of", ka);
    if (tmp_is_raise(kwargs)) {
        free(pid); json_free(proj); json_free(path_args);
        return kwargs;
    }
    json_t *a = tmp_args0();
    tmp_asets(a, "fn_name", md->fn_name);
    tmp_aset(a, "conn", conn);
    tmp_asets(a, "id", pid);
    json_set(a, "args", path_args); /* adopted */
    json_set(a, "kwargs", kwargs);  /* adopted */
    json_t *mut = tmp_call(ctx, "pdb.mutator", a);
    if (tmp_is_raise(mut)) { free(pid); json_free(proj); return mut; }
    json_free(mut);
    /* pdb.get_project(conn, proj.id).to_dict() -- proj.id re-read */
    char *pid2 = tmp_proj_id(proj, &raise);
    json_free(proj);
    if (raise) { free(pid); free(pid2); return raise; }
    json_t *id_node = json_string(pid2 ? pid2 : "");
    json_t *proj2 = tmp_pdb_get_project(ctx, conn, id_node);
    json_free(id_node);
    free(pid2);
    free(pid);
    if (tmp_is_raise(proj2)) return proj2;
    if (!proj2 || proj2->type == JSON_NULL) {
        json_free(proj2);
        return tmp_err_spec("AttributeError", "'NoneType' object has no attribute 'to_dict'");
    }
    json_free(proj2);
    json_t *td = tmp_to_dict(ctx);
    if (tmp_is_raise(td)) return td;
    json_t *payload = json_object();
    json_set(payload, "project", td);
    return tmp_ok(ctx, rid, payload);
}

/* PoP: tmp_register_project_mutator @ tui_gateway/methods_projects.py:_register_project_mutator */
static json_t *tmp_register_project_mutator(tmp_ctx_t *ctx, const json_t *args) {
    const char *suffix = json_get_str(args, "suffix", "");
    tmp_mutator_data_t md;
    md.fn_name = json_get_str(args, "fn_name", "");
    md.takes_path = json_get_bool(args, "takes_path", false);
    char name[192];
    snprintf(name, sizeof name, "projects.%s", suffix);
    tmp_pending_append(ctx, name);
    return tmp_projects_handler_core(ctx, json_obj_get(args, "rid"),
                                     json_obj_get(args, "params"),
                                     tmp_mutator_cb, &md);
}

/* ── bare RPC handler bodies (decorators stripped; driven with rid/params/conn) ── */

/* PoP: tmp_projects_list @ tui_gateway/methods_projects.py:_ */
static json_t *tmp_projects_list(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *conn = json_obj_get(args, "conn");
    json_t *raise = NULL;
    json_t *payload = tmp_projects_payload(ctx, conn, &raise);
    if (raise) return raise;
    return tmp_ok(ctx, rid, payload);
}

/* PoP: tmp_projects_get @ tui_gateway/methods_projects.py:_ */
static json_t *tmp_projects_get(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *conn = json_obj_get(args, "conn");
    json_t *raise = NULL;
    json_t *proj = tmp_require_project(ctx, conn, json_obj_get(args, "params"), &raise);
    if (raise) return raise;
    json_free(proj);
    json_t *td = tmp_to_dict(ctx);
    if (tmp_is_raise(td)) return td;
    json_t *payload = json_object();
    json_set(payload, "project", td);
    return tmp_ok(ctx, rid, payload);
}

/* PoP: tmp_projects_create @ tui_gateway/methods_projects.py:_ */
static json_t *tmp_projects_create(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *conn = json_obj_get(args, "conn");
    const json_t *params = json_obj_get(args, "params");
    json_t *raise = NULL;
    const json_t *name_v = tmp_dict_get(params, "name", &raise);
    if (raise) return raise;
    char *name_s = tmp_truthy(name_v) ? tmp_pystr(name_v) : tmp_strdup("");
    const json_t *folders_v = tmp_dict_get(params, "folders", &raise);
    if (raise) { free(name_s); return raise; }
    json_t *folders = tmp_truthy(folders_v) ? json_copy(folders_v) : json_array();
    /* **_pick(params, "slug", "primary_path", "description", "icon", "color",
     * "board_slug") -- the live _pick runs here (pure) */
    static const char *PICK_KEYS[] = {
        "slug", "primary_path", "description", "icon", "color", "board_slug"
    };
    json_t *picked = json_object();
    for (size_t i = 0; i < 6; i++) {
        const json_t *v = tmp_dict_get(params, PICK_KEYS[i], &raise);
        if (raise) { free(name_s); json_free(folders); json_free(picked); return raise; }
        json_set(picked, PICK_KEYS[i], v ? json_copy(v) : json_null());
    }
    json_t *a = tmp_args0();
    tmp_aset(a, "conn", conn);
    tmp_asets(a, "name", name_s);
    free(name_s);
    json_set(a, "folders", folders);
    for (size_t i = 0; i < 6; i++)
        json_set(a, PICK_KEYS[i], json_copy(json_obj_get(picked, PICK_KEYS[i])));
    json_free(picked);
    json_t *pid = tmp_call(ctx, "pdb.create_project", a);
    if (tmp_is_raise(pid)) return pid;
    const json_t *use = tmp_dict_get(params, "use", &raise);
    if (raise) { json_free(pid); return raise; }
    if (tmp_truthy(use)) {
        json_t *sa = tmp_pdb_set_active(ctx, conn, pid);
        if (tmp_is_raise(sa)) { json_free(pid); return sa; }
        json_free(sa);
    }
    json_t *fa = tmp_args0();
    tmp_asets(fa, "feature", "projects");
    json_t *rfu = tmp_call(ctx, "record_feature_used", fa);
    if (tmp_is_raise(rfu)) { json_free(pid); return rfu; }
    json_free(rfu);
    json_t *proj = tmp_pdb_get_project(ctx, conn, pid);
    if (tmp_is_raise(proj)) { json_free(pid); return proj; }
    /* proj.to_dict() if proj else None -- a dict double (even {}) is TRUTHY */
    json_t *proj_val = NULL;
    if (proj && proj->type != JSON_NULL) {
        json_free(proj);
        proj_val = tmp_to_dict(ctx);
        if (tmp_is_raise(proj_val)) { json_free(pid); return proj_val; }
    } else {
        json_free(proj);
        proj_val = json_null();
    }
    json_free(pid);
    json_t *payload = json_object();
    json_set(payload, "project", proj_val);
    return tmp_ok(ctx, rid, payload);
}

/* PoP: tmp_projects_archive @ tui_gateway/methods_projects.py:_ */
static json_t *tmp_projects_archive(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *conn = json_obj_get(args, "conn");
    const json_t *params = json_obj_get(args, "params");
    json_t *raise = NULL;
    json_t *proj = tmp_require_project(ctx, conn, params, &raise);
    if (raise) return raise;
    char *pid = tmp_proj_id(proj, &raise);
    json_free(proj);
    if (raise) return raise;
    const json_t *restore = tmp_dict_get(params, "restore", &raise);
    if (raise) { free(pid); return raise; }
    json_t *ar = tmp_pdb_archive(ctx, conn, pid ? pid : "", tmp_truthy(restore));
    free(pid);
    if (tmp_is_raise(ar)) return ar;
    json_free(ar);
    json_t *payload = tmp_projects_payload(ctx, conn, &raise);
    if (raise) return raise;
    return tmp_ok(ctx, rid, payload);
}

/* PoP: tmp_projects_delete @ tui_gateway/methods_projects.py:_ */
static json_t *tmp_projects_delete(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *conn = json_obj_get(args, "conn");
    json_t *raise = NULL;
    json_t *proj = tmp_require_project(ctx, conn, json_obj_get(args, "params"), &raise);
    if (raise) return raise;
    char *pid = tmp_proj_id(proj, &raise);
    json_free(proj);
    if (raise) return raise;
    json_t *del = tmp_pdb_delete(ctx, conn, pid ? pid : "");
    free(pid);
    if (tmp_is_raise(del)) return del;
    json_free(del);
    json_t *payload = tmp_projects_payload(ctx, conn, &raise);
    if (raise) return raise;
    return tmp_ok(ctx, rid, payload);
}

/* PoP: tmp_projects_set_active @ tui_gateway/methods_projects.py:_ */
static json_t *tmp_projects_set_active(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *conn = json_obj_get(args, "conn");
    const json_t *params = json_obj_get(args, "params");
    json_t *raise = NULL;
    const json_t *idv = tmp_dict_get(params, "id", &raise);
    if (raise) return raise;
    json_t *id_val = NULL; /* owned or NULL */
    char *pid = NULL;
    if (tmp_truthy(idv)) {
        json_t *proj = tmp_require_project(ctx, conn, params, &raise);
        if (raise) return raise;
        pid = tmp_proj_id(proj, &raise);
        json_free(proj);
        if (raise) return raise;
        id_val = json_string(pid ? pid : "");
    }
    json_t *sa = tmp_pdb_set_active(ctx, conn, id_val);
    free(pid);
    json_free(id_val);
    if (tmp_is_raise(sa)) return sa;
    json_free(sa);
    json_t *aid = tmp_pdb_get_active_id(ctx, conn);
    if (tmp_is_raise(aid)) return aid;
    json_t *payload = json_object();
    json_set(payload, "active_id", aid);
    return tmp_ok(ctx, rid, payload);
}

/* PoP: tmp_projects_for_cwd @ tui_gateway/methods_projects.py:_ */
static json_t *tmp_projects_for_cwd(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *conn = json_obj_get(args, "conn");
    const json_t *params = json_obj_get(args, "params");
    json_t *raise = NULL;
    const json_t *cwdv = tmp_dict_get(params, "cwd", &raise);
    if (raise) return raise;
    json_t *carg = json_object();
    if (tmp_truthy(cwdv)) {
        char *cs = tmp_pystr(cwdv);
        char *st = tmp_strip(cs ? cs : "");
        free(cs);
        tmp_asets(carg, "cwd", st ? st : "");
        free(st);
    }
    json_t *a = tmp_args0();
    json_set(a, "arg", carg);
    json_t *cwd = tmp_call(ctx, "completion_cwd", a);
    if (tmp_is_raise(cwd)) return cwd;
    const char *cwd_s = tmp_str(cwd) ? tmp_str(cwd) : "";
    json_t *proj = tmp_pdb_project_for_path(ctx, conn, cwd_s);
    if (tmp_is_raise(proj)) { json_free(cwd); return proj; }
    json_t *proj_val = NULL;
    if (proj && proj->type != JSON_NULL) {
        json_free(proj);
        proj_val = tmp_to_dict(ctx);
        if (tmp_is_raise(proj_val)) { json_free(cwd); return proj_val; }
    } else {
        json_free(proj);
        proj_val = json_null();
    }
    json_t *ba = tmp_args0();
    tmp_asets(ba, "cwd", cwd_s);
    json_t *branch = tmp_call(ctx, "git_probe.branch", ba);
    if (tmp_is_raise(branch)) { json_free(cwd); json_free(proj_val); return branch; }
    json_t *payload = json_object();
    json_set(payload, "project", proj_val);
    tmp_aset(payload, "cwd", cwd);
    json_set(payload, "branch", branch);
    json_free(cwd);
    return tmp_ok(ctx, rid, payload);
}

/* ── _scan_discovered_repos_remote ── */
typedef struct {
    json_t *pairs;      /* owned array of [dirpath, label] */
    json_t *seen;       /* owned array of dirpaths (set membership) */
    const json_t *excludes;
    bool cap;
} tmp_scan_t;

static bool tmp_is_excluded(const char *path, const json_t *excludes) {
    for (size_t i = 0; excludes && i < json_len(excludes); i++) {
        const json_t *ex = json_get(excludes, i);
        const char *exs = tmp_str(ex);
        if (!exs || !exs[0]) continue; /* `if ex` */
        if (!strcmp(path, exs)) return true;
        char *base = tmp_rstrip(exs, "/\\");
        size_t n = strlen(base ? base : "");
        char *pfx = malloc(n + 2);
        bool hit = false;
        if (pfx) {
            memcpy(pfx, base ? base : "", n);
            pfx[n] = '/';
            pfx[n + 1] = '\0';
            hit = tmp_startswith(path, pfx);
            free(pfx);
        }
        free(base);
        if (hit) return true;
    }
    return false;
}

/* one os.walk yield: process (dirpath, dirnames, files), then recurse into the
 * SURVIVING dirnames in order (the caller's dirnames[:] pruning is honored).
 * returns 0 = continue, 1 = cap break. */
static int tmp_walk_visit(tmp_ctx_t *ctx, tmp_scan_t *sc, const char *path,
                          const json_t *node) {
    const json_t *dirs = node ? json_obj_get(node, "dirs") : NULL;
    if (!dirs || dirs->type != JSON_OBJECT) dirs = NULL;
    json_t *dirnames = json_array();
    for (size_t i = 0; dirs && i < dirs->c.count; i++)
        json_append(dirnames, json_string(dirs->c.keys[i]));
    /* process the yield */
    if (tmp_is_excluded(path, sc->excludes)) {
        json_free(dirnames);
        dirnames = json_array(); /* dirnames[:] = [] */
    } else {
        bool has_git = false;
        for (size_t i = 0; i < json_len(dirnames); i++)
            if (!strcmp(tmp_str(json_get(dirnames, i)) ? tmp_str(json_get(dirnames, i)) : "", ".git"))
                has_git = true;
        if (has_git) {
            bool seen = false;
            for (size_t i = 0; i < json_len(sc->seen); i++)
                if (!strcmp(tmp_str(json_get(sc->seen, i)) ? tmp_str(json_get(sc->seen, i)) : "", path))
                    seen = true;
            if (!seen) {
                json_append(sc->seen, json_string(path));
                char *bn = tmp_posix_basename(path);
                json_t *pair = json_array();
                json_append(pair, json_string(path));
                json_append(pair, json_string(bn ? bn : ""));
                free(bn);
                json_append(sc->pairs, pair);
            }
            json_free(dirnames);
            dirnames = json_array(); /* don't hunt nested repos */
        } else {
            json_t *fresh = json_array();
            for (size_t i = 0; i < json_len(dirnames); i++) {
                const char *d = tmp_str(json_get(dirnames, i));
                if (d && d[0] != '.' && strcmp(d, "node_modules") != 0)
                    json_append(fresh, json_string(d));
            }
            json_free(dirnames);
            dirnames = fresh;
        }
    }
    if (json_len(sc->pairs) >= 500) {
        json_free(dirnames);
        sc->cap = true;
        return 1;
    }
    /* recurse into the surviving dirnames (index iteration over the mutated
     * list, exactly as os.walk / the oracle double) */
    for (size_t i = 0; i < json_len(dirnames); i++) {
        const char *d = tmp_str(json_get(dirnames, i));
        const json_t *sub = dirs && d ? json_obj_get(dirs, d) : NULL;
        if (!sub) continue;
        char *child = tmp_posix_join2(path, d);
        int r = tmp_walk_visit(ctx, sc, child ? child : "", sub);
        free(child);
        if (r) { json_free(dirnames); return r; }
    }
    json_free(dirnames);
    return 0;
}

/* PoP: tmp_scan_discovered_repos_remote @ tui_gateway/methods_projects.py:_scan_discovered_repos_remote */
static json_t *tmp_scan_discovered_repos_remote(tmp_ctx_t *ctx, const json_t *args) {
    const json_t *conn = json_obj_get(args, "conn");
    const json_t *policy = json_obj_get(args, "policy");
    json_t *raise = NULL;
    const json_t *roots = tmp_dict_get(policy, "roots", &raise);
    if (raise) return raise;
    if (!tmp_truthy(roots)) roots = NULL;
    const json_t *excludes = tmp_dict_get(policy, "exclude_paths", &raise);
    if (raise) return raise;
    if (!tmp_truthy(excludes)) excludes = NULL;
    if (roots && roots->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(roots));
        return tmp_err_spec("TypeError", msg);
    }
    tmp_scan_t sc;
    sc.pairs = json_array();
    sc.seen = json_array();
    sc.excludes = excludes;
    sc.cap = false;
    bool authoritative = true;
    for (size_t ri = 0; roots && ri < json_len(roots); ri++) {
        const json_t *root = json_get(roots, ri);
        const char *root_s = tmp_str(root) ? tmp_str(root) : "";
        json_t *isd = tmp_isdir(ctx, root_s);
        if (tmp_is_raise(isd)) {
            /* the isdir probe is OUTSIDE the try -- propagate */
            json_free(sc.pairs); json_free(sc.seen);
            return isd;
        }
        bool is_dir = tmp_truthy(isd);
        json_free(isd);
        if (!is_dir) {
            authoritative = false;
            json_t *la = json_array();
            json_append(la, json_copy(root));
            json_t *lg = tmp_log_debug(ctx, "discover_repos scan root missing, skipping: %s", la, false);
            if (tmp_is_raise(lg)) { json_free(sc.pairs); json_free(sc.seen); return lg; }
            json_free(lg);
            continue;
        }
        json_t *wa = tmp_args0();
        tmp_asets(wa, "root", root_s);
        json_t *tree = tmp_call(ctx, "os.walk", wa);
        if (tmp_is_raise(tree)) {
            if (!tmp_exc_caught(tmp_raise_kind(tree))) {
                /* BaseException passes through the except-Exception site */
                json_free(sc.pairs); json_free(sc.seen);
                return tree;
            }
            authoritative = false;
            json_t *la = json_array();
            json_append(la, json_copy(root));
            json_t *lg = tmp_log_debug(ctx, "discover_repos scan failed for root %s", la, true);
            if (tmp_is_raise(lg)) { json_free(tree); json_free(sc.pairs); json_free(sc.seen); return lg; }
            json_free(lg);
            json_free(tree);
        } else {
            if (tree && tree->type == JSON_OBJECT)
                tmp_walk_visit(ctx, &sc, root_s, tree);
            json_free(tree);
        }
        if (json_len(sc.pairs) >= 500) {
            authoritative = false;
            break;
        }
    }
    if (json_len(sc.pairs) > 0) {
        /* policy_key=_repo_discovery_policy_key(policy) evaluated inside try */
        json_t *pk = tmp_repo_discovery_policy_key(ctx, policy, &raise);
        json_t *rec = NULL;
        if (raise) {
            /* except Exception around the record call (kwarg eval included) */
            if (!tmp_exc_caught(tmp_raise_kind(raise))) {
                json_free(sc.pairs); json_free(sc.seen);
                return raise;
            }
            json_t *lg = tmp_log_debug(ctx, "discover_repos cache write failed", NULL, true);
            if (tmp_is_raise(lg)) { json_free(raise); json_free(sc.pairs); json_free(sc.seen); return lg; }
            json_free(lg);
            json_free(raise);
            authoritative = false;
        } else {
            json_t *a = tmp_args0();
            tmp_aset(a, "conn", conn);
            tmp_aset(a, "pairs", sc.pairs);
            tmp_asetb(a, "replace", authoritative);
            tmp_aset(a, "policy_key", pk);
            rec = tmp_call(ctx, "pdb.record_discovered_repos", a);
            json_free(pk);
            if (tmp_is_raise(rec)) {
                if (!tmp_exc_caught(tmp_raise_kind(rec))) {
                    json_free(sc.pairs); json_free(sc.seen);
                    return rec;
                }
                json_t *lg = tmp_log_debug(ctx, "discover_repos cache write failed", NULL, true);
                if (tmp_is_raise(lg)) { json_free(rec); json_free(sc.pairs); json_free(sc.seen); return lg; }
                json_free(lg);
                authoritative = false;
                json_free(rec);
            } else {
                json_free(rec);
            }
        }
    }
    json_free(sc.pairs);
    json_free(sc.seen);
    return json_bool(authoritative);
}

/* ── _discover_repos_payload ── */
/* stable sort of repo dicts by last_active DESC (insertion order on ties) */
static void tmp_repos_sort(json_t *arr) {
    size_t n = json_len(arr);
    for (size_t i = 1; i < n; i++) {
        json_t *cur = arr->c.items[i];
        double key = json_get_num(cur, "last_active", 0);
        size_t j = i;
        while (j > 0 && json_get_num(arr->c.items[j - 1], "last_active", 0) < key) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = cur;
    }
}

static json_t *tmp_agg_get(json_t *repos, const char *root) {
    json_t *agg = json_obj_get(repos, root);
    if (!agg) {
        agg = json_object();
        tmp_asets(agg, "root", root);
        tmp_asets(agg, "label", "");
        tmp_asetn(agg, "sessions", 0);
        tmp_asetn(agg, "last_active", 0);
        json_set(repos, root, agg);
    }
    return agg;
}

/* shared core (also composed by _project_tree_inputs). persistence is a
 * BORROWED double dict or NULL; db/conn borrowed; returns owned list-or-raise. */
static json_t *tmp_discover_core(tmp_ctx_t *ctx, const json_t *db, const json_t *conn,
                                 bool backfill, bool include_cached,
                                 const json_t *persistence_arg, json_t **raise_out) {
    *raise_out = NULL;
    json_t *raise = NULL;
    json_t *repos = json_object();
    json_t *cwd_to_root = json_object();
    json_t *rows = NULL;      /* owned copy of the cwd rows */
    json_t *entries = NULL;   /* owned cache entries */
    json_t *out = NULL;
    const json_t *persistence = NULL;
    json_t *cm = NULL;
    /* with nullcontext(persistence) if persistence is not None else _persistence_for(db) */
    if (persistence_arg && persistence_arg->type != JSON_NULL) {
        persistence = persistence_arg;
    } else {
        json_t *a = tmp_args0();
        tmp_aset(a, "db", db);
        cm = tmp_call(ctx, "persistence_for", a);
        if (tmp_is_raise(cm)) { raise = cm; cm = NULL; goto cleanup; }
        persistence = cm; /* YieldCM: the seam ret IS the yielded handle */
        if (persistence && persistence->type == JSON_NULL) persistence = NULL;
    }
    /* cwd_rows = list(persistence.sessions.distinct_session_cwds()) if ... */
    if (persistence) {
        const json_t *sessions = tmp_attr(persistence, "sessions", &raise);
        if (raise) goto cleanup;
        (void)sessions;
        json_t *dsc = tmp_call(ctx, "persistence.distinct_session_cwds", tmp_args0());
        if (tmp_is_raise(dsc)) { raise = dsc; goto cleanup; }
        rows = dsc;
        if (rows && rows->type != JSON_ARRAY) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(rows));
            raise = tmp_err_spec("TypeError", msg);
            goto cleanup;
        }
    }
    /* git_probe.warm_roots(str(r.cwd or "") for r in cwd_rows) -- the generator
     * is fully consumed building the args BEFORE the seam fires */
    {
        json_t *warm = json_array();
        for (size_t i = 0; rows && i < json_len(rows); i++) {
            const json_t *row = json_get(rows, i);
            const json_t *cwdv = tmp_attr(row, "cwd", &raise);
            if (raise) { json_free(warm); goto cleanup; }
            char *cs = tmp_truthy(cwdv) ? tmp_pystr(cwdv) : tmp_strdup("");
            json_append(warm, json_string(cs ? cs : ""));
            free(cs);
        }
        json_t *wr = tmp_warm_roots(ctx, warm);
        if (tmp_is_raise(wr)) { raise = wr; goto cleanup; }
        json_free(wr);
    }
    for (size_t i = 0; rows && i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        const json_t *cwdv = tmp_attr(row, "cwd", &raise);
        if (raise) goto cleanup;
        char *cwd = tmp_truthy(cwdv) ? tmp_pystr(cwdv) : tmp_strdup("");
        json_t *ca = tmp_args0();
        tmp_asets(ca, "cwd", cwd ? cwd : "");
        json_t *root = tmp_call(ctx, "git_probe.common_repo_root", ca);
        if (tmp_is_raise(root)) { free(cwd); raise = root; goto cleanup; }
        if (!tmp_truthy(root)) { free(cwd); json_free(root); continue; }
        const char *root_s = tmp_str(root) ? tmp_str(root) : "";
        json_set(cwd_to_root, cwd ? cwd : "", json_copy(root));
        json_t *junk = tmp_is_repo_junk(ctx, root);
        if (tmp_is_raise(junk)) { free(cwd); json_free(root); raise = junk; goto cleanup; }
        bool is_junk = tmp_truthy(junk);
        json_free(junk);
        if (is_junk) { free(cwd); json_free(root); continue; }
        json_t *agg = tmp_agg_get(repos, root_s);
        const json_t *sess = tmp_attr(row, "sessions", &raise);
        if (raise) { free(cwd); json_free(root); goto cleanup; }
        json_t *iv = tmp_py_int(tmp_truthy(sess) ? sess : NULL, &raise);
        if (raise) { free(cwd); json_free(root); goto cleanup; }
        json_set(agg, "sessions", json_number(json_get_num(agg, "sessions", 0) + iv->num_val));
        json_free(iv);
        const json_t *la = tmp_attr(row, "last_active", &raise);
        if (raise) { free(cwd); json_free(root); goto cleanup; }
        json_t *fv = tmp_py_float(tmp_truthy(la) ? la : NULL, &raise);
        if (raise) { free(cwd); json_free(root); goto cleanup; }
        double cur = json_get_num(agg, "last_active", 0);
        if (fv->num_val > cur) json_set(agg, "last_active", json_number(fv->num_val));
        json_free(fv);
        free(cwd);
        json_free(root);
    }
    /* if backfill and persistence is not None and not persistence.read_only */
    if (backfill && persistence) {
        const json_t *ro = tmp_attr(persistence, "read_only", &raise);
        if (raise) goto cleanup; /* OUTSIDE the try -- propagates */
        if (!tmp_truthy(ro)) {
            json_t *ba = tmp_args0();
            tmp_aset(ba, "cwd_to_root", cwd_to_root);
            json_t *bf = tmp_call(ctx, "persistence.backfill_repo_roots", ba);
            if (tmp_is_raise(bf)) {
                if (!tmp_exc_caught(tmp_raise_kind(bf))) { raise = bf; goto cleanup; }
                json_t *lg = tmp_log_debug(ctx, "failed to backfill repo roots", NULL, true);
                if (tmp_is_raise(lg)) { json_free(bf); raise = lg; goto cleanup; }
                json_free(lg);
                json_free(bf);
            } else {
                json_free(bf);
            }
        }
    }
    if (include_cached) {
        const json_t *c = NULL;
        json_t *ccm = NULL;
        json_t *cache_raise = NULL;
        if (conn && conn->type != JSON_NULL) {
            c = conn; /* contextlib.nullcontext(conn) -- silent */
        } else {
            ccm = tmp_connect_closing(ctx);
            if (tmp_is_raise(ccm)) {
                if (!tmp_exc_caught(tmp_raise_kind(ccm))) { raise = ccm; ccm = NULL; goto cleanup; }
                json_t *lg = tmp_log_debug(ctx, "failed to read discovered repo cache", NULL, true);
                if (tmp_is_raise(lg)) { json_free(ccm); raise = lg; goto cleanup; }
                json_free(lg);
                json_free(ccm);
                goto cache_done;
            }
            c = ccm;
            if (c && c->type == JSON_NULL) c = NULL;
        }
        json_t *la = tmp_args0();
        tmp_aset(la, "conn", c);
        entries = tmp_call(ctx, "pdb.list_discovered_repos", la);
        json_free(ccm);
        if (tmp_is_raise(entries)) {
            if (!tmp_exc_caught(tmp_raise_kind(entries))) { raise = entries; entries = NULL; goto cleanup; }
            cache_raise = entries;
            entries = NULL;
            goto cache_failed;
        }
        if (entries && entries->type != JSON_ARRAY) {
            /* the for-loop iteration raises INSIDE the try */
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(entries));
            cache_raise = tmp_err_spec("TypeError", msg);
            goto cache_failed;
        }
        for (size_t i = 0; entries && i < json_len(entries); i++) {
            const json_t *entry = json_get(entries, i);
            const json_t *rv = tmp_dict_get(entry, "root", &cache_raise);
            if (cache_raise) goto cache_failed;
            char *root = tmp_truthy(rv) ? tmp_pystr(rv) : tmp_strdup("");
            if (!root || !root[0]) { free(root); continue; }
            json_t *jv = json_string(root);
            json_t *junk = tmp_is_repo_junk(ctx, jv);
            json_free(jv);
            if (tmp_is_raise(junk)) { free(root); cache_raise = junk; goto cache_failed; }
            bool is_junk = tmp_truthy(junk);
            json_free(junk);
            if (is_junk) { free(root); continue; }
            json_t *agg = tmp_agg_get(repos, root);
            const json_t *label = tmp_dict_get(entry, "label", &cache_raise);
            if (cache_raise) { free(root); goto cache_failed; }
            if (tmp_truthy(label)) json_set(agg, "label", json_copy(label));
            free(root);
        }
        goto cache_done;
cache_failed:
        if (!tmp_exc_caught(tmp_raise_kind(cache_raise))) {
            raise = cache_raise;
            goto cleanup;
        }
        {
            json_t *lg = tmp_log_debug(ctx, "failed to read discovered repo cache", NULL, true);
            if (tmp_is_raise(lg)) { json_free(cache_raise); raise = lg; goto cleanup; }
            json_free(lg);
        }
        json_free(cache_raise);
cache_done:
        ;
    }
    /* out = sorted(repos.values(), key=last_active, reverse=True) */
    out = json_array();
    for (size_t i = 0; i < repos->c.count; i++)
        json_append(out, json_copy(repos->c.items[i]));
    tmp_repos_sort(out);
    for (size_t i = 0; i < json_len(out); i++) {
        json_t *r = json_get(out, i);
        const char *label = json_get_str(r, "label", "");
        if (!label || !label[0]) {
            const char *root = json_get_str(r, "root", "");
            char *stripped = tmp_rstrip(root, "/\\");
            char *bn = tmp_posix_basename(stripped ? stripped : "");
            free(stripped);
            const char *fallback = (bn && bn[0]) ? bn : root;
            json_set_hdr(r, "label", fallback);
            free(bn);
        }
    }
cleanup:
    json_free(repos);
    json_free(cwd_to_root);
    json_free(rows);
    json_free(entries);
    json_free(cm);
    if (raise) { *raise_out = raise; json_free(out); return NULL; }
    return out;
}

/* PoP: tmp_discover_repos_payload @ tui_gateway/methods_projects.py:_discover_repos_payload */
static json_t *tmp_discover_repos_payload(tmp_ctx_t *ctx, const json_t *args) {
    json_t *raise = NULL;
    const json_t *persistence = json_obj_get(args, "persistence");
    json_t *out = tmp_discover_core(ctx, json_obj_get(args, "db"), json_obj_get(args, "conn"),
                                  json_get_bool(args, "backfill", true),
                                  json_get_bool(args, "include_cached", true),
                                  persistence, &raise);
    return raise ? raise : out;
}

/* ── _project_tree_inputs / _build_project_tree ── */
static const char *TMP_TREE_EXCLUDED_SOURCES[] = {
    "cron", "kanban", "oneshot",
    "telegram", "discord", "slack", "mattermost", "matrix", "signal", "whatsapp",
    "bluebubbles", "photon", "homeassistant", "email", "sms", "webhook", "api_server",
    "weixin", "wecom", "qqbot", "yuanbao", "dingtalk", "feishu",
};

/* shared core (composed by _build_project_tree). persistence BORROWED double. */
static json_t *tmp_tree_inputs_core(tmp_ctx_t *ctx, const json_t *persistence,
                                    double session_limit, bool include_discovered,
                                    json_t **raise_out) {
    *raise_out = NULL;
    json_t *raise = NULL;
    json_t *sessions = json_array();
    json_t *projects = json_array();
    json_t *discovered = json_array();
    json_t *rows = NULL;
    json_t *policy = NULL;
    json_t *pk = NULL;
    json_t *conn = NULL;
    json_t *ps = NULL;
    json_t *aid = NULL;
    json_t *out = NULL;
    /* SessionQuery(...) ctor seam */
    json_t *qargs = json_object();
    tmp_asets(qargs, "order", "LAST_ACTIVE");
    tmp_asetn(qargs, "min_message_count", 1);
    tmp_asetb(qargs, "include_children", false);
    json_t *excl = json_array();
    for (size_t i = 0; i < sizeof(TMP_TREE_EXCLUDED_SOURCES) / sizeof(TMP_TREE_EXCLUDED_SOURCES[0]); i++)
        json_append(excl, json_string(TMP_TREE_EXCLUDED_SOURCES[i]));
    json_set(qargs, "exclude_sources", excl);
    tmp_asetb(qargs, "include_archived", false);
    json_t *q = tmp_call(ctx, "SessionQuery", json_copy(qargs));
    if (tmp_is_raise(q)) { raise = q; json_free(qargs); goto cleanup; }
    json_free(q);
    /* SessionWindow(limit=session_limit, offset=0) ctor seam */
    json_t *wargs = json_object();
    tmp_asetn(wargs, "limit", session_limit);
    tmp_asetn(wargs, "offset", 0);
    json_t *w = tmp_call(ctx, "SessionWindow", json_copy(wargs));
    if (tmp_is_raise(w)) { raise = w; json_free(qargs); json_free(wargs); goto cleanup; }
    json_free(w);
    /* persistence.sessions.list_sessions(query, window, compact_rows=True) */
    json_t *lsa = tmp_args0();
    json_set(lsa, "query", json_copy(qargs));
    json_set(lsa, "window", json_copy(wargs));
    tmp_asetb(lsa, "compact_rows", true);
    json_free(qargs);
    json_free(wargs);
    rows = tmp_call(ctx, "persistence.list_sessions", lsa);
    if (tmp_is_raise(rows)) { raise = rows; rows = NULL; goto cleanup; }
    if (rows && rows->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(rows));
        raise = tmp_err_spec("TypeError", msg);
        goto cleanup;
    }
    for (size_t i = 0; rows && i < json_len(rows); i++) {
        json_t *ra = tmp_args0();
        tmp_aset(ra, "entry", json_get(rows, i));
        json_t *row = tmp_project_tree_row(ctx, ra);
        json_free(ra);
        if (tmp_is_raise(row)) { raise = row; goto cleanup; }
        json_append(sessions, row);
    }
    /* git_probe.warm_roots(s["cwd"] for s in sessions if s.get("cwd")) */
    {
        json_t *warm = json_array();
        for (size_t i = 0; i < json_len(sessions); i++) {
            const json_t *s = json_get(sessions, i);
            const json_t *cwd = json_obj_get(s, "cwd");
            if (tmp_truthy(cwd)) json_append(warm, json_copy(cwd));
        }
        json_t *wr = tmp_warm_roots(ctx, warm);
        if (tmp_is_raise(wr)) { raise = wr; goto cleanup; }
        json_free(wr);
    }
    policy = tmp_policy_core(ctx, NULL, &raise);
    if (raise) goto cleanup;
    pk = tmp_repo_discovery_policy_key(ctx, policy, &raise);
    if (raise) goto cleanup;
    conn = tmp_connect_closing(ctx);
    if (tmp_is_raise(conn)) { raise = conn; conn = NULL; goto cleanup; }
    if (include_discovered) {
        json_t *pud = tmp_repo_discovery_policy_is_default(ctx, policy);
        if (tmp_is_raise(pud)) { raise = pud; goto cleanup; }
        json_t *ra = tmp_args0();
        tmp_aset(ra, "conn", conn);
        tmp_aset(ra, "policy_key", pk);
        tmp_aset(ra, "preserve_unversioned", pud);
        json_free(pud);
        json_t *rec = tmp_call(ctx, "pdb.reconcile_discovered_repos_policy", ra);
        if (tmp_is_raise(rec)) { raise = rec; goto cleanup; }
        json_free(rec);
    }
    ps = tmp_pdb_list_projects(ctx, conn, false);
    if (tmp_is_raise(ps)) { raise = ps; ps = NULL; goto cleanup; }
    if (ps && ps->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(ps));
        raise = tmp_err_spec("TypeError", msg);
        goto cleanup;
    }
    for (size_t i = 0; ps && i < json_len(ps); i++) {
        json_t *td = tmp_to_dict(ctx);
        if (tmp_is_raise(td)) { raise = td; goto cleanup; }
        json_append(projects, td);
    }
    aid = tmp_pdb_get_active_id(ctx, conn);
    if (tmp_is_raise(aid)) { raise = aid; aid = NULL; goto cleanup; }
    if (include_discovered) {
        json_t *raise2 = NULL;
        const json_t *enabled = tmp_subscript(policy, "enabled", &raise2);
        if (raise2) { raise = raise2; goto cleanup; }
        json_t *disc = tmp_discover_core(ctx, NULL, conn, false, tmp_truthy(enabled),
                                         persistence, &raise2);
        if (raise2) { raise = raise2; goto cleanup; }
        json_free(discovered);
        discovered = disc;
    }
    out = json_array();
    json_append(out, sessions);
    json_append(out, projects);
    json_append(out, discovered);
    json_append(out, aid);
    sessions = projects = discovered = NULL;
    aid = NULL;
cleanup:
    json_free(sessions);
    json_free(projects);
    json_free(discovered);
    json_free(rows);
    json_free(policy);
    json_free(pk);
    json_free(conn);
    json_free(ps);
    json_free(aid);
    if (raise) { *raise_out = raise; json_free(out); return NULL; }
    return out;
}

/* PoP: tmp_project_tree_inputs @ tui_gateway/methods_projects.py:_project_tree_inputs */
static json_t *tmp_project_tree_inputs(tmp_ctx_t *ctx, const json_t *args) {
    json_t *raise = NULL;
    json_t *out = tmp_tree_inputs_core(ctx, json_obj_get(args, "persistence"),
                                       json_get_num(args, "session_limit", 2000),
                                       json_get_bool(args, "include_discovered", true),
                                       &raise);
    return raise ? raise : out;
}

/* PoP: tmp_build_project_tree @ tui_gateway/methods_projects.py:_build_project_tree */
static json_t *tmp_build_project_tree(tmp_ctx_t *ctx, const json_t *args) {
    tmp_ensure_cache(ctx);
    /* _DIR_EXISTS_CACHE.clear() */
    json_set(ctx->state, "_DIR_EXISTS_CACHE", json_object());
    json_t *raise = NULL;
    json_t *cm = NULL;
    json_t *inputs = NULL;
    json_t *out = NULL;
    const json_t *persistence = json_obj_get(args, "persistence");
    if (!persistence || persistence->type == JSON_NULL) {
        json_t *a = tmp_args0();
        tmp_aset(a, "db", json_obj_get(args, "db"));
        cm = tmp_call(ctx, "persistence_for", a);
        if (tmp_is_raise(cm)) return cm;
        persistence = cm;
        if (persistence && persistence->type == JSON_NULL) persistence = NULL;
    }
    inputs = tmp_tree_inputs_core(ctx, persistence,
                                  json_get_num(args, "session_limit", 2000),
                                  json_get_bool(args, "include_discovered", true), &raise);
    if (raise) { json_free(cm); return raise; }
    const json_t *sessions = json_get(inputs, 0);
    const json_t *projects = json_get(inputs, 1);
    const json_t *discovered = json_get(inputs, 2);
    const json_t *active_id = json_get(inputs, 3);
    /* warm_roots([folder paths] + [discovered roots]) */
    json_t *warm = json_array();
    for (size_t i = 0; projects && i < json_len(projects); i++) {
        const json_t *p = json_get(projects, i);
        json_t *raise2 = NULL;
        const json_t *folders = tmp_dict_get(p, "folders", &raise2);
        if (raise2) { json_free(warm); json_free(inputs); json_free(cm); return raise2; }
        if (!tmp_truthy(folders)) continue;
        if (folders->type != JSON_ARRAY) {
            char msg[128];
            snprintf(msg, sizeof msg, "'%s' object is not iterable", tmp_type_name(folders));
            json_free(warm); json_free(inputs); json_free(cm);
            return tmp_err_spec("TypeError", msg);
        }
        for (size_t j = 0; j < json_len(folders); j++) {
            const json_t *f = json_get(folders, j);
            const json_t *pv = tmp_dict_get(f, "path", &raise2);
            if (raise2) { json_free(warm); json_free(inputs); json_free(cm); return raise2; }
            char *ps = tmp_truthy(pv) ? tmp_pystr(pv) : tmp_strdup("");
            json_append(warm, json_string(ps ? ps : ""));
            free(ps);
        }
    }
    for (size_t i = 0; discovered && i < json_len(discovered); i++) {
        const json_t *r = json_get(discovered, i);
        json_t *raise2 = NULL;
        const json_t *rv = tmp_dict_get(r, "root", &raise2);
        if (raise2) { json_free(warm); json_free(inputs); json_free(cm); return raise2; }
        char *rs = tmp_truthy(rv) ? tmp_pystr(rv) : tmp_strdup("");
        json_append(warm, json_string(rs ? rs : ""));
        free(rs);
    }
    json_t *wr = tmp_warm_roots(ctx, warm);
    if (tmp_is_raise(wr)) { json_free(inputs); json_free(cm); return wr; }
    json_free(wr);
    /* project_tree.build_tree(...) -- callables project to name markers */
    json_t *ba = tmp_args0();
    tmp_aset(ba, "projects", projects);
    tmp_aset(ba, "sessions", sessions);
    tmp_aset(ba, "discovered", discovered);
    tmp_asets(ba, "resolve", "git_probe.resolve");
    tmp_asetn(ba, "preview_limit", json_get_num(args, "preview_limit", 3));
    tmp_asetb(ba, "hydrate", json_get_bool(args, "hydrate", false));
    tmp_asets(ba, "is_junk_root", "_is_repo_junk");
    tmp_asets(ba, "is_junk_cwd", "_is_session_cwd_junk");
    tmp_asets(ba, "exists", "_dir_exists_cached");
    json_t *tree = tmp_call(ctx, "project_tree.build_tree", ba);
    if (tmp_is_raise(tree)) { json_free(inputs); json_free(cm); return tree; }
    out = json_array();
    json_append(out, tree);
    json_append(out, json_copy(active_id));
    json_free(inputs);
    json_free(cm);
    return out;
}

/* ── harness entry adapters ((ctx, args) signature over the cores above) ── */
static json_t *tmp_non_workspace_dirs_entry(tmp_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *raise = NULL;
    json_t *out = tmp_non_workspace_dirs(ctx, &raise);
    return raise ? raise : out;
}

static json_t *tmp_is_repo_junk_entry(tmp_ctx_t *ctx, const json_t *args) {
    return tmp_is_repo_junk(ctx, json_obj_get(args, "root"));
}

static json_t *tmp_is_session_cwd_junk_entry(tmp_ctx_t *ctx, const json_t *args) {
    return tmp_is_session_cwd_junk(ctx, json_obj_get(args, "cwd"));
}

static json_t *tmp_repo_discovery_policy_key_entry(tmp_ctx_t *ctx, const json_t *args) {
    json_t *raise = NULL;
    json_t *out = tmp_repo_discovery_policy_key(ctx, json_obj_get(args, "policy"), &raise);
    return raise ? raise : out;
}

static json_t *tmp_repo_discovery_policy_is_default_entry(tmp_ctx_t *ctx, const json_t *args) {
    return tmp_repo_discovery_policy_is_default(ctx, json_obj_get(args, "policy"));
}

static json_t *tmp_require_project_entry(tmp_ctx_t *ctx, const json_t *args) {
    json_t *raise = NULL;
    json_t *out = tmp_require_project(ctx, json_obj_get(args, "conn"),
                                      json_obj_get(args, "params"), &raise);
    return raise ? raise : out;
}

static json_t *tmp_projects_payload_entry(tmp_ctx_t *ctx, const json_t *args) {
    json_t *raise = NULL;
    json_t *out = tmp_projects_payload(ctx, json_obj_get(args, "conn"), &raise);
    return raise ? raise : out;
}