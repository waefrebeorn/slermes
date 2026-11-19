/*
 * commands_flow.c — command-surface FLOW cores + leaves of the Hermes CLI
 * slash-command registry cluster (LIVE authority: hermes_cli/commands.py +
 * its derivation siblings commands_platforms.py / commands_completion.py /
 * slash_exec.py / cli_unknown_command.py).
 *
 * Self-contained C11, libjson-only. Ports the UNCLAIMED decision sequences:
 * the localized-label leaves (_localized / category_label / CommandDef.
 * describe), the registry projections (desktop_surface_registry /
 * build_commands_by_category / build_commands / _gateway_available_commands),
 * the gateway-availability gate (_is_gateway_available), the PEP 562 module
 * __getattr__ dispatch, the Telegram menu helpers (_normalize_telegram_desc /
 * _truncate_desc / _sanitized_rank / _iter_gateway_skills / _gateway_bundles),
 * the prompt_toolkit completion leaves (_completion / _prefix_completions /
 * _split_args / _dir_completions / SlashCommandCompleter._call_provider), the
 * registry-owned executor slice (_exec_autoprune / _skill_commands) and the
 * unknown-command copy (unknown_command_lines). Claimed siblings
 * (_build_description / _resolve_config_gates / _sanitize_telegram_name /
 * _file_size_label) are SEAMED, never re-ported.
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; the caller frees args); the result is
 * a NEW json_t* (NULL == None; a JSON null result is delivered as a NULL
 * pointer; {"__raise__": cls, "msg": ...} == a raised exception). The harness
 * records {"ev": name, "args": args, "ret": result} per call in order; the
 * oracle's recording stubs emit the identical shape driving LIVE Python with
 * the same scripts. The cores perform NO I/O.
 *
 * Exception propagation: an UNCAUGHT seam raise parks its spec in the
 * file-static cf_pending channel and the core returns a sentinel; the harness
 * emits the parked spec as ret. `except Exception` sites swallow plain
 * Exception specs (full builtin hierarchy walked) but pass the BaseException
 * trio (SystemExit / KeyboardInterrupt / GeneratorExit) through. Fail-closed:
 * a missing/empty seam script yields a RuntimeError raise spec (recorded,
 * then propagated or swallowed exactly as live).
 *
 * Modeling notes:
 *  - COMMAND_REGISTRY is INJECTED per case (fixture "registry": list of
 *    CommandDef field dicts, all 15 fields); the live 90-entry registry is
 *    outside the fixture domain. CommandDef / CommandContext / CommandReply /
 *    prompt_toolkit Completion are plain record projections (no ctor seam):
 *    the oracle materializes the real dataclasses / a Completion double and
 *    norm() projects them back to the same field dicts.
 *  - pathlib.Path is modeled as a LEXICAL PurePosixPath (POSIX scope): the
 *    oracle stubs sys.modules["pathlib"].Path with a PurePosixPath subclass
 *    whose .resolve() fires the path_resolve seam; / join, parent, parts,
 *    is_relative_to, relative_to are lexical on both sides. PurePosixPath
 *    keeps ".." components and the "//" root (probed on the gate interpreter).
 *  - os.path in _dir_completions is posixpath (the oracle seeds an os double
 *    with path=posixpath); dirname/basename/join transcribed verbatim.
 *  - str.split()/lower() fixtures are ASCII (space \t \n \r \v \f whitespace);
 *    len()/slices in _truncate_desc/_completion count UTF-8 CODEPOINTS.
 *  - _sanitized_rank str(raw) domain: str / integral int / bool / null
 *    (container repr and integral-float str() are outside the fixture domain).
 *  - _TG_DASHES folds exactly U+2012 U+2013 U+2014 U+2015 U+2212 to '-'.
 *  - Import gates: fixture "import_gates" {module: true} models
 *    sys.modules[module] = None -> ModuleNotFoundError "import of <mod>
 *    halted; None in sys.modules" (probed), raised at the from-import
 *    statement (inside the try for _skill_commands -> swallowed; outside for
 *    _exec_autoprune / _iter_gateway_skills -> propagates).
 *
 * Seam name table (name -> live call):
 *   t                        agent.i18n.t(key, **kwargs) -> str
 *   _build_description       commands._build_description(cmd)  [claimed]
 *   _resolve_config_gates    commands._resolve_config_gates()  [claimed]
 *   _sanitize_telegram_name  platforms._sanitize_telegram_name(raw)  [claimed]
 *   _file_size_label         completion._file_size_label(path)  [claimed]
 *   build_commands           commands.build_commands()  (via __getattr__ table)
 *   build_commands_by_category commands.build_commands_by_category() (via table)
 *   get_skill_commands       agent.skill_commands.get_skill_commands()
 *   get_disabled_skill_names agent.skill_utils.get_disabled_skill_names(platform=)
 *   get_external_skills_dirs agent.skill_utils.get_external_skills_dirs()
 *   get_project_skills_dirs  agent.skill_utils.get_project_skills_dirs()
 *   get_skill_bundles        agent.skill_bundles.get_skill_bundles()
 *   _skills_dir              tools.skills_tool._skills_dir() -> path str
 *   path_resolve             Path(p).resolve() -> path str
 *   run_command              hermes_cli.prune_advisor.run_command(args)
 *   os_listdir               os.listdir(path) -> [str]
 *   os_isdir                 os.path.isdir(path) -> bool
 *   text_for                 _dir_completions text_for(full_path) callback
 *   provider                 _call_provider provider() callback
 *   get_close_matches        difflib.get_close_matches(word, possibilities, n=, cutoff=)
 *   logger.warning           logger.warning(fmt, exc_info=True)  args{msg,exc_info}
 *
 * Ownership: returned json_t* / char* are malloc'd (caller frees) unless noted.
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
#include "json.h"

/* ── universal seam plumbing ── */
typedef struct cf_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cf_seams_t;

static json_t *cf_pending = NULL;  /* parked uncaught raise spec */

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
/* Python `x is not None` (a present, non-null node). */
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static char *xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

static json_t *cf_call(const cf_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static const char *cf_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
#define CF_RAISED(r) (cf_raise_class(r) != NULL)
/* Park an uncaught raise spec (takes ownership) and signal propagation. */
static void cf_park(json_t *r) { cf_pending = r; }
static json_t *cf_take_pending(void) { json_t *r = cf_pending; cf_pending = NULL; return r; }

static json_t *cf_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static void cf_park_new(const char *cls, const char *msg) {
    cf_park(cf_make_raise(cls, msg));
}

/* ── exception hierarchy (FULL builtin table; `except Exception` walks it,
 *    the BaseException trio never matches Exception) ── */
static const char *cf_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeError", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"TypeError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"}, {"LookupError", "Exception"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"AssertionError", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"ReferenceError", "Exception"}, {"MemoryError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
    };
    if (!c) return NULL;
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (!strcmp(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool cf_exc_matches(const json_t *spec, const char *const *classes, size_t n) {
    const char *cls = cf_raise_class(spec);
    if (!cls) return false;
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (!strcmp(cls, classes[i])) return true;
        const char *p = cf_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
/* except Exception (NOT the BaseException trio). */
static bool cf_exc_is_exception(const json_t *spec) {
    static const char *E[] = {"Exception"};
    return cf_exc_matches(spec, E, 1);
}
/* except OSError (the _dir_completions listdir guard). */
static bool cf_exc_is_oserror(const json_t *spec) {
    static const char *E[] = {"OSError"};
    return cf_exc_matches(spec, E, 1);
}

/* ── args builders ── */
static json_t *args0(void) { return json_object(); }
static void aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── UTF-8 codepoint helpers (len/slice count CODEPOINTS, not bytes) ── */
static size_t cf_u8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
/* s[:stop] with Python negative-stop semantics (stop += len, clamp [0,len]). */
static char *cf_u8_prefix(const char *s, long long stop, long long len) {
    if (stop < 0) stop += len;
    if (stop < 0) stop = 0;
    if (stop > len) stop = len;
    const unsigned char *p = (const unsigned char *)s;
    long long seen = 0;
    while (*p && seen < stop) {
        p++;
        while ((*p & 0xC0) == 0x80) p++;
        seen++;
    }
    size_t n = (size_t)(p - (const unsigned char *)s);
    char *out = malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ── ASCII string helpers (fixture domain: ASCII; bytes >= 0x80 pass through) ── */
static char *cf_lower(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[n] = '\0';
    return out;
}
static bool cf_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* str.split() with no args: runs of whitespace, no empty tokens. */
static json_t *cf_split(const char *s) {
    json_t *out = json_array();
    if (!s) return out;
    const char *p = s;
    while (*p) {
        while (*p && cf_is_ws(*p)) p++;
        if (!*p) break;
        const char *b = p;
        while (*p && !cf_is_ws(*p)) p++;
        size_t n = (size_t)(p - b);
        char *w = malloc(n + 1);
        memcpy(w, b, n);
        w[n] = '\0';
        json_append(out, json_string(w));
        free(w);
    }
    return out;
}
static bool cf_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}
static bool cf_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t n = strlen(s), m = strlen(suf);
    return m <= n && !strcmp(s + n - m, suf);
}
/* repr(name) for the __getattr__ AttributeError message — fixture domain:
 * printable ASCII without quotes/backslashes/control chars. */
static char *cf_repr_str(const char *s) { return xsf("'%s'", s ? s : ""); }
/* str(v) for _sanitized_rank's str(raw) — domain: str/int/bool/null. */
static char *cf_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return xsf("%.0f", v->num_val);
    return strdup("");  /* containers: outside the fixture domain */
}
/* Python type name of a JSON value (integral numbers read as int). */
static const char *cf_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}
/* Park TypeError "'<T>' object is not iterable" (for/sorted/list on a scalar). */
static void cf_park_not_iterable(const json_t *v) {
    char *m = xsf("'%s' object is not iterable", cf_type_name(v));
    cf_park_new("TypeError", m);
    free(m);
}
/* `name in container` — array/object membership; a non-container parks the
 * py3.14 TypeError. Returns false when parked (caller checks cf_pending). */
static bool cf_member(const json_t *c, const char *name) {
    if (is_arr(c)) {
        for (size_t i = 0; i < json_len(c); i++) {
            const char *s = jstr(json_get(c, i));
            if (s && !strcmp(s, name)) return true;
        }
        return false;
    }
    if (is_obj(c)) return json_has(c, name);
    char *m = xsf("argument of type '%s' is not a container or iterable", cf_type_name(c));
    cf_park_new("TypeError", m);
    free(m);
    return false;
}
/* Sort an array of strings in place (codepoint order == UTF-8 byte order). */
static int cf_str_cmp(const void *a, const void *b) {
    const json_t *ja = *(const json_t *const *)a;
    const json_t *jb = *(const json_t *const *)b;
    return strcmp(ja->str_val ? ja->str_val : "", jb->str_val ? jb->str_val : "");
}
static void cf_sort_strs(json_t *arr) {
    if (is_arr(arr) && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), cf_str_cmp);
}
/* Sorted object keys (fresh malloc'd array of borrowed key pointers). */
static const char **cf_sorted_keys(const json_t *obj, size_t *n_out) {
    size_t n = is_obj(obj) ? obj->c.count : 0;
    const char **keys = malloc((n ? n : 1) * sizeof(char *));
    for (size_t i = 0; i < n; i++) keys[i] = obj->c.keys[i];
    for (size_t i = 1; i < n; i++) {
        const char *k = keys[i];
        size_t j = i;
        while (j > 0 && strcmp(keys[j - 1], k) > 0) { keys[j] = keys[j - 1]; j--; }
        keys[j] = k;
    }
    *n_out = n;
    return keys;
}

/* ── lexical PurePosixPath model (probed on the gate interpreter: keeps "..",
 *    keeps the "//" root, collapses "." and duplicate slashes, "" -> ".") ── */
typedef struct {
    char *root;     /* "", "/" or "//" */
    char **parts;   /* components without root, no "" or "." */
    size_t n;
} cf_path_t;

static void cf_path_free(cf_path_t *p) {
    if (!p) return;
    free(p->root);
    for (size_t i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts);
    p->root = NULL; p->parts = NULL; p->n = 0;
}
static cf_path_t cf_path_parse(const char *s) {
    cf_path_t p;
    p.root = strdup("");
    p.parts = NULL;
    p.n = 0;
    if (!s) s = "";
    const char *rest = s;
    if (s[0] == '/') {
        if (s[1] == '/' && s[2] != '/') {
            free(p.root); p.root = strdup("//"); rest = s + 2;
        } else {
            free(p.root); p.root = strdup("/"); rest = s + 1;
            while (*rest == '/') rest++;
        }
    }
    size_t cap = 0;
    const char *q = rest;
    while (*q) {
        while (*q == '/') q++;
        if (!*q) break;
        const char *b = q;
        while (*q && *q != '/') q++;
        size_t len = (size_t)(q - b);
        if (len == 1 && b[0] == '.') continue;  /* collapse "." */
        if (p.n == cap) {
            cap = cap ? cap * 2 : 4;
            p.parts = realloc(p.parts, cap * sizeof(char *));
        }
        char *part = malloc(len + 1);
        memcpy(part, b, len);
        part[len] = '\0';
        p.parts[p.n++] = part;
    }
    return p;
}
static char *cf_path_build(const char *root, char **parts, size_t n) {
    size_t len = strlen(root);
    for (size_t i = 0; i < n; i++) len += strlen(parts[i]) + 1;
    char *out = malloc(len + 2);
    size_t o = 0;
    size_t rl = strlen(root);
    memcpy(out, root, rl); o = rl;
    for (size_t i = 0; i < n; i++) {
        if (o > 0 && out[o - 1] != '/') out[o++] = '/';
        size_t pl = strlen(parts[i]);
        memcpy(out + o, parts[i], pl);
        o += pl;
    }
    out[o] = '\0';
    if (!o) { free(out); return strdup("."); }
    return out;
}
static char *cf_path_str(const cf_path_t *p) {
    return cf_path_build(p->root, p->parts, p->n);
}
static char *cf_path_norm(const char *s) {
    cf_path_t p = cf_path_parse(s);
    char *out = cf_path_str(&p);
    cf_path_free(&p);
    return out;
}
/* PurePosixPath a / b: an absolute b replaces a; else append components. */
static char *cf_path_join(const char *a, const char *b) {
    cf_path_t pa = cf_path_parse(a), pb = cf_path_parse(b);
    char *out;
    if (pb.root[0]) {
        out = cf_path_str(&pb);
    } else {
        size_t n = pa.n + pb.n;
        char **parts = malloc((n ? n : 1) * sizeof(char *));
        for (size_t i = 0; i < pa.n; i++) parts[i] = pa.parts[i];
        for (size_t i = 0; i < pb.n; i++) parts[pa.n + i] = pb.parts[i];
        out = cf_path_build(pa.root, parts, n);
        free(parts);
    }
    cf_path_free(&pa);
    cf_path_free(&pb);
    return out;
}
static char *cf_path_parent(const char *a) {
    cf_path_t p = cf_path_parse(a);
    char *out;
    if (p.n > 0) out = cf_path_build(p.root, p.parts, p.n - 1);
    else out = p.root[0] ? strdup(p.root) : strdup(".");
    cf_path_free(&p);
    return out;
}
static json_t *cf_path_parts(const char *a) {
    cf_path_t p = cf_path_parse(a);
    json_t *out = json_array();
    if (p.root[0]) json_append(out, json_string(p.root));
    for (size_t i = 0; i < p.n; i++) json_append(out, json_string(p.parts[i]));
    cf_path_free(&p);
    return out;
}
/* is_relative_to: other's (root, parts) is a prefix of self's. */
static bool cf_path_is_rel(const char *a, const char *b) {
    cf_path_t pa = cf_path_parse(a), pb = cf_path_parse(b);
    bool ok = !strcmp(pa.root, pb.root) && pb.n <= pa.n;
    if (ok) for (size_t i = 0; i < pb.n; i++)
        if (strcmp(pa.parts[i], pb.parts[i])) { ok = false; break; }
    cf_path_free(&pa);
    cf_path_free(&pb);
    return ok;
}
/* relative_to(...).parts — parks the live ValueError when not a subpath. */
static json_t *cf_path_rel_parts(const char *a, const char *b) {
    if (!cf_path_is_rel(a, b)) {
        char *na = cf_path_norm(a), *nb = cf_path_norm(b);
        char *m = xsf("'%s' is not in the subpath of '%s'", na, nb);
        cf_park_new("ValueError", m);
        free(m); free(na); free(nb);
        return NULL;
    }
    cf_path_t pa = cf_path_parse(a), pb = cf_path_parse(b);
    json_t *out = json_array();
    for (size_t i = pb.n; i < pa.n; i++) json_append(out, json_string(pa.parts[i]));
    cf_path_free(&pa);
    cf_path_free(&pb);
    return out;
}

/* ── posixpath (os.path double in the oracle; transcribed verbatim) ── */
static char *cf_dirname(const char *p) {
    if (!p) p = "";
    const char *slash = strrchr(p, '/');
    size_t i = slash ? (size_t)(slash - p) + 1 : 0;
    char *head = malloc(i + 1);
    memcpy(head, p, i);
    head[i] = '\0';
    if (head[0]) {
        bool all_sep = true;
        for (size_t k = 0; k < i; k++) if (head[k] != '/') { all_sep = false; break; }
        if (!all_sep) {
            size_t e = i;
            while (e > 0 && head[e - 1] == '/') e--;
            head[e] = '\0';
        }
    }
    return head;
}
static char *cf_basename(const char *p) {
    if (!p) p = "";
    const char *slash = strrchr(p, '/');
    return strdup(slash ? slash + 1 : p);
}
static char *cf_posix_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/' || !a[0]) return strdup(b);
    size_t n = strlen(a);
    if (a[n - 1] == '/') return xsf("%s%s", a, b);
    return xsf("%s/%s", a, b);
}

/* ── Path ctor / resolve seams ── */
/* Path(v): a str is required; parks the probed py3.14 TypeError otherwise.
 * Returns the normalized path string (malloc'd) or NULL when parked. */
static char *cf_path_ctor(const json_t *v) {
    if (is_str(v)) return cf_path_norm(v->str_val);
    char *m = xsf("argument should be a str or an os.PathLike object where "
                  "__fspath__ returns a str, not '%s'", cf_type_name(v));
    cf_park_new("TypeError", m);
    free(m);
    return NULL;
}
/* Path(p).resolve() -> path_resolve seam -> Path(result) ctor check. */
static char *cf_path_resolve(const char *path, const cf_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "path", path);
    json_t *r = cf_call(s, "path_resolve", a);
    json_free(a);
    if (CF_RAISED(r)) { cf_park(r); return NULL; }
    char *out = cf_path_ctor(r);
    json_free(r);
    return out;
}

/* ── import gates (sys.modules[mod] = None -> ModuleNotFoundError) ── */
static bool cf_import_gated(const json_t *inp, const char *mod) {
    const json_t *gates = json_obj_get(inp, "import_gates");
    return jtruth(json_obj_get(gates, mod));
}
static void cf_park_import_halt(const char *mod) {
    char *m = xsf("import of %s halted; None in sys.modules", mod);
    cf_park_new("ModuleNotFoundError", m);
    free(m);
}

/* ══ hermes_cli/commands.py ══ */

/* ── _localized: t(key) with default when the catalog returns the key ── */
/* PoP: cf_localized @ hermes_cli/commands.py:_localized */
static json_t *cf_localized(const char *key, const char *def, const cf_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "key", key);
    json_t *r = cf_call(s, "t", a);
    json_free(a);
    if (CF_RAISED(r)) { cf_park(r); return NULL; }
    if (is_str(r) && !strcmp(r->str_val, key)) {
        json_free(r);
        return json_string(def ? def : "");
    }
    return r;  /* NULL == None (value != key) */
}

/* ── category_label: _CATEGORY_SLUGS lookup + localized label ── */
/* PoP: cf_category_label @ hermes_cli/commands.py:category_label */
static json_t *cf_category_label(const char *category, const cf_seams_t *s) {
    static const struct { const char *cat, *slug; } T[] = {
        {"Session", "session"}, {"Configuration", "configuration"}, {"Info", "info"},
        {"Tools & Skills", "tools_skills"}, {"Plugins", "plugins"}, {"Exit", "exit"},
        {"Context", "context"}, {"Background & Automation", "background_automation"},
    };
    const char *slug = NULL;
    if (category) {
        for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
            if (!strcmp(T[i].cat, category)) { slug = T[i].slug; break; }
    }
    if (slug) {
        char *key = xsf("slash.category.%s", slug);
        json_t *r = cf_localized(key, category, s);
        free(key);
        return r;
    }
    return category ? json_string(category) : json_null();
}

/* ── CommandDef.describe: localized slash.<name>.description ── */
/* PoP: cf_describe @ hermes_cli/commands.py:describe */
static json_t *cf_describe(const json_t *cmd, const cf_seams_t *s) {
    const char *name = json_get_str(cmd, "name", "");
    const char *desc = json_get_str(cmd, "description", "");
    char *key = xsf("slash.%s.description", name);
    json_t *r = cf_localized(key, desc, s);
    free(key);
    return r;
}

/* ── desktop_surface_registry: /name + aliases -> desktop or None ── */
/* PoP: cf_desktop_surface_registry @ hermes_cli/commands.py:desktop_surface_registry */
static json_t *cf_desktop_surface_registry(const json_t *registry) {
    json_t *out = json_object();
    if (!is_arr(registry)) return out;
    for (size_t i = 0; i < json_len(registry); i++) {
        const json_t *cmd = json_get(registry, i);
        const json_t *d = json_obj_get(cmd, "desktop");
        const char *name = json_get_str(cmd, "name", "");
        char *k = xsf("/%s", name);
        json_set(out, k, jtruth(d) ? json_copy(d) : json_null());
        free(k);
        const json_t *al = json_obj_get(cmd, "aliases");
        if (is_arr(al)) {
            for (size_t j = 0; j < json_len(al); j++) {
                const char *a = jstr(json_get(al, j));
                if (!a) continue;
                k = xsf("/%s", a);
                json_set(out, k, jtruth(d) ? json_copy(d) : json_null());
                free(k);
            }
        }
    }
    return out;
}

/* ── build_commands_by_category: grouped localized descriptions ── */
/* PoP: cf_build_commands_by_category @ hermes_cli/commands.py:build_commands_by_category */
static json_t *cf_build_commands_by_category(const json_t *registry, const cf_seams_t *s) {
    json_t *grouped = json_object();
    if (!is_arr(registry)) {
        cf_park_not_iterable(registry);
        json_free(grouped);
        return NULL;
    }
    for (size_t i = 0; i < json_len(registry); i++) {
        const json_t *cmd = json_get(registry, i);
        if (jtruth(json_obj_get(cmd, "gateway_only"))) continue;
        const char *name = json_get_str(cmd, "name", "");
        json_t *entries = json_object();
        /* entries = {"/name": _build_description(cmd)}  [claimed sibling seam] */
        json_t *a = args0();
        aset_j(a, "cmd", cmd);
        json_t *r = cf_call(s, "_build_description", a);
        json_free(a);
        if (CF_RAISED(r)) {
            cf_park(r);
            json_free(entries); json_free(grouped);
            return NULL;
        }
        char *k = xsf("/%s", name);
        json_set(entries, k, r ? r : json_null());
        free(k);
        const json_t *al = json_obj_get(cmd, "aliases");
        if (is_arr(al)) {
            for (size_t j = 0; j < json_len(al); j++) {
                const char *alias = jstr(json_get(al, j));
                if (!alias) continue;
                /* kwargs evaluate left-to-right: description=cmd.describe()
                 * fires the describe t-seam BEFORE the alias_for t-seam. */
                json_t *desc = cf_describe(cmd, s);
                if (cf_pending) {
                    json_free(desc); json_free(entries); json_free(grouped);
                    return NULL;
                }
                a = args0();
                aset_s(a, "key", "slash.shared.alias_for");
                aset_j(a, "description", desc);
                aset_s(a, "name", name);
                json_free(desc);
                r = cf_call(s, "t", a);
                json_free(a);
                if (CF_RAISED(r)) {
                    cf_park(r);
                    json_free(entries); json_free(grouped);
                    return NULL;
                }
                k = xsf("/%s", alias);
                json_set(entries, k, r ? r : json_null());
                free(k);
            }
        }
        /* grouped.setdefault(cmd.category, {}).update(entries) */
        const char *cat = json_get_str(cmd, "category", "");
        json_t *g = json_obj_get(grouped, cat);
        if (!g) {
            g = json_object();
            json_set(grouped, cat, g);
        }
        for (size_t e = 0; e < entries->c.count; e++)
            json_set(g, entries->c.keys[e], json_copy(entries->c.items[e]));
        json_free(entries);
    }
    return grouped;
}

/* ── build_commands: flat merge of the per-category tables ── */
/* PoP: cf_build_commands @ hermes_cli/commands.py:build_commands */
static json_t *cf_build_commands(const json_t *registry, const cf_seams_t *s) {
    json_t *by = cf_build_commands_by_category(registry, s);
    if (cf_pending) { json_free(by); return NULL; }
    json_t *flat = json_object();
    if (is_obj(by)) {
        for (size_t i = 0; i < by->c.count; i++) {
            const json_t *entries = by->c.items[i];
            if (!is_obj(entries)) continue;
            for (size_t e = 0; e < entries->c.count; e++)
                json_set(flat, entries->c.keys[e], json_copy(entries->c.items[e]));
        }
    }
    json_free(by);
    return flat;
}

/* ── _is_gateway_available: cli_only gate + config-gate overrides ── */
/* PoP: cf_is_gateway_available @ hermes_cli/commands.py:_is_gateway_available */
static bool cf_is_gateway_available(const json_t *cmd, const json_t *overrides,
                                    const cf_seams_t *s) {
    if (!jtruth(json_obj_get(cmd, "cli_only"))) return true;
    if (!jtruth(json_obj_get(cmd, "gateway_config_gate"))) return false;
    const json_t *ov = overrides;
    json_t *owned = NULL;
    if (!jpresent(ov)) {  /* config_overrides is None -> _resolve_config_gates() */
        owned = cf_call(s, "_resolve_config_gates", args0());
        if (CF_RAISED(owned)) { cf_park(owned); return false; }
        ov = owned;
    }
    const char *name = json_get_str(cmd, "name", "");
    bool in = cf_member(ov, name);
    json_free(owned);
    return in;
}

/* ── module __getattr__ (PEP 562): lazy COMMANDS / COMMANDS_BY_CATEGORY ── */
/* PoP: cf_module_getattr @ hermes_cli/commands.py:__getattr__ */
static json_t *cf_module_getattr(const char *name, const cf_seams_t *s) {
    const char *seam = NULL;
    if (name && !strcmp(name, "COMMANDS")) seam = "build_commands";
    else if (name && !strcmp(name, "COMMANDS_BY_CATEGORY")) seam = "build_commands_by_category";
    if (!seam) {
        char *rn = cf_repr_str(name);
        char *m = xsf("module 'hermes_cli.commands' has no attribute %s", rn);
        cf_park_new("AttributeError", m);
        free(rn); free(m);
        return NULL;
    }
    json_t *r = cf_call(s, seam, args0());
    if (CF_RAISED(r)) { cf_park(r); return NULL; }
    return r;
}

/* ══ hermes_cli/commands_platforms.py ══ */

/* ── _gateway_available_commands: registry filtered by the config gates ── */
/* PoP: cf_gateway_available_commands @ hermes_cli/commands_platforms.py:_gateway_available_commands */
static json_t *cf_gateway_available_commands(const json_t *registry, const cf_seams_t *s) {
    json_t *ov = cf_call(s, "_resolve_config_gates", args0());
    if (CF_RAISED(ov)) { cf_park(ov); return NULL; }
    json_t *out = json_array();
    if (!is_arr(registry)) {
        cf_park_not_iterable(registry);
        json_free(out); json_free(ov);
        return NULL;
    }
    for (size_t i = 0; i < json_len(registry); i++) {
        const json_t *cmd = json_get(registry, i);
        bool ok = cf_is_gateway_available(cmd, ov, s);
        if (cf_pending) { json_free(out); json_free(ov); return NULL; }
        if (ok) json_append(out, json_copy(cmd));
    }
    json_free(ov);
    return out;
}

/* ── _normalize_telegram_desc: fold the five Unicode dashes to '-' ── */
/* PoP: cf_normalize_telegram_desc @ hermes_cli/commands_platforms.py:_normalize_telegram_desc */
static char *cf_normalize_telegram_desc(const char *desc) {
    if (!desc) return strdup("");
    size_t n = strlen(desc);
    char *out = malloc(n + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; ) {
        unsigned char c = (unsigned char)desc[i];
        if (c == 0xE2 && i + 2 < n && (unsigned char)desc[i + 1] == 0x80) {
            unsigned char c3 = (unsigned char)desc[i + 2];
            if (c3 == 0x92 || c3 == 0x93 || c3 == 0x94 || c3 == 0x95) {
                out[o++] = '-'; i += 3; continue;
            }
        }
        if (c == 0xE2 && i + 2 < n && (unsigned char)desc[i + 1] == 0x88
                && (unsigned char)desc[i + 2] == 0x92) {
            out[o++] = '-'; i += 3; continue;
        }
        out[o++] = desc[i++];
    }
    out[o] = '\0';
    return out;
}

/* ── _truncate_desc: clamp to limit codepoints with a '...' tail ── */
/* PoP: cf_truncate_desc @ hermes_cli/commands_platforms.py:_truncate_desc */
static char *cf_truncate_desc(const char *desc, double limit_d) {
    if (!desc) desc = "";
    long long limit = (long long)limit_d;
    long long len = (long long)cf_u8_len(desc);
    if (len <= limit) return strdup(desc);
    char *pre = cf_u8_prefix(desc, limit - 3, len);
    char *out = xsf("%s...", pre);
    free(pre);
    return out;
}

/* ── _sanitized_rank: deduped sanitized name -> first-occurrence rank ── */
/* PoP: cf_sanitized_rank @ hermes_cli/commands_platforms.py:_sanitized_rank */
static json_t *cf_sanitized_rank(const json_t *raw_names, const cf_seams_t *s) {
    json_t *rank = json_object();
    if (!is_arr(raw_names)) {
        cf_park_not_iterable(raw_names);
        json_free(rank);
        return NULL;
    }
    for (size_t i = 0; i < json_len(raw_names); i++) {
        const json_t *raw = json_get(raw_names, i);
        char *sv = cf_pystr(raw);
        json_t *a = args0();
        aset_s(a, "raw", sv);
        free(sv);
        json_t *r = cf_call(s, "_sanitize_telegram_name", a);
        json_free(a);
        if (CF_RAISED(r)) { cf_park(r); json_free(rank); return NULL; }
        if (jtruth(r)) {
            const char *nm = jstr(r);
            if (nm && !json_has(rank, nm))
                json_set(rank, nm, json_number((double)rank->c.count));
        }
        json_free(r);
    }
    return rank;
}

/* ── _iter_gateway_skills: eligible gateway skill commands (generator) ── */
/* PoP: cf_iter_gateway_skills @ hermes_cli/commands_platforms.py:_iter_gateway_skills */
static json_t *cf_iter_gateway_skills(const json_t *inp, const cf_seams_t *s) {
    const char *platform = json_get_str(inp, "platform", "");
    char *skills_dir = NULL, *hub = NULL, *sp = NULL, *par = NULL;
    char **roots = NULL;
    size_t nroots = 0;
    json_t *dis = NULL, *sc = NULL, *out = NULL;
    bool dis_empty_set = false;
    const char **keys = NULL;
    size_t nkeys = 0;

    /* In-function imports (statement order), all OUTSIDE any try. */
    static const char *MODS[] = {"agent.skill_commands", "agent.skill_utils", "tools.skills_tool"};
    for (size_t i = 0; i < 3; i++) {
        if (cf_import_gated(inp, MODS[i])) {
            cf_park_import_halt(MODS[i]);
            return NULL;
        }
    }
    json_t *sd_r = cf_call(s, "_skills_dir", args0());
    if (CF_RAISED(sd_r)) { cf_park(sd_r); return NULL; }
    skills_dir = cf_path_ctor(sd_r);
    json_free(sd_r);
    if (cf_pending) return NULL;

    /* try: disabled = get_disabled_skill_names(platform=platform)
     * except Exception: disabled = set() */
    json_t *a = args0();
    aset_s(a, "platform", platform);
    dis = cf_call(s, "get_disabled_skill_names", a);
    json_free(a);
    if (CF_RAISED(dis)) {
        if (cf_exc_is_exception(dis)) { json_free(dis); dis = NULL; dis_empty_set = true; }
        else { cf_park(dis); goto fail; }
    }

    /* hub_dir = (skills_dir / ".hub").resolve(); roots = [skills_dir.resolve()] */
    char *hub_j = cf_path_join(skills_dir, ".hub");
    hub = cf_path_resolve(hub_j, s);
    free(hub_j);
    if (cf_pending) goto fail;
    char *root0 = cf_path_resolve(skills_dir, s);
    if (cf_pending) goto fail;
    roots = malloc(2 * sizeof(char *));
    roots[0] = root0;
    nroots = 1;

    /* for getter in (get_external_skills_dirs, get_project_skills_dirs):
     *   try: for d in getter(): try: roots.append(Path(d).resolve())
     *                            except Exception: continue
     *   except Exception: pass */
    static const char *GETTERS[] = {"get_external_skills_dirs", "get_project_skills_dirs"};
    for (size_t g = 0; g < 2; g++) {
        json_t *r = cf_call(s, GETTERS[g], args0());
        if (CF_RAISED(r)) {
            if (cf_exc_is_exception(r)) { json_free(r); continue; }
            cf_park(r); goto fail;
        }
        if (!is_arr(r)) { json_free(r); continue; }  /* iteration TypeError -> outer except */
        for (size_t i = 0; i < json_len(r); i++) {
            const json_t *d = json_get(r, i);
            if (!is_str(d)) continue;  /* Path(d) TypeError -> inner except: continue */
            char *rd = cf_path_resolve(d->str_val, s);
            if (cf_pending) {
                json_t *pr = cf_take_pending();
                if (cf_exc_is_exception(pr)) { json_free(pr); continue; }
                cf_park(pr); json_free(r); goto fail;
            }
            roots = realloc(roots, (nroots + 1) * sizeof(char *));
            roots[nroots++] = rd;
        }
        json_free(r);
    }

    sc = cf_call(s, "get_skill_commands", args0());
    if (CF_RAISED(sc)) { cf_park(sc); goto fail; }
    if (!is_obj(sc)) {  /* sorted(None) TypeError (domain: object or null) */
        cf_park_not_iterable(sc);
        goto fail;
    }
    out = json_array();
    keys = cf_sorted_keys(sc, &nkeys);
    for (size_t i = 0; i < nkeys; i++) {
        const char *cmd_key = keys[i];
        const json_t *info = json_obj_get(sc, cmd_key);
        const char *skill_path = json_get_str(info, "skill_md_path", "");
        if (!skill_path[0]) continue;
        sp = cf_path_resolve(skill_path, s);
        if (cf_pending) goto fail;
        if (cf_path_is_rel(sp, hub)) { free(sp); sp = NULL; continue; }
        const char *root = NULL;
        for (size_t r = 0; r < nroots; r++)
            if (cf_path_is_rel(sp, roots[r])) { root = roots[r]; break; }
        if (!root) { free(sp); sp = NULL; continue; }
        /* if root is None or info.get("name", "") in disabled: continue */
        const char *nm = json_get_str(info, "name", "");
        bool disabled = dis_empty_set ? false : cf_member(dis, nm);
        if (cf_pending) goto fail;
        if (disabled) { free(sp); sp = NULL; continue; }
        par = cf_path_parent(sp);
        free(sp); sp = NULL;
        json_t *rel = cf_path_rel_parts(par, root);
        free(par); par = NULL;
        if (cf_pending) goto fail;
        json_t *entry = json_array();
        json_append(entry, json_string(cmd_key));
        json_append(entry, json_copy(info));
        json_append(entry, rel);
        json_append(out, entry);
    }
    free(keys);
    json_free(sc);
    for (size_t r = 0; r < nroots; r++) free(roots[r]);
    free(roots);
    free(hub);
    free(skills_dir);
    json_free(dis);
    return out;
fail:
    free(sp); free(par);
    free(keys);
    json_free(sc);
    json_free(out);
    for (size_t r = 0; r < nroots; r++) free(roots[r]);
    free(roots);
    free(hub);
    free(skills_dir);
    json_free(dis);
    return NULL;
}

/* ── _gateway_bundles: sorted bundle items, warning fallback ── */
/* PoP: cf_gateway_bundles @ hermes_cli/commands_platforms.py:_gateway_bundles */
static json_t *cf_gateway_bundles(const cf_seams_t *s) {
    json_t *r = cf_call(s, "get_skill_bundles", args0());
    bool warn = false;
    if (CF_RAISED(r)) {
        if (cf_exc_is_exception(r)) { json_free(r); r = NULL; warn = true; }
        else { cf_park(r); return NULL; }
    } else if (!is_obj(r)) {  /* .items() AttributeError -> except Exception */
        json_free(r); r = NULL; warn = true;
    }
    if (warn) {
        json_t *a = args0();
        aset_s(a, "msg", "Skill bundles could not be scanned; omitting them from the command catalog");
        aset_b(a, "exc_info", true);
        json_t *lr = cf_call(s, "logger.warning", a);
        json_free(a);
        if (CF_RAISED(lr)) { cf_park(lr); return NULL; }
        json_free(lr);
        return json_array();
    }
    json_t *out = json_array();
    size_t n = 0;
    const char **keys = cf_sorted_keys(r, &n);
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(keys[i]));
        json_append(pair, json_copy(json_obj_get(r, keys[i])));
        json_append(out, pair);
    }
    free(keys);
    json_free(r);
    return out;
}

/* ══ hermes_cli/commands_completion.py ══ */

/* ── _completion: Completion replacing the word under the cursor ── */
/* PoP: cf_completion @ hermes_cli/commands_completion.py:_completion */
static json_t *cf_completion(const char *text, const char *word, const char *display,
                             const json_t *meta) {
    json_t *c = json_object();
    json_set(c, "text", json_string(text ? text : ""));
    json_set(c, "start_position", json_number(-(double)cf_u8_len(word)));
    json_set(c, "display", json_string(display ? display : ""));
    json_set(c, "display_meta", meta ? json_copy(meta) : json_null());
    return c;
}

/* ── _prefix_completions: case-folded prefix filter over (name, meta) rows ── */
/* PoP: cf_prefix_completions @ hermes_cli/commands_completion.py:_prefix_completions */
static json_t *cf_prefix_completions(const json_t *rows, const char *partial, bool skip_exact) {
    json_t *out = json_array();
    if (!is_arr(rows)) {
        cf_park_not_iterable(rows);
        json_free(out);
        return NULL;
    }
    char *lowered = cf_lower(partial);
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        if (!is_arr(row)) {
            char *m = xsf("cannot unpack non-iterable %s object", cf_type_name(row));
            cf_park_new("TypeError", m);
            free(m); free(lowered); json_free(out);
            return NULL;
        }
        size_t n = json_len(row);
        if (n != 2) {
            char *m = xsf("%s values to unpack (expected 2, got %zu)",
                          n < 2 ? "not enough" : "too many", n);
            cf_park_new("ValueError", m);
            free(m); free(lowered); json_free(out);
            return NULL;
        }
        const json_t *name_v = json_get(row, 0);
        const char *name = jstr(name_v);
        if (!name) {
            char *m = xsf("'%s' object has no attribute 'startswith'", cf_type_name(name_v));
            cf_park_new("AttributeError", m);
            free(m); free(lowered); json_free(out);
            return NULL;
        }
        const json_t *meta = json_get(row, 1);
        if (cf_startswith(name, lowered) && !(skip_exact && !strcmp(name, lowered)))
            json_append(out, cf_completion(name, partial, name, meta));
    }
    free(lowered);
    return out;
}

/* ── _split_args: (completed_words, partial); trailing space = fresh word ── */
/* PoP: cf_split_args @ hermes_cli/commands_completion.py:_split_args */
static json_t *cf_split_args(const char *sub_text) {
    if (!sub_text) sub_text = "";
    json_t *parts = cf_split(sub_text);
    json_t *out = json_array();
    if (cf_endswith(sub_text, " ") || json_len(parts) == 0) {
        json_append(out, parts);
        json_append(out, json_string(""));
        return out;
    }
    json_t *completed = json_array();
    for (size_t i = 0; i + 1 < json_len(parts); i++)
        json_append(completed, json_copy(json_get(parts, i)));
    json_append(out, completed);
    json_append(out, json_copy(json_get(parts, json_len(parts) - 1)));
    json_free(parts);
    return out;
}

/* ── _dir_completions: directory-listing completions (generator) ── */
/* PoP: cf_dir_completions @ hermes_cli/commands_completion.py:_dir_completions */
static json_t *cf_dir_completions(const json_t *inp, const cf_seams_t *s) {
    const char *expanded = json_get_str(inp, "expanded", "");
    const char *word = json_get_str(inp, "word", "");
    long long limit = (long long)json_get_num(inp, "limit", 30);
    const json_t *want_dir = json_obj_get(inp, "want_dir");
    char *search_dir = NULL, *prefix = NULL, *prefix_lower = NULL, *full = NULL;
    char *text = NULL, *display = NULL, *el = NULL;
    json_t *entries = NULL, *out = json_array(), *meta = NULL;

    if (cf_endswith(expanded, "/")) {
        search_dir = strdup(expanded);
        prefix = strdup("");
    } else {
        search_dir = cf_dirname(expanded);
        if (!search_dir[0]) { free(search_dir); search_dir = strdup("."); }
        prefix = cf_basename(expanded);
    }
    /* try: entries = os.listdir(search_dir) except OSError: return */
    json_t *a = args0();
    aset_s(a, "path", search_dir);
    entries = cf_call(s, "os_listdir", a);
    json_free(a);
    if (CF_RAISED(entries)) {
        if (cf_exc_is_oserror(entries)) {
            json_free(entries);
            free(search_dir); free(prefix);
            return out;  /* empty generator */
        }
        cf_park(entries);
        goto fail;
    }
    if (!is_arr(entries)) {  /* sorted(None) TypeError propagates (no try) */
        cf_park_not_iterable(entries);
        goto fail;
    }
    prefix_lower = cf_lower(prefix);
    cf_sort_strs(entries);
    long long count = 0;
    for (size_t i = 0; i < json_len(entries); i++) {
        const char *entry = jstr(json_get(entries, i));
        if (!entry) continue;
        el = cf_lower(entry);
        bool match = cf_startswith(el, prefix_lower);
        free(el); el = NULL;
        if (!match) continue;
        full = cf_posix_join(search_dir, entry);
        a = args0();
        aset_s(a, "path", full);
        json_t *isdir_r = cf_call(s, "os_isdir", a);
        json_free(a);
        if (CF_RAISED(isdir_r)) { cf_park(isdir_r); goto fail; }
        bool is_dir = jtruth(isdir_r);
        json_free(isdir_r);
        if (jpresent(want_dir) && is_bool(want_dir) && want_dir->bool_val != is_dir) {
            free(full); full = NULL;
            continue;
        }
        if (count >= limit) { free(full); full = NULL; break; }
        const char *suffix = is_dir ? "/" : "";
        /* yield args evaluate left-to-right: text_for first, then the meta
         * expression (_file_size_label only for non-dirs). */
        a = args0();
        aset_s(a, "path", full);
        json_t *tf = cf_call(s, "text_for", a);
        json_free(a);
        if (CF_RAISED(tf)) { cf_park(tf); goto fail; }
        text = xsf("%s%s", jstr(tf) ? jstr(tf) : "", suffix);
        json_free(tf);
        if (is_dir) {
            meta = json_string("dir");
        } else {
            a = args0();
            aset_s(a, "path", full);
            meta = cf_call(s, "_file_size_label", a);
            json_free(a);
            if (CF_RAISED(meta)) { cf_park(meta); goto fail; }
        }
        display = xsf("%s%s", entry, suffix);
        json_append(out, cf_completion(text, word, display, meta));
        free(text); text = NULL;
        free(display); display = NULL;
        json_free(meta); meta = NULL;
        free(full); full = NULL;
        count++;
    }
    free(prefix_lower);
    json_free(entries);
    free(search_dir);
    free(prefix);
    return out;
fail:
    free(el); free(full); free(text); free(display);
    json_free(meta);
    free(prefix_lower);
    json_free(entries);
    free(search_dir);
    free(prefix);
    json_free(out);
    return NULL;
}

/* ── SlashCommandCompleter._call_provider: provider() or {} with guard ── */
/* PoP: cf_call_provider @ hermes_cli/commands_completion.py:_call_provider */
static json_t *cf_call_provider(bool provider_present, const cf_seams_t *s) {
    json_t *r = NULL;
    if (provider_present) {
        r = cf_call(s, "provider", args0());
        if (CF_RAISED(r)) {
            if (cf_exc_is_exception(r)) { json_free(r); r = NULL; }
            else { cf_park(r); return NULL; }
        }
    }
    if (jtruth(r)) return r;
    json_free(r);
    return json_object();
}

/* ══ hermes_cli/slash_exec.py ══ */

/* ── _exec_autoprune: CommandReply(run_command(ctx.args)) ── */
/* PoP: cf_exec_autoprune @ hermes_cli/slash_exec.py:_exec_autoprune */
static json_t *cf_exec_autoprune(const json_t *ctx, const json_t *inp, const cf_seams_t *s) {
    if (cf_import_gated(inp, "hermes_cli.prune_advisor")) {
        cf_park_import_halt("hermes_cli.prune_advisor");
        return NULL;
    }
    const char *args = json_get_str(ctx, "args", "");
    json_t *a = args0();
    aset_s(a, "args", args);
    json_t *r = cf_call(s, "run_command", a);
    json_free(a);
    if (CF_RAISED(r)) { cf_park(r); return NULL; }
    json_t *reply = json_object();
    json_set(reply, "text", r ? r : json_null());
    json_set(reply, "data", json_object());
    json_set(reply, "format", json_string("plain"));
    return reply;
}

/* ── _skill_commands: registered skill commands or {} ── */
/* PoP: cf_skill_commands @ hermes_cli/slash_exec.py:_skill_commands */
static json_t *cf_skill_commands(const json_t *inp, const cf_seams_t *s) {
    /* the from-import sits INSIDE the try: a gated import is swallowed */
    if (cf_import_gated(inp, "agent.skill_commands")) return json_object();
    json_t *r = cf_call(s, "get_skill_commands", args0());
    if (CF_RAISED(r)) {
        if (cf_exc_is_exception(r)) { json_free(r); return json_object(); }
        cf_park(r);
        return NULL;
    }
    if (jtruth(r)) return r;
    json_free(r);
    return json_object();
}

/* ══ hermes_cli/cli_unknown_command.py ══ */

/* ── unknown_command_lines: (lead, pointer) with a fuzzy near-miss hint ── */
/* PoP: cf_unknown_command_lines @ hermes_cli/cli_unknown_command.py:unknown_command_lines */
static json_t *cf_unknown_command_lines(const char *typed, const json_t *known,
                                        const cf_seams_t *s) {
    if (!typed) typed = "";
    json_t *parts = cf_split(typed);
    const char *base = json_len(parts) ? jstr(json_get(parts, 0)) : typed;
    /* list(known) evaluates BEFORE the get_close_matches call */
    if (!is_arr(known)) {
        cf_park_not_iterable(known);
        json_free(parts);
        return NULL;
    }
    json_t *a = args0();
    aset_s(a, "word", base);
    aset_j(a, "possibilities", known);
    aset_n(a, "n", 1);
    aset_n(a, "cutoff", 0.6);
    json_t *close = cf_call(s, "get_close_matches", a);
    json_free(a);
    json_free(parts);
    if (CF_RAISED(close)) { cf_park(close); return NULL; }
    char *hint = strdup("");
    if (jtruth(close)) {
        const json_t *first = json_get(close, 0);
        a = args0();
        aset_s(a, "key", "cli.command.did_you_mean_one");
        aset_j(a, "suggestion", first);
        json_t *r = cf_call(s, "t", a);
        json_free(a);
        if (CF_RAISED(r)) { cf_park(r); free(hint); json_free(close); return NULL; }
        if (!is_str(r)) {
            char *m = xsf("can only concatenate str (not \"%s\") to str", cf_type_name(r));
            cf_park_new("TypeError", m);
            free(m); free(hint); json_free(r); json_free(close);
            return NULL;
        }
        free(hint);
        hint = xsf(" %s", r->str_val);
        json_free(r);
    }
    json_free(close);
    a = args0();
    aset_s(a, "key", "cli.command.unknown_nothing_sent");
    aset_s(a, "command", base);
    aset_s(a, "hint", hint);
    json_t *lead = cf_call(s, "t", a);
    json_free(a);
    free(hint);
    if (CF_RAISED(lead)) { cf_park(lead); return NULL; }
    a = args0();
    aset_s(a, "key", "cli.command.type_help");
    json_t *pointer = cf_call(s, "t", a);
    json_free(a);
    if (CF_RAISED(pointer)) { cf_park(pointer); json_free(lead); return NULL; }
    json_t *out = json_array();
    json_append(out, lead ? lead : json_null());
    json_append(out, pointer ? pointer : json_null());
    return out;
}
