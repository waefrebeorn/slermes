/*
 * profiles_flow.c — FLOW cores + leaves of hermes_cli/profiles.py (LIVE
 * authority) that NO existing PoP claims: the credential-path privacy
 * predicates (_fold, profile_path_is_private, profile_path_contains_private_store,
 * _export_credential_entries), the name-suggestion + error-message builders
 * (_suggest_profile_name, _invalid_profile_name_error, _missing_profile_error,
 * _unknown_profile_error, _profile_exists_error), the previous-names cleaner
 * (_clean_previous_names), the Hermes-argv probes (_is_hermes_argv,
 * _argv_profile_selectors), the pure path algebra (profile_root_for_env_home,
 * parked_marker_path), and the seam flows: _standalone_truthy, _canon_valid,
 * _existing_profile_dir, _wrapper_path, _is_our_wrapper, _find_our_wrapper,
 * _non_exportable_entries, _notify_multiplexer, _purge_identity,
 * _live_default_multiplexer, _served_by_running_multiplexer,
 * _retarget_active_profile, current_profile_name, _inside_git_checkout,
 * _s6_runtime_manager, _refuse_rename_without_file_tooling, _load_yaml_dict,
 * _seed_file_if_missing, _clone_file, _resolve_clone_source, _seed_model_config,
 * _strip_multiplex_flag.
 *
 * Self-contained C11, libjson-only. Claimed siblings (normalize_profile_name,
 * validate_profile_name, get_profile_dir, _get_wrapper_dir, get_active_profile,
 * set_active_profile, get_active_profile_name) and every foreign collaborator
 * (gateway.config._bool_token, the gateway_multiplex_served / profile_identity /
 * gateway / service_manager / config_backend / hermes_yaml / config /
 * profile_model_seed helpers, os.lstat / os.chmod, shutil.copy2, the Path
 * stat/IO methods) enter through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; pf_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is delivered
 * as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised exception).
 * The harness records {"ev": name, "args": args, "ret": result} per call in
 * order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (pf_suggest_profile_name inside
 * pf_invalid_profile_name_error; pf_canon_valid inside pf_existing_profile_dir
 * and pf_resolve_clone_source; pf_wrapper_path + pf_is_our_wrapper inside
 * pf_find_our_wrapper); the oracle keeps the extracted defs real on the same
 * paths, so their internal seam events appear in BOTH traces.
 *
 * Exception propagation: every pf flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except Exception` sites swallow every
 * non-BaseException kind; `except OSError` / `suppress(OSError)` sites swallow
 * the OSError family only; `except (OSError, RuntimeError)` (_inside_git_checkout)
 * swallows those two families. The BaseException trio (SystemExit /
 * KeyboardInterrupt / GeneratorExit) ALWAYS passes through. The error-message
 * builders RETURN a raise spec (live returns the exception object; the oracle
 * projects it to {__raise__, msg}); composed callers raise it.
 * Fail-closed: a missing/empty seam script yields a RuntimeError raise spec
 * (recorded, then propagated/swallowed exactly as live); no seam installed at
 * all yields the same spec.
 *
 * Path model: paths are verbatim POSIX strings. The oracle seeds PurePosixPath
 * (str keeps separators on this Windows host) and a posixpath-backed os double;
 * the C uses a lexical PurePosixPath component model (drop empty + '.' parts,
 * keep '..', root '/' — or '//' for EXACTLY two leading slashes). Path-valued
 * seam results materialize as a PPath double whose stat/IO methods fire the
 * is_dir / exists / is_file / is_symlink / resolve / read_text / write_text /
 * mkdir seams; the C reads the path string and fires the same seam.
 *
 * Fixture domain (ASCII): inputs are ASCII; integral floats normalize to ints
 * on both sides and are kept OUT of str()-computed positions (_clean_previous_names
 * items, error-message names); _is_hermes_argv argv is a non-empty string list;
 * _non_exportable_entries lstat scripts are {st_mode:int} or an OSError raise;
 * _s6_runtime_manager get_service_manager scripts are a marker string or null;
 * _refuse_rename_without_file_tooling get_config_backend scripts are {name:str}
 * or null; regex probes (_STORE_COPY_RE / _PYTHON_INTERPRETER_RE / PROFILE_ID_RE)
 * stay single-line (re '.' excludes newline).
 *
 * Seam name table (name -> live call):
 *   normalize_profile_name      profiles.normalize_profile_name(name) [claimed]
 *   validate_profile_name       profiles.validate_profile_name(name) [claimed]
 *   get_profile_dir             profiles.get_profile_dir(name) [claimed]
 *   _get_wrapper_dir            profiles._get_wrapper_dir() [claimed]
 *   get_active_profile          profiles.get_active_profile() [claimed]
 *   set_active_profile          profiles.set_active_profile(name) [claimed]
 *   get_active_profile_name     profiles.get_active_profile_name() [claimed]
 *   wrapper_read                open(path,utf-8-sig).read(8192) head text
 *   is_dir / exists / is_file / is_symlink   Path.<method>() args{path}
 *   resolve                     Path.resolve() args{path}
 *   read_text                   Path.read_text(utf-8-sig) args{path}
 *   write_text                  Path.write_text(text,utf-8) args{path,text}
 *   mkdir                       Path.mkdir(parents=True,exist_ok=True) args{path}
 *   lstat                       os.lstat(path) args{path} -> {st_mode}
 *   chmod                       os.chmod(path,mode) args{path,mode}
 *   copy2                       shutil.copy2(src,dst) args{src,dst}
 *   _bool_token                 gateway.config._bool_token(value) args{value}
 *   notify_multiplexer_profiles_changed   gateway_multiplex_served args{canon}
 *   purge_profile_identity      profile_identity.purge_profile_identity {canon}
 *   recorded_served_profiles    gateway_multiplex_served.recorded_served_profiles {}
 *   named_profile_served_by_running_multiplexer  hermes_cli.gateway {profile_name}
 *   get_hermes_home_override    hermes_constants.get_hermes_home_override {}
 *   get_hermes_home             hermes_constants.get_hermes_home {}
 *   detect_service_manager      service_manager.detect_service_manager {}
 *   get_service_manager         service_manager.get_service_manager {}
 *   supports_runtime_registration   mgr.supports_runtime_registration() {}
 *   get_config_backend          config_backend.get_config_backend {}
 *   supports_file_tooling       backend.supports_file_tooling() {}
 *   safe_load                   hermes_yaml.safe_load(text) args{text}
 *   require_file_tooling        config_backend.require_file_tooling(label) {label}
 *   config_exists               config_backend.config_exists(path) {path}
 *   read_user_config_raw        hermes_cli.config.read_user_config_raw(path) {path}
 *   launch_model_seed           profile_model_seed.launch_model_seed(cfg) {config}
 *   atomic_config_write         hermes_cli.config.atomic_config_write(p,cfg) {path,config}
 *   atomic_config_replace       hermes_cli.config.atomic_config_replace(p,cfg) {path,config}
 *   print                       builtin print(*values) args{args:[...]}
 *
 * Ambient (io->env, not seams): win32 (sys.platform=='win32'); os.environ for
 * current_profile_name travels in the case input "environ" (a plain dict read).
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

typedef json_t *(*pf_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct pf_io {
    pf_seam_fn call;
    void *ctx;
    const json_t *env;   /* borrowed ambient values (win32 flag) */
} pf_io_t;

static int pf_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *pf_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *pf_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* ── FULL builtin exception hierarchy (parent table) ──
 * pf_exc_parent walks to Exception / BaseException. Unknown kinds default to
 * Exception (a synthetic Exception subclass), so `except Exception` catches
 * them; the BaseException trio walks to BaseException only and passes through. */
static const struct { const char *kind; const char *parent; } PF_EXC_PARENTS[] = {
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
};
#define PF_N_EXC ((int)(sizeof PF_EXC_PARENTS / sizeof PF_EXC_PARENTS[0]))

static const char *pf_exc_parent(const char *k) {
    for (int i = 0; i < PF_N_EXC; i++)
        if (strcmp(PF_EXC_PARENTS[i].kind, k) == 0) return PF_EXC_PARENTS[i].parent;
    return "Exception";   /* unknown kind: a synthetic Exception subclass */
}

/* Walk the parent chain until `ancestor` (true) or BaseException/NULL (false). */
static int pf_kind_is_a(const char *k, const char *ancestor) {
    char cur[128];
    snprintf(cur, sizeof cur, "%s", k);
    for (int depth = 0; depth < 32; depth++) {
        if (strcmp(cur, ancestor) == 0) return 1;
        if (strcmp(cur, "BaseException") == 0) return 0;
        const char *p = pf_exc_parent(cur);
        if (!p || strcmp(p, cur) == 0) return 0;
        snprintf(cur, sizeof cur, "%s", p);
    }
    return 0;
}

static int pf_caught_by_exception(const char *k) { return pf_kind_is_a(k, "Exception"); }
static int pf_is_oserror(const char *k) { return pf_kind_is_a(k, "OSError"); }
static int pf_is_valueerror(const char *k) { return pf_kind_is_a(k, "ValueError"); }
static int pf_is_runtimeerror(const char *k) { return pf_kind_is_a(k, "RuntimeError"); }

static json_t *pf_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *pf_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return pf_raise(kind, buf);
}

static char *pf_fmt(const char *fmt, ...) {
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
static json_t *pf_call(pf_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = pf_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

static const json_t *pf_env(const pf_io_t *io, const char *key) {
    return (io && io->env) ? json_obj_get(io->env, key) : NULL;
}

static int pf_env_bool(const pf_io_t *io, const char *key) {
    const json_t *v = pf_env(io, key);
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int pf_truthy(const json_t *v) {
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

/* Python `x is not None` (a present, non-null node). */
static int pf_present(const json_t *v) { return v && v->type != JSON_NULL; }

/* Scalar string reader (json_get_str is an OBJECT+key accessor). */
static const char *pf_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static const char *pf_type_name(const json_t *v) {
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

/* Decode one UTF-8 codepoint at s[*i], advance *i. Invalid bytes -> U+FFFD. */
static unsigned long pf_utf8_next(const char *s, size_t n, size_t *i) {
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

static int pf_is_ws_cp(unsigned long cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || cp == 0x85 || cp == 0xA0) return 1;
    if (cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    return 0;
}

/* str.strip() — full Unicode White_Space set, codepoint-aware. malloc'd. */
static char *pf_strip_ws(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t start = 0, end = n;
    size_t i = 0;
    while (i < n) {
        size_t save = i;
        unsigned long cp = pf_utf8_next(s, n, &i);
        if (!pf_is_ws_cp(cp)) { start = save; break; }
        start = i;
    }
    /* trim trailing: walk codepoints from start, remember last non-ws end */
    size_t last = start;
    i = start;
    while (i < n) {
        unsigned long cp = pf_utf8_next(s, n, &i);
        if (!pf_is_ws_cp(cp)) last = i;
    }
    end = last;
    if (end < start) end = start;
    char *out = (char *)malloc(end - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
    return out;
}

/* str.strip(chars) — strip a SET of ASCII chars from both ends. malloc'd. */
static char *pf_strip_chars(const char *s, const char *chars) {
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

/* ASCII lower (fixture domain is ASCII). malloc'd. */
static char *pf_lower_ascii(const char *s) {
    if (!s) return strdup("");
    char *out = strdup(s);
    if (!out) return NULL;
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return out;
}

/* casefold == lower for the ASCII fixture domain. */
static char *pf_casefold(const char *s) { return pf_lower_ascii(s); }

/* Shortest round-trip float repr matching Python repr(). */
static char *pf_float_repr(double v) {
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
static char *pf_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) {
            snprintf(buf, sizeof buf, "%.0f", d);
            return strdup(buf);
        }
        return pf_float_repr(d);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* repr() of a string (Python delimiter choice + escapes; ASCII domain). */
static char *pf_repr_str(const char *s) {
    if (!s) s = "";
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == (unsigned char)q || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7f) { o += (size_t)snprintf(out + o, 5, "\\x%02x", c); }
        else out[o++] = (char)c;
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}

/* ── lexical PurePosixPath component model ── */

typedef struct {
    char *root;     /* "", "/" or "//" */
    char **parts;   /* owned components (no empty / '.'; '..' kept) */
    int n;
    int cap;
} pf_path_t;

static void pf_path_init(pf_path_t *p) { p->root = strdup(""); p->parts = NULL; p->n = 0; p->cap = 0; }

static void pf_path_free(pf_path_t *p) {
    free(p->root);
    for (int i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->root = NULL; p->parts = NULL; p->n = 0; p->cap = 0;
}

static void pf_path_push(pf_path_t *p, const char *s, size_t len) {
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 8;
        p->parts = (char **)realloc(p->parts, (size_t)p->cap * sizeof(char *));
    }
    char *c = (char *)malloc(len + 1);
    memcpy(c, s, len); c[len] = '\0';
    p->parts[p->n++] = c;
}

/* Parse a POSIX path string into components (PurePosixPath semantics). */
static void pf_path_parse(pf_path_t *p, const char *s) {
    pf_path_init(p);
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
        else if (len > 0) pf_path_push(p, s + i, len);
        i = j;
        while (i < n && s[i] == '/') i++;
    }
}

/* as_posix() string. malloc'd. */
static char *pf_path_str(const pf_path_t *p) {
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

static void pf_path_copy(pf_path_t *dst, const pf_path_t *src) {
    pf_path_init(dst);
    free(dst->root);
    dst->root = strdup(src->root);
    for (int i = 0; i < src->n; i++) pf_path_push(dst, src->parts[i], strlen(src->parts[i]));
}

/* parent: drop the last component (root/'.' unchanged when n==0). */
static void pf_path_parent(pf_path_t *dst, const pf_path_t *src) {
    pf_path_copy(dst, src);
    if (dst->n > 0) { free(dst->parts[dst->n - 1]); dst->n--; }
}

/* name: last component, or "" when n==0. malloc'd. */
static char *pf_path_name(const pf_path_t *p) {
    if (p->n == 0) return strdup("");
    return strdup(p->parts[p->n - 1]);
}

/* join: append b's components (b absolute resets). malloc'd string. */
static char *pf_posix_join(const char *a, const char *b) {
    pf_path_t pa, pb, out;
    pf_path_parse(&pa, a);
    pf_path_parse(&pb, b);
    if (pb.root[0] != '\0') {
        pf_path_copy(&out, &pb);
    } else {
        pf_path_copy(&out, &pa);
        for (int i = 0; i < pb.n; i++) pf_path_push(&out, pb.parts[i], strlen(pb.parts[i]));
    }
    char *s = pf_path_str(&out);
    pf_path_free(&pa); pf_path_free(&pb); pf_path_free(&out);
    return s;
}

/* os.path.basename (POSIX): text after the last '/'; '' for a trailing '/'. */
static char *pf_posix_basename(const char *s) {
    if (!s) return strdup("");
    const char *slash = strrchr(s, '/');
    if (!slash) return strdup(s);
    return strdup(slash + 1);
}

/* ── regex predicates (single-line ASCII domain) ── */

/* PROFILE_ID_RE: ^[a-z0-9][a-z0-9_-]{0,63}$ (full match). */
static int pf_profile_id_match(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 1 || n > 64) return 0;
    unsigned char c0 = (unsigned char)s[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9'))) return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* _PYTHON_INTERPRETER_RE: ^python[\d.]*w?(\.exe)?$ (full match). */
static int pf_python_interp_match(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    if (n < 6 || strncmp(s, "python", 6) != 0) return 0;
    size_t i = 6;
    while (i < n && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.')) i++;
    if (i < n && s[i] == 'w') i++;
    if (i == n) return 1;
    if (n - i == 4 && strcmp(s + i, ".exe") == 0) return 1;
    return 0;
}

static int pf_all_digits(const char *s, size_t n) {
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* `.` excludes newline; a trailing .* / .+ run must be newline-free. */
static int pf_no_newline(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) if (s[i] == '\n') return 0;
    return 1;
}

/* _STORE_COPY_RE.fullmatch — four alternatives (see port header). */
static int pf_store_copy_fullmatch(const char *s) {
    if (!s) return 0;
    size_t n = strlen(s);
    /* A: auth\.json\..+ */
    if (n > 10 && strncmp(s, "auth.json.", 10) == 0 && pf_no_newline(s + 10, n - 10)) return 1;
    /* B: \.env\.bak.* */
    if (n >= 8 && strncmp(s, ".env.bak", 8) == 0 && pf_no_newline(s + 8, n - 8)) return 1;
    /* C: gateway-connections?(?:-tokens)?\.json\..+ */
    if (n > 18 && strncmp(s, "gateway-connection", 18) == 0) {
        size_t i = 18;
        if (i < n && s[i] == 's') i++;
        if (n - i >= 7 && strncmp(s + i, "-tokens", 7) == 0) i += 7;
        if (n - i > 6 && strncmp(s + i, ".json.", 6) == 0) {
            size_t rest = i + 6;
            if (n > rest && pf_no_newline(s + rest, n - rest)) return 1;
        }
    }
    /* D: config\.yaml\.(?:bak-\d{8}t\d{6}z(?:\.\d+)?|bak\.\d+|corrupt\..*|bak-pre-migrate-.*) */
    if (n > 12 && strncmp(s, "config.yaml.", 12) == 0) {
        const char *r = s + 12;
        size_t rn = n - 12;
        /* D1: bak-\d{8}t\d{6}z(?:\.\d+)? */
        if (rn >= 4 + 8 + 1 + 6 + 1 && strncmp(r, "bak-", 4) == 0) {
            const char *d8 = r + 4;
            if (pf_all_digits(d8, 8) && d8[8] == 't' && pf_all_digits(d8 + 9, 6) && d8[15] == 'z') {
                size_t used = 16;
                if (rn == used) return 1;
                if (rn > used && r[used] == '.' && pf_all_digits(r + used + 1, rn - used - 1)) return 1;
            }
        }
        /* D2: bak\.\d+ */
        if (rn > 4 && strncmp(r, "bak.", 4) == 0 && pf_all_digits(r + 4, rn - 4)) return 1;
        /* D3: corrupt\..* */
        if (rn >= 8 && strncmp(r, "corrupt.", 8) == 0 && pf_no_newline(r + 8, rn - 8)) return 1;
        /* D4: bak-pre-migrate-.* */
        if (rn >= 16 && strncmp(r, "bak-pre-migrate-", 16) == 0 && pf_no_newline(r + 16, rn - 16)) return 1;
    }
    return 0;
}
/* ── credential-store tables (transcribed from PROFILE_CREDENTIAL_PATHS /
 * _EXPORT_CREDENTIAL_FILES, case-folded path components) ── */
typedef struct { const char *parts[8]; int n; } pf_cred_t;
static const pf_cred_t PF_CRED_STORES[] = {
    {{".anthropic_oauth.json"}, 1},
    {{".aws"}, 1},
    {{".azure"}, 1},
    {{".config", "gcloud"}, 2},
    {{".config", "gh"}, 2},
    {{".copilot_jwt.json"}, 1},
    {{".docker"}, 1},
    {{".env"}, 1},
    {{".envrc"}, 1},
    {{".gnupg"}, 1},
    {{".kube"}, 1},
    {{".op.env"}, 1},
    {{".ssh"}, 1},
    {{"auth", "google_oauth.json"}, 2},
    {{"auth.json"}, 1},
    {{"backups"}, 1},
    {{"bot-desktop"}, 1},
    {{"browser-profile"}, 1},
    {{"browser-profiles"}, 1},
    {{"browser_auth"}, 1},
    {{"browser_profiles"}, 1},
    {{"bws_cache.enc.json"}, 1},
    {{"bws_cache.json"}, 1},
    {{"cache", "bws_cache.enc.json"}, 2},
    {{"cache", "bws_cache.json"}, 2},
    {{"cache", "op_cache.json"}, 2},
    {{"chrome-debug"}, 1},
    {{"feishu_comment_pairing.json"}, 1},
    {{"gateway-connection-tokens.json"}, 1},
    {{"gateway-connections.json"}, 1},
    {{"google_chat_user_client_secret.json"}, 1},
    {{"google_chat_user_oauth_pending"}, 1},
    {{"google_chat_user_oauth_pending.json"}, 1},
    {{"google_chat_user_token.json"}, 1},
    {{"google_chat_user_tokens"}, 1},
    {{"google_client_secret.json"}, 1},
    {{"google_oauth_pending.json"}, 1},
    {{"google_token.json"}, 1},
    {{"home"}, 1},
    {{"honcho.json"}, 1},
    {{"matrix", "store"}, 2},
    {{"mcp-tokens"}, 1},
    {{"mem0.json"}, 1},
    {{"npmrc"}, 1},
    {{"op_cache.json"}, 1},
    {{"pairing"}, 1},
    {{"photon-sidecar.json"}, 1},
    {{"platforms", "matrix", "store"}, 3},
    {{"platforms", "pairing"}, 2},
    {{"platforms", "whatsapp", "session"}, 3},
    {{"proxy"}, 1},
    {{"runtime", "photon-sidecar.json"}, 2},
    {{"slack_tokens.json"}, 1},
    {{"state-snapshots"}, 1},
    {{"teams_pipeline_store.json"}, 1},
    {{"vault"}, 1},
    {{"webapp"}, 1},
    {{"webhook_subscriptions.json"}, 1},
    {{"weixin", "accounts"}, 2},
    {{"whatsapp", "session"}, 2},
    {{"workspace", "meetings", "node_token.json"}, 3},
};
#define PF_N_CRED ((int)(sizeof PF_CRED_STORES / sizeof PF_CRED_STORES[0]))
static const pf_cred_t PF_EXPORT_STORES[] = {
    {{".aws"}, 1},
    {{".azure"}, 1},
    {{".config", "gcloud"}, 2},
    {{".config", "gh"}, 2},
    {{".docker"}, 1},
    {{".env"}, 1},
    {{".envrc"}, 1},
    {{".gnupg"}, 1},
    {{".kube"}, 1},
    {{".ssh"}, 1},
    {{"auth.json"}, 1},
    {{"bot-desktop"}, 1},
};
#define PF_N_EXPORT ((int)(sizeof PF_EXPORT_STORES / sizeof PF_EXPORT_STORES[0]))

static const char *const PF_HERMES_ARGV_MARKERS[] = {"hermes_cli.main", "hermes-gateway", "tui_gateway"};
static const char *const PF_CONSOLE_SCRIPT_NAMES[] = {"hermes", "hermes-agent", "hermes-acp"};
static const char PF_PROFILE_NAME_RULE[] =
    "Use lowercase letters, numbers, '-' or '_', starting with a letter or number, up to 64 characters";

/* ── pure leaves ── */

/* PoP: pf_fold @ hermes_cli/profiles.py:_fold */
static json_t *pf_fold(const json_t *parts) {
    json_t *out = json_array();
    if (parts && parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(parts); i++) {
            const json_t *p = json_get(parts, i);
            char *f = pf_casefold(pf_jstr(p));
            json_append(out, json_string(f ? f : ""));
            free(f);
        }
    }
    return out;
}

/* Fold a parts array into a malloc'd char* vector (caller frees each + vec). */
static char **pf_fold_vec(const json_t *parts, int *n) {
    *n = 0;
    if (!parts || parts->type != JSON_ARRAY) return NULL;
    int cnt = (int)json_len(parts);
    char **v = (char **)calloc((size_t)(cnt > 0 ? cnt : 1), sizeof(char *));
    for (int i = 0; i < cnt; i++) v[i] = pf_casefold(pf_jstr(json_get(parts, (size_t)i)));
    *n = cnt;
    return v;
}

static void pf_free_vec(char **v, int n) {
    if (!v) return;
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
}

/* PoP: pf_profile_path_is_private @ hermes_cli/profiles.py:profile_path_is_private */
static json_t *pf_profile_path_is_private(const json_t *parts) {
    int nf;
    char **f = pf_fold_vec(parts, &nf);
    int hit = 0;
    if (nf == 1 && pf_store_copy_fullmatch(f[0])) hit = 1;
    if (!hit) {
        for (int s = 0; s < PF_N_CRED && !hit; s++) {
            const pf_cred_t *st = &PF_CRED_STORES[s];
            if (st->n <= nf) {
                int eq = 1;
                for (int k = 0; k < st->n; k++)
                    if (strcmp(f[k], st->parts[k]) != 0) { eq = 0; break; }
                if (eq) hit = 1;
            }
        }
    }
    pf_free_vec(f, nf);
    return json_bool(hit ? true : false);
}

/* PoP: pf_profile_path_contains_private_store @ hermes_cli/profiles.py:profile_path_contains_private_store */
static json_t *pf_profile_path_contains_private_store(const json_t *parts) {
    int nf;
    char **f = pf_fold_vec(parts, &nf);
    int hit = 0;
    for (int s = 0; s < PF_N_CRED && !hit; s++) {
        const pf_cred_t *st = &PF_CRED_STORES[s];
        if (st->n > nf) {
            int eq = 1;
            for (int k = 0; k < nf; k++)
                if (strcmp(f[k], st->parts[k]) != 0) { eq = 0; break; }
            if (eq) hit = 1;
        }
    }
    pf_free_vec(f, nf);
    return json_bool(hit ? true : false);
}

/* _suggest_profile_name core: malloc'd candidate (or "my-work"). */
static char *pf_suggest_str(const char *name) {
    char *stripped = pf_strip_ws(name ? name : "");
    char *low = pf_lower_ascii(stripped ? stripped : "");
    free(stripped);
    /* re.sub(r"[^a-z0-9_-]+", "-", low): collapse each disallowed run to '-'. */
    size_t n = strlen(low);
    char *sub = (char *)malloc(n + 1);
    size_t o = 0;
    int in_run = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)low[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (ok) { sub[o++] = (char)c; in_run = 0; }
        else if (!in_run) { sub[o++] = '-'; in_run = 1; }
    }
    sub[o] = '\0';
    free(low);
    char *trimmed = pf_strip_chars(sub, "-_");
    free(sub);
    /* [:64] codepoint truncation (ASCII domain: bytes == codepoints). */
    if (strlen(trimmed) > 64) trimmed[64] = '\0';
    char *result;
    if (pf_profile_id_match(trimmed)) result = strdup(trimmed);
    else result = strdup("my-work");
    free(trimmed);
    return result;
}

/* PoP: pf_suggest_profile_name @ hermes_cli/profiles.py:_suggest_profile_name */
static json_t *pf_suggest_profile_name(const char *name) {
    char *s = pf_suggest_str(name);
    json_t *r = json_string(s ? s : "my-work");
    free(s);
    return r;
}

/* PoP: pf_missing_profile_error @ hermes_cli/profiles.py:_missing_profile_error */
static json_t *pf_missing_profile_error(const char *canon) {
    return pf_raise_fmt("FileNotFoundError",
                        "Profile '%s' does not exist. Create it with: hermes profile create %s",
                        canon ? canon : "", canon ? canon : "");
}

/* PoP: pf_unknown_profile_error @ hermes_cli/profiles.py:_unknown_profile_error */
static json_t *pf_unknown_profile_error(const char *canon) {
    return pf_raise_fmt("FileNotFoundError",
                        "No profile named '%s'. See your profiles with: hermes profile list",
                        canon ? canon : "");
}

/* PoP: pf_profile_exists_error @ hermes_cli/profiles.py:_profile_exists_error */
static json_t *pf_profile_exists_error(const char *canon) {
    return pf_raise_fmt("FileExistsError",
                        "A profile named '%s' already exists. Switch to it with `hermes profile use %s`, "
                        "see all profiles with `hermes profile list`, or choose a different name.",
                        canon ? canon : "", canon ? canon : "");
}

/* PoP: pf_invalid_profile_name_error @ hermes_cli/profiles.py:_invalid_profile_name_error */
static json_t *pf_invalid_profile_name_error(const char *name) {
    char *sugg = pf_suggest_str(name);
    char *rep = pf_repr_str(name ? name : "");
    json_t *r = pf_raise_fmt("ValueError",
                             "%s is not a valid profile name. %s (for example: %s). "
                             "Then run `hermes profile create %s`.",
                             rep ? rep : "''", PF_PROFILE_NAME_RULE,
                             sugg ? sugg : "my-work", sugg ? sugg : "my-work");
    free(sugg);
    free(rep);
    return r;
}

/* PoP: pf_clean_previous_names @ hermes_cli/profiles.py:_clean_previous_names */
static json_t *pf_clean_previous_names(const json_t *raw) {
    json_t *out = json_array();
    if (!raw || raw->type != JSON_ARRAY) return out;
    char **seen = NULL;
    int nseen = 0, capseen = 0;
    for (size_t i = 0; i < json_len(raw); i++) {
        const json_t *item = json_get(raw, i);
        char *s = pf_truthy(item) ? pf_py_str(item) : strdup("");
        char *name = pf_strip_ws(s ? s : "");
        free(s);
        if (name && name[0]) {
            int dup = 0;
            for (int k = 0; k < nseen; k++) if (strcmp(seen[k], name) == 0) { dup = 1; break; }
            if (!dup) {
                if (nseen == capseen) {
                    capseen = capseen ? capseen * 2 : 8;
                    seen = (char **)realloc(seen, (size_t)capseen * sizeof(char *));
                }
                seen[nseen++] = strdup(name);
                json_append(out, json_string(name));
            }
        }
        free(name);
    }
    pf_free_vec(seen, nseen);
    return out;
}

/* PoP: pf_is_hermes_argv @ hermes_cli/profiles.py:_is_hermes_argv */
static json_t *pf_is_hermes_argv(const json_t *argv) {
    if (!argv || argv->type != JSON_ARRAY || json_len(argv) == 0)
        return pf_raise("IndexError", "list index out of range");
    size_t na = json_len(argv);
    /* joined = " ".join(argv) */
    size_t total = 0;
    for (size_t i = 0; i < na; i++) total += strlen(pf_jstr(json_get(argv, i))) + 1;
    char *joined = (char *)malloc(total + 1);
    size_t o = 0;
    for (size_t i = 0; i < na; i++) {
        if (i) joined[o++] = ' ';
        const char *t = pf_jstr(json_get(argv, i));
        size_t tl = strlen(t);
        memcpy(joined + o, t, tl); o += tl;
    }
    joined[o] = '\0';
    char *base = pf_posix_basename(pf_jstr(json_get(argv, 0)));
    char *exe = pf_lower_ascii(base ? base : "");
    free(base);
    int hit = 0;
    for (size_t m = 0; m < sizeof PF_HERMES_ARGV_MARKERS / sizeof PF_HERMES_ARGV_MARKERS[0]; m++)
        if (strstr(joined, PF_HERMES_ARGV_MARKERS[m])) { hit = 1; break; }
    if (!hit && strncmp(exe, "hermes", 6) == 0) hit = 1;
    free(joined);
    if (hit) { free(exe); return json_bool(true); }
    if (na >= 2 && pf_python_interp_match(exe)) {
        char *sb = pf_posix_basename(pf_jstr(json_get(argv, 1)));
        char *sl = pf_lower_ascii(sb ? sb : "");
        free(sb);
        /* rsplit(".", 1)[0] */
        char *dot = strrchr(sl, '.');
        if (dot) *dot = '\0';
        int found = 0;
        for (size_t c = 0; c < sizeof PF_CONSOLE_SCRIPT_NAMES / sizeof PF_CONSOLE_SCRIPT_NAMES[0]; c++)
            if (strcmp(sl, PF_CONSOLE_SCRIPT_NAMES[c]) == 0) { found = 1; break; }
        free(sl);
        free(exe);
        return json_bool(found ? true : false);
    }
    free(exe);
    return json_bool(false);
}

/* PoP: pf_argv_profile_selectors @ hermes_cli/profiles.py:_argv_profile_selectors */
static json_t *pf_argv_profile_selectors(const json_t *argv) {
    json_t *out = json_array();
    if (!argv || argv->type != JSON_ARRAY) return out;
    size_t na = json_len(argv);
    for (size_t i = 0; i < na; i++) {
        const char *tok = pf_jstr(json_get(argv, i));
        if ((strcmp(tok, "--profile") == 0 || strcmp(tok, "-p") == 0) && i + 1 < na) {
            json_append(out, json_string(pf_jstr(json_get(argv, i + 1))));
        } else if (strncmp(tok, "--profile=", 10) == 0) {
            json_append(out, json_string(tok + 10));
        }
    }
    return out;
}

/* PoP: pf_profile_root_for_env_home @ hermes_cli/profiles.py:profile_root_for_env_home */
static json_t *pf_profile_root_for_env_home(const char *env_home, const char *default_root) {
    char *stripped = pf_strip_ws(env_home ? env_home : "");
    if (!stripped || !stripped[0]) {
        free(stripped);
        return json_string(default_root ? default_root : "");
    }
    pf_path_t p, par, namep;
    pf_path_parse(&p, stripped);
    pf_path_parent(&par, &p);
    char *nm = pf_path_name(&par);
    char *res;
    if (nm && strcmp(nm, "profiles") == 0) {
        pf_path_parent(&namep, &par);
        res = pf_path_str(&namep);
        pf_path_free(&namep);
    } else {
        res = pf_path_str(&p);
    }
    free(nm);
    pf_path_free(&p);
    pf_path_free(&par);
    free(stripped);
    json_t *r = json_string(res ? res : "");
    free(res);
    return r;
}

/* PoP: pf_parked_marker_path @ hermes_cli/profiles.py:parked_marker_path */
static json_t *pf_parked_marker_path(const char *home) {
    char *j = pf_posix_join(home ? home : "", "gateway.parked");
    json_t *r = json_string(j ? j : "");
    free(j);
    return r;
}

/* Full Path.parts (root component first for absolute paths). */
static char **pf_full_parts(const char *directory, int *n) {
    pf_path_t p;
    pf_path_parse(&p, directory);
    int cnt = p.n + (p.root[0] ? 1 : 0);
    char **v = (char **)calloc((size_t)(cnt > 0 ? cnt : 1), sizeof(char *));
    int idx = 0;
    if (p.root[0]) v[idx++] = strdup(p.root);
    for (int i = 0; i < p.n; i++) v[idx++] = strdup(p.parts[i]);
    *n = cnt;
    pf_path_free(&p);
    return v;
}

/* PoP: pf_export_credential_entries @ hermes_cli/profiles.py:_export_credential_entries */
static json_t *pf_export_credential_entries(const char *directory, const json_t *contents) {
    int np;
    char **parts = pf_full_parts(directory ? directory : "", &np);
    char **hits = NULL;
    int nhits = 0, caphits = 0;
    if (contents && contents->type == JSON_ARRAY) {
        for (size_t e = 0; e < json_len(contents); e++) {
            const char *entry = pf_jstr(json_get(contents, e));
            int matched = 0;
            for (int s = 0; s < PF_N_EXPORT && !matched; s++) {
                const pf_cred_t *st = &PF_EXPORT_STORES[s];
                if (strcmp(entry, st->parts[st->n - 1]) != 0) continue;
                int need = st->n - 1;
                if (np < need) continue;
                int eq = 1;
                for (int k = 0; k < need; k++)
                    if (strcmp(parts[np - need + k], st->parts[k]) != 0) { eq = 0; break; }
                if (eq) matched = 1;
            }
            if (matched) {
                int dup = 0;
                for (int k = 0; k < nhits; k++) if (strcmp(hits[k], entry) == 0) { dup = 1; break; }
                if (!dup) {
                    if (nhits == caphits) {
                        caphits = caphits ? caphits * 2 : 8;
                        hits = (char **)realloc(hits, (size_t)caphits * sizeof(char *));
                    }
                    hits[nhits++] = strdup(entry);
                }
            }
        }
    }
    /* set -> sorted array */
    for (int i = 0; i < nhits; i++)
        for (int j = i + 1; j < nhits; j++)
            if (strcmp(hits[j], hits[i]) < 0) { char *t = hits[i]; hits[i] = hits[j]; hits[j] = t; }
    json_t *out = json_array();
    for (int i = 0; i < nhits; i++) json_append(out, json_string(hits[i]));
    pf_free_vec(hits, nhits);
    pf_free_vec(parts, np);
    return out;
}
/* ── seam argument builders ── */

static json_t *pf_path_args(const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    return a;
}

static json_t *pf_name_args(const char *name) {
    json_t *a = json_object();
    json_set(a, "name", json_string(name ? name : ""));
    return a;
}

static json_t *pf_canon_args(const char *canon) {
    json_t *a = json_object();
    json_set(a, "canon", json_string(canon ? canon : ""));
    return a;
}

/* ── seam flows ── */

/* PoP: pf_standalone_truthy @ hermes_cli/profiles.py:_standalone_truthy */
static json_t *pf_standalone_truthy(pf_io_t *io, const json_t *value) {
    if (value && value->type == JSON_STRING) {
        json_t *a = json_object();
        json_set(a, "value", json_copy(value));
        json_t *r = pf_call(io, "_bool_token", a);
        if (pf_is_raise(r)) return r;
        int is_true = r && r->type == JSON_BOOL && r->bool_val;
        json_free(r);
        return json_bool(is_true ? true : false);
    }
    return json_bool(pf_truthy(value) ? true : false);
}

/* PoP: pf_canon_valid @ hermes_cli/profiles.py:_canon_valid */
static json_t *pf_canon_valid(pf_io_t *io, const char *name) {
    json_t *canon = pf_call(io, "normalize_profile_name", pf_name_args(name));
    if (pf_is_raise(canon)) return canon;
    json_t *v = pf_call(io, "validate_profile_name", pf_name_args(pf_jstr(canon)));
    if (pf_is_raise(v)) { json_free(canon); return v; }
    json_free(v);
    return canon;
}

/* PoP: pf_existing_profile_dir @ hermes_cli/profiles.py:_existing_profile_dir */
static json_t *pf_existing_profile_dir(pf_io_t *io, const char *name) {
    json_t *canon = pf_canon_valid(io, name);
    if (pf_is_raise(canon)) return canon;
    const char *cs = pf_jstr(canon);
    json_t *pd = pf_call(io, "get_profile_dir", pf_name_args(cs));
    if (pf_is_raise(pd)) { json_free(canon); return pd; }
    json_t *d = pf_call(io, "is_dir", pf_path_args(pf_jstr(pd)));
    if (pf_is_raise(d)) { json_free(canon); json_free(pd); return d; }
    int isdir = pf_truthy(d);
    json_free(d);
    if (!isdir) {
        json_t *err = pf_unknown_profile_error(cs);
        json_free(canon);
        json_free(pd);
        return err;
    }
    json_t *ret = json_array();
    json_append(ret, json_string(cs));
    json_append(ret, json_string(pf_jstr(pd)));
    json_free(canon);
    json_free(pd);
    return ret;
}

/* PoP: pf_wrapper_path @ hermes_cli/profiles.py:_wrapper_path */
static json_t *pf_wrapper_path(pf_io_t *io, const char *alias) {
    json_t *wd = pf_call(io, "_get_wrapper_dir", json_object());
    if (pf_is_raise(wd)) return wd;
    int win32 = pf_env_bool(io, "win32");
    char *nm = win32 ? pf_fmt("%s.bat", alias ? alias : "") : strdup(alias ? alias : "");
    char *j = pf_posix_join(pf_jstr(wd), nm ? nm : "");
    free(nm);
    json_free(wd);
    json_t *r = json_string(j ? j : "");
    free(j);
    return r;
}

/* PoP: pf_is_our_wrapper @ hermes_cli/profiles.py:_is_our_wrapper */
static json_t *pf_is_our_wrapper(pf_io_t *io, const char *path) {
    json_t *r = pf_call(io, "wrapper_read", pf_path_args(path));
    if (pf_is_raise(r)) {
        if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); return json_bool(false); }
        return r;
    }
    int hit = strstr(pf_jstr(r), "hermes -p") != NULL;
    json_free(r);
    return json_bool(hit ? true : false);
}

/* PoP: pf_find_our_wrapper @ hermes_cli/profiles.py:_find_our_wrapper */
static json_t *pf_find_our_wrapper(pf_io_t *io, const char *alias) {
    int win32 = pf_env_bool(io, "win32");
    /* candidates built FIRST (each fires _get_wrapper_dir). */
    json_t *c0 = pf_wrapper_path(io, alias);
    if (pf_is_raise(c0)) return c0;
    char *c0s = strdup(pf_jstr(c0));
    json_free(c0);
    char *c1s = NULL;
    if (win32) {
        json_t *wd = pf_call(io, "_get_wrapper_dir", json_object());
        if (pf_is_raise(wd)) { free(c0s); return wd; }
        c1s = pf_posix_join(pf_jstr(wd), alias ? alias : "");
        json_free(wd);
    }
    json_t *ret = NULL;
    const char *cands[2] = {c0s, c1s};
    int nc = win32 ? 2 : 1;
    for (int i = 0; i < nc; i++) {
        json_t *r = pf_is_our_wrapper(io, cands[i]);
        if (pf_is_raise(r)) { ret = r; goto done; }
        int ours = pf_truthy(r);
        json_free(r);
        if (ours) { ret = json_string(cands[i]); goto done; }
    }
    ret = NULL;
done:
    free(c0s);
    free(c1s);
    return ret;
}

/* stat mode predicates (S_ISREG / S_ISDIR / S_ISLNK). */
static int pf_mode_special_ok(long mode) {
    long fmt = mode & 0170000;
    return fmt == 0100000 || fmt == 0040000 || fmt == 0120000;
}

static int pf_endswith_any(const char *s, const char *const *suf, int n) {
    size_t ls = strlen(s);
    for (int i = 0; i < n; i++) {
        size_t lf = strlen(suf[i]);
        if (ls >= lf && strcmp(s + ls - lf, suf[i]) == 0) return 1;
    }
    return 0;
}

/* PoP: pf_non_exportable_entries @ hermes_cli/profiles.py:_non_exportable_entries */
static json_t *pf_non_exportable_entries(pf_io_t *io, const char *directory, const json_t *contents) {
    static const char *const suf[] = {".sock", ".tmp", ".pyc", ".pyo"};
    char **ign = NULL;
    int nign = 0, capign = 0;
    json_t *ret = NULL;
    if (contents && contents->type == JSON_ARRAY) {
        for (size_t e = 0; e < json_len(contents); e++) {
            const char *entry = pf_jstr(json_get(contents, e));
            int add = 0;
            if (strcmp(entry, "__pycache__") == 0 || pf_endswith_any(entry, suf, 4)) {
                add = 1;
            } else {
                char *j = pf_posix_join(directory ? directory : "", entry);
                json_t *r = pf_call(io, "lstat", pf_path_args(j));
                free(j);
                if (pf_is_raise(r)) {
                    if (pf_is_oserror(pf_raise_kind(r))) { json_free(r); add = 1; }
                    else { ret = r; goto done; }
                } else if (!r || r->type != JSON_OBJECT) {
                    /* .st_mode on a non-stat result -> AttributeError (propagates) */
                    ret = pf_raise_fmt("AttributeError", "'%s' object has no attribute 'st_mode'",
                                       pf_type_name(r));
                    json_free(r);
                    goto done;
                } else {
                    long mode = (long)json_get_num(r, "st_mode", 0.0);
                    json_free(r);
                    if (!pf_mode_special_ok(mode)) add = 1;
                }
            }
            if (add) {
                int dup = 0;
                for (int k = 0; k < nign; k++) if (strcmp(ign[k], entry) == 0) { dup = 1; break; }
                if (!dup) {
                    if (nign == capign) {
                        capign = capign ? capign * 2 : 8;
                        ign = (char **)realloc(ign, (size_t)capign * sizeof(char *));
                    }
                    ign[nign++] = strdup(entry);
                }
            }
        }
    }
    for (int i = 0; i < nign; i++)
        for (int j = i + 1; j < nign; j++)
            if (strcmp(ign[j], ign[i]) < 0) { char *t = ign[i]; ign[i] = ign[j]; ign[j] = t; }
    ret = json_array();
    for (int i = 0; i < nign; i++) json_append(ret, json_string(ign[i]));
done:
    pf_free_vec(ign, nign);
    return ret;
}

/* PoP: pf_notify_multiplexer @ hermes_cli/profiles.py:_notify_multiplexer */
static json_t *pf_notify_multiplexer(pf_io_t *io, const char *canon) {
    json_t *r = pf_call(io, "notify_multiplexer_profiles_changed", pf_canon_args(canon));
    if (pf_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: pf_purge_identity @ hermes_cli/profiles.py:_purge_identity */
static json_t *pf_purge_identity(pf_io_t *io, const char *canon) {
    json_t *r = pf_call(io, "purge_profile_identity", pf_canon_args(canon));
    if (pf_is_raise(r)) return r;
    return r;
}

/* PoP: pf_live_default_multiplexer @ hermes_cli/profiles.py:_live_default_multiplexer */
static json_t *pf_live_default_multiplexer(pf_io_t *io) {
    json_t *r = pf_call(io, "recorded_served_profiles", json_object());
    if (pf_is_raise(r)) {
        if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); return json_bool(false); }
        return r;
    }
    int present = pf_present(r);
    json_free(r);
    return json_bool(present ? true : false);
}

/* PoP: pf_served_by_running_multiplexer @ hermes_cli/profiles.py:_served_by_running_multiplexer */
static json_t *pf_served_by_running_multiplexer(pf_io_t *io, const char *profile_name) {
    json_t *a = json_object();
    json_set(a, "profile_name", json_string(profile_name ? profile_name : ""));
    json_t *r = pf_call(io, "named_profile_served_by_running_multiplexer", a);
    if (pf_is_raise(r)) {
        if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); return json_bool(false); }
        return r;
    }
    return r;
}

/* PoP: pf_retarget_active_profile @ hermes_cli/profiles.py:_retarget_active_profile */
static json_t *pf_retarget_active_profile(pf_io_t *io, const char *old, const char *new,
                                          const char *message) {
    /* with contextlib.suppress(Exception): whole block. */
    json_t *r = pf_call(io, "get_active_profile", json_object());
    if (pf_is_raise(r)) {
        if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); return NULL; }
        return r;
    }
    int eq = strcmp(pf_jstr(r), old ? old : "") == 0;
    json_free(r);
    if (eq) {
        json_t *s = pf_call(io, "set_active_profile", pf_name_args(new));
        if (pf_is_raise(s)) {
            if (pf_caught_by_exception(pf_raise_kind(s))) { json_free(s); return NULL; }
            return s;
        }
        json_free(s);
        json_t *pa = json_object();
        json_t *vals = json_array();
        json_append(vals, json_string(message ? message : ""));
        json_set(pa, "args", vals);
        json_t *p = pf_call(io, "print", pa);
        if (pf_is_raise(p)) {
            if (pf_caught_by_exception(pf_raise_kind(p))) { json_free(p); return NULL; }
            return p;
        }
        json_free(p);
    }
    return NULL;
}

/* PoP: pf_current_profile_name @ hermes_cli/profiles.py:current_profile_name */
static json_t *pf_current_profile_name(pf_io_t *io, const json_t *default_val,
                                       const json_t *envd) {
    /* get_hermes_home_override() is OUTSIDE the try: a raise propagates. */
    json_t *ov = pf_call(io, "get_hermes_home_override", json_object());
    if (pf_is_raise(ov)) return ov;
    int has_override = pf_present(ov);
    json_free(ov);
    if (!has_override) {
        static const char *const env_names[] = {"HERMES_PROFILE_NAME", "HERMES_PROFILE"};
        for (int i = 0; i < 2; i++) {
            const json_t *v = envd ? json_obj_get(envd, env_names[i]) : NULL;
            char *s = pf_truthy(v) ? pf_py_str(v) : strdup("");
            char *stripped = pf_strip_ws(s ? s : "");
            free(s);
            if (stripped && stripped[0]) {
                json_t *r = json_string(stripped);
                free(stripped);
                return r;
            }
            free(stripped);
        }
    }
    /* try: return get_active_profile_name() or default */
    json_t *r = pf_call(io, "get_active_profile_name", json_object());
    if (pf_is_raise(r)) {
        if (pf_caught_by_exception(pf_raise_kind(r))) {
            json_free(r);
            return default_val ? json_copy(default_val) : json_null();
        }
        return r;
    }
    if (pf_truthy(r)) return r;
    json_free(r);
    return default_val ? json_copy(default_val) : json_null();
}

/* exists() probe of candidate/.git — NULL continue, bool(true) found, raise propagate. */
static json_t *pf_git_exists(pf_io_t *io, const char *cand) {
    char *g = pf_posix_join(cand, ".git");
    json_t *e = pf_call(io, "exists", pf_path_args(g));
    free(g);
    if (pf_is_raise(e)) return e;
    int t = pf_truthy(e);
    json_free(e);
    return t ? json_bool(true) : NULL;
}

/* PoP: pf_inside_git_checkout @ hermes_cli/profiles.py:_inside_git_checkout */
static json_t *pf_inside_git_checkout(pf_io_t *io, const char *path) {
    json_t *r = pf_call(io, "resolve", pf_path_args(path));
    if (pf_is_raise(r)) {
        const char *k = pf_raise_kind(r);
        if (pf_is_oserror(k) || pf_is_runtimeerror(k)) { json_free(r); return json_bool(true); }
        return r;
    }
    char *resolved = strdup(pf_jstr(r));
    json_free(r);
    pf_path_t p, cur;
    pf_path_parse(&p, resolved);
    pf_path_init(&cur);
    json_t *ret = NULL;
    json_t *c = pf_git_exists(io, resolved);
    if (c) { ret = c; goto done; }
    pf_path_copy(&cur, &p);
    while (cur.n > 0) {
        free(cur.parts[cur.n - 1]);
        cur.n--;
        char *cs = pf_path_str(&cur);
        c = pf_git_exists(io, cs);
        free(cs);
        if (c) { ret = c; goto done; }
    }
    ret = json_bool(false);
done:
    free(resolved);
    pf_path_free(&p);
    pf_path_free(&cur);
    return ret;
}

/* PoP: pf_s6_runtime_manager @ hermes_cli/profiles.py:_s6_runtime_manager */
static json_t *pf_s6_runtime_manager(pf_io_t *io) {
    /* try: detect + get_service_manager */
    json_t *d = pf_call(io, "detect_service_manager", json_object());
    if (pf_is_raise(d)) {
        if (pf_caught_by_exception(pf_raise_kind(d))) { json_free(d); return NULL; }
        return d;
    }
    int is_s6 = d && d->type == JSON_STRING && strcmp(d->str_val, "s6") == 0;
    json_free(d);
    if (!is_s6) return NULL;
    json_t *m = pf_call(io, "get_service_manager", json_object());
    if (pf_is_raise(m)) {
        if (pf_caught_by_exception(pf_raise_kind(m))) { json_free(m); return NULL; }
        return m;
    }
    /* OUTSIDE try: mgr.supports_runtime_registration() */
    if (!pf_present(m)) {
        json_free(m);
        return pf_raise("AttributeError",
                        "'NoneType' object has no attribute 'supports_runtime_registration'");
    }
    json_t *s = pf_call(io, "supports_runtime_registration", json_object());
    if (pf_is_raise(s)) { json_free(m); return s; }
    int sup = pf_truthy(s);
    json_free(s);
    if (sup) return m;
    json_free(m);
    return NULL;
}

/* PoP: pf_refuse_rename_without_file_tooling @ hermes_cli/profiles.py:_refuse_rename_without_file_tooling */
static json_t *pf_refuse_rename_without_file_tooling(pf_io_t *io, const char *old_canon) {
    json_t *b = pf_call(io, "get_config_backend", json_object());
    if (pf_is_raise(b)) return b;
    if (!pf_present(b)) {
        json_free(b);
        return pf_raise("AttributeError", "'NoneType' object has no attribute 'supports_file_tooling'");
    }
    json_t *s = pf_call(io, "supports_file_tooling", json_object());
    if (pf_is_raise(s)) { json_free(b); return s; }
    int sup = pf_truthy(s);
    json_free(s);
    if (!sup) {
        const char *nm = json_get_str(b, "name", "");
        char *rep = pf_repr_str(nm);
        json_t *err = pf_raise_fmt("ValueError",
                                   "Cannot rename profile '%s': its config lives in the %s config "
                                   "backend under that name, and renaming it there is not supported. "
                                   "Nothing was changed.",
                                   old_canon ? old_canon : "", rep ? rep : "''");
        free(rep);
        json_free(b);
        return err;
    }
    json_free(b);
    return NULL;
}

/* PoP: pf_load_yaml_dict @ hermes_cli/profiles.py:_load_yaml_dict */
static json_t *pf_load_yaml_dict(pf_io_t *io, const char *path) {
    json_t *f = pf_call(io, "is_file", pf_path_args(path));
    if (pf_is_raise(f)) return f;   /* outside try */
    int isfile = pf_truthy(f);
    json_free(f);
    if (!isfile) return NULL;
    /* try region */
    json_t *t = pf_call(io, "read_text", pf_path_args(path));
    if (pf_is_raise(t)) {
        if (pf_caught_by_exception(pf_raise_kind(t))) { json_free(t); return NULL; }
        return t;
    }
    char *text = strdup(pf_jstr(t));
    json_free(t);
    json_t *sa = json_object();
    json_set(sa, "text", json_string(text ? text : ""));
    free(text);
    json_t *sl = pf_call(io, "safe_load", sa);
    if (pf_is_raise(sl)) {
        if (pf_caught_by_exception(pf_raise_kind(sl))) { json_free(sl); return NULL; }
        return sl;
    }
    /* data = sl or {} ; return data if isinstance(data, dict) else None */
    json_t *data;
    if (pf_truthy(sl)) data = sl;
    else { json_free(sl); data = json_object(); }
    if (data->type == JSON_OBJECT) return data;
    json_free(data);
    return NULL;
}

/* PoP: pf_seed_file_if_missing @ hermes_cli/profiles.py:_seed_file_if_missing */
static json_t *pf_seed_file_if_missing(pf_io_t *io, const char *path, const char *text,
                                       const json_t *mode) {
    json_t *e = pf_call(io, "exists", pf_path_args(path));
    if (pf_is_raise(e)) return e;   /* outside suppress */
    int exists = pf_truthy(e);
    json_free(e);
    if (exists) return NULL;
    /* with suppress(OSError): write_text + optional chmod */
    json_t *wa = json_object();
    json_set(wa, "path", json_string(path ? path : ""));
    json_set(wa, "text", json_string(text ? text : ""));
    json_t *w = pf_call(io, "write_text", wa);
    if (pf_is_raise(w)) {
        if (pf_is_oserror(pf_raise_kind(w))) { json_free(w); return NULL; }
        return w;
    }
    json_free(w);
    if (pf_present(mode)) {
        json_t *ca = json_object();
        json_set(ca, "path", json_string(path ? path : ""));
        json_set(ca, "mode", json_copy(mode));
        json_t *c = pf_call(io, "chmod", ca);
        if (pf_is_raise(c)) {
            if (pf_is_oserror(pf_raise_kind(c))) { json_free(c); return NULL; }
            return c;
        }
        json_free(c);
    }
    return NULL;
}

/* PoP: pf_clone_file @ hermes_cli/profiles.py:_clone_file */
static json_t *pf_clone_file(pf_io_t *io, const char *source_dir, const char *profile_dir,
                             const char *relpath) {
    char *src = pf_posix_join(source_dir ? source_dir : "", relpath ? relpath : "");
    json_t *e = pf_call(io, "exists", pf_path_args(src));
    if (pf_is_raise(e)) { free(src); return e; }
    int exists = pf_truthy(e);
    json_free(e);
    if (!exists) { free(src); return NULL; }
    char *dst = pf_posix_join(profile_dir ? profile_dir : "", relpath ? relpath : "");
    /* dst.parent.mkdir(parents=True, exist_ok=True) */
    pf_path_t pd, par;
    pf_path_parse(&pd, dst);
    pf_path_parent(&par, &pd);
    char *pars = pf_path_str(&par);
    pf_path_free(&pd);
    pf_path_free(&par);
    json_t *m = pf_call(io, "mkdir", pf_path_args(pars));
    free(pars);
    if (pf_is_raise(m)) { free(src); free(dst); return m; }
    json_free(m);
    json_t *ca = json_object();
    json_set(ca, "src", json_string(src));
    json_set(ca, "dst", json_string(dst));
    json_t *c = pf_call(io, "copy2", ca);
    if (pf_is_raise(c)) { free(src); free(dst); return c; }
    json_free(c);
    if (relpath && strcmp(relpath, ".env") == 0) {
        /* with suppress(OSError): os.chmod(str(dst), 0o600) */
        json_t *cha = json_object();
        json_set(cha, "path", json_string(dst));
        json_set(cha, "mode", json_number(384.0));
        json_t *ch = pf_call(io, "chmod", cha);
        if (pf_is_raise(ch)) {
            if (pf_is_oserror(pf_raise_kind(ch))) { json_free(ch); }
            else { free(src); free(dst); return ch; }
        } else {
            json_free(ch);
        }
    }
    free(src);
    free(dst);
    return NULL;
}

/* PoP: pf_resolve_clone_source @ hermes_cli/profiles.py:_resolve_clone_source */
static json_t *pf_resolve_clone_source(pf_io_t *io, const json_t *clone_from) {
    json_t *rta = json_object();
    json_set(rta, "label", json_string("Profile clone"));
    json_t *rt = pf_call(io, "require_file_tooling", rta);
    if (pf_is_raise(rt)) return rt;
    json_free(rt);
    char *canon_owned = NULL;
    const char *label = "active";
    json_t *source = NULL;
    if (!pf_present(clone_from)) {
        source = pf_call(io, "get_hermes_home", json_object());
        if (pf_is_raise(source)) return source;
    } else {
        json_t *canon = pf_canon_valid(io, pf_jstr(clone_from));
        if (pf_is_raise(canon)) return canon;
        canon_owned = strdup(pf_jstr(canon));
        json_free(canon);
        label = canon_owned;
        source = pf_call(io, "get_profile_dir", pf_name_args(canon_owned));
        if (pf_is_raise(source)) { free(canon_owned); return source; }
    }
    const char *ss = pf_jstr(source);
    json_t *d = pf_call(io, "is_dir", pf_path_args(ss));
    if (pf_is_raise(d)) { free(canon_owned); json_free(source); return d; }
    int isdir = pf_truthy(d);
    json_free(d);
    if (!isdir) {
        json_t *err = pf_raise_fmt("FileNotFoundError",
                                   "Source profile '%s' does not exist at %s", label, ss);
        free(canon_owned);
        json_free(source);
        return err;
    }
    json_t *ret = json_string(ss);
    free(canon_owned);
    json_free(source);
    return ret;
}

/* PoP: pf_seed_model_config @ hermes_cli/profiles.py:_seed_model_config */
static json_t *pf_seed_model_config(pf_io_t *io, const char *profile_dir) {
    char *config_path = pf_posix_join(profile_dir ? profile_dir : "", "config.yaml");
    char *source = NULL;
    json_t *seed = NULL;
    json_t *ret = NULL;
    json_t *r;
    /* config_exists(config_path) — outside suppress */
    r = pf_call(io, "config_exists", pf_path_args(config_path));
    if (pf_is_raise(r)) { ret = r; goto done; }
    if (pf_truthy(r)) { json_free(r); ret = NULL; goto done; }
    json_free(r);
    /* with suppress(Exception): */
    r = pf_call(io, "get_hermes_home", json_object());
    if (pf_is_raise(r)) {
        if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); ret = NULL; goto done; }
        ret = r; goto done;
    }
    source = pf_posix_join(pf_jstr(r), "config.yaml");
    json_free(r);
    r = pf_call(io, "config_exists", pf_path_args(source));
    if (pf_is_raise(r)) {
        if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); ret = NULL; goto done; }
        ret = r; goto done;
    }
    int src_exists = pf_truthy(r);
    json_free(r);
    if (src_exists) {
        r = pf_call(io, "read_user_config_raw", pf_path_args(source));
        if (pf_is_raise(r)) {
            if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); ret = NULL; goto done; }
            ret = r; goto done;
        }
        json_t *la = json_object();
        json_set(la, "config", json_copy(r));
        json_free(r);
        seed = pf_call(io, "launch_model_seed", la);
        if (pf_is_raise(seed)) {
            if (pf_caught_by_exception(pf_raise_kind(seed))) { json_free(seed); seed = NULL; ret = NULL; goto done; }
            ret = seed; seed = NULL; goto done;
        }
    } else {
        seed = json_object();
    }
    if (pf_truthy(seed)) {
        json_t *wa = json_object();
        json_set(wa, "path", json_string(config_path));
        json_set(wa, "config", json_copy(seed));
        r = pf_call(io, "atomic_config_write", wa);
        if (pf_is_raise(r)) {
            if (pf_caught_by_exception(pf_raise_kind(r))) { json_free(r); ret = NULL; goto done; }
            ret = r; goto done;
        }
        json_free(r);
    }
    ret = NULL;
done:
    free(config_path);
    free(source);
    json_free(seed);
    return ret;
}

/* PoP: pf_strip_multiplex_flag @ hermes_cli/profiles.py:_strip_multiplex_flag */
static json_t *pf_strip_multiplex_flag(pf_io_t *io, const char *config_path) {
    json_t *ce = pf_call(io, "config_exists", pf_path_args(config_path));
    if (pf_is_raise(ce)) return ce;   /* outside suppress */
    int exists = pf_truthy(ce);
    json_free(ce);
    if (!exists) return NULL;
    /* with suppress(Exception): */
    json_t *cfg = pf_call(io, "read_user_config_raw", pf_path_args(config_path));
    if (pf_is_raise(cfg)) {
        if (pf_caught_by_exception(pf_raise_kind(cfg))) { json_free(cfg); return NULL; }
        return cfg;
    }
    if (!cfg || cfg->type != JSON_OBJECT) {
        /* cfg.get(...) raises AttributeError -> swallowed */
        json_free(cfg);
        return NULL;
    }
    json_t *gw_node = json_obj_get(cfg, "gateway");
    json_t *gateway;
    int gw_attached = 0;
    if (gw_node && gw_node->type == JSON_OBJECT) { gateway = gw_node; gw_attached = 1; }
    else gateway = json_object();
    int in_cfg = json_has(cfg, "multiplex_profiles");
    int in_gw = json_has(gateway, "multiplex_profiles");
    if (!in_cfg && !in_gw) {
        if (!gw_attached) json_free(gateway);
        json_free(cfg);
        return NULL;
    }
    json_obj_del(cfg, "multiplex_profiles");
    json_obj_del(gateway, "multiplex_profiles");
    if (gateway->c.count == 0 && json_has(cfg, "gateway"))
        json_obj_del(cfg, "gateway");
    json_t *wa = json_object();
    json_set(wa, "path", json_string(config_path ? config_path : ""));
    json_set(wa, "config", json_copy(cfg));
    json_t *w = pf_call(io, "atomic_config_replace", wa);
    if (!gw_attached) json_free(gateway);
    json_free(cfg);
    if (pf_is_raise(w)) {
        if (pf_caught_by_exception(pf_raise_kind(w))) { json_free(w); return NULL; }
        return w;
    }
    json_free(w);
    return NULL;
}
