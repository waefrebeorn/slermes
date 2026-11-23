/*
 * cli_output_flow.c — FLOW cores + leaves of the hermes_cli/cli_output.py
 * rendering cluster (LIVE authority) that NO existing PoP claims:
 *
 *   hermes_cli/cli_output.py : print_truncated            (the only unclaimed
 *       cli_output def; print_info/success/warning/error/header, line_input,
 *       prompt, prompt_yes_no are claimed elsewhere and enter as seams)
 *   hermes_cli/secret_prompt.py : _write                  (masked-prompt emit leaf)
 *   agent/i18n.py : _current_home, _config_language, _resolve_language,
 *       supported_languages, resolve_language_id, _load_bundled,
 *       surface_catalog, language_surfaces               (catalog resolution
 *       flows behind t(); t/_normalize_lang/_load_catalog/_locales_dir/
 *       _config_language_cached/get_language/reset_language_cache/_flatten_into
 *       are claimed and enter as seams)
 *   agent/i18n_layers.py : normalize_language_id, is_language_id, flatten,
 *       non_text_leaves, overlay_dir, load_locale_source, pack_layer,
 *       pack_info, overlay_layer, scan_locale_dir        (pure catalog leaves
 *       + pack/overlay state flows; the whole module is unclaimed)
 *
 * Self-contained C11, libjson-only. Every foreign collaborator and every
 * claimed sibling enters through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; co2_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is delivered
 * as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised exception).
 * The harness records {"ev": name, "args": args, "ret": result} per call in
 * order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported SAME-MODULE calls compose REAL (co2_current_home inside
 * co2_config_language / co2_supported_languages / co2_surface_catalog /
 * co2_language_surfaces; co2_supported_languages inside co2_resolve_language_id;
 * co2_load_bundled inside co2_surface_catalog; co2_normalize_language_id inside
 * co2_is_language_id and co2_resolve_language_id; co2_flatten inside
 * co2_load_locale_source; co2_overlay_dir inside co2_overlay_layer). The oracle
 * keeps those extracted defs real on the same paths, so their internal seam
 * events appear in BOTH traces. CROSS-MODULE calls are seamed even when the
 * callee is ported here for its own standalone cases (i18n_layers.overlay_layer
 * / pack_layer inside language_surfaces; i18n_layers.parse_locale_file inside
 * _load_bundled / overlay_layer / load_locale_source) — the oracle's
 * i18n_layers module double carries recorders for those names. The one
 * exception is i18n_layers.normalize_language_id inside resolve_language_id,
 * a pure leaf composed REAL on both sides (no seam events).
 *
 * Exception propagation: every co2 flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except Exception` (_load_bundled,
 * overlay_layer) swallows every non-BaseException kind; `except
 * UnscopedSecretError` (_resolve_language) swallows that kind only. The
 * BaseException trio (SystemExit / KeyboardInterrupt / GeneratorExit) and
 * BaseException itself ALWAYS pass through. The full builtin exception
 * parent table below drives co2_kind_is_base_only (unknown kinds are treated
 * as Exception subclasses, matching the oracle's synthetic make_exc).
 * Fail-closed: a missing/empty seam script yields a RuntimeError raise spec
 * (recorded, then propagated/swallowed exactly as live); no seam installed at
 * all yields the same spec.
 *
 * Locks (_catalog_lock / _lock) are single-threaded no-ops: they emit NO seam
 * events on either side (the oracle seeds silent context-manager doubles).
 * logger.debug/warning are silent on both sides (RecLogger).
 *
 * Ordered-collection modeling: live frozensets iterate in hash-randomized
 * order. The oracle materializes set-valued seam results (layered_languages,
 * overlay_surfaces, pack_surfaces, _bundled_surfaces) as OSet doubles whose
 * union iterates LEFT items first then NEW right items; C models the same
 * ordered dedup union. The only order-sensitive observable left is the
 * language_surfaces result dict's key insertion order, which the oracle's
 * canon() sorts away; per-language surface tuples stay in SURFACES order on
 * both sides. supported_languages sorts its result (ASCII strcmp == Python
 * sorted for lowercase language ids).
 *
 * State (injected json_t, mutated in place, echoed by the harness):
 *   supported_languages / resolve_language_id:
 *     {"supported_cache": {home: [lang, ...]}}      (_supported_cache)
 *   pack_layer / pack_info:
 *     {"packs": [{lang,surface,messages,source,endonym,rtl,token}, ...],
 *      "pack_cache": {"<lang>\x1f<surface>": {key: text}}}
 *   overlay_layer:
 *     {"overlay_cache": {"<home>\x1f<lang>\x1f<surface>": {key: text}}}
 *   flatten:
 *     out object (in.out) mutated in place; echoed as {"out": ...}.
 * Cache keys use the \x1f (unit separator) composite encoding; the oracle
 * converts to/from real tuple-keyed dicts. `cached is not None` is modeled as
 * "node present AND type != JSON_NULL" (an explicit null cache entry recomputes).
 *
 * Fixture domain (ASCII): paths are verbatim clean POSIX strings (no trailing
 * slash, no "//", no "."/".." segments; the oracle seeds Path=PathDouble, a
 * PurePosixPath subclass, so str()/join keep POSIX separators on Windows).
 * Path.is_file/is_dir/iterdir are seams carrying {"path": str}. iterdir scripts
 * are full path strings in one directory (strcmp sort == PurePosixPath sort
 * there). Numbers in str()-computed positions (more, get_hermes_home /
 * _normalize_lang / t results) are ints or strings — integral floats are kept
 * OUT of those positions (JSON cannot distinguish int from float, and live
 * str(5.0) == "5.0"). layered_languages / overlay_surfaces / pack_surfaces /
 * _bundled_surfaces scripts are arrays (a null there raises TypeError live).
 * parse_locale_file / layers.surface_catalog scripts are objects or raises
 * (dict.update(None) -> TypeError "'NoneType' object is not iterable" is
 * modeled defensively). load_locale_source non-mapping/non-str probes use
 * int/bool/null/list (type names int/bool/NoneType/list). _locales_dir scripts
 * are path strings (Path(None) TypeError modeled defensively).
 *
 * Seam name table (name -> live call):
 *   t                       agent.i18n.t(key, **kwargs) [claimed]
 *                           args{key, kwargs?}
 *   color                   hermes_cli.colors.color(text, *codes) [claimed]
 *                           args{text, codes:[...]}
 *   print                   builtin print(*values)  args{args:[...]}
 *   stdout.write            sys.stdout.write(text)  args{text}
 *   stdout.flush            sys.stdout.flush()      args{}
 *   get_hermes_home         hermes_constants.get_hermes_home()  args{}
 *   _config_language_cached agent.i18n._config_language_cached(home) [claimed]
 *                           args{hermes_home}
 *   _normalize_lang         agent.i18n._normalize_lang(value, home?) [claimed]
 *                           args{value, home?}
 *   get_secret              agent.secret_scope.get_secret(name)  args{name}
 *   environ.get             os.environ.get(key)  args{key}
 *   _locales_dir            agent.i18n._locales_dir() [claimed]  args{}
 *   path.is_file            Path.is_file()  args{path}
 *   path.is_dir             Path.is_dir()   args{path}
 *   path.iterdir            Path.iterdir()  args{path}
 *   parse_locale_file       agent.i18n_layers.parse_locale_file(path)  args{path}
 *   layered_languages       agent.i18n_layers.layered_languages(home)  args{home}
 *   overlay_surfaces        agent.i18n_layers.overlay_surfaces(home)  args{home}
 *   pack_surfaces           agent.i18n_layers.pack_surfaces()  args{}
 *   overlay_layer           agent.i18n_layers.overlay_layer(home,lang,surface)
 *                           args{home, lang, surface}
 *   pack_layer              agent.i18n_layers.pack_layer(lang,surface)
 *                           args{lang, surface}
 *   layers.surface_catalog  agent.i18n_layers.surface_catalog(home,lang,surface)
 *                           args{home, lang, surface}
 *   _bundled_surfaces       agent.i18n._bundled_surfaces(locales_dir)
 *                           args{locales_dir}
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include "hermes_json.h"

/* ── universal seam plumbing ── */

typedef json_t *(*co2_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct co2_io {
    co2_seam_fn call;
    void *ctx;
} co2_io_t;

static int co2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *co2_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *co2_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* Full builtin exception parent table (Python 3.14) + the quarry's
 * UnscopedSecretError(RuntimeError). Drives co2_kind_is_base_only. */
typedef struct co2_exc_parent { const char *name; const char *parent; } co2_exc_parent_t;
static const co2_exc_parent_t co2_exc_parents[] = {
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
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"UnscopedSecretError", "RuntimeError"},
};

static const char *co2_exc_parent_of(const char *name) {
    for (size_t i = 0; i < sizeof co2_exc_parents / sizeof co2_exc_parents[0]; i++) {
        if (strcmp(co2_exc_parents[i].name, name) == 0) return co2_exc_parents[i].parent;
    }
    return NULL;
}

/* BaseException-only kinds: NOT caught by `except Exception`. Walk the parent
 * chain; reaching Exception means caught, reaching BaseException without
 * Exception means base-only. Unknown kinds are synthetic Exception subclasses
 * (the oracle's make_exc) -> caught. */
static int co2_kind_is_base_only(const char *k) {
    const char *cur = k;
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cur, "Exception") == 0) return 0;
        if (strcmp(cur, "BaseException") == 0) return 1;
        const char *p = co2_exc_parent_of(cur);
        if (!p) return 0;
        cur = p;
    }
    return 0;
}

static int co2_caught_by_exception(const char *k) {
    return !co2_kind_is_base_only(k);
}

static json_t *co2_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *co2_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return co2_raise(kind, buf);
}

static char *co2_fmt(const char *fmt, ...) {
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
static json_t *co2_call(co2_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = co2_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int co2_truthy(const json_t *v) {
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

/* str() of a primitive seam result. Domain: strings, None, bools, ints.
 * Integral floats are kept OUT of str()-computed positions (see header). */
static char *co2_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) return co2_fmt("%.0f", v->num_val);
    return strdup("");
}

/* PurePosixPath join for the clean fixture domain (no "//", "." or "..").
 * An absolute right operand replaces the base; trailing slashes collapse. */
static char *co2_path_join(const char *base, const char *name) {
    if (!base) base = "";
    if (!name) name = "";
    if (name[0] == '/') return strdup(name);
    size_t bl = strlen(base);
    while (bl > 0 && base[bl - 1] == '/') bl--;
    if (bl == 0) {
        if (base[0] == '/') return co2_fmt("/%s", name);  /* root "/" */
        return strdup(name);                               /* empty base */
    }
    return co2_fmt("%.*s/%s", (int)bl, base, name);
}

static const char *co2_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* dict.update: overwrite values, keep first-insertion key position (libjson
 * json_set already keeps the first position with the last value). */
static void co2_obj_update(json_t *dst, const json_t *src) {
    if (!dst || !src || src->type != JSON_OBJECT) return;
    for (size_t i = 0; i < src->c.count; i++) {
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
    }
}

static int co2_arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && strcmp(v->str_val, s) == 0) return 1;
    }
    return 0;
}

/* The catalog/pack/overlay caches model ALWAYS-EXISTENT module globals
 * (_supported_cache / _pack_cache / _overlay_cache / _packs): materialize the
 * sub-object at flow entry so the echoed state carries the key even on an
 * early return/raise, matching the oracle's projection of the live global. */
static void co2_ensure_obj(json_t *state, const char *key) {
    if (state && !json_obj_get(state, key)) json_set(state, key, json_object());
}

/* ── embedded module constants (verbatim from the quarry; the oracle seeds
 *    the real constants from the AST and the shadow diff verifies parity) ── */

static const char *co2_SUPPORTED_LANGUAGES[] = {
    "en", "zh", "zh-hant", "ja", "de", "es", "fr", "tr", "uk",
    "af", "ko", "it", "ga", "pt", "ru", "hu", "ar",
};
#define CO2_N_SUPPORTED (sizeof co2_SUPPORTED_LANGUAGES / sizeof co2_SUPPORTED_LANGUAGES[0])

static const char *co2_SURFACES[] = { "core", "tui", "desktop" };
#define CO2_N_SURFACES (sizeof co2_SURFACES / sizeof co2_SURFACES[0])

#define CO2_DEFAULT_LANGUAGE "en"
#define CO2_CORE_SURFACE "core"
#define CO2_DIM "\x1b[2m"   /* Colors.DIM */

typedef struct co2_alias { const char *key; const char *val; } co2_alias_t;
static const co2_alias_t co2_LANGUAGE_ALIASES[] = {
    {"english", "en"},
    {"en-us", "en"},
    {"en-gb", "en"},
    {"chinese", "zh"},
    {"mandarin", "zh"},
    {"zh-cn", "zh"},
    {"zh-hans", "zh"},
    {"zh-sg", "zh"},
    {"traditional-chinese", "zh-hant"},
    {"traditional_chinese", "zh-hant"},
    {"zh-tw", "zh-hant"},
    {"zh-hk", "zh-hant"},
    {"zh-mo", "zh-hant"},
    {"japanese", "ja"},
    {"jp", "ja"},
    {"ja-jp", "ja"},
    {"german", "de"},
    {"deutsch", "de"},
    {"de-de", "de"},
    {"de-at", "de"},
    {"de-ch", "de"},
    {"spanish", "es"},
    {"espa\xc3\xb1ol", "es"},
    {"espanol", "es"},
    {"es-es", "es"},
    {"es-mx", "es"},
    {"es-ar", "es"},
    {"french", "fr"},
    {"fran\xc3\xa7" "ais", "fr"},
    {"france", "fr"},
    {"fr-fr", "fr"},
    {"fr-be", "fr"},
    {"fr-ca", "fr"},
    {"fr-ch", "fr"},
    {"ukrainian", "uk"},
    {"ukrainisch", "uk"},
    {"\xd1\x83\xd0\xba\xd1\x80\xd0\xb0\xd1\x97\xd0\xbd\xd1\x81\xd1\x8c\xd0\xba\xd0\xb0", "uk"},
    {"uk-ua", "uk"},
    {"ua", "uk"},
    {"turkish", "tr"},
    {"t\xc3\xbcrk\xc3\xa7" "e", "tr"},
    {"tr-tr", "tr"},
    {"afrikaans", "af"},
    {"af-za", "af"},
    {"korean", "ko"},
    {"\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", "ko"},
    {"ko-kr", "ko"},
    {"italian", "it"},
    {"italiano", "it"},
    {"it-it", "it"},
    {"it-ch", "it"},
    {"irish", "ga"},
    {"gaeilge", "ga"},
    {"ga-ie", "ga"},
    {"portuguese", "pt"},
    {"portugu\xc3\xaas", "pt"},
    {"portugues", "pt"},
    {"pt-pt", "pt"},
    {"pt-br", "pt"},
    {"brazilian", "pt"},
    {"brasileiro", "pt"},
    {"russian", "ru"},
    {"\xd1\x80\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9", "ru"},
    {"ru-ru", "ru"},
    {"hungarian", "hu"},
    {"magyar", "hu"},
    {"hu-hu", "hu"},
    {"arabic", "ar"},
    {"\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9", "ar"},
    {"ar-sa", "ar"},
    {"ar-eg", "ar"},
    {"ar-ae", "ar"},
    {"ar-ma", "ar"},
    {"ar-dz", "ar"},
};
#define CO2_N_ALIASES (sizeof co2_LANGUAGE_ALIASES / sizeof co2_LANGUAGE_ALIASES[0])

static const char *co2_alias_lookup(const char *key) {
    for (size_t i = 0; i < CO2_N_ALIASES; i++) {
        if (strcmp(co2_LANGUAGE_ALIASES[i].key, key) == 0) return co2_LANGUAGE_ALIASES[i].val;
    }
    return NULL;
}

/* ── pure leaves ── */

/* PoP: co2_normalize_language_id @ agent/i18n_layers.py:normalize_language_id */
/* normalize_language_id: str(value).strip().lower().replace("_","-") for str
 * inputs, else "". ASCII domain: strip removes ' \t\n\r\v\f', lower folds A-Z. */
static json_t *co2_normalize_language_id(const json_t *value) {
    if (!value || value->type != JSON_STRING) return json_string("");
    const char *s = value->str_val ? value->str_val : "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && strchr(" \t\n\r\v\f", s[a])) a++;
    while (b > a && strchr(" \t\n\r\v\f", s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return json_string("");
    size_t j = 0;
    for (size_t i = a; i < b; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == '_') c = '-';
        out[j++] = c;
    }
    out[j] = '\0';
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* _LANGUAGE_ID_RE: ^[a-z]{2,3}(-[a-z0-9]{2,8})*$ (full match; normalize strips
 * any trailing newline so the $-before-newline nuance never fires). */
static int co2_lang_id_match(const char *s) {
    size_t i = 0, n = strlen(s);
    size_t letters = 0;
    while (i < n && s[i] >= 'a' && s[i] <= 'z') { i++; letters++; }
    if (letters < 2 || letters > 3) return 0;
    while (i < n && s[i] == '-') {
        i++;
        size_t part = 0;
        while (i < n && ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9'))) { i++; part++; }
        if (part < 2 || part > 8) return 0;
    }
    return i == n;
}

/* PoP: co2_is_language_id @ agent/i18n_layers.py:is_language_id */
static json_t *co2_is_language_id(const json_t *value) {
    json_t *norm = co2_normalize_language_id(value);
    int m = co2_lang_id_match(norm->str_val ? norm->str_val : "");
    json_free(norm);
    return json_bool(m ? true : false);
}

/* PoP: co2_flatten @ agent/i18n_layers.py:flatten */
/* flatten: nested mapping -> {dotted.key: text}; non-str/non-mapping leaves
 * dropped. out (when given) is mutated in place and returned. */
static json_t *co2_flatten(const json_t *node, const char *prefix, json_t *out) {
    if (!out) out = json_object();
    if (!prefix) prefix = "";
    if (node && node->type == JSON_OBJECT) {
        for (size_t i = 0; i < node->c.count; i++) {
            const char *key = node->c.keys[i];
            char *child = (prefix[0]) ? co2_fmt("%s.%s", prefix, key) : strdup(key);
            co2_flatten(node->c.items[i], child, out);
            free(child);
        }
    } else if (node && node->type == JSON_STRING) {
        json_set(out, prefix, json_copy(node));
    }
    return out;
}

/* PoP: co2_non_text_leaves @ agent/i18n_layers.py:non_text_leaves */
/* non_text_leaves: dotted paths of leaves that are neither text nor mapping. */
static json_t *co2_non_text_leaves(const json_t *node, const char *prefix) {
    json_t *found = json_array();
    if (!prefix) prefix = "";
    if (node && node->type == JSON_OBJECT) {
        for (size_t i = 0; i < node->c.count; i++) {
            const char *key = node->c.keys[i];
            char *child = (prefix[0]) ? co2_fmt("%s.%s", prefix, key) : strdup(key);
            json_t *sub = co2_non_text_leaves(node->c.items[i], child);
            free(child);
            for (size_t j = 0; j < sub->c.count; j++) json_append(found, json_copy(sub->c.items[j]));
            json_free(sub);
        }
    } else if (!(node && node->type == JSON_STRING)) {
        json_append(found, json_string(prefix[0] ? prefix : "<root>"));
    }
    return found;
}

/* PoP: co2_overlay_dir @ agent/i18n_layers.py:overlay_dir */
/* overlay_dir: Path(home) / "locales" (clean POSIX domain). */
static json_t *co2_overlay_dir(const char *home) {
    char *p = co2_path_join(home ? home : "", "locales");
    json_t *r = json_string(p);
    free(p);
    return r;
}

/* _LOCALE_FILE_RE: ^(?P<lang>[a-z]{2,3}(?:-[a-z0-9]{2,8})*)(?:\.(?P<surface>[a-z]+))?\.ya?ml$
 * Greedy lang letters (2-3), maximal -region parts (2-8 alnum), then an
 * optional .surface (maximal lowercase run) tried BEFORE absence, then the
 * .yaml/.yml suffix. Returns 1 and mallocs *lang / *surface (NULL when absent). */
static int co2_locale_file_match(const char *name, char **lang, char **surface) {
    *lang = NULL; *surface = NULL;
    size_t i = 0, n = strlen(name);
    size_t letters = 0;
    while (i < n && name[i] >= 'a' && name[i] <= 'z') { i++; letters++; }
    if (letters < 2 || letters > 3) return 0;
    size_t lang_end = i;
    while (i < n && name[i] == '-') {
        size_t part_start = i + 1;
        size_t part = 0;
        i++;
        while (i < n && ((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9'))) { i++; part++; }
        if (part < 2 || part > 8) return 0;
        lang_end = i;
        (void)part_start;
    }
    /* optional surface group, then suffix */
    size_t surf_start = 0, surf_end = 0;
    int have_surf = 0;
    if (i < n && name[i] == '.') {
        size_t j = i + 1;
        while (j < n && name[j] >= 'a' && name[j] <= 'z') j++;
        if (j > i + 1) { surf_start = i + 1; surf_end = j; have_surf = 1; }
    }
    const char *suffix_ok_at = NULL;
    if (have_surf) {
        const char *rest = name + surf_end;
        if (strcmp(rest, ".yaml") == 0 || strcmp(rest, ".yml") == 0) suffix_ok_at = rest;
    }
    if (!suffix_ok_at) {
        const char *rest = name + lang_end;
        if (strcmp(rest, ".yaml") == 0 || strcmp(rest, ".yml") == 0) { suffix_ok_at = rest; have_surf = 0; }
    }
    if (!suffix_ok_at) return 0;
    *lang = strndup(name, lang_end);
    if (have_surf) *surface = strndup(name + surf_start, surf_end - surf_start);
    return 1;
}

/* ── sorted / dedup string array helpers ── */

static int co2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Collect array-of-strings into a malloc'd char* vector (skips non-strings). */
static char **co2_str_vec(const json_t *arr, size_t *out_n) {
    size_t cap = (arr && arr->type == JSON_ARRAY) ? arr->c.count : 0;
    char **v = (char **)malloc((cap + 1) * sizeof(char *));
    size_t n = 0;
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < arr->c.count; i++) {
            const json_t *s = arr->c.items[i];
            if (s && s->type == JSON_STRING) v[n++] = strdup(s->str_val);
        }
    }
    *out_n = n;
    return v;
}

static void co2_free_vec(char **v, size_t n) {
    for (size_t i = 0; i < n; i++) free(v[i]);
    free(v);
}

/* ── FLOW ports ── */

/* PoP: co2_print_truncated @ hermes_cli/cli_output.py:print_truncated */
static json_t *co2_print_truncated(co2_io_t *io, const json_t *more, const char *hint) {
    json_t *count;
    if (more && more->type != JSON_NULL) {
        char *cs = co2_str_of(more);
        json_t *ka = json_object();
        json_set(ka, "key", json_string("cli.shared.n_more"));
        json_t *kw = json_object();
        json_set(kw, "count", json_string(cs));
        free(cs);
        json_set(ka, "kwargs", kw);
        count = co2_call(io, "t", ka);
    } else {
        json_t *ka = json_object();
        json_set(ka, "key", json_string("cli.shared.more_not_shown"));
        count = co2_call(io, "t", ka);
    }
    if (co2_is_raise(count)) return count;
    char *suffix = (hint && hint[0]) ? co2_fmt(" (%s)", hint) : strdup("");
    char *cnts = co2_str_of(count);
    char *text = co2_fmt("  \xe2\x80\xa6 %s%s", cnts, suffix);
    json_t *ca = json_object();
    json_set(ca, "text", json_string(text));
    json_t *codes = json_array();
    json_append(codes, json_string(CO2_DIM));
    json_set(ca, "codes", codes);
    json_t *colored = co2_call(io, "color", ca);
    free(suffix); free(cnts); free(text);
    if (co2_is_raise(colored)) { json_free(count); return colored; }
    json_t *pa = json_object();
    json_t *args = json_array();
    json_append(args, colored ? json_copy(colored) : json_null());
    json_set(pa, "args", args);
    json_t *pr = co2_call(io, "print", pa);
    json_free(count);
    json_free(colored);
    return pr;   /* NULL (None) on success, raise spec on print failure */
}

/* PoP: co2_write @ hermes_cli/secret_prompt.py:_write */
static json_t *co2_write(co2_io_t *io, const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_t *r = co2_call(io, "stdout.write", a);
    if (co2_is_raise(r)) return r;
    json_free(r);
    r = co2_call(io, "stdout.flush", json_object());
    if (co2_is_raise(r)) return r;
    json_free(r);
    return NULL;   /* _write returns None; write/flush results discarded */
}

/* PoP: co2_current_home @ agent/i18n.py:_current_home */
static json_t *co2_current_home(co2_io_t *io) {
    json_t *r = co2_call(io, "get_hermes_home", json_object());
    if (co2_is_raise(r)) return r;
    char *s = co2_str_of(r);
    json_free(r);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: co2_config_language @ agent/i18n.py:_config_language */
static json_t *co2_config_language(co2_io_t *io) {
    json_t *home = co2_current_home(io);
    if (co2_is_raise(home)) return home;
    json_t *a = json_object();
    json_set(a, "hermes_home", json_copy(home));
    json_t *r = co2_call(io, "_config_language_cached", a);
    json_free(home);
    return r;
}

/* PoP: co2_resolve_language @ agent/i18n.py:_resolve_language */
static json_t *co2_resolve_language(co2_io_t *io, const char *home) {
    json_t *a = json_object();
    json_set(a, "name", json_string("HERMES_LANGUAGE"));
    json_t *env_lang = co2_call(io, "get_secret", a);
    if (co2_is_raise(env_lang)) {
        if (strcmp(co2_raise_kind(env_lang), "UnscopedSecretError") == 0) {
            json_free(env_lang);
            json_t *ea = json_object();
            json_set(ea, "key", json_string("HERMES_LANGUAGE"));
            env_lang = co2_call(io, "environ.get", ea);
            if (co2_is_raise(env_lang)) return env_lang;
        } else {
            return env_lang;
        }
    }
    json_t *ret;
    if (co2_truthy(env_lang)) {
        json_t *na = json_object();
        json_set(na, "value", json_copy(env_lang));
        json_set(na, "home", json_string(home ? home : ""));
        ret = co2_call(io, "_normalize_lang", na);
    } else {
        json_t *ca = json_object();
        json_set(ca, "hermes_home", json_string(home ? home : ""));
        json_t *r = co2_call(io, "_config_language_cached", ca);
        if (co2_is_raise(r)) { json_free(env_lang); return r; }
        if (co2_truthy(r)) {
            ret = r;
        } else {
            json_free(r);
            ret = json_string(CO2_DEFAULT_LANGUAGE);
        }
    }
    json_free(env_lang);
    return ret;
}

/* PoP: co2_supported_languages @ agent/i18n.py:supported_languages */
/* supported_languages: home or _current_home(); cache lookup; union of
 * SUPPORTED_LANGUAGES and the layered_languages seam result; ("en", *sorted(rest)). */
static json_t *co2_supported_languages(co2_io_t *io, const char *home, json_t *state) {
    co2_ensure_obj(state, "supported_cache");
    char *h;
    if (home && home[0]) {
        h = strdup(home);
    } else {
        json_t *hh = co2_current_home(io);
        if (co2_is_raise(hh)) return hh;
        h = co2_str_of(hh);
        json_free(hh);
    }
    json_t *cache = state ? json_obj_get(state, "supported_cache") : NULL;
    json_t *cached = cache ? json_obj_get(cache, h) : NULL;
    if (cached && cached->type != JSON_NULL) { free(h); return json_copy(cached); }
    json_t *la = json_object();
    json_set(la, "home", json_string(h));
    json_t *layered = co2_call(io, "layered_languages", la);
    if (co2_is_raise(layered)) { free(h); return layered; }
    if (!layered || layered->type != JSON_ARRAY) {
        json_free(layered); free(h);
        return co2_raise("TypeError", "unsupported operand type(s) for |: 'set' and 'NoneType'");
    }
    /* union dedup */
    size_t nl = 0;
    char **lv = co2_str_vec(layered, &nl);
    json_free(layered);
    size_t cap = CO2_N_SUPPORTED + nl;
    char **all = (char **)malloc((cap + 1) * sizeof(char *));
    size_t na = 0;
    for (size_t i = 0; i < CO2_N_SUPPORTED; i++) all[na++] = strdup(co2_SUPPORTED_LANGUAGES[i]);
    for (size_t i = 0; i < nl; i++) {
        int dup = 0;
        for (size_t j = 0; j < na; j++) if (strcmp(all[j], lv[i]) == 0) { dup = 1; break; }
        if (!dup) all[na++] = strdup(lv[i]);
    }
    co2_free_vec(lv, nl);
    qsort(all, na, sizeof(char *), co2_cmp_str);
    json_t *result = json_array();
    json_append(result, json_string(CO2_DEFAULT_LANGUAGE));
    for (size_t i = 0; i < na; i++) {
        if (strcmp(all[i], CO2_DEFAULT_LANGUAGE) != 0) {
            if (i == 0 || strcmp(all[i], all[i - 1]) != 0) json_append(result, json_string(all[i]));
        }
    }
    co2_free_vec(all, na);
    if (state) {
        if (!cache) { cache = json_object(); json_set(state, "supported_cache", cache); }
        json_set(cache, h, json_copy(result));
    }
    free(h);
    return result;
}

/* PoP: co2_resolve_language_id @ agent/i18n.py:resolve_language_id */
static json_t *co2_resolve_language_id(co2_io_t *io, const json_t *value, const char *home, json_t *state) {
    co2_ensure_obj(state, "supported_cache");
    json_t *kj = co2_normalize_language_id(value);
    char *key = strdup(kj->str_val ? kj->str_val : "");
    json_free(kj);
    if (!key[0]) { free(key); return NULL; }
    json_t *sup = co2_supported_languages(io, home, state);
    if (co2_is_raise(sup)) { free(key); return sup; }
    json_t *ret = NULL;
    if (co2_arr_has_str(sup, key)) {
        ret = json_string(key);
    } else {
        const char *alias = co2_alias_lookup(key);
        if (alias && co2_arr_has_str(sup, alias)) {
            ret = json_string(alias);
        } else {
            char *dash = strchr(key, '-');
            char *base = dash ? strndup(key, (size_t)(dash - key)) : strdup(key);
            if (co2_arr_has_str(sup, base)) ret = json_string(base);
            free(base);
        }
    }
    json_free(sup);
    free(key);
    return ret;
}

/* PoP: co2_load_bundled @ agent/i18n.py:_load_bundled */
static json_t *co2_load_bundled(co2_io_t *io, const char *lang) {
    json_t *dir = co2_call(io, "_locales_dir", json_object());
    if (co2_is_raise(dir)) return dir;
    if (!dir) {
        return co2_raise("TypeError",
            "argument should be a str or an os.PathLike object where __fspath__ returns a str, not 'NoneType'");
    }
    char *dirs = co2_str_of(dir);
    json_free(dir);
    char *name = co2_fmt("%s.yaml", lang ? lang : "");
    char *path = co2_path_join(dirs, name);
    free(dirs); free(name);
    json_t *fa = json_object();
    json_set(fa, "path", json_string(path));
    json_t *isf = co2_call(io, "path.is_file", fa);
    if (co2_is_raise(isf)) { free(path); return isf; }
    int file = co2_truthy(isf);
    json_free(isf);
    if (!file) { free(path); return json_object(); }
    json_t *pa = json_object();
    json_set(pa, "path", json_string(path));
    free(path);
    json_t *r = co2_call(io, "parse_locale_file", pa);
    if (co2_is_raise(r)) {
        if (co2_caught_by_exception(co2_raise_kind(r))) { json_free(r); return json_object(); }
        return r;
    }
    return r;
}

/* PoP: co2_surface_catalog @ agent/i18n.py:surface_catalog */
static json_t *co2_surface_catalog(co2_io_t *io, const char *lang, const char *surface) {
    json_t *na = json_object();
    json_set(na, "value", json_string(lang ? lang : ""));
    json_t *lj = co2_call(io, "_normalize_lang", na);
    if (co2_is_raise(lj)) return lj;
    char *l = co2_str_of(lj);
    json_free(lj);
    json_t *merged = json_object();
    if (strcmp(surface ? surface : CO2_CORE_SURFACE, CO2_CORE_SURFACE) != 0) {
        char *bl = co2_fmt("%s.%s", l, surface);
        json_t *r = co2_load_bundled(io, bl);
        free(bl);
        if (co2_is_raise(r)) { free(l); json_free(merged); return r; }
        if (!r) { free(l); json_free(merged); return co2_raise("TypeError", "'NoneType' object is not iterable"); }
        if (r->type != JSON_OBJECT) {
            free(l); json_free(merged); json_free(r);
            return co2_raise("ValueError", "dictionary update sequence element #0 has length 1; 2 is required");
        }
        co2_obj_update(merged, r);
        json_free(r);
    }
    json_t *home = co2_current_home(io);
    if (co2_is_raise(home)) { free(l); json_free(merged); return home; }
    json_t *sa = json_object();
    json_set(sa, "home", json_copy(home));
    json_set(sa, "lang", json_string(l));
    json_set(sa, "surface", json_string(surface ? surface : CO2_CORE_SURFACE));
    json_t *r2 = co2_call(io, "layers.surface_catalog", sa);
    json_free(home);
    free(l);
    if (co2_is_raise(r2)) { json_free(merged); return r2; }
    if (!r2) { json_free(merged); return co2_raise("TypeError", "'NoneType' object is not iterable"); }
    if (r2->type != JSON_OBJECT) {
        json_free(merged); json_free(r2);
        return co2_raise("ValueError", "dictionary update sequence element #0 has length 1; 2 is required");
    }
    co2_obj_update(merged, r2);
    json_free(r2);
    return merged;
}

/* ordered dedup union of two pair arrays ([lang, surface] elements).
 * Returns a malloc'd vector of borrowed-or-copied pair nodes; caller frees the
 * vector and the copied nodes. Left items first, then new right items. */
typedef struct co2_pair { char *lang; char *surface; } co2_pair_t;

static co2_pair_t *co2_pair_union(const json_t *left, const json_t *right, size_t *out_n) {
    size_t cap = ((left && left->type == JSON_ARRAY) ? left->c.count : 0)
               + ((right && right->type == JSON_ARRAY) ? right->c.count : 0);
    co2_pair_t *v = (co2_pair_t *)malloc((cap + 1) * sizeof(co2_pair_t));
    size_t n = 0;
    const json_t *srcs[2] = { left, right };
    for (int s = 0; s < 2; s++) {
        const json_t *arr = srcs[s];
        if (!arr || arr->type != JSON_ARRAY) continue;
        for (size_t i = 0; i < arr->c.count; i++) {
            const json_t *p = arr->c.items[i];
            if (!p || p->type != JSON_ARRAY || p->c.count < 2) continue;
            const json_t *lj = p->c.items[0];
            const json_t *sj = p->c.items[1];
            if (!lj || lj->type != JSON_STRING || !sj || sj->type != JSON_STRING) continue;
            int dup = 0;
            for (size_t j = 0; j < n; j++) {
                if (strcmp(v[j].lang, lj->str_val) == 0 && strcmp(v[j].surface, sj->str_val) == 0) { dup = 1; break; }
            }
            if (!dup) { v[n].lang = strdup(lj->str_val); v[n].surface = strdup(sj->str_val); n++; }
        }
    }
    *out_n = n;
    return v;
}

static void co2_free_pairs(co2_pair_t *v, size_t n) {
    for (size_t i = 0; i < n; i++) { free(v[i].lang); free(v[i].surface); }
    free(v);
}

/* PoP: co2_language_surfaces @ agent/i18n.py:language_surfaces */
static json_t *co2_language_surfaces(co2_io_t *io, const char *home) {
    char *h;
    if (home && home[0]) {
        h = strdup(home);
    } else {
        json_t *hh = co2_current_home(io);
        if (co2_is_raise(hh)) return hh;
        h = co2_str_of(hh);
        json_free(hh);
    }
    json_t *oa = json_object();
    json_set(oa, "home", json_string(h));
    json_t *os = co2_call(io, "overlay_surfaces", oa);
    if (co2_is_raise(os)) { free(h); return os; }
    json_t *ps = co2_call(io, "pack_surfaces", json_object());
    if (co2_is_raise(ps)) { free(h); json_free(os); return ps; }
    if (!os || os->type != JSON_ARRAY || !ps || ps->type != JSON_ARRAY) {
        free(h); json_free(os); json_free(ps);
        return co2_raise("TypeError", "unsupported operand type(s) for |: 'NoneType' and 'frozenset'");
    }
    size_t nu = 0;
    co2_pair_t *u = co2_pair_union(os, ps, &nu);
    json_free(os); json_free(ps);
    /* layered comprehension: include pair when overlay_layer truthy, else pack_layer truthy */
    co2_pair_t *layered = (co2_pair_t *)malloc((nu + 1) * sizeof(co2_pair_t));
    size_t nl = 0;
    for (size_t i = 0; i < nu; i++) {
        json_t *ola = json_object();
        json_set(ola, "home", json_string(h));
        json_set(ola, "lang", json_string(u[i].lang));
        json_set(ola, "surface", json_string(u[i].surface));
        json_t *ol = co2_call(io, "overlay_layer", ola);
        if (co2_is_raise(ol)) {
            co2_free_pairs(u, nu); co2_free_pairs(layered, nl); free(h);
            return ol;
        }
        int include = co2_truthy(ol);
        json_free(ol);
        if (!include) {
            json_t *pla = json_object();
            json_set(pla, "lang", json_string(u[i].lang));
            json_set(pla, "surface", json_string(u[i].surface));
            json_t *pl = co2_call(io, "pack_layer", pla);
            if (co2_is_raise(pl)) {
                co2_free_pairs(u, nu); co2_free_pairs(layered, nl); free(h);
                return pl;
            }
            include = co2_truthy(pl);
            json_free(pl);
        }
        if (include) { layered[nl].lang = strdup(u[i].lang); layered[nl].surface = strdup(u[i].surface); nl++; }
    }
    co2_free_pairs(u, nu);
    json_t *ld = co2_call(io, "_locales_dir", json_object());
    if (co2_is_raise(ld)) { co2_free_pairs(layered, nl); free(h); return ld; }
    char *lds = co2_str_of(ld);
    json_free(ld);
    json_t *ba = json_object();
    json_set(ba, "locales_dir", json_string(lds));
    free(lds);
    json_t *bs = co2_call(io, "_bundled_surfaces", ba);
    if (co2_is_raise(bs)) { co2_free_pairs(layered, nl); free(h); return bs; }
    if (!bs || bs->type != JSON_ARRAY) {
        co2_free_pairs(layered, nl); free(h); json_free(bs);
        return co2_raise("TypeError", "unsupported operand type(s) for |: 'NoneType' and 'set'");
    }
    /* pairs = bundled | layered (ordered dedup); found[lang] = surfaces */
    json_t *layered_arr = json_array();
    for (size_t i = 0; i < nl; i++) {
        json_t *p = json_array();
        json_append(p, json_string(layered[i].lang));
        json_append(p, json_string(layered[i].surface));
        json_append(layered_arr, p);
    }
    co2_free_pairs(layered, nl);
    size_t np = 0;
    co2_pair_t *pairs = co2_pair_union(bs, layered_arr, &np);
    json_free(bs); json_free(layered_arr);
    json_t *found = json_object();
    for (size_t i = 0; i < np; i++) {
        json_t *surfs = json_obj_get(found, pairs[i].lang);
        if (!surfs) { surfs = json_array(); json_set(found, pairs[i].lang, surfs); }
        if (!co2_arr_has_str(surfs, pairs[i].surface)) json_append(surfs, json_string(pairs[i].surface));
    }
    co2_free_pairs(pairs, np);
    /* per-language surfaces in SURFACES order */
    for (size_t i = 0; i < found->c.count; i++) {
        json_t *surfs = found->c.items[i];
        json_t *ordered = json_array();
        for (size_t s = 0; s < CO2_N_SURFACES; s++) {
            if (co2_arr_has_str(surfs, co2_SURFACES[s])) json_append(ordered, json_string(co2_SURFACES[s]));
        }
        json_free(surfs);
        found->c.items[i] = ordered;
    }
    free(h);
    return found;
}

/* PoP: co2_load_locale_source @ agent/i18n_layers.py:load_locale_source */
static json_t *co2_load_locale_source(co2_io_t *io, const json_t *source) {
    if (source && source->type == JSON_OBJECT) {
        return co2_flatten(source, "", NULL);
    }
    if (source && source->type == JSON_STRING) {
        json_t *pa = json_object();
        json_set(pa, "path", json_string(source->str_val));
        return co2_call(io, "parse_locale_file", pa);
    }
    const char *tn;
    if (!source || source->type == JSON_NULL) tn = "NoneType";
    else if (source->type == JSON_BOOL) tn = "bool";
    else if (source->type == JSON_NUMBER) tn = "int";
    else if (source->type == JSON_ARRAY) tn = "list";
    else tn = "object";
    return co2_raise_fmt("TypeError",
        "locale source must be a mapping or a path to a YAML file, got %s", tn);
}

/* PoP: co2_pack_layer @ agent/i18n_layers.py:pack_layer */
/* pack_layer: merged pack messages for (lang, surface) in registration order
 * (last wins); cached in state.pack_cache. No seams (state-only flow). */
static json_t *co2_pack_layer(const char *lang, const char *surface, json_t *state) {
    co2_ensure_obj(state, "pack_cache");
    if (state && !json_obj_get(state, "packs")) json_set(state, "packs", json_array());
    char *key = co2_fmt("%s\x1f%s", lang ? lang : "", surface ? surface : "");
    json_t *cache = state ? json_obj_get(state, "pack_cache") : NULL;
    json_t *cached = cache ? json_obj_get(cache, key) : NULL;
    if (cached && cached->type != JSON_NULL) { free(key); return json_copy(cached); }
    json_t *merged = json_object();
    json_t *packs = state ? json_obj_get(state, "packs") : NULL;
    if (packs && packs->type == JSON_ARRAY) {
        for (size_t i = 0; i < packs->c.count; i++) {
            const json_t *e = packs->c.items[i];
            if (!e || e->type != JSON_OBJECT) continue;
            const char *el = json_get_str(e, "lang", "");
            const char *es = json_get_str(e, "surface", "");
            if (strcmp(el, lang ? lang : "") == 0 && strcmp(es, surface ? surface : "") == 0) {
                const json_t *msgs = json_obj_get(e, "messages");
                if (msgs && msgs->type == JSON_OBJECT) co2_obj_update(merged, msgs);
            }
        }
    }
    if (state) {
        if (!cache) { cache = json_object(); json_set(state, "pack_cache", cache); }
        json_set(cache, key, json_copy(merged));
    }
    free(key);
    return merged;
}

/* PoP: co2_pack_info @ agent/i18n_layers.py:pack_info */
static json_t *co2_pack_info(const char *lang, json_t *state) {
    if (state && !json_obj_get(state, "packs")) json_set(state, "packs", json_array());
    json_t *packs = state ? json_obj_get(state, "packs") : NULL;
    const json_t *entries[256];
    size_t ne = 0;
    if (packs && packs->type == JSON_ARRAY) {
        for (size_t i = 0; i < packs->c.count && ne < 256; i++) {
            const json_t *e = packs->c.items[i];
            if (e && e->type == JSON_OBJECT && strcmp(json_get_str(e, "lang", ""), lang ? lang : "") == 0) {
                entries[ne++] = e;
            }
        }
    }
    if (ne == 0) return NULL;
    /* endonym: newest truthy endonym */
    json_t *endonym = NULL;
    for (size_t i = ne; i > 0; i--) {
        const json_t *e = entries[i - 1];
        const json_t *en = json_obj_get(e, "endonym");
        if (co2_truthy(en)) { endonym = json_copy(en); break; }
    }
    /* rtl: newest entry whose endonym is not None (incl ""), else last entry's rtl */
    int rtl = json_get_bool(entries[ne - 1], "rtl", false);
    for (size_t i = ne; i > 0; i--) {
        const json_t *e = entries[i - 1];
        const json_t *en = json_obj_get(e, "endonym");
        if (en && en->type != JSON_NULL) { rtl = json_get_bool(e, "rtl", false); break; }
    }
    json_t *ret = json_object();
    json_set(ret, "endonym", endonym ? endonym : json_null());
    json_set(ret, "rtl", json_bool(rtl ? true : false));
    json_set(ret, "source", json_string(json_get_str(entries[ne - 1], "source", "")));
    return ret;
}

/* PoP: co2_overlay_layer @ agent/i18n_layers.py:overlay_layer */
static json_t *co2_overlay_layer(co2_io_t *io, const char *home, const char *lang, const char *surface, json_t *state) {
    co2_ensure_obj(state, "overlay_cache");
    char *key = co2_fmt("%s\x1f%s\x1f%s", home ? home : "", lang ? lang : "", surface ? surface : "");
    json_t *cache = state ? json_obj_get(state, "overlay_cache") : NULL;
    json_t *cached = cache ? json_obj_get(cache, key) : NULL;
    if (cached && cached->type != JSON_NULL) { free(key); return json_copy(cached); }
    char *name = (strcmp(surface ? surface : CO2_CORE_SURFACE, CO2_CORE_SURFACE) == 0)
        ? co2_fmt("%s.yaml", lang ? lang : "")
        : co2_fmt("%s.%s.yaml", lang ? lang : "", surface);
    json_t *od = co2_overlay_dir(home);
    char *path = co2_path_join(od->str_val, name);
    json_free(od); free(name);
    json_t *fa = json_object();
    json_set(fa, "path", json_string(path));
    json_t *isf = co2_call(io, "path.is_file", fa);
    if (co2_is_raise(isf)) { free(key); free(path); return isf; }
    int file = co2_truthy(isf);
    json_free(isf);
    json_t *flat = json_object();
    if (file) {
        json_t *pa = json_object();
        json_set(pa, "path", json_string(path));
        json_t *r = co2_call(io, "parse_locale_file", pa);
        if (co2_is_raise(r)) {
            if (co2_caught_by_exception(co2_raise_kind(r))) {
                json_free(r);   /* flat stays {} */
            } else {
                json_free(flat); free(key); free(path);
                return r;       /* BaseException: NO cache write */
            }
        } else {
            json_free(flat);
            flat = r;           /* may be NULL (scripted null) -> cached as None */
        }
    }
    free(path);
    if (state) {
        if (!cache) { cache = json_object(); json_set(state, "overlay_cache", cache); }
        json_set(cache, key, flat ? json_copy(flat) : json_null());
    }
    free(key);
    return flat;
}

/* PurePosixPath(directory) normalization: the oracle injects PathDouble
 * (a PurePosixPath), so path.is_dir / path.iterdir seams record str(self) with
 * duplicate separators collapsed, '.' dropped and the trailing separator
 * removed (a leading exactly-two slashes stays '//'). Match that on the C side. */
static char *co2_pureposix_posix(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), i = 0;
    while (i < n && s[i] == '/') i++;
    const char *lead = (i == 2) ? "//" : (i > 0) ? "/" : "";
    char *out = (char *)calloc(n + 3, 1);
    if (!out) return strdup(".");
    size_t k = 0;
    strcpy(out, lead); k = strlen(lead);
    size_t p = i;
    while (p < n) {
        while (p < n && s[p] == '/') p++;
        size_t st = p;
        while (p < n && s[p] != '/') p++;
        size_t len = p - st;
        if (!len || (len == 1 && s[st] == '.')) continue;
        if (k && out[k - 1] != '/') out[k++] = '/';
        memcpy(out + k, s + st, len); k += len;
    }
    if (!k) out[k++] = '.';   /* PurePosixPath("") == Path(".") */
    out[k] = '\0';
    return out;
}

/* PoP: co2_scan_locale_dir @ agent/i18n_layers.py:scan_locale_dir */
static json_t *co2_scan_locale_dir(co2_io_t *io, const char *directory) {
    char *dir = co2_pureposix_posix(directory);
    json_t *da = json_object();
    json_set(da, "path", json_string(dir));
    json_t *isd = co2_call(io, "path.is_dir", da);
    if (co2_is_raise(isd)) { free(dir); return isd; }
    int isdir = co2_truthy(isd);
    json_free(isd);
    if (!isdir) { free(dir); return json_array(); }
    json_t *ia = json_object();
    json_set(ia, "path", json_string(dir));
    json_t *it = co2_call(io, "path.iterdir", ia);
    if (co2_is_raise(it)) { free(dir); return it; }
    if (!it || it->type != JSON_ARRAY) { json_free(it); free(dir); return json_array(); }
    size_t n = 0;
    char **paths = co2_str_vec(it, &n);
    json_free(it);
    qsort(paths, n, sizeof(char *), co2_cmp_str);
    json_t *found = json_array();
    for (size_t i = 0; i < n; i++) {
        const char *bname = co2_basename(paths[i]);
        char *lang = NULL, *surface = NULL;
        if (!co2_locale_file_match(bname, &lang, &surface)) continue;
        json_t *fa = json_object();
        json_set(fa, "path", json_string(paths[i]));
        json_t *isf = co2_call(io, "path.is_file", fa);
        if (co2_is_raise(isf)) {
            free(lang); free(surface);
            co2_free_vec(paths, n);
            json_free(found);
            free(dir);
            return isf;
        }
        int file = co2_truthy(isf);
        json_free(isf);
        if (!file) { free(lang); free(surface); continue; }
        char *surf = surface ? surface : strdup(CO2_CORE_SURFACE);
        int known = 0;
        for (size_t s = 0; s < CO2_N_SURFACES; s++) if (strcmp(surf, co2_SURFACES[s]) == 0) { known = 1; break; }
        if (!known) { free(lang); free(surface); if (surf != surface) free(surf); continue; }
        json_t *triple = json_array();
        json_append(triple, json_string(lang));
        json_append(triple, json_string(surf));
        json_append(triple, json_string(paths[i]));
        json_append(found, triple);
        free(lang); free(surface); if (surf != surface) free(surf);
    }
    co2_free_vec(paths, n);
    free(dir);
    return found;
}
