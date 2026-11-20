/*
 * i18n_flow2.c — FLOW remainder of agent/i18n.py (LIVE authority) that the
 * cli_output_flow port left unclaimed, plus the one unclaimed i18n_layers
 * callee those flows compose:
 *
 *   agent/i18n.py : _locales_dir, _bundled_surfaces, _normalize_lang,
 *       _flatten_into, _load_catalog, _config_language_cached,
 *       reset_language_cache, get_language, t
 *   agent/i18n_layers.py : clear_cache   (composed REAL inside
 *       reset_language_cache, exactly as live)
 *
 * The eight i18n defs cli_output_flow already ported (_current_home,
 * _config_language, _resolve_language, supported_languages,
 * resolve_language_id, _load_bundled, surface_catalog, language_surfaces)
 * are CLAIMED siblings and enter through the universal seam. The stub-era
 * src/agent/i18n_ports.c PoP claims (i18_* printf/strstr shims) move no
 * behaviour; this port supersedes them differentially.
 *
 * Self-contained C11, libjson-only. Every foreign collaborator and every
 * claimed sibling enters through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; i18n2_call frees args); the result
 * is a NEW json_t* the caller frees (NULL == None; a scripted JSON null is
 * delivered as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised
 * exception). The harness records {"ev": name, "args": args, "ret": result}
 * per call in order, dropping null-valued args exactly as the oracle's
 * seam() does; the oracle's recording stubs emit the identical shape driving
 * LIVE Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (no seam events on either side):
 *   i18n2_normalize_lang inside i18n2_config_language_cached and i18n2_t;
 *   i18n2_load_catalog inside i18n2_t (target catalog + English fallback);
 *   i18n2_clear_cache inside i18n2_reset_language_cache.
 * The oracle keeps those extracted defs real on the same paths. CROSS-MODULE
 * calls are seamed (i18n_layers.scan_locale_dir inside _bundled_surfaces;
 * i18n_layers.flatten inside _flatten_into; i18n_layers.overlay_layer /
 * pack_layer inside _load_catalog) — the oracle's i18n_layers module double
 * carries recorders for those names.
 *
 * Exception propagation: every flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except Exception` (_config_language_cached)
 * swallows every non-BaseException kind; t's format step swallows ONLY
 * KeyError / IndexError / ValueError. The BaseException trio (SystemExit /
 * KeyboardInterrupt / GeneratorExit) and BaseException itself ALWAYS pass
 * through. The full builtin exception parent table below drives
 * i18n2_kind_is_base_only (unknown kinds are treated as Exception subclasses,
 * matching the oracle's synthetic make_exc). Fail-closed: a missing/empty
 * seam script yields a RuntimeError raise spec (recorded, then
 * propagated/swallowed exactly as live); no seam installed at all yields the
 * same spec.
 *
 * lru_cache modeling: _bundled_surfaces (maxsize=4) and _config_language_cached
 * (maxsize=8) are state-backed caches — key PRESENCE is a hit (an explicit
 * null config_lang_cache entry is a cached None and HITS; the catalog_cache
 * `cached is not None` check instead skips present-null entries). The oracle
 * replaces functools.lru_cache with an injectable store double with the same
 * hit/miss/cache_clear semantics. Fixture domain: at most 3 distinct keys per
 * cache per case (below both maxsizes), so LRU eviction never fires and the
 * access-order reshuffle is unobservable.
 *
 * Locks (_catalog_lock / _lock) are single-threaded no-ops: they emit NO seam
 * events on either side (the oracle seeds silent context-manager doubles).
 * logger.debug/warning are silent on both sides (RecLogger).
 *
 * Ordered-collection modeling: _bundled_surfaces returns a live frozenset of
 * (lang, surface) pairs whose iteration is hash-randomized; the oracle's
 * norm() sorts it, so C emits the deduped pairs SORTED by (lang, surface)
 * (ASCII strcmp == Python tuple sort there), both as the return value and as
 * the cached/echoed state. scan_locale_dir scripts are arrays of exactly-3
 * string triples [lang, surface, path]; a non-array element raises the live
 * py3.14 "cannot unpack non-iterable <type> object" (dict elements — which
 * live unpacks as their keys — are outside the fixture domain).
 *
 * State (injected json_t, mutated in place, echoed by the harness):
 *   _load_catalog / t:
 *     {"catalog_cache": {"<home>\x1f<lang>": {key: text}}}   (_catalog_cache)
 *   _config_language_cached:
 *     {"config_lang_cache": {home: text-or-null}}            (lru store)
 *   _bundled_surfaces:
 *     {"bundled_surfaces_cache": {dir: [[lang, surface], ...]}}  (lru store,
 *      values are SORTED pair arrays — the frozenset echo projection)
 *   reset_language_cache: all seven sub-objects (catalog_cache,
 *     supported_cache, config_lang_cache, bundled_surfaces_cache, pack_cache,
 *     overlay_cache, overlay_files_cache), cleared.
 *   clear_cache: the three i18n_layers caches, cleared.
 * Cache keys use the \x1f (unit separator) composite encoding; the oracle
 * converts to/from real tuple-keyed dicts.
 *
 * Fixture domain (ASCII): paths are verbatim clean POSIX strings (no trailing
 * slash, no "//", no "."/".." segments; the oracle seeds Path=PathDouble, a
 * PurePosixPath subclass, so str()/join keep POSIX separators on Windows).
 * Path("") materializes as "." (PurePosixPath identity), modeled for the
 * scan_locale_dir seam argument. os.getenv / project_root_str scripts are
 * strings or null (null raises the live AttributeError/TypeError, modeled).
 * _current_home / _resolve_language scripts are strings; resolve_language_id
 * scripts are strings or null. _load_bundled / overlay_layer / pack_layer
 * scripts are objects, null or raises (dict.update(None) -> TypeError
 * "'NoneType' object is not iterable"; None.update -> AttributeError, both
 * modeled). Numbers in str()-computed positions are ints or strings —
 * integral floats are kept OUT of those positions (JSON cannot distinguish
 * int from float, and live str(5.0) == "5.0").
 *
 * t's str.format domain (literal CPython parse_field transcription, see
 * Objects/stringlib/unicode_format.h): literal text, {{ / }} escapes,
 * {name} / {name!s} / {name!r} / {name!a} fields with EMPTY format specs,
 * auto {} and numeric {0} fields (args tuple is always empty -> IndexError),
 * missing kwargs (-> KeyError), and the malformed-template ValueError family
 * (single braces, unterminated fields, unexpected '{' in field name, unknown
 * conversion specifier, expected ':' after conversion specifier, unmatched
 * '{' in format spec, end of string while looking for conversion specifier).
 * Field lookup happens BEFORE conversion validation (CPython output_markup
 * order), and the part before the first '.'/'[' resolves exactly as live
 * (missing base -> KeyError of that first part). kwargs values are
 * str/int/bool/None; !r/!a operate on ASCII strings (repr quoting: single
 * quotes unless the value contains ' and no "). Attribute/index traversal of
 * a FOUND field value and non-empty format specs are OUTSIDE the fixture
 * domain and raise a defensive ValueError.
 * A format failure returns the unformatted string; any other exception kind
 * propagates. A non-string catalog value with kwargs raises the live
 * AttributeError ('int' object has no attribute 'format', etc.).
 *
 * Seam name table (name -> live call):
 *   _current_home           agent.i18n._current_home() [claimed]  args{}
 *   _resolve_language       agent.i18n._resolve_language(home) [claimed]
 *                           args{home?}
 *   resolve_language_id     agent.i18n.resolve_language_id(value, home)
 *                           [claimed]  args{value?, home?}
 *   _load_bundled           agent.i18n._load_bundled(lang) [claimed]
 *                           args{lang}
 *   os.getenv               os.getenv(key, default)  args{key, default}
 *   path.is_dir             Path.is_dir()  args{path}
 *   project_root_str        hermes_cli._startup_fast.project_root_str()
 *                           args{}
 *   load_config_readonly    hermes_cli.config.load_config_readonly()  args{}
 *   scan_locale_dir         agent.i18n_layers.scan_locale_dir(dir)
 *                           args{directory}
 *   flatten                 agent.i18n_layers.flatten(node, prefix, out)
 *                           args{node?, prefix, out?}
 *   overlay_layer           agent.i18n_layers.overlay_layer(home, lang)
 *                           args{home, lang, surface:"core"}
 *   pack_layer              agent.i18n_layers.pack_layer(lang)
 *                           args{lang, surface:"core"}
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* ── universal seam plumbing ── */

typedef json_t *(*i18n2_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct i18n2_io {
    i18n2_seam_fn call;
    void *ctx;
} i18n2_io_t;

static int i18n2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *i18n2_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

/* Full builtin exception parent table (Python 3.14) + the quarry's
 * UnscopedSecretError(RuntimeError). Drives i18n2_kind_is_base_only. */
typedef struct i18n2_exc_parent { const char *name; const char *parent; } i18n2_exc_parent_t;
static const i18n2_exc_parent_t i18n2_exc_parents[] = {
    {"BaseException", ""},
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
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    /* OSError aliases (builtins.EnvironmentError / IOError / WindowsError are
     * OSError itself on py3.14) — same catch semantics, named for scripts. */
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"WindowsError", "OSError"},
    {"UnscopedSecretError", "RuntimeError"},
};

static const char *i18n2_exc_parent_of(const char *name) {
    for (size_t i = 0; i < sizeof i18n2_exc_parents / sizeof i18n2_exc_parents[0]; i++) {
        if (strcmp(i18n2_exc_parents[i].name, name) == 0) return i18n2_exc_parents[i].parent;
    }
    return NULL;
}

/* BaseException-only kinds: NOT caught by `except Exception`. Walk the parent
 * chain; reaching Exception means caught, reaching BaseException without
 * Exception means base-only. Unknown kinds are synthetic Exception subclasses
 * (the oracle's make_exc) -> caught. */
static int i18n2_kind_is_base_only(const char *k) {
    const char *cur = k;
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cur, "Exception") == 0) return 0;
        if (strcmp(cur, "BaseException") == 0) return 1;
        const char *p = i18n2_exc_parent_of(cur);
        if (!p) return 0;
        cur = p;
    }
    return 0;
}

static int i18n2_caught_by_exception(const char *k) {
    return !i18n2_kind_is_base_only(k);
}

static json_t *i18n2_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *i18n2_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return i18n2_raise(kind, buf);
}

static char *i18n2_fmt(const char *fmt, ...) {
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
static json_t *i18n2_call(i18n2_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = i18n2_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int i18n2_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:  return v->c.count ? 1 : 0;
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default:          return 0;
    }
}

/* Python type name of a JSON value (integral-float domain: numbers are ints). */
static const char *i18n2_type_name(const json_t *v) {
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

/* str() of a primitive seam result. Domain: strings, None, bools, ints.
 * Integral floats are kept OUT of str()-computed positions (see header). */
static char *i18n2_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) return i18n2_fmt("%.0f", v->num_val);
    return strdup("");
}

/* PurePosixPath join for the clean fixture domain (no "//", "." or "..").
 * An absolute right operand replaces the base; trailing slashes collapse. */
static char *i18n2_path_join(const char *base, const char *name) {
    if (!base) base = "";
    if (!name) name = "";
    if (name[0] == '/') return strdup(name);
    size_t bl = strlen(base);
    while (bl > 0 && base[bl - 1] == '/') bl--;
    if (bl == 0) {
        if (base[0] == '/') return i18n2_fmt("/%s", name);  /* root "/" */
        return strdup(name);                                /* empty base */
    }
    return i18n2_fmt("%.*s/%s", (int)bl, base, name);
}

/* dict.update: overwrite values, keep first-insertion key position (libjson
 * json_set already keeps the first position with the last value). */
static void i18n2_obj_update(json_t *dst, const json_t *src) {
    if (!dst || !src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++) {
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
    }
}

/* dict.clear() in place. */
static void i18n2_obj_clear(json_t *obj) {
    if (!obj || obj->type != JSON_OBJECT) return;
    while (obj->c.count > 0) json_obj_del(obj, obj->c.keys[0]);
}

/* The cache models ALWAYS-EXISTENT module globals: materialize the sub-object
 * at flow entry so the echoed state carries the key even on an early
 * return/raise, matching the oracle's projection of the live global. */
static void i18n2_ensure_obj(json_t *state, const char *key) {
    if (state && !json_obj_get(state, key)) json_set(state, key, json_object());
}

/* str.strip() for the ASCII whitespace set. */
static char *i18n2_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && strchr(" \t\n\r\v\f", s[a])) a++;
    while (b > a && strchr(" \t\n\r\v\f", s[b - 1])) b--;
    return i18n2_fmt("%.*s", (int)(b - a), s + a);
}

#define I18N2_DEFAULT_LANGUAGE "en"
#define I18N2_CORE_SURFACE "core"

/* ── str() / repr() for the format kwargs domain (str/int/bool/None) ── */

static char *i18n2_py_str(const json_t *v) {
    return i18n2_str_of(v);
}

/* repr() of an ASCII string: single quotes unless the value contains ' and
 * no " (then double quotes); escape \\, the active quote, \n \r \t and other
 * control chars as \xNN. Non-ASCII bytes pass through (printable-Unicode repr
 * domain; !a on non-ASCII is outside the fixture domain). */
static char *i18n2_py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return i18n2_fmt("%.0f", v->num_val);
    if (v->type != JSON_STRING) return strdup("");
    const char *s = v->str_val ? v->str_val : "";
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    size_t cap = n * 4 + 3;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\\' || ch == (unsigned char)q) {
            out[o++] = '\\'; out[o++] = (char)ch;
        } else if (ch == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (ch == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (ch == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (ch < 0x20 || ch == 0x7f) {
            o += (size_t)snprintf(out + o, cap - o, "\\x%02x", ch);
        } else {
            out[o++] = (char)ch;
        }
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}

/* ── str.format (literal CPython parse_field transcription; domain above) ── */

typedef struct i18n2_buf {
    char *s;
    size_t len, cap;
} i18n2_buf_t;

static void i18n2_buf_init(i18n2_buf_t *b) {
    b->cap = 64;
    b->len = 0;
    b->s = (char *)malloc(b->cap);
    if (b->s) b->s[0] = '\0';
}

static void i18n2_buf_addn(i18n2_buf_t *b, const char *s, size_t n) {
    if (!b->s) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap * 2;
        while (nc < b->len + n + 1) nc *= 2;
        char *ns = (char *)realloc(b->s, nc);
        if (!ns) return;
        b->s = ns;
        b->cap = nc;
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}

static void i18n2_buf_addc(i18n2_buf_t *b, char c) {
    i18n2_buf_addn(b, &c, 1);
}

static void i18n2_buf_addstr(i18n2_buf_t *b, const char *s) {
    if (s) i18n2_buf_addn(b, s, strlen(s));
}

/* value.format(**kwargs) with an EMPTY positional args tuple. Returns a new
 * json string, or a raise spec (KeyError / IndexError / ValueError are caught
 * by t's format step; anything else propagates). */
static json_t *i18n2_format(const char *tpl, const json_t *kwargs) {
    size_t n = strlen(tpl);
    i18n2_buf_t out;
    i18n2_buf_init(&out);
    size_t i = 0;
    long auto_idx = 0;
    while (i < n) {
        char c = tpl[i];
        if (c == '{') {
            if (i + 1 < n && tpl[i + 1] == '{') {
                i18n2_buf_addc(&out, '{');
                i += 2;
                continue;
            }
            if (i + 1 == n) {
                free(out.s);
                return i18n2_raise("ValueError", "Single '{' encountered in format string");
            }
            /* parse_field: pos starts just after the '{' */
            size_t pos = i + 1;
            size_t field_start = pos;
            char c2 = 0;
            while (pos < n) {
                c2 = tpl[pos]; pos++;
                if (c2 == '{') {
                    free(out.s);
                    return i18n2_raise("ValueError", "unexpected '{' in field name");
                }
                if (c2 == '[') {
                    while (pos < n && tpl[pos] != ']') pos++;
                    continue;
                }
                if (c2 == '}' || c2 == ':' || c2 == '!') break;
            }
            size_t field_end = pos - 1;
            char conv = 0;
            size_t spec_start = 0, spec_end = 0;
            int has_spec = 0;
            int field_done = 0;
            if (c2 == '!' || c2 == ':') {
                if (c2 == '!') {
                    if (pos >= n) {
                        free(out.s);
                        return i18n2_raise("ValueError",
                            "end of string while looking for conversion specifier");
                    }
                    conv = tpl[pos]; pos++;
                    if (pos < n) {
                        char c3 = tpl[pos]; pos++;
                        if (c3 == '}') {
                            field_done = 1;
                        } else if (c3 != ':') {
                            free(out.s);
                            return i18n2_raise("ValueError",
                                "expected ':' after conversion specifier");
                        }
                    }
                }
                if (!field_done) {
                    spec_start = pos;
                    int count = 1;
                    while (pos < n) {
                        char c4 = tpl[pos]; pos++;
                        if (c4 == '{') {
                            count++;
                        } else if (c4 == '}') {
                            count--;
                            if (count == 0) {
                                spec_end = pos - 1;
                                has_spec = 1;
                                field_done = 1;
                                break;
                            }
                        }
                    }
                    if (!field_done) {
                        free(out.s);
                        return i18n2_raise("ValueError", "unmatched '{' in format spec");
                    }
                }
            } else if (c2 != '}') {
                free(out.s);
                return i18n2_raise("ValueError", "expected '}' before end of string");
            }
            i = pos;   /* past the closing '}' */
            size_t flen = field_end - field_start;
            char *field = i18n2_fmt("%.*s", (int)flen, tpl + field_start);
            /* output_markup order: field lookup FIRST, then conversion, then
             * format spec. field_name_split: resolve the part before the first
             * '.'/'[' exactly as live (auto/digit -> IndexError — the args
             * tuple is empty; missing kwarg base -> KeyError of the FIRST
             * part); attribute/index traversal of a FOUND value is outside the
             * fixture domain. */
            size_t first_end = flen;
            for (size_t k = 0; k < flen; k++) {
                if (field[k] == '.' || field[k] == '[') { first_end = k; break; }
            }
            int has_rest = first_end < flen;
            char *first = i18n2_fmt("%.*s", (int)first_end, field);
            free(field);
            if (first_end == 0) {
                long idx = auto_idx++;
                free(first);
                free(out.s);
                return i18n2_raise_fmt("IndexError",
                    "Replacement index %ld out of range for positional args tuple", idx);
            }
            int all_digits = 1;
            for (size_t k = 0; k < first_end; k++) {
                if (!isdigit((unsigned char)first[k])) { all_digits = 0; break; }
            }
            if (all_digits) {
                long idx = strtol(first, NULL, 10);
                free(first);
                free(out.s);
                return i18n2_raise_fmt("IndexError",
                    "Replacement index %ld out of range for positional args tuple", idx);
            }
            const json_t *val = json_obj_get(kwargs, first);
            if (!val) {
                /* str(KeyError(key)) == repr(key) */
                json_t *kj = json_string(first);
                char *rk = i18n2_py_repr(kj);
                json_free(kj);
                json_t *e = i18n2_raise("KeyError", rk);
                free(rk);
                free(first);
                free(out.s);
                return e;
            }
            free(first);
            if (has_rest) {
                free(out.s);
                return i18n2_raise("ValueError",
                    "attribute/index field access is outside the i18n_flow2 format domain");
            }
            char *sval = NULL;
            if (conv == 0 || conv == 's') {
                sval = i18n2_py_str(val);
            } else if (conv == 'r' || conv == 'a') {
                sval = i18n2_py_repr(val);
            } else {
                json_t *e;
                if (conv > 32 && conv < 127) {
                    e = i18n2_raise_fmt("ValueError", "Unknown conversion specifier %c", conv);
                } else {
                    e = i18n2_raise_fmt("ValueError", "Unknown conversion specifier \\x%x",
                                        (unsigned int)(unsigned char)conv);
                }
                free(out.s);
                return e;
            }
            if (has_spec && spec_end > spec_start) {
                json_t *e = i18n2_raise_fmt("ValueError",
                    "non-empty format spec is outside the i18n_flow2 format domain: '%.*s'",
                    (int)(spec_end - spec_start), tpl + spec_start);
                free(sval);
                free(out.s);
                return e;
            }
            i18n2_buf_addstr(&out, sval);
            free(sval);
        } else if (c == '}') {
            if (i + 1 < n && tpl[i + 1] == '}') {
                i18n2_buf_addc(&out, '}');
                i += 2;
                continue;
            }
            free(out.s);
            return i18n2_raise("ValueError", "Single '}' encountered in format string");
        } else {
            i18n2_buf_addc(&out, c);
            i++;
        }
    }
    json_t *r = json_string(out.s ? out.s : "");
    free(out.s);
    return r;
}

/* ── the ported flows ── */

/* PoP: i18n2_locales_dir @ agent/i18n.py:_locales_dir */
static json_t *i18n2_locales_dir(i18n2_io_t *io) {
    json_t *a = json_object();
    json_set(a, "key", json_string("HERMES_BUNDLED_LOCALES"));
    json_set(a, "default", json_string(""));
    json_t *ov = i18n2_call(io, "os.getenv", a);
    if (i18n2_is_raise(ov)) return ov;
    if (!ov || ov->type != JSON_STRING) {
        json_t *e = i18n2_raise_fmt("AttributeError", "'%s' object has no attribute 'strip'",
                                    i18n2_type_name(ov));
        json_free(ov);
        return e;
    }
    char *s = i18n2_strip(ov->str_val);
    json_free(ov);
    if (s[0]) {
        json_t *da = json_object();
        json_set(da, "path", json_string(s));
        json_t *d = i18n2_call(io, "path.is_dir", da);
        if (i18n2_is_raise(d)) { free(s); return d; }
        int isdir = i18n2_truthy(d);
        json_free(d);
        if (isdir) {
            json_t *r = json_string(s);   /* Path(override): clean-domain identity */
            free(s);
            return r;
        }
        /* logger.warning: silent on both sides */
    }
    free(s);
    /* lazy import hermes_cli._startup_fast.project_root_str: no seam event */
    json_t *r = i18n2_call(io, "project_root_str", json_object());
    if (i18n2_is_raise(r)) return r;
    if (!r || r->type != JSON_STRING) {
        json_t *e = i18n2_raise_fmt("TypeError",
            "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
            i18n2_type_name(r));
        json_free(r);
        return e;
    }
    char *joined = i18n2_path_join(r->str_val, "locales");
    json_free(r);
    json_t *res = json_string(joined);
    free(joined);
    return res;
}

/* PoP: i18n2_bundled_surfaces @ agent/i18n.py:_bundled_surfaces */
/* lru_cache(maxsize=4) keyed by locales_dir; frozenset((lang, surface) for
 * lang, surface, _path in scan_locale_dir(Path(locales_dir))). The frozenset
 * echo/return projection is SORTED pairs (norm sorts live sets). */
typedef struct i18n2_pair { char *lang; char *surface; } i18n2_pair_t;

static int i18n2_cmp_pair(const void *a, const void *b) {
    const i18n2_pair_t *pa = (const i18n2_pair_t *)a;
    const i18n2_pair_t *pb = (const i18n2_pair_t *)b;
    int c = strcmp(pa->lang, pb->lang);
    return c != 0 ? c : strcmp(pa->surface, pb->surface);
}

static json_t *i18n2_bundled_surfaces(i18n2_io_t *io, const char *locales_dir, json_t *state) {
    i18n2_ensure_obj(state, "bundled_surfaces_cache");
    json_t *cache = json_obj_get(state, "bundled_surfaces_cache");
    json_t *cached = json_obj_get(cache, locales_dir ? locales_dir : "");
    if (cached) return json_copy(cached);   /* lru hit on key presence */
    /* Path(locales_dir): PurePosixPath("") materializes as "." */
    const char *dir = (locales_dir && locales_dir[0]) ? locales_dir : ".";
    json_t *a = json_object();
    json_set(a, "directory", json_string(dir));
    json_t *r = i18n2_call(io, "scan_locale_dir", a);
    if (i18n2_is_raise(r)) return r;
    if (!r || r->type != JSON_ARRAY) {
        json_t *e = i18n2_raise_fmt("TypeError", "'%s' object is not iterable",
                                    i18n2_type_name(r));
        json_free(r);
        return e;
    }
    size_t cap = r->c.count + 1;
    i18n2_pair_t *v = (i18n2_pair_t *)malloc(cap * sizeof(i18n2_pair_t));
    size_t nv = 0;
    json_t *err = NULL;
    for (size_t i = 0; i < r->c.count && !err; i++) {
        const json_t *t = r->c.items[i];
        if (!t || t->type != JSON_ARRAY) {
            /* unpacking a non-iterable element (py3.14 wording) */
            err = i18n2_raise_fmt("TypeError", "cannot unpack non-iterable %s object",
                                  i18n2_type_name(t));
            break;
        }
        if (t->c.count < 3) {
            err = i18n2_raise_fmt("ValueError", "not enough values to unpack (expected 3, got %zu)",
                                  t->c.count);
            break;
        }
        if (t->c.count > 3) {
            err = i18n2_raise_fmt("ValueError", "too many values to unpack (expected 3, got %zu)",
                                  t->c.count);
            break;
        }
        char *lj = i18n2_str_of(t->c.items[0]);
        char *sj = i18n2_str_of(t->c.items[1]);
        int dup = 0;
        for (size_t j = 0; j < nv; j++) {
            if (strcmp(v[j].lang, lj) == 0 && strcmp(v[j].surface, sj) == 0) { dup = 1; break; }
        }
        if (!dup) { v[nv].lang = lj; v[nv].surface = sj; nv++; }
        else { free(lj); free(sj); }
    }
    json_free(r);
    if (err) {
        for (size_t j = 0; j < nv; j++) { free(v[j].lang); free(v[j].surface); }
        free(v);
        return err;
    }
    qsort(v, nv, sizeof(i18n2_pair_t), i18n2_cmp_pair);
    json_t *result = json_array();
    for (size_t j = 0; j < nv; j++) {
        json_t *p = json_array();
        json_append(p, json_string(v[j].lang));
        json_append(p, json_string(v[j].surface));
        json_append(result, p);
        free(v[j].lang);
        free(v[j].surface);
    }
    free(v);
    json_set(cache, locales_dir ? locales_dir : "", json_copy(result));
    return result;
}

/* PoP: i18n2_normalize_lang @ agent/i18n.py:_normalize_lang */
static json_t *i18n2_normalize_lang(i18n2_io_t *io, const json_t *value, const json_t *home) {
    json_t *a = json_object();
    if (value) json_set(a, "value", json_copy(value));
    if (home) json_set(a, "home", json_copy(home));
    json_t *r = i18n2_call(io, "resolve_language_id", a);
    if (i18n2_is_raise(r)) return r;
    if (i18n2_truthy(r)) return r;
    json_free(r);
    return json_string(I18N2_DEFAULT_LANGUAGE);
}

/* PoP: i18n2_flatten_into @ agent/i18n.py:_flatten_into */
static json_t *i18n2_flatten_into(i18n2_io_t *io, const json_t *node, const char *prefix,
                                  const json_t *out) {
    json_t *a = json_object();
    if (node) json_set(a, "node", json_copy(node));
    json_set(a, "prefix", json_string(prefix ? prefix : ""));
    if (out) json_set(a, "out", json_copy(out));
    json_t *r = i18n2_call(io, "flatten", a);
    if (i18n2_is_raise(r)) return r;
    json_free(r);
    return NULL;   /* _flatten_into returns None; the delegation result is discarded */
}

/* PoP: i18n2_load_catalog @ agent/i18n.py:_load_catalog */
static json_t *i18n2_load_catalog(i18n2_io_t *io, const char *lang, const json_t *home,
                                  json_t *state) {
    i18n2_ensure_obj(state, "catalog_cache");
    json_t *h;
    if (i18n2_truthy(home)) {
        h = json_copy(home);
    } else {
        h = i18n2_call(io, "_current_home", json_object());
        if (i18n2_is_raise(h)) return h;
    }
    char *hs = i18n2_str_of(h);
    char *key = i18n2_fmt("%s\x1f%s", hs, lang ? lang : "");
    json_t *cache = json_obj_get(state, "catalog_cache");
    json_t *cached = json_obj_get(cache, key);
    if (cached && cached->type != JSON_NULL) {   /* cached is not None */
        json_t *r = json_copy(cached);
        free(hs); free(key); json_free(h);
        return r;
    }
    json_t *ba = json_object();
    json_set(ba, "lang", json_string(lang ? lang : ""));
    json_t *merged = i18n2_call(io, "_load_bundled", ba);
    if (i18n2_is_raise(merged)) { free(hs); free(key); json_free(h); return merged; }
    if (!merged || merged->type != JSON_OBJECT) {
        json_t *e = i18n2_raise_fmt("AttributeError", "'%s' object has no attribute 'update'",
                                    i18n2_type_name(merged));
        json_free(merged); free(hs); free(key); json_free(h);
        return e;
    }
    /* merged.update(i18n_layers.overlay_layer(home, lang)) — surface defaults core */
    json_t *oa = json_object();
    json_set(oa, "home", json_string(hs));
    json_set(oa, "lang", json_string(lang ? lang : ""));
    json_set(oa, "surface", json_string(I18N2_CORE_SURFACE));
    json_t *ol = i18n2_call(io, "overlay_layer", oa);
    if (i18n2_is_raise(ol)) { json_free(merged); free(hs); free(key); json_free(h); return ol; }
    if (!ol) {
        json_free(merged); free(hs); free(key); json_free(h);
        return i18n2_raise("TypeError", "'NoneType' object is not iterable");
    }
    if (ol->type != JSON_OBJECT) {
        json_free(ol); json_free(merged); free(hs); free(key); json_free(h);
        return i18n2_raise("ValueError", "dictionary update sequence element #0 has length 1; 2 is required");
    }
    i18n2_obj_update(merged, ol);
    json_free(ol);
    /* merged.update(i18n_layers.pack_layer(lang)) */
    json_t *pa = json_object();
    json_set(pa, "lang", json_string(lang ? lang : ""));
    json_set(pa, "surface", json_string(I18N2_CORE_SURFACE));
    json_t *pl = i18n2_call(io, "pack_layer", pa);
    if (i18n2_is_raise(pl)) { json_free(merged); free(hs); free(key); json_free(h); return pl; }
    if (!pl) {
        json_free(merged); free(hs); free(key); json_free(h);
        return i18n2_raise("TypeError", "'NoneType' object is not iterable");
    }
    if (pl->type != JSON_OBJECT) {
        json_free(pl); json_free(merged); free(hs); free(key); json_free(h);
        return i18n2_raise("ValueError", "dictionary update sequence element #0 has length 1; 2 is required");
    }
    i18n2_obj_update(merged, pl);
    json_free(pl);
    json_set(cache, key, json_copy(merged));
    free(hs); free(key); json_free(h);
    return merged;
}

/* PoP: i18n2_config_language_cached @ agent/i18n.py:_config_language_cached */
/* lru_cache(maxsize=8) keyed by hermes_home; a None result IS cached (key
 * presence hits). The whole body sits inside `except Exception`. */
static json_t *i18n2_config_language_cached(i18n2_io_t *io, const char *hermes_home,
                                            json_t *state) {
    i18n2_ensure_obj(state, "config_lang_cache");
    json_t *cache = json_obj_get(state, "config_lang_cache");
    json_t *cached = json_obj_get(cache, hermes_home ? hermes_home : "");
    if (cached) return json_copy(cached);   /* lru hit on presence, incl. cached None */
    json_t *result = NULL;
    json_t *r = i18n2_call(io, "load_config_readonly", json_object());
    if (i18n2_is_raise(r)) {
        if (i18n2_caught_by_exception(i18n2_raise_kind(r))) {
            json_free(r);   /* except Exception -> logger.debug silent -> None */
        } else {
            return r;
        }
    } else if (!r || r->type != JSON_OBJECT) {
        json_free(r);   /* .get on a non-dict: AttributeError, caught -> None */
    } else {
        /* lang = (config.get("display") or {}).get("language") */
        const json_t *display = json_obj_get(r, "display");
        json_t *owned_lang = NULL;
        if (i18n2_truthy(display)) {
            if (display->type != JSON_OBJECT) {
                json_free(r);   /* .get on a non-dict display: caught -> None */
                goto store;
            }
            const json_t *lang = json_obj_get(display, "language");
            if (lang) owned_lang = json_copy(lang);   /* SNAPSHOT: lang borrows from r */
        }
        json_free(r);
        if (i18n2_truthy(owned_lang)) {
            json_t *hj = json_string(hermes_home ? hermes_home : "");
            result = i18n2_normalize_lang(io, owned_lang, hj);   /* REAL compose */
            json_free(hj);
            if (i18n2_is_raise(result)) {
                if (i18n2_caught_by_exception(i18n2_raise_kind(result))) {
                    json_free(result);
                    result = NULL;
                } else {
                    json_free(owned_lang);
                    return result;   /* BaseException passes; nothing cached */
                }
            }
        }
        json_free(owned_lang);
    }
store:
    json_set(cache, hermes_home ? hermes_home : "", result ? json_copy(result) : json_null());
    return result;
}

/* PoP: i18n2_clear_cache @ agent/i18n_layers.py:clear_cache */
static json_t *i18n2_clear_cache(json_t *state) {
    i18n2_ensure_obj(state, "pack_cache");
    i18n2_ensure_obj(state, "overlay_cache");
    i18n2_ensure_obj(state, "overlay_files_cache");
    i18n2_obj_clear(json_obj_get(state, "pack_cache"));
    i18n2_obj_clear(json_obj_get(state, "overlay_cache"));
    i18n2_obj_clear(json_obj_get(state, "overlay_files_cache"));
    return NULL;
}

/* PoP: i18n2_reset_language_cache @ agent/i18n.py:reset_language_cache */
static json_t *i18n2_reset_language_cache(json_t *state) {
    /* _config_language_cached.cache_clear(); _bundled_surfaces.cache_clear() */
    i18n2_ensure_obj(state, "config_lang_cache");
    i18n2_obj_clear(json_obj_get(state, "config_lang_cache"));
    i18n2_ensure_obj(state, "bundled_surfaces_cache");
    i18n2_obj_clear(json_obj_get(state, "bundled_surfaces_cache"));
    /* with _catalog_lock: _catalog_cache.clear(); _supported_cache.clear() */
    i18n2_ensure_obj(state, "catalog_cache");
    i18n2_obj_clear(json_obj_get(state, "catalog_cache"));
    i18n2_ensure_obj(state, "supported_cache");
    i18n2_obj_clear(json_obj_get(state, "supported_cache"));
    /* i18n_layers.clear_cache() — REAL compose (no seam events) */
    return i18n2_clear_cache(state);
}

/* PoP: i18n2_get_language @ agent/i18n.py:get_language */
static json_t *i18n2_get_language(i18n2_io_t *io) {
    json_t *home = i18n2_call(io, "_current_home", json_object());
    if (i18n2_is_raise(home)) return home;
    json_t *a = json_object();
    if (home) json_set(a, "home", json_copy(home));
    json_t *r = i18n2_call(io, "_resolve_language", a);
    json_free(home);
    return r;
}

/* PoP: i18n2_t @ agent/i18n.py:t */
static json_t *i18n2_t(i18n2_io_t *io, const char *key, const json_t *lang,
                       const json_t *kwargs, json_t *state) {
    /* _catalog_cache is an always-existent module global: materialize it at
     * flow entry so the echoed state carries the key even when an early seam
     * raise (_current_home / resolve_language_id / _resolve_language) exits
     * before _load_catalog runs — matching the oracle's drive projection. */
    i18n2_ensure_obj(state, "catalog_cache");
    json_t *home = i18n2_call(io, "_current_home", json_object());
    if (i18n2_is_raise(home)) return home;
    json_t *target;
    if (i18n2_truthy(lang)) {
        target = i18n2_normalize_lang(io, lang, home);   /* REAL compose */
    } else {
        json_t *a = json_object();
        if (home) json_set(a, "home", json_copy(home));
        target = i18n2_call(io, "_resolve_language", a);
    }
    if (i18n2_is_raise(target)) { json_free(home); return target; }
    char *ts = i18n2_str_of(target);
    json_free(target);
    /* value = _load_catalog(target, home).get(key) */
    json_t *cat = i18n2_load_catalog(io, ts, home, state);
    if (i18n2_is_raise(cat)) { free(ts); json_free(home); return cat; }
    if (!cat || cat->type != JSON_OBJECT) {
        json_t *e = i18n2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                                    i18n2_type_name(cat));
        json_free(cat); free(ts); json_free(home);
        return e;
    }
    const json_t *value = json_obj_get(cat, key);
    int have = value && value->type != JSON_NULL;
    json_t *val_owned = have ? json_copy(value) : NULL;
    json_free(cat);
    if (!have && strcmp(ts, I18N2_DEFAULT_LANGUAGE) != 0) {
        json_t *cat2 = i18n2_load_catalog(io, I18N2_DEFAULT_LANGUAGE, home, state);
        if (i18n2_is_raise(cat2)) { free(ts); json_free(home); return cat2; }
        if (!cat2 || cat2->type != JSON_OBJECT) {
            json_t *e = i18n2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                                        i18n2_type_name(cat2));
            json_free(cat2); free(ts); json_free(home);
            return e;
        }
        value = json_obj_get(cat2, key);
        have = value && value->type != JSON_NULL;
        val_owned = have ? json_copy(value) : NULL;
        json_free(cat2);
    }
    if (!have) {
        /* logger.debug silent; value = key */
        val_owned = json_string(key ? key : "");
    }
    if (!kwargs || kwargs->type != JSON_OBJECT || kwargs->c.count == 0) {
        free(ts); json_free(home);
        return val_owned;
    }
    /* value.format(**format_kwargs) — only KeyError/IndexError/ValueError caught */
    if (val_owned->type != JSON_STRING) {
        json_t *e = i18n2_raise_fmt("AttributeError", "'%s' object has no attribute 'format'",
                                    i18n2_type_name(val_owned));
        json_free(val_owned); free(ts); json_free(home);
        return e;
    }
    json_t *fr = i18n2_format(val_owned->str_val, kwargs);
    if (i18n2_is_raise(fr)) {
        const char *k = i18n2_raise_kind(fr);
        if (strcmp(k, "KeyError") == 0 || strcmp(k, "IndexError") == 0 ||
            strcmp(k, "ValueError") == 0) {
            json_free(fr);   /* logger.warning silent; return unformatted value */
            free(ts); json_free(home);
            return val_owned;
        }
        json_free(val_owned); free(ts); json_free(home);
        return fr;
    }
    json_free(val_owned);
    free(ts); json_free(home);
    return fr;
}