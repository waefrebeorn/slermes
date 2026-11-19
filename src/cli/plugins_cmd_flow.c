/*
 * plugins_cmd_flow.c — FLOW cores + leaves of hermes_cli/plugins_cmd.py (LIVE
 * authority) that NO existing PoP claims: the console/config helpers (_console,
 * _table, _is_tty, _fail, _ask_yes, _config_value, _clone_timeout_seconds,
 * _config_name_set, _config_str, _write_config_value), the manifest readers
 * (_native_manifest_file, _has_portable_manifest, _load_yaml_manifest,
 * _looks_like_plugin_dir, _read_manifest_info, _missing_env_specs), the message
 * builders (_clone_failure_message, _unknown_plugin_message, _preserved_files_note,
 * _scan_merged_tree), the selection algebra (_discard_key_and_leaf, _plugin_aliases,
 * _disabled_by_update, _activate_key, _apply_activation, _forget_plugin_config,
 * _plugin_selection_version, _refuse_recalled_entry, _tri_state_flag,
 * _category_active_names, _saved_list_toolset_key) and the discovery scan
 * (_scan_level, _discover_all_plugins, _default_on).
 *
 * Self-contained C11, libjson-only. Claimed siblings (_scan_plugin_tree,
 * _plugins_dir, _get_enabled_set / _get_disabled_set / _get_current_memory_provider
 * functools.partial accessors, _get_plugin_toolset_key, left_core_migration
 * moved_plugin_hint / legacy_plugin_keys) and every foreign collaborator
 * (rich Console/Table/markup.escape, hermes_cli.config load_config/save_config/
 * cfg_get/get_env_value, pm.plugin_declarations native_manifest_file/
 * read_native_manifest, pm.plugins_state read_home_selection, runtime_state
 * _digest, plugins_cmd_catalog refuse_if_installed_removed, providers
 * _declares_model_provider_kind, hermes_cli.plugins get_bundled_plugins_dir/
 * discover_entrypoint_manifests, agent_plugins read_agent_plugin_manifest,
 * toolset_validation parse_platform_toolsets_value, the Path stat/IO methods,
 * sys.stdin/stdout.isatty, the _ask_yes reader, logger.warning) enter through ONE
 * universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object
 * (the seam copies what it keeps; plc_call frees args); the result is a NEW
 * json_t* the caller frees (NULL == None; a scripted JSON null is delivered as a
 * NULL pointer; {"__raise__": kind, "msg": ...} == a raised exception). The
 * harness records {"ev": name, "args": args, "ret": result} per call in order;
 * the oracle's recording stubs emit the identical shape driving LIVE Python with
 * the same scripts. The ports perform NO real I/O.
 *
 * The unported unclaimed sibling _set_plugin_enabled (dragging in _selection_lock
 * threading + the _admit_and_save_plugin_sets admission transaction) is SEAMED
 * like a claimed sibling; _activate_key composes it through the seam.
 *
 * Ported-to-ported calls compose REAL (plc_config_value inside plc_clone_timeout_seconds
 * / plc_config_name_set / plc_config_str; plc_native_manifest_file / plc_has_portable_manifest
 * / plc_load_yaml_manifest inside plc_looks_like_plugin_dir / plc_read_manifest_info /
 * plc_default_on; plc_preserved_files_note inside plc_scan_merged_tree;
 * plc_discard_key_and_leaf inside plc_apply_activation; plc_plugin_aliases /
 * plc_disabled_by_update inside plc_activate_key; plc_discover_all_plugins inside
 * plc_plugin_aliases; plc_scan_level inside plc_discover_all_plugins and itself);
 * the oracle keeps the extracted defs real on the same paths, so their internal
 * seam events appear in BOTH traces.
 *
 * Exception propagation: every plc flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except Exception` sites swallow every
 * non-BaseException kind; `except ValueError` (_native_manifest_file) converts the
 * ValueError family to PluginOperationError; `except (EOFError, KeyboardInterrupt)`
 * (_ask_yes) swallows those two families (KeyboardInterrupt is a BaseException but
 * is explicitly caught); `except (OSError, PluginOperationError)` (_scan_level)
 * swallows those two families; `except PluginScanBlocked` (_scan_merged_tree)
 * re-raises with a preserved-files note. The BaseException trio (SystemExit /
 * KeyboardInterrupt / GeneratorExit) ALWAYS passes through any `except Exception`.
 * Fail-closed: a missing/empty seam script yields a RuntimeError raise spec
 * (recorded, then propagated/swallowed exactly as live); no seam installed at all
 * yields the same spec.
 *
 * Path model: paths are verbatim POSIX strings. The oracle seeds PurePosixPath
 * (str keeps separators on this Windows host); the C uses a lexical PurePosixPath
 * component model. Path-valued seam results materialize as a PPath double whose
 * stat/IO methods fire the is_dir / exists / is_symlink / iterdir seams; the C
 * reads the path string and fires the same seam. iterdir children sort by string
 * (ASCII domain).
 *
 * Fixture domain (ASCII): inputs are ASCII; integral floats normalize to ints on
 * both sides (the oracle deep-normalizes input AND seam scripts) so isinstance(int)
 * gates agree with the C number-is-int model; set-valued inputs/results are lists
 * of strings (sorted-unique on output); entries are 6-element arrays
 * [name, version, description, source, dir_path, key]; findings are {file:str};
 * console markers are non-empty dicts or null; kwargs are plain dicts.
 *
 * Seam name table (name -> live call):
 *   Console                       rich.console.Console() ctor {} -> marker
 *   Table                         rich.table.Table(**kwargs) ctor {kwargs} -> marker
 *   add_column                    table.add_column(header, style=style) {header,style}
 *   console_print                 console.print(*values) {args:[...]}
 *   stdin_isatty / stdout_isatty  sys.stdin/stdout.isatty() {}
 *   reader                        _ask_yes reader(prompt) {prompt}
 *   load_config                   hermes_cli.config.load_config() {}
 *   save_config                   hermes_cli.config.save_config(config) {config}
 *   cfg_get                       cfg_get(config, *keys, default=default) {config,keys,default}
 *   get_env_value                 hermes_cli.config.get_env_value(name) {name}
 *   logger_warning                logger.warning(fmt, *args) {fmt,args:[...]}
 *   native_manifest_file          pm.plugin_declarations.native_manifest_file(dir) {plugin_dir}
 *   read_native_manifest          pm.plugin_declarations.read_native_manifest(file) {manifest_file}
 *   read_agent_plugin_manifest    agent_plugins.read_agent_plugin_manifest(dir) {plugin_dir}
 *   exists / is_dir / is_symlink  Path.<method>() {path}
 *   iterdir                       Path.iterdir() {path} -> [child path strings]
 *   markup_escape                 rich.markup.escape(text) {text}
 *   moved_plugin_hint             left_core_migration.moved_plugin_hint(name) {name} [claimed]
 *   legacy_plugin_keys            left_core_migration.legacy_plugin_keys(key) {key} [claimed]
 *   get_enabled_set/get_disabled_set  functools.partial accessors {} [claimed]
 *   get_current_memory_provider   functools.partial accessor {} [claimed]
 *   get_plugin_toolset_key        _get_plugin_toolset_key(name) {name} [claimed]
 *   _plugins_dir                  _plugins_dir() {} [claimed]
 *   _scan_plugin_tree             _scan_plugin_tree(dir, id, **kwargs) {plugin_dir,identifier,...} [claimed]
 *   set_plugin_enabled            _set_plugin_enabled(name, enable=, aliases=, console=) [unported seam]
 *   read_home_selection           pm.plugins_state.read_home_selection(home) {home}
 *   get_hermes_home               hermes_constants.get_hermes_home() {}
 *   _digest                       hermes_cli.runtime_state._digest(path) {path}
 *   refuse_if_installed_removed   plugins_cmd_catalog.refuse_if_installed_removed(name, dir) {name,plugin_dir}
 *   parse_platform_toolsets_value toolset_validation.parse_platform_toolsets_value(v) {value}
 *   _declares_model_provider_kind providers._declares_model_provider_kind(path) {path}
 *   get_bundled_plugins_dir       hermes_cli.plugins.get_bundled_plugins_dir() {}
 *   discover_entrypoint_manifests hermes_cli.plugins.discover_entrypoint_manifests() {}
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

typedef json_t *(*plc_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct plc_io {
    plc_seam_fn call;
    void *ctx;
    const json_t *env;   /* borrowed ambient values (unused; kept for shape parity) */
} plc_io_t;

static int plc_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *plc_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *plc_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* ── FULL builtin exception hierarchy (parent table) + the module classes ──
 * plc_exc_parent walks to Exception / BaseException. Unknown kinds default to
 * Exception (a synthetic Exception subclass), so `except Exception` catches them;
 * the BaseException trio walks to BaseException only and passes through. The
 * plugins_cmd module classes (PluginOperationError / PluginSourceMoved /
 * PluginScanBlocked) are appended so their family catches transcribe exactly. */
static const struct { const char *kind; const char *parent; } PLC_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"Exception", "BaseException"},
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
};
#define PLC_N_EXC ((int)(sizeof PLC_EXC_PARENTS / sizeof PLC_EXC_PARENTS[0]))

static const char *plc_exc_parent(const char *k) {
    for (int i = 0; i < PLC_N_EXC; i++)
        if (strcmp(PLC_EXC_PARENTS[i].kind, k) == 0) return PLC_EXC_PARENTS[i].parent;
    return "Exception";   /* unknown kind: a synthetic Exception subclass */
}

static int plc_kind_is_a(const char *k, const char *ancestor) {
    char cur[128];
    snprintf(cur, sizeof cur, "%s", k);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, ancestor) == 0) return 1;
        if (strcmp(cur, "BaseException") == 0) return 0;
        const char *p = plc_exc_parent(cur);
        if (!p || strcmp(p, cur) == 0) return 0;
        snprintf(cur, sizeof cur, "%s", p);
    }
    return 0;
}

static int plc_caught_by_exception(const char *k) { return plc_kind_is_a(k, "Exception"); }
static int plc_is_oserror(const char *k) { return plc_kind_is_a(k, "OSError"); }
static int plc_is_valueerror(const char *k) { return plc_kind_is_a(k, "ValueError"); }
static int plc_is_eoferror(const char *k) { return plc_kind_is_a(k, "EOFError"); }
static int plc_is_keyboardinterrupt(const char *k) { return plc_kind_is_a(k, "KeyboardInterrupt"); }
static int plc_is_plugin_operation_error(const char *k) { return plc_kind_is_a(k, "PluginOperationError"); }
static int plc_is_plugin_scan_blocked(const char *k) { return plc_kind_is_a(k, "PluginScanBlocked"); }

static json_t *plc_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *plc_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return plc_raise(kind, buf);
}

static char *plc_fmt(const char *fmt, ...) {
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
static json_t *plc_call(plc_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = plc_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int plc_truthy(const json_t *v) {
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

static int plc_present(const json_t *v) { return v && v->type != JSON_NULL; }

static const char *plc_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* isinstance(v, int) model: a JSON number with an integral value (bool excluded). */
static int plc_is_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && v->num_val == floor(v->num_val);
}

/* Python type name for AttributeError messages (JSON model). */
static const char *plc_type_name_of(const json_t *v) {
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

/* ── UTF-8 codepoint helpers + Unicode White_Space strip ── */

static unsigned long plc_utf8_next(const char *s, size_t n, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) { *i += 1; return c; }
    int extra; unsigned long cp;
    if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
    else { *i += 1; return 0xFFFD; }
    if (*i + (size_t)extra >= n) { *i += 1; return 0xFFFD; }
    for (int k = 1; k <= extra; k++) {
        unsigned char cc = (unsigned char)s[*i + (size_t)k];
        if ((cc & 0xC0) != 0x80) { *i += 1; return 0xFFFD; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    *i += (size_t)extra + 1;
    return cp;
}

static int plc_is_ws_cp(unsigned long cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || cp == 0x85 || cp == 0xA0) return 1;
    if (cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    return 0;
}

static char *plc_strip_ws(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t start = 0, end = n;
    size_t i = 0;
    while (i < n) {
        size_t save = i;
        unsigned long cp = plc_utf8_next(s, n, &i);
        if (!plc_is_ws_cp(cp)) { start = save; break; }
        start = i;
    }
    size_t last = start;
    i = start;
    while (i < n) {
        unsigned long cp = plc_utf8_next(s, n, &i);
        if (!plc_is_ws_cp(cp)) last = i;
    }
    end = last;
    if (end < start) end = start;
    char *out = (char *)malloc(end - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
    return out;
}

static char *plc_strip_chars(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < n && strchr(chars, s[a])) a++;
    while (b > a && strchr(chars, s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *plc_lower_ascii(const char *s) {
    if (!s) return strdup("");
    char *out = strdup(s);
    if (!out) return NULL;
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return out;
}

static char *plc_float_repr(double v) {
    char sci[64], buf[64];
    int p;
    for (p = 1; p <= 17; p++) {
        snprintf(sci, sizeof sci, "%.*e", p - 1, v);
        if (strtod(sci, NULL) == v) break;
    }
    const char *epos = strchr(sci, 'e');
    int exp = epos ? atoi(epos + 1) : 0;
    if (exp < -4 || exp >= 16) return strdup(sci);
    int decimals = p - 1 - exp;
    if (decimals < 0) decimals = 0;
    snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return strdup(buf);
}

/* str() of a scalar JSON value (containers out of the str() fixture domain). */
static char *plc_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) {
            snprintf(buf, sizeof buf, "%.0f", d);
            return strdup(buf);
        }
        return plc_float_repr(d);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* str(x or "") — falsy collapses to the EMPTY STRING, not str(None). */
static char *plc_py_str_or_empty(const json_t *v) {
    if (!plc_truthy(v)) return strdup("");
    return plc_py_str(v);
}

/* ── lexical PurePosixPath component model ── */

typedef struct {
    char *root;     /* "", "/" or "//" */
    char **parts;   /* owned components (no empty / '.'; '..' kept) */
    int n;
    int cap;
} plc_path_t;

static void plc_path_init(plc_path_t *p) { p->root = strdup(""); p->parts = NULL; p->n = 0; p->cap = 0; }

static void plc_path_free(plc_path_t *p) {
    free(p->root);
    for (int i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->root = NULL; p->parts = NULL; p->n = 0; p->cap = 0;
}

static void plc_path_push(plc_path_t *p, const char *s, size_t len) {
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 8;
        p->parts = (char **)realloc(p->parts, (size_t)p->cap * sizeof(char *));
    }
    char *c = (char *)malloc(len + 1);
    memcpy(c, s, len); c[len] = '\0';
    p->parts[p->n++] = c;
}

static void plc_path_parse(plc_path_t *p, const char *s) {
    plc_path_init(p);
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
        else if (len > 0) plc_path_push(p, s + i, len);
        i = j;
        while (i < n && s[i] == '/') i++;
    }
}

static char *plc_path_str(const plc_path_t *p) {
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

static void plc_path_copy(plc_path_t *dst, const plc_path_t *src) {
    plc_path_init(dst);
    free(dst->root);
    dst->root = strdup(src->root);
    for (int i = 0; i < src->n; i++) plc_path_push(dst, src->parts[i], strlen(src->parts[i]));
}

static void plc_path_parent(plc_path_t *dst, const plc_path_t *src) {
    plc_path_copy(dst, src);
    if (dst->n > 0) { free(dst->parts[dst->n - 1]); dst->n--; }
}

static char *plc_path_name(const plc_path_t *p) {
    if (p->n == 0) return strdup("");
    return strdup(p->parts[p->n - 1]);
}

static char *plc_posix_join(const char *a, const char *b) {
    plc_path_t pa, pb, out;
    plc_path_parse(&pa, a);
    plc_path_parse(&pb, b);
    if (pb.root[0] != '\0') {
        plc_path_copy(&out, &pb);
    } else {
        plc_path_copy(&out, &pa);
        for (int i = 0; i < pb.n; i++) plc_path_push(&out, pb.parts[i], strlen(pb.parts[i]));
    }
    char *s = plc_path_str(&out);
    plc_path_free(&pa); plc_path_free(&pb); plc_path_free(&out);
    return s;
}

static json_t *plc_path_args(const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    return a;
}

/* ── string-set helpers (sets of ASCII strings; sorted-unique output) ── */

typedef struct {
    char **items;
    int n;
    int cap;
} plc_strset_t;

static void plc_ss_init(plc_strset_t *s) { s->items = NULL; s->n = 0; s->cap = 0; }

static void plc_ss_free(plc_strset_t *s) {
    for (int i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL; s->n = 0; s->cap = 0;
}

static int plc_ss_contains(const plc_strset_t *s, const char *x) {
    for (int i = 0; i < s->n; i++) if (strcmp(s->items[i], x) == 0) return 1;
    return 0;
}

static void plc_ss_add(plc_strset_t *s, const char *x) {
    if (plc_ss_contains(s, x)) return;
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->items = (char **)realloc(s->items, (size_t)s->cap * sizeof(char *));
    }
    s->items[s->n++] = strdup(x);
}

static void plc_ss_discard(plc_strset_t *s, const char *x) {
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->items[i], x) == 0) {
            free(s->items[i]);
            for (int j = i; j < s->n - 1; j++) s->items[j] = s->items[j + 1];
            s->n--;
            return;
        }
    }
}

static int plc_ss_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void plc_ss_sort(plc_strset_t *s) {
    if (s->n > 1) qsort(s->items, (size_t)s->n, sizeof(char *), plc_ss_cmp);
}

/* Load a JSON array of strings into a set (non-string items skipped). */
static void plc_ss_from_array(plc_strset_t *s, const json_t *arr) {
    plc_ss_init(s);
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arr); i++) {
            const json_t *v = json_get(arr, i);
            if (v && v->type == JSON_STRING) plc_ss_add(s, plc_jstr(v));
        }
    }
}

static json_t *plc_ss_to_array(plc_strset_t *s) {
    plc_ss_sort(s);
    json_t *a = json_array();
    for (int i = 0; i < s->n; i++) json_append(a, json_string(s->items[i]));
    return a;
}

static int plc_ss_intersects(const plc_strset_t *a, const plc_strset_t *b) {
    for (int i = 0; i < a->n; i++) if (plc_ss_contains(b, a->items[i])) return 1;
    return 0;
}

/* key.split("/")[-1] — the bare leaf. malloc'd. */
static char *plc_leaf(const char *key) {
    if (!key) return strdup("");
    const char *slash = strrchr(key, '/');
    return strdup(slash ? slash + 1 : key);
}

/* ── module constants ── */
#define PLC_DEFAULT_CLONE_TIMEOUT 300
#define PLC_MAX_CLONE_TIMEOUT 3600
static const char *const PLC_BUNDLED_DEFAULT_ON_KINDS[] = {"backend", "platform", "model-provider"};
static const char *const PLC_BUNDLED_SKIP[] = {"memory", "context_engine", "computer_use", "model-providers", "config_backends"};
#define PLC_N_BUNDLED_SKIP 5
static const char *PLC_RECORD = "disabled_by_update";   /* pm.plugins_state.RECORD */

/* ── console / table markers ── */

/* console.print(message) seam: {args:[message]}. */
static json_t *plc_console_print(plc_io_t *io, const json_t *console, const char *message) {
    if (!plc_present(console))
        return plc_raise("AttributeError", "'NoneType' object has no attribute 'print'");
    json_t *a = json_object();
    json_t *vals = json_array();
    json_append(vals, json_string(message ? message : ""));
    json_set(a, "args", vals);
    return plc_call(io, "console_print", a);
}

/* ── pure / config leaves ── */

/* PoP: plc_console @ hermes_cli/plugins_cmd.py:_console */
static json_t *plc_console(plc_io_t *io) {
    return plc_call(io, "Console", json_object());
}

/* PoP: plc_table @ hermes_cli/plugins_cmd.py:_table */
static json_t *plc_table(plc_io_t *io, const json_t *columns, const json_t *kwargs) {
    json_t *ta = kwargs ? json_copy(kwargs) : json_object();
    if (ta->type != JSON_OBJECT) { json_free(ta); ta = json_object(); }
    json_t *table = plc_call(io, "Table", ta);
    if (plc_is_raise(table)) return table;
    if (columns && columns->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(columns); i++) {
            const json_t *col = json_get(columns, i);
            const char *header = "";
            const json_t *style = NULL;
            if (col && col->type == JSON_ARRAY && json_len(col) >= 1) {
                header = plc_jstr(json_get(col, 0));
                if (json_len(col) >= 2) style = json_get(col, 1);
            }
            json_t *a = json_object();
            json_set(a, "header", json_string(header));
            if (style) json_set(a, "style", json_copy(style));
            json_t *r = plc_call(io, "add_column", a);
            if (plc_is_raise(r)) { json_free(table); return r; }
            json_free(r);
        }
    }
    return table;
}

/* PoP: plc_is_tty @ hermes_cli/plugins_cmd.py:_is_tty */
static json_t *plc_is_tty(plc_io_t *io) {
    json_t *si = plc_call(io, "stdin_isatty", json_object());
    if (plc_is_raise(si)) return si;
    int stdin_tty = plc_truthy(si);
    json_free(si);
    if (!stdin_tty) return json_bool(false);   /* short-circuit: stdout seam not fired */
    json_t *so = plc_call(io, "stdout_isatty", json_object());
    if (plc_is_raise(so)) return so;
    int stdout_tty = plc_truthy(so);
    json_free(so);
    return json_bool(stdout_tty ? true : false);
}

/* PoP: plc_fail @ hermes_cli/plugins_cmd.py:_fail */
static json_t *plc_fail(plc_io_t *io, const json_t *console, const char *message) {
    json_t *p = plc_console_print(io, console, message);
    if (plc_is_raise(p)) return p;
    json_free(p);
    return plc_raise("SystemExit", "1");   /* sys.exit(1) */
}

/* PoP: plc_ask_yes @ hermes_cli/plugins_cmd.py:_ask_yes */
static json_t *plc_ask_yes(plc_io_t *io, const char *prompt) {
    json_t *a = json_object();
    json_set(a, "prompt", json_string(prompt ? prompt : ""));
    json_t *r = plc_call(io, "reader", a);
    if (plc_is_raise(r)) {
        const char *k = plc_raise_kind(r);
        if (plc_is_eoferror(k) || plc_is_keyboardinterrupt(k)) { json_free(r); return json_bool(false); }
        return r;
    }
    if (!r || r->type != JSON_STRING) {
        /* answer.strip() on a non-str raises AttributeError */
        json_t *e = plc_raise_fmt("AttributeError", "'%s' object has no attribute 'strip'",
                                  plc_type_name_of(r));
        json_free(r);
        return e;
    }
    char *stripped = plc_strip_ws(r->str_val);
    char *low = plc_lower_ascii(stripped ? stripped : "");
    free(stripped);
    int yes = (strcmp(low, "y") == 0 || strcmp(low, "yes") == 0);
    free(low);
    json_free(r);
    return json_bool(yes ? true : false);
}

/* PoP: plc_config_value @ hermes_cli/plugins_cmd.py:_config_value */
static json_t *plc_config_value(plc_io_t *io, const json_t *keys, const json_t *default_v) {
    /* try: load_config(); cfg_get(...)  except Exception: return default */
    json_t *cfg = plc_call(io, "load_config", json_object());
    if (plc_is_raise(cfg)) {
        if (plc_caught_by_exception(plc_raise_kind(cfg))) { json_free(cfg); return default_v ? json_copy(default_v) : NULL; }
        return cfg;
    }
    json_t *a = json_object();
    json_set(a, "config", cfg ? json_copy(cfg) : json_null());
    json_set(a, "keys", keys ? json_copy(keys) : json_array());
    json_set(a, "default", default_v ? json_copy(default_v) : json_null());
    json_t *r = plc_call(io, "cfg_get", a);
    json_free(cfg);
    if (plc_is_raise(r)) {
        if (plc_caught_by_exception(plc_raise_kind(r))) { json_free(r); return default_v ? json_copy(default_v) : NULL; }
        return r;
    }
    return r;
}

/* logger.warning(fmt, *args) seam: {fmt, args:[...]}. */
static json_t *plc_log_warning(plc_io_t *io, const char *fmt, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args_arr);
    return plc_call(io, "logger_warning", a);
}

/* PoP: plc_clone_timeout_seconds @ hermes_cli/plugins_cmd.py:_clone_timeout_seconds */
static json_t *plc_clone_timeout_seconds(plc_io_t *io) {
    json_t *keys = json_array();
    json_append(keys, json_string("plugins"));
    json_append(keys, json_string("clone_timeout_seconds"));
    json_t *def = json_number(PLC_DEFAULT_CLONE_TIMEOUT);
    json_t *value = plc_config_value(io, keys, def);
    json_free(keys); json_free(def);
    if (plc_is_raise(value)) return value;
    /* if isinstance(value, bool) or not isinstance(value, int) or value < 1 */
    int bad = (value && value->type == JSON_BOOL) || !plc_is_int(value) ||
              (plc_is_int(value) && value->num_val < 1);
    if (bad) {
        json_t *la = json_array();
        json_append(la, json_number(PLC_DEFAULT_CLONE_TIMEOUT));
        json_t *w = plc_log_warning(io, "plugins.clone_timeout_seconds must be a positive integer; using %ss", la);
        if (plc_is_raise(w)) { json_free(value); return w; }
        json_free(w);
        json_free(value);
        return json_number(PLC_DEFAULT_CLONE_TIMEOUT);
    }
    if (value->num_val > PLC_MAX_CLONE_TIMEOUT) {
        json_t *la = json_array();
        json_append(la, json_number(PLC_MAX_CLONE_TIMEOUT));
        json_t *w = plc_log_warning(io, "plugins.clone_timeout_seconds exceeds %ss; clamping", la);
        if (plc_is_raise(w)) { json_free(value); return w; }
        json_free(w);
        json_free(value);
        return json_number(PLC_MAX_CLONE_TIMEOUT);
    }
    return value;
}

/* PoP: plc_config_name_set @ hermes_cli/plugins_cmd.py:_config_name_set */
static json_t *plc_config_name_set(plc_io_t *io, const json_t *keys) {
    json_t *def = json_array();
    json_t *value = plc_config_value(io, keys, def);
    json_free(def);
    if (plc_is_raise(value)) return value;
    plc_strset_t s;
    plc_ss_init(&s);
    if (value && value->type == JSON_ARRAY) plc_ss_from_array(&s, value);
    json_free(value);
    json_t *out = plc_ss_to_array(&s);
    plc_ss_free(&s);
    return out;
}

/* PoP: plc_config_str @ hermes_cli/plugins_cmd.py:_config_str */
static json_t *plc_config_str(plc_io_t *io, const json_t *keys, const json_t *default_v) {
    json_t *value = plc_config_value(io, keys, default_v);
    if (plc_is_raise(value)) return value;
    if (plc_truthy(value)) return value;
    json_free(value);
    return default_v ? json_copy(default_v) : NULL;
}

/* PoP: plc_write_config_value @ hermes_cli/plugins_cmd.py:_write_config_value */
static json_t *plc_write_config_value(plc_io_t *io, const char *section, const char *key, const json_t *value) {
    json_t *config = plc_call(io, "load_config", json_object());
    if (plc_is_raise(config)) return config;
    if (!config || config->type != JSON_OBJECT) {
        json_t *e = plc_raise_fmt("AttributeError", "'%s' object has no attribute 'setdefault'",
                                  config ? plc_type_name_of(config) : "NoneType");
        json_free(config);
        return e;
    }
    /* config.setdefault(section, {})[key] = value */
    json_t *sec = json_obj_get(config, section);
    if (!sec) {
        sec = json_object();
        json_set(config, section, sec);
    }
    if (sec->type != JSON_OBJECT) {
        json_t *e = plc_raise_fmt("TypeError", "'%s' object does not support item assignment",
                                  plc_type_name_of(sec));
        json_free(config);
        return e;
    }
    json_set(sec, key, value ? json_copy(value) : json_null());
    json_t *a = json_object();
    json_set(a, "config", json_copy(config));
    json_free(config);
    json_t *r = plc_call(io, "save_config", a);
    if (plc_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── manifest cluster ── */

/* PoP: plc_native_manifest_file @ hermes_cli/plugins_cmd.py:_native_manifest_file */
static json_t *plc_native_manifest_file(plc_io_t *io, const char *plugin_dir) {
    json_t *a = json_object();
    json_set(a, "plugin_dir", json_string(plugin_dir ? plugin_dir : ""));
    json_t *r = plc_call(io, "native_manifest_file", a);
    if (plc_is_raise(r)) {
        if (plc_is_valueerror(plc_raise_kind(r))) {
            /* raise PluginOperationError(str(exc), failure_class="manifest_invalid") from exc */
            json_t *e = plc_raise("PluginOperationError", plc_raise_msg(r));
            json_free(r);
            return e;
        }
        return r;
    }
    return r;   /* path string or NULL */
}

/* PoP: plc_has_portable_manifest @ hermes_cli/plugins_cmd.py:_has_portable_manifest */
static json_t *plc_has_portable_manifest(plc_io_t *io, const char *plugin_dir) {
    char *pf = plc_posix_join(plugin_dir ? plugin_dir : "", "plugin.json");
    json_t *e = plc_call(io, "exists", plc_path_args(pf));
    if (plc_is_raise(e)) { free(pf); return e; }
    int exists = plc_truthy(e);
    json_free(e);
    if (exists) { free(pf); return json_bool(true); }   /* short-circuit or */
    json_t *s = plc_call(io, "is_symlink", plc_path_args(pf));
    free(pf);
    if (plc_is_raise(s)) return s;
    int sym = plc_truthy(s);
    json_free(s);
    return json_bool(sym ? true : false);
}

/* PoP: plc_load_yaml_manifest @ hermes_cli/plugins_cmd.py:_load_yaml_manifest */
static json_t *plc_load_yaml_manifest(plc_io_t *io, const char *manifest_file) {
    json_t *a = json_object();
    json_set(a, "manifest_file", json_string(manifest_file ? manifest_file : ""));
    return plc_call(io, "read_native_manifest", a);
}

/* PoP: plc_looks_like_plugin_dir @ hermes_cli/plugins_cmd.py:_looks_like_plugin_dir */
static json_t *plc_looks_like_plugin_dir(plc_io_t *io, const char *target) {
    json_t *nm = plc_native_manifest_file(io, target);
    if (plc_is_raise(nm)) return nm;
    if (plc_present(nm)) { json_free(nm); return json_bool(true); }
    json_free(nm);
    char *pj = plc_posix_join(target ? target : "", "plugin.json");
    json_t *e = plc_call(io, "exists", plc_path_args(pj));
    free(pj);
    if (plc_is_raise(e)) return e;
    if (plc_truthy(e)) { json_free(e); return json_bool(true); }
    json_free(e);
    char *init = plc_posix_join(target ? target : "", "__init__.py");
    json_t *i = plc_call(io, "exists", plc_path_args(init));
    free(init);
    if (plc_is_raise(i)) return i;
    int has = plc_truthy(i);
    json_free(i);
    return json_bool(has ? true : false);
}

/* PoP: plc_read_manifest_info @ hermes_cli/plugins_cmd.py:_read_manifest_info */
static json_t *plc_read_manifest_info(plc_io_t *io, const char *d, const char *prefix) {
    json_t *mf = plc_native_manifest_file(io, d);
    if (plc_is_raise(mf)) return mf;
    plc_path_t dp; plc_path_parse(&dp, d ? d : "");
    char *dname = plc_path_name(&dp);
    plc_path_free(&dp);
    json_t *manifest = NULL;
    const char *name = NULL;
    char *name_owned = NULL;
    if (!plc_present(mf)) {
        json_free(mf);
        json_t *hp = plc_has_portable_manifest(io, d);
        if (plc_is_raise(hp)) { free(dname); return hp; }
        int portable = plc_truthy(hp);
        json_free(hp);
        if (!portable) { free(dname); return NULL; }
        /* try: manifest = read_agent_plugin_manifest(d)[0]; name = manifest["name"]
         * except Exception: return None */
        json_t *a = json_object();
        json_set(a, "plugin_dir", json_string(d ? d : ""));
        json_t *r = plc_call(io, "read_agent_plugin_manifest", a);
        if (plc_is_raise(r)) {
            if (plc_caught_by_exception(plc_raise_kind(r))) { json_free(r); free(dname); return NULL; }
            free(dname); return r;
        }
        /* manifest = read_agent_plugin_manifest(d)[0]; name = manifest["name"]
         * — the whole block is inside except Exception: return None, so any
         * subscript/key/type failure degrades to None. Success only when the
         * result is a non-empty list whose [0] is a dict carrying "name". */
        if (!r || r->type != JSON_ARRAY || json_len(r) == 0) { json_free(r); free(dname); return NULL; }
        manifest = json_copy(json_get(r, 0));
        json_free(r);
        if (!manifest || manifest->type != JSON_OBJECT || !json_has(manifest, "name")) {
            json_free(manifest); free(dname);
            return NULL;
        }
        name_owned = plc_py_str(json_obj_get(manifest, "name"));
        name = name_owned;
    } else {
        /* try: manifest = _load_yaml_manifest(manifest_file) except Exception: {} */
        char *mfs = strdup(plc_jstr(mf));
        json_free(mf);
        manifest = plc_load_yaml_manifest(io, mfs);
        free(mfs);
        if (plc_is_raise(manifest)) {
            if (plc_caught_by_exception(plc_raise_kind(manifest))) { json_free(manifest); manifest = json_object(); }
            else { free(dname); return manifest; }
        }
        if (!manifest || manifest->type != JSON_OBJECT) { json_free(manifest); manifest = json_object(); }
        /* name = manifest.get("name", d.name) */
        if (json_has(manifest, "name")) {
            name_owned = plc_py_str(json_obj_get(manifest, "name"));
            name = name_owned;
        } else {
            name = dname;
        }
    }
    /* key = f"{prefix}/{d.name}" if prefix else name */
    char *key;
    if (prefix && prefix[0]) key = plc_fmt("%s/%s", prefix, dname);
    else key = strdup(name ? name : "");
    json_t *out = json_array();
    json_append(out, json_string(name ? name : ""));
    json_append(out, json_has(manifest, "version") ? json_copy(json_obj_get(manifest, "version")) : json_string(""));
    json_append(out, json_has(manifest, "description") ? json_copy(json_obj_get(manifest, "description")) : json_string(""));
    json_append(out, json_string(key));
    free(key);
    free(name_owned);
    free(dname);
    json_free(manifest);
    return out;
}

/* PoP: plc_missing_env_specs @ hermes_cli/plugins_cmd.py:_missing_env_specs */
static json_t *plc_missing_env_specs(plc_io_t *io, const json_t *manifest) {
    if (!manifest || manifest->type != JSON_OBJECT)
        return plc_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", plc_type_name_of(manifest));
    /* env_specs = [normalized entries] (comprehension over requires_env or []) */
    const json_t *re = json_obj_get(manifest, "requires_env");
    const json_t *list = plc_truthy(re) ? re : NULL;
    json_t *specs = json_array();
    if (list && list->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(list); i++) {
            const json_t *entry = json_get(list, i);
            if (entry && entry->type == JSON_STRING) {
                json_t *d = json_object();
                json_set(d, "name", json_copy(entry));
                json_append(specs, d);
            } else if (entry && entry->type == JSON_OBJECT) {
                const json_t *nm = json_obj_get(entry, "name");
                if (plc_truthy(nm)) json_append(specs, json_copy(entry));
            }
            /* other types dropped */
        }
    }
    if (json_len(specs) == 0) { json_free(specs); return json_array(); }
    /* [s for s in env_specs if not get_env_value(s["name"])] — atomic comprehension */
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(specs); i++) {
        const json_t *s = json_get(specs, i);
        const char *nm = plc_jstr(json_obj_get(s, "name"));
        json_t *a = json_object();
        json_set(a, "name", json_string(nm));
        json_t *v = plc_call(io, "get_env_value", a);
        if (plc_is_raise(v)) { json_free(v); json_free(out); json_free(specs); return v; }
        int unset = !plc_truthy(v);
        json_free(v);
        if (unset) json_append(out, json_copy(s));
    }
    json_free(specs);
    return out;
}

/* ── message builders ── */

/* PoP: plc_clone_failure_message @ hermes_cli/plugins_cmd.py:_clone_failure_message */
static json_t *plc_clone_failure_message(plc_io_t *io, const char *git_url, const char *git_error) {
    char *stripped = plc_strip_ws(git_error ? git_error : "");
    json_t *a = json_object();
    json_set(a, "text", json_string(stripped ? stripped : ""));
    free(stripped);
    json_t *esc = plc_call(io, "markup_escape", a);
    if (plc_is_raise(esc)) return esc;
    char *msg = plc_fmt(
        "Could not download the plugin from %s. Check the address (browse the catalog "
        "with `hermes plugins search`), check your internet connection, or, if the repository "
        "is private, sign in first with `gh auth login` (or set GITHUB_TOKEN in your .env).\n"
        "Details: %s",
        git_url ? git_url : "", plc_jstr(esc));
    json_free(esc);
    json_t *out = json_string(msg ? msg : "");
    free(msg);
    return out;
}

/* PoP: plc_unknown_plugin_message @ hermes_cli/plugins_cmd.py:_unknown_plugin_message */
static json_t *plc_unknown_plugin_message(plc_io_t *io, const char *name, int downloaded_only) {
    const char *scope = downloaded_only
        ? " This command only works on downloaded plugins; bundled ones can only be enabled or disabled."
        : " Bundled plugins can only be enabled or disabled.";
    json_t *a = json_object();
    json_set(a, "name", json_string(name ? name : ""));
    json_t *moved = plc_call(io, "moved_plugin_hint", a);
    if (plc_is_raise(moved)) return moved;
    if (plc_truthy(moved)) {
        char *msg = plc_fmt("[red]No plugin named '%s'.[/red] %s", name ? name : "", plc_jstr(moved));
        json_free(moved);
        json_t *out = json_string(msg ? msg : "");
        free(msg);
        return out;
    }
    json_free(moved);
    char *msg = plc_fmt(
        "[red]No plugin named '%s'.[/red] Run `hermes plugins list` to see the exact names "
        "(nested plugins use their full key, e.g. web/firecrawl).%s "
        "To add one: `hermes plugins install <owner/repo>`.",
        name ? name : "", scope);
    json_t *out = json_string(msg ? msg : "");
    free(msg);
    return out;
}

/* _preserved_files_note core: exc message + scan_result projection + merged list.
 * Returns a malloc'd note string (the full f"{exc}\n\n{note}" text). */
static char *plc_preserved_files_note_core(const char *exc_msg, const json_t *scan_result, const json_t *merged) {
    plc_strset_t preserved;
    plc_ss_from_array(&preserved, merged);
    /* hits = sorted({f.file for f in findings if f.file in preserved}) */
    plc_strset_t hits;
    plc_ss_init(&hits);
    if (plc_present(scan_result) && scan_result->type == JSON_OBJECT) {
        const json_t *findings = json_obj_get(scan_result, "findings");
        if (findings && findings->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(findings); i++) {
                const json_t *f = json_get(findings, i);
                const char *file = plc_jstr(json_obj_get(f, "file"));
                if (plc_ss_contains(&preserved, file)) plc_ss_add(&hits, file);
            }
        }
    }
    plc_ss_sort(&hits);
    char *note;
    if (hits.n > 0) {
        /* ', '.join(hits) */
        size_t total = 0;
        for (int i = 0; i < hits.n; i++) total += strlen(hits.items[i]) + 2;
        char *joined = (char *)malloc(total + 1);
        size_t o = 0;
        for (int i = 0; i < hits.n; i++) {
            if (i) { joined[o++] = ','; joined[o++] = ' '; }
            size_t l = strlen(hits.items[i]);
            memcpy(joined + o, hits.items[i], l); o += l;
        }
        joined[o] = '\0';
        note = plc_fmt("These findings come from user files preserved from the installed copy: "
                       "%s. Move or remove them and retry the update.", joined);
        free(joined);
    } else {
        note = strdup("The scanned tree included user files preserved from the installed copy.");
    }
    char *full = plc_fmt("%s\n\n%s", exc_msg ? exc_msg : "", note ? note : "");
    free(note);
    plc_ss_free(&hits);
    plc_ss_free(&preserved);
    return full;
}

/* PoP: plc_preserved_files_note @ hermes_cli/plugins_cmd.py:_preserved_files_note */
static json_t *plc_preserved_files_note(const json_t *exc, const json_t *merged) {
    /* exc projection: {message, scan_result}; live exc is PluginScanBlocked. */
    const char *msg = json_get_str(exc, "message", "");
    const json_t *sr = exc ? json_obj_get(exc, "scan_result") : NULL;
    char *full = plc_preserved_files_note_core(msg, sr, merged);
    json_t *out = json_string(full ? full : "");
    free(full);
    return out;
}

/* PoP: plc_scan_merged_tree @ hermes_cli/plugins_cmd.py:_scan_merged_tree */
static json_t *plc_scan_merged_tree(plc_io_t *io, const char *plugin_dir, const char *identifier,
                                    const json_t *merged, const json_t *kwargs) {
    /* try: return _scan_plugin_tree(plugin_dir, identifier, **kwargs) */
    json_t *a = json_object();
    json_set(a, "plugin_dir", json_string(plugin_dir ? plugin_dir : ""));
    json_set(a, "identifier", json_string(identifier ? identifier : ""));
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < kwargs->c.count; i++)
            json_set(a, kwargs->c.keys[i], json_copy(kwargs->c.items[i]));
    }
    json_t *r = plc_call(io, "_scan_plugin_tree", a);
    if (!plc_is_raise(r)) return r;
    /* except PluginScanBlocked as exc */
    if (!plc_is_plugin_scan_blocked(plc_raise_kind(r))) return r;
    if (!plc_truthy(merged)) return r;   /* if not merged: raise */
    const json_t *sr = json_obj_get(r, "scan_result");
    char *note = plc_preserved_files_note_core(plc_raise_msg(r), sr, merged);
    /* the re-raise's ret projection is {__raise__, msg} only (drive catches the
     * live exception and projects type+str); scan_result rides the scripted seam
     * raise spec for materialization, not the returned ret. */
    json_t *e = plc_raise("PluginScanBlocked", note ? note : "");
    free(note);
    json_free(r);
    return e;
}

/* ── selection algebra ── */

/* forward declarations (composed ported-to-ported calls defined below) */
static json_t *plc_discover_all_plugins(plc_io_t *io);
static json_t *plc_scan_level(plc_io_t *io, const char *base, const char *source,
                              const json_t *skip_names, const char *prefix, int depth, json_t *seen);

/* PoP: plc_discard_key_and_leaf @ hermes_cli/plugins_cmd.py:_discard_key_and_leaf */
static json_t *plc_discard_key_and_leaf(const json_t *names, const char *key) {
    plc_strset_t s;
    plc_ss_from_array(&s, names);
    plc_ss_discard(&s, key ? key : "");
    char *leaf = plc_leaf(key);
    plc_ss_discard(&s, leaf);
    free(leaf);
    json_t *out = plc_ss_to_array(&s);
    plc_ss_free(&s);
    return out;
}

/* PoP: plc_plugin_aliases @ hermes_cli/plugins_cmd.py:_plugin_aliases */
static json_t *plc_plugin_aliases(plc_io_t *io, const char *key, const json_t *entries) {
    json_t *a = json_object();
    json_set(a, "key", json_string(key ? key : ""));
    json_t *legacy = plc_call(io, "legacy_plugin_keys", a);
    if (plc_is_raise(legacy)) return legacy;
    plc_strset_t s;
    plc_ss_init(&s);
    plc_ss_add(&s, key ? key : "");
    char *leaf = plc_leaf(key);
    plc_ss_add(&s, leaf);
    free(leaf);
    if (legacy && legacy->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(legacy); i++) plc_ss_add(&s, plc_jstr(json_get(legacy, i)));
    json_free(legacy);
    /* entries is None -> _discover_all_plugins() REAL; else the provided list */
    json_t *src;
    int src_owned = 0;
    if (plc_present(entries)) {
        src = (json_t *)entries;
    } else {
        src = plc_discover_all_plugins(io);
        src_owned = 1;
        if (plc_is_raise(src)) { plc_ss_free(&s); return src; }
    }
    if (src && src->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(src); i++) {
            const json_t *e = json_get(src, i);
            if (e && e->type == JSON_ARRAY && json_len(e) >= 6) {
                if (strcmp(plc_jstr(json_get(e, 5)), key ? key : "") == 0)
                    plc_ss_add(&s, plc_jstr(json_get(e, 0)));
            }
        }
    }
    if (src_owned) json_free(src);
    json_t *out = plc_ss_to_array(&s);
    plc_ss_free(&s);
    return out;
}

/* PoP: plc_disabled_by_update @ hermes_cli/plugins_cmd.py:_disabled_by_update */
static json_t *plc_disabled_by_update(plc_io_t *io, const char *key, const json_t *aliases) {
    json_t *home = plc_call(io, "get_hermes_home", json_object());
    if (plc_is_raise(home)) return home;
    json_t *ha = json_object();
    json_set(ha, "home", json_string(plc_jstr(home)));
    json_free(home);
    json_t *sel = plc_call(io, "read_home_selection", ha);
    if (plc_is_raise(sel)) return sel;
    /* (sel or {}).get("plugins") */
    json_t *cfg = plc_truthy(sel) ? sel : NULL;
    if (cfg && cfg->type != JSON_OBJECT) {
        json_t *e = plc_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", plc_type_name_of(cfg));
        json_free(sel); return e;
    }
    const json_t *plugins = cfg ? json_obj_get(cfg, "plugins") : NULL;
    const json_t *pcfg = plc_truthy(plugins) ? plugins : NULL;
    if (pcfg && pcfg->type != JSON_OBJECT) {
        json_t *e = plc_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", plc_type_name_of(pcfg));
        json_free(sel); return e;
    }
    const json_t *record = pcfg ? json_obj_get(pcfg, PLC_RECORD) : NULL;
    int is_dict = record && record->type == JSON_OBJECT;
    int hit = 0;
    if (is_dict) {
        plc_strset_t want;
        plc_ss_init(&want);
        plc_ss_add(&want, key ? key : "");
        plc_strset_t al;
        plc_ss_from_array(&al, aliases);
        for (int i = 0; i < al.n; i++) plc_ss_add(&want, al.items[i]);
        plc_ss_free(&al);
        for (size_t i = 0; i < record->c.count && !hit; i++)
            if (plc_ss_contains(&want, record->c.keys[i])) hit = 1;
        plc_ss_free(&want);
    }
    json_free(sel);
    return json_bool((is_dict && hit) ? true : false);
}

/* PoP: plc_apply_activation @ hermes_cli/plugins_cmd.py:_apply_activation */
static json_t *plc_apply_activation(const json_t *enabled, const json_t *disabled,
                                    const char *key, const json_t *aliases, int enable) {
    plc_strset_t en, dis, al;
    plc_ss_from_array(&en, enabled);
    plc_ss_from_array(&dis, disabled);
    plc_ss_from_array(&al, aliases);
    plc_strset_t *removed = enable ? &dis : &en;
    plc_ss_discard(removed, key ? key : "");
    char *leaf = plc_leaf(key);
    plc_ss_discard(removed, leaf);
    free(leaf);
    for (int i = 0; i < al.n; i++) plc_ss_discard(removed, al.items[i]);
    plc_strset_t *target = enable ? &en : &dis;
    plc_ss_add(target, key ? key : "");
    json_t *out = json_object();
    json_set(out, "enabled", plc_ss_to_array(&en));
    json_set(out, "disabled", plc_ss_to_array(&dis));
    plc_ss_free(&en); plc_ss_free(&dis); plc_ss_free(&al);
    return out;
}

/* PoP: plc_activate_key @ hermes_cli/plugins_cmd.py:_activate_key */
static json_t *plc_activate_key(plc_io_t *io, const char *key, int enable, const json_t *console) {
    json_t *en = plc_call(io, "get_enabled_set", json_object());
    if (plc_is_raise(en)) return en;
    json_t *dis = plc_call(io, "get_disabled_set", json_object());
    if (plc_is_raise(dis)) { json_free(en); return dis; }
    json_t *aliases = plc_plugin_aliases(io, key, NULL);
    if (plc_is_raise(aliases)) { json_free(en); json_free(dis); return aliases; }
    plc_strset_t en_s, dis_s, al_s;
    plc_ss_from_array(&en_s, en);
    plc_ss_from_array(&dis_s, dis);
    plc_ss_from_array(&al_s, aliases);
    plc_strset_t *target = enable ? &en_s : &dis_s;
    plc_strset_t *other = enable ? &dis_s : &en_s;
    int skip = 0;
    /* if key in target and not (aliases & other) and not (not enable and _disabled_by_update(...)) */
    if (plc_ss_contains(target, key ? key : "") && !plc_ss_intersects(&al_s, other)) {
        if (enable) {
            skip = 1;
        } else {
            json_t *dbu = plc_disabled_by_update(io, key, aliases);
            if (plc_is_raise(dbu)) {
                plc_ss_free(&en_s); plc_ss_free(&dis_s); plc_ss_free(&al_s);
                json_free(en); json_free(dis); json_free(aliases);
                return dbu;
            }
            int disabled_by_update = plc_truthy(dbu);
            json_free(dbu);
            if (!disabled_by_update) skip = 1;
        }
    }
    plc_ss_free(&en_s); plc_ss_free(&dis_s); plc_ss_free(&al_s);
    json_free(en); json_free(dis);
    if (skip) { json_free(aliases); return json_bool(false); }
    /* _set_plugin_enabled(key, enable=enable, aliases=aliases, console=console) — seam */
    json_t *a = json_object();
    json_set(a, "name", json_string(key ? key : ""));
    json_set(a, "enable", json_bool(enable ? true : false));
    json_set(a, "aliases", json_copy(aliases));
    if (plc_present(console)) json_set(a, "console", json_copy(console));
    json_free(aliases);
    json_t *r = plc_call(io, "set_plugin_enabled", a);
    if (plc_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* PoP: plc_forget_plugin_config @ hermes_cli/plugins_cmd.py:_forget_plugin_config */
static json_t *plc_forget_plugin_config(plc_io_t *io, const json_t *aliases) {
    json_t *config = plc_call(io, "load_config", json_object());
    if (plc_is_raise(config)) return config;
    if (!config || config->type != JSON_OBJECT) {
        json_t *e = plc_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", plc_type_name_of(config));
        json_free(config); return e;
    }
    plc_strset_t al;
    plc_ss_from_array(&al, aliases);
    int changed = 0;
    json_t *result = json_object();
    json_t *plugins_cfg = json_obj_get(config, "plugins");
    if (plugins_cfg && plugins_cfg->type == JSON_OBJECT) {
        const char *list_keys[2] = {"enabled", "disabled"};
        for (int k = 0; k < 2; k++) {
            json_t *names = json_obj_get(plugins_cfg, list_keys[k]);
            if (names && names->type == JSON_ARRAY) {
                plc_strset_t ns;
                plc_ss_from_array(&ns, names);
                if (plc_ss_intersects(&al, &ns)) {
                    plc_strset_t diff;
                    plc_ss_init(&diff);
                    for (int i = 0; i < ns.n; i++)
                        if (!plc_ss_contains(&al, ns.items[i])) plc_ss_add(&diff, ns.items[i]);
                    json_set(plugins_cfg, list_keys[k], plc_ss_to_array(&diff));
                    plc_ss_free(&diff);
                    changed = 1;
                }
                plc_ss_free(&ns);
            }
        }
        json_t *entries = json_obj_get(plugins_cfg, "entries");
        if (entries && entries->type == JSON_OBJECT) {
            int hit = 0;
            for (size_t i = 0; i < entries->c.count; i++)
                if (plc_ss_contains(&al, entries->c.keys[i])) { hit = 1; break; }
            if (hit) {
                /* del entries[alias] for alias in aliases & set(entries) */
                for (int i = 0; i < al.n; i++)
                    if (json_has(entries, al.items[i])) json_obj_del(entries, al.items[i]);
                changed = 1;
            }
        }
    }
    json_t *memory_cfg = json_obj_get(config, "memory");
    if (memory_cfg && memory_cfg->type == JSON_OBJECT) {
        char *prov = plc_py_str_or_empty(json_obj_get(memory_cfg, "provider"));
        char *stripped = plc_strip_ws(prov);
        free(prov);
        if (plc_ss_contains(&al, stripped ? stripped : "")) {
            json_set(memory_cfg, "provider", json_string(""));
            changed = 1;
            json_free(result);
            result = json_object();
            json_set(result, "cleared_memory_provider", json_bool(true));
        }
        free(stripped);
    }
    plc_ss_free(&al);
    if (changed) {
        json_t *a = json_object();
        json_set(a, "config", json_copy(config));
        json_t *r = plc_call(io, "save_config", a);
        if (plc_is_raise(r)) { json_free(config); json_free(result); return r; }
        json_free(r);
    }
    json_free(config);
    return result;
}

/* PoP: plc_plugin_selection_version @ hermes_cli/plugins_cmd.py:_plugin_selection_version */
static json_t *plc_plugin_selection_version(plc_io_t *io) {
    json_t *home = plc_call(io, "get_hermes_home", json_object());
    if (plc_is_raise(home)) return home;
    char *path = plc_posix_join(plc_jstr(home), "config.yaml");
    json_free(home);
    json_t *a = json_object();
    json_set(a, "path", json_string(path));
    free(path);
    json_t *d = plc_call(io, "_digest", a);
    if (plc_is_raise(d)) return d;
    if (plc_truthy(d)) return d;
    json_free(d);
    return json_string("missing");
}

/* PoP: plc_refuse_recalled_entry @ hermes_cli/plugins_cmd.py:_refuse_recalled_entry */
static json_t *plc_refuse_recalled_entry(plc_io_t *io, const json_t *entry) {
    if (plc_present(entry) && entry->type == JSON_ARRAY && json_len(entry) >= 5) {
        if (strcmp(plc_jstr(json_get(entry, 3)), "bundled") != 0) {
            json_t *a = json_object();
            json_set(a, "name", json_string(plc_jstr(json_get(entry, 0))));
            json_set(a, "plugin_dir", json_string(plc_jstr(json_get(entry, 4))));
            json_t *r = plc_call(io, "refuse_if_installed_removed", a);
            if (plc_is_raise(r)) return r;
            json_free(r);
        }
    }
    return NULL;
}

/* PoP: plc_tri_state_flag @ hermes_cli/plugins_cmd.py:_tri_state_flag */
static json_t *plc_tri_state_flag(const json_t *args, const char *yes_attr, const char *no_attr) {
    const json_t *y = args ? json_obj_get(args, yes_attr) : NULL;
    if (plc_truthy(y)) return json_bool(true);
    const json_t *n = args ? json_obj_get(args, no_attr) : NULL;
    if (plc_truthy(n)) return json_bool(false);
    return NULL;
}

/* PoP: plc_category_active_names @ hermes_cli/plugins_cmd.py:_category_active_names */
static json_t *plc_category_active_names(plc_io_t *io) {
    json_t *m = plc_call(io, "get_current_memory_provider", json_object());
    if (plc_is_raise(m)) return m;
    json_t *out = json_array();
    if (plc_truthy(m)) json_append(out, json_string(plc_jstr(m)));
    json_free(m);
    return out;
}

/* PoP: plc_saved_list_toolset_key @ hermes_cli/plugins_cmd.py:_saved_list_toolset_key */
static json_t *plc_saved_list_toolset_key(plc_io_t *io, const char *name) {
    json_t *cfg = plc_call(io, "load_config", json_object());
    if (plc_is_raise(cfg)) return cfg;
    if (!cfg || cfg->type != JSON_OBJECT) {
        json_t *e = plc_raise_fmt("AttributeError", "'%s' object has no attribute 'get'", plc_type_name_of(cfg));
        json_free(cfg); return e;
    }
    const json_t *saved = json_obj_get(cfg, "platform_toolsets");
    int any_list = 0;
    if (saved && saved->type == JSON_OBJECT) {
        /* all(parse(v) is None for v in saved.values()) — short-circuit on first non-None */
        for (size_t i = 0; i < saved->c.count; i++) {
            json_t *a = json_object();
            json_set(a, "value", json_copy(saved->c.items[i]));
            json_t *p = plc_call(io, "parse_platform_toolsets_value", a);
            if (plc_is_raise(p)) { json_free(cfg); return p; }
            int is_none = !plc_present(p);
            json_free(p);
            if (!is_none) { any_list = 1; break; }
        }
    }
    json_free(cfg);
    if (!any_list) return NULL;
    json_t *a = json_object();
    json_set(a, "name", json_string(name ? name : ""));
    return plc_call(io, "get_plugin_toolset_key", a);
}

/* ── discovery scan ── */

/* logger.warning("Skipping unreadable plugin directory %s: %s", path, exc) */
static json_t *plc_scan_warn(plc_io_t *io, const char *path, const char *exc_msg) {
    json_t *la = json_array();
    json_append(la, json_string(path ? path : ""));
    json_append(la, json_string(exc_msg ? exc_msg : ""));
    return plc_log_warning(io, "Skipping unreadable plugin directory %s: %s", la);
}

/* PoP: plc_scan_level @ hermes_cli/plugins_cmd.py:_scan_level */
static json_t *plc_scan_level(plc_io_t *io, const char *base, const char *source,
                              const json_t *skip_names, const char *prefix, int depth, json_t *seen) {
    json_t *isd = plc_call(io, "is_dir", plc_path_args(base));
    if (plc_is_raise(isd)) return isd;
    int isdir = plc_truthy(isd);
    json_free(isd);
    if (!isdir) return NULL;
    json_t *children = plc_call(io, "iterdir", plc_path_args(base));
    if (plc_is_raise(children)) {
        if (plc_is_oserror(plc_raise_kind(children))) {
            json_t *w = plc_scan_warn(io, base, plc_raise_msg(children));
            json_free(children);
            if (plc_is_raise(w)) return w;
            json_free(w);
            return NULL;
        }
        return children;
    }
    /* sorted(base.iterdir()) — sort child path strings (ASCII domain) */
    plc_strset_t kids;
    plc_ss_init(&kids);
    if (children && children->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(children); i++) plc_ss_add(&kids, plc_jstr(json_get(children, i)));
    json_free(children);
    plc_ss_sort(&kids);
    plc_strset_t skip;
    plc_ss_from_array(&skip, skip_names);
    json_t *ret = NULL;
    for (int ci = 0; ci < kids.n && !ret; ci++) {
        const char *d = kids.items[ci];
        plc_path_t dpath; plc_path_parse(&dpath, d);
        char *dname = plc_path_name(&dpath);
        plc_path_free(&dpath);
        /* try: if not d.is_dir() or (depth==0 and skip and d.name in skip): continue;
         *      info = _read_manifest_info(d, prefix)
         * except (OSError, PluginOperationError): warn; continue */
        json_t *d_isdir = plc_call(io, "is_dir", plc_path_args(d));
        if (plc_is_raise(d_isdir)) {
            const char *k = plc_raise_kind(d_isdir);
            if (plc_is_oserror(k) || plc_is_plugin_operation_error(k)) {
                json_t *w = plc_scan_warn(io, d, plc_raise_msg(d_isdir));
                json_free(d_isdir);
                if (plc_is_raise(w)) { ret = w; free(dname); break; }
                json_free(w);
                free(dname);
                continue;
            }
            ret = d_isdir; free(dname); break;
        }
        int d_is_dir = plc_truthy(d_isdir);
        json_free(d_isdir);
        if (!d_is_dir || (depth == 0 && skip.n > 0 && plc_ss_contains(&skip, dname))) {
            free(dname);
            continue;
        }
        json_t *info = plc_read_manifest_info(io, d, prefix);
        if (plc_is_raise(info)) {
            const char *k = plc_raise_kind(info);
            if (plc_is_oserror(k) || plc_is_plugin_operation_error(k)) {
                json_t *w = plc_scan_warn(io, d, plc_raise_msg(info));
                json_free(info);
                if (plc_is_raise(w)) { ret = w; free(dname); break; }
                json_free(w);
                free(dname);
                continue;
            }
            ret = info; free(dname); break;
        }
        if (!info) {
            if (depth == 0) {
                char *np = (prefix && prefix[0]) ? plc_fmt("%s/%s", prefix, dname) : strdup(dname);
                json_t *empty_skip = json_array();
                json_t *r = plc_scan_level(io, d, source, empty_skip, np, 1, seen);
                json_free(empty_skip);
                free(np);
                if (plc_is_raise(r)) { ret = r; free(dname); break; }
                json_free(r);
            }
            free(dname);
            continue;
        }
        /* name, version, description, key = info */
        const char *key = plc_jstr(json_get(info, 3));
        if (json_has(seen, key) && strcmp(source, "bundled") == 0) {
            json_free(info); free(dname);
            continue;
        }
        const char *src_label = source;
        char *git_label = NULL;
        if (strcmp(source, "user") == 0) {
            char *gitp = plc_posix_join(d, ".git");
            json_t *ge = plc_call(io, "exists", plc_path_args(gitp));
            free(gitp);
            if (plc_is_raise(ge)) { ret = ge; json_free(info); free(dname); break; }
            if (plc_truthy(ge)) { git_label = strdup("git"); src_label = git_label; }
            json_free(ge);
        }
        json_t *row = json_array();
        json_append(row, json_copy(json_get(info, 0)));
        json_append(row, json_copy(json_get(info, 1)));
        json_append(row, json_copy(json_get(info, 2)));
        json_append(row, json_string(src_label));
        json_append(row, json_string(d));
        json_append(row, json_string(key));
        json_set(seen, key, row);
        free(git_label);
        json_free(info);
        free(dname);
    }
    plc_ss_free(&kids);
    plc_ss_free(&skip);
    return ret;
}

/* PoP: plc_discover_all_plugins @ hermes_cli/plugins_cmd.py:_discover_all_plugins */
static json_t *plc_discover_all_plugins(plc_io_t *io) {
    json_t *seen = json_object();
    /* The live `for base, source, skip in ((get_bundled_plugins_dir(), ...),
     * (_plugins_dir(), ...))` evaluates the tuple EAGERLY: BOTH dir seams fire
     * before any _scan_level. Fetch both up front, then scan in order. */
    json_t *bundled = plc_call(io, "get_bundled_plugins_dir", json_object());
    if (plc_is_raise(bundled)) { json_free(seen); return bundled; }
    json_t *userdir = plc_call(io, "_plugins_dir", json_object());
    if (plc_is_raise(userdir)) { json_free(seen); json_free(bundled); return userdir; }
    json_t *skip = json_array();
    for (int i = 0; i < PLC_N_BUNDLED_SKIP; i++) json_append(skip, json_string(PLC_BUNDLED_SKIP[i]));
    json_t *r = plc_scan_level(io, plc_jstr(bundled), "bundled", skip, "", 0, seen);
    json_free(skip);
    json_free(bundled);
    if (plc_is_raise(r)) { json_free(seen); json_free(userdir); return r; }
    json_free(r);
    json_t *empty_skip = json_array();
    r = plc_scan_level(io, plc_jstr(userdir), "user", empty_skip, "", 0, seen);
    json_free(empty_skip);
    json_free(userdir);
    if (plc_is_raise(r)) { json_free(seen); return r; }
    json_free(r);
    json_t *eps = plc_call(io, "discover_entrypoint_manifests", json_object());
    if (plc_is_raise(eps)) { json_free(seen); return eps; }
    if (eps && eps->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(eps); i++) {
            const json_t *m = json_get(eps, i);
            const char *mname = plc_jstr(json_obj_get(m, "name"));
            if (!json_has(seen, mname)) {
                json_t *row = json_array();
                json_append(row, json_string(mname));
                json_append(row, json_copy(json_obj_get(m, "version")));
                json_append(row, json_copy(json_obj_get(m, "description")));
                json_append(row, json_string("entrypoint"));
                json_append(row, json_copy(json_obj_get(m, "path")));
                json_append(row, json_string(mname));
                json_set(seen, mname, row);
            }
        }
    }
    json_free(eps);
    json_t *out = json_array();
    for (size_t i = 0; i < seen->c.count; i++) json_append(out, json_copy(seen->c.items[i]));
    json_free(seen);
    return out;
}

/* PoP: plc_default_on @ hermes_cli/plugins_cmd.py:_default_on */
static json_t *plc_default_on(plc_io_t *io, const char *dir_path, const char *source) {
    json_t *isd = plc_call(io, "is_dir", plc_path_args(dir_path));
    if (plc_is_raise(isd)) return isd;
    int isdir = plc_truthy(isd);
    json_free(isd);
    if (!isdir) return json_bool(false);
    plc_path_t pp; plc_path_parse(&pp, dir_path ? dir_path : "");
    char *pname = plc_path_name(&pp);
    plc_path_t par; plc_path_parent(&par, &pp);
    char *parent = plc_path_str(&par);
    plc_path_free(&pp); plc_path_free(&par);
    if (strcmp(source, "bundled") != 0) {
        json_t *root = plc_call(io, "_plugins_dir", json_object());
        if (plc_is_raise(root)) { free(pname); free(parent); return root; }
        plc_path_t rp; plc_path_parse(&rp, plc_jstr(root));
        char *root_s = plc_path_str(&rp);
        plc_path_free(&rp);
        json_free(root);
        char *root_mp = plc_posix_join(root_s, "model-providers");
        int name_bad = (pname[0] == '_' || pname[0] == '.');
        int parent_ok = (strcmp(parent, root_s) == 0 || strcmp(parent, root_mp) == 0);
        if (name_bad || !parent_ok) {
            free(pname); free(parent); free(root_s); free(root_mp);
            return json_bool(false);
        }
        if (strcmp(parent, root_s) == 0) {
            json_t *a = json_object();
            json_set(a, "path", json_string(dir_path ? dir_path : ""));
            json_t *dk = plc_call(io, "_declares_model_provider_kind", a);
            free(pname); free(parent); free(root_s); free(root_mp);
            if (plc_is_raise(dk)) return dk;
            int declares = plc_truthy(dk);
            json_free(dk);
            return json_bool(declares ? true : false);
        }
        free(root_s); free(root_mp);
    }
    free(pname); free(parent);
    json_t *mf = plc_native_manifest_file(io, dir_path);
    if (plc_is_raise(mf)) return mf;
    if (!plc_present(mf)) { json_free(mf); return json_bool(false); }
    char *mfs = strdup(plc_jstr(mf));
    json_free(mf);
    json_t *manifest = plc_load_yaml_manifest(io, mfs);
    free(mfs);
    if (plc_is_raise(manifest)) {
        if (plc_caught_by_exception(plc_raise_kind(manifest))) { json_free(manifest); return json_bool(false); }
        return manifest;
    }
    if (!manifest || manifest->type != JSON_OBJECT) { json_free(manifest); return json_bool(false); }
    const json_t *kind_raw = json_has(manifest, "kind") ? json_obj_get(manifest, "kind") : NULL;
    char *ks = kind_raw ? plc_py_str(kind_raw) : strdup("standalone");
    json_free(manifest);
    char *stripped = plc_strip_ws(ks);
    free(ks);
    char *kind = plc_lower_ascii(stripped ? stripped : "");
    free(stripped);
    int on = (strcmp(kind, "model-provider") == 0);
    if (!on && strcmp(source, "bundled") == 0) {
        for (size_t i = 0; i < sizeof PLC_BUNDLED_DEFAULT_ON_KINDS / sizeof PLC_BUNDLED_DEFAULT_ON_KINDS[0]; i++)
            if (strcmp(kind, PLC_BUNDLED_DEFAULT_ON_KINDS[i]) == 0) { on = 1; break; }
    }
    free(kind);
    return json_bool(on ? true : false);
}