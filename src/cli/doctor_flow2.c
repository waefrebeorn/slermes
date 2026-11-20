/* doctor_flow2.c — FLOW-core port of the UNCLAIMED doctor sibling remainder.
 *
 * The doctor.py facade itself is fully claimed (doctor_flow.c). This port takes
 * the next unclaimed leaves of the doctor_* siblings. Defining modules (PoP
 * credits the DEFINING module, never the re-exporting doctor.py facade):
 *   hermes_cli/doctor_tools.py      _plural, _setup_gated, _pm_tool_path,
 *                                   _pm_package_for_command, _require,
 *                                   _chromium_not_needed_reason
 *   hermes_cli/doctor_config.py     _failing_boot_migration, _drift_deprecations,
 *                                   _drift_structure, _provider_has_credentials
 *   hermes_cli/doctor_platform.py   _python_repair_hint, _unreadable_reason
 *
 * Universal seam: every collaborator / I/O call goes through
 * df2_seam(io, name, args); the dispatcher appends `name` to io->trace,
 * returns the scripted result, applies "state" effects, and models a raised
 * exception with {"__raise__": {type,message,code?,...}}.
 *
 * AUTO cosmetic seams (no script, never fail closed): "print", "color"
 * (identity), and the report primitives "check_ok"/"check_warn"/"check_fail"/
 * "check_info"/"_section" (capture a {kind,text,detail} row). Claimed siblings
 * (check_bool, warn_on_error, Path stat/access, ...) are modeled as seams.
 * Every other seam is SCRIPTED and FAIL-CLOSED: a missing script raises
 * RuntimeError("unscripted seam: <name>").
 *
 * io->state is a json OBJECT mirroring caller-held state: "issues" (list),
 * "manual_issues" (list), "fixed" (number), plus any seeded key.
 *
 * Exception fidelity: df2_exc_isa walks the FULL py3.14 builtin hierarchy so
 * `except Exception`, `except (ImportError, OSError, ValueError)` and
 * `except OSError` catch exactly what live Python catches. The BaseException
 * trio (SystemExit/KeyboardInterrupt/GeneratorExit) passes through every
 * except-Exception site. Unknown/custom types default to Exception lineage.
 *
 * Representation contract (both sides): paths are posix strings; an uncaught
 * exception rides as {"__raise__": {type,message,code?,...}}; integral JSON
 * numbers read as int (libjson has no int/float identity). Fixture input
 * domain is ASCII; non-ASCII glyphs in message CONSTANTS are byte-pinned UTF-8.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / state /
 * cap_* are harness-owned. df2_seam() frees its args. Every public df2_*
 * function returns a NEW reference (or a raise sentinel).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct df2_io_t {
    json_t *(*call)(struct df2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    json_t *cap_stdout;
    json_t *cap_stderr;
    json_t *cap_rows;
    void   *ctx;
} df2_io_t;

static void df2_trace(df2_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}
static json_t *df2_call(df2_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}
static json_t *df2_seam(df2_io_t *io, const char *name, json_t *args) {
    df2_trace(io, name);
    json_t *r = df2_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    return r;
}
static bool df2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *df2_raise_spec(const json_t *r) {
    return df2_raised(r) ? json_obj_get(r, "__raise__") : NULL;
}
static json_t *df2_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type ? type : "Exception"));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool df2_truthy(const json_t *v) {
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
static bool df2_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool df2_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool df2_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool df2_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *df2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static char *df2_fmt(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *out = malloc((size_t)n + 1);
    if (out) vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}
static char *df2_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}
static char *df2_lower_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && (p[a]==' '||p[a]=='\t'||p[a]=='\n'||p[a]=='\r'||p[a]=='\v'||p[a]=='\f')) a++;
    while (b > a && (p[b-1]==' '||p[b-1]=='\t'||p[b-1]=='\n'||p[b-1]=='\r'||p[b-1]=='\v'||p[b-1]=='\f')) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    for (size_t i = a; i < b; i++) out[i-a] = (char)tolower((unsigned char)p[i]);
    out[b-a] = '\0';
    return out;
}

/* ── FULL py3.14 builtin exception hierarchy ───────────────────────────── */
static const char *df2_exc_parent(const char *t) {
    if (!t) return "Exception";
    struct P { const char *n, *p; };
    static const struct P T[] = {
        {"BaseException",NULL},{"Exception","BaseException"},
        {"GeneratorExit","BaseException"},{"KeyboardInterrupt","BaseException"},{"SystemExit","BaseException"},
        {"BaseExceptionGroup","BaseException"},{"ExceptionGroup","Exception"},
        {"ArithmeticError","Exception"},{"AssertionError","Exception"},{"AttributeError","Exception"},
        {"BufferError","Exception"},{"EOFError","Exception"},{"ImportError","Exception"},
        {"LookupError","Exception"},{"MemoryError","Exception"},{"NameError","Exception"},
        {"OSError","Exception"},{"ReferenceError","Exception"},{"RuntimeError","Exception"},
        {"StopAsyncIteration","Exception"},{"StopIteration","Exception"},{"SyntaxError","Exception"},
        {"SystemError","Exception"},{"TypeError","Exception"},{"ValueError","Exception"},
        {"Warning","Exception"},
        {"BlockingIOError","OSError"},{"BrokenPipeError","ConnectionError"},
        {"ChildProcessError","OSError"},{"ConnectionError","OSError"},
        {"ConnectionAbortedError","ConnectionError"},{"ConnectionRefusedError","ConnectionError"},
        {"ConnectionResetError","ConnectionError"},{"FileExistsError","OSError"},{"FileNotFoundError","OSError"},
        {"InterruptedError","OSError"},{"IsADirectoryError","OSError"},{"NotADirectoryError","OSError"},
        {"PermissionError","OSError"},{"ProcessLookupError","OSError"},{"TimeoutError","OSError"},
        {"EnvironmentError","OSError"},{"IOError","OSError"},{"WindowsError","OSError"},
        {"FloatingPointError","ArithmeticError"},{"OverflowError","ArithmeticError"},{"ZeroDivisionError","ArithmeticError"},
        {"IndexError","LookupError"},{"KeyError","LookupError"},
        {"UnboundLocalError","NameError"},{"ModuleNotFoundError","ImportError"},
        {"NotImplementedError","RuntimeError"},{"PythonFinalizationError","RuntimeError"},{"RecursionError","RuntimeError"},
        {"IndentationError","SyntaxError"},{"TabError","IndentationError"},{"_IncompleteInputError","SyntaxError"},
        {"UnicodeError","ValueError"},{"UnicodeDecodeError","UnicodeError"},{"UnicodeEncodeError","UnicodeError"},
        {"UnicodeTranslateError","UnicodeError"},
        {"BytesWarning","Warning"},{"DeprecationWarning","Warning"},{"EncodingWarning","Warning"},
        {"FutureWarning","Warning"},{"ImportWarning","Warning"},{"PendingDeprecationWarning","Warning"},
        {"ResourceWarning","Warning"},{"RuntimeWarning","Warning"},{"SyntaxWarning","Warning"},
        {"UnicodeWarning","Warning"},{"UserWarning","Warning"},
    };
    for (size_t i = 0; i < sizeof(T)/sizeof(T[0]); i++)
        if (strcmp(t, T[i].n) == 0) return T[i].p ? T[i].p : "BaseException";
    return "Exception";   /* custom/unknown -> Exception lineage */
}
static bool df2_exc_isa(const char *t, const char *base) {
    if (!t || !base) return false;
    const char *cur = t;
    for (int i = 0; i < 24 && cur; i++) {
        if (strcmp(cur, base) == 0) return true;
        cur = df2_exc_parent(cur);
    }
    return false;
}
/* BaseException trio passes through every except-Exception site. */
static bool df2_base_exc(const char *t) {
    return t && (strcmp(t,"SystemExit")==0 || strcmp(t,"KeyboardInterrupt")==0 || strcmp(t,"GeneratorExit")==0);
}
/* A raise is caught by a tuple of except families (base kinds). */
static bool df2_caught_by(const json_t *r, const char *const *bases, size_t n) {
    json_t *spec = df2_raise_spec(r);
    if (!spec) return false;
    const char *t = json_get_str(spec, "type", "");
    if (df2_base_exc(t)) return false;
    for (size_t i = 0; i < n; i++) if (df2_exc_isa(t, bases[i])) return true;
    return false;
}

/* ── state helpers ─────────────────────────────────────────────────────── */
static json_t *df2_state_arr(df2_io_t *io, const char *key) {
    json_t *a = io->state ? json_obj_get(io->state, key) : NULL;
    if (a && a->type == JSON_ARRAY) return a;
    a = json_array();
    if (io->state) json_set(io->state, key, a);
    return a;
}
static void df2_append_issue(df2_io_t *io, const char *key, const char *msg) {
    json_append(df2_state_arr(io, key), json_string(msg ? msg : ""));
}

/* ── report primitives (AUTO seams) ────────────────────────────────────── */
static void df2_report(df2_io_t *io, const char *seam, const char *kind,
                       const char *text, const char *detail) {
    df2_trace(io, seam);
    json_t *row = json_object();
    json_set(row, "kind", json_string(kind));
    json_set(row, "text", json_string(text ? text : ""));
    json_set(row, "detail", json_string(detail ? detail : ""));
    if (io->cap_rows && io->cap_rows->type == JSON_ARRAY) json_append(io->cap_rows, row);
    else json_free(row);
}
static void df2_section(df2_io_t *io, const char *title) {
    df2_report(io, "_section", "section", title, "");
}

/* ══════════════════════ PORTED FLOW FUNCTIONS ═══════════════════════════ */

/* PoP: df2_plural @ hermes_cli/doctor_tools.py:_plural */
json_t *df2_plural(double n) {
    return json_string(n == 1.0 ? "vulnerability" : "vulnerabilities");
}

/* PoP: df2_setup_gated @ hermes_cli/doctor_tools.py:_setup_gated */
json_t *df2_setup_gated(const json_t *item) {
    if (df2_truthy(json_obj_get(item, "missing_vars")) || df2_truthy(json_obj_get(item, "env_vars"))) {
        return json_bool(true);
    }
    const char *name = df2_sval(json_obj_get(item, "name"));
    return json_bool(strcmp(name, "image_gen") == 0);
}

/* PoP: df2_pm_tool_path @ hermes_cli/doctor_tools.py:_pm_tool_path */
json_t *df2_pm_tool_path(df2_io_t *io, const char *name) {
    json_t *a = json_object();
    json_set(a, "name", json_string(name ? name : ""));
    json_t *r = df2_seam(io, "installed_package", a);
    if (df2_raised(r)) {
        if (df2_caught_by(r, (const char *[]){"Exception"}, 1)) { json_free(r); return json_null(); }
        return r;  /* BaseException propagates */
    }
    if (df2_null(r)) { json_free(r); return json_null(); }
    json_t *bin = json_obj_get(r, "binary");
    json_t *out = (bin && !df2_null(bin)) ? json_copy(bin) : json_null();
    json_free(r);
    return out;
}

/* PoP: df2_pm_package_for_command @ hermes_cli/doctor_tools.py:_pm_package_for_command */
json_t *df2_pm_package_for_command(df2_io_t *io, const char *command) {
    json_t *tgt = df2_seam(io, "current_target", json_object());
    if (df2_raised(tgt)) {
        if (df2_caught_by(tgt, (const char *[]){"Exception"}, 1)) { json_free(tgt); return json_null(); }
        return tgt;
    }
    json_t *names = df2_seam(io, "all_packages", json_object());
    if (df2_raised(names)) {
        if (df2_caught_by(names, (const char *[]){"Exception"}, 1)) { json_free(tgt); json_free(names); return json_null(); }
        json_free(tgt); return names;
    }
    json_t *result = json_null();
    if (df2_arr(names)) {
        for (size_t i = 0; i < names->c.count; i++) {
            const char *pname = df2_sval(json_get(names, i));
            json_t *pa = json_object(); json_set(pa, "name", json_string(pname));
            json_t *pkg = df2_seam(io, "get_package", pa);
            if (df2_raised(pkg)) {
                int caught = df2_caught_by(pkg, (const char *[]){"Exception"}, 1);
                json_free(names); json_free(tgt);
                if (caught) { json_free(pkg); return json_null(); }
                return pkg;  /* BaseException propagates */
            }
            bool isbin = json_get_bool(pkg, "binary", false);
            json_free(pkg);
            if (!isbin) continue;
            json_t *ra = json_object();
            json_set(ra, "target", tgt ? json_copy(tgt) : json_null());
            json_set(ra, "name", json_string(pname));
            json_t *relr = df2_seam(io, "_rel", ra);
            if (df2_raised(relr)) {
                int caught = df2_caught_by(relr, (const char *[]){"Exception"}, 1);
                json_free(names); json_free(tgt);
                if (caught) { json_free(relr); return json_null(); }
                return relr;  /* BaseException propagates */
            }
            const char *rel = df2_sval(relr);
            json_free(relr);
            if (!rel[0]) continue;
            /* Path(rel).name.removesuffix(".exe").removesuffix(".cmd") */
            const char *slash = strrchr(rel, '/');
            const char *base = slash ? slash + 1 : rel;
            size_t nb = strlen(base);
            if (nb >= 4 && strcmp(base + nb - 4, ".exe") == 0) nb -= 4;
            else if (nb >= 4 && strcmp(base + nb - 4, ".cmd") == 0) nb -= 4;
            if ((int)nb == (int)strlen(command) && strncmp(base, command, nb) == 0) {
                result = json_string(pname);
                break;
            }
        }
    }
    json_free(names); json_free(tgt);
    return result;
}

/* PoP: df2_require @ hermes_cli/doctor_tools.py:_require */
json_t *df2_require(df2_io_t *io, const json_t *cond, const json_t *ok,
                    const json_t *bad, const char *issue) {
    json_t *a = json_object();
    json_set(a, "cond", cond ? json_copy(cond) : json_null());
    json_set(a, "ok", ok ? json_copy(ok) : json_null());
    json_set(a, "bad", bad ? json_copy(bad) : json_null());
    json_t *r = df2_seam(io, "check_bool", a);
    if (df2_raised(r)) return r;
    bool good = df2_truthy(r);
    json_free(r);
    if (!good) df2_append_issue(io, "issues", issue);
    return json_null();
}

/* PoP: df2_chromium_not_needed_reason @ hermes_cli/doctor_tools.py:_chromium_not_needed_reason */
json_t *df2_chromium_not_needed_reason(df2_io_t *io) {
    json_t *cfg = df2_seam(io, "load_config_readonly", json_object());
    if (df2_raised(cfg)) return cfg;
    json_free(cfg);
    json_t *ea = json_object(); json_set(ea, "key", json_string("BROWSER_CDP_URL"));
    json_t *cdp = df2_seam(io, "environ_get", ea);
    if (df2_raised(cdp)) return cdp;
    bool cdp_on = df2_truthy(cdp);
    json_free(cdp);
    if (!cdp_on) {
        json_t *a = json_object(); json_set(a, "key", json_string("cdp_url"));
        json_t *r = df2_seam(io, "cfg_get", a);
        if (df2_raised(r)) return r;
        cdp_on = df2_truthy(r);
        json_free(r);
    }
    if (cdp_on) return json_string("a CDP override is set");
    json_t *ea2 = json_object(); json_set(ea2, "key", json_string("engine"));
    json_t *eng = df2_seam(io, "cfg_get", ea2);
    if (df2_raised(eng)) return eng;
    char *el = df2_lower_strip(df2_sval(eng));
    json_free(eng);
    bool light = strcmp(el, "lightpanda") == 0;
    free(el);
    if (light) return json_string("browser.engine is lightpanda");
    json_t *pa = json_object(); json_set(pa, "key", json_string("cloud_provider"));
    json_t *prov = df2_seam(io, "cfg_get", pa);
    if (df2_raised(prov)) return prov;
    char *pl = df2_lower_strip(df2_sval(prov));
    json_free(prov);
    if (strcmp(pl, "") != 0 && strcmp(pl, "local") != 0) {
        char *msg = df2_fmt("cloud provider %s is selected", pl);
        free(pl);
        json_t *out = json_string(msg ? msg : ""); free(msg);
        return out;
    }
    free(pl);
    return json_string("");
}

/* PoP: df2_failing_boot_migration @ hermes_cli/doctor_config.py:_failing_boot_migration */
json_t *df2_failing_boot_migration(df2_io_t *io) {
    static const char *const CAUGHT[] = {"ImportError", "OSError", "ValueError"};
    json_t *root = df2_seam(io, "install_root", json_object());
    if (df2_raised(root)) {
        bool c = df2_caught_by(root, CAUGHT, 3); if (c) json_free(root); return c ? json_null() : root;
    }
    json_t *ra = json_object(); json_set(ra, "root", root ? json_copy(root) : json_null());
    json_t *path = df2_seam(io, "record_path", ra);
    if (df2_raised(path)) {
        bool c = df2_caught_by(path, CAUGHT, 3); if (c) { json_free(root); json_free(path); } return c ? json_null() : path;
    }
    json_t *pa = json_object(); json_set(pa, "path", path ? json_copy(path) : json_null());
    json_t *last = df2_seam(io, "read_last_known", pa);
    if (df2_raised(last)) {
        bool c = df2_caught_by(last, CAUGHT, 3); if (c) { json_free(root); json_free(path); json_free(last); } return c ? json_null() : last;
    }
    json_t *oa = json_object(); json_set(oa, "last", last ? json_copy(last) : json_null());
    json_t *owed = df2_seam(io, "owed_config_migration", oa);
    if (df2_raised(owed)) {
        bool c = df2_caught_by(owed, CAUGHT, 3); if (c) { json_free(root); json_free(path); json_free(last); json_free(owed); } return c ? json_null() : owed;
    }
    json_free(root); json_free(path); json_free(last);
    if (df2_null(owed) || !df2_truthy(owed)) { json_free(owed); return json_null(); }
    json_t *w = json_obj_get(owed, "failed_warning");
    json_t *e = json_obj_get(owed, "error");
    json_t *out = (w && !df2_null(w)) ? json_copy(w) : ((e && !df2_null(e)) ? json_copy(e) : json_null());
    json_free(owed);
    return out;
}

/* PoP: df2_drift_deprecations @ hermes_cli/doctor_config.py:_drift_deprecations */
json_t *df2_drift_deprecations(df2_io_t *io, const char *config_path) {
    json_t *raw = json_object();
    if (config_path) {
        json_t *a = json_object(); json_set(a, "path", json_string(config_path));
        json_t *r = df2_seam(io, "read_user_config_raw", a);
        if (df2_raised(r)) return r;
        if (!df2_null(r)) { json_free(raw); raw = r; } else json_free(r);
    }
    json_t *er = df2_seam(io, "load_env", json_object());
    json_t *env = json_object();
    if (df2_raised(er)) {
        if (df2_base_exc(json_get_str(df2_raise_spec(er), "type", ""))) { json_free(raw); return er; }
        json_free(er);  /* warn_on_error swallows Exception */
    } else {
        if (!df2_null(er)) { json_free(env); env = er; } else json_free(er);
    }
    json_t *a = json_object();
    json_set(a, "raw", json_copy(raw));
    json_set(a, "env", json_copy(env));
    json_t *r = df2_seam(io, "report_deprecated_config_and_env", a);
    if (df2_raised(r)) { json_free(raw); json_free(env); return r; }
    json_free(r); json_free(raw); json_free(env);
    return json_null();
}

/* PoP: df2_drift_structure @ hermes_cli/doctor_config.py:_drift_structure */
json_t *df2_drift_structure(df2_io_t *io) {
    json_t *issues = df2_seam(io, "validate_config_structure", json_object());
    if (df2_raised(issues)) return issues;
    if (df2_null(issues) || !df2_truthy(issues)) { json_free(issues); return json_null(); }
    df2_section(io, "Config Structure");
    for (size_t i = 0; i < issues->c.count; i++) {
        json_t *ci = json_get(issues, i);
        const char *sev = df2_sval(json_obj_get(ci, "severity"));
        const char *seam = (strcmp(sev, "error") == 0) ? "check_fail" : "check_warn";
        const char *msg = df2_sval(json_obj_get(ci, "message"));
        df2_report(io, seam, (strcmp(seam,"check_fail")==0)?"fail":"warn", msg, "");
        const char *hint = df2_sval(json_obj_get(ci, "hint"));
        /* str.splitlines() on \n and \r\n (ASCII fixture domain) */
        const char *p = hint;
        while (*p) {
            const char *e = p; while (*e && *e != '\n' && *e != '\r') e++;
            size_t len = (size_t)(e - p);
            char *line = malloc(len + 1); if (line) { memcpy(line, p, len); line[len]='\0'; df2_report(io, "check_info", "info", line, ""); free(line); }
            if (*e == '\r') e++;
            if (*e == '\n') e++;
            p = e;
        }
        df2_append_issue(io, "issues", msg);
    }
    json_free(issues);
    return json_null();
}

/* PoP: df2_provider_has_credentials @ hermes_cli/doctor_config.py:_provider_has_credentials */
json_t *df2_provider_has_credentials(df2_io_t *io, const char *provider) {
    if (strcmp(provider, "openrouter") == 0) {
        const char *keys[] = {"OPENROUTER_API_KEY", "OPENAI_API_KEY"};
        for (int i = 0; i < 2; i++) {
            json_t *a = json_object(); json_set(a, "key", json_string(keys[i]));
            json_t *v = df2_seam(io, "get_env_value", a);
            if (df2_raised(v)) return v;
            bool on = df2_truthy(v); json_free(v);
            if (on) return json_bool(true);
        }
        return json_bool(false);
    }
    json_t *reg = df2_seam(io, "provider_registry", json_object());
    if (df2_raised(reg)) return reg;
    json_t *pconfig = df2_obj(reg) ? json_obj_get(reg, provider) : NULL;
    const char *atype = (pconfig && df2_obj(pconfig)) ? df2_sval(json_obj_get(pconfig, "auth_type")) : "";
    bool apikey = (pconfig && df2_truthy(pconfig)) && strcmp(atype, "api_key") == 0;
    json_free(reg);
    if (!apikey) return json_bool(true);
    json_t *a = json_object(); json_set(a, "provider", json_string(provider));
    json_t *st = df2_seam(io, "get_auth_status", a);
    if (df2_raised(st)) return st;
    bool ok = false;
    if (df2_obj(st)) {
        ok = df2_truthy(json_obj_get(st, "configured")) || df2_truthy(json_obj_get(st, "logged_in")) || df2_truthy(json_obj_get(st, "api_key"));
    }
    json_free(st);
    return json_bool(ok);
}

/* PoP: df2_python_repair_hint @ hermes_cli/doctor_platform.py:_python_repair_hint */
json_t *df2_python_repair_hint(df2_io_t *io) {
    json_t *mr = df2_seam(io, "detect_install_method", json_object());
    if (df2_raised(mr)) return mr;
    char *method = strdup(df2_sval(mr));
    json_t *na = json_object(); json_set(na, "method", json_copy(mr));
    json_t *nr = df2_seam(io, "is_nix_install_method", na);
    json_free(mr);
    if (df2_raised(nr)) { free(method); return nr; }
    bool nix = df2_truthy(nr); json_free(nr);
    json_t *ca = json_object();
    if (nix) {
        json_t *cmd = df2_seam(io, "recommended_update_command_for_method", ca);
        free(method);
        if (df2_raised(cmd)) return cmd;
        json_t *out = json_copy(cmd); json_free(cmd); return out;
    }
    json_set(ca, "method", json_string(method ? method : ""));
    bool docker_apt = (strcmp(method, "docker") == 0 || strcmp(method, "apt") == 0);
    json_t *cmd = NULL;
    const char *cs = "";
    if (docker_apt) {
        cmd = df2_seam(io, "recommended_update_command_for_method", ca);
        if (df2_raised(cmd)) { free(method); return cmd; }
        cs = df2_sval(cmd);
    }
    json_t *out;
    if (docker_apt) {
        char *s = df2_fmt("Run `%s`", cs);
        if (strcmp(method, "docker") == 0) { char *s2 = df2_join(s, ", then recreate the Hermes container"); free(s); s = s2; }
        out = json_string(s ? s : ""); free(s);
    } else {
        out = json_string("Run `hermes pm repair`, then restart Hermes");
    }
    free(method);
    if (cmd) json_free(cmd);
    return out;
}

/* PoP: df2_unreadable_reason @ hermes_cli/doctor_platform.py:_unreadable_reason */
json_t *df2_unreadable_reason(df2_io_t *io, const char *path) {
    json_t *a = json_object(); json_set(a, "path", json_string(path ? path : ""));
    json_t *r = df2_seam(io, "path_stat", a);
    if (df2_raised(r)) {
        json_t *spec = df2_raise_spec(r);
        if (df2_exc_isa(json_get_str(spec, "type", ""), "OSError")) {
            json_t *out = json_string(json_get_str(spec, "message", ""));
            json_free(r); return out;
        }
        return r;  /* non-OSError propagates */
    }
    json_free(r);
    json_t *aa = json_object(); json_set(aa, "path", json_string(path ? path : "")); json_set(aa, "mode", json_string("R_OK"));
    json_t *acc = df2_seam(io, "os_access", aa);
    if (df2_raised(acc)) return acc;
    bool ok = df2_truthy(acc); json_free(acc);
    if (ok) return json_string("file could not be read");
    char *s = df2_fmt("permission denied: %s", path ? path : "");
    json_t *out = json_string(s ? s : ""); free(s);
    return out;
}
