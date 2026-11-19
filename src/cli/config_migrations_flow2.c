/* config_migrations_flow2.c — FLOW-core C11 port of the UNCLAIMED remainder of
 * hermes_cli/config_migrations.py (the table-driven config migration registry).
 *
 * Sibling of the claimed config_migrations.c / config_migration_flow.c ports:
 * the claimed siblings (_cfg, support_floor_message, run_migrations and the
 * _migrate_to_12..33 steps) keep their PoP credit and are SEAMED here where a
 * ported flow calls them (_cfg is the only claimed sibling these flows call).
 * This file ports the 20 unclaimed defs / partials:
 *   _local_files_allowed, read_raw_config, _persist_migration, _dict_at,
 *   _commit, _rewrite_key, _rewrite_stale_default, _rewrite_stale_defaults,
 *   _lower_is, _installed_user_plugins, _migrate_to_41, _migrate_to_44,
 *   _migrate_to_45, _migrate_to_46, _migrate_to_48, _migrate_to_49,
 *   _migrate_to_50, _prompt_caching_child_lines, _migrate_to_54,
 *   disable_suspicious_mcp_servers.
 *
 * Universal seam: every collaborator / I/O call goes through
 * cmg2_seam(io, name, args). The dispatcher appends `name` to io->trace,
 * returns the scripted result, applies "state" effects, and models a raised
 * exception with {"__raise__": {type,message,code?,name?}}. An "import_raise"
 * script entry models a failed in-function import: cmg2_import_check() calls
 * the seam with an {"__import__": name} arg; the dispatcher returns
 * {"__raise__": spec, "__untrace__": true} (token retracted — the live import
 * statement records nothing) or {"__import_ok__": true} on success (token also
 * retracted). The __untrace__ flag lets a consumer tell an import failure
 * (caught by `except Exception`) from a call raise (propagates).
 *
 * AUTO seams (no script, never fail closed): "print" (captures args.text into
 * io->cap_stdout; a scripted "raise" entry still fires), and the two
 * state-backed config-module seams "config.read_raw_config" (returns a copy of
 * io->state.raw_config, or a scripted "ret"/"raise" override) and
 * "config._persist_migration" (sets io->state.raw_config = copy(args.config),
 * or a scripted "raise"). Every OTHER seam is SCRIPTED and FAIL-CLOSED: a
 * missing / exhausted script raises RuntimeError("unscripted seam: <name>").
 *
 * io->state is a json OBJECT mirroring the on-disk config document:
 *   raw_config   the persisted config (read_raw_config / _persist_migration).
 * The caller's `results` dict is a separate input the flows mutate in place
 * (config_added / warnings lists); the harness echoes it as out.results.
 *
 * Representation contract (both sides): paths are posix strings (FakePath
 * double; / and .name are lexical, filesystem probes are seams); set results
 * project to SORTED arrays; an uncaught exception rides as
 * {"__raise__": {type,message,code?,name?}}; SystemExit carries code; a
 * scripted KeyError message is the RAW key (both sides repr it). Integral JSON
 * numbers read as int (libjson has no int/float identity); bool is distinct
 * from int in the oracle's deep_eq. Fixture input domain is ASCII; the few
 * non-ASCII glyphs in message CONSTANTS (check/arrow/warn/info/em-dash) are
 * byte-pinned UTF-8 literals, not fixture data.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * / io->cap_stdout are harness-owned. cmg2_seam() frees its args. Every ported
 * function returns a NEW reference (or a raise sentinel). _dict_at returns a
 * BORROWED child when the value is a dict (writes alias) or a FRESH object
 * (flagged via *fresh, caller frees) otherwise — mirroring live aliasing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── byte-pinned message constants (generated from the quarry source) ────── */
#define CMG2_M41_ADD_PRE "removed legacy Bot Mode section from SOUL.md ("
#define CMG2_M41_PRINT_PRE "  \xe2" "\x9c" "\x93 Removed the plugin-era 'Messaging other agents' section from SOUL.md ("
#define CMG2_M41_PRINT_SUF ") \xe2" "\x80" "\x94 Bot Chat sessions now get the live roster instead."
#define CMG2_M45_ADD_PRE "enabled the connections toolset for "
#define CMG2_M45_PRINT_PRE "  \xe2" "\x9c" "\x93 Enabled the Connections toolset (Gmail, Linear, Notion, local MCP servers) for "
#define CMG2_M45_PRINT_SUF ". Uncheck Connections in `hermes tools` to turn it off."
#define CMG2_M46_ADD_PRE "mcp_servers: disabled \xe2" "\x86" "\x92 enabled: false ("
#define CMG2_M46_ADD_SUF ")"
#define CMG2_M46_PRINT_PRE "  \xe2" "\x9c" "\x93 Turned off MCP servers the profile editor had marked disabled: "
#define CMG2_M46_PRINT_SUF "."
#define CMG2_M48_ADD_PRE "terminal."
#define CMG2_M48_ADD_MID " unset (follows the default, "
#define CMG2_M48_ADD_SUF ")"
#define CMG2_M48_PRINT_PRE "  \xe2" "\x9c" "\x93 terminal."
#define CMG2_M48_PRINT_MID ": was the old default; now follows the default sandbox image ("
#define CMG2_M48_PRINT_SUF ")"
#define CMG2_M49_ADD_PRE "terminal.vercel_runtime unset (fresh sandboxes use terminal.vercel_image, "
#define CMG2_M49_ADD_SUF ")"
#define CMG2_M49_PRINT_PRE "  \xe2" "\x9c" "\x93 terminal.vercel_runtime: was the old default; fresh sandboxes now use the managed image ("
#define CMG2_M49_PRINT_SUF ")"
#define CMG2_M49_ENV_PRINT "  \xe2" "\x9c" "\x93 Cleared TERMINAL_VERCEL_RUNTIME from .env (was the old default; the image is used instead)"
#define CMG2_M50_ADD "removed security.tirith_* (scanner no longer bundled)"
#define CMG2_M50_PRINT "  \xe2" "\x9c" "\x93 Removed security.tirith_* \xe2" "\x80" "\x94 the tirith scanner is no longer bundled with Hermes."
#define CMG2_M54_INFO "  \xe2" "\x84" "\xb9 prompt_caching.cache_ttl: \"5m\" pins every session to the 5m cache tier; the default is now \"auto\" (1h for sessions you type into). Remove the line to follow it."
#define CMG2_M54_ADD "removed prompt_caching.cache_ttl: \"5m\" (seeded by the config template)"
#define CMG2_M54_PRINT "  \xe2" "\x9c" "\x93 Removed the template-seeded prompt_caching.cache_ttl: \"5m\". Prompt caching now follows \"auto\": the 1h tier for sessions you type into, 5m for subagents, cron and other machine-paced runs. Set cache_ttl: \"5m\" to pin the old behaviour."
#define CMG2_MDS_WARN_PRE "  \xe2" "\x9a" "\xa0 "
#define CMG2_MDS_ADD_PRE "Disabled suspicious MCP server '"
#define CMG2_MDS_ADD_SUF "'"
#define CMG2_MDS_PRINT_PRE "  \xe2" "\x9a" "\xa0 Disabled MCP server '"
#define CMG2_MDS_PRINT_SUF "' pending review"
#define CMG2_M44_ROW0_ADD "curator.stale_after_days=14 (was: 30)"
#define CMG2_M44_ROW0_PRINT "  \xe2" "\x9c" "\x93 curator.stale_after_days 30\xe2" "\x86" "\x92" "14 \xe2" "\x80" "\x94 unused skills are flagged stale after two weeks."
#define CMG2_M44_ROW1_ADD "curator.archive_after_days=30 (was: 90)"
#define CMG2_M44_ROW1_PRINT "  \xe2" "\x9c" "\x93 curator.archive_after_days 90\xe2" "\x86" "\x92" "30 \xe2" "\x80" "\x94 skills unused for a month are archived to skills/.archive/ (recoverable with `hermes curator restore`). Set it back to 90 to keep the old window."

/* ── module constants (quarry config_migrations.py / config_defaults.py) ─── */
#define CMG2_PROTOCOL_HEADING "## Messaging other agents"
#define CMG2_DEFAULT_SANDBOX_IMAGE "nousresearch/hermes-sandbox:desktop"
#define CMG2_DEFAULT_VERCEL_IMAGE "vercel/sandbox/universal:latest"
#define CMG2_LEGACY_VERCEL_RUNTIME "node24"
static const char *CMG2_LEGACY_SANDBOX_IMAGES[] = {
    "nikolaik/python-nodejs:python3.11-nodejs20",
    "nikolaik/python-nodejs:python3.14-nodejs22",
    NULL,
};
static const char *CMG2_RETIRED_TIRITH_KEYS[] = {
    "tirith_enabled", "tirith_path", "tirith_timeout", "tirith_fail_open", NULL,
};

/* ── seam dispatcher + trace + state ─────────────────────────────────────── */
typedef struct cmg2_flow_io_t {
    json_t *(*call)(struct cmg2_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;        /* ARRAY of string tokens (harness-owned) */
    json_t *state;        /* OBJECT: raw_config mirror (harness-owned) */
    json_t *cap_stdout;   /* ARRAY of captured print text (harness-owned) */
    void   *ctx;
} cmg2_flow_io_t;

static void cmg2_trace(cmg2_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *cmg2_call(cmg2_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

static bool cmg2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    json_t *s = json_obj_get(r, "__raise__");
    return s && s->type != JSON_NULL;
}

static json_t *cmg2_raise_spec(const json_t *r) {
    if (!cmg2_raised(r)) return NULL;
    return json_obj_get(r, "__raise__");
}

static json_t *cmg2_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

static void cmg2_retract_token(cmg2_flow_io_t *io) {
    if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
        size_t last = io->trace->c.count - 1;
        json_free(io->trace->c.items[last]);
        io->trace->c.count = last;
    }
}

/* seam: trace the attempt, dispatch, free args. __import_ok__ / __untrace__
 * results retract the trace token (a live import statement records nothing). */
static json_t *cmg2_seam(cmg2_flow_io_t *io, const char *name, json_t *args) {
    cmg2_trace(io, name);
    json_t *r = cmg2_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__import_ok__")) {
        cmg2_retract_token(io);
        json_free(r);
        return json_null();
    }
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        cmg2_retract_token(io);
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = json_object();
        json_set(out, "__raise__", spec ? json_copy(spec) : json_null());
        json_set(out, "__untrace__", json_bool(true));
        json_free(r);
        return out;
    }
    return r;
}

/* in-function import check: peek the designated script for import_raise. */
static json_t *cmg2_import_check(cmg2_flow_io_t *io, const char *script_name) {
    json_t *args = json_object();
    json_set(args, "__import__", json_string(script_name));
    return cmg2_seam(io, script_name, args);
}

/* print(text) — AUTO capture seam (a scripted raise still fires). */
static json_t *cmg2_print(cmg2_flow_io_t *io, const char *text) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    return cmg2_seam(io, "print", args);
}

static json_t *cmg2_state_get(cmg2_flow_io_t *io, const char *key) {
    if (!io || !io->state || io->state->type != JSON_OBJECT) return NULL;
    return json_obj_get(io->state, key);
}

/* ── full builtin exception hierarchy (py3.14; for except-family tests) ──── */
typedef struct { const char *name; const char *parent; } cmg2_exc_t;
static const cmg2_exc_t CMG2_EXC_TABLE[] = {
    {"BaseException", NULL},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    /* ExceptionGroup has two bases (BaseExceptionGroup, Exception); collapsed
     * to the Exception branch for catch-family tests (it IS an Exception). */
    {"ExceptionGroup", "Exception"},
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
    /* aliases of OSError (IOError is OSError, EnvironmentError is OSError,
     * WindowsError is OSError on Windows) — parent chain resolves via OSError. */
    {"IOError", "OSError"},
    {"EnvironmentError", "OSError"},
    {"WindowsError", "OSError"},
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

static const char *cmg2_exc_parent(const char *type) {
    if (!type) return NULL;
    for (int i = 0; CMG2_EXC_TABLE[i].name; i++)
        if (strcmp(type, CMG2_EXC_TABLE[i].name) == 0) return CMG2_EXC_TABLE[i].parent;
    return NULL;
}

static bool cmg2_exc_known(const char *type) {
    if (!type) return false;
    for (int i = 0; CMG2_EXC_TABLE[i].name; i++)
        if (strcmp(type, CMG2_EXC_TABLE[i].name) == 0) return true;
    return false;
}

/* isinstance(exc, Exception): walk to "Exception". Unknown kinds model the
 * oracle make_exc fallback (a synthetic Exception subclass) -> true. */
static bool cmg2_exc_is_exception(const char *type) {
    if (!cmg2_exc_known(type)) return true;
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = cmg2_exc_parent(cur);
    }
    return false;
}

/* isinstance(exc, OSError): walk to "OSError". Unknown -> false. */
static bool cmg2_exc_is_oserror(const char *type) {
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "OSError") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = cmg2_exc_parent(cur);
    }
    return false;
}

static const char *cmg2_raise_type(const json_t *r) {
    json_t *spec = cmg2_raise_spec(r);
    return spec ? json_get_str(spec, "type", "") : "";
}
/* ── Python-faithful value helpers ───────────────────────────────────────── */
static bool cmg2_truthy(const json_t *v) {
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
static bool cmg2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cmg2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cmg2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool cmg2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool cmg2_is_bool_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }
static const char *cmg2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* type(x).__name__ for a JSON value (integral numbers read as int). */
static const char *cmg2_py_type_name(const json_t *v) {
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

/* Python == over JSON values: bool cross-equals int (True==1), numbers compare
 * numerically, containers structurally (dict order-independent). */
static bool cmg2_py_eq(const json_t *a, const json_t *b) {
    if (cmg2_is_null(a) && cmg2_is_null(b)) return true;
    if (!a || !b) return false;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING) return strcmp(cmg2_sval(a), cmg2_sval(b)) == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!cmg2_py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !cmg2_py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* array membership using Python == (str needle matches only equal values). */
static bool cmg2_member(const json_t *arr, const json_t *needle) {
    if (!cmg2_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (cmg2_py_eq(arr->c.items[i], needle)) return true;
    return false;
}
static bool cmg2_member_str(const json_t *arr, const char *s) {
    json_t *n = json_string(s);
    bool r = cmg2_member(arr, n);
    json_free(n);
    return r;
}

/* Python repr of a str (single quotes preferred; double when contains ' not "). */
static char *cmg2_repr_str(const char *s) {
    const char *p = s ? s : "";
    bool has_sq = strchr(p, '\'') != NULL;
    bool has_dq = strchr(p, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(p);
    char *out = malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c == (unsigned char)q || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) { k += (size_t)sprintf(out + k, "\\x%02x", c); }
        else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

/* forward decl for container repr */
static char *cmg2_py_repr(const json_t *v);

/* str(x) projection (scalar domain; containers repr'd for robustness). */
static char *cmg2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 1e15)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return strdup(buf);
        }
        case JSON_STRING: return strdup(cmg2_sval(v));
        default: return cmg2_py_repr(v);
    }
}

static char *cmg2_py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER || v->type == JSON_STRING) return cmg2_py_str(v);
    if (v->type == JSON_ARRAY) {
        size_t cap = 64; char *out = malloc(cap); size_t k = 0;
        out[k++] = '[';
        for (size_t i = 0; i < v->c.count; i++) {
            char *r = cmg2_py_repr(v->c.items[i]);
            size_t rl = strlen(r);
            while (k + rl + 4 > cap) { cap *= 2; out = realloc(out, cap); }
            if (i) { out[k++] = ','; out[k++] = ' '; }
            memcpy(out + k, r, rl); k += rl; free(r);
        }
        out[k++] = ']'; out[k] = '\0'; return out;
    }
    /* object */
    size_t cap = 64; char *out = malloc(cap); size_t k = 0;
    out[k++] = '{';
    for (size_t i = 0; i < v->c.count; i++) {
        char *kr = cmg2_repr_str(v->c.keys[i]);
        char *vr = cmg2_py_repr(v->c.items[i]);
        size_t need = strlen(kr) + strlen(vr) + 6;
        while (k + need > cap) { cap *= 2; out = realloc(out, cap); }
        if (i) { out[k++] = ','; out[k++] = ' '; }
        k += (size_t)sprintf(out + k, "%s: %s", kr, vr);
        free(kr); free(vr);
    }
    out[k++] = '}'; out[k] = '\0'; return out;
}

/* Unicode White_Space predicate (str.strip set). */
static bool cmg2_is_ws_cp(unsigned long cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d)) return true;
    if (cp >= 0x1c && cp <= 0x1f) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
static unsigned long cmg2_next_cp(const char *s, size_t n, size_t *i) {
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
static char *cmg2_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    if (n == 0) return strdup("");
    size_t cap = n;
    size_t *starts = malloc(cap * sizeof(size_t));
    size_t *ends = malloc(cap * sizeof(size_t));
    unsigned long *cps = malloc(cap * sizeof(unsigned long));
    if (!starts || !ends || !cps) { free(starts); free(ends); free(cps); return strdup(p); }
    size_t m = 0, i = 0;
    while (i < n) {
        size_t st = i;
        unsigned long cp = cmg2_next_cp(p, n, &i);
        starts[m] = st; ends[m] = i; cps[m] = cp; m++;
    }
    size_t a = 0, b = m;
    while (a < b && cmg2_is_ws_cp(cps[a])) a++;
    while (b > a && cmg2_is_ws_cp(cps[b - 1])) b--;
    size_t from = (a < m) ? starts[a] : n;
    size_t to = (b > 0 && b <= m) ? ends[b - 1] : from;
    if (b == a) { from = n; to = n; }
    char *out = malloc(to - from + 1);
    if (!out) { free(starts); free(ends); free(cps); return NULL; }
    memcpy(out, p + from, to - from);
    out[to - from] = '\0';
    free(starts); free(ends); free(cps);
    return out;
}
/* ASCII lower (fixture domain is ASCII). */
static char *cmg2_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

static char *cmg2_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}
static char *cmg2_join3(const char *a, const char *b, const char *c) {
    char *t = cmg2_join(a, b); char *r = cmg2_join(t, c); free(t); return r;
}

/* FakePath.__truediv__ model: base.rstrip('/') (empty when base=="/"), then
 * ALWAYS one '/' separator + name. Absolute posix paths (fixture domain). */
static char *cmg2_path_join(const char *base, const char *name) {
    const char *b = base ? base : "";
    size_t nb = strlen(b);
    if (nb == 1 && b[0] == '/') nb = 0;
    else while (nb > 0 && b[nb - 1] == '/') nb--;
    const char *nm = name ? name : "";
    char *out = malloc(nb + 1 + strlen(nm) + 1);
    if (!out) return NULL;
    memcpy(out, b, nb);
    out[nb] = '/';
    strcpy(out + nb + 1, nm);
    return out;
}
/* FakePath.name: rstrip('/').rsplit('/',1)[-1]. */
static char *cmg2_path_name(const char *path) {
    const char *p = path ? path : "";
    size_t n = strlen(p);
    while (n > 0 && p[n - 1] == '/') n--;
    size_t start = n;
    while (start > 0 && p[start - 1] != '/') start--;
    char *out = malloc(n - start + 1);
    if (!out) return NULL;
    memcpy(out, p + start, n - start);
    out[n - start] = '\0';
    return out;
}

/* AttributeError "'<type>' object has no attribute '<name>'". */
static json_t *cmg2_attr_raise(const json_t *carrier, const char *name) {
    char *m1 = cmg2_join("'", cmg2_py_type_name(carrier));
    char *m2 = cmg2_join(m1, "' object has no attribute '"); free(m1);
    char *m3 = cmg2_join(m2, name); free(m2);
    char *msg = cmg2_join(m3, "'"); free(m3);
    json_t *r = cmg2_make_raise("AttributeError", msg);
    free(msg);
    return r;
}
/* TypeError "unsupported operand type(s) for /: '<a>' and '<b>'". */
static json_t *cmg2_truediv_raise(const json_t *a, const char *btype) {
    char *m1 = cmg2_join("unsupported operand type(s) for /: '", cmg2_py_type_name(a));
    char *m2 = cmg2_join(m1, "' and '"); free(m1);
    char *m3 = cmg2_join(m2, btype); free(m2);
    char *msg = cmg2_join(m3, "'"); free(m3);
    json_t *r = cmg2_make_raise("TypeError", msg);
    free(msg);
    return r;
}
/* KeyError with the RAW key (message repr'd to match str(KeyError(k))). */
static json_t *cmg2_key_raise(const char *key) {
    char *rk = cmg2_repr_str(key);
    json_t *r = cmg2_make_raise("KeyError", rk);
    free(rk);
    return r;
}
/* results[key].append — KeyError when absent, AttributeError when non-list. */
static json_t *cmg2_results_append(json_t *results, const char *key, json_t *value) {
    json_t *arr = json_obj_get(results, key);
    if (!arr) return cmg2_key_raise(key);
    if (arr->type != JSON_ARRAY) return cmg2_attr_raise(arr, "append");
    json_append(arr, value);   /* takes ownership */
    return NULL;
}

/* sorted unique string array (strcmp order). Returns NEW array of strings. */
static json_t *cmg2_sorted_unique_strs(json_t *strs) {
    size_t n = cmg2_is_arr(strs) ? strs->c.count : 0;
    char **v = malloc((n + 1) * sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        char *s = cmg2_py_str(strs->c.items[i]);
        bool dup = false;
        for (size_t j = 0; j < m; j++) if (strcmp(v[j], s) == 0) { dup = true; break; }
        if (dup) free(s); else v[m++] = s;
    }
    for (size_t i = 0; i + 1 < m; i++)
        for (size_t j = i + 1; j < m; j++)
            if (strcmp(v[i], v[j]) > 0) { char *t = v[i]; v[i] = v[j]; v[j] = t; }
    json_t *out = json_array();
    for (size_t i = 0; i < m; i++) { json_append(out, json_string(v[i])); free(v[i]); }
    free(v);
    return out;
}
/* ", ".join of an array of strings. malloc'd. */
static char *cmg2_join_comma(const json_t *arr) {
    size_t n = cmg2_is_arr(arr) ? arr->c.count : 0;
    size_t cap = 64; char *out = malloc(cap); size_t k = 0; out[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        const char *s = cmg2_sval(arr->c.items[i]);
        size_t sl = strlen(s);
        while (k + sl + 4 > cap) { cap *= 2; out = realloc(out, cap); }
        if (i) { out[k++] = ','; out[k++] = ' '; }
        memcpy(out + k, s, sl); k += sl; out[k] = '\0';
    }
    return out;
}

/* ── match / extra_guard predicates (descriptor-driven) ──────────────────── */
static bool cmg2_lower_is_match(const char *word, const json_t *cur) {
    if (!cmg2_is_str(cur)) return false;
    char *st = cmg2_strip(cmg2_sval(cur));
    char *lo = cmg2_lower(st);
    bool r = strcmp(lo, word ? word : "") == 0;
    free(st); free(lo);
    return r;
}
static bool cmg2_pred_match(const json_t *desc, const json_t *cur) {
    const char *kind = json_get_str(desc, "kind", "");
    if (strcmp(kind, "eq") == 0) return cmg2_py_eq(cur, json_obj_get(desc, "value"));
    if (strcmp(kind, "lower_is") == 0) return cmg2_lower_is_match(json_get_str(desc, "word", ""), cur);
    if (strcmp(kind, "none_or_lower_is") == 0)
        return cmg2_is_null(cur) || cmg2_lower_is_match(json_get_str(desc, "word", ""), cur);
    if (strcmp(kind, "not_none") == 0) return !cmg2_is_null(cur);
    if (strcmp(kind, "always_true") == 0) return true;
    return false;
}
static bool cmg2_pred_guard(const json_t *desc, const json_t *raw) {
    if (!desc) return true;   /* default lambda _m: True */
    const char *kind = json_get_str(desc, "kind", "always_true");
    const char *key = json_get_str(desc, "key", "");
    if (strcmp(kind, "always_true") == 0) return true;
    if (strcmp(kind, "is_true") == 0) return cmg2_is_bool_true(json_obj_get(raw, key));
    if (strcmp(kind, "has_key") == 0) return json_has(raw, key);
    if (strcmp(kind, "lacks_key") == 0) return !json_has(raw, key);
    return true;
}

/* ── str.splitlines (ASCII boundaries: \n \r \r\n \v \f \x1c \x1d \x1e) ───── */
static json_t *cmg2_splitlines(const char *text) {
    json_t *out = json_array();
    const char *p = text ? text : "";
    size_t n = strlen(p), start = 0, i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)p[i];
        bool brk = (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
                    c == 0x1c || c == 0x1d || c == 0x1e);
        if (brk) {
            char *line = malloc(i - start + 1);
            memcpy(line, p + start, i - start); line[i - start] = '\0';
            json_append(out, json_string(line)); free(line);
            if (c == '\r' && i + 1 < n && p[i + 1] == '\n') i++;
            i++; start = i;
        } else i++;
    }
    if (start < n) {
        char *line = malloc(n - start + 1);
        memcpy(line, p + start, n - start); line[n - start] = '\0';
        json_append(out, json_string(line)); free(line);
    }
    return out;
}

/* _TOP_LEVEL_KEY.match(line): ^(?:["']?)([A-Za-z_][\w-]*)(?:["']?)[ \t]*:
 * Returns malloc'd group(1) or NULL when no match. ASCII \w domain. */
static char *cmg2_top_level_key(const char *line) {
    const char *p = line ? line : "";
    size_t i = 0, n = strlen(p);
    if (i < n && (p[i] == '"' || p[i] == '\'')) i++;   /* optional open quote */
    if (i >= n || !((p[i] >= 'A' && p[i] <= 'Z') || (p[i] >= 'a' && p[i] <= 'z') || p[i] == '_'))
        return NULL;
    size_t ks = i;
    while (i < n) {
        char c = p[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') i++;
        else break;
    }
    size_t ke = i;
    if (i < n && (p[i] == '"' || p[i] == '\'')) i++;   /* optional close quote */
    while (i < n && (p[i] == ' ' || p[i] == '\t')) i++;
    if (i >= n || p[i] != ':') return NULL;
    char *g = malloc(ke - ks + 1);
    memcpy(g, p + ks, ke - ks); g[ke - ks] = '\0';
    return g;
}

/* _TEMPLATE_CACHE_TTL_LINE.fullmatch(line) (MULTILINE; lines carry no \n):
 * ^[ \t]+cache_ttl:[ \t]*(["']?)5m\1[ \t]*#[ \t]*use "1h" for long sessions
 *   with pauses between turns[ \t]*$ */
static bool cmg2_template_ttl_line(const char *line) {
    const char *p = line ? line : "";
    size_t i = 0, n = strlen(p);
    size_t ws = i;
    while (i < n && (p[i] == ' ' || p[i] == '\t')) i++;
    if (i == ws) return false;   /* [ \t]+ requires >= 1 */
    static const char *lit1 = "cache_ttl:";
    size_t l1 = strlen(lit1);
    if (strncmp(p + i, lit1, l1) != 0) return false;
    i += l1;
    while (i < n && (p[i] == ' ' || p[i] == '\t')) i++;
    char quote = 0;
    if (i < n && (p[i] == '"' || p[i] == '\'')) { quote = p[i]; i++; }
    if (strncmp(p + i, "5m", 2) != 0) return false;
    i += 2;
    if (quote) { if (i >= n || p[i] != quote) return false; i++; }
    while (i < n && (p[i] == ' ' || p[i] == '\t')) i++;
    if (i >= n || p[i] != '#') return false;
    i++;
    while (i < n && (p[i] == ' ' || p[i] == '\t')) i++;
    static const char *lit2 = "use \"1h\" for long sessions with pauses between turns";
    size_t l2 = strlen(lit2);
    if (strncmp(p + i, lit2, l2) != 0) return false;
    i += l2;
    while (i < n && (p[i] == ' ' || p[i] == '\t')) i++;
    return i == n;   /* fullmatch */
}
/* ===================================================================
 * _local_files_allowed  (config_migrations.py:38)
 * =================================================================== */
/* PoP: cmg2_local_files_allowed @ hermes_cli/config_migrations.py:_local_files_allowed */
static json_t *cmg2_local_files_allowed(cmg2_flow_io_t *io) {
    json_t *r = cmg2_seam(io, "CONFIG_ONLY.get", json_object());
    if (cmg2_raised(r)) return r;
    json_t *out = json_bool(!cmg2_truthy(r));
    json_free(r);
    return out;
}

/* ===================================================================
 * read_raw_config  (config_migrations.py:62)  — _cfg().read_raw_config()
 * =================================================================== */
/* PoP: cmg2_read_raw_config @ hermes_cli/config_migrations.py:read_raw_config */
static json_t *cmg2_read_raw_config(cmg2_flow_io_t *io) {
    json_t *cfg = cmg2_seam(io, "cfg", json_object());
    if (cmg2_raised(cfg)) return cfg;
    json_free(cfg);   /* module identity is opaque; only the method call matters */
    return cmg2_seam(io, "config.read_raw_config", json_object());
}

/* ===================================================================
 * _persist_migration  (config_migrations.py:66)
 * =================================================================== */
/* PoP: cmg2_persist_migration @ hermes_cli/config_migrations.py:_persist_migration */
static json_t *cmg2_persist_migration(cmg2_flow_io_t *io, const json_t *config) {
    json_t *cfg = cmg2_seam(io, "cfg", json_object());
    if (cmg2_raised(cfg)) return cfg;
    json_free(cfg);
    json_t *args = json_object();
    json_set(args, "config", config ? json_copy(config) : json_null());
    return cmg2_seam(io, "config._persist_migration", args);
}

/* ===================================================================
 * _dict_at  (config_migrations.py:70)  — config[key] if dict else fresh {}
 * Returns a BORROWED child (aliasing) or a FRESH object (*fresh=true).
 * =================================================================== */
/* PoP: cmg2_dict_at @ hermes_cli/config_migrations.py:_dict_at */
static json_t *cmg2_dict_at(const json_t *config, const char *key, bool *fresh) {
    json_t *v = json_obj_get(config, key);
    if (cmg2_is_obj(v)) { *fresh = false; return v; }
    *fresh = true;
    return json_object();
}

/* ===================================================================
 * _commit  (config_migrations.py:76)
 * =================================================================== */
/* PoP: cmg2_commit @ hermes_cli/config_migrations.py:_commit */
static json_t *cmg2_commit(cmg2_flow_io_t *io, const json_t *config, json_t *results,
                           bool quiet, const char *added, const char *message) {
    json_t *r = cmg2_persist_migration(io, config);
    if (cmg2_raised(r)) return r;
    json_free(r);
    if (added && added[0]) {   /* if added: (non-empty str) */
        json_t *err = cmg2_results_append(results, "config_added", json_string(added));
        if (err) return err;
    }
    if (message && message[0] && !quiet) {
        r = cmg2_print(io, message);
        if (cmg2_raised(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* ===================================================================
 * _lower_is  (config_migrations.py:147)  — factory; predicate applied by caller
 * =================================================================== */
/* PoP: cmg2_lower_is @ hermes_cli/config_migrations.py:_lower_is */
static json_t *cmg2_lower_is(const char *word) {
    json_t *d = json_object();
    json_set(d, "kind", json_string("lower_is"));
    json_set(d, "word", json_string(word ? word : ""));
    return d;
}

/* ===================================================================
 * _rewrite_stale_default  (config_migrations.py:119)  — partial factory
 * Projects to {"__partial__":"_rewrite_key","kw":{...,match:{kind:eq,value:old}}}.
 * =================================================================== */
/* PoP: cmg2_rewrite_stale_default @ hermes_cli/config_migrations.py:_rewrite_stale_default */
static json_t *cmg2_rewrite_stale_default(const json_t *old, const json_t *kw) {
    json_t *out = json_object();
    json_set(out, "__partial__", json_string("_rewrite_key"));
    json_t *kwo = json_object();
    if (cmg2_is_obj(kw)) {
        for (size_t i = 0; i < kw->c.count; i++)
            json_set(kwo, kw->c.keys[i], json_copy(kw->c.items[i]));
    }
    json_t *match = json_object();
    json_set(match, "kind", json_string("eq"));
    json_set(match, "value", old ? json_copy(old) : json_null());
    json_set(kwo, "match", match);
    json_set(out, "kw", kwo);
    return out;
}

/* ===================================================================
 * _rewrite_key  (config_migrations.py:90)
 * =================================================================== */
/* PoP: cmg2_rewrite_key @ hermes_cli/config_migrations.py:_rewrite_key */
static json_t *cmg2_rewrite_key(cmg2_flow_io_t *io, json_t *results, bool quiet,
                                const char *section, const char *key,
                                const json_t *match, const json_t *new_val,
                                const char *added, const char *message,
                                const json_t *extra_guard, bool create_section) {
    json_t *config = cmg2_read_raw_config(io);
    if (cmg2_raised(config)) return config;
    if (!cmg2_is_obj(config)) { json_t *e = cmg2_attr_raise(config, "get"); json_free(config); return e; }
    json_t *raw = json_obj_get(config, section);
    bool raw_fresh = false;
    if (!cmg2_is_obj(raw)) {
        if (!create_section) { json_free(config); return json_null(); }
        raw = json_object(); raw_fresh = true;
    }
    json_t *cur = json_obj_get(raw, key);   /* None when absent */
    if (cmg2_pred_match(match, cur) && cmg2_pred_guard(extra_guard, raw)) {
        if (cmg2_is_null(new_val)) {   /* new is None -> del raw[key] */
            if (!json_has(raw, key)) {
                json_t *e = cmg2_key_raise(key);
                if (raw_fresh) json_free(raw);
                json_free(config);
                return e;
            }
            json_obj_del(raw, key);
        } else {
            json_set(raw, key, json_copy(new_val));
        }
        if (raw_fresh) { json_set(config, section, raw); raw_fresh = false; }
        /* else raw aliases config[section]; live config[section]=raw is a no-op */
        json_t *r = cmg2_commit(io, config, results, quiet, added, message);
        json_free(config);
        return r;
    }
    if (raw_fresh) json_free(raw);
    json_free(config);
    return json_null();
}

/* ===================================================================
 * _rewrite_stale_defaults  (config_migrations.py:125)  — ONE write for all due rows
 * rows: array of [key, old, new, added, message].
 * =================================================================== */
/* PoP: cmg2_rewrite_stale_defaults @ hermes_cli/config_migrations.py:_rewrite_stale_defaults */
static json_t *cmg2_rewrite_stale_defaults(cmg2_flow_io_t *io, json_t *results, bool quiet,
                                           const char *section, const json_t *rows) {
    json_t *config = cmg2_read_raw_config(io);
    if (cmg2_raised(config)) return config;
    if (!cmg2_is_obj(config)) { json_t *e = cmg2_attr_raise(config, "get"); json_free(config); return e; }
    bool raw_fresh = false;
    json_t *raw = cmg2_dict_at(config, section, &raw_fresh);
    size_t nrows = cmg2_is_arr(rows) ? rows->c.count : 0;
    /* due = rows whose raw.get(key) == old */
    size_t *due = malloc((nrows + 1) * sizeof(size_t));
    size_t ndue = 0;
    for (size_t i = 0; i < nrows; i++) {
        json_t *row = rows->c.items[i];
        if (!cmg2_is_arr(row) || row->c.count < 5) continue;
        const char *key = cmg2_sval(row->c.items[0]);
        json_t *cur = json_obj_get(raw, key);
        if (cmg2_py_eq(cur, row->c.items[1])) due[ndue++] = i;
    }
    if (ndue == 0) {
        free(due);
        if (raw_fresh) json_free(raw);
        json_free(config);
        return json_null();
    }
    for (size_t d = 0; d < ndue; d++) {
        json_t *row = rows->c.items[due[d]];
        const char *key = cmg2_sval(row->c.items[0]);
        json_t *new_val = row->c.items[2];
        if (cmg2_is_null(new_val)) {
            if (!json_has(raw, key)) {
                json_t *e = cmg2_key_raise(key);
                free(due);
                if (raw_fresh) json_free(raw);
                json_free(config);
                return e;
            }
            json_obj_del(raw, key);
        } else {
            json_set(raw, key, json_copy(new_val));
        }
    }
    /* NOTE: live _rewrite_stale_defaults does NOT write config[section]=raw
     * (unlike _rewrite_key). A fresh raw (absent section) stays detached, so
     * its mutations are NOT persisted — only a borrowed (present) raw aliases
     * config. Do not attach raw here. */
    json_t *r = cmg2_persist_migration(io, config);
    if (cmg2_raised(r)) { free(due); if (raw_fresh) json_free(raw); json_free(config); return r; }
    json_free(r);
    for (size_t d = 0; d < ndue; d++) {
        json_t *row = rows->c.items[due[d]];
        const char *added = cmg2_sval(row->c.items[3]);
        const char *message = cmg2_sval(row->c.items[4]);
        json_t *err = cmg2_results_append(results, "config_added", json_string(added));
        if (err) { free(due); if (raw_fresh) json_free(raw); json_free(config); return err; }
        if (!quiet) {
            r = cmg2_print(io, message);
            if (cmg2_raised(r)) { free(due); if (raw_fresh) json_free(raw); json_free(config); return r; }
            json_free(r);
        }
    }
    free(due);
    if (raw_fresh) json_free(raw);
    json_free(config);
    return json_null();
}

/* ===================================================================
 * _prompt_caching_child_lines  (config_migrations.py:766)  — pure
 * =================================================================== */
/* PoP: cmg2_prompt_caching_child_lines @ hermes_cli/config_migrations.py:_prompt_caching_child_lines */
static json_t *cmg2_prompt_caching_child_lines(const json_t *text) {
    if (!cmg2_is_str(text)) return cmg2_attr_raise(text, "splitlines");
    json_t *lines = cmg2_splitlines(cmg2_sval(text));
    json_t *block = json_array();
    size_t n = lines->c.count;
    for (size_t i = 0; i < n; i++) {
        char *g = cmg2_top_level_key(cmg2_sval(lines->c.items[i]));
        bool is_pc = g && strcmp(g, "prompt_caching") == 0;
        free(g);
        if (!is_pc) continue;
        long indent = -1;   /* None */
        for (size_t j = i + 1; j < n; j++) {
            const char *child = cmg2_sval(lines->c.items[j]);
            size_t cl = strlen(child);
            size_t s = 0;
            while (s < cl && (child[s] == ' ' || child[s] == '\t')) s++;
            if (s == cl || child[s] == '#') continue;   /* blank or comment */
            if (s == 0) break;   /* back at column 0: block ended */
            long width = (long)s;
            if (indent < 0) indent = width;
            if (width == indent) json_append(block, json_string(child));
        }
        break;   /* only the first prompt_caching block */
    }
    json_free(lines);
    return block;
}
/* ===================================================================
 * _installed_user_plugins  (config_migrations.py:322)
 * outer try/except Exception -> return []; inner try (open+fast_safe_load)
 * except Exception -> manifest={}. BaseException kinds pass through both.
 * =================================================================== */
/* PoP: cmg2_installed_user_plugins @ hermes_cli/config_migrations.py:_installed_user_plugins */
static json_t *cmg2_installed_user_plugins(cmg2_flow_io_t *io, const json_t *disabled) {
    json_t *found = json_array();
    json_t *pend = NULL;
    json_t *home = NULL, *it = NULL, *sorted = NULL, *manifest = NULL;
    char *pdir = NULL, *mf = NULL;

    json_t *cfg = cmg2_seam(io, "cfg", json_object());
    if (cmg2_raised(cfg)) { json_free(found); return cfg; }   /* _c=_cfg() is OUTSIDE try */
    json_free(cfg);

    home = cmg2_seam(io, "config.get_hermes_home", json_object());
    if (cmg2_raised(home)) { pend = home; home = NULL; goto outer; }
    if (!cmg2_is_str(home)) { pend = cmg2_truediv_raise(home, "str"); goto outer; }
    pdir = cmg2_path_join(cmg2_sval(home), "plugins");

    json_t *r = cmg2_seam(io, "path.is_dir", ({ json_t *a = json_object(); json_set(a, "path", json_string(pdir)); a; }));
    if (cmg2_raised(r)) { pend = r; goto outer; }
    bool is_dir = cmg2_truthy(r);
    json_free(r);
    if (is_dir) {
        it = cmg2_seam(io, "path.iterdir", ({ json_t *a = json_object(); json_set(a, "path", json_string(pdir)); a; }));
        if (cmg2_raised(it)) { pend = it; it = NULL; goto outer; }
        if (!cmg2_is_arr(it)) {
            char *m1 = cmg2_join("'", cmg2_py_type_name(it));
            char *msg = cmg2_join(m1, "' object is not iterable"); free(m1);
            pend = cmg2_make_raise("TypeError", msg); free(msg);
            goto outer;
        }
        sorted = json_copy(it);
        /* sort child path strings (strcmp) — mirrors sorted(FakePath) by str */
        for (size_t i = 0; i + 1 < sorted->c.count; i++)
            for (size_t j = i + 1; j < sorted->c.count; j++)
                if (strcmp(cmg2_sval(sorted->c.items[i]), cmg2_sval(sorted->c.items[j])) > 0) {
                    json_t *t = sorted->c.items[i]; sorted->c.items[i] = sorted->c.items[j]; sorted->c.items[j] = t;
                }
        for (size_t ci = 0; ci < sorted->c.count; ci++) {
            const char *child = cmg2_sval(sorted->c.items[ci]);
            json_t *cd = cmg2_seam(io, "path.is_dir", ({ json_t *a = json_object(); json_set(a, "path", json_string(child)); a; }));
            if (cmg2_raised(cd)) { pend = cd; goto outer; }
            bool cdir = cmg2_truthy(cd); json_free(cd);
            if (!cdir) continue;
            free(mf); mf = cmg2_path_join(child, "plugin.yaml");
            json_t *ex = cmg2_seam(io, "path.exists", ({ json_t *a = json_object(); json_set(a, "path", json_string(mf)); a; }));
            if (cmg2_raised(ex)) { pend = ex; goto outer; }
            bool exists = cmg2_truthy(ex); json_free(ex);
            if (!exists) { free(mf); mf = cmg2_path_join(child, "plugin.yml"); }
            /* live has a SECOND `if not manifest_file.exists(): continue` */
            ex = cmg2_seam(io, "path.exists", ({ json_t *a = json_object(); json_set(a, "path", json_string(mf)); a; }));
            if (cmg2_raised(ex)) { pend = ex; goto outer; }
            exists = cmg2_truthy(ex); json_free(ex);
            if (!exists) continue;
            /* inner try: open + fast_safe_load */
            json_t *op = cmg2_seam(io, "open", ({ json_t *a = json_object(); json_set(a, "path", json_string(mf)); json_set(a, "encoding", json_string("utf-8-sig")); a; }));
            if (cmg2_raised(op)) {
                if (cmg2_exc_is_exception(cmg2_raise_type(op))) { json_free(op); manifest = json_object(); }
                else { pend = op; goto outer; }
            } else {
                json_free(op);
                json_t *fs = cmg2_seam(io, "config.fast_safe_load", ({ json_t *a = json_object(); json_set(a, "file", json_string(mf)); a; }));
                if (cmg2_raised(fs)) {
                    if (cmg2_exc_is_exception(cmg2_raise_type(fs))) { json_free(fs); manifest = json_object(); }
                    else { pend = fs; goto outer; }
                } else if (cmg2_truthy(fs)) {
                    manifest = fs;   /* owned */
                } else {
                    json_free(fs); manifest = json_object();
                }
            }
            /* name = manifest.get("name") or child.name  (OUTSIDE inner try) */
            if (!cmg2_is_obj(manifest)) { pend = cmg2_attr_raise(manifest, "get"); goto outer; }
            json_t *namev = json_obj_get(manifest, "name");
            json_t *name;
            if (cmg2_truthy(namev)) name = json_copy(namev);
            else { char *cn = cmg2_path_name(child); name = json_string(cn); free(cn); }
            json_free(manifest); manifest = NULL;
            if (cmg2_is_arr(name) || cmg2_is_obj(name)) {
                char *m1 = cmg2_join("cannot use '", cmg2_py_type_name(name));
                char *m2 = cmg2_join(m1, "' as a set element (unhashable type: '"); free(m1);
                char *m3 = cmg2_join(m2, cmg2_py_type_name(name)); free(m2);
                char *msg = cmg2_join(m3, "')"); free(m3);
                pend = cmg2_make_raise("TypeError", msg); free(msg);
                json_free(name);
                goto outer;
            }
            if (!cmg2_member(disabled, name)) json_append(found, name);
            else json_free(name);
        }
    }
    /* success */
    json_free(home); free(pdir); json_free(it); json_free(sorted); free(mf); json_free(manifest);
    return found;
outer:
    json_free(home); free(pdir); json_free(it); json_free(sorted); free(mf); json_free(manifest);
    if (cmg2_exc_is_exception(cmg2_raise_type(pend))) {
        json_free(pend); json_free(found);
        return json_array();   /* except Exception: return [] */
    }
    json_free(found);
    return pend;   /* BaseException propagates */
}

/* ===================================================================
 * _migrate_to_41  (config_migrations.py:578)  — SOUL.md legacy section strip
 * per-profile try/except OSError -> continue; other kinds propagate.
 * =================================================================== */
/* PoP: cmg2_migrate_to_41 @ hermes_cli/config_migrations.py:_migrate_to_41 */
static json_t *cmg2_migrate_to_41(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *r = cmg2_local_files_allowed(io);
    if (cmg2_raised(r)) return r;
    bool allowed = cmg2_truthy(r); json_free(r);
    if (!allowed) return json_null();
    /* imports (not in a try -> any raise propagates) */
    r = cmg2_import_check(io, "get_hermes_home");
    if (cmg2_raised(r)) return r;
    json_free(r);
    r = cmg2_import_check(io, "_roster");
    if (cmg2_raised(r)) return r;
    json_free(r);

    json_t *home = cmg2_seam(io, "get_hermes_home", json_object());
    if (cmg2_raised(home)) return home;
    json_t *root = cmg2_seam(io, "_hermes_root", ({ json_t *a = json_object(); json_set(a, "home", json_copy(home)); a; }));
    json_free(home);
    if (cmg2_raised(root)) return root;
    json_t *roster = cmg2_seam(io, "_roster", ({ json_t *a = json_object(); json_set(a, "root", json_copy(root)); a; }));
    json_free(root);
    if (cmg2_raised(roster)) return roster;
    if (!cmg2_is_arr(roster)) {
        char *m1 = cmg2_join("'", cmg2_py_type_name(roster));
        char *msg = cmg2_join(m1, "' object is not iterable"); free(m1);
        json_t *e = cmg2_make_raise("TypeError", msg); free(msg);
        json_free(roster);
        return e;
    }
    json_t *cleaned = json_array();
    for (size_t i = 0; i < roster->c.count; i++) {
        json_t *item = roster->c.items[i];
        if (!cmg2_is_arr(item)) {
            char *m1 = cmg2_join("cannot unpack non-iterable ", cmg2_py_type_name(item));
            char *msg = cmg2_join(m1, " object"); free(m1);
            json_t *e = cmg2_make_raise("TypeError", msg); free(msg);
            json_free(cleaned); json_free(roster); return e;
        }
        if (item->c.count != 2) {
            char buf[96];
            if (item->c.count > 2) snprintf(buf, sizeof buf, "too many values to unpack (expected 2, got %zu)", item->c.count);
            else snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", item->c.count);
            json_t *e = cmg2_make_raise("ValueError", buf);
            json_free(cleaned); json_free(roster); return e;
        }
        json_t *name = item->c.items[0];
        json_t *profile = item->c.items[1];
        if (!cmg2_is_str(profile)) {
            json_t *e = cmg2_truediv_raise(profile, "str");
            json_free(cleaned); json_free(roster); return e;
        }
        char *soul = cmg2_path_join(cmg2_sval(profile), "SOUL.md");
        /* try region (except OSError) */
        json_t *isf = cmg2_seam(io, "path.is_file", ({ json_t *a = json_object(); json_set(a, "path", json_string(soul)); a; }));
        if (cmg2_raised(isf)) {
            if (cmg2_exc_is_oserror(cmg2_raise_type(isf))) { json_free(isf); free(soul); continue; }
            json_free(cleaned); json_free(roster); free(soul); return isf;
        }
        bool is_file = cmg2_truthy(isf); json_free(isf);
        json_t *text;
        if (is_file) {
            text = cmg2_seam(io, "path.read_text", ({ json_t *a = json_object(); json_set(a, "path", json_string(soul)); json_set(a, "encoding", json_string("utf-8-sig")); a; }));
            if (cmg2_raised(text)) {
                if (cmg2_exc_is_oserror(cmg2_raise_type(text))) { json_free(text); free(soul); continue; }
                json_free(cleaned); json_free(roster); free(soul); return text;
            }
            /* strip a leading UTF-8 BOM (utf-8-sig) */
            if (cmg2_is_str(text) && strncmp(text->str_val, "\xef\xbb\xbf", 3) == 0) {
                char *nb = strdup(text->str_val + 3);
                json_free(text); text = json_string(nb); free(nb);
            }
        } else {
            text = json_string("");
        }
        if (!cmg2_is_str(text)) {
            char *m1 = cmg2_join("argument of type '", cmg2_py_type_name(text));
            char *msg = cmg2_join(m1, "' is not a container or iterable"); free(m1);
            json_t *e = cmg2_make_raise("TypeError", msg); free(msg);
            json_free(text); json_free(cleaned); json_free(roster); free(soul); return e;
        }
        if (strstr(cmg2_sval(text), CMG2_PROTOCOL_HEADING)) {
            json_t *stripped = cmg2_seam(io, "strip_legacy_protocol", ({ json_t *a = json_object(); json_set(a, "text", json_copy(text)); a; }));
            if (cmg2_raised(stripped)) {
                if (cmg2_exc_is_oserror(cmg2_raise_type(stripped))) { json_free(stripped); json_free(text); free(soul); continue; }
                json_free(text); json_free(cleaned); json_free(roster); free(soul); return stripped;
            }
            json_t *wt = cmg2_seam(io, "path.write_text", ({ json_t *a = json_object(); json_set(a, "path", json_string(soul)); json_set(a, "text", json_copy(stripped)); json_set(a, "encoding", json_string("utf-8")); a; }));
            json_free(stripped);
            if (cmg2_raised(wt)) {
                if (cmg2_exc_is_oserror(cmg2_raise_type(wt))) { json_free(wt); json_free(text); free(soul); continue; }
                json_free(text); json_free(cleaned); json_free(roster); free(soul); return wt;
            }
            json_free(wt);
            json_append(cleaned, json_copy(name));
        }
        json_free(text);
        free(soul);
    }
    json_free(roster);
    if (cleaned->c.count > 0) {
        char *js = cmg2_join_comma(cleaned);
        char *added = cmg2_join3(CMG2_M41_ADD_PRE, js, ")");
        json_t *err = cmg2_results_append(results, "config_added", json_string(added));
        free(js); free(added);
        if (err) { json_free(cleaned); return err; }
        if (!quiet) {
            char *js2 = cmg2_join_comma(cleaned);
            char *p2 = cmg2_join3(CMG2_M41_PRINT_PRE, js2, CMG2_M41_PRINT_SUF);
            free(js2);
            json_t *pr = cmg2_print(io, p2);
            free(p2);
            if (cmg2_raised(pr)) { json_free(cleaned); return pr; }
            json_free(pr);
        }
    }
    json_free(cleaned);
    return json_null();
}

/* ===================================================================
 * _migrate_to_44  (config_migrations.py:728)  — partial(_rewrite_stale_defaults)
 * =================================================================== */
/* PoP: cmg2_migrate_to_44 @ hermes_cli/config_migrations.py:_migrate_to_44 */
static json_t *cmg2_migrate_to_44(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *rows = json_array();
    json_t *r0 = json_array();
    json_append(r0, json_string("stale_after_days"));
    json_append(r0, json_number(30));
    json_append(r0, json_number(14));
    json_append(r0, json_string(CMG2_M44_ROW0_ADD));
    json_append(r0, json_string(CMG2_M44_ROW0_PRINT));
    json_append(rows, r0);
    json_t *r1 = json_array();
    json_append(r1, json_string("archive_after_days"));
    json_append(r1, json_number(90));
    json_append(r1, json_number(30));
    json_append(r1, json_string(CMG2_M44_ROW1_ADD));
    json_append(r1, json_string(CMG2_M44_ROW1_PRINT));
    json_append(rows, r1);
    json_t *r = cmg2_rewrite_stale_defaults(io, results, quiet, "curator", rows);
    json_free(rows);
    return r;
}

/* ===================================================================
 * _migrate_to_45  (config_migrations.py:604)  — connections toolset backfill
 * =================================================================== */
/* PoP: cmg2_migrate_to_45 @ hermes_cli/config_migrations.py:_migrate_to_45 */
static json_t *cmg2_migrate_to_45(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *r = cmg2_import_check(io, "parse_config_string_list");
    if (cmg2_raised(r)) return r;
    json_free(r);
    r = cmg2_import_check(io, "_configurable_keys");
    if (cmg2_raised(r)) return r;
    json_free(r);
    r = cmg2_import_check(io, "toolset_allowed_for_platform");
    if (cmg2_raised(r)) return r;
    json_free(r);

    json_t *config = cmg2_read_raw_config(io);
    if (cmg2_raised(config)) return config;
    if (!cmg2_is_obj(config)) { json_t *e = cmg2_attr_raise(config, "get"); json_free(config); return e; }
    json_t *saved = json_obj_get(config, "platform_toolsets");
    if (!cmg2_is_obj(saved)) { json_free(config); return json_null(); }

    bool agent_fresh = false;
    json_t *agent = cmg2_dict_at(config, "agent", &agent_fresh);
    json_t *dis = json_obj_get(agent, "disabled_toolsets");
    json_t *pl = cmg2_seam(io, "parse_config_string_list", ({ json_t *a = json_object(); json_set(a, "value", dis ? json_copy(dis) : json_null()); a; }));
    if (agent_fresh) json_free(agent);
    if (cmg2_raised(pl)) { json_free(config); return pl; }
    /* live: "connections" in parse_config_string_list(...) — a non-container
     * result raises TypeError (py3.14 message). */
    if (!cmg2_is_arr(pl)) {
        char *m1 = cmg2_join("argument of type '", cmg2_py_type_name(pl));
        char *msg = cmg2_join(m1, "' is not a container or iterable"); free(m1);
        json_t *e = cmg2_make_raise("TypeError", msg); free(msg);
        json_free(pl); json_free(config);
        return e;
    }
    bool dis_conn = cmg2_member_str(pl, "connections");
    json_free(pl);
    if (dis_conn) { json_free(config); return json_null(); }

    bool known_fresh = false;
    json_t *known = cmg2_dict_at(config, "known_builtin_toolsets", &known_fresh);
    json_t *ck = cmg2_seam(io, "_configurable_keys", json_object());
    if (cmg2_raised(ck)) { if (known_fresh) json_free(known); json_free(config); return ck; }
    json_t *pk = cmg2_seam(io, "_get_plugin_toolset_keys", json_object());
    if (cmg2_raised(pk)) { json_free(ck); if (known_fresh) json_free(known); json_free(config); return pk; }
    /* explicit_keys = set(ck) | set(pk); membership only */
    json_t *enabled_for = json_array();
    for (size_t i = 0; i < saved->c.count; i++) {
        const char *platform = saved->c.keys[i];
        json_t *toolsets = saved->c.items[i];
        if (!cmg2_is_arr(toolsets) || cmg2_member_str(toolsets, "connections")) continue;
        json_t *ta = cmg2_seam(io, "toolset_allowed_for_platform", ({ json_t *a = json_object(); json_set(a, "toolset", json_string("connections")); json_set(a, "platform", json_string(platform)); a; }));
        if (cmg2_raised(ta)) { json_free(ck); json_free(pk); json_free(enabled_for); if (known_fresh) json_free(known); json_free(config); return ta; }
        bool allowed = cmg2_truthy(ta); json_free(ta);
        if (!allowed) continue;
        bool any_explicit = false;
        for (size_t j = 0; j < toolsets->c.count; j++) {
            char *ts = cmg2_py_str(toolsets->c.items[j]);
            if (cmg2_member_str(ck, ts) || cmg2_member_str(pk, ts)) any_explicit = true;
            free(ts);
            if (any_explicit) break;
        }
        if (!any_explicit) continue;
        json_t *offered = json_obj_get(known, platform);
        if (cmg2_is_arr(offered) && cmg2_member_str(offered, "connections")) continue;
        json_t *newset = json_copy(toolsets);
        json_append(newset, json_string("connections"));
        json_t *sorted_new = cmg2_sorted_unique_strs(newset);
        json_free(newset);
        json_set(saved, platform, sorted_new);
        if (cmg2_is_arr(offered)) {
            json_t *off2 = json_copy(offered);
            json_append(off2, json_string("connections"));
            json_t *sorted_off = cmg2_sorted_unique_strs(off2);
            json_free(off2);
            json_set(known, platform, sorted_off);
        }
        json_append(enabled_for, json_string(platform));
    }
    json_free(ck); json_free(pk);
    if (enabled_for->c.count == 0) {
        json_free(enabled_for);
        if (known_fresh) json_free(known);
        json_free(config);
        return json_null();
    }
    json_set(config, "platform_toolsets", json_copy(saved));
    if (cmg2_truthy(known)) {
        if (known_fresh) { json_set(config, "known_builtin_toolsets", known); known_fresh = false; }
        else json_set(config, "known_builtin_toolsets", json_copy(known));
    }
    json_t *sorted_plats = cmg2_sorted_unique_strs(enabled_for);
    char *plats = cmg2_join_comma(sorted_plats);
    json_free(sorted_plats);
    char *added = cmg2_join3(CMG2_M45_ADD_PRE, plats, "");
    char *p1 = cmg2_join3(CMG2_M45_PRINT_PRE, plats, CMG2_M45_PRINT_SUF);
    json_t *cr = cmg2_commit(io, config, results, quiet, added, p1);
    free(plats); free(added); free(p1);
    json_free(enabled_for);
    if (known_fresh) json_free(known);
    json_free(config);
    return cr;
}

/* ===================================================================
 * _migrate_to_46  (config_migrations.py:651)  — disabled:true -> enabled:false
 * =================================================================== */
/* PoP: cmg2_migrate_to_46 @ hermes_cli/config_migrations.py:_migrate_to_46 */
static json_t *cmg2_migrate_to_46(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *r = cmg2_import_check(io, "_parse_boolish");
    if (cmg2_raised(r)) return r;
    json_free(r);
    json_t *config = cmg2_read_raw_config(io);
    if (cmg2_raised(config)) return config;
    if (!cmg2_is_obj(config)) { json_t *e = cmg2_attr_raise(config, "get"); json_free(config); return e; }
    json_t *servers = json_obj_get(config, "mcp_servers");
    if (!cmg2_is_obj(servers)) { json_free(config); return json_null(); }
    json_t *turned_off = json_array();
    for (size_t i = 0; i < servers->c.count; i++) {
        const char *name = servers->c.keys[i];
        json_t *e = servers->c.items[i];
        if (!cmg2_is_obj(e) || !json_has(e, "disabled")) continue;
        json_t *pb = cmg2_seam(io, "_parse_boolish", ({ json_t *a = json_object(); json_set(a, "value", json_copy(json_obj_get(e, "disabled"))); json_set(a, "default", json_bool(false)); a; }));
        if (cmg2_raised(pb)) { json_free(turned_off); json_free(config); return pb; }
        bool off = cmg2_truthy(pb); json_free(pb);
        if (off) json_append(turned_off, json_string(name));
    }
    /* sorted(key=str) */
    for (size_t i = 0; i + 1 < turned_off->c.count; i++)
        for (size_t j = i + 1; j < turned_off->c.count; j++)
            if (strcmp(cmg2_sval(turned_off->c.items[i]), cmg2_sval(turned_off->c.items[j])) > 0) {
                json_t *t = turned_off->c.items[i]; turned_off->c.items[i] = turned_off->c.items[j]; turned_off->c.items[j] = t;
            }
    if (turned_off->c.count == 0) { json_free(turned_off); json_free(config); return json_null(); }
    for (size_t i = 0; i < turned_off->c.count; i++) {
        const char *name = cmg2_sval(turned_off->c.items[i]);
        json_t *e = json_obj_get(servers, name);
        json_obj_del(e, "disabled");
        json_set(e, "enabled", json_bool(false));
    }
    char *names = cmg2_join_comma(turned_off);
    char *added = cmg2_join3(CMG2_M46_ADD_PRE, names, CMG2_M46_ADD_SUF);
    char *msg = cmg2_join3(CMG2_M46_PRINT_PRE, names, CMG2_M46_PRINT_SUF);
    json_t *cr = cmg2_commit(io, config, results, quiet, added, msg);
    free(names); free(added); free(msg);
    json_free(turned_off);
    json_free(config);
    return cr;
}
/* ===================================================================
 * _migrate_to_48  (config_migrations.py:677)  — drop stale sandbox images
 * =================================================================== */
/* PoP: cmg2_migrate_to_48 @ hermes_cli/config_migrations.py:_migrate_to_48 */
static json_t *cmg2_migrate_to_48(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    static const char *keys[] = {"docker_image", "modal_image", "daytona_image", "singularity_image", NULL};
    json_t *rows = json_array();
    for (int li = 0; CMG2_LEGACY_SANDBOX_IMAGES[li]; li++) {
        const char *legacy = CMG2_LEGACY_SANDBOX_IMAGES[li];
        char *sing = cmg2_join("docker://", legacy);
        for (int ki = 0; keys[ki]; ki++) {
            const char *key = keys[ki];
            const char *old = (strcmp(key, "singularity_image") == 0) ? sing : legacy;
            char *a1 = cmg2_join3(CMG2_M48_ADD_PRE, key, CMG2_M48_ADD_MID);
            char *a2 = cmg2_join3(a1, CMG2_DEFAULT_SANDBOX_IMAGE, CMG2_M48_ADD_SUF); free(a1);
            char *m1 = cmg2_join3(CMG2_M48_PRINT_PRE, key, CMG2_M48_PRINT_MID);
            char *m2 = cmg2_join3(m1, CMG2_DEFAULT_SANDBOX_IMAGE, CMG2_M48_PRINT_SUF); free(m1);
            json_t *row = json_array();
            json_append(row, json_string(key));
            json_append(row, json_string(old));
            json_append(row, json_null());
            json_append(row, json_string(a2));
            json_append(row, json_string(m2));
            json_append(rows, row);
            free(a2); free(m2);
        }
        free(sing);
    }
    json_t *r = cmg2_rewrite_stale_defaults(io, results, quiet, "terminal", rows);
    json_free(rows);
    return r;
}

/* ===================================================================
 * _migrate_to_49  (config_migrations.py:702)  — drop seeded Vercel runtime
 * =================================================================== */
/* PoP: cmg2_migrate_to_49 @ hermes_cli/config_migrations.py:_migrate_to_49 */
static json_t *cmg2_migrate_to_49(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    char *added = cmg2_join3(CMG2_M49_ADD_PRE, CMG2_DEFAULT_VERCEL_IMAGE, CMG2_M49_ADD_SUF);
    char *message = cmg2_join3(CMG2_M49_PRINT_PRE, CMG2_DEFAULT_VERCEL_IMAGE, CMG2_M49_PRINT_SUF);
    json_t *match = json_object();
    json_set(match, "kind", json_string("eq"));
    json_set(match, "value", json_string(CMG2_LEGACY_VERCEL_RUNTIME));
    json_t *r = cmg2_rewrite_key(io, results, quiet, "terminal", "vercel_runtime",
                                 match, json_null(), added, message, NULL, false);
    json_free(match); free(added); free(message);
    if (cmg2_raised(r)) return r;
    json_free(r);

    json_t *cfg = cmg2_seam(io, "cfg", json_object());
    if (cmg2_raised(cfg)) return cfg;
    json_free(cfg);
    r = cmg2_local_files_allowed(io);
    if (cmg2_raised(r)) return r;
    bool allowed = cmg2_truthy(r); json_free(r);
    if (!allowed) return json_null();   /* the .env mirror is a local file */

    json_t *ev = cmg2_seam(io, "config.get_env_value_prefer_dotenv", ({ json_t *a = json_object(); json_set(a, "key", json_string("TERMINAL_VERCEL_RUNTIME")); a; }));
    if (cmg2_raised(ev)) return ev;
    /* (ev or "").strip() */
    const char *raw = cmg2_truthy(ev) ? cmg2_sval(ev) : "";
    if (cmg2_truthy(ev) && !cmg2_is_str(ev)) {
        json_t *e = cmg2_attr_raise(ev, "strip");
        json_free(ev);
        return e;
    }
    char *st = cmg2_strip(raw);
    bool match_env = strcmp(st, CMG2_LEGACY_VERCEL_RUNTIME) == 0;
    free(st);
    json_free(ev);
    if (match_env) {
        json_t *rm = cmg2_seam(io, "config.remove_env_value", ({ json_t *a = json_object(); json_set(a, "key", json_string("TERMINAL_VERCEL_RUNTIME")); a; }));
        if (cmg2_raised(rm)) return rm;
        json_free(rm);
        if (!quiet) {
            json_t *pr = cmg2_print(io, CMG2_M49_ENV_PRINT);
            if (cmg2_raised(pr)) return pr;
            json_free(pr);
        }
    }
    return json_null();
}

/* ===================================================================
 * _migrate_to_50  (config_migrations.py:740)  — drop retired tirith keys
 * =================================================================== */
/* PoP: cmg2_migrate_to_50 @ hermes_cli/config_migrations.py:_migrate_to_50 */
static json_t *cmg2_migrate_to_50(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *config = cmg2_read_raw_config(io);
    if (cmg2_raised(config)) return config;
    if (!cmg2_is_obj(config)) { json_t *e = cmg2_attr_raise(config, "get"); json_free(config); return e; }
    json_t *security = json_obj_get(config, "security");
    if (!cmg2_is_obj(security)) { json_free(config); return json_null(); }
    json_t *present = json_array();
    for (int i = 0; CMG2_RETIRED_TIRITH_KEYS[i]; i++)
        if (json_has(security, CMG2_RETIRED_TIRITH_KEYS[i]))
            json_append(present, json_string(CMG2_RETIRED_TIRITH_KEYS[i]));
    if (present->c.count == 0) { json_free(present); json_free(config); return json_null(); }
    for (size_t i = 0; i < present->c.count; i++)
        json_obj_del(security, cmg2_sval(present->c.items[i]));
    json_free(present);
    json_t *r = cmg2_commit(io, config, results, quiet, CMG2_M50_ADD, CMG2_M50_PRINT);
    json_free(config);
    return r;
}

/* ===================================================================
 * _migrate_to_54  (config_migrations.py:789)  — template-seeded cache_ttl
 * =================================================================== */
/* PoP: cmg2_migrate_to_54 @ hermes_cli/config_migrations.py:_migrate_to_54 */
static json_t *cmg2_migrate_to_54(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *config = cmg2_read_raw_config(io);
    if (cmg2_raised(config)) return config;
    if (!cmg2_is_obj(config)) { json_t *e = cmg2_attr_raise(config, "get"); json_free(config); return e; }
    json_t *section = json_obj_get(config, "prompt_caching");
    if (!cmg2_is_obj(section)) { json_free(config); return json_null(); }
    json_t *ttl = json_obj_get(section, "cache_ttl");
    if (!(cmg2_is_str(ttl) && strcmp(cmg2_sval(ttl), "5m") == 0)) { json_free(config); return json_null(); }

    json_t *r = cmg2_import_check(io, "supports_file_tooling");
    if (cmg2_raised(r)) { json_free(config); return r; }
    json_free(r);
    json_t *cfg = cmg2_seam(io, "cfg", json_object());
    if (cmg2_raised(cfg)) { json_free(config); return cfg; }
    json_free(cfg);
    json_t *path = cmg2_seam(io, "config.get_config_path", json_object());
    if (cmg2_raised(path)) { json_free(config); return path; }
    json_t *sft = cmg2_seam(io, "supports_file_tooling", json_object());
    if (cmg2_raised(sft)) { json_free(path); json_free(config); return sft; }
    bool seeded = false;
    if (cmg2_truthy(sft)) {
        if (!cmg2_is_str(path)) {
            json_t *e = cmg2_attr_raise(path, "read_text");
            json_free(sft); json_free(path); json_free(config);
            return e;
        }
        json_t *txt = cmg2_seam(io, "path.read_text", ({ json_t *a = json_object(); json_set(a, "path", json_copy(path)); json_set(a, "encoding", json_string("utf-8-sig")); a; }));
        if (cmg2_raised(txt)) { json_free(sft); json_free(path); json_free(config); return txt; }
        if (cmg2_is_str(txt) && strncmp(txt->str_val, "\xef\xbb\xbf", 3) == 0) {
            char *nb = strdup(txt->str_val + 3);
            json_free(txt); txt = json_string(nb); free(nb);
        }
        json_t *children = cmg2_prompt_caching_child_lines(txt);
        json_free(txt);
        if (cmg2_raised(children)) { json_free(sft); json_free(path); json_free(config); return children; }
        for (size_t i = 0; i < children->c.count; i++) {
            if (cmg2_template_ttl_line(cmg2_sval(children->c.items[i]))) { seeded = true; break; }
        }
        json_free(children);
    }
    json_free(sft); json_free(path);
    if (!seeded) {
        if (!quiet) {
            json_t *pr = cmg2_print(io, CMG2_M54_INFO);
            if (cmg2_raised(pr)) { json_free(config); return pr; }
            json_free(pr);
        }
        json_free(config);
        return json_null();
    }
    json_obj_del(section, "cache_ttl");
    json_t *cr = cmg2_commit(io, config, results, quiet, CMG2_M54_ADD, CMG2_M54_PRINT);
    json_free(config);
    return cr;
}

/* ===================================================================
 * disable_suspicious_mcp_servers  (config_migrations.py:816)
 * import try/except Exception -> return; call raises propagate (no try).
 * =================================================================== */
/* PoP: cmg2_disable_suspicious_mcp_servers @ hermes_cli/config_migrations.py:disable_suspicious_mcp_servers */
static json_t *cmg2_disable_suspicious_mcp_servers(cmg2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *config = cmg2_read_raw_config(io);
    if (cmg2_raised(config)) return config;
    if (!cmg2_is_obj(config)) { json_t *e = cmg2_attr_raise(config, "get"); json_free(config); return e; }
    json_t *servers = json_obj_get(config, "mcp_servers");
    if (!cmg2_is_obj(servers)) { json_free(config); return json_null(); }
    /* try: from hermes_cli.mcp_security import validate_mcp_server_entry */
    json_t *imp = cmg2_import_check(io, "validate_mcp_server_entry");
    if (cmg2_raised(imp)) {
        if (json_obj_get(imp, "__untrace__") && cmg2_exc_is_exception(cmg2_raise_type(imp))) {
            json_free(imp); json_free(config);
            return json_null();   /* except Exception: return */
        }
        json_free(config);
        return imp;   /* BaseException propagates */
    }
    json_free(imp);
    bool touched = false;
    for (size_t i = 0; i < servers->c.count; i++) {
        const char *name = servers->c.keys[i];
        json_t *entry = servers->c.items[i];
        json_t *issues;
        if (cmg2_is_obj(entry)) {
            issues = cmg2_seam(io, "validate_mcp_server_entry", ({ json_t *a = json_object(); json_set(a, "name", json_string(name)); json_set(a, "entry", json_copy(entry)); a; }));
            if (cmg2_raised(issues)) { json_free(config); return issues; }
        } else {
            issues = json_null();
        }
        if (!cmg2_truthy(issues)) { json_free(issues); continue; }
        json_set(entry, "enabled", json_bool(false));
        touched = true;
        char *a1 = cmg2_join3(CMG2_MDS_ADD_PRE, name, CMG2_MDS_ADD_SUF);
        json_t *err = cmg2_results_append(results, "warnings", json_string(a1));
        free(a1);
        if (err) { json_free(issues); json_free(config); return err; }
        if (!quiet) {
            for (size_t j = 0; j < issues->c.count; j++) {
                char *is = cmg2_py_str(issues->c.items[j]);
                char *pm = cmg2_join(CMG2_MDS_WARN_PRE, is); free(is);
                json_t *pr = cmg2_print(io, pm); free(pm);
                if (cmg2_raised(pr)) { json_free(issues); json_free(config); return pr; }
                json_free(pr);
            }
            char *p1 = cmg2_join3(CMG2_MDS_PRINT_PRE, name, CMG2_MDS_PRINT_SUF);
            json_t *pr = cmg2_print(io, p1); free(p1);
            if (cmg2_raised(pr)) { json_free(issues); json_free(config); return pr; }
            json_free(pr);
        }
        json_free(issues);
    }
    if (touched) {
        json_set(config, "mcp_servers", json_copy(servers));
        json_t *r = cmg2_persist_migration(io, config);
        json_free(config);
        return r;
    }
    json_free(config);
    return json_null();
}
