/* config_migration_flow2.c — FLOW-core C11 port of the UNCLAIMED remainder of
 * the hermes_cli config-migration registry: hermes_cli/config_migrations.py
 * steps _migrate_to_34 / _migrate_to_38 / _migrate_to_39, plus the registered
 * browser sibling hermes_cli/config_migrations_browser.py (migrate_to_52 is
 * imported at module top and registered in MIGRATIONS; its 9 defs are all
 * unclaimed). 12 ported defs total:
 *   config_migrations.py:        _migrate_to_34, _migrate_to_38, _migrate_to_39
 *   config_migrations_browser.py: _agent_browser_env, _inside_pm_store,
 *     _browser_value_from_env, _config_decided, _backend_removed_note,
 *     move_agent_browser_env, agent_browser_env_pending,
 *     adopt_agent_browser_env, migrate_to_52
 *
 * Claimed siblings are SEAMED (one trace token each; PoP credit stays with the
 * claiming port): read_raw_config / _persist_migration / _commit (claimed by
 * cli/config_migrations_flow2.c), record_core_use / defer_startup_notice
 * (claimed by hermes_cli/config_migration_flow.c / left_core_migration_flow.c).
 * The ported defs compose LIVE (C calls C; the oracle keeps its extracted defs
 * real), matching the sibling flow ports' convention.
 *
 * Universal seam: every collaborator / I/O call goes through
 * cm2_seam(io, name, args). The dispatcher appends `name` to io->trace,
 * returns the scripted result, applies "state" effects, and models a raised
 * exception with {"__raise__": {type,message,code?,name?}}. An "import_raise"
 * script entry models a failed in-function import: cm2_import_check() calls
 * the seam with an {"__import__": name} arg; the dispatcher returns
 * {"__raise__": spec, "__untrace__": true} (token retracted — the live import
 * statement records nothing) or {"__import_ok__": true} on success (token also
 * retracted). Import checks fire per imported NAME in source order.
 *
 * Seam name table (C name -> live call):
 *   read_raw_config            config_migrations.read_raw_config()   [AUTO]
 *   config.read_raw_config     hermes_cli.config.read_raw_config()   [AUTO]
 *   persist_migration          config_migrations._persist_migration(config) [AUTO]
 *   commit                     config_migrations._commit(config,results,quiet,
 *                              added,message)                        [AUTO effect]
 *   print                      builtin print(text)                   [AUTO]
 *   record_core_use            left_core_migration.record_core_use(home,cfg,52)
 *   defer_startup_notice       left_core_migration.defer_startup_notice(note)
 *   available_personalities / normalize_personality_name / prompt_text /
 *   render_personality_prompt  hermes_cli.personality.*
 *   legacy_relay_plugin_keys   hermes_cli.relay_plugin_cutover.*
 *   resolve_home_dotenv        hermes_cli.env_loader.*
 *   hermes_home_key / get_routing_process_hermes_home / get_hermes_home
 *                              hermes_constants.*
 *   pm.store_root / pm.writable_store_root   pm.paths.*
 *   path.expanduser / path.resolve           pathlib.Path methods (double)
 *   os.environ.get             os.environ.get(name)
 *   os.path.expanduser / os.path.abspath     os.path.*
 *   pm.paths                   import gate for `from pm import paths`
 *
 * AUTO seams (no script, never fail closed): "print" (captures args.text into
 * io->cap_stdout; a scripted "raise" entry still fires); "read_raw_config" and
 * "config.read_raw_config" (return a copy of io->state.raw_config, or a
 * scripted "ret"/"raise" override); "persist_migration" (sets
 * io->state.raw_config = copy(args.config), or a scripted "raise"); "commit"
 * (models the claimed _commit's observable effects atomically: persist
 * args.config, append args.added to results["config_added"] when truthy —
 * KeyError when the key is absent, AttributeError when it is not a list — and
 * capture args.message when truthy and not quiet; a scripted "raise" fires
 * before any effect). Every OTHER seam is SCRIPTED and FAIL-CLOSED: a missing
 * / exhausted script raises RuntimeError("unscripted seam: <name>"). With no
 * seam channel installed the production default is fail-closed too.
 *
 * io->state is a json OBJECT mirroring the on-disk config document
 * (raw_config). The caller's `results` dict is a separate input the flows
 * mutate in place (config_added / warnings / notices); the harness echoes it
 * as out.results. move_agent_browser_env mutates its `browser` argument in
 * place (echoed as out.browser); adopt_agent_browser_env mutates `config`
 * (echoed as out.config).
 *
 * Representation contract (both sides): paths are POSIX strings (BrowserPath
 * double; expanduser/resolve are seams, is_relative_to is a lexical component
 * prefix); os.path.isabs is the lexical posix model (fixture domain excludes
 * drive-letter and backslash paths, where ntpath would differ); set results
 * project to arrays; move_agent_browser_env's tuple return projects to
 * {"moved":[...],"pinned":[...]}; an uncaught exception rides as
 * {"__raise__": {type,message,code?,name?}}; SystemExit carries code; a
 * scripted KeyError message is the RAW key (both sides repr it). Integral JSON
 * numbers read as int (libjson has no int/float identity); bool is distinct
 * from int in the oracle's deep_eq. Fixture input domain is ASCII; the few
 * non-ASCII glyphs in message CONSTANTS (check/warn/em-dash/arrow) are
 * byte-pinned UTF-8 literals, not fixture data. str()-position seam rets
 * (normalize_personality_name, prompt_text, path/os.path rets) are strings in
 * the fixture domain; integral floats are excluded from str() positions.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state
 * / io->cap_stdout are harness-owned. cm2_seam() frees its args. Every ported
 * function returns a NEW reference (or a raise sentinel). Aliasing mirrors
 * live: migrate_to_52 / adopt_agent_browser_env borrow config["browser"] when
 * it is a dict (writes alias); agent_browser_env_pending copies it (dict()).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── byte-pinned message constants (generated from the quarry source) ────── */
#define CM2_M34_ADD "display.personality=none (one-time reset)"
#define CM2_M34_P1A "  \xe2\x9c\x93 Personality reset to none (was '"
#define CM2_M34_P1B "'). Personality state was previously saved inconsistently across surfaces and could re-enable a personality you had turned off. Run /personality "
#define CM2_M34_P1C " to turn it back on."
#define CM2_M34_P2 "  \xe2\x9c\x93 Removed personality text from agent.system_prompt (written by an older /personality). That field is now reserved for manual system prompts; personalities live in display.personality."
#define CM2_M38_PRE "Removed legacy Relay plugin from plugins.enabled: "
#define CM2_M38_SUF ". Configure a standard user or system Relay plugins.toml, or use HERMES_NEMO_RELAY_PLUGINS_TOML for an explicit user-file override."
#define CM2_M38_WARN "  \xe2\x9a\xa0 "
#define CM2_M39_ADD "removed retired 'bfl' toolset from saved toolset lists"
#define CM2_M39_MSG "  \xe2\x9c\x93 Removed the retired BFL FLUX 3 toolset from saved toolset lists \xe2\x80\x94 video generation now lives under `hermes tools` \xe2\x86\x92 Video Generation (Nous Subscription or FAL)."
#define CM2_M52_OFF_NOTE "removed browser.backend: off \xe2\x80\x94 the built-in browser tools are gone and browser_exec is the browser now; to turn browsing off, disable the `browser` toolset in `hermes tools`"
#define CM2_BN_UNSET "removed browser.backend (unset)"
#define CM2_BN_PRE "removed browser.backend: "
#define CM2_BN_DASH " \xe2\x80\x94 "
#define CM2_BN_DEFAULT "browser_exec is the only browser now"
#define CM2_MOVE_ARROW " \xe2\x86\x92 browser."
#define CM2_PIN_PRE "pinned browser.user_data_dir to "
#define CM2_PIN_MID ": "
#define CM2_PIN_EQ "="
#define CM2_PIN_SUF " was relative to the directory Hermes ran in; check that the browser logins are there"
#define CM2_M52_EXT "removed browser.extension_control"
#define CM2_M52_BROWSER_PRE "browser: "
#define CM2_M52_MOVED_PRE "  \xe2\x9c\x93 Moved agent-browser settings from .env to config.yaml: "
#define CM2_CHECK_PRE "  \xe2\x9c\x93 "
#define CM2_DOT "."

/* ── module constants (quarry config_migrations_browser.py) ──────────────── */
typedef struct { const char *env; const char *key; } cm2_env_key_t;
static const cm2_env_key_t CM2_AGENT_BROWSER_ENV_KEYS[] = {
    {"AGENT_BROWSER_EXECUTABLE_PATH", "executable_path"},
    {"AGENT_BROWSER_PROFILE", "user_data_dir"},
    {"AGENT_BROWSER_ARGS", "chromium_args"},
    {"AGENT_BROWSER_HEADED", "headed"},
    {"AGENT_BROWSER_ENGINE", "engine"},
    {NULL, NULL},
};
typedef struct { const char *key; const char *reason; } cm2_backend_reason_t;
static const cm2_backend_reason_t CM2_BACKEND_REASONS[] = {
    {"browser-use", "browser_exec (Browser Use mode) is the only browser now"},
    {"agent-browser", "agent-browser left Hermes; browser_exec is the browser now"},
    {"camofox", "Camofox is now the camofox plugin (`hermes plugins install camofox`)"},
    {"local", "browser_exec launches Hermes's packaged Chromium itself (browser.cloud_provider: local)"},
    {"browserbase", "a cloud browser is chosen with browser.cloud_provider (`hermes tools` \xe2\x86\x92 Browser Automation)"},
    {"firecrawl", "a cloud browser is chosen with browser.cloud_provider (`hermes tools` \xe2\x86\x92 Browser Automation)"},
    {"nous", "a cloud browser is chosen with browser.cloud_provider (`hermes tools` \xe2\x86\x92 Browser Automation)"},
    {NULL, NULL},
};

/* ── seam dispatcher + trace + state ─────────────────────────────────────── */
typedef struct cm2_flow_io_t {
    json_t *(*call)(struct cm2_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;        /* ARRAY of string tokens (harness-owned) */
    json_t *state;        /* OBJECT: raw_config mirror (harness-owned) */
    json_t *cap_stdout;   /* ARRAY of captured print text (harness-owned) */
    void   *ctx;
} cm2_flow_io_t;

static void cm2_trace(cm2_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

static json_t *cm2_make_raise(const char *type, const char *message);

static json_t *cm2_call(cm2_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    (void)name; (void)args;
    return cm2_make_raise("RuntimeError", "no seam channel installed");
}

static bool cm2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    json_t *s = json_obj_get(r, "__raise__");
    return s && s->type != JSON_NULL;
}

static json_t *cm2_raise_spec(const json_t *r) {
    if (!cm2_raised(r)) return NULL;
    return json_obj_get(r, "__raise__");
}

static json_t *cm2_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

static void cm2_retract_token(cm2_flow_io_t *io) {
    if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
        size_t last = io->trace->c.count - 1;
        json_free(io->trace->c.items[last]);
        io->trace->c.count = last;
    }
}

/* seam: trace the attempt, dispatch, free args. __import_ok__ / __untrace__
 * results retract the trace token (a live import statement records nothing). */
static json_t *cm2_seam(cm2_flow_io_t *io, const char *name, json_t *args) {
    cm2_trace(io, name);
    json_t *r = cm2_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__import_ok__")) {
        cm2_retract_token(io);
        json_free(r);
        return json_null();
    }
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        cm2_retract_token(io);
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
static json_t *cm2_import_check(cm2_flow_io_t *io, const char *script_name) {
    json_t *args = json_object();
    json_set(args, "__import__", json_string(script_name));
    return cm2_seam(io, script_name, args);
}

/* print(text) — AUTO capture seam (a scripted raise still fires). */
static json_t *cm2_print(cm2_flow_io_t *io, const char *text) {
    json_t *args = json_object();
    json_set(args, "text", json_string(text ? text : ""));
    return cm2_seam(io, "print", args);
}

static json_t *cm2_state_get(cm2_flow_io_t *io, const char *key) {
    if (!io || !io->state || io->state->type != JSON_OBJECT) return NULL;
    return json_obj_get(io->state, key);
}

/* ── full builtin exception hierarchy (py3.14; for except-family tests) ──── */
typedef struct { const char *name; const char *parent; } cm2_exc_t;
static const cm2_exc_t CM2_EXC_TABLE[] = {
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

static const char *cm2_exc_parent(const char *type) {
    if (!type) return NULL;
    for (int i = 0; CM2_EXC_TABLE[i].name; i++)
        if (strcmp(type, CM2_EXC_TABLE[i].name) == 0) return CM2_EXC_TABLE[i].parent;
    return NULL;
}

static bool cm2_exc_known(const char *type) {
    if (!type) return false;
    for (int i = 0; CM2_EXC_TABLE[i].name; i++)
        if (strcmp(type, CM2_EXC_TABLE[i].name) == 0) return true;
    return false;
}

/* isinstance(exc, Exception): walk to "Exception". Unknown kinds model the
 * oracle make_exc fallback (a synthetic Exception subclass) -> true. */
static bool cm2_exc_is_exception(const char *type) {
    if (!cm2_exc_known(type)) return true;
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "Exception") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = cm2_exc_parent(cur);
    }
    return false;
}

/* isinstance(exc, OSError): walk to "OSError". Unknown -> false. */
static bool cm2_exc_is_oserror(const char *type) {
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "OSError") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = cm2_exc_parent(cur);
    }
    return false;
}

/* isinstance(exc, RuntimeError): walk to "RuntimeError". Unknown -> false. */
static bool cm2_exc_is_runtimeerror(const char *type) {
    const char *cur = type;
    for (int depth = 0; cur && depth < 32; depth++) {
        if (strcmp(cur, "RuntimeError") == 0) return true;
        if (strcmp(cur, "BaseException") == 0) return false;
        cur = cm2_exc_parent(cur);
    }
    return false;
}

static const char *cm2_raise_type(const json_t *r) {
    json_t *spec = cm2_raise_spec(r);
    return spec ? json_get_str(spec, "type", "") : "";
}

/* ── Python-faithful value helpers ───────────────────────────────────────── */
static bool cm2_truthy(const json_t *v) {
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
static bool cm2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cm2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cm2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool cm2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool cm2_is_bool_true(const json_t *v) { return v && v->type == JSON_BOOL && v->bool_val; }
static const char *cm2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* type(x).__name__ for a JSON value (integral numbers read as int). */
static const char *cm2_py_type_name(const json_t *v) {
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
static bool cm2_py_eq(const json_t *a, const json_t *b) {
    if (cm2_is_null(a) && cm2_is_null(b)) return true;
    if (!a || !b) return false;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER) return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL) return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING) return strcmp(cm2_sval(a), cm2_sval(b)) == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!cm2_py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !cm2_py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* array membership using Python ==. */
static bool cm2_member(const json_t *arr, const json_t *needle) {
    if (!cm2_is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (cm2_py_eq(arr->c.items[i], needle)) return true;
    return false;
}
static bool cm2_member_str(const json_t *arr, const char *s) {
    json_t *n = json_string(s);
    bool r = cm2_member(arr, n);
    json_free(n);
    return r;
}

/* Python repr of a str (single quotes preferred; double when contains ' not "). */
static char *cm2_repr_str(const char *s) {
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

static char *cm2_py_repr(const json_t *v);

/* str(x) projection (scalar domain; containers repr'd for robustness). */
static char *cm2_py_str(const json_t *v) {
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
        case JSON_STRING: return strdup(cm2_sval(v));
        default: return cm2_py_repr(v);
    }
}

static char *cm2_py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return cm2_py_str(v);
    if (v->type == JSON_STRING) return cm2_repr_str(cm2_sval(v));
    if (v->type == JSON_ARRAY) {
        size_t cap = 64; char *out = malloc(cap); size_t k = 0;
        out[k++] = '[';
        for (size_t i = 0; i < v->c.count; i++) {
            char *r = cm2_py_repr(v->c.items[i]);
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
        char *kr = cm2_repr_str(v->c.keys[i]);
        char *vr = cm2_py_repr(v->c.items[i]);
        size_t need = strlen(kr) + strlen(vr) + 6;
        while (k + need > cap) { cap *= 2; out = realloc(out, cap); }
        if (i) { out[k++] = ','; out[k++] = ' '; }
        k += (size_t)sprintf(out + k, "%s: %s", kr, vr);
        free(kr); free(vr);
    }
    out[k++] = '}'; out[k] = '\0'; return out;
}

/* Unicode White_Space predicate (str.strip set). */
static bool cm2_is_ws_cp(unsigned long cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d)) return true;
    if (cp >= 0x1c && cp <= 0x1f) return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000) return true;
    return false;
}
static unsigned long cm2_next_cp(const char *s, size_t n, size_t *i) {
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
static char *cm2_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    if (n == 0) return strdup("");
    size_t cap = n + 1;
    size_t *starts = malloc(cap * sizeof(size_t));
    size_t *ends = malloc(cap * sizeof(size_t));
    unsigned long *cps = malloc(cap * sizeof(unsigned long));
    if (!starts || !ends || !cps) { free(starts); free(ends); free(cps); return strdup(p); }
    size_t m = 0, i = 0;
    while (i < n) {
        size_t st = i;
        unsigned long cp = cm2_next_cp(p, n, &i);
        starts[m] = st; ends[m] = i; cps[m] = cp; m++;
    }
    size_t lo = 0, hi = m;
    while (lo < hi && cm2_is_ws_cp(cps[lo])) lo++;
    while (hi > lo && cm2_is_ws_cp(cps[hi - 1])) hi--;
    size_t bs = (lo < hi) ? starts[lo] : n;
    size_t be = (lo < hi) ? ends[hi - 1] : n;
    char *out = malloc(be - bs + 1);
    memcpy(out, p + bs, be - bs);
    out[be - bs] = '\0';
    free(starts); free(ends); free(cps);
    return out;
}

/* str.lower() — ASCII fixture domain (byte tolower). malloc'd. */
static char *cm2_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

static char *cm2_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (a) memcpy(out, a, na);
    if (b) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}
static char *cm2_join3(const char *a, const char *b, const char *c) {
    char *t = cm2_join(a, b); char *r = cm2_join(t, c); free(t); return r;
}

/* ", ".join(arr) with the live str-only TypeError guard. On success returns
 * NULL and sets *out (malloc'd); on a non-str element returns the raise. */
static json_t *cm2_join_comma_checked(const json_t *arr, char **out) {
    size_t n = cm2_is_arr(arr) ? arr->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        if (!cm2_is_str(arr->c.items[i])) {
            char buf[128];
            snprintf(buf, sizeof buf, "sequence item %zu: expected str instance, %s found",
                     i, cm2_py_type_name(arr->c.items[i]));
            return cm2_make_raise("TypeError", buf);
        }
    }
    size_t cap = 64; char *o = malloc(cap); size_t k = 0; o[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        const char *s = cm2_sval(arr->c.items[i]);
        size_t sl = strlen(s);
        while (k + sl + 4 > cap) { cap *= 2; o = realloc(o, cap); }
        if (i) { o[k++] = ','; o[k++] = ' '; }
        memcpy(o + k, s, sl); k += sl; o[k] = '\0';
    }
    *out = o;
    return NULL;
}

/* ", ".join of a string array (elements str by construction). malloc'd. */
static char *cm2_join_comma(const json_t *arr) {
    char *o = NULL;
    json_t *e = cm2_join_comma_checked(arr, &o);
    if (e) { json_free(e); return strdup(""); }
    return o;
}

/* AttributeError "'<type>' object has no attribute '<name>'". */
static json_t *cm2_attr_raise(const json_t *carrier, const char *name) {
    char *m1 = cm2_join("'", cm2_py_type_name(carrier));
    char *m2 = cm2_join(m1, "' object has no attribute '"); free(m1);
    char *m3 = cm2_join(m2, name); free(m2);
    char *msg = cm2_join(m3, "'"); free(m3);
    json_t *r = cm2_make_raise("AttributeError", msg);
    free(msg);
    return r;
}
/* KeyError with the RAW key (message repr'd to match str(KeyError(k))). */
static json_t *cm2_key_raise(const char *key) {
    char *rk = cm2_repr_str(key);
    json_t *r = cm2_make_raise("KeyError", rk);
    free(rk);
    return r;
}

/* results[key].append(value) — KeyError when absent, AttributeError when
 * non-list. Takes ownership of value on success; frees it on error. */
static json_t *cm2_results_append(json_t *results, const char *key, json_t *value) {
    json_t *arr = json_obj_get(results, key);
    if (!arr) { json_free(value); return cm2_key_raise(key); }
    if (arr->type != JSON_ARRAY) { json_free(value); return cm2_attr_raise(arr, "append"); }
    json_append(arr, value);
    return NULL;
}
/* results[key].extend(src) — KeyError when absent, AttributeError non-list. */
static json_t *cm2_results_extend(json_t *results, const char *key, const json_t *src) {
    json_t *arr = json_obj_get(results, key);
    if (!arr) return cm2_key_raise(key);
    if (arr->type != JSON_ARRAY) return cm2_attr_raise(arr, "extend");
    size_t n = cm2_is_arr(src) ? src->c.count : 0;
    for (size_t i = 0; i < n; i++) json_append(arr, json_copy(src->c.items[i]));
    return NULL;
}
/* results.setdefault(key, []).extend(src) — creates when absent; a present
 * non-list (incl. null) raises AttributeError 'extend'. */
static json_t *cm2_results_setdefault_extend(json_t *results, const char *key, const json_t *src) {
    json_t *arr = json_obj_get(results, key);
    if (!arr) {
        arr = json_array();
        json_set(results, key, arr);
    } else if (arr->type != JSON_ARRAY) {
        return cm2_attr_raise(arr, "extend");
    }
    size_t n = cm2_is_arr(src) ? src->c.count : 0;
    for (size_t i = 0; i < n; i++) json_append(arr, json_copy(src->c.items[i]));
    return NULL;
}

/* f"{note[0].upper()}{note[1:]}." — ASCII first-char upper. malloc'd. */
static char *cm2_upper_first_dot(const char *note) {
    const char *p = note ? note : "";
    size_t n = strlen(p);
    char *out = malloc(n + 2);
    size_t k = 0;
    if (n > 0) out[k++] = (char)toupper((unsigned char)p[0]);
    if (n > 1) { memcpy(out + k, p + 1, n - 1); k += n - 1; }
    out[k++] = '.';
    out[k] = '\0';
    return out;
}

/* os.path.isabs — lexical posix model (fixture domain: posix absolute or
 * relative paths; drive-letter / backslash paths excluded). */
static bool cm2_path_isabs(const char *s) {
    return s && s[0] == '/';
}

/* PurePosixPath.is_relative_to — lexical component prefix (case-sensitive). */
static bool cm2_path_is_relative_to(const char *a, const char *b) {
    const char *pa = a ? a : "";
    const char *pb = b ? b : "";
    /* walk components of b; each must match the next component of a */
    size_t ia = 0, ib = 0;
    size_t na = strlen(pa), nb = strlen(pb);
    for (;;) {
        while (ib < nb && pb[ib] == '/') ib++;
        if (ib >= nb) return true;   /* b exhausted: prefix holds */
        size_t sb = ib;
        while (ib < nb && pb[ib] != '/') ib++;
        while (ia < na && pa[ia] == '/') ia++;
        if (ia >= na) return false;  /* a exhausted before b */
        size_t sa = ia;
        while (ia < na && pa[ia] != '/') ia++;
        if ((ib - sb) != (ia - sa) || strncmp(pa + sa, pb + sb, ib - sb) != 0)
            return false;
    }
}

/* re.split(r"[,\n]", value) — ASCII separators, keeps empty segments. */
static json_t *cm2_split_comma_nl(const char *value) {
    json_t *out = json_array();
    const char *p = value ? value : "";
    size_t n = strlen(p), start = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == ',' || p[i] == '\n') {
            char *seg = malloc(i - start + 1);
            memcpy(seg, p + start, i - start); seg[i - start] = '\0';
            json_append(out, json_string(seg)); free(seg);
            start = i + 1;
        }
    }
    char *seg = malloc(n - start + 1);
    memcpy(seg, p + start, n - start); seg[n - start] = '\0';
    json_append(out, json_string(seg)); free(seg);
    return out;
}

/* dict(_AGENT_BROWSER_ENV_KEYS)[name] — KeyError when unknown. */
static const char *cm2_env_key_for(const char *name) {
    for (int i = 0; CM2_AGENT_BROWSER_ENV_KEYS[i].env; i++)
        if (strcmp(name, CM2_AGENT_BROWSER_ENV_KEYS[i].env) == 0)
            return CM2_AGENT_BROWSER_ENV_KEYS[i].key;
    return NULL;
}

/* path-method seam probe: fires `name` with {path}, expects a str ret (the
 * next method's carrier). Non-str ret models the AttributeError the next
 * method call would raise. Returns NULL + *out on success, else a raise. */
static json_t *cm2_path_probe(cm2_flow_io_t *io, const char *name, const char *path,
                              const char *next_method, char **out) {
    json_t *args = json_object();
    json_set(args, "path", json_string(path ? path : ""));
    json_t *r = cm2_seam(io, name, args);
    if (cm2_raised(r)) return r;
    if (!cm2_is_str(r)) {
        json_t *e = cm2_attr_raise(r, next_method);
        json_free(r);
        return e;
    }
    *out = strdup(cm2_sval(r));
    json_free(r);
    return NULL;
}

/* except (OSError, RuntimeError) family test for _inside_pm_store. */
static bool cm2_pm_caught(const json_t *e) {
    const char *t = cm2_raise_type(e);
    return cm2_exc_is_oserror(t) || cm2_exc_is_runtimeerror(t);
}

/* ===================================================================
 * _agent_browser_env  (config_migrations_browser.py:22)
 * =================================================================== */
/* PoP: cm2_agent_browser_env @ hermes_cli/config_migrations_browser.py:_agent_browser_env */
static json_t *cm2_agent_browser_env(cm2_flow_io_t *io, const char *home) {
    json_t *r = cm2_import_check(io, "resolve_home_dotenv");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "get_routing_process_hermes_home");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "hermes_home_key");
    if (cm2_raised(r)) return r;
    json_free(r);

    json_t *args = json_object();
    json_set(args, "home", json_string(home ? home : ""));
    json_t *dotenv = cm2_seam(io, "resolve_home_dotenv", args);
    if (cm2_raised(dotenv)) return dotenv;

    args = json_object();
    json_set(args, "home", json_string(home ? home : ""));
    json_t *k1 = cm2_seam(io, "hermes_home_key", args);
    if (cm2_raised(k1)) { json_free(dotenv); return k1; }

    json_t *routing = cm2_seam(io, "get_routing_process_hermes_home", json_object());
    if (cm2_raised(routing)) { json_free(dotenv); json_free(k1); return routing; }

    args = json_object();
    json_set(args, "home", json_copy(routing));
    json_t *k2 = cm2_seam(io, "hermes_home_key", args);
    if (cm2_raised(k2)) { json_free(dotenv); json_free(k1); json_free(routing); return k2; }

    bool launch = cm2_py_eq(k1, k2);
    json_free(k1); json_free(k2); json_free(routing);

    if (!cm2_is_obj(dotenv)) {
        json_t *e = cm2_attr_raise(dotenv, "get");
        json_free(dotenv);
        return e;
    }
    json_t *found = json_object();
    for (int i = 0; CM2_AGENT_BROWSER_ENV_KEYS[i].env; i++) {
        const char *name = CM2_AGENT_BROWSER_ENV_KEYS[i].env;
        json_t *dv = json_obj_get(dotenv, name);   /* borrowed */
        json_t *ev = NULL;
        const json_t *val;
        if (cm2_truthy(dv)) {
            val = dv;
        } else if (launch) {
            args = json_object();
            json_set(args, "name", json_string(name));
            ev = cm2_seam(io, "os.environ.get", args);
            if (cm2_raised(ev)) { json_free(found); json_free(dotenv); return ev; }
            val = ev;
        } else {
            val = NULL;
        }
        const char *raw;
        if (!cm2_truthy(val)) {
            raw = "";
        } else if (!cm2_is_str(val)) {
            json_t *e = cm2_attr_raise(val, "strip");
            json_free(ev); json_free(found); json_free(dotenv);
            return e;
        } else {
            raw = cm2_sval(val);
        }
        char *st = cm2_strip(raw);
        if (st[0]) json_set(found, name, json_string(st));
        free(st);
        json_free(ev);
    }
    json_free(dotenv);
    return found;
}

/* ===================================================================
 * _inside_pm_store  (config_migrations_browser.py:38)
 * =================================================================== */
/* PoP: cm2_inside_pm_store @ hermes_cli/config_migrations_browser.py:_inside_pm_store */
static json_t *cm2_inside_pm_store(cm2_flow_io_t *io, const char *value) {
    json_t *r = cm2_import_check(io, "pm.paths");
    if (cm2_raised(r)) return r;
    json_free(r);

    char *expanded = NULL, *resolved = NULL, *root0 = NULL, *root1 = NULL;
    json_t *fail = NULL;
    bool rel = false;

    fail = cm2_path_probe(io, "path.expanduser", value, "resolve", &expanded);
    if (fail) goto done;
    fail = cm2_path_probe(io, "path.resolve", expanded, "is_relative_to", &resolved);
    if (fail) goto done;

    r = cm2_seam(io, "pm.store_root", json_object());
    if (cm2_raised(r)) { fail = r; goto done; }
    if (!cm2_is_str(r)) { fail = cm2_attr_raise(r, "resolve"); json_free(r); goto done; }
    fail = cm2_path_probe(io, "path.resolve", cm2_sval(r), "is_relative_to", &root0);
    json_free(r);
    if (fail) goto done;

    r = cm2_seam(io, "pm.writable_store_root", json_object());
    if (cm2_raised(r)) { fail = r; goto done; }
    if (!cm2_is_str(r)) { fail = cm2_attr_raise(r, "resolve"); json_free(r); goto done; }
    fail = cm2_path_probe(io, "path.resolve", cm2_sval(r), "is_relative_to", &root1);
    json_free(r);
    if (fail) goto done;

    rel = cm2_path_is_relative_to(resolved, root0) || cm2_path_is_relative_to(resolved, root1);

done:
    free(expanded); free(resolved); free(root0); free(root1);
    if (fail) {
        if (cm2_pm_caught(fail)) { json_free(fail); return json_bool(false); }
        return fail;
    }
    return json_bool(rel);
}

/* ===================================================================
 * _browser_value_from_env  (config_migrations_browser.py:53)
 * =================================================================== */
/* PoP: cm2_browser_value_from_env @ hermes_cli/config_migrations_browser.py:_browser_value_from_env */
static json_t *cm2_browser_value_from_env(cm2_flow_io_t *io, const char *key, const char *value) {
    const char *k = key ? key : "";
    const char *v = value ? value : "";
    if (strcmp(k, "chromium_args") == 0) {
        json_t *parts = cm2_split_comma_nl(v);
        json_t *out = json_array();
        for (size_t i = 0; i < parts->c.count; i++) {
            char *st = cm2_strip(cm2_sval(parts->c.items[i]));
            if (st[0]) json_append(out, json_string(st));
            free(st);
        }
        json_free(parts);
        if (out->c.count == 0) { json_free(out); return json_null(); }
        return out;
    }
    if (strcmp(k, "headed") == 0) {
        char *lo = cm2_lower(v);
        bool m = strcmp(lo, "1") == 0 || strcmp(lo, "true") == 0 ||
                 strcmp(lo, "yes") == 0 || strcmp(lo, "on") == 0;
        free(lo);
        return m ? json_bool(true) : json_null();
    }
    if (strcmp(k, "engine") == 0) {
        char *lo = cm2_lower(v);
        bool m = strcmp(lo, "auto") == 0 || strcmp(lo, "lightpanda") == 0 || strcmp(lo, "chrome") == 0;
        json_t *out = m ? json_string(lo) : json_null();
        free(lo);
        return out;
    }
    if (strcmp(k, "executable_path") == 0) {
        json_t *r = cm2_inside_pm_store(io, v);
        if (cm2_raised(r)) return r;
        bool inside = cm2_is_bool_true(r);
        json_free(r);
        if (inside) return json_null();
        return json_string(v);
    }
    if (strcmp(k, "user_data_dir") == 0) {
        json_t *args = json_object();
        json_set(args, "value", json_string(v));
        json_t *eu1 = cm2_seam(io, "os.path.expanduser", args);
        if (cm2_raised(eu1)) return eu1;
        bool abs = cm2_path_isabs(cm2_sval(eu1));
        if (!abs) {
            args = json_object();
            json_set(args, "value", json_string(v));
            json_t *eu2 = cm2_seam(io, "os.path.expanduser", args);
            if (cm2_raised(eu2)) { json_free(eu1); return eu2; }
            args = json_object();
            json_set(args, "path", json_string(cm2_sval(eu2)));
            json_t *ab = cm2_seam(io, "os.path.abspath", args);
            json_free(eu1); json_free(eu2);
            if (cm2_raised(ab)) return ab;
            return ab;
        }
        json_free(eu1);
        return json_string(v);
    }
    return json_string(v);
}

/* ===================================================================
 * _config_decided  (config_migrations_browser.py:70)  — pure
 * =================================================================== */
/* PoP: cm2_config_decided @ hermes_cli/config_migrations_browser.py:_config_decided */
static json_t *cm2_config_decided(const char *key, const json_t *current) {
    const char *k = key ? key : "";
    if (strcmp(k, "engine") == 0) {
        char *s = cm2_truthy(current) ? cm2_py_str(current) : strdup("");
        char *st = cm2_strip(s); free(s);
        char *lo = cm2_lower(st); free(st);
        bool decided = !(strcmp(lo, "") == 0 || strcmp(lo, "auto") == 0);
        free(lo);
        return json_bool(decided);
    }
    if (strcmp(k, "headed") == 0) {
        if (cm2_is_null(current)) return json_bool(false);
        char *s = cm2_py_str(current);
        char *st = cm2_strip(s); free(s);
        char *lo = cm2_lower(st); free(st);
        bool m = strcmp(lo, "true") == 0 || strcmp(lo, "1") == 0 || strcmp(lo, "yes") == 0;
        free(lo);
        return json_bool(m);
    }
    json_t *tup = json_array();
    json_append(tup, json_null());
    json_append(tup, json_string(""));
    json_append(tup, json_array());
    bool decided = !cm2_member(tup, current);
    json_free(tup);
    return json_bool(decided);
}

/* ===================================================================
 * _backend_removed_note  (config_migrations_browser.py:93)  — pure
 * =================================================================== */
/* PoP: cm2_backend_removed_note @ hermes_cli/config_migrations_browser.py:_backend_removed_note */
static json_t *cm2_backend_removed_note(const json_t *backend) {
    char *value;
    if (cm2_is_null(backend) || (backend && backend->type == JSON_BOOL)) {
        value = strdup("");
    } else {
        char *s = cm2_py_str(backend);
        value = cm2_strip(s);
        free(s);
    }
    if (!value[0]) { free(value); return json_string(CM2_BN_UNSET); }
    char *lo = cm2_lower(value);
    const char *reason = CM2_BN_DEFAULT;
    for (int i = 0; CM2_BACKEND_REASONS[i].key; i++) {
        if (strcmp(lo, CM2_BACKEND_REASONS[i].key) == 0) { reason = CM2_BACKEND_REASONS[i].reason; break; }
    }
    free(lo);
    char *m1 = cm2_join(CM2_BN_PRE, value);
    char *m2 = cm2_join(m1, CM2_BN_DASH); free(m1);
    char *msg = cm2_join(m2, reason); free(m2);
    free(value);
    json_t *out = json_string(msg);
    free(msg);
    return out;
}

/* ===================================================================
 * move_agent_browser_env  (config_migrations_browser.py:101)
 * tuple return projects to {"moved":[...],"pinned":[...]}; browser mutated
 * =================================================================== */
/* PoP: cm2_move_agent_browser_env @ hermes_cli/config_migrations_browser.py:move_agent_browser_env */
static json_t *cm2_move_agent_browser_env(cm2_flow_io_t *io, const char *home, json_t *browser) {
    json_t *env = cm2_agent_browser_env(io, home);
    if (cm2_raised(env)) return env;
    if (!cm2_is_obj(env)) {
        json_t *e = cm2_attr_raise(env, "items");
        json_free(env);
        return e;
    }
    json_t *moved = json_array();
    json_t *pinned = json_array();
    for (size_t i = 0; i < env->c.count; i++) {
        const char *name = env->c.keys[i];
        json_t *value = env->c.items[i];   /* stripped str by construction */
        const char *key = cm2_env_key_for(name);
        if (!key) {
            json_free(moved); json_free(pinned); json_free(env);
            return cm2_key_raise(name);
        }
        if (!cm2_is_obj(browser)) {
            json_free(moved); json_free(pinned); json_free(env);
            return cm2_attr_raise(browser, "get");
        }
        json_t *cur = json_obj_get(browser, key);
        json_t *dec = cm2_config_decided(key, cur);
        bool decided = cm2_is_bool_true(dec);
        json_free(dec);
        if (decided) continue;
        json_t *nv = cm2_browser_value_from_env(io, key, cm2_sval(value));
        if (cm2_raised(nv)) { json_free(moved); json_free(pinned); json_free(env); return nv; }
        if (cm2_is_null(nv)) { json_free(nv); continue; }
        json_set(browser, key, json_copy(nv));
        char *m1 = cm2_join(name, CM2_MOVE_ARROW);
        char *m = cm2_join(m1, key); free(m1);
        json_append(moved, json_string(m)); free(m);
        if (strcmp(key, "user_data_dir") == 0 && !cm2_py_eq(nv, value)) {
            char *p1 = cm2_join(CM2_PIN_PRE, cm2_sval(nv));
            char *p2 = cm2_join(p1, CM2_PIN_MID); free(p1);
            char *p3 = cm2_join(p2, name); free(p2);
            char *p4 = cm2_join(p3, CM2_PIN_EQ); free(p3);
            char *p5 = cm2_join(p4, cm2_sval(value)); free(p4);
            char *p6 = cm2_join(p5, CM2_PIN_SUF); free(p5);
            json_append(pinned, json_string(p6)); free(p6);
        }
        json_free(nv);
    }
    json_free(env);
    json_t *out = json_object();
    json_set(out, "moved", moved);
    json_set(out, "pinned", pinned);
    return out;
}

/* ===================================================================
 * agent_browser_env_pending  (config_migrations_browser.py:122)
 * =================================================================== */
/* PoP: cm2_agent_browser_env_pending @ hermes_cli/config_migrations_browser.py:agent_browser_env_pending */
static json_t *cm2_agent_browser_env_pending(cm2_flow_io_t *io) {
    json_t *r = cm2_import_check(io, "config.read_raw_config");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "get_hermes_home");
    if (cm2_raised(r)) return r;
    json_free(r);

    json_t *config = cm2_seam(io, "config.read_raw_config", json_object());
    if (cm2_raised(config)) return config;
    if (!cm2_is_obj(config)) {
        json_t *e = cm2_attr_raise(config, "get");
        json_free(config);
        return e;
    }
    json_t *b = json_obj_get(config, "browser");
    json_t *browser = (b && b->type == JSON_OBJECT) ? json_copy(b) : json_object();

    json_t *hh = cm2_seam(io, "get_hermes_home", json_object());
    if (cm2_raised(hh)) { json_free(config); json_free(browser); return hh; }

    json_t *mv = cm2_move_agent_browser_env(io, cm2_sval(hh), browser);
    json_free(hh);
    if (cm2_raised(mv)) { json_free(config); json_free(browser); return mv; }
    json_t *moved = json_obj_get(mv, "moved");
    bool pending = cm2_truthy(moved);
    json_free(mv); json_free(browser); json_free(config);
    return json_bool(pending);
}

/* ===================================================================
 * adopt_agent_browser_env  (config_migrations_browser.py:134)
 * config mutated in place; browser aliases config["browser"] when a dict
 * =================================================================== */
/* PoP: cm2_adopt_agent_browser_env @ hermes_cli/config_migrations_browser.py:adopt_agent_browser_env */
static json_t *cm2_adopt_agent_browser_env(cm2_flow_io_t *io, const char *home, json_t *config) {
    if (!cm2_is_obj(config)) return cm2_attr_raise(config, "get");
    json_t *b = json_obj_get(config, "browser");
    bool fresh = !(b && b->type == JSON_OBJECT);
    json_t *browser = fresh ? json_object() : b;   /* borrowed when aliased */

    json_t *mv = cm2_move_agent_browser_env(io, home, browser);
    if (cm2_raised(mv)) { if (fresh) json_free(browser); return mv; }
    json_t *moved = json_obj_get(mv, "moved");
    json_t *pinned = json_obj_get(mv, "pinned");
    if (cm2_truthy(moved)) json_set(config, "browser", json_copy(browser));
    json_t *fail = NULL;
    if (cm2_truthy(pinned)) {
        json_t *r = cm2_import_check(io, "defer_startup_notice");
        if (cm2_raised(r)) { fail = r; goto done; }
        json_free(r);
        for (size_t i = 0; i < pinned->c.count; i++) {
            char *n = cm2_upper_first_dot(cm2_sval(pinned->c.items[i]));
            json_t *args = json_object();
            json_set(args, "note", json_string(n));
            free(n);
            r = cm2_seam(io, "defer_startup_notice", args);
            if (cm2_raised(r)) { fail = r; goto done; }
            json_free(r);
        }
    }
done:
    if (fail) { json_free(mv); if (fresh) json_free(browser); return fail; }
    json_t *out = json_copy(moved);
    json_free(mv);
    if (fresh) json_free(browser);
    return out;
}

/* ===================================================================
 * migrate_to_52  (config_migrations_browser.py:149)
 * =================================================================== */
/* PoP: cm2_migrate_to_52 @ hermes_cli/config_migrations_browser.py:migrate_to_52 */
static json_t *cm2_migrate_to_52(cm2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *r = cm2_import_check(io, "persist_migration");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "read_raw_config");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "record_core_use");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "get_hermes_home");
    if (cm2_raised(r)) return r;
    json_free(r);

    json_t *config = cm2_seam(io, "read_raw_config", json_object());
    if (cm2_raised(config)) return config;
    if (!cm2_is_obj(config)) {
        json_t *e = cm2_attr_raise(config, "get");
        json_free(config);
        return e;
    }
    json_t *b = json_obj_get(config, "browser");
    bool fresh = !(b && b->type == JSON_OBJECT);
    json_t *browser = fresh ? json_object() : b;   /* borrowed when aliased */

    json_t *added = json_array();
    json_t *messages = json_array();
    json_t *notices = json_array();
    json_t *fail = NULL;

    if (json_has(browser, "backend")) {
        json_t *backend = json_copy(json_obj_get(browser, "backend"));
        json_obj_del(browser, "backend");
        bool is_off = (backend->type == JSON_BOOL && !backend->bool_val);
        if (!is_off && cm2_is_str(backend)) {
            char *st = cm2_strip(cm2_sval(backend));
            char *lo = cm2_lower(st); free(st);
            is_off = strcmp(lo, "off") == 0;
            free(lo);
        }
        if (is_off) {
            json_append(added, json_string(CM2_M52_OFF_NOTE));
            char *n = cm2_upper_first_dot(CM2_M52_OFF_NOTE);
            char *m = cm2_join(CM2_CHECK_PRE, n);
            json_append(messages, json_string(m)); free(m);
            json_append(notices, json_string(n)); free(n);
        } else {
            json_append(added, cm2_backend_removed_note(backend));
        }
        json_free(backend);
    }
    if (json_has(browser, "extension_control")) {
        json_obj_del(browser, "extension_control");
        json_append(added, json_string(CM2_M52_EXT));
    }

    json_t *hh = cm2_seam(io, "get_hermes_home", json_object());
    if (cm2_raised(hh)) { fail = hh; goto done; }
    json_t *mv = cm2_move_agent_browser_env(io, cm2_sval(hh), browser);
    json_free(hh);
    if (cm2_raised(mv)) { fail = mv; goto done; }
    json_t *moved = json_obj_get(mv, "moved");
    json_t *pinned = json_obj_get(mv, "pinned");
    for (size_t i = 0; i < pinned->c.count; i++) {
        const char *note = cm2_sval(pinned->c.items[i]);
        json_append(added, json_string(note));
        char *n = cm2_upper_first_dot(note);
        char *m = cm2_join(CM2_CHECK_PRE, n);
        json_append(messages, json_string(m)); free(m);
        json_append(notices, json_string(n)); free(n);
    }
    if (cm2_truthy(moved)) {
        char *j = cm2_join_comma(moved);
        char *a1 = cm2_join(CM2_M52_BROWSER_PRE, j);
        json_append(added, json_string(a1)); free(a1);
        char *a2 = cm2_join(CM2_M52_MOVED_PRE, j);
        json_append(messages, json_string(a2)); free(a2);
        free(j);
    }
    json_free(mv);

    hh = cm2_seam(io, "get_hermes_home", json_object());
    if (cm2_raised(hh)) { fail = hh; goto done; }
    json_t *args = json_object();
    json_set(args, "home", json_copy(hh));
    json_set(args, "config", json_copy(config));
    json_set(args, "version", json_number(52));
    json_free(hh);
    json_t *used = cm2_seam(io, "record_core_use", args);
    if (cm2_raised(used)) { fail = used; goto done; }
    bool has_added = cm2_truthy(added);
    bool has_used = cm2_truthy(used);
    json_free(used);
    if (!has_added && !has_used) goto done;   /* early return, no persist */

    if (has_added) json_set(config, "browser", json_copy(browser));
    args = json_object();
    json_set(args, "config", json_copy(config));
    r = cm2_seam(io, "persist_migration", args);
    if (cm2_raised(r)) { fail = r; goto done; }
    json_free(r);
    fail = cm2_results_extend(results, "config_added", added);
    if (fail) goto done;
    fail = cm2_results_setdefault_extend(results, "notices", notices);
    if (fail) goto done;
    if (!quiet) {
        for (size_t i = 0; i < messages->c.count; i++) {
            r = cm2_print(io, cm2_sval(messages->c.items[i]));
            if (cm2_raised(r)) { fail = r; goto done; }
            json_free(r);
        }
    }

done:
    json_free(added); json_free(messages); json_free(notices);
    if (fresh) json_free(browser);
    json_free(config);
    if (fail) return fail;
    return json_null();
}

/* ===================================================================
 * _migrate_to_34  (config_migrations.py:475)
 * =================================================================== */
/* PoP: cm2_migrate_to_34 @ hermes_cli/config_migrations.py:_migrate_to_34 */
static json_t *cm2_migrate_to_34(cm2_flow_io_t *io, json_t *results, bool quiet) {
    (void)results;   /* _commit's results effect rides the seam channel (sc->results) */
    json_t *r = cm2_import_check(io, "available_personalities");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "normalize_personality_name");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "prompt_text");
    if (cm2_raised(r)) return r;
    json_free(r);
    r = cm2_import_check(io, "render_personality_prompt");
    if (cm2_raised(r)) return r;
    json_free(r);

    json_t *config = cm2_seam(io, "read_raw_config", json_object());
    if (cm2_raised(config)) return config;
    if (!cm2_is_obj(config)) {
        json_t *e = cm2_attr_raise(config, "get");
        json_free(config);
        return e;
    }
    bool touched = false;
    char *old_name = strdup("");
    bool old_name_truthy = false;
    json_t *fail = NULL;

    json_t *raw_display = json_obj_get(config, "display");
    if (cm2_is_obj(raw_display)) {
        json_t *p = json_obj_get(raw_display, "personality");
        json_t *args = json_object();
        json_set(args, "name", p ? json_copy(p) : json_string(""));
        json_t *nm = cm2_seam(io, "normalize_personality_name", args);
        if (cm2_raised(nm)) { fail = nm; goto done; }
        free(old_name);
        old_name = cm2_py_str(nm);
        old_name_truthy = cm2_truthy(nm);
        json_free(nm);
        if (old_name_truthy) {
            json_set(raw_display, "personality", json_string(""));
            json_set(config, "display", json_copy(raw_display));
            touched = true;
        }
    }

    json_t *raw_agent = json_obj_get(config, "agent");
    bool scrubbed = false;
    if (cm2_is_obj(raw_agent)) {
        json_t *sp = json_obj_get(raw_agent, "system_prompt");
        json_t *args = json_object();
        json_set(args, "text", sp ? json_copy(sp) : json_string(""));
        json_t *manual = cm2_seam(io, "prompt_text", args);
        if (cm2_raised(manual)) { fail = manual; goto done; }
        if (cm2_truthy(manual)) {
            args = json_object();
            json_set(args, "config", json_copy(config));
            json_t *avail = cm2_seam(io, "available_personalities", args);
            if (cm2_raised(avail)) { json_free(manual); fail = avail; goto done; }
            if (!cm2_is_obj(avail)) {
                json_free(manual);
                fail = cm2_attr_raise(avail, "values");
                json_free(avail);
                goto done;
            }
            json_t *rendered = json_array();   /* set model */
            for (size_t i = 0; i < avail->c.count; i++) {
                args = json_object();
                json_set(args, "defn", json_copy(avail->c.items[i]));
                json_t *rr = cm2_seam(io, "render_personality_prompt", args);
                if (cm2_raised(rr)) { json_free(rendered); json_free(avail); json_free(manual); fail = rr; goto done; }
                if (!cm2_member(rendered, rr)) json_append(rendered, json_copy(rr));
                json_free(rr);
            }
            bool in_rendered = cm2_member(rendered, manual);
            json_free(rendered);
            json_free(avail);
            if (in_rendered) {
                json_set(raw_agent, "system_prompt", json_string(""));
                json_set(config, "agent", json_copy(raw_agent));
                touched = true;
                scrubbed = true;
            }
        }
        json_free(manual);
    }

    if (!touched) goto done;
    {
        json_t *args = json_object();
        json_set(args, "config", json_copy(config));
        json_set(args, "quiet", json_bool(quiet));
        json_set(args, "added", json_string(CM2_M34_ADD));
        json_set(args, "message", json_null());
        r = cm2_seam(io, "commit", args);
        if (cm2_raised(r)) { fail = r; goto done; }
        json_free(r);
    }
    if (quiet) goto done;
    if (old_name_truthy) {
        char *m1 = cm2_join(CM2_M34_P1A, old_name);
        char *m2 = cm2_join(m1, CM2_M34_P1B); free(m1);
        char *m3 = cm2_join(m2, old_name); free(m2);
        char *m4 = cm2_join(m3, CM2_M34_P1C); free(m3);
        r = cm2_print(io, m4); free(m4);
        if (cm2_raised(r)) { fail = r; goto done; }
        json_free(r);
    }
    if (scrubbed) {
        r = cm2_print(io, CM2_M34_P2);
        if (cm2_raised(r)) { fail = r; goto done; }
        json_free(r);
    }

done:
    free(old_name);
    json_free(config);
    if (fail) return fail;
    return json_null();
}

/* ===================================================================
 * _migrate_to_38  (config_migrations.py:528)
 * =================================================================== */
/* PoP: cm2_migrate_to_38 @ hermes_cli/config_migrations.py:_migrate_to_38 */
static json_t *cm2_migrate_to_38(cm2_flow_io_t *io, json_t *results, bool quiet) {
    json_t *r = cm2_import_check(io, "legacy_relay_plugin_keys");
    if (cm2_raised(r)) return r;
    json_free(r);

    json_t *config = cm2_seam(io, "read_raw_config", json_object());
    if (cm2_raised(config)) return config;
    if (!cm2_is_obj(config)) {
        json_t *e = cm2_attr_raise(config, "get");
        json_free(config);
        return e;
    }
    json_t *plugins = json_obj_get(config, "plugins");
    if (!cm2_is_obj(plugins)) { json_free(config); return json_null(); }
    json_t *enabled = json_obj_get(plugins, "enabled");   /* borrowed */
    json_t *args = json_object();
    json_set(args, "enabled", enabled ? json_copy(enabled) : json_null());
    json_t *removed = cm2_seam(io, "legacy_relay_plugin_keys", args);
    if (cm2_raised(removed)) { json_free(config); return removed; }
    if (!cm2_truthy(removed) || !cm2_is_arr(enabled)) {
        json_free(removed); json_free(config);
        return json_null();
    }
    json_t *filt = json_array();
    for (size_t i = 0; i < enabled->c.count; i++) {
        if (!cm2_member(removed, enabled->c.items[i]))
            json_append(filt, json_copy(enabled->c.items[i]));
    }
    json_set(plugins, "enabled", filt);
    json_set(config, "plugins", json_copy(plugins));
    args = json_object();
    json_set(args, "config", json_copy(config));
    r = cm2_seam(io, "persist_migration", args);
    if (cm2_raised(r)) { json_free(removed); json_free(config); return r; }
    json_free(r);

    char *j = NULL;
    json_t *e = cm2_join_comma_checked(removed, &j);
    if (e) { json_free(removed); json_free(config); return e; }
    char *m1 = cm2_join(CM2_M38_PRE, j); free(j);
    char *msg = cm2_join(m1, CM2_M38_SUF); free(m1);
    e = cm2_results_append(results, "warnings", json_string(msg));
    if (e) { free(msg); json_free(removed); json_free(config); return e; }
    if (!quiet) {
        char *w = cm2_join(CM2_M38_WARN, msg);
        r = cm2_print(io, w); free(w);
        if (cm2_raised(r)) { free(msg); json_free(removed); json_free(config); return r; }
        json_free(r);
    }
    free(msg);
    json_free(removed);
    json_free(config);
    return json_null();
}

/* ===================================================================
 * _migrate_to_39  (config_migrations.py:554)
 * =================================================================== */
/* PoP: cm2_migrate_to_39 @ hermes_cli/config_migrations.py:_migrate_to_39 */
static json_t *cm2_migrate_to_39(cm2_flow_io_t *io, json_t *results, bool quiet) {
    (void)results;   /* _commit's results effect rides the seam channel (sc->results) */
    json_t *config = cm2_seam(io, "read_raw_config", json_object());
    if (cm2_raised(config)) return config;
    if (!cm2_is_obj(config)) {
        json_t *e = cm2_attr_raise(config, "get");
        json_free(config);
        return e;
    }
    bool changed = false;
    static const char *sections[] = {"platform_toolsets", "known_builtin_toolsets"};
    for (int s = 0; s < 2; s++) {
        json_t *mapping = json_obj_get(config, sections[s]);
        if (!cm2_is_obj(mapping)) continue;
        for (size_t i = 0; i < mapping->c.count; i++) {
            json_t *toolsets = mapping->c.items[i];
            if (cm2_is_arr(toolsets) && cm2_member_str(toolsets, "bfl")) {
                json_t *filt = json_array();
                json_t *bfl = json_string("bfl");
                for (size_t j = 0; j < toolsets->c.count; j++) {
                    if (!cm2_py_eq(toolsets->c.items[j], bfl))
                        json_append(filt, json_copy(toolsets->c.items[j]));
                }
                json_free(bfl);
                json_set(mapping, mapping->c.keys[i], filt);
                changed = true;
            }
        }
        if (changed) json_set(config, sections[s], json_copy(mapping));
    }
    if (changed) {
        json_t *args = json_object();
        json_set(args, "config", json_copy(config));
        json_set(args, "quiet", json_bool(quiet));
        json_set(args, "added", json_string(CM2_M39_ADD));
        json_set(args, "message", json_string(CM2_M39_MSG));
        json_t *r = cm2_seam(io, "commit", args);
        if (cm2_raised(r)) { json_free(config); return r; }
        json_free(r);
    }
    json_free(config);
    return json_null();
}