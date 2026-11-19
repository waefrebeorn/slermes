/* status_phrases_flow.c — FLOW-core port of gateway/status_phrases.py.
 *
 * Self-contained, libjson-only. Every live external call (YAML load, the
 * filesystem-touching pathlib.Path methods, hermes_constants.get_hermes_home,
 * random.choice) enters through ONE universal seam; the pure decision core
 * (phrase cleaning, catalog merge order, surface classification, recent-repeat
 * avoidance, posix path algebra) is transcribed inline. No real I/O happens
 * inside the ported core.
 *
 * Claim policy
 * ------------
 * gateway/status_phrases.py has 10 top-level defs. Nine are already claimed by
 * the legacy wrapper stubs in src/gateway/status_phrases.c (grep PoP). Per the
 * flow-port doctrine (claimed siblings are not re-claimed; pure leaves are
 * transcribed inline without a PoP re-claim) this revival implements the full
 * call graph as static sph_* helpers and PoP-credits ONLY the genuinely
 * unclaimed def, _copy_catalog. The legacy claims are zero-arg printf stubs;
 * this flow port revives the real behaviour and is verified differentially by
 * tests/oracle_status_phrases_flow.py against the LIVE quarry defs.
 *
 * Seam model
 * ----------
 * sph_io_t.call(io, name, args) dispatches a live call by name:
 *   - sph_call appends {"name": name, "args": args} to io->trace (ordered) so
 *     the decision SEQUENCE can be compared against live Python under scripted
 *     I/O;
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - a raised exception is modeled {"__raise__": "<Type>", "message": "..."}.
 * args passed to sph_call are BORROWED (sph_call copies what it records); the
 * caller frees args after the call. Inputs are borrowed. Catalog/recent state
 * objects are mutated in place; the harness owns them.
 *
 * Live counterpart of each seam name:
 *   get_hermes_home     hermes_constants.get_hermes_home() -> Path
 *   Path.expanduser     pathlib.Path.expanduser()
 *   Path.resolve        pathlib.Path.resolve()
 *   Path.is_file        pathlib.Path.is_file()
 *   Path.is_dir         pathlib.Path.is_dir()
 *   Path.iterdir        pathlib.Path.iterdir() -> [Path]
 *   Path.read_text      pathlib.Path.read_text(encoding="utf-8-sig")
 *   yaml.safe_load      hermes_yaml.safe_load(text)
 *   random.choice       (rng or _random).choice(candidates)
 *
 * Representation contract (shared with the oracle)
 * ------------------------------------------------
 * - Path is modeled as a POSIX path string. The oracle drives the live defs
 *   with pathlib.PurePosixPath (posix semantics, deterministic cross-platform)
 *   whose filesystem methods are seamed; pure path algebra (parts, suffix,
 *   is_absolute, parent, join, relative_to) is transcribed here to match
 *   PurePosixPath exactly (probe-verified on py3.14.9).
 * - Paths reach traces/results as posix strings (as_posix).
 * - str() projection is ASCII-scoped; fixtures keep str()-projected positions
 *   (item / kind / mode / raw_path) free of float values (Python str(2.0) keeps
 *   ".0"; integral ints project as decimal). strip()/lower() are ASCII.
 * - Integral floats normalize to ints on both sides before comparison.
 * - `except Exception` (in _merge_phrase_file) swallows Exception subclasses
 *   but lets BaseException-only types (SystemExit / KeyboardInterrupt /
 *   GeneratorExit) propagate. sph_is_exception_subclass walks the FULL builtin
 *   exception hierarchy to decide; unknown names default to an Exception
 *   subclass (matching the oracle's synthetic exception class).
 * - _DEFAULT_PHRASES (a module global built at import from _FALLBACK_PHRASES +
 *   the bundled assets YAML) is injected per case as in.default_phrases; the
 *   module-level construction itself is ported as sph_load_default_phrases.
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* ── universal seam ────────────────────────────────────────────────────── */
typedef struct sph_io_t {
    json_t *(*call)(struct sph_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* ordered JSON array of {"name","args"} */
    void   *ctx;     /* harness script backend */
} sph_io_t;

static json_t *sph_call(sph_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}

static bool sph_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *raise_desc(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}

/* ── FULL builtin exception hierarchy (name -> parent) ─────────────────── */
typedef struct { const char *name; const char *parent; } exc_parent_t;
static const exc_parent_t EXC_PARENTS[] = {
    {"BaseException", ""},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
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
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
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
    {"ExceptionGroup", "Exception"},
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
#define EXC_PARENT_COUNT (sizeof(EXC_PARENTS) / sizeof(EXC_PARENTS[0]))

static const char *exc_parent_of(const char *name) {
    for (size_t i = 0; i < EXC_PARENT_COUNT; i++)
        if (strcmp(EXC_PARENTS[i].name, name) == 0) return EXC_PARENTS[i].parent;
    return "Exception";  /* unknown -> synthetic Exception subclass (oracle parity) */
}

/* True when `except Exception` catches this type (Exception subclass).
 * BaseException-only types (SystemExit/KeyboardInterrupt/GeneratorExit/
 * BaseExceptionGroup/BaseException) propagate. */
static bool sph_is_exception_subclass(const char *name) {
    if (!name) return true;
    const char *cur = name;
    for (int depth = 0; depth < 64; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        const char *parent = exc_parent_of(cur);
        if (!parent || !*parent) return false;
        if (strcmp(parent, cur) == 0) return false;
        cur = parent;
    }
    return false;
}

/* ── Python truthiness / scalar helpers ────────────────────────────────── */
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
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

static void dtoa_repr(double x, char *buf, size_t cap) {
    if (isnan(x)) { snprintf(buf, cap, "nan"); return; }
    if (isinf(x)) { snprintf(buf, cap, x < 0 ? "-inf" : "inf"); return; }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, x);
        if (strtod(buf, NULL) == x) return;
    }
    snprintf(buf, cap, "%.17g", x);
}

/* Python str() of a JSON scalar (containers out of fixture scope). */
static char *py_str_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) snprintf(buf, sizeof buf, "%.0f", x);
        else dtoa_repr(x, buf, sizeof buf);
        return strdup(buf);
    }
    return strdup("");
}

/* str(x or "") — falsy x projects to "" before str(). */
static char *str_or_empty(const json_t *v) {
    if (!jtruth(v)) return strdup("");
    return py_str_scalar(v);
}

static bool py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *py_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static char *py_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

/* ── PurePosixPath algebra (probe-verified py3.14.9) ───────────────────── */
/* Root token: exactly two leading slashes -> "//" (POSIX preserves it); one
 * or >=3 leading slashes -> "/"; none -> "" (relative). */
static void path_root(const char *s, char *root) {
    int n = 0;
    while (s && s[n] == '/') n++;
    if (n == 2) { root[0] = '/'; root[1] = '/'; root[2] = '\0'; }
    else if (n >= 1) { root[0] = '/'; root[1] = '\0'; }
    else { root[0] = '\0'; }
}
/* Split a posix path into normalized components (drops "" and ".", keeps
 * ".."). root_out receives the root token. Returns a malloc'd array of
 * malloc'd strings; *count set. Caller frees each + the array. */
static char **path_segments(const char *s, char *root_out, size_t *count) {
    path_root(s, root_out);
    size_t cap = 8, n = 0;
    char **segs = malloc(cap * sizeof(char *));
    const char *p = s ? s : "";
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;  /* drop "." */
        if (n == cap) { cap *= 2; segs = realloc(segs, cap * sizeof(char *)); }
        char *seg = malloc(len + 1);
        memcpy(seg, start, len);
        seg[len] = '\0';
        segs[n++] = seg;
    }
    *count = n;
    return segs;
}
static void free_segments(char **segs, size_t n) {
    for (size_t i = 0; i < n; i++) free(segs[i]);
    free(segs);
}
/* Reconstruct: root token concatenated with "/"-joined segments (the root
 * already ends in slash); empty root + no segments -> ".". */
static char *path_build(const char *root, char **segs, size_t n) {
    size_t rl = strlen(root);
    size_t total = rl;
    for (size_t i = 0; i < n; i++) total += strlen(segs[i]) + 1;
    char *out = malloc(total + 2);
    size_t pos = 0;
    memcpy(out + pos, root, rl);
    pos += rl;
    for (size_t i = 0; i < n; i++) {
        if (i > 0) out[pos++] = '/';
        size_t l = strlen(segs[i]);
        memcpy(out + pos, segs[i], l);
        pos += l;
    }
    out[pos] = '\0';
    if (pos == 0) { out[0] = '.'; out[1] = '\0'; }
    return out;
}
static char *path_norm(const char *s) {
    char root[3]; size_t n;
    char **segs = path_segments(s, root, &n);
    char *out = path_build(root, segs, n);
    free_segments(segs, n);
    return out;
}
static bool path_is_absolute(const char *s) { return s && s[0] == '/'; }

/* parts as a JSON array of strings (root token first when absolute). */
static json_t *path_parts(const char *s) {
    char root[3]; size_t n;
    char **segs = path_segments(s, root, &n);
    json_t *arr = json_array();
    if (root[0]) json_append(arr, json_string(root));
    for (size_t i = 0; i < n; i++) json_append(arr, json_string(segs[i]));
    free_segments(segs, n);
    return arr;
}
static bool path_has_dotdot(const char *s) {
    char root[3]; size_t n;
    char **segs = path_segments(s, root, &n);
    bool found = false;
    for (size_t i = 0; i < n; i++) if (strcmp(segs[i], "..") == 0) { found = true; break; }
    free_segments(segs, n);
    return found;
}
/* suffix of the final component (PurePosixPath.suffix for the fixture domain). */
static char *path_suffix(const char *s) {
    char root[3]; size_t n;
    char **segs = path_segments(s, root, &n);
    char *out = strdup("");
    if (n > 0) {
        const char *name = segs[n - 1];
        if (strcmp(name, "..") != 0 && strcmp(name, ".") != 0) {
            const char *dot = strrchr(name, '.');
            if (dot && dot != name) { free(out); out = strdup(dot); }
        }
    }
    free_segments(segs, n);
    return out;
}
static char *path_parent(const char *s) {
    char root[3]; size_t n;
    char **segs = path_segments(s, root, &n);
    char *out = path_build(root, segs, n > 0 ? n - 1 : 0);
    free_segments(segs, n);
    return out;
}
/* PurePosixPath truediv: absolute rhs replaces; else combine components. */
static char *path_join(const char *a, const char *b) {
    if (path_is_absolute(b)) return path_norm(b);
    char root_a[3]; size_t na;
    char **sa = path_segments(a, root_a, &na);
    char root_b[3]; size_t nb;
    char **sb = path_segments(b, root_b, &nb);
    size_t total = na + nb;
    char **combined = malloc((total ? total : 1) * sizeof(char *));
    for (size_t i = 0; i < na; i++) combined[i] = sa[i];
    for (size_t i = 0; i < nb; i++) combined[na + i] = sb[i];
    char *out = path_build(root_a, combined, total);
    free(combined);
    free_segments(sa, na);
    free_segments(sb, nb);
    return out;
}
/* relative_to succeeds iff the anchors (root tokens) match AND base's parts
 * are a prefix of path's parts (absolute path is never relative to a relative
 * base; "/" and "//" roots differ). */
static bool path_relative_to_ok(const char *path, const char *base) {
    json_t *pp = path_parts(path);
    json_t *bp = path_parts(base);
    size_t np = json_len(pp), nb = json_len(bp);
    const char *ap = np > 0 ? json_string_value(json_get(pp, 0)) : "";
    const char *ab = nb > 0 ? json_string_value(json_get(bp, 0)) : "";
    const char *anchor_p = (ap && (strcmp(ap, "/") == 0 || strcmp(ap, "//") == 0)) ? ap : "";
    const char *anchor_b = (ab && (strcmp(ab, "/") == 0 || strcmp(ab, "//") == 0)) ? ab : "";
    bool ok = (strcmp(anchor_p, anchor_b) == 0) && nb <= np;
    if (ok) {
        for (size_t i = 0; i < nb; i++) {
            const char *a = json_string_value(json_get(pp, i));
            const char *b = json_string_value(json_get(bp, i));
            if (strcmp(a ? a : "", b ? b : "") != 0) { ok = false; break; }
        }
    }
    json_free(pp);
    json_free(bp);
    return ok;
}
static bool path_suffix_is_yaml(const char *s) {
    char *suf = path_suffix(s);
    char *low = py_lower(suf);
    bool y = (strcmp(low, ".yaml") == 0 || strcmp(low, ".yml") == 0);
    free(suf);
    free(low);
    return y;
}

/* ── module constants ──────────────────────────────────────────────────── */
static const char *SPH_SURFACES[2] = {"status", "generic"};
#define SPH_MAX_PHRASES 80
#define SPH_MAX_CHARS 160
static const char *SPH_CONVENTIONAL[2] = {"status_phrases.yaml", "status_phrases"};
static const char *SPH_CONFIG_KEYS[2] = {"generic_status_phrases", "status_phrases"};

static json_t *fallback_phrases(void) {
    json_t *o = json_object();
    json_t *st = json_array();
    json_append(st, json_string("still on it"));
    json_append(st, json_string("still working through it"));
    json_append(st, json_string("waiting for the result"));
    json_set(o, "status", st);
    json_t *ge = json_array();
    json_append(ge, json_string("on it"));
    json_append(ge, json_string("one sec"));
    json_append(ge, json_string("checking that now"));
    json_set(o, "generic", ge);
    return o;
}

/* default catalog injected per case (models the _DEFAULT_PHRASES global). */
static json_t *default_catalog(json_t *in) {
    json_t *d = json_obj_get(in, "default_phrases");
    if (j_is_obj(d)) return json_copy(d);
    return fallback_phrases();
}

/* ── _clean_phrase_list (claimed sibling: inline, no PoP re-claim) ─────── */
static json_t *sph_clean_list(const json_t *value) {
    json_t *cleaned = json_array();
    if (j_is_arr(value)) {
        size_t n = json_len(value);
        if (n > SPH_MAX_PHRASES) n = SPH_MAX_PHRASES;
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_get(value, i);
            char *raw = str_or_empty(item);
            char *phrase = py_strip(raw);
            free(raw);
            size_t len = strlen(phrase);
            if (len == 0 || len > SPH_MAX_CHARS) { free(phrase); continue; }
            bool dup = false;
            for (size_t k = 0; k < json_len(cleaned); k++) {
                const char *e = json_string_value(json_get(cleaned, k));
                if (e && strcmp(e, phrase) == 0) { dup = true; break; }
            }
            if (!dup) json_append(cleaned, json_string(phrase));
            free(phrase);
        }
    }
    return cleaned;
}

/* ── _copy_catalog (UNCLAIMED — PoP credited) ──────────────────────────── */
/* PoP: sph_copy_catalog @ gateway/status_phrases.py:_copy_catalog */
static json_t *sph_copy_catalog(sph_io_t *io, json_t *in) {
    (void)io;
    json_t *catalog = json_obj_get(in, "catalog");
    json_t *out = json_object();
    if (j_is_obj(catalog)) {
        for (size_t i = 0; i < json_object_size(catalog); i++) {
            const char *key = json_object_get_key_at(catalog, i);
            json_t *val = json_object_get_at(catalog, i);
            /* list(phrases): copy a list value verbatim (fixture domain). */
            json_set(out, key, val ? json_copy(val) : json_null());
        }
    }
    return out;
}

/* ── _merge_phrase_mapping (claimed sibling: inline) ───────────────────── */
static void sph_merge_mapping(json_t *catalog, json_t *section, const char *inherited_mode) {
    if (!j_is_obj(section)) return;
    /* replace = str(section.get("mode") or inherited_mode or "append").strip().lower()=="replace" */
    json_t *mode_v = json_obj_get(section, "mode");
    char *mode_str;
    if (jtruth(mode_v)) mode_str = py_str_scalar(mode_v);
    else if (inherited_mode && inherited_mode[0]) mode_str = strdup(inherited_mode);
    else mode_str = strdup("append");
    char *mode_strip = py_strip(mode_str);
    char *mode_low = py_lower(mode_strip);
    bool replace = (strcmp(mode_low, "replace") == 0);
    free(mode_str); free(mode_strip); free(mode_low);

    json_t *ph = json_obj_get(section, "phrases");
    json_t *phrase_map = j_is_obj(ph) ? ph : section;
    for (int s = 0; s < 2; s++) {
        const char *surface = SPH_SURFACES[s];
        json_t *pv = j_is_obj(phrase_map) ? json_obj_get(phrase_map, surface) : NULL;
        json_t *phrases = sph_clean_list(pv);
        if (json_len(phrases) > 0) {
            if (replace) {
                json_set(catalog, surface, phrases);
            } else {
                json_t *existing = json_obj_get(catalog, surface);
                json_t *merged = json_array();
                if (j_is_arr(existing))
                    for (size_t i = 0; i < json_len(existing); i++)
                        json_append(merged, json_copy(json_get(existing, i)));
                for (size_t i = 0; i < json_len(phrases); i++)
                    json_append(merged, json_copy(json_get(phrases, i)));
                json_set(catalog, surface, merged);
                json_free(phrases);
            }
        } else {
            json_free(phrases);
        }
    }
}

/* ── _merge_phrase_file (claimed sibling: inline; seams read_text/yaml) ── */
static json_t *sph_merge_file(sph_io_t *io, json_t *catalog, const char *path,
                              const char *inherited_mode) {
    /* try: loaded = yaml.safe_load(path.read_text(encoding="utf-8-sig"))
     * except Exception: return  (BaseException propagates) */
    json_t *args = json_object();
    json_set_hdr(args, "path", path ? path : "");
    json_set_hdr(args, "encoding", "utf-8-sig");
    json_t *text = sph_call(io, "Path.read_text", args);
    json_free(args);
    if (sph_raised(text)) {
        const char *ty = json_get_str(text, "__raise__", "RuntimeError");
        if (sph_is_exception_subclass(ty)) { json_free(text); return json_null(); }
        return text;  /* BaseException passthrough */
    }
    json_t *yargs = json_object();
    json_set(yargs, "text", text ? json_copy(text) : json_null());
    json_t *loaded = sph_call(io, "yaml.safe_load", yargs);
    json_free(yargs);
    json_free(text);
    if (sph_raised(loaded)) {
        const char *ty = json_get_str(loaded, "__raise__", "RuntimeError");
        if (sph_is_exception_subclass(ty)) { json_free(loaded); return json_null(); }
        return loaded;
    }
    if (j_is_obj(loaded)) sph_merge_mapping(catalog, loaded, inherited_mode);
    json_free(loaded);
    return json_null();
}

/* ── _iter_phrase_files (claimed sibling: inline; Path seams) ──────────── */
static json_t *sph_iter_files(sph_io_t *io, const char *base_dir, json_t *raw_path) {
    char *raw0 = str_or_empty(raw_path);
    char *raw = py_strip(raw0);
    free(raw0);
    /* candidate = Path(raw).expanduser() — Path(raw) normalizes raw first
     * ("" -> "."), so the seam arg is the normalized posix path. The seam
     * fires BEFORE the not-raw guard. */
    char *raw_norm = path_norm(raw);
    json_t *eargs = json_object();
    json_set_hdr(eargs, "path", raw_norm);
    json_t *expanded = sph_call(io, "Path.expanduser", eargs);
    json_free(eargs);
    free(raw_norm);
    if (sph_raised(expanded)) { free(raw); return expanded; }
    char *cand = j_is_str(expanded) ? strdup(expanded->str_val) : strdup("");
    json_free(expanded);
    if (!raw[0] || path_is_absolute(cand) || path_has_dotdot(cand)) {
        free(raw); free(cand);
        return json_array();
    }
    free(raw);
    /* base = base_dir.resolve() — base_dir is a Path (normalized) live, so the
     * seam arg is the normalized posix path. */
    char *base_norm = path_norm(base_dir ? base_dir : "");
    json_t *bargs = json_object();
    json_set_hdr(bargs, "path", base_norm);
    json_t *base_r = sph_call(io, "Path.resolve", bargs);
    json_free(bargs);
    free(base_norm);
    if (sph_raised(base_r)) { free(cand); return base_r; }
    char *base = j_is_str(base_r) ? strdup(base_r->str_val) : strdup("");
    json_free(base_r);
    /* path = (base / candidate).resolve() */
    char *joined = path_join(base, cand);
    free(cand);
    json_t *pargs = json_object();
    json_set_hdr(pargs, "path", joined);
    json_t *path_r = sph_call(io, "Path.resolve", pargs);
    json_free(pargs);
    free(joined);
    if (sph_raised(path_r)) { free(base); return path_r; }
    char *path = j_is_str(path_r) ? strdup(path_r->str_val) : strdup("");
    json_free(path_r);
    /* try: path.relative_to(base) except ValueError: return [] */
    if (!path_relative_to_ok(path, base)) {
        free(base); free(path);
        return json_array();
    }
    free(base);
    /* if path.is_file() and path.suffix.lower() in {.yaml,.yml}: return [path] */
    json_t *fargs = json_object();
    json_set_hdr(fargs, "path", path);
    json_t *isfile = sph_call(io, "Path.is_file", fargs);
    json_free(fargs);
    if (sph_raised(isfile)) { free(path); return isfile; }
    bool file_flag = jtruth(isfile);
    json_free(isfile);
    if (file_flag && path_suffix_is_yaml(path)) {
        json_t *out = json_array();
        json_append(out, json_string(path));
        free(path);
        return out;
    }
    /* if path.is_dir(): sorted(c for c in iterdir if c.is_file() and suffix yaml) */
    json_t *dargs = json_object();
    json_set_hdr(dargs, "path", path);
    json_t *isdir = sph_call(io, "Path.is_dir", dargs);
    json_free(dargs);
    if (sph_raised(isdir)) { free(path); return isdir; }
    bool dir_flag = jtruth(isdir);
    json_free(isdir);
    if (!dir_flag) { free(path); return json_array(); }
    json_t *iargs = json_object();
    json_set_hdr(iargs, "path", path);
    json_t *children = sph_call(io, "Path.iterdir", iargs);
    json_free(iargs);
    free(path);
    if (sph_raised(children)) return children;
    json_t *kept = json_array();
    if (j_is_arr(children)) {
        for (size_t i = 0; i < json_len(children); i++) {
            json_t *cj = json_get(children, i);
            const char *child = j_is_str(cj) ? cj->str_val : "";
            json_t *cfargs = json_object();
            json_set_hdr(cfargs, "path", child);
            json_t *cf = sph_call(io, "Path.is_file", cfargs);
            json_free(cfargs);
            if (sph_raised(cf)) { json_free(kept); json_free(children); return cf; }
            bool cfile = jtruth(cf);
            json_free(cf);
            if (cfile && path_suffix_is_yaml(child)) json_append(kept, json_string(child));
        }
    }
    json_free(children);
    /* sorted() — lexicographic on posix strings (ASCII fixture domain) */
    size_t nk = json_len(kept);
    for (size_t a = 0; a + 1 < nk; a++)
        for (size_t b = a + 1; b < nk; b++) {
            const char *sa = json_string_value(json_get(kept, a));
            const char *sb = json_string_value(json_get(kept, b));
            if (sb && sa && strcmp(sb, sa) < 0) {
                json_t *tmp = json_copy(json_get(kept, a));
                json_array_set(kept, a, json_copy(json_get(kept, b)));
                json_array_set(kept, b, tmp);
            }
        }
    return kept;
}

/* ── _merge_phrase_paths (claimed sibling: inline) ─────────────────────── */
static json_t *sph_merge_paths(sph_io_t *io, json_t *catalog, json_t *paths,
                               const char *base_dir, const char *inherited_mode) {
    if (!paths || paths->type == JSON_NULL) return json_null();
    json_t *list;
    bool owned_list = false;
    if (j_is_arr(paths)) list = paths;
    else { list = json_array(); json_append(list, json_copy(paths)); owned_list = true; }
    json_t *err = NULL;
    for (size_t i = 0; i < json_len(list); i++) {
        json_t *raw_path = json_get(list, i);
        json_t *files = sph_iter_files(io, base_dir, raw_path);
        if (sph_raised(files)) { err = files; break; }
        if (j_is_arr(files)) {
            for (size_t k = 0; k < json_len(files); k++) {
                const char *pf = json_string_value(json_get(files, k));
                json_t *r = sph_merge_file(io, catalog, pf ? pf : "", inherited_mode);
                if (sph_raised(r)) { err = r; break; }
                json_free(r);
            }
        }
        json_free(files);
        if (err) break;
    }
    if (owned_list) json_free(list);
    return err ? err : json_null();
}

/* ── _merge_phrase_config (claimed sibling: inline) ────────────────────── */
static json_t *sph_merge_config(sph_io_t *io, json_t *catalog, json_t *section,
                                const char *base_dir) {
    if (!j_is_obj(section)) return json_null();
    json_t *mode_v = json_obj_get(section, "mode");
    char *mode_str = jtruth(mode_v) ? py_str_scalar(mode_v) : strdup("append");
    char *mode_strip = py_strip(mode_str);
    char *mode = py_lower(mode_strip);
    free(mode_str); free(mode_strip);
    static const char *keys[2] = {"path", "paths"};
    json_t *err = NULL;
    for (int k = 0; k < 2; k++) {
        json_t *pv = json_obj_get(section, keys[k]);
        json_t *r = sph_merge_paths(io, catalog, pv, base_dir, mode);
        if (sph_raised(r)) { err = r; break; }
        json_free(r);
    }
    if (!err) sph_merge_mapping(catalog, section, NULL);
    free(mode);
    return err ? err : json_null();
}

/* ── resolve_status_phrase_catalog (claimed sibling: inline) ───────────── */
static json_t *sph_resolve_catalog(sph_io_t *io, json_t *in) {
    json_t *def = default_catalog(in);
    json_t *catalog = json_object();
    for (size_t i = 0; i < json_object_size(def); i++) {
        const char *key = json_object_get_key_at(def, i);
        json_t *val = json_object_get_at(def, i);
        json_set(catalog, key, val ? json_copy(val) : json_null());
    }
    json_free(def);
    /* hermes_home = get_hermes_home() */
    json_t *home_r = sph_call(io, "get_hermes_home", NULL);
    if (sph_raised(home_r)) { json_free(catalog); return home_r; }
    char *home = j_is_str(home_r) ? strdup(home_r->str_val) : strdup("");
    json_free(home_r);
    /* _merge_phrase_paths(catalog, list(_CONVENTIONAL_RELATIVE_PATHS), base_dir=home) */
    json_t *conv = json_array();
    json_append(conv, json_string(SPH_CONVENTIONAL[0]));
    json_append(conv, json_string(SPH_CONVENTIONAL[1]));
    json_t *r = sph_merge_paths(io, catalog, conv, home, NULL);
    json_free(conv);
    if (sph_raised(r)) { free(home); json_free(catalog); return r; }
    json_free(r);
    /* display = (user_config or {}).get("display") if isinstance(user_config, Mapping) else None */
    json_t *user_config = json_obj_get(in, "user_config");
    json_t *display = NULL;
    if (j_is_obj(user_config)) display = json_obj_get(user_config, "display");
    if (!j_is_obj(display)) { free(home); return catalog; }
    /* sections = [display]; platforms = display.get("platforms") */
    json_t *platforms = json_obj_get(display, "platforms");
    const char *platform_key = json_get_str(in, "platform_key", "");
    json_t *plat_section = NULL;
    if (platform_key && platform_key[0] && j_is_obj(platforms)) {
        json_t *ps = json_obj_get(platforms, platform_key);
        if (j_is_obj(ps)) plat_section = ps;
    }
    json_t *sections[2] = {display, plat_section};
    int nsec = plat_section ? 2 : 1;
    json_t *err = NULL;
    for (int si = 0; si < nsec && !err; si++) {
        for (int ki = 0; ki < 2; ki++) {
            json_t *sec = json_obj_get(sections[si], SPH_CONFIG_KEYS[ki]);
            json_t *rr = sph_merge_config(io, catalog, sec, home);
            if (sph_raised(rr)) { err = rr; break; }
            json_free(rr);
        }
    }
    free(home);
    if (err) { json_free(catalog); return err; }
    return catalog;
}

/* ── classify_status_context (claimed sibling: inline) ─────────────────── */
static json_t *sph_classify(sph_io_t *io, json_t *in) {
    (void)io;
    json_t *kind = json_obj_get(in, "kind");
    char *raw = str_or_empty(kind);
    char *strip = py_strip(raw);
    char *low = py_lower(strip);
    free(raw); free(strip);
    static const char *status_kinds[4] = {"heartbeat", "waiting", "long_running", "status"};
    bool is_status = false;
    for (int i = 0; i < 4; i++) if (strcmp(low, status_kinds[i]) == 0) { is_status = true; break; }
    free(low);
    return json_string(is_status ? "status" : "generic");
}

/* ── choose_status_phrase (claimed sibling: inline; random.choice seam) ── */
static json_t *sph_choose(sph_io_t *io, json_t *in) {
    /* phrase_catalog = catalog or _DEFAULT_PHRASES */
    json_t *catalog = json_obj_get(in, "catalog");
    json_t *def = default_catalog(in);
    json_t *phrase_catalog = jtruth(catalog) ? catalog : def;
    /* category = classify_status_context(kind, ...) */
    json_t *kind = json_obj_get(in, "kind");
    char *raw = str_or_empty(kind);
    char *strip = py_strip(raw);
    char *low = py_lower(strip);
    free(raw); free(strip);
    static const char *status_kinds[4] = {"heartbeat", "waiting", "long_running", "status"};
    bool is_status = false;
    for (int i = 0; i < 4; i++) if (strcmp(low, status_kinds[i]) == 0) { is_status = true; break; }
    free(low);
    const char *category = is_status ? "status" : "generic";
    /* candidates = list(pc.get(category) or pc.get("generic") or _DEFAULT_PHRASES["generic"]) */
    json_t *cand_src = json_obj_get(phrase_catalog, category);
    if (!jtruth(cand_src)) cand_src = json_obj_get(phrase_catalog, "generic");
    if (!jtruth(cand_src)) {
        cand_src = json_obj_get(def, "generic");
        if (!cand_src) {
            /* _DEFAULT_PHRASES["generic"] -> KeyError 'generic' */
            json_free(def);
            return raise_desc("KeyError", "'generic'");
        }
    }
    json_t *candidates = json_array();
    if (j_is_arr(cand_src))
        for (size_t i = 0; i < json_len(cand_src); i++)
            json_append(candidates, json_copy(json_get(cand_src, i)));
    /* if recent: candidates = [p for p in candidates if p not in set(recent)] or candidates */
    json_t *recent = json_obj_get(in, "recent");
    if (jtruth(recent) && j_is_arr(recent)) {
        json_t *filtered = json_array();
        for (size_t i = 0; i < json_len(candidates); i++) {
            const char *p = json_string_value(json_get(candidates, i));
            bool in_recent = false;
            for (size_t k = 0; k < json_len(recent); k++) {
                const char *rp = json_string_value(json_get(recent, k));
                if (p && rp && strcmp(p, rp) == 0) { in_recent = true; break; }
            }
            if (!in_recent) json_append(filtered, json_copy(json_get(candidates, i)));
        }
        if (json_len(filtered) > 0) {
            json_free(candidates);
            candidates = filtered;
        } else {
            json_free(filtered);
        }
    }
    /* phrase = (rng or _random).choice(candidates) */
    json_t *cargs = json_object();
    json_set(cargs, "candidates", json_copy(candidates));
    json_t *phrase = sph_call(io, "random.choice", cargs);
    json_free(cargs);
    json_free(candidates);
    if (sph_raised(phrase)) { json_free(def); return phrase; }
    /* if recent is not None: recent.append(phrase); del recent[:-6] */
    if (recent && recent->type != JSON_NULL && j_is_arr(recent)) {
        json_append(recent, json_copy(phrase));
        size_t n = json_len(recent);
        if (n > 6) {
            size_t drop = n - 6;
            for (size_t i = 0; i < drop; i++) json_array_remove(recent, 0);
        }
    }
    json_free(def);
    return phrase;
}

/* ── module-level _DEFAULT_PHRASES construction (unclaimed module flow) ── */
static json_t *sph_load_default_phrases(sph_io_t *io, json_t *in) {
    /* catalog = _copy_catalog(_FALLBACK_PHRASES) */
    json_t *catalog = fallback_phrases();
    /* path = Path(__file__).resolve().parent / "assets" / "status_phrases.yaml" */
    const char *module_file = json_get_str(in, "module_file", "");
    char *mf_norm = path_norm(module_file);
    json_t *rargs = json_object();
    json_set_hdr(rargs, "path", mf_norm);
    json_t *resolved = sph_call(io, "Path.resolve", rargs);
    json_free(rargs);
    free(mf_norm);
    if (sph_raised(resolved)) { json_free(catalog); return resolved; }
    char *res = j_is_str(resolved) ? strdup(resolved->str_val) : strdup("");
    json_free(resolved);
    char *parent = path_parent(res);
    free(res);
    char *p1 = path_join(parent, "assets");
    free(parent);
    char *path = path_join(p1, "status_phrases.yaml");
    free(p1);
    /* _merge_phrase_file(catalog, path, inherited_mode="replace") */
    json_t *r = sph_merge_file(io, catalog, path, "replace");
    free(path);
    if (sph_raised(r)) { json_free(catalog); return r; }
    json_free(r);
    return catalog;
}

/* ── public entry wrappers (uniform json in -> json out) ───────────────── */
static json_t *sph_clean_phrase_list(sph_io_t *io, json_t *in) {
    (void)io;
    return sph_clean_list(json_obj_get(in, "value"));
}
static json_t *sph_merge_phrase_mapping(sph_io_t *io, json_t *in) {
    (void)io;
    json_t *catalog = json_obj_get(in, "catalog");
    json_t *section = json_obj_get(in, "section");
    const char *inh = json_get_str(in, "inherited_mode", "");
    sph_merge_mapping(catalog, section, inh[0] ? inh : NULL);
    return json_null();
}
static json_t *sph_merge_phrase_file(sph_io_t *io, json_t *in) {
    json_t *catalog = json_obj_get(in, "catalog");
    const char *path = json_get_str(in, "path", "");
    const char *inh = json_get_str(in, "inherited_mode", "");
    return sph_merge_file(io, catalog, path, inh[0] ? inh : NULL);
}
static json_t *sph_iter_phrase_files(sph_io_t *io, json_t *in) {
    const char *base = json_get_str(in, "base_dir", "");
    return sph_iter_files(io, base, json_obj_get(in, "raw_path"));
}
static json_t *sph_merge_phrase_paths(sph_io_t *io, json_t *in) {
    json_t *catalog = json_obj_get(in, "catalog");
    json_t *paths = json_obj_get(in, "paths");
    const char *base = json_get_str(in, "base_dir", "");
    const char *inh = json_get_str(in, "inherited_mode", "");
    return sph_merge_paths(io, catalog, paths, base, inh[0] ? inh : NULL);
}
static json_t *sph_merge_phrase_config(sph_io_t *io, json_t *in) {
    json_t *catalog = json_obj_get(in, "catalog");
    json_t *section = json_obj_get(in, "section");
    const char *base = json_get_str(in, "base_dir", "");
    return sph_merge_config(io, catalog, section, base);
}
static json_t *sph_resolve_status_phrase_catalog(sph_io_t *io, json_t *in) {
    return sph_resolve_catalog(io, in);
}
static json_t *sph_classify_status_context(sph_io_t *io, json_t *in) {
    return sph_classify(io, in);
}
static json_t *sph_choose_status_phrase(sph_io_t *io, json_t *in) {
    return sph_choose(io, in);
}