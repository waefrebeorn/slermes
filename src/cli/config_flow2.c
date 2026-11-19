/* config_flow2.c — FLOW-core port of the UNCLAIMED remainder of
 * hermes_cli/config.py (the config command surface, the .env write-lock
 * refusal guards, the platform-manifest secret declaration logic and the
 * interactive offer/prompt helpers).
 *
 * Sibling of config_load_save_flow.c (src/hermes_cli/): same universal seam
 * dispatcher model. Every collaborator / I/O call goes through
 * cfg2_seam(io, name, args); the dispatcher appends `name` to io->trace,
 * returns the scripted result, applies "state" effects, and models a raised
 * exception with {"__raise__": {type,message,...}}. An "import_raise" script
 * entry models a failed in-function import: the dispatcher returns
 * {"__raise__": spec, "__untrace__": true} and cfg2_seam retracts the trace
 * token (the live import statement records nothing before it raises) while
 * KEEPING the __untrace__ flag so a consumer can tell an import failure
 * (caught by `except Exception`) from a call raise (propagates).
 *
 * Two cosmetic seams are AUTO (no script, never fail closed): "print"
 * (captures args.text into io->cap_stdout or io->cap_stderr per args.file) and
 * "color" (returns args.text verbatim — the identity / NO_COLOR projection).
 * Every other seam is SCRIPTED and FAIL-CLOSED: a missing script raises
 * RuntimeError("unscripted seam: <name>").
 *
 * io->state is a json OBJECT mirroring the module globals / caller dicts the
 * flows mutate or read:
 *   _DOCKER_UPDATE_MESSAGE                 (format_docker_update_message)
 *   BUNDLED_PLATFORM_SECRET_ENVS           (platform_manifest_secret_envs)
 *   results                                (_prompt_and_save_env env_added)
 *   user_config                            (_dropped_shadow_notes mutation)
 *   skipped                                (platform_manifest_secret_scan)
 *
 * Claimed siblings (_split_key_path is a pure leaf transcribed inline;
 * _unset_nested / _locate_nested / is_managed / format_managed_message /
 * get_config_value / set_config_value / unset_config_value / show_config /
 * core_declared_env_names / _platform_plugin_manifests / save_env_value) are
 * SEAMED, so their existing PoP claims keep credit. _platform_manifest_env_entries
 * and _is_plugin_dir_name are pure and ported here (untagged PoP).
 *
 * Representation contract (both sides): paths are posix strings; frozenset /
 * set results project to SORTED arrays; an uncaught exception rides as
 * {"__raise__": {type,message,code?,...}}; SystemExit carries code. Integral
 * JSON numbers read as int (libjson has no int/float identity). Fixture input
 * domain is ASCII (env names, yes/no answers, simple identifiers); the few
 * non-ASCII glyphs in message CONSTANTS (check/cross/warn/bullet/em-dash) are
 * byte-pinned UTF-8 literals, not fixture data.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * / io->stdout / io->stderr are harness-owned. cfg2_seam() frees its args.
 * Every public cfg2_* function returns a NEW reference (or a raise sentinel).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── message-constant glyphs (byte-pinned UTF-8) ───────────────────────── */
#define CFG2_CROSS "\xe2\x9c\x97"   /* ✗ */
#define CFG2_CHECK "\xe2\x9c\x93"   /* ✓ */
#define CFG2_WARN  "\xe2\x9a\xa0"   /* ⚠ */
#define CFG2_BULLET "\xe2\x80\xa2"  /* • */
#define CFG2_EMDASH "\xe2\x80\x94"  /* — */

/* Colors (hermes_cli/colors.py) — ANSI codes passed to the color seam. */
#define CFG2_RESET  "\x1b[0m"
#define CFG2_BOLD   "\x1b[1m"
#define CFG2_DIM    "\x1b[2m"
#define CFG2_RED    "\x1b[31m"
#define CFG2_GREEN  "\x1b[32m"
#define CFG2_YELLOW "\x1b[33m"
#define CFG2_CYAN   "\x1b[36m"

/* _USAGE_GET / _USAGE_SET / _USAGE_UNSET (config.py:3405) — byte-pinned. */
static const char *CFG2_USAGE_GET =
    "Usage: hermes config get <key> [--json] [--raw]";
static const char *CFG2_USAGE_GET_EX[] = {
    "hermes config get model", "hermes config get terminal.backend",
    "hermes config get skills.config --json", NULL,
};
static const char *CFG2_USAGE_SET =
    "Usage: hermes config set [--force] <key> <value>";
static const char *CFG2_USAGE_SET_EX[] = {
    "hermes config set model anthropic/claude-sonnet-4",
    "hermes config set terminal.backend docker",
    "hermes config set OPENROUTER_API_KEY sk-or-...", NULL,
};
static const char *CFG2_USAGE_SET_EXTRA[] = {
    "", "  --force: skip the unknown-key notice for unrecognized keys,",
    "           and allow a scalar to replace a whole mapping section", NULL,
};
static const char *CFG2_USAGE_UNSET =
    "Usage: hermes config unset <key>";
static const char *CFG2_USAGE_UNSET_EX[] = {
    "hermes config unset model", "hermes config unset terminal.backend",
    "hermes config unset OPENROUTER_API_KEY", NULL,
};

/* PLATFORM_SECRET_ENV_SUFFIXES (config.py:3682). */
static const char *CFG2_SECRET_SUFFIXES[] = {
    "_TOKEN", "_SECRET", "_KEY", "_PASSWORD", "_JSON", NULL,
};

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct cfg2_flow_io_t {
    json_t *(*call)(struct cfg2_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;          /* ARRAY of string tokens (harness-owned) */
    json_t *state;          /* OBJECT (harness-owned) */
    json_t *cap_stdout;     /* ARRAY of captured stdout print text */
    json_t *cap_stderr;     /* ARRAY of captured stderr print text */
    void   *ctx;
} cfg2_flow_io_t;

static void cfg2_trace(cfg2_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *cfg2_call(cfg2_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool cfg2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *cfg2_raise_spec(const json_t *r) {
    if (!cfg2_raised(r)) return NULL;
    return json_obj_get(r, "__raise__");
}

static json_t *cfg2_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

static json_t *cfg2_make_raise_code(const char *type, double code) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    char buf[32];
    snprintf(buf, sizeof buf, "%.0f", code);
    json_set(spec, "message", json_string(buf));
    json_set(spec, "code", json_number(code));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

/* seam: trace the attempt, dispatch, free args. An "__untrace__" result (failed
 * in-function import) retracts the trace token but KEEPS the flag so consumers
 * can distinguish an import failure from a call raise. */
static json_t *cfg2_seam(cfg2_flow_io_t *io, const char *name, json_t *args) {
    cfg2_trace(io, name);
    json_t *r = cfg2_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = json_object();
        json_set(out, "__raise__", spec ? json_copy(spec) : json_null());
        json_set(out, "__untrace__", json_bool(true));
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool cfg2_truthy(const json_t *v) {
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

static bool cfg2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool cfg2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

/* `x is False` identity gate — JSON_BOOL false ONLY (not 0 / null / ""). */
static bool cfg2_is_false(const json_t *v) {
    return v && v->type == JSON_BOOL && !v->bool_val;
}

static const char *cfg2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Python type(x).__name__ for a JSON value (integral numbers read as int). */
static const char *cfg2_py_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Unicode White_Space predicate over a decoded codepoint (str.strip set). */
static bool cfg2_is_ws_cp(unsigned long cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d)) return true;   /* ASCII */
    if (cp >= 0x1c && cp <= 0x1f) return true;                   /* FS..US */
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}

/* decode one UTF-8 codepoint at s[*i], advance *i; returns cp (0 on bad end). */
static unsigned long cfg2_next_cp(const char *s, size_t n, size_t *i) {
    size_t p = *i;
    if (p >= n) return 0;
    unsigned char c = (unsigned char)s[p];
    unsigned long cp; size_t extra;
    if (c < 0x80) { cp = c; extra = 0; }
    else if ((c & 0xe0) == 0xc0) { cp = c & 0x1f; extra = 1; }
    else if ((c & 0xf0) == 0xe0) { cp = c & 0x0f; extra = 2; }
    else if ((c & 0xf8) == 0xf0) { cp = c & 0x07; extra = 3; }
    else { *i = p + 1; return 0xfffd; }
    if (p + extra >= n) { *i = p + 1; return 0xfffd; }
    for (size_t k = 1; k <= extra; k++) {
        unsigned char cc = (unsigned char)s[p + k];
        if ((cc & 0xc0) != 0x80) { *i = p + 1; return 0xfffd; }
        cp = (cp << 6) | (cc & 0x3f);
    }
    *i = p + extra + 1;
    return cp;
}

/* str.strip() — full Unicode White_Space, codepoint-aware. malloc'd. */
static char *cfg2_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    if (n == 0) return strdup("");
    /* decode codepoint boundaries: starts[i], ends[i], cps[i] */
    size_t cap = n;   /* at most n codepoints */
    size_t *starts = malloc(cap * sizeof(size_t));
    size_t *ends = malloc(cap * sizeof(size_t));
    unsigned long *cps = malloc(cap * sizeof(unsigned long));
    if (!starts || !ends || !cps) { free(starts); free(ends); free(cps); return strdup(p); }
    size_t m = 0, i = 0;
    while (i < n) {
        size_t st = i;
        unsigned long cp = cfg2_next_cp(p, n, &i);
        starts[m] = st; ends[m] = i; cps[m] = cp; m++;
    }
    size_t a = 0, b = m;
    while (a < b && cfg2_is_ws_cp(cps[a])) a++;
    while (b > a && cfg2_is_ws_cp(cps[b - 1])) b--;
    size_t from = (a < m) ? starts[a] : n;
    size_t to = (b > 0 && b <= m) ? ends[b - 1] : from;
    if (b == a) { from = n; to = n; }   /* all whitespace -> empty */
    char *out = malloc(to - from + 1);
    if (!out) { free(starts); free(ends); free(cps); return NULL; }
    memcpy(out, p + from, to - from);
    out[to - from] = '\0';
    free(starts); free(ends); free(cps);
    return out;
}

/* ASCII lower/upper (env names + yes/no answers are ASCII by fixture domain). */
static char *cfg2_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

static char *cfg2_upper(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)toupper((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

static bool cfg2_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}

static bool cfg2_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t ns = strlen(s), nf = strlen(suf);
    return ns >= nf && strcmp(s + ns - nf, suf) == 0;
}

static char *cfg2_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}

/* Path-style join mirroring the oracle FakePath.__truediv__: base.rstrip('/')
 * (empty when base == "/"), then ALWAYS a single '/' separator + name. */
static char *cfg2_path_join(const char *base, const char *name) {
    const char *b = base ? base : "";
    size_t nb = strlen(b);
    if (nb == 1 && b[0] == '/') nb = 0;
    else while (nb > 0 && b[nb - 1] == '/') nb--;
    const char *nm = name ? name : "";
    size_t nn = strlen(nm);
    char *out = malloc(nb + 1 + nn + 1);
    if (!out) return NULL;
    if (nb) memcpy(out, b, nb);
    out[nb] = '/';
    if (nn) memcpy(out + nb + 1, nm, nn);
    out[nb + 1 + nn] = '\0';
    return out;
}

/* NEW sorted copy of a string array (frozenset projection). */
static json_t *cfg2_sorted_strs(const json_t *arr) {
    json_t *out = json_array();
    size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    if (n == 0) return out;
    const char **tmp = malloc(n * sizeof(char *));
    for (size_t i = 0; i < n; i++) tmp[i] = cfg2_sval(json_get(arr, i));
    for (size_t i = 1; i < n; i++) {
        const char *v = tmp[i]; size_t j = i;
        while (j > 0 && strcmp(tmp[j - 1], v) > 0) { tmp[j] = tmp[j - 1]; j--; }
        tmp[j] = v;
    }
    for (size_t i = 0; i < n; i++) json_append(out, json_string(tmp[i]));
    free(tmp);
    return out;
}

/* _split_key_path (config.py:650) — pure leaf, transcribed inline (claimed
 * elsewhere; static + untagged keeps that PoP credit). NEW array of segments. */
static json_t *cfg2_split_key_path(const char *key) {
    json_t *parts = json_array();
    if (!key) return parts;
    size_t n = strlen(key), i = 0;
    char *cur = malloc(n + 1);
    size_t cl = 0;
    if (!cur) return parts;
    while (i < n) {
        char ch = key[i];
        if (ch == '\\' && i + 1 < n && key[i + 1] == '.') {
            cur[cl++] = '.'; i += 2; continue;
        }
        if (ch == '.') {
            cur[cl] = '\0';
            json_append(parts, json_string(cur));
            cl = 0;
        } else {
            cur[cl++] = ch;
        }
        i++;
    }
    cur[cl] = '\0';
    json_append(parts, json_string(cur));
    free(cur);
    return parts;
}

/* ".".join(segments) */
static char *cfg2_join_segments(const json_t *segments) {
    size_t total = 1;
    size_t n = json_len(segments);
    for (size_t i = 0; i < n; i++) {
        json_t *s = json_get(segments, i);
        total += strlen(cfg2_sval(s)) + 1;
    }
    char *out = malloc(total);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        const char *t = cfg2_sval(json_get(segments, i));
        if (i > 0) out[k++] = '.';
        size_t tl = strlen(t);
        memcpy(out + k, t, tl); k += tl;
    }
    out[k] = '\0';
    return out;
}

static json_t *cfg2_state_get(cfg2_flow_io_t *io, const char *key) {
    if (!io || !io->state || io->state->type != JSON_OBJECT) return NULL;
    return json_obj_get(io->state, key);
}

/* ── full builtin exception hierarchy (for `except Exception` family tests) ─ */
typedef struct { const char *name; const char *parent; } cfg2_exc_t;
static const cfg2_exc_t CFG2_EXC_TABLE[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"Exception", "BaseException"},
    {"TypeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
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
    {NULL, NULL},
};

static const char *cfg2_exc_parent(const char *type) {
    if (!type) return NULL;
    for (int i = 0; CFG2_EXC_TABLE[i].name; i++)
        if (strcmp(type, CFG2_EXC_TABLE[i].name) == 0) return CFG2_EXC_TABLE[i].parent;
    return NULL;
}

/* isinstance(exc, Exception) — walks the parent chain to "Exception". */
static bool cfg2_exc_is_exception(const char *type) {
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = cfg2_exc_parent(cur);
    }
    return false;
}

static bool cfg2_raise_is(const json_t *r, const char *type) {
    json_t *spec = cfg2_raise_spec(r);
    return spec && strcmp(json_get_str(spec, "type", ""), type) == 0;
}

/* ── cosmetic auto seams (print / color) ───────────────────────────────── */
/* print(text, file=...) — dispatcher captures; returns null. */
static json_t *cfg2_print(cfg2_flow_io_t *io, const char *text, bool to_stderr) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    json_set(args, "file", to_stderr ? json_string("stderr") : json_null());
    return cfg2_seam(io, "print", args);
}

/* color(text, *codes) — dispatcher returns text verbatim (identity). */
static json_t *cfg2_color(cfg2_flow_io_t *io, const char *text, const char **codes) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    json_t *ca = json_array();
    for (int i = 0; codes && codes[i]; i++) json_append(ca, json_string(codes[i]));
    json_set(args, "codes", ca);
    return cfg2_seam(io, "color", args);
}

/* ── forward declarations (composed siblings) ──────────────────────────── */
json_t *cfg2_managed_source(cfg2_flow_io_t *io, const char *filename);
json_t *cfg2_env_write_refusal(cfg2_flow_io_t *io, const char *key, const char *action);
json_t *cfg2_exit_invalid(cfg2_flow_io_t *io, const char *msg);
json_t *cfg2_usage_exit(cfg2_flow_io_t *io, const char *usage, const char **examples, const char **extra);
json_t *cfg2_run_write_command(cfg2_flow_io_t *io, const char *fn_name, json_t *args);
json_t *cfg2_ask_yes_no(cfg2_flow_io_t *io, const char *prompt);
json_t *cfg2_platform_manifest_env_entries(cfg2_flow_io_t *io, const json_t *manifest, bool optional);
json_t *cfg2_manifest_secret_envs(cfg2_flow_io_t *io, const json_t *manifests, const json_t *core);

/* ===================================================================
 * format_docker_update_message  (config.py:384)
 * =================================================================== */
/* PoP: cfg2_format_docker_update_message @ hermes_cli/config.py:format_docker_update_message */
json_t *cfg2_format_docker_update_message(cfg2_flow_io_t *io) {
    json_t *m = cfg2_state_get(io, "_DOCKER_UPDATE_MESSAGE");
    return m ? json_copy(m) : json_null();
}

/* ===================================================================
 * __getattr__  (config.py:3770)  — module-level attribute hook
 * =================================================================== */
/* PoP: cfg2_module_getattr @ hermes_cli/config.py:__getattr__ */
json_t *cfg2_module_getattr(cfg2_flow_io_t *io, const char *name) {
    if (name && strcmp(name, "CORE_DECLARED_ENV_NAMES") == 0) {
        json_t *r = cfg2_seam(io, "core_declared_env_names", NULL);
        if (cfg2_raised(r)) return r;
        /* core_declared_env_names() returns a frozenset -> sorted projection */
        json_t *sorted = cfg2_sorted_strs(r);
        json_free(r);
        return sorted;
    }
    /* f"module {__name__!r} has no attribute {name!r}" — __name__ is
     * 'hermes_cli.config'; name is an ASCII identifier (fixture domain). */
    char *m1 = cfg2_join("module 'hermes_cli.config' has no attribute '", name ? name : "");
    char *msg = cfg2_join(m1, "'");
    free(m1);
    json_t *r = cfg2_make_raise("AttributeError", msg);
    free(msg);
    return r;
}

/* ===================================================================
 * _managed_source  (config.py:2557)
 * =================================================================== */
/* PoP: cfg2_managed_source @ hermes_cli/config.py:_managed_source */
json_t *cfg2_managed_source(cfg2_flow_io_t *io, const char *filename) {
    json_t *md = cfg2_seam(io, "get_managed_dir", NULL);
    if (cfg2_raised(md)) return md;
    /* managed_dir is a Path OBJECT (always truthy) or None — identity gate. */
    if (!cfg2_is_null(md)) {
        char *p = cfg2_path_join(cfg2_sval(md), filename);
        json_t *out = json_string(p);
        free(p);
        json_free(md);
        return out;
    }
    json_free(md);
    return json_string("the managed scope");
}

/* ===================================================================
 * env_write_refusal  (config.py:2527)
 * =================================================================== */
/* PoP: cfg2_env_write_refusal @ hermes_cli/config.py:env_write_refusal */
json_t *cfg2_env_write_refusal(cfg2_flow_io_t *io, const char *key, const char *action) {
    json_t *im = cfg2_seam(io, "is_managed", NULL);
    if (cfg2_raised(im)) return im;
    if (cfg2_truthy(im)) {
        json_free(im);
        char *act = cfg2_join(action ? action : "", " ");
        char *actkey = cfg2_join(act, key ? key : "");
        free(act);
        json_t *args = json_object();
        json_set(args, "action", json_string(actkey));
        free(actkey);
        return cfg2_seam(io, "format_managed_message", args);
    }
    json_free(im);
    json_t *iem = cfg2_seam(io, "is_env_managed", NULL);
    if (cfg2_raised(iem)) return iem;
    if (cfg2_truthy(iem)) {
        json_free(iem);
        json_t *ms = cfg2_managed_source(io, ".env");
        if (cfg2_raised(ms)) return ms;
        /* f"Cannot {action} {key}: it is managed by your administrator ({ms}) "
         * "and cannot be changed." */
        char *m1 = cfg2_join("Cannot ", action ? action : "");
        char *m2 = cfg2_join(m1, " "); free(m1);
        char *m3 = cfg2_join(m2, key ? key : ""); free(m2);
        char *m4 = cfg2_join(m3, ": it is managed by your administrator ("); free(m3);
        char *m5 = cfg2_join(m4, cfg2_sval(ms)); free(m4);
        char *msg = cfg2_join(m5, ") and cannot be changed."); free(m5);
        json_free(ms);
        json_t *out = json_string(msg);
        free(msg);
        return out;
    }
    json_free(iem);
    return json_null();
}

/* ===================================================================
 * _env_write_blocked  (config.py:2540)
 * =================================================================== */
/* PoP: cfg2_env_write_blocked @ hermes_cli/config.py:_env_write_blocked */
json_t *cfg2_env_write_blocked(cfg2_flow_io_t *io, const char *key, const char *action) {
    json_t *refusal = cfg2_env_write_refusal(io, key, action);
    if (cfg2_raised(refusal)) return refusal;
    if (cfg2_truthy(refusal)) {
        json_t *pr = cfg2_print(io, cfg2_sval(refusal), true);
        json_free(pr);
    }
    json_t *out = json_bool(!cfg2_is_null(refusal));   /* refusal is not None */
    json_free(refusal);
    return out;
}

/* ===================================================================
 * require_env_writable  (config.py:2548)
 * =================================================================== */
/* PoP: cfg2_require_env_writable @ hermes_cli/config.py:require_env_writable */
json_t *cfg2_require_env_writable(cfg2_flow_io_t *io, const char *key, const char *action) {
    json_t *refusal = cfg2_env_write_refusal(io, key, action);
    if (cfg2_raised(refusal)) return refusal;
    if (cfg2_truthy(refusal)) {
        json_t *r = cfg2_make_raise("ValueError", cfg2_sval(refusal));
        json_free(refusal);
        return r;
    }
    json_free(refusal);
    return json_null();
}

/* ===================================================================
 * _write_anthropic_slots  (config.py:2623)
 * =================================================================== */
/* PoP: cfg2_write_anthropic_slots @ hermes_cli/config.py:_write_anthropic_slots */
json_t *cfg2_write_anthropic_slots(cfg2_flow_io_t *io, const char *token, const char *api_key,
                                   const json_t *save_fn, bool token_first) {
    /* writer = save_fn or save_env_value (truthiness on the marker) */
    const char *writer = cfg2_truthy(save_fn) ? "save_fn" : "save_env_value";
    const char *names[2] = {"ANTHROPIC_TOKEN", "ANTHROPIC_API_KEY"};
    const char *vals[2] = {token ? token : "", api_key ? api_key : ""};
    int order[2] = {0, 1};
    if (!token_first) { order[0] = 1; order[1] = 0; }
    for (int k = 0; k < 2; k++) {
        int idx = order[k];
        json_t *args = json_object();
        json_set(args, "name", json_string(names[idx]));
        json_set(args, "value", json_string(vals[idx]));
        json_t *r = cfg2_seam(io, writer, args);
        if (cfg2_raised(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* ===================================================================
 * _scoped_environ_get  (config.py:2675)
 * =================================================================== */
/* PoP: cfg2_scoped_environ_get @ hermes_cli/config.py:_scoped_environ_get */
json_t *cfg2_scoped_environ_get(cfg2_flow_io_t *io, const char *key) {
    json_t *args = json_object();
    json_set(args, "name", json_string(key ? key : ""));
    json_t *r = cfg2_seam(io, "get_secret", args);
    if (cfg2_raised(r)) {
        /* only a failed IMPORT (except Exception) falls back to os.environ;
         * a get_secret CALL raise propagates. */
        if (json_obj_get(r, "__untrace__")) {
            json_t *spec = cfg2_raise_spec(r);
            const char *type = json_get_str(spec, "type", "");
            if (cfg2_exc_is_exception(type)) {
                json_free(r);
                json_t *a2 = json_object();
                json_set(a2, "name", json_string(key ? key : ""));
                return cfg2_seam(io, "os.environ.get", a2);
            }
        }
        return r;
    }
    return r;
}

/* ===================================================================
 * _legacy_gateway_platforms_key  (config.py:3074)
 * =================================================================== */
/* PoP: cfg2_legacy_gateway_platforms_key @ hermes_cli/config.py:_legacy_gateway_platforms_key */
json_t *cfg2_legacy_gateway_platforms_key(cfg2_flow_io_t *io, const char *requested_key) {
    (void)io;
    json_t *segs = cfg2_split_key_path(requested_key);
    size_t n = json_len(segs);
    json_t *out = json_null();
    if (n >= 3 && strcmp(cfg2_sval(json_get(segs, 0)), "gateway") == 0
        && strcmp(cfg2_sval(json_get(segs, 1)), "platforms") == 0) {
        char *joined = cfg2_join_segments(segs);
        out = json_string(joined);
        free(joined);
    }
    json_free(segs);
    return out;
}

/* ===================================================================
 * _exit_if_key_managed  (config.py:3085)
 * =================================================================== */
/* PoP: cfg2_exit_if_key_managed @ hermes_cli/config.py:_exit_if_key_managed */
json_t *cfg2_exit_if_key_managed(cfg2_flow_io_t *io, const char *key, const char *action) {
    json_t *ikm = cfg2_seam(io, "is_key_managed", NULL);
    if (cfg2_raised(ikm)) return ikm;
    if (cfg2_truthy(ikm)) {
        json_free(ikm);
        json_t *ms = cfg2_managed_source(io, "config.yaml");
        if (cfg2_raised(ms)) return ms;
        /* f"Cannot {action} '{key}': it is managed by your administrator ({ms}) "
         * f"and cannot be changed. Contact your administrator to modify it." */
        char *m1 = cfg2_join("Cannot ", action ? action : "");
        char *m2 = cfg2_join(m1, " '"); free(m1);
        char *m3 = cfg2_join(m2, key ? key : ""); free(m2);
        char *m4 = cfg2_join(m3, "': it is managed by your administrator ("); free(m3);
        char *m5 = cfg2_join(m4, cfg2_sval(ms)); free(m4);
        char *msg = cfg2_join(m5, ") and cannot be changed. Contact your administrator to modify it.");
        free(m5);
        json_free(ms);
        json_t *pr = cfg2_print(io, msg, true);
        json_free(pr);
        free(msg);
        return cfg2_make_raise_code("SystemExit", 1);
    }
    json_free(ikm);
    /* level = get_config_backend().locked(get_config_path().parent, key) */
    json_t *backend = cfg2_seam(io, "get_config_backend", NULL);
    if (cfg2_raised(backend)) return backend;
    json_free(backend);
    json_t *cp = cfg2_seam(io, "get_config_path", NULL);
    if (cfg2_raised(cp)) return cp;
    /* get_config_path().parent — a non-Path result raises AttributeError on
     * `.parent` (uncaught here; propagates). */
    if (!cfg2_is_str(cp)) {
        char *m1 = cfg2_join("'", cfg2_py_type_name(cp));
        char *msg = cfg2_join(m1, "' object has no attribute 'parent'");
        free(m1);
        json_free(cp);
        json_t *r = cfg2_make_raise("AttributeError", msg);
        free(msg);
        return r;
    }
    json_free(cp);
    json_t *largs = json_object();
    json_set(largs, "key", json_string(key ? key : ""));
    json_t *level = cfg2_seam(io, "backend_locked", largs);
    if (cfg2_raised(level)) return level;
    if (cfg2_truthy(level)) {
        /* f"Cannot {action} '{key}': it is locked by the {level} level of Remote "
         * f"Config and cannot be changed from this agent." */
        char *m1 = cfg2_join("Cannot ", action ? action : "");
        char *m2 = cfg2_join(m1, " '"); free(m1);
        char *m3 = cfg2_join(m2, key ? key : ""); free(m2);
        char *m4 = cfg2_join(m3, "': it is locked by the "); free(m3);
        char *m5 = cfg2_join(m4, cfg2_sval(level)); free(m4);
        char *msg = cfg2_join(m5, " level of Remote Config and cannot be changed from this agent.");
        free(m5);
        json_free(level);
        json_t *pr = cfg2_print(io, msg, true);
        json_free(pr);
        free(msg);
        return cfg2_make_raise_code("SystemExit", 1);
    }
    json_free(level);
    return json_null();
}

/* ===================================================================
 * _touch_skin_file  (config.py:3101)
 * =================================================================== */
/* PoP: cfg2_touch_skin_file @ hermes_cli/config.py:_touch_skin_file */
json_t *cfg2_touch_skin_file(cfg2_flow_io_t *io, const char *key, const json_t *value) {
    if (!(key && strcmp(key, "display.skin") == 0 && cfg2_is_str(value) && cfg2_truthy(value)))
        return json_null();
    /* try: ... except Exception: pass */
    json_t *gh = cfg2_seam(io, "get_hermes_home", NULL);
    if (cfg2_raised(gh)) {
        if (cfg2_exc_is_exception(json_get_str(cfg2_raise_spec(gh), "type", ""))) {
            json_free(gh); return json_null();
        }
        return gh;
    }
    /* get_hermes_home() yields a Path; a non-Path result (None/bool/...) makes
     * `home / "skins"` raise TypeError, swallowed by the except Exception. */
    if (!cfg2_is_str(gh)) { json_free(gh); return json_null(); }
    char *skins = cfg2_path_join(cfg2_sval(gh), "skins");
    json_free(gh);
    char *fname = cfg2_join(cfg2_sval(value), ".yaml");
    char *skin_path = cfg2_path_join(skins, fname);
    free(skins); free(fname);
    json_t *eargs = json_object();
    json_set(eargs, "path", json_string(skin_path));
    json_t *ex = cfg2_seam(io, "path_exists", eargs);
    if (cfg2_raised(ex)) {
        free(skin_path);
        if (cfg2_exc_is_exception(json_get_str(cfg2_raise_spec(ex), "type", ""))) {
            json_free(ex); return json_null();
        }
        return ex;
    }
    if (cfg2_truthy(ex)) {
        json_t *targs = json_object();
        json_set(targs, "path", json_string(skin_path));
        json_t *t = cfg2_seam(io, "touch", targs);
        if (cfg2_raised(t)) {
            free(skin_path); json_free(ex);
            if (cfg2_exc_is_exception(json_get_str(cfg2_raise_spec(t), "type", ""))) {
                json_free(t); return json_null();
            }
            return t;
        }
        json_free(t);
    }
    json_free(ex);
    free(skin_path);
    return json_null();
}

/* ===================================================================
 * _exit_invalid  (config.py:3113)
 * =================================================================== */
/* PoP: cfg2_exit_invalid @ hermes_cli/config.py:_exit_invalid */
json_t *cfg2_exit_invalid(cfg2_flow_io_t *io, const char *msg) {
    json_t *pr = cfg2_print(io, msg, true);
    json_free(pr);
    return cfg2_make_raise_code("SystemExit", 1);
}

/* ===================================================================
 * _write_user_config  (config.py:3118)
 * =================================================================== */
/* PoP: cfg2_write_user_config @ hermes_cli/config.py:_write_user_config */
json_t *cfg2_write_user_config(cfg2_flow_io_t *io, const char *config_path, const json_t *user_config) {
    json_t *eh = cfg2_seam(io, "ensure_hermes_home", NULL);
    if (cfg2_raised(eh)) return eh;
    json_free(eh);
    json_t *args = json_object();
    json_set(args, "config_path", json_string(config_path ? config_path : ""));
    json_set(args, "user_config", user_config ? json_copy(user_config) : json_null());
    json_t *r = cfg2_seam(io, "recording_raw_config_write", args);
    if (cfg2_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* ===================================================================
 * _print_unknown_key_notice  (config.py:3125)
 * =================================================================== */
/* PoP: cfg2_print_unknown_key_notice @ hermes_cli/config.py:_print_unknown_key_notice */
json_t *cfg2_print_unknown_key_notice(cfg2_flow_io_t *io, const char *key, const char *suggestion) {
    const char *yellow[] = {CFG2_YELLOW, NULL};
    const char *dim[] = {CFG2_DIM, NULL};
    /* f"⚠ '{key}' is not a recognized config key — it was saved anyway, "
     * "but Hermes may not read it." */
    char *m1 = cfg2_join(CFG2_WARN " '", key ? key : "");
    char *m2 = cfg2_join(m1, "' is not a recognized config key " CFG2_EMDASH " it was saved anyway, but Hermes may not read it.");
    free(m1);
    json_t *c1 = cfg2_color(io, m2, yellow);
    free(m2);
    if (cfg2_raised(c1)) return c1;
    json_t *p1 = cfg2_print(io, cfg2_sval(c1), false);
    json_free(c1); json_free(p1);

    if (suggestion && suggestion[0]) {
        char *s1 = cfg2_join("  Did you mean: ", suggestion);
        json_t *c2 = cfg2_color(io, s1, yellow);
        free(s1);
        if (cfg2_raised(c2)) return c2;
        json_t *p2 = cfg2_print(io, cfg2_sval(c2), false);
        json_free(c2); json_free(p2);
    }
    json_t *parts = cfg2_split_key_path(key);
    size_t nseg = json_len(parts);
    json_free(parts);
    const char *footer = (nseg == 1)
        ? "  (Custom top-level keys are supported and bridged to the environment for skills/external tools. Use --force to skip this notice.)"
        : "  (Use --force to skip this notice.)";
    json_t *c3 = cfg2_color(io, footer, dim);
    if (cfg2_raised(c3)) return c3;
    json_t *p3 = cfg2_print(io, cfg2_sval(c3), false);
    json_free(c3); json_free(p3);
    return json_null();
}

/* ===================================================================
 * _dropped_shadow_notes  (config.py:3142)
 * =================================================================== */
/* PoP: cfg2_dropped_shadow_notes @ hermes_cli/config.py:_dropped_shadow_notes */
json_t *cfg2_dropped_shadow_notes(cfg2_flow_io_t *io, const json_t *user_config, const char *key,
                                  const char *legacy_key) {
    json_t *notes = json_array();
    if (legacy_key && legacy_key[0]) {
        json_t *uargs = json_object();
        json_set(uargs, "config", user_config ? json_copy(user_config) : json_null());
        json_set(uargs, "dotted_key", json_string(legacy_key));
        json_t *un = cfg2_seam(io, "unset_nested", uargs);
        if (cfg2_raised(un)) { json_free(notes); return un; }
        if (cfg2_truthy(un)) {
            char *n1 = cfg2_join("  (removed the shadowed ", legacy_key);
            char *note = cfg2_join(n1, " duplicate)");
            free(n1);
            json_append(notes, json_string(note));
            free(note);
        }
        json_free(un);
    }
    json_t *parts = cfg2_split_key_path(key);
    json_t *largs = json_object();
    /* re-fetch user_config: the unset_nested seam's state effect may have
     * replaced state["user_config"], freeing the borrowed param pointer. */
    json_t *uc_now = cfg2_state_get(io, "user_config");
    json_set(largs, "config", uc_now ? json_copy(uc_now) : json_null());
    json_set(largs, "parts", parts);
    json_t *loc = cfg2_seam(io, "locate_nested", largs);
    if (cfg2_raised(loc)) { json_free(notes); return loc; }
    json_t *dargs = json_object();
    json_set(dargs, "loc", loc ? json_copy(loc) : json_null());
    json_t *dl = cfg2_seam(io, "drop_legacy_disabled", dargs);
    if (cfg2_raised(dl)) { json_free(notes); json_free(loc); return dl; }
    if (cfg2_truthy(dl)) {
        json_append(notes, json_string("  (removed the legacy `disabled` key, which would keep the server off)"));
    }
    json_free(dl);
    json_free(loc);
    return notes;
}

/* ===================================================================
 * _usage_exit  (config.py:3385)
 * =================================================================== */
/* PoP: cfg2_usage_exit @ hermes_cli/config.py:_usage_exit */
json_t *cfg2_usage_exit(cfg2_flow_io_t *io, const char *usage, const char **examples, const char **extra) {
    json_t *p;
    p = cfg2_print(io, usage, false); json_free(p);
    p = cfg2_print(io, "", false); json_free(p);
    p = cfg2_print(io, "Examples:", false); json_free(p);
    for (int i = 0; examples && examples[i]; i++) {
        char *line = cfg2_join("  ", examples[i]);
        p = cfg2_print(io, line, false);
        free(line); json_free(p);
    }
    for (int i = 0; extra && extra[i]; i++) {
        p = cfg2_print(io, extra[i], false); json_free(p);
    }
    return cfg2_make_raise_code("SystemExit", 1);
}

/* ===================================================================
 * _run_write_command  (config.py:3396)
 * =================================================================== */
/* PoP: cfg2_run_write_command @ hermes_cli/config.py:_run_write_command */
json_t *cfg2_run_write_command(cfg2_flow_io_t *io, const char *fn_name, json_t *args) {
    json_t *wargs = json_object();
    json_set(wargs, "args", args ? args : json_array());
    json_t *r = cfg2_seam(io, fn_name, wargs);
    if (cfg2_raised(r)) {
        if (cfg2_raise_is(r, "RuntimeError")) {
            const char *m = json_get_str(cfg2_raise_spec(r), "message", "");
            char *msg = cfg2_join(CFG2_CROSS " ", m);
            json_free(r);
            json_t *ei = cfg2_exit_invalid(io, msg);
            free(msg);
            return ei;
        }
        return r;   /* other exceptions propagate */
    }
    json_free(r);
    return json_null();
}

/* ===================================================================
 * _cmd_config_get  (config.py:3418)
 * =================================================================== */
/* PoP: cfg2_cmd_config_get @ hermes_cli/config.py:_cmd_config_get */
json_t *cfg2_cmd_config_get(cfg2_flow_io_t *io, const json_t *args) {
    json_t *key = args ? json_obj_get(args, "key") : NULL;
    if (!cfg2_truthy(key)) {
        return cfg2_usage_exit(io, CFG2_USAGE_GET, CFG2_USAGE_GET_EX, NULL);
    }
    json_t *gargs = json_object();
    json_set(gargs, "key", json_copy(key));
    json_t *jv = args ? json_obj_get(args, "json") : NULL;
    json_set(gargs, "as_json", json_bool(cfg2_truthy(jv)));
    json_t *rv = args ? json_obj_get(args, "raw") : NULL;
    json_set(gargs, "raw", json_bool(cfg2_truthy(rv)));
    /* get_config_value(...) is a statement-position call — the def returns None. */
    json_t *r = cfg2_seam(io, "get_config_value", gargs);
    if (cfg2_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* ===================================================================
 * _cmd_config_set  (config.py:3425)
 * =================================================================== */
/* PoP: cfg2_cmd_config_set @ hermes_cli/config.py:_cmd_config_set */
json_t *cfg2_cmd_config_set(cfg2_flow_io_t *io, const json_t *args) {
    json_t *key = args ? json_obj_get(args, "key") : NULL;
    json_t *value = args ? json_obj_get(args, "value") : NULL;
    if (!cfg2_truthy(key) || cfg2_is_null(value)) {
        return cfg2_usage_exit(io, CFG2_USAGE_SET, CFG2_USAGE_SET_EX, CFG2_USAGE_SET_EXTRA);
    }
    json_t *fv = args ? json_obj_get(args, "force") : NULL;
    json_t *call_args = json_array();
    json_append(call_args, json_copy(key));
    json_append(call_args, json_copy(value));
    json_append(call_args, json_bool(cfg2_truthy(fv)));
    return cfg2_run_write_command(io, "set_config_value", call_args);
}

/* ===================================================================
 * _cmd_config_unset  (config.py:3433)
 * =================================================================== */
/* PoP: cfg2_cmd_config_unset @ hermes_cli/config.py:_cmd_config_unset */
json_t *cfg2_cmd_config_unset(cfg2_flow_io_t *io, const json_t *args) {
    json_t *key = args ? json_obj_get(args, "key") : NULL;
    if (!cfg2_truthy(key)) {
        return cfg2_usage_exit(io, CFG2_USAGE_UNSET, CFG2_USAGE_UNSET_EX, NULL);
    }
    json_t *call_args = json_array();
    json_append(call_args, json_copy(key));
    return cfg2_run_write_command(io, "unset_config_value", call_args);
}

/* ===================================================================
 * _tools_suffix  (config.py:3440)
 * =================================================================== */
/* PoP: cfg2_tools_suffix @ hermes_cli/config.py:_tools_suffix */
json_t *cfg2_tools_suffix(cfg2_flow_io_t *io, const json_t *info, const char *fmt) {
    (void)io;
    json_t *tools = info ? json_obj_get(info, "tools") : NULL;
    if (!tools || tools->type != JSON_ARRAY || json_len(tools) == 0)
        return json_string("");
    /* ", ".join(tools[:2]) */
    size_t n = json_len(tools); if (n > 2) n = 2;
    size_t total = 1;
    for (size_t i = 0; i < n; i++) total += strlen(cfg2_sval(json_get(tools, i))) + 2;
    char *joined = malloc(total);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (i > 0) { memcpy(joined + k, ", ", 2); k += 2; }
        const char *t = cfg2_sval(json_get(tools, i));
        size_t tl = strlen(t); memcpy(joined + k, t, tl); k += tl;
    }
    joined[k] = '\0';
    /* fmt.format(joined) — single "{}" placeholder (fixture domain). */
    const char *f = fmt ? fmt : "";
    const char *ph = strstr(f, "{}");
    char *out;
    if (ph) {
        size_t pre = (size_t)(ph - f);
        size_t post = strlen(ph + 2);
        out = malloc(pre + k + post + 1);
        memcpy(out, f, pre);
        memcpy(out + pre, joined, k);
        memcpy(out + pre + k, ph + 2, post);
        out[pre + k + post] = '\0';
    } else {
        out = strdup(f);
    }
    free(joined);
    json_t *res = json_string(out);
    free(out);
    return res;
}

/* ===================================================================
 * _print_banner  (config.py:3445)
 * =================================================================== */
/* PoP: cfg2_print_banner @ hermes_cli/config.py:_print_banner */
json_t *cfg2_print_banner(cfg2_flow_io_t *io, const char *text) {
    const char *codes[] = {CFG2_CYAN, CFG2_BOLD, NULL};
    json_t *p;
    p = cfg2_print(io, "", false); json_free(p);
    json_t *c = cfg2_color(io, text, codes);
    if (cfg2_raised(c)) return c;
    p = cfg2_print(io, cfg2_sval(c), false);
    json_free(c); json_free(p);
    p = cfg2_print(io, "", false); json_free(p);
    return json_null();
}

/* ===================================================================
 * _cmd_config_show  (config.py:3532)
 * =================================================================== */
/* PoP: cfg2_cmd_config_show @ hermes_cli/config.py:_cmd_config_show */
json_t *cfg2_cmd_config_show(cfg2_flow_io_t *io) {
    json_t *r = cfg2_seam(io, "show_config", NULL);
    if (cfg2_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* ===================================================================
 * _is_plugin_dir_name  (config.py:3583)  — pure
 * =================================================================== */
/* PoP: cfg2_is_plugin_dir_name @ hermes_cli/config.py:_is_plugin_dir_name */
json_t *cfg2_is_plugin_dir_name(cfg2_flow_io_t *io, const char *name) {
    (void)io;
    const char *s = name ? name : "";
    bool dunder = cfg2_startswith(s, "__") && cfg2_endswith(s, "__");
    return json_bool(!dunder);
}

/* ===================================================================
 * _platform_manifest_env_entries  (config.py:3685)  — pure generator
 * =================================================================== */
/* Returns a NEW array of [name, is_secret, meta] triples. */
/* PoP: cfg2_platform_manifest_env_entries @ hermes_cli/config.py:_platform_manifest_env_entries */
json_t *cfg2_platform_manifest_env_entries(cfg2_flow_io_t *io, const json_t *manifest, bool optional) {
    (void)io;
    json_t *out = json_array();
    /* entries = [*(requires_env or []), *((optional_env or []) if optional else [])] */
    json_t *req = manifest ? json_obj_get(manifest, "requires_env") : NULL;
    json_t *opt = manifest ? json_obj_get(manifest, "optional_env") : NULL;
    json_t *entries = json_array();
    if (req && req->type == JSON_ARRAY)
        for (size_t i = 0; i < req->c.count; i++) json_append(entries, json_copy(req->c.items[i]));
    if (optional && opt && opt->type == JSON_ARRAY)
        for (size_t i = 0; i < opt->c.count; i++) json_append(entries, json_copy(opt->c.items[i]));
    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *entry = json_get(entries, i);
        /* meta = {"name":entry} if str else entry if dict else {} */
        json_t *meta;
        if (entry && entry->type == JSON_STRING) {
            meta = json_object();
            json_set(meta, "name", json_copy(entry));
        } else if (entry && entry->type == JSON_OBJECT) {
            meta = json_copy(entry);
        } else {
            meta = json_object();
        }
        json_t *name_v = json_obj_get(meta, "name");
        if (!cfg2_truthy(name_v) || !cfg2_is_str(name_v)) { json_free(meta); continue; }
        const char *name = cfg2_sval(name_v);
        json_t *pw = json_obj_get(meta, "password");
        json_t *sec = json_obj_get(meta, "secret");
        bool is_secret = cfg2_truthy(pw) || cfg2_truthy(sec);
        if (!is_secret && !cfg2_is_false(pw)) {
            char *up = cfg2_upper(name);
            for (int s = 0; CFG2_SECRET_SUFFIXES[s]; s++) {
                if (cfg2_endswith(up, CFG2_SECRET_SUFFIXES[s])) { is_secret = true; break; }
            }
            free(up);
        }
        json_t *triple = json_array();
        json_append(triple, json_string(name));
        json_append(triple, json_bool(is_secret));
        json_append(triple, meta);
        json_append(out, triple);
    }
    json_free(entries);
    return out;
}

/* ===================================================================
 * _manifest_secret_envs  (config.py:3701)
 * =================================================================== */
/* manifests: array of [dir_name, manifest] pairs. core: array or null. */
/* PoP: cfg2_manifest_secret_envs @ hermes_cli/config.py:_manifest_secret_envs */
json_t *cfg2_manifest_secret_envs(cfg2_flow_io_t *io, const json_t *manifests, const json_t *core) {
    /* names = {name.upper() for ... if is_secret and (category or "messaging")=="messaging"} */
    json_t *names = json_array();   /* used as an ordered set (dedup) */
    if (manifests && manifests->type == JSON_ARRAY) {
        for (size_t i = 0; i < manifests->c.count; i++) {
            json_t *pair = manifests->c.items[i];
            json_t *manifest = (pair && pair->type == JSON_ARRAY && json_len(pair) >= 2)
                ? json_get(pair, 1) : NULL;
            json_t *entries = cfg2_platform_manifest_env_entries(io, manifest, true);
            if (cfg2_raised(entries)) { json_free(names); return entries; }
            for (size_t j = 0; j < json_len(entries); j++) {
                json_t *triple = json_get(entries, j);
                const char *name = cfg2_sval(json_get(triple, 0));
                json_t *is_secret_v = json_get(triple, 1);
                bool is_secret = is_secret_v && is_secret_v->type == JSON_BOOL && is_secret_v->bool_val;
                json_t *meta = json_get(triple, 2);
                json_t *cat = meta ? json_obj_get(meta, "category") : NULL;
                const char *category = cfg2_truthy(cat) ? cfg2_sval(cat) : "messaging";
                if (is_secret && strcmp(category, "messaging") == 0) {
                    char *up = cfg2_upper(name);
                    bool dup = false;
                    for (size_t k = 0; k < json_len(names); k++)
                        if (strcmp(cfg2_sval(json_get(names, k)), up) == 0) { dup = true; break; }
                    if (!dup) json_append(names, json_string(up));
                    free(up);
                }
            }
            json_free(entries);
        }
    }
    /* core_names = core if core is not None else core_declared_env_names() */
    json_t *core_names;
    if (!cfg2_is_null(core)) {
        core_names = json_copy(core);
    } else {
        core_names = cfg2_seam(io, "core_declared_env_names", NULL);
        if (cfg2_raised(core_names)) { json_free(names); return core_names; }
    }
    /* names - {n.upper() for n in core_names} */
    json_t *core_up = json_array();
    if (core_names && core_names->type == JSON_ARRAY) {
        for (size_t i = 0; i < core_names->c.count; i++) {
            char *up = cfg2_upper(cfg2_sval(json_get(core_names, i)));
            json_append(core_up, json_string(up));
            free(up);
        }
    }
    json_t *result = json_array();
    for (size_t i = 0; i < json_len(names); i++) {
        const char *nm = cfg2_sval(json_get(names, i));
        bool in_core = false;
        for (size_t k = 0; k < json_len(core_up); k++)
            if (strcmp(cfg2_sval(json_get(core_up, k)), nm) == 0) { in_core = true; break; }
        if (!in_core) json_append(result, json_string(nm));
    }
    json_free(core_up);
    json_free(core_names);
    json_free(names);
    /* frozenset projects to a SORTED array */
    size_t n = json_len(result);
    if (n > 1) {
        const char **tmp = malloc(n * sizeof(char *));
        for (size_t i = 0; i < n; i++) tmp[i] = cfg2_sval(json_get(result, i));
        /* insertion sort by strcmp */
        for (size_t i = 1; i < n; i++) {
            const char *v = tmp[i]; size_t j = i;
            while (j > 0 && strcmp(tmp[j - 1], v) > 0) { tmp[j] = tmp[j - 1]; j--; }
            tmp[j] = v;
        }
        json_t *sorted = json_array();
        for (size_t i = 0; i < n; i++) json_append(sorted, json_string(tmp[i]));
        free(tmp);
        json_free(result);
        result = sorted;
    }
    return result;
}

/* ===================================================================
 * platform_manifest_secret_envs  (config.py:3710)
 * =================================================================== */
/* PoP: cfg2_platform_manifest_secret_envs @ hermes_cli/config.py:platform_manifest_secret_envs */
json_t *cfg2_platform_manifest_secret_envs(cfg2_flow_io_t *io, const char *home, const char *source, bool strict) {
    const char *src = source ? source : "user";
    json_t *bundled = cfg2_state_get(io, "BUNDLED_PLATFORM_SECRET_ENVS");
    if (strcmp(src, "bundled") == 0 && !cfg2_is_null(bundled)) {
        /* frozenset(BUNDLED - {n.upper() for n in core_declared_env_names()}) */
        json_t *core = cfg2_seam(io, "core_declared_env_names", NULL);
        if (cfg2_raised(core)) return core;
        json_t *core_up = json_array();
        if (core && core->type == JSON_ARRAY)
            for (size_t i = 0; i < core->c.count; i++) {
                char *up = cfg2_upper(cfg2_sval(json_get(core, i)));
                json_append(core_up, json_string(up)); free(up);
            }
        json_t *result = json_array();
        if (bundled && bundled->type == JSON_ARRAY) {
            for (size_t i = 0; i < bundled->c.count; i++) {
                const char *nm = cfg2_sval(json_get(bundled, i));
                bool in_core = false;
                for (size_t k = 0; k < json_len(core_up); k++)
                    if (strcmp(cfg2_sval(json_get(core_up, k)), nm) == 0) { in_core = true; break; }
                bool dup = false;
                for (size_t k = 0; k < json_len(result); k++)
                    if (strcmp(cfg2_sval(json_get(result, k)), nm) == 0) { dup = true; break; }
                if (!in_core && !dup) json_append(result, json_string(nm));
            }
        }
        json_free(core_up);
        json_free(core);
        /* sorted */
        size_t n = json_len(result);
        if (n > 1) {
            const char **tmp = malloc(n * sizeof(char *));
            for (size_t i = 0; i < n; i++) tmp[i] = cfg2_sval(json_get(result, i));
            for (size_t i = 1; i < n; i++) {
                const char *v = tmp[i]; size_t j = i;
                while (j > 0 && strcmp(tmp[j - 1], v) > 0) { tmp[j] = tmp[j - 1]; j--; }
                tmp[j] = v;
            }
            json_t *sorted = json_array();
            for (size_t i = 0; i < n; i++) json_append(sorted, json_string(tmp[i]));
            free(tmp); json_free(result); result = sorted;
        }
        return result;
    }
    /* _manifest_secret_envs(_platform_plugin_manifests(home, source, strict=strict)) */
    json_t *margs = json_object();
    json_set(margs, "home", home ? json_string(home) : json_null());
    json_set(margs, "source", json_string(src));
    json_set(margs, "strict", json_bool(strict));
    json_t *manifests = cfg2_seam(io, "platform_plugin_manifests", margs);
    if (cfg2_raised(manifests)) return manifests;
    json_t *out = cfg2_manifest_secret_envs(io, manifests, NULL);
    json_free(manifests);
    return out;
}

/* ===================================================================
 * platform_manifest_secret_scan  (config.py:3720)
 * =================================================================== */
/* PoP: cfg2_platform_manifest_secret_scan @ hermes_cli/config.py:platform_manifest_secret_scan */
json_t *cfg2_platform_manifest_secret_scan(cfg2_flow_io_t *io, const char *home) {
    /* skipped: list = [] — seeded into state so the seam mutation echoes */
    json_t *st = io->state;
    if (st && st->type == JSON_OBJECT && !json_obj_get(st, "skipped"))
        json_set(st, "skipped", json_array());
    json_t *margs = json_object();
    json_set(margs, "home", home ? json_string(home) : json_null());
    json_set(margs, "source", json_string("user"));
    json_set(margs, "strict", json_bool(true));
    json_set(margs, "skipped", json_bool(true));
    json_t *manifests = cfg2_seam(io, "platform_plugin_manifests", margs);
    if (cfg2_raised(manifests)) return manifests;
    json_t *names = cfg2_manifest_secret_envs(io, manifests, NULL);
    json_free(manifests);
    if (cfg2_raised(names)) return names;
    /* return (names, not skipped) */
    json_t *skipped = cfg2_state_get(io, "skipped");
    bool complete = !(skipped && skipped->type == JSON_ARRAY && json_len(skipped) > 0);
    json_t *out = json_array();
    json_append(out, names);
    json_append(out, json_bool(complete));
    return out;
}

/* ===================================================================
 * _ask_yes_no  (config.py:1170)
 * =================================================================== */
/* PoP: cfg2_ask_yes_no @ hermes_cli/config.py:_ask_yes_no */
json_t *cfg2_ask_yes_no(cfg2_flow_io_t *io, const char *prompt) {
    json_t *iargs = json_object();
    json_set(iargs, "prompt", json_string(prompt ? prompt : ""));
    json_t *ans = cfg2_seam(io, "input", iargs);
    char *answer = NULL;
    if (cfg2_raised(ans)) {
        const char *type = json_get_str(cfg2_raise_spec(ans), "type", "");
        if (strcmp(type, "EOFError") == 0 || strcmp(type, "KeyboardInterrupt") == 0) {
            json_free(ans);
            answer = strdup("n");
        } else {
            return ans;   /* other exceptions propagate */
        }
    } else {
        char *stripped = cfg2_strip(cfg2_sval(ans));
        answer = cfg2_lower(stripped);
        free(stripped);
        json_free(ans);
    }
    bool yes = (strcmp(answer, "y") == 0 || strcmp(answer, "yes") == 0);
    free(answer);
    return json_bool(yes);
}

/* ===================================================================
 * _offer_list  (config.py:1277)
 * =================================================================== */
/* PoP: cfg2_offer_list @ hermes_cli/config.py:_offer_list */
json_t *cfg2_offer_list(cfg2_flow_io_t *io, const char *heading, const json_t *items, const char *question) {
    json_t *p;
    p = cfg2_print(io, heading, false); json_free(p);
    if (items && items->type == JSON_ARRAY) {
        for (size_t i = 0; i < items->c.count; i++) {
            char *line = cfg2_join("    " CFG2_BULLET " ", cfg2_sval(items->c.items[i]));
            p = cfg2_print(io, line, false);
            free(line); json_free(p);
        }
    }
    p = cfg2_print(io, "", false); json_free(p);
    json_t *yn = cfg2_ask_yes_no(io, question);
    if (cfg2_raised(yn)) return yn;
    bool yes = yn && yn->type == JSON_BOOL && yn->bool_val;
    json_free(yn);
    if (!yes) {
        p = cfg2_print(io, "  Set later with: hermes config set <key> <value>", false);
        json_free(p);
        return json_bool(false);
    }
    p = cfg2_print(io, "", false); json_free(p);
    return json_bool(true);
}

/* ===================================================================
 * _prompt_and_save_env  (config.py:1159)
 * =================================================================== */
/* PoP: cfg2_prompt_and_save_env @ hermes_cli/config.py:_prompt_and_save_env */
json_t *cfg2_prompt_and_save_env(cfg2_flow_io_t *io, const char *name, const json_t *info,
                                 const char *prompt, json_t *results) {
    json_t *pw = info ? json_obj_get(info, "password") : NULL;
    json_t *val;
    if (cfg2_truthy(pw)) {
        json_t *margs = json_object();
        json_set(margs, "prompt", json_string(prompt ? prompt : ""));
        val = cfg2_seam(io, "masked_secret_prompt", margs);
    } else {
        json_t *largs = json_object();
        json_set(largs, "prompt", json_string(prompt ? prompt : ""));
        json_t *li = cfg2_seam(io, "line_input", largs);
        if (cfg2_raised(li)) return li;
        char *stripped = cfg2_strip(cfg2_sval(li));
        val = json_string(stripped);
        free(stripped);
        json_free(li);
    }
    if (cfg2_raised(val)) return val;
    if (!cfg2_truthy(val)) { json_free(val); return json_bool(false); }
    /* save_env_value(name, value) */
    json_t *sargs = json_object();
    json_set(sargs, "name", json_string(name ? name : ""));
    json_set(sargs, "value", json_copy(val));
    json_t *sv = cfg2_seam(io, "save_env_value", sargs);
    if (cfg2_raised(sv)) { json_free(val); return sv; }
    json_free(sv);
    /* results["env_added"].append(name) */
    if (results && results->type == JSON_OBJECT) {
        json_t *ea = json_obj_get(results, "env_added");
        if (!ea || ea->type != JSON_ARRAY) {
            ea = json_array();
            json_set(results, "env_added", ea);
        }
        json_append(ea, json_string(name ? name : ""));
    }
    char *m1 = cfg2_join("  " CFG2_CHECK " Saved ", name ? name : "");
    json_t *p = cfg2_print(io, m1, false);
    free(m1); json_free(p);
    json_free(val);
    return json_bool(true);
}