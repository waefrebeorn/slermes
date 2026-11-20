/*
 * plugins_cmd_flow2.c — the UNCLAIMED remainder of hermes_cli/plugins_cmd.py
 * (LIVE authority) after plugins_cmd_flow.c claimed 34 cores/leaves. This second
 * wave ports the admission/selection transaction (_admit_and_save_plugin_sets,
 * _selection_lock, _set_plugin_enabled), the name resolvers (_catalog_installed_dir,
 * _find_plugin_entry, _resolved_user_plugin_dir), the enable-consent gate
 * (_enable_consent), the module accessor (_catalog), the argparse action dispatchers
 * (_action_update, _action_auto_update, _action_pack, _action_idle_tasks) and FOUR
 * nested closures (cmd_enable._refuse_legacy_relay, _get_plugin_toolset_key.
 * _first_toolset / _from_loaded_plugin / _from_manifest_on_disk).
 *
 * Self-contained C11, libjson-only. Claimed siblings (_console, _fail, _plugins_dir,
 * _plugin_selection_version, _refuse_recalled_entry, _discover_all_plugins,
 * _apply_activation, _read_manifest, _get_plugin_toolset_key) and every foreign
 * collaborator (rich Console/markup.escape, hermes_cli.plugins_admission
 * admit_plugin_set_change + AdmissionRefused/DependencyConflict, hermes_constants
 * get_hermes_home/hermes_home_key, threading.Lock, pm.plugins_state
 * read_home_selection, plugins_cmd_git._read_install_metadata, plugins_cmd_catalog
 * catalog_install_record, left_core_migration.legacy_plugin_alias,
 * plugins_cmd_capabilities._declared_capabilities_for_key/_run_capability_consent/
 * _resolve_tool_override_grant, plugins_cmd_update.cmd_update,
 * plugins_cmd_update_all.cmd_update_all, plugins_auto_update.auto_update_state/
 * set_auto_update, plugin_packs.pack_command, plugins_idle_tasks.cmd_idle_tasks,
 * tools.registry.registry.get_entry, hermes_cli.plugins.discover_plugins/
 * get_plugin_manager/get_bundled_plugins_dir, the Path.is_dir stat method) enter
 * through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object
 * (the seam copies what it keeps; pc2_call frees args); the result is a NEW json_t*
 * the caller frees (NULL == None; a scripted JSON null is delivered as a NULL
 * pointer; {"__raise__": kind, "msg": ...} == a raised exception). The harness
 * records {"ev": name, "args": args, "ret": result} per call in order; the oracle's
 * recording stubs emit the identical shape driving LIVE Python with the same
 * scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (pc2_find_plugin_entry inside
 * pc2_set_plugin_enabled / pc2_resolved_user_plugin_dir; pc2_catalog_installed_dir
 * inside pc2_find_plugin_entry; pc2_selection_lock + pc2_admit_and_save_plugin_sets
 * inside pc2_set_plugin_enabled; pc2_first_toolset inside pc2_from_loaded_plugin /
 * pc2_from_manifest_on_disk); the oracle keeps the extracted defs real on the same
 * paths, so their internal seam events appear in BOTH traces.
 *
 * Exception propagation: every pc2 flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except PluginOperationError`
 * (_catalog_installed_dir, _action_auto_update) swallows that family;
 * `except DependencyConflict` / `except AdmissionRefused`
 * (_admit_and_save_plugin_sets) print + re-raise. The BaseException trio
 * (SystemExit / KeyboardInterrupt / GeneratorExit) ALWAYS passes through any
 * `except Exception`/family catch. Fail-closed: a missing/empty seam script yields
 * a RuntimeError raise spec (recorded, then propagated/swallowed exactly as live);
 * no seam installed at all yields the same spec.
 *
 * Path model: paths are verbatim POSIX strings; the oracle seeds PPath doubles
 * (str keeps separators on this Windows host). The C uses a lexical PurePosixPath
 * component model for join / name / equality.
 *
 * State model: _selection_lock owns the module-global _SELECTION_LOCKS registry
 * (home_key -> lock marker); the harness echoes it as out.state.locks. Lock markers
 * are opaque values (the oracle wraps them in a silent context-manager LockDouble;
 * `with lock:` is a single-threaded no-op in C). threading.Lock() is EAGER
 * (dict.setdefault evaluates its factory on every call, even a cache hit).
 * _set_plugin_enabled echoes out.state {locks, enabled, disabled}: the working
 * allow/deny sets built from read_home_selection and mutated by the scripted
 * _apply_activation seam effect ({"state": {"enabled": [...], "disabled": [...]}}).
 * Both state windows are echoed EVEN ON A RAISE (partial mutation history).
 *
 * Fixture domain (ASCII): inputs are ASCII; integral floats normalize to ints on
 * both sides; set-valued inputs (enabled/disabled/aliases/legacy_keys) are lists of
 * strings iterated in fixture order (aliases is an ORDERED list, not a hash-random
 * set); entries are 6-element arrays [name, version, description, source, dir_path,
 * key] with str name/key/dir_path; requires_env/provides_tools/tool_names are lists
 * of strings; read_install_metadata scripts are dicts with str keys (or a
 * PluginOperationError raise / null); catalog records are dicts with a str
 * catalog_name; check_hours is a number; console markers are non-empty dicts or
 * null; args are plain dicts (SimpleNamespace doubles).
 *
 * Seam name table (name -> live call):
 *   Console                       _console() [claimed] {} -> marker
 *   fail                          _fail(console, message) [claimed] {console,message}
 *   console_print                 console.print(*values) {args:[...]}
 *   markup_escape                 rich.markup.escape(text) {text}
 *   _plugins_dir                  _plugins_dir() [claimed] {} -> path str
 *   admit_plugin_set_change       plugins_admission.admit_plugin_set_change(...)
 *                                 {enabled,disabled,active_plugins_dir,extra_dirs,
 *                                  expected_config,plugin,settled}
 *   hermes_home_key               hermes_constants.hermes_home_key() {} -> str
 *   threading_lock                threading.Lock() {} -> lock marker
 *   plugin_selection_version      _plugin_selection_version() [claimed] {} -> str
 *   get_hermes_home               hermes_constants.get_hermes_home() {} -> path str
 *   read_home_selection           pm.plugins_state.read_home_selection(home) {home}
 *   apply_activation              _apply_activation(enabled,disabled,name,aliases,enable=)
 *                                 [claimed] {enabled,disabled,name,aliases,enable}
 *                                 (scripted ret may carry a "state" effect)
 *   refuse_recalled_entry         _refuse_recalled_entry(entry) [claimed] {entry}
 *   discover_all_plugins          _discover_all_plugins() [claimed] {} -> [entries]
 *   legacy_plugin_alias           left_core_migration.legacy_plugin_alias(name) {name}
 *   read_install_metadata         plugins_cmd_git._read_install_metadata() {} -> dict
 *   catalog_install_record        plugins_cmd_catalog.catalog_install_record(dir) {plugin_dir}
 *   is_dir                        Path.is_dir() {path}
 *   declared_capabilities_for_key plugins_cmd_capabilities._declared_capabilities_for_key(key) {key}
 *   run_capability_consent        plugins_cmd_capabilities._run_capability_consent(...)
 *                                 {console,key,declared_caps,context}
 *   resolve_tool_override_grant   plugins_cmd_capabilities._resolve_tool_override_grant(...)
 *                                 {console,key,allow_tool_override}
 *   catalog_module                `from hermes_cli import plugins_cmd_catalog` {} -> marker
 *   cmd_update                    plugins_cmd_update.cmd_update(name, force=) {name,force}
 *   cmd_update_all                plugins_cmd_update_all.cmd_update_all(yes=) {yes}
 *   auto_update_state             plugins_auto_update.auto_update_state() {}
 *   set_auto_update               plugins_auto_update.set_auto_update(on) {on}
 *   pack_command                  plugin_packs.pack_command(args) {args}
 *   cmd_idle_tasks                plugins_idle_tasks.cmd_idle_tasks(name, pause=, resume=)
 *                                 {name,pause,resume}
 *   registry_get_entry            tools.registry.registry.get_entry(t) {name}
 *   discover_plugins              hermes_cli.plugins.discover_plugins() {}
 *   get_plugin_manager            hermes_cli.plugins.get_plugin_manager() {}
 *   read_manifest                 _read_manifest(dir) [claimed] {plugin_dir}
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "hermes_json.h"

/* ── universal seam plumbing ── */

typedef json_t *(*pc2_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct pc2_io {
    pc2_seam_fn call;
    void *ctx;
    const json_t *env;   /* borrowed ambient values (unused; kept for shape parity) */
} pc2_io_t;

static int pc2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *pc2_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *pc2_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* ── FULL builtin exception hierarchy (parent table) + the module classes ──
 * pc2_exc_parent walks to Exception / BaseException. Unknown kinds default to
 * Exception (a synthetic Exception subclass), so `except Exception` catches them;
 * the BaseException trio walks to BaseException only and passes through.
 * IOError/EnvironmentError/WindowsError are OSError aliases; ExceptionGroup routes
 * through Exception to keep the `except Exception` contract. The plugins_cmd module
 * classes and the plugins_admission classes are appended so family catches
 * transcribe exactly (DependencyConflict -> AdmissionRefused -> RuntimeError). */
static const struct { const char *kind; const char *parent; } PC2_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"Exception", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
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
    {"IOError", "OSError"},
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
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    /* hermes_cli/plugins_cmd.py module classes */
    {"PluginOperationError", "Exception"},
    {"PluginSourceMoved", "PluginOperationError"},
    {"PluginScanBlocked", "PluginOperationError"},
    /* hermes_cli/plugins_admission.py classes */
    {"AdmissionRefused", "RuntimeError"},
    {"DependencyConflict", "AdmissionRefused"},
};
#define PC2_N_EXC ((int)(sizeof PC2_EXC_PARENTS / sizeof PC2_EXC_PARENTS[0]))

static const char *pc2_exc_parent(const char *k) {
    for (int i = 0; i < PC2_N_EXC; i++)
        if (strcmp(PC2_EXC_PARENTS[i].kind, k) == 0) return PC2_EXC_PARENTS[i].parent;
    return "Exception";   /* unknown kind: a synthetic Exception subclass */
}

static int pc2_kind_is_a(const char *k, const char *ancestor) {
    char cur[128];
    snprintf(cur, sizeof cur, "%s", k);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, ancestor) == 0) return 1;
        if (strcmp(cur, "BaseException") == 0) return 0;
        const char *p = pc2_exc_parent(cur);
        if (!p || strcmp(p, cur) == 0) return 0;
        snprintf(cur, sizeof cur, "%s", p);
    }
    return 0;
}

static int pc2_is_plugin_operation_error(const char *k) { return pc2_kind_is_a(k, "PluginOperationError"); }
static int pc2_is_admission_refused(const char *k) { return pc2_kind_is_a(k, "AdmissionRefused"); }
static int pc2_is_dependency_conflict(const char *k) { return pc2_kind_is_a(k, "DependencyConflict"); }

static json_t *pc2_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *pc2_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return pc2_raise(kind, buf);
}

static char *pc2_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return strdup("");
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* Universal seam call: args owned by the caller (freed here); result owned by
 * the caller. Fail-closed when no seam is installed. A JSON null result is
 * delivered as a NULL pointer (Python None). */
static json_t *pc2_call(pc2_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = pc2_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int pc2_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static int pc2_present(const json_t *v) { return v && v->type != JSON_NULL; }

static const char *pc2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python type name for AttributeError/TypeError messages (JSON model). */
static const char *pc2_type_name_of(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* str() of a scalar JSON value (containers out of the str() fixture domain). */
static char *pc2_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) {
            snprintf(buf, sizeof buf, "%.0f", d);
            return strdup(buf);
        }
        snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* Python format(x, 'g') — C %g matches on the probe corpus (6 sig digits,
 * trailing-zero strip, scientific for exp < -4 or >= 16). */
static char *pc2_format_g(double v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%g", v);
    return strdup(buf);
}

/* TypeError message for value[key] on a non-dict (py3.14 probed). */
static json_t *pc2_subscript_error(const json_t *v) {
    if (v && v->type == JSON_STRING)
        return pc2_raise("TypeError", "string indices must be integers, not 'str'");
    if (v && v->type == JSON_ARRAY)
        return pc2_raise("TypeError", "list indices must be integers or slices, not str");
    return pc2_raise_fmt("TypeError", "'%s' object is not subscriptable", pc2_type_name_of(v));
}
/* ── lexical PurePosixPath component model ── */

typedef struct {
    char *root;     /* "", "/" or "//" */
    char **parts;   /* owned components (no empty / '.'; '..' kept) */
    int n;
    int cap;
} pc2_path_t;

static void pc2_path_init(pc2_path_t *p) { p->root = strdup(""); p->parts = NULL; p->n = 0; p->cap = 0; }

static void pc2_path_free(pc2_path_t *p) {
    free(p->root);
    for (int i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->root = NULL; p->parts = NULL; p->n = 0; p->cap = 0;
}

static void pc2_path_push(pc2_path_t *p, const char *s, size_t len) {
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 8;
        p->parts = (char **)realloc(p->parts, (size_t)p->cap * sizeof(char *));
    }
    char *c = (char *)malloc(len + 1);
    memcpy(c, s, len); c[len] = '\0';
    p->parts[p->n++] = c;
}

static void pc2_path_parse(pc2_path_t *p, const char *s) {
    pc2_path_init(p);
    if (!s) s = "";
    size_t n = strlen(s);
    size_t lead = 0;
    while (lead < n && s[lead] == '/') lead++;
    if (lead == 2) { free(p->root); p->root = strdup("//"); }
    else if (lead >= 1) { free(p->root); p->root = strdup("/"); }
    size_t i = lead;
    while (i < n) {
        size_t j = i;
        while (j < n && s[j] != '/') j++;
        size_t len = j - i;
        if (len == 1 && s[i] == '.') { /* drop */ }
        else if (len > 0) pc2_path_push(p, s + i, len);
        i = j;
        while (i < n && s[i] == '/') i++;
    }
}

static char *pc2_path_str(const pc2_path_t *p) {
    if (p->n == 0 && p->root[0] == '\0') return strdup(".");
    size_t cap = strlen(p->root) + 1;
    for (int i = 0; i < p->n; i++) cap += strlen(p->parts[i]) + 1;
    char *out = (char *)malloc(cap + 1);
    if (!out) return NULL;
    size_t o = 0;
    size_t rl = strlen(p->root);
    memcpy(out + o, p->root, rl); o += rl;
    for (int i = 0; i < p->n; i++) {
        if (o > 0 && out[o - 1] != '/') out[o++] = '/';
        size_t pl = strlen(p->parts[i]);
        memcpy(out + o, p->parts[i], pl); o += pl;
    }
    out[o] = '\0';
    return out;
}

static void pc2_path_copy(pc2_path_t *dst, const pc2_path_t *src) {
    pc2_path_init(dst);
    free(dst->root);
    dst->root = strdup(src->root);
    for (int i = 0; i < src->n; i++) pc2_path_push(dst, src->parts[i], strlen(src->parts[i]));
}

static char *pc2_posix_join(const char *a, const char *b) {
    pc2_path_t pa, pb, out;
    pc2_path_parse(&pa, a);
    pc2_path_parse(&pb, b);
    if (pb.root[0] != '\0') {
        pc2_path_copy(&out, &pb);
    } else {
        pc2_path_copy(&out, &pa);
        for (int i = 0; i < pb.n; i++) pc2_path_push(&out, pb.parts[i], strlen(pb.parts[i]));
    }
    char *s = pc2_path_str(&out);
    pc2_path_free(&pa); pc2_path_free(&pb); pc2_path_free(&out);
    return s;
}

/* PurePosixPath equality (normalized components, case-sensitive). */
static int pc2_path_eq(const char *a, const char *b) {
    pc2_path_t pa, pb;
    pc2_path_parse(&pa, a);
    pc2_path_parse(&pb, b);
    int eq = (strcmp(pa.root, pb.root) == 0 && pa.n == pb.n);
    for (int i = 0; eq && i < pa.n; i++)
        if (strcmp(pa.parts[i], pb.parts[i]) != 0) eq = 0;
    pc2_path_free(&pa); pc2_path_free(&pb);
    return eq;
}

static json_t *pc2_path_args(const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    return a;
}

/* key.split("/")[-1] — the bare leaf. malloc'd. */
static char *pc2_leaf(const char *key) {
    if (!key) return strdup("");
    const char *slash = strrchr(key, '/');
    return strdup(slash ? slash + 1 : key);
}

/* ── string-set helpers (sets of ASCII strings; sorted-unique output) ── */

typedef struct {
    char **items;
    int n;
    int cap;
} pc2_strset_t;

static void pc2_ss_init(pc2_strset_t *s) { s->items = NULL; s->n = 0; s->cap = 0; }

static void pc2_ss_free(pc2_strset_t *s) {
    for (int i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL; s->n = 0; s->cap = 0;
}

static int pc2_ss_contains(const pc2_strset_t *s, const char *x) {
    for (int i = 0; i < s->n; i++) if (strcmp(s->items[i], x) == 0) return 1;
    return 0;
}

static void pc2_ss_add(pc2_strset_t *s, const char *x) {
    if (pc2_ss_contains(s, x)) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->items = (char **)realloc(s->items, (size_t)s->cap * sizeof(char *));
    }
    s->items[s->n++] = strdup(x);
}

static int pc2_ss_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void pc2_ss_sort(pc2_strset_t *s) {
    if (s->n > 1) qsort(s->items, (size_t)s->n, sizeof(char *), pc2_ss_cmp);
}

/* Load a JSON array of strings into a set (non-string items skipped). */
static void pc2_ss_from_array(pc2_strset_t *s, const json_t *arr) {
    pc2_ss_init(s);
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arr); i++) {
            const json_t *v = json_get(arr, i);
            if (v && v->type == JSON_STRING) pc2_ss_add(s, pc2_jstr(v));
        }
    }
}

static json_t *pc2_ss_to_array(pc2_strset_t *s) {
    pc2_ss_sort(s);
    json_t *a = json_array();
    for (int i = 0; i < s->n; i++) json_append(a, json_string(s->items[i]));
    return a;
}

/* sorted-unique array projection of a set-valued input list. */
static json_t *pc2_sorted_set_array(const json_t *arr) {
    pc2_strset_t s;
    pc2_ss_from_array(&s, arr);
    json_t *out = pc2_ss_to_array(&s);
    pc2_ss_free(&s);
    return out;
}

/* ── console / fail seams ── */

/* console.print(message) seam: {args:[message]}. */
static json_t *pc2_console_print(pc2_io_t *io, const json_t *console, const char *message) {
    if (!pc2_present(console))
        return pc2_raise("AttributeError", "'NoneType' object has no attribute 'print'");
    json_t *a = json_object();
    json_t *vals = json_array();
    json_append(vals, json_string(message ? message : ""));
    json_set(a, "args", vals);
    return pc2_call(io, "console_print", a);
}

/* _fail(console, message) [claimed] seam: {console,message}. */
static json_t *pc2_fail_seam(pc2_io_t *io, const json_t *console, const char *message) {
    json_t *a = json_object();
    if (pc2_present(console)) json_set(a, "console", json_copy(console));
    json_set(a, "message", json_string(message ? message : ""));
    return pc2_call(io, "fail", a);
}

/* ── admission transaction ── */

/* PoP: pc2_admit_and_save_plugin_sets @ hermes_cli/plugins_cmd.py:_admit_and_save_plugin_sets */
static json_t *pc2_admit_and_save_plugin_sets(pc2_io_t *io, const json_t *enabled, const json_t *disabled,
                                              const json_t *extra_dirs, const json_t *console,
                                              const char *action, const json_t *expected_config,
                                              const json_t *plugin, const json_t *settled) {
    /* active_plugins_dir=_plugins_dir() fires BEFORE the admit seam (kwarg eval). */
    json_t *pdir = pc2_call(io, "_plugins_dir", json_object());
    if (pc2_is_raise(pdir)) return pdir;
    json_t *a = json_object();
    json_set(a, "enabled", pc2_sorted_set_array(enabled));
    json_set(a, "disabled", pc2_sorted_set_array(disabled));
    json_set(a, "active_plugins_dir", json_string(pc2_jstr(pdir)));
    json_free(pdir);
    json_set(a, "extra_dirs", (extra_dirs && extra_dirs->type == JSON_ARRAY) ? json_copy(extra_dirs) : json_array());
    if (pc2_present(expected_config)) json_set(a, "expected_config", json_copy(expected_config));
    if (pc2_present(plugin)) json_set(a, "plugin", json_copy(plugin));
    json_set(a, "settled", (settled && settled->type == JSON_ARRAY) ? json_copy(settled) : json_array());
    json_t *r = pc2_call(io, "admit_plugin_set_change", a);
    if (!pc2_is_raise(r)) { json_free(r); return NULL; }
    const char *k = pc2_raise_kind(r);
    /* except DependencyConflict (checked first: it subclasses AdmissionRefused) */
    if (pc2_is_dependency_conflict(k)) {
        if (pc2_present(console)) {
            json_t *ea = json_object();
            json_set(ea, "text", json_string(pc2_raise_msg(r)));
            json_t *esc = pc2_call(io, "markup_escape", ea);
            if (pc2_is_raise(esc)) { json_free(r); return esc; }
            char *m1 = pc2_fmt("[red]\xe2\x9c\x97[/red] %s", pc2_jstr(esc));
            json_free(esc);
            json_t *p1 = pc2_console_print(io, console, m1);
            free(m1);
            if (pc2_is_raise(p1)) { json_free(r); return p1; }
            json_free(p1);
            json_t *p2 = pc2_console_print(io, console,
                "[dim]config.yaml and the active environment are unchanged.[/dim]");
            if (pc2_is_raise(p2)) { json_free(r); return p2; }
            json_free(p2);
        }
        return r;   /* re-raise the original */
    }
    /* except AdmissionRefused */
    if (pc2_is_admission_refused(k)) {
        if (pc2_present(console)) {
            char *m1 = pc2_fmt("[red]\xe2\x9c\x97[/red] %s refused: %s",
                               action ? action : "enable", pc2_raise_msg(r));
            json_t *p1 = pc2_console_print(io, console, m1);
            free(m1);
            if (pc2_is_raise(p1)) { json_free(r); return p1; }
            json_free(p1);
            json_t *p2 = pc2_console_print(io, console,
                "[dim]config.yaml and the active environment are unchanged. "
                "Run `hermes pm install` to resolve dependencies, then retry.[/dim]");
            if (pc2_is_raise(p2)) { json_free(r); return p2; }
            json_free(p2);
        }
        return r;   /* re-raise the original */
    }
    return r;   /* other kinds propagate uncaught */
}

/* ── selection lock registry ── */

/* PoP: pc2_selection_lock @ hermes_cli/plugins_cmd.py:_selection_lock */
static json_t *pc2_selection_lock(pc2_io_t *io, json_t *locks) {
    json_t *key = pc2_call(io, "hermes_home_key", json_object());
    if (pc2_is_raise(key)) return key;
    /* dict.setdefault(k, threading.Lock()) evaluates the factory EAGERLY. */
    json_t *lock = pc2_call(io, "threading_lock", json_object());
    if (pc2_is_raise(lock)) { json_free(key); return lock; }
    const char *ks = pc2_jstr(key);
    json_free(key);
    json_t *existing = json_obj_get(locks, ks);
    if (existing) { json_free(lock); return json_copy(existing); }
    json_set(locks, ks, lock);   /* takes ownership */
    return json_copy(lock);
}

/* forward declarations (composed ported-to-ported calls defined below) */
static json_t *pc2_find_plugin_entry(pc2_io_t *io, const char *name);
static json_t *pc2_catalog_installed_dir(pc2_io_t *io, const char *name);
static json_t *pc2_first_toolset(pc2_io_t *io, const json_t *tool_names);

/* PoP: pc2_set_plugin_enabled @ hermes_cli/plugins_cmd.py:_set_plugin_enabled */
static json_t *pc2_set_plugin_enabled(pc2_io_t *io, const char *name, int enable,
                                      const json_t *aliases, const json_t *console,
                                      json_t *locks, json_t *work) {
    if (enable) {
        json_t *entry = pc2_find_plugin_entry(io, name);
        if (pc2_is_raise(entry)) return entry;
        json_t *a = json_object();
        if (pc2_present(entry)) json_set(a, "entry", json_copy(entry));
        json_free(entry);
        json_t *rr = pc2_call(io, "refuse_recalled_entry", a);
        if (pc2_is_raise(rr)) return rr;
        json_free(rr);
    }
    json_t *lk = pc2_selection_lock(io, locks);
    if (pc2_is_raise(lk)) return lk;
    json_free(lk);   /* with lock: single-threaded no-op */
    json_t *ver = pc2_call(io, "plugin_selection_version", json_object());
    if (pc2_is_raise(ver)) return ver;
    json_t *home = pc2_call(io, "get_hermes_home", json_object());
    if (pc2_is_raise(home)) { json_free(ver); return home; }
    json_t *ha = json_object();
    json_set(ha, "home", json_string(pc2_jstr(home)));
    json_free(home);
    json_t *config = pc2_call(io, "read_home_selection", ha);
    if (pc2_is_raise(config)) { json_free(ver); return config; }
    /* config = read_home_selection(...) or {} */
    json_t *cfg = pc2_truthy(config) ? config : NULL;
    if (cfg && cfg->type != JSON_OBJECT) {
        json_t *e = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", pc2_type_name_of(cfg));
        json_free(config); json_free(ver); return e;
    }
    const json_t *plugins = cfg ? json_obj_get(cfg, "plugins") : NULL;
    const json_t *pcfg = pc2_truthy(plugins) ? plugins : NULL;
    if (pcfg && pcfg->type != JSON_OBJECT) {
        json_t *e = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", pc2_type_name_of(pcfg));
        json_free(config); json_free(ver); return e;
    }
    pc2_strset_t en, dis;
    pc2_ss_init(&en); pc2_ss_init(&dis);
    const json_t *en_v = pcfg ? json_obj_get(pcfg, "enabled") : NULL;
    const json_t *dis_v = pcfg ? json_obj_get(pcfg, "disabled") : NULL;
    if (pc2_truthy(en_v) && en_v->type == JSON_ARRAY) pc2_ss_from_array(&en, en_v);
    if (pc2_truthy(dis_v) && dis_v->type == JSON_ARRAY) pc2_ss_from_array(&dis, dis_v);
    json_free(config);
    json_set(work, "enabled", pc2_ss_to_array(&en));
    json_set(work, "disabled", pc2_ss_to_array(&dis));
    /* _apply_activation [claimed] seam with a scripted state effect. */
    json_t *aa = json_object();
    json_set(aa, "enabled", pc2_ss_to_array(&en));
    json_set(aa, "disabled", pc2_ss_to_array(&dis));
    json_set(aa, "name", json_string(name ? name : ""));
    json_set(aa, "aliases", (aliases && aliases->type == JSON_ARRAY) ? json_copy(aliases) : json_array());
    json_set(aa, "enable", json_bool(enable ? true : false));
    json_t *ar = pc2_call(io, "apply_activation", aa);
    if (pc2_is_raise(ar)) { pc2_ss_free(&en); pc2_ss_free(&dis); json_free(ver); return ar; }
    if (ar && ar->type == JSON_OBJECT) {
        const json_t *st = json_obj_get(ar, "state");
        if (st && st->type == JSON_OBJECT) {
            const json_t *se = json_obj_get(st, "enabled");
            const json_t *sd = json_obj_get(st, "disabled");
            if (se && se->type == JSON_ARRAY) { pc2_ss_free(&en); pc2_ss_from_array(&en, se); }
            if (sd && sd->type == JSON_ARRAY) { pc2_ss_free(&dis); pc2_ss_from_array(&dis, sd); }
        }
    }
    json_free(ar);
    json_set(work, "enabled", pc2_ss_to_array(&en));
    json_set(work, "disabled", pc2_ss_to_array(&dis));
    /* compose _admit_and_save_plugin_sets REAL */
    char *action = pc2_fmt("%s '%s'", enable ? "Enable" : "Disable", name ? name : "");
    json_t *settled = json_array();
    json_append(settled, json_string(name ? name : ""));
    if (aliases && aliases->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(aliases); i++) json_append(settled, json_copy(json_get(aliases, i)));
    json_t *en_arr = pc2_ss_to_array(&en);
    json_t *dis_arr = pc2_ss_to_array(&dis);
    json_t *plugin_arg = enable ? json_string(name ? name : "") : NULL;
    json_t *r = pc2_admit_and_save_plugin_sets(io, en_arr, dis_arr, NULL, console, action, ver, plugin_arg, settled);
    free(action); json_free(settled); json_free(en_arr); json_free(dis_arr);
    json_free(plugin_arg); json_free(ver);
    pc2_ss_free(&en); pc2_ss_free(&dis);
    if (pc2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── name resolvers ── */

/* PoP: pc2_catalog_installed_dir @ hermes_cli/plugins_cmd.py:_catalog_installed_dir */
static json_t *pc2_catalog_installed_dir(pc2_io_t *io, const char *name) {
    json_t *md = pc2_call(io, "read_install_metadata", json_object());
    if (pc2_is_raise(md)) {
        if (pc2_is_plugin_operation_error(pc2_raise_kind(md))) { json_free(md); return NULL; }
        return md;
    }
    /* keys = list(md): dict keys in insertion order (array/str domains too). */
    char **keys = NULL; int nkeys = 0;
    if (!md) return pc2_raise("TypeError", "'NoneType' object is not iterable");
    if (md->type == JSON_OBJECT) {
        nkeys = (int)md->c.count;
        keys = (char **)malloc((size_t)(nkeys ? nkeys : 1) * sizeof(char *));
        for (int i = 0; i < nkeys; i++) keys[i] = strdup(md->c.keys[i]);
    } else if (md->type == JSON_ARRAY) {
        nkeys = (int)json_len(md);
        keys = (char **)malloc((size_t)(nkeys ? nkeys : 1) * sizeof(char *));
        for (int i = 0; i < nkeys; i++) keys[i] = strdup(pc2_jstr(json_get(md, i)));
    } else if (md->type == JSON_STRING) {
        nkeys = (int)strlen(md->str_val);
        keys = (char **)malloc((size_t)(nkeys ? nkeys : 1) * sizeof(char *));
        for (int i = 0; i < nkeys; i++) { keys[i] = (char *)malloc(2); keys[i][0] = md->str_val[i]; keys[i][1] = '\0'; }
    } else {
        json_t *e = pc2_raise_fmt("TypeError", "'%s' object is not iterable", pc2_type_name_of(md));
        json_free(md); return e;
    }
    json_free(md);
    json_t *ret = NULL;
    char **hits = NULL; int nhits = 0;
    for (int i = 0; i < nkeys && !ret; i++) {
        json_t *root = pc2_call(io, "_plugins_dir", json_object());
        if (pc2_is_raise(root)) { ret = root; break; }
        char *target = pc2_posix_join(pc2_jstr(root), keys[i]);
        json_free(root);
        json_t *isd = pc2_call(io, "is_dir", pc2_path_args(target));
        if (pc2_is_raise(isd)) { free(target); ret = isd; break; }
        int isdir = pc2_truthy(isd); json_free(isd);
        if (!isdir) { free(target); continue; }
        json_t *ra = json_object();
        json_set(ra, "plugin_dir", json_string(target));
        json_t *rec = pc2_call(io, "catalog_install_record", ra);
        if (pc2_is_raise(rec)) { free(target); ret = rec; break; }
        if (!pc2_truthy(rec)) { json_free(rec); free(target); continue; }
        if (rec->type != JSON_OBJECT) {
            ret = pc2_subscript_error(rec);
            json_free(rec); free(target); break;
        }
        if (!json_has(rec, "catalog_name")) {
            ret = pc2_raise("KeyError", "'catalog_name'");
            json_free(rec); free(target); break;
        }
        const json_t *cn = json_obj_get(rec, "catalog_name");
        if (cn && cn->type == JSON_STRING && strcmp(cn->str_val, name ? name : "") == 0) {
            hits = (char **)realloc(hits, (size_t)(nhits + 1) * sizeof(char *));
            hits[nhits++] = strdup(target);
        }
        json_free(rec); free(target);
    }
    for (int i = 0; i < nkeys; i++) free(keys[i]);
    free(keys);
    if (!ret && nhits == 1) ret = json_string(hits[0]);
    for (int i = 0; i < nhits; i++) free(hits[i]);
    free(hits);
    return ret;
}

/* PoP: pc2_find_plugin_entry @ hermes_cli/plugins_cmd.py:_find_plugin_entry */
static json_t *pc2_find_plugin_entry(pc2_io_t *io, const char *name) {
    json_t *entries = pc2_call(io, "discover_all_plugins", json_object());
    if (pc2_is_raise(entries)) return entries;
    json_t *la = json_object();
    json_set(la, "name", json_string(name ? name : ""));
    json_t *alias = pc2_call(io, "legacy_plugin_alias", la);
    if (pc2_is_raise(alias)) { json_free(entries); return alias; }
    const char *target = pc2_truthy(alias) ? pc2_jstr(alias) : (name ? name : "");
    /* exact = next((e for e in entries if target in (e[0], e[5])), None) */
    json_t *exact = NULL;
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const json_t *e = json_get(entries, i);
            if (e && e->type == JSON_ARRAY && json_len(e) >= 6) {
                const json_t *n0 = json_get(e, 0);
                const json_t *n5 = json_get(e, 5);
                int hit = (n0 && n0->type == JSON_STRING && strcmp(n0->str_val, target) == 0)
                       || (n5 && n5->type == JSON_STRING && strcmp(n5->str_val, target) == 0);
                if (hit) { exact = json_copy(e); break; }
            }
        }
    }
    json_free(alias);
    if (exact) { json_free(entries); return exact; }
    /* leaf_matches = [e for e in entries if name == e[5].split("/")[-1]] (atomic) */
    json_t *leaf_first = NULL; int leaf_n = 0;
    if (entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const json_t *e = json_get(entries, i);
            if (e && e->type == JSON_ARRAY && json_len(e) >= 6) {
                const json_t *k = json_get(e, 5);
                if (!k || k->type != JSON_STRING) {
                    json_t *err = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'split'", pc2_type_name_of(k));
                    json_free(entries); json_free(leaf_first); return err;
                }
                char *leaf = pc2_leaf(k->str_val);
                int match = strcmp(leaf, name ? name : "") == 0;
                free(leaf);
                if (match) { leaf_n++; if (!leaf_first) leaf_first = json_copy(e); }
            }
        }
    }
    if (leaf_n > 0) {
        json_free(entries);
        if (leaf_n == 1) return leaf_first;
        json_free(leaf_first); return NULL;
    }
    json_free(leaf_first);
    /* target = _catalog_installed_dir(name) REAL */
    json_t *cat = pc2_catalog_installed_dir(io, name);
    if (pc2_is_raise(cat)) { json_free(entries); return cat; }
    json_t *found = NULL;
    if (pc2_present(cat) && entries && entries->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const json_t *e = json_get(entries, i);
            if (e && e->type == JSON_ARRAY && json_len(e) >= 6) {
                const json_t *dp = json_get(e, 4);
                if (pc2_truthy(dp)) {
                    char *ds = pc2_py_str(dp);
                    int eq = pc2_path_eq(ds, pc2_jstr(cat));
                    free(ds);
                    if (eq) { found = json_copy(e); break; }
                }
            }
        }
    }
    json_free(cat); json_free(entries);
    return found;
}

/* PoP: pc2_resolved_user_plugin_dir @ hermes_cli/plugins_cmd.py:_resolved_user_plugin_dir */
static json_t *pc2_resolved_user_plugin_dir(pc2_io_t *io, const char *name) {
    json_t *entry = pc2_find_plugin_entry(io, name);
    if (pc2_is_raise(entry)) return entry;
    char *found = NULL;
    if (pc2_truthy(entry) && entry->type == JSON_ARRAY && json_len(entry) >= 6) {
        const json_t *src = json_get(entry, 3);
        const json_t *dp = json_get(entry, 4);
        const char *s = pc2_jstr(src);
        if ((strcmp(s, "user") == 0 || strcmp(s, "git") == 0) && pc2_truthy(dp))
            found = pc2_py_str(dp);
    }
    json_free(entry);
    if (!found) return NULL;
    json_t *isd = pc2_call(io, "is_dir", pc2_path_args(found));
    if (pc2_is_raise(isd)) { free(found); return isd; }
    int isdir = pc2_truthy(isd); json_free(isd);
    if (!isdir) { free(found); return NULL; }
    json_t *out = json_string(found);
    free(found);
    return out;
}
/* ── enable consent + module accessor ── */

/* PoP: pc2_enable_consent @ hermes_cli/plugins_cmd.py:_enable_consent */
static json_t *pc2_enable_consent(pc2_io_t *io, const json_t *console, const char *key,
                                  const json_t *allow_tool_override) {
    json_t *ka = json_object();
    json_set(ka, "key", json_string(key ? key : ""));
    json_t *caps = pc2_call(io, "declared_capabilities_for_key", ka);
    if (pc2_is_raise(caps)) return caps;
    if (pc2_truthy(caps)) {
        json_t *a = json_object();
        if (pc2_present(console)) json_set(a, "console", json_copy(console));
        json_set(a, "key", json_string(key ? key : ""));
        json_set(a, "declared_caps", json_copy(caps));
        json_set(a, "context", json_string("enable"));
        json_t *r = pc2_call(io, "run_capability_consent", a);
        if (pc2_is_raise(r)) { json_free(caps); return r; }
        json_free(r);
    }
    json_free(caps);
    /* if allow_tool_override is not None (identity: present-null skips, false fires) */
    if (pc2_present(allow_tool_override)) {
        json_t *a = json_object();
        if (pc2_present(console)) json_set(a, "console", json_copy(console));
        json_set(a, "key", json_string(key ? key : ""));
        json_set(a, "allow_tool_override", json_copy(allow_tool_override));
        json_t *r = pc2_call(io, "resolve_tool_override_grant", a);
        if (pc2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* PoP: pc2_catalog @ hermes_cli/plugins_cmd.py:_catalog */
static json_t *pc2_catalog(pc2_io_t *io) {
    return pc2_call(io, "catalog_module", json_object());
}

/* ── argparse action dispatchers ── */

/* PoP: pc2_action_update @ hermes_cli/plugins_cmd.py:_action_update */
static json_t *pc2_action_update(pc2_io_t *io, const json_t *args) {
    const json_t *name = args ? json_obj_get(args, "name") : NULL;
    if (!name)
        return pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'name'");
    if (pc2_truthy(name)) {
        const json_t *yes = json_obj_get(args, "yes");
        if (pc2_truthy(yes)) {
            json_t *con = pc2_call(io, "Console", json_object());
            if (pc2_is_raise(con)) return con;
            json_t *f = pc2_fail_seam(io, con,
                "[red]Error:[/red] --yes only applies to `hermes plugins update` with no name.");
            json_free(con);
            if (pc2_is_raise(f)) return f;
            json_free(f);
        }
        const json_t *force = json_obj_get(args, "force");
        json_t *a = json_object();
        json_set(a, "name", json_copy(name));
        json_set(a, "force", json_bool(pc2_truthy(force) ? true : false));
        json_t *r = pc2_call(io, "cmd_update", a);
        if (pc2_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    const json_t *force = json_obj_get(args, "force");
    if (pc2_truthy(force)) {
        json_t *con = pc2_call(io, "Console", json_object());
        if (pc2_is_raise(con)) return con;
        json_t *f = pc2_fail_seam(io, con,
            "[red]Error:[/red] --force applies to one named plugin: `hermes plugins update <name> --force`.");
        json_free(con);
        if (pc2_is_raise(f)) return f;
        json_free(f);
    }
    const json_t *yes = json_obj_get(args, "yes");
    json_t *a = json_object();
    json_set(a, "yes", json_bool(pc2_truthy(yes) ? true : false));
    json_t *r = pc2_call(io, "cmd_update_all", a);
    if (pc2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: pc2_action_auto_update @ hermes_cli/plugins_cmd.py:_action_auto_update */
static json_t *pc2_action_auto_update(pc2_io_t *io, const json_t *args) {
    json_t *console = pc2_call(io, "Console", json_object());
    if (pc2_is_raise(console)) return console;
    const json_t *state = args ? json_obj_get(args, "state") : NULL;
    json_t *cur;
    if (!pc2_present(state)) {
        cur = pc2_call(io, "auto_update_state", json_object());
    } else {
        int on = (state->type == JSON_STRING && strcmp(state->str_val, "on") == 0);
        json_t *a = json_object();
        json_set(a, "on", json_bool(on ? true : false));
        cur = pc2_call(io, "set_auto_update", a);
    }
    if (pc2_is_raise(cur)) {
        if (pc2_is_plugin_operation_error(pc2_raise_kind(cur))) {
            char *m = pc2_fmt("[red]Error:[/red] %s", pc2_raise_msg(cur));
            json_t *f = pc2_fail_seam(io, console, m);
            free(m);
            if (pc2_is_raise(f)) { json_free(cur); json_free(console); return f; }
            json_free(f);
            json_free(cur); json_free(console);
            /* _fail returned without exiting: `current` was never bound. */
            return pc2_raise("UnboundLocalError",
                "cannot access local variable 'current' where it is not associated with a value");
        }
        json_free(console);
        return cur;
    }
    /* current["auto_apply"] */
    if (!cur || cur->type != JSON_OBJECT) {
        json_t *e = pc2_subscript_error(cur);
        json_free(cur); json_free(console); return e;
    }
    if (!json_has(cur, "auto_apply")) {
        json_free(cur); json_free(console); return pc2_raise("KeyError", "'auto_apply'");
    }
    const char *word = pc2_truthy(json_obj_get(cur, "auto_apply")) ? "[green]on[/green]" : "[dim]off[/dim]";
    if (!json_has(cur, "check_hours")) {
        json_free(cur); json_free(console); return pc2_raise("KeyError", "'check_hours'");
    }
    const json_t *ch = json_obj_get(cur, "check_hours");
    double chv;
    if (ch && ch->type == JSON_NUMBER) chv = ch->num_val;
    else if (ch && ch->type == JSON_BOOL) chv = ch->bool_val ? 1.0 : 0.0;
    else {
        json_t *e = pc2_raise_fmt("TypeError", "'>' not supported between instances of '%s' and 'int'",
                                  pc2_type_name_of(ch));
        json_free(cur); json_free(console); return e;
    }
    char *cadence;
    if (chv > 0) {
        char *g = pc2_format_g(chv);
        cadence = pc2_fmt("checked every %sh by the gateway", g);
        free(g);
    } else {
        cadence = strdup("the gateway's check is off: plugins.auto_update_check_hours is 0");
    }
    char *msg = pc2_fmt("Plugin auto-update: %s (%s; updates load in new sessions)", word, cadence);
    free(cadence);
    json_free(cur);
    json_t *p = pc2_console_print(io, console, msg);
    free(msg); json_free(console);
    if (pc2_is_raise(p)) return p;
    json_free(p);
    return NULL;
}

/* PoP: pc2_action_pack @ hermes_cli/plugins_cmd.py:_action_pack */
static json_t *pc2_action_pack(pc2_io_t *io, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "args", args ? json_copy(args) : json_object());
    json_t *r = pc2_call(io, "pack_command", a);
    if (pc2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: pc2_action_idle_tasks @ hermes_cli/plugins_cmd.py:_action_idle_tasks */
static json_t *pc2_action_idle_tasks(pc2_io_t *io, const json_t *args) {
    const json_t *name = args ? json_obj_get(args, "name") : NULL;
    if (!name)
        return pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'name'");
    const json_t *pause = json_obj_get(args, "pause");
    if (!pause)
        return pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'pause'");
    const json_t *resume = json_obj_get(args, "resume");
    if (!resume)
        return pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'resume'");
    json_t *a = json_object();
    json_set(a, "name", json_copy(name));
    json_set(a, "pause", json_copy(pause));
    json_set(a, "resume", json_copy(resume));
    json_t *r = pc2_call(io, "cmd_idle_tasks", a);
    if (pc2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── nested closures ── */

/* PoP: pc2_refuse_legacy_relay @ hermes_cli/plugins_cmd.py:_refuse_legacy_relay */
static json_t *pc2_refuse_legacy_relay(pc2_io_t *io, const json_t *console, const char *plugin,
                                       const json_t *legacy_keys, const char *env) {
    int member = 0;
    if (legacy_keys && legacy_keys->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(legacy_keys); i++)
            if (strcmp(pc2_jstr(json_get(legacy_keys, i)), plugin ? plugin : "") == 0) { member = 1; break; }
    if (!member) return NULL;
    char *m = pc2_fmt("[red]Plugin '%s' was removed.[/red] Relay lifecycle is owned "
                      "by Hermes core; configure a standard user or system Relay plugins.toml, or use "
                      "%s for an explicit user-file override.", plugin ? plugin : "", env ? env : "");
    json_t *f = pc2_fail_seam(io, console, m);
    free(m);
    if (pc2_is_raise(f)) return f;
    json_free(f);
    return NULL;
}

/* PoP: pc2_first_toolset @ hermes_cli/plugins_cmd.py:_first_toolset */
static json_t *pc2_first_toolset(pc2_io_t *io, const json_t *tool_names) {
    if (tool_names && tool_names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tool_names); i++) {
            const json_t *t = json_get(tool_names, i);
            json_t *a = json_object();
            json_set(a, "name", json_string(pc2_jstr(t)));
            json_t *e = pc2_call(io, "registry_get_entry", a);
            if (pc2_is_raise(e)) return e;
            if (!pc2_truthy(e)) { json_free(e); continue; }
            if (e->type != JSON_OBJECT) {
                json_t *err = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'toolset'",
                                            pc2_type_name_of(e));
                json_free(e); return err;
            }
            const json_t *ts = json_obj_get(e, "toolset");
            if (!ts) {
                json_free(e);
                return pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'toolset'");
            }
            if (pc2_truthy(ts)) { json_t *out = json_copy(ts); json_free(e); return out; }
            json_free(e);
        }
    }
    return NULL;
}

/* PoP: pc2_from_loaded_plugin @ hermes_cli/plugins_cmd.py:_from_loaded_plugin */
static json_t *pc2_from_loaded_plugin(pc2_io_t *io, const char *name) {
    json_t *dp = pc2_call(io, "discover_plugins", json_object());
    if (pc2_is_raise(dp)) return dp;
    json_free(dp);
    json_t *mgr = pc2_call(io, "get_plugin_manager", json_object());
    if (pc2_is_raise(mgr)) return mgr;
    if (!mgr || mgr->type == JSON_NULL) {
        json_free(mgr); return pc2_raise("AttributeError", "'NoneType' object has no attribute '_plugins'");
    }
    if (mgr->type != JSON_OBJECT) {
        json_t *e = pc2_raise_fmt("AttributeError", "'%s' object has no attribute '_plugins'", pc2_type_name_of(mgr));
        json_free(mgr); return e;
    }
    const json_t *pl = json_obj_get(mgr, "_plugins");
    if (!pl) {
        json_free(mgr);
        return pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute '_plugins'");
    }
    if (pl->type != JSON_OBJECT) {
        json_t *e = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'items'", pc2_type_name_of(pl));
        json_free(mgr); return e;
    }
    json_t *ret = NULL; int done = 0;
    for (size_t i = 0; i < pl->c.count && !done; i++) {
        const char *key = pl->c.keys[i];
        const json_t *loaded = pl->c.items[i];
        const json_t *manifest;
        if (!loaded || loaded->type == JSON_NULL) {
            ret = pc2_raise("AttributeError", "'NoneType' object has no attribute 'manifest'"); done = 1; break;
        }
        if (loaded->type != JSON_OBJECT) {
            ret = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'manifest'", pc2_type_name_of(loaded));
            done = 1; break;
        }
        manifest = json_obj_get(loaded, "manifest");
        if (!manifest) {
            ret = pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'manifest'");
            done = 1; break;
        }
        const json_t *nm;
        if (manifest->type == JSON_NULL) {
            ret = pc2_raise("AttributeError", "'NoneType' object has no attribute 'name'"); done = 1; break;
        }
        if (manifest->type != JSON_OBJECT) {
            ret = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'name'", pc2_type_name_of(manifest));
            done = 1; break;
        }
        nm = json_obj_get(manifest, "name");
        if (!nm) {
            ret = pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'name'");
            done = 1; break;
        }
        int match = (nm->type == JSON_STRING && strcmp(nm->str_val, name ? name : "") == 0)
                    || strcmp(key, name ? name : "") == 0;
        if (match) {
            const json_t *tr = json_obj_get(loaded, "tools_registered");
            if (!tr) {
                ret = pc2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute 'tools_registered'");
                done = 1; break;
            }
            ret = pc2_first_toolset(io, tr);   /* returns even when NULL (no toolset) */
            done = 1; break;
        }
    }
    json_free(mgr);
    return ret;
}

/* PoP: pc2_from_manifest_on_disk @ hermes_cli/plugins_cmd.py:_from_manifest_on_disk */
static json_t *pc2_from_manifest_on_disk(pc2_io_t *io, const char *name) {
    /* the (get_bundled_plugins_dir(), _plugins_dir()) tuple is EAGER: both fire. */
    json_t *b1 = pc2_call(io, "get_bundled_plugins_dir", json_object());
    if (pc2_is_raise(b1)) return b1;
    json_t *b2 = pc2_call(io, "_plugins_dir", json_object());
    if (pc2_is_raise(b2)) { json_free(b1); return b2; }
    const json_t *bases[2] = {b1, b2};
    json_t *ret = NULL; int done = 0;
    for (int bi = 0; bi < 2 && !done; bi++) {
        const json_t *base = bases[bi];
        if (!pc2_present(base)) {
            ret = pc2_raise("AttributeError", "'NoneType' object has no attribute 'is_dir'"); done = 1; break;
        }
        if (base->type != JSON_STRING) {
            ret = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'is_dir'", pc2_type_name_of(base));
            done = 1; break;
        }
        const char *bs = base->str_val;
        json_t *isd = pc2_call(io, "is_dir", pc2_path_args(bs));
        if (pc2_is_raise(isd)) { ret = isd; done = 1; break; }
        int d1 = pc2_truthy(isd); json_free(isd);
        if (!d1) continue;
        char *joined = pc2_posix_join(bs, name ? name : "");
        json_t *isd2 = pc2_call(io, "is_dir", pc2_path_args(joined));
        if (pc2_is_raise(isd2)) { free(joined); ret = isd2; done = 1; break; }
        int d2 = pc2_truthy(isd2); json_free(isd2);
        if (!d2) { free(joined); continue; }
        json_t *ma = json_object();
        json_set(ma, "plugin_dir", json_string(joined));
        json_t *rm = pc2_call(io, "read_manifest", ma);
        if (pc2_is_raise(rm)) { free(joined); ret = rm; done = 1; break; }
        if (!rm || rm->type != JSON_OBJECT) {
            ret = pc2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", pc2_type_name_of(rm));
            json_free(rm); free(joined); done = 1; break;
        }
        const json_t *pt = json_obj_get(rm, "provides_tools");
        const json_t *list = pc2_truthy(pt) ? pt : NULL;
        json_t *ts = pc2_first_toolset(io, list);
        json_free(rm); free(joined);
        if (pc2_is_raise(ts)) { ret = ts; done = 1; break; }
        if (pc2_truthy(ts)) { ret = ts; done = 1; break; }
        json_free(ts);
    }
    json_free(b1); json_free(b2);
    return ret;
}
