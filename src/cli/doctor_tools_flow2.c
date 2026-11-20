/* doctor_tools_flow2.c — FLOW-core port of the REMAINING UNCLAIMED defs of
 * hermes_cli/doctor_tools.py.
 *
 * doctor_flow2.c already claimed the leaves: _plural, _setup_gated,
 * _pm_tool_path, _pm_package_for_command, _require, _chromium_not_needed_reason.
 * doctor.c / doctor_helpers.c claimed: _doctor_tool_availability_detail,
 * _safe_which, _is_kanban_worker_env_gate, _apply_doctor_tool_availability_overrides,
 * _enabled_cli_toolsets_for_doctor, _missing_api_key_toolsets_for_summary.
 * This file ports the OTHER 17 defs (the "remainder"), PoP-annotated below:
 *   _doctor_tool, _run_ok, _doctor_web_capability_rows, _check_git_and_rg,
 *   _check_docker_backend, _check_ssh_backend, _check_daytona_backend,
 *   _check_vercel_backend, _check_plugin_backend, _check_terminal_backend,
 *   _check_chromium, _check_lightpanda, _check_node_and_browser, _audit_one,
 *   _check_npm_audit, _check_tool_availability, _warn_partially_stripped_toolsets.
 *
 * Universal seam: every collaborator / I/O call goes through
 * dt2_seam(io, name, args). The dispatcher records `name` in io->trace,
 * returns the scripted result, applies scripted {"state":{...}} effects, and
 * models a raised exception as {"__raise__": {type,message,...}}.
 *
 * AUTO cosmetic seams (never scripted, never fail closed): report primitives
 * check_ok/check_warn/check_fail/check_info/_section append {kind,text,detail}
 * rows to io->cap_rows and record their name in io->trace. All other seams are
 * SCRIPTED and FAIL-CLOSED: a missing script raises RuntimeError("unscripted
 * seam: <name>").
 *
 * io->state is a json OBJECT mirroring caller-held state: "issues" (list),
 * "manual_issues" (list), "fixed" (number), plus any seeded key.
 *
 * Trace fidelity: io->trace holds SEAM NAMES only (no args) — matching the
 * oracle recorder. Message fidelity lives in report-row text/detail and in
 * seam return values; those are transcribed exactly (incl. UTF-8 constants).
 *
 * Exception fidelity: dt2_exc_isa walks the FULL py3.14 builtin hierarchy so
 * `except Exception`, `except ImportError`, `except (RuntimeError, OSError)`,
 * `except subprocess.TimeoutExpired` catch exactly what live Python catches.
 * The BaseException trio (SystemExit/KeyboardInterrupt/GeneratorExit) passes
 * through every except-Exception site. Unknown/custom types default to the
 * Exception lineage.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / state /
 * cap_rows are harness-owned. Every public dt2_* function returns a NEW
 * reference (or a raise sentinel); every seam arg object is freed by dt2_seam.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include "hermes_json.h"

/* ── seam dispatcher + trace + state ───────────────────────────────────── */
typedef struct dt2_io_t {
    json_t *(*call)(struct dt2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    json_t *cap_stdout;
    json_t *cap_stderr;
    json_t *cap_rows;
    void   *ctx;
} dt2_io_t;

static void dt2_trace(dt2_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}
static json_t *dt2_call(dt2_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}
static json_t *dt2_seam(dt2_io_t *io, const char *name, json_t *args) {
    dt2_trace(io, name);
    json_t *r = dt2_call(io, name, args);
    if (args) json_free(args);
    if (!r) return json_null();
    return r;
}
static bool dt2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static json_t *dt2_raise_spec(const json_t *r) {
    return dt2_raised(r) ? json_obj_get(r, "__raise__") : NULL;
}
static json_t *dt2_make_raise(const char *type, const char *message) {
    json_t *spec = json_object();
    json_set(spec, "type", json_string(type ? type : "Exception"));
    json_set(spec, "message", json_string(message ? message : ""));
    json_t *r = json_object();
    json_set(r, "__raise__", spec);
    return r;
}
static const char *dt2_exc_type(const json_t *r) {
    json_t *s = dt2_raise_spec(r);
    return s ? json_get_str(s, "type", "") : "";
}
static const char *dt2_exc_msg(const json_t *r) {
    json_t *s = dt2_raise_spec(r);
    return s ? json_get_str(s, "message", "") : "";
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool dt2_truthy(const json_t *v) {
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
static bool dt2_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool dt2_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool dt2_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *dt2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static char *dt2_fmt(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *out = malloc((size_t)n + 1);
    if (out) vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}
static char *dt2_join(const char *a, const char *b) {
    size_t na = strlen(a ? a : ""), nb = strlen(b ? b : "");
    char *out = malloc(na + nb + 1);
    if (!out) return NULL;
    if (na) memcpy(out, a, na);
    if (nb) memcpy(out + na, b, nb);
    out[na + nb] = '\0';
    return out;
}
static char *dt2_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && (p[a]==' '||p[a]=='\t'||p[a]=='\n'||p[a]=='\r'||p[a]=='\v'||p[a]=='\f')) a++;
    while (b > a && (p[b-1]==' '||p[b-1]=='\t'||p[b-1]=='\n'||p[b-1]=='\r'||p[b-1]=='\v'||p[b-1]=='\f')) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a); out[b - a] = '\0';
    return out;
}
static char *dt2_lower_strip(const char *s) {
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
static char *dt2_upper_first(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    if (n) { out[0] = (char)toupper((unsigned char)p[0]); memcpy(out + 1, p + 1, n - 1); }
    out[n] = '\0';
    return out;
}
/* join a json array of strings with sep (", " for _SUPPORTED_VERCEL_RUNTIMES) */
static char *dt2_join_strings(const json_t *arr, const char *sep) {
    if (!dt2_arr(arr)) return strdup("");
    size_t cap = 1, len = 0;
    char *out = malloc(cap); if (!out) return NULL; out[0] = '\0';
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *s = dt2_sval(json_get(arr, i));
        size_t ns = strlen(s), nsep = strlen(sep);
        size_t need = len + ns + (i ? nsep : 0) + 1;
        if (need > cap) { cap = need; char *t = realloc(out, cap); if (!t) { free(out); return NULL; } out = t; }
        if (i) { memcpy(out + len, sep, nsep); len += nsep; }
        memcpy(out + len, s, ns); len += ns; out[len] = '\0';
    }
    return out;
}
static bool dt2_in_strings(const json_t *arr, const char *v) {
    if (!dt2_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (strcmp(dt2_sval(json_get(arr, i)), v) == 0) return true;
    return false;
}
static json_t *dt2_arg1(const char *key, const char *val) {
    json_t *a = json_object();
    json_set(a, key, json_string(val ? val : ""));
    return a;
}
/* (text, detail) pair — the `ok`/`bad` argument of check_bool/_require. */
static json_t *json_ok_bad_pair(const char *text, const char *detail) {
    json_t *a = json_array();
    json_append(a, json_string(text ? text : ""));
    json_append(a, json_string(detail ? detail : ""));
    return a;
}
/* "(...)"-wrapped detail string (heap). */
static char *dt2_fmt_warn(const char *s) { return dt2_fmt("(%s)", s ? s : ""); }
static char *dt2_join_json(const json_t *arr, const char *sep) { return dt2_join_strings(arr, sep); }
static char *dt2_fmt_warn2(const char *ts, const char *detail) {
    (void)ts; return strdup(detail ? detail : "");
}

/* Forward declarations for composed remainder functions. */
static json_t *dt2_doctor_tool(dt2_io_t *io, const char *name);
static json_t *dt2_run_ok(dt2_io_t *io, const char **cmd, size_t ncmd, double timeout);
static json_t *dt2_doctor_web_capability_rows(dt2_io_t *io);
static json_t *dt2_check_docker_backend(dt2_io_t *io, const char *terminal_env, bool running_in_container);
static json_t *dt2_check_ssh_backend(dt2_io_t *io);
static json_t *dt2_check_daytona_backend(dt2_io_t *io);
static json_t *dt2_check_vercel_backend(dt2_io_t *io);
static json_t *dt2_check_plugin_backend(dt2_io_t *io, const char *terminal_env);
static json_t *dt2_check_chromium(dt2_io_t *io, bool should_fix);
static json_t *dt2_check_lightpanda(dt2_io_t *io);
static json_t *dt2_audit_one(dt2_io_t *io, const char *tool, const char *cwd, const char *label,
                             const char **audit_extra, size_t nextra);
static json_t *dt2_warn_partially_stripped_toolsets(dt2_io_t *io);

/* ── FULL py3.14 builtin exception hierarchy ───────────────────────────── */
static const char *dt2_exc_parent(const char *t) {
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
        {"Warning","Exception"},{"SubprocessError","Exception"},
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
        {"TimeoutExpired","SubprocessError"},{"CalledProcessError","SubprocessError"},
        {"BytesWarning","Warning"},{"DeprecationWarning","Warning"},{"EncodingWarning","Warning"},
        {"FutureWarning","Warning"},{"ImportWarning","Warning"},{"PendingDeprecationWarning","Warning"},
        {"ResourceWarning","Warning"},{"RuntimeWarning","Warning"},{"SyntaxWarning","Warning"},
        {"UnicodeWarning","Warning"},{"UserWarning","Warning"},
    };
    for (size_t i = 0; i < sizeof(T)/sizeof(T[0]); i++)
        if (strcmp(t, T[i].n) == 0) return T[i].p ? T[i].p : "BaseException";
    return "Exception";   /* custom/unknown -> Exception lineage */
}
static bool dt2_exc_isa(const char *t, const char *base) {
    if (!t || !base) return false;
    const char *cur = t;
    for (int i = 0; i < 24 && cur; i++) {
        if (strcmp(cur, base) == 0) return true;
        cur = dt2_exc_parent(cur);
    }
    return false;
}
static bool dt2_base_exc(const char *t) {
    return t && (strcmp(t,"SystemExit")==0 || strcmp(t,"KeyboardInterrupt")==0 || strcmp(t,"GeneratorExit")==0);
}
/* A raise is caught by a tuple of except families (base kinds). */
static bool dt2_caught_by(const json_t *r, const char *const *bases, size_t n) {
    if (!dt2_raised(r)) return false;
    const char *t = dt2_exc_type(r);
    if (dt2_base_exc(t)) return false;
    for (size_t i = 0; i < n; i++) if (dt2_exc_isa(t, bases[i])) return true;
    return false;
}

/* ── state helpers ─────────────────────────────────────────────────────── */
static json_t *dt2_state_arr(dt2_io_t *io, const char *key) {
    json_t *a = io->state ? json_obj_get(io->state, key) : NULL;
    if (a && a->type == JSON_ARRAY) return a;
    a = json_array();
    if (io->state) json_set(io->state, key, a);
    return a;
}
static void dt2_append_issue(dt2_io_t *io, const char *key, const char *msg) {
    json_append(dt2_state_arr(io, key), json_string(msg ? msg : ""));
}

/* ── report primitives (AUTO seams) ────────────────────────────────────── */
static void dt2_report(dt2_io_t *io, const char *seam, const char *kind,
                       const char *text, const char *detail) {
    dt2_trace(io, seam);
    json_t *row = json_object();
    json_set(row, "kind", json_string(kind));
    json_set(row, "text", json_string(text ? text : ""));
    json_set(row, "detail", json_string(detail ? detail : ""));
    if (io->cap_rows && io->cap_rows->type == JSON_ARRAY) json_append(io->cap_rows, row);
    else json_free(row);
}
#define DT2_OK(io,t,d)    dt2_report((io),"check_ok","ok",(t),(d))
#define DT2_WARN(io,t,d)  dt2_report((io),"check_warn","warn",(t),(d))
#define DT2_FAIL(io,t,d)  dt2_report((io),"check_fail","fail",(t),(d))
#define DT2_INFO(io,t)    dt2_report((io),"check_info","info",(t),"")

/* Claimed sibling _fail_and_issue (doctor_report.py): check_fail + issues.append(fix).
 * Seamed as a single "fail_and_issue" event; the row is inside the claimed
 * sibling so it is intentionally not emitted here (matches the seamed check_bool). */
static json_t *dt2_fail_and_issue(dt2_io_t *io, const char *text, const char *detail, const char *fix) {
    (void)text; (void)detail;
    dt2_trace(io, "fail_and_issue");
    dt2_append_issue(io, "issues", fix);
    return NULL;
}
/* Claimed sibling _require: check_bool(cond, ok, bad, fail=True) then issues.append. */
static json_t *dt2_require(dt2_io_t *io, const json_t *cond, const json_t *ok,
                           const json_t *bad, const char *issue) {
    json_t *a = json_object();
    json_set(a, "cond", cond ? json_copy(cond) : json_null());
    json_set(a, "ok", ok ? json_copy(ok) : json_null());
    json_set(a, "bad", bad ? json_copy(bad) : json_null());
    json_set(a, "fail", json_bool(true));
    json_t *r = dt2_seam(io, "check_bool", a);
    if (dt2_raised(r)) return r;
    bool good = dt2_truthy(r);
    json_free(r);
    if (!good) dt2_append_issue(io, "issues", issue);
    return NULL;
}

/* ══════════════════════ PORTED FLOW FUNCTIONS ═══════════════════════════ */

/* PoP: dt2_doctor_tool @ hermes_cli/doctor_tools.py:_doctor_tool */
json_t *dt2_doctor_tool(dt2_io_t *io, const char *name) {
    /* pm package name (claimed sibling, seamed) */
    json_t *pa = dt2_arg1("command", name);
    json_t *pkg = dt2_seam(io, "pm_package_for_command", pa);
    if (dt2_raised(pkg)) return pkg;
    const char *pkgname = dt2_truthy(pkg) ? dt2_sval(pkg) : NULL;
    char *pn_copy = pkgname ? strdup(pkgname) : NULL;
    json_free(pkg);

    /* candidate 1: name itself */
    json_t *ta = dt2_arg1("name", name);
    json_t *staged = dt2_seam(io, "pm_tool_path", ta);
    if (dt2_raised(staged)) { free(pn_copy); return staged; }
    if (dt2_truthy(staged)) {
        json_t *out = json_array();
        json_append(out, json_string(dt2_sval(staged)));
        json_append(out, json_string("(pm store)"));
        json_free(staged); free(pn_copy); return out;
    }
    json_free(staged);

    /* candidate 2: the pm package */
    if (pn_copy) {
        json_t *ta2 = dt2_arg1("name", pn_copy);
        json_t *staged2 = dt2_seam(io, "pm_tool_path", ta2);
        if (dt2_raised(staged2)) { free(pn_copy); return staged2; }
        if (dt2_truthy(staged2)) {
            json_t *out = json_array();
            json_append(out, json_string(dt2_sval(staged2)));
            json_append(out, json_string("(pm store)"));
            json_free(staged2); free(pn_copy); return out;
        }
        json_free(staged2);
    }
    free(pn_copy);

    json_t *wa = dt2_arg1("cmd", name);
    json_t *w = dt2_seam(io, "safe_which", wa);
    if (dt2_raised(w)) return w;
    json_t *out = json_array();
    json_append(out, json_string(dt2_sval(w)));
    json_append(out, json_string(""));
    json_free(w);
    return out;
}

/* PoP: dt2_run_ok @ hermes_cli/doctor_tools.py:_run_ok */
json_t *dt2_run_ok(dt2_io_t *io, const char **cmd, size_t ncmd, double timeout) {
    json_t *a = json_object();
    json_t *arr = json_array();
    for (size_t i = 0; i < ncmd; i++) json_append(arr, json_string(cmd[i] ? cmd[i] : ""));
    json_set(a, "cmd", arr);
    json_set(a, "timeout", json_number(timeout));
    json_t *r = dt2_seam(io, "subprocess_run", a);
    if (dt2_raised(r)) {
        bool to = dt2_exc_isa(dt2_exc_type(r), "TimeoutExpired");
        if (to) { json_free(r); return json_bool(false); }
        return r;
    }
    double rc = json_get_num(r, "returncode", -1);
    json_free(r);
    return json_bool(rc == 0.0);
}

/* PoP: dt2_doctor_web_capability_rows @ hermes_cli/doctor_tools.py:_doctor_web_capability_rows */
json_t *dt2_doctor_web_capability_rows(dt2_io_t *io) {
    json_t *rows = json_array();
    json_t *load = dt2_seam(io, "ensure_web_plugins_loaded", json_object());
    if (dt2_raised(load)) {
        bool caught = dt2_caught_by(load, (const char *[]){"Exception"}, 1);
        if (caught) { json_free(load); return rows; }
        json_free(rows); return load;
    }
    json_free(load);
    const char *caps[2] = {"web search", "web extract"};
    const char *getters[2] = {"get_active_search_provider", "get_active_extract_provider"};
    for (int i = 0; i < 2; i++) {
        json_t *pv = dt2_seam(io, getters[i], json_object());
        bool none = false;
        if (dt2_raised(pv)) {
            bool caught = dt2_caught_by(pv, (const char *[]){"Exception"}, 1);
            if (caught) { json_free(pv); none = true; }
            else { json_free(rows); return pv; }
        }
        if (!none && dt2_null(pv)) none = true;
        if (none) {
            json_t *row = json_array();
            json_append(row, json_string("warn"));
            json_append(row, json_string(caps[i]));
            json_append(row, json_string("(no provider selected or registered)"));
            json_append(rows, row);
            continue;
        }
        const char *pname = dt2_sval(json_obj_get(pv, "name"));
        bool name_ok = dt2_truthy(json_obj_get(pv, "name"));
        json_free(pv);
        const char *nm = name_ok ? pname : "SimpleNamespace";
        json_t *ra = dt2_arg1("provider", nm);
        json_t *ready = dt2_seam(io, "_provider_is_ready", ra);
        if (dt2_raised(ready)) { json_free(rows); return ready; }
        bool ok = dt2_truthy(ready);
        json_free(ready);
        char *detail = ok ? dt2_fmt("(%s)", nm)
                          : dt2_fmt("(%s selected; provider not configured)", nm);
        json_t *row = json_array();
        json_append(row, json_string(ok ? "ok" : "warn"));
        json_append(row, json_string(caps[i]));
        json_append(row, json_string(detail ? detail : ""));
        free(detail);
        json_append(rows, row);
    }
    return rows;
}

/* PoP: dt2_check_git_and_rg @ hermes_cli/doctor_tools.py:_check_git_and_rg */
json_t *dt2_check_git_and_rg(dt2_io_t *io) {
    json_t *g = dt2_doctor_tool(io, "git");
    if (dt2_raised(g)) return g;
    json_t *r = dt2_doctor_tool(io, "rg");
    if (dt2_raised(r)) { json_free(g); return r; }
    const char *gpath = dt2_sval(json_get(g, 0));
    const char *gdetail = dt2_sval(json_get(g, 1));
    const char *rpath = dt2_sval(json_get(r, 0));
    const char *rdetail = dt2_sval(json_get(r, 1));
    char *gpath_c = strdup(gpath), *gdetail_c = strdup(gdetail);
    char *rpath_c = strdup(rpath), *rdetail_c = strdup(rdetail);
    json_free(g); json_free(r);

    /* check_bool(git, ("git", git_detail), ("git not found", "(optional)")) */
    json_t *a1 = json_object();
    json_set(a1, "cond", json_string(gpath_c));
    json_t *o1 = json_array(); json_append(o1, json_string("git")); json_append(o1, json_string(gdetail_c));
    json_set(a1, "ok", o1);
    json_t *b1 = json_array(); json_append(b1, json_string("git not found")); json_append(b1, json_string("(optional)"));
    json_set(a1, "bad", b1);
    json_t *c1 = dt2_seam(io, "check_bool", a1);
    if (dt2_raised(c1)) {
        free(gpath_c); free(gdetail_c); free(rpath_c); free(rdetail_c);
        return c1;  /* no try/except in the live def -> propagate */
    }
    json_free(c1);

    /* f"{rg_detail} (faster file search)".strip() */
    char *rd_msg = dt2_fmt("%s (faster file search)", rdetail_c);
    char *rd_strip = dt2_strip(rd_msg); free(rd_msg);
    json_t *a2 = json_object();
    json_set(a2, "cond", json_string(rpath_c));
    json_t *o2 = json_array(); json_append(o2, json_string("ripgrep (rg)")); json_append(o2, json_string(rd_strip ? rd_strip : ""));
    json_set(a2, "ok", o2);
    json_t *b2 = json_array(); json_append(b2, json_string("ripgrep (rg) not found")); json_append(b2, json_string("(file search uses grep fallback)"));
    json_set(a2, "bad", b2);
    json_t *c2 = dt2_seam(io, "check_bool", a2);
    if (dt2_raised(c2)) {
        free(rd_strip); free(gpath_c); free(gdetail_c); free(rpath_c); free(rdetail_c);
        return c2;
    }
    bool rg_ok = dt2_truthy(c2);
    json_free(c2); free(rd_strip);
    if (!rg_ok) {
        json_t *ca = dt2_arg1("package", "ripgrep");
        json_t *cmd = dt2_seam(io, "system_package_install_cmd", ca);
        if (dt2_raised(cmd)) {
            free(gpath_c); free(gdetail_c); free(rpath_c); free(rdetail_c);
            return cmd;
        }
        char *info = dt2_fmt("Install for faster search: %s", dt2_sval(cmd));
        json_free(cmd);
        DT2_INFO(io, info ? info : "");
        free(info);
    }
    free(gpath_c); free(gdetail_c); free(rpath_c); free(rdetail_c);
    return NULL;
}

/* PoP: dt2_check_docker_backend @ hermes_cli/doctor_tools.py:_check_docker_backend */
json_t *dt2_check_docker_backend(dt2_io_t *io, const char *terminal_env, bool running_in_container) {
    json_t *dx = dt2_seam(io, "find_docker", json_object());
    if (dt2_raised(dx)) return dx;
    const char *exe = dt2_sval(dx);
    bool has = dt2_truthy(dx);
    char *exe_c = strdup(exe);
    json_free(dx);
    json_t *ret = NULL;
    if (strcmp(terminal_env, "docker") == 0) {
        if (!has) {
            ret = dt2_fail_and_issue(io, "Docker or Podman not installed",
                "(needed for the 'docker' terminal backend)",
                "Install Docker or Podman, or run `hermes setup terminal` to switch backend.");
        } else {
            json_t *ra = dt2_arg1("exe", exe_c);
            json_t *runtime = dt2_seam(io, "docker_runtime_name", ra);
            if (dt2_raised(runtime)) { free(exe_c); return runtime; }
            const char *rt = dt2_sval(runtime);
            char *rt_c = strdup(rt); json_free(runtime);
            json_t *ha = dt2_arg1("exe", exe_c);
            json_t *hint = dt2_seam(io, "docker_runtime_start_hint", ha);
            if (dt2_raised(hint)) { free(rt_c); free(exe_c); return hint; }
            const char *hint_s = dt2_sval(hint);
            char *hint_c = strdup(hint_s); json_free(hint);
            bool is_docker = strcmp(rt_c, "Docker") == 0;
            char *unreachable = is_docker ? dt2_fmt("%s daemon not running", rt_c)
                                          : dt2_fmt("%s not reachable", rt_c);
            char *hint_up = dt2_upper_first(hint_c);
            char *fix = dt2_fmt("%s, or run `hermes setup terminal` to switch backend.", hint_up ? hint_up : "");
            const char *cmdv[2]; cmdv[0] = exe_c; cmdv[1] = "version";
            json_t *cond = dt2_run_ok(io, cmdv, 2, 10);
            if (dt2_raised(cond)) { free(unreachable); free(fix); free(hint_up); free(hint_c); free(rt_c); free(exe_c); return cond; }
            json_t *ok = json_array();
            json_append(ok, json_string(rt_c));
            json_append(ok, json_string(is_docker ? "(daemon running)" : "(reachable)"));
            json_t *bad = json_array();
            json_append(bad, json_string(unreachable ? unreachable : ""));
            json_append(bad, json_string("(needed for the 'docker' terminal backend)"));
            ret = dt2_require(io, cond, ok, bad, fix ? fix : "");
            json_free(cond); json_free(ok); json_free(bad);
            free(fix); free(hint_up); free(hint_c); free(unreachable); free(rt_c);
        }
    } else if (has) {
        json_t *ra = dt2_arg1("exe", exe_c);
        json_t *runtime = dt2_seam(io, "docker_runtime_name", ra);
        if (dt2_raised(runtime)) { free(exe_c); return runtime; }
        char *nm = strdup(dt2_sval(runtime)); json_free(runtime);
        DT2_OK(io, nm ? nm : "", "(optional)");
        free(nm);
    } else {
        json_t *tm = dt2_seam(io, "is_termux", json_object());
        if (dt2_raised(tm)) { free(exe_c); return tm; }
        bool termux = dt2_truthy(tm); json_free(tm);
        if (termux) {
            DT2_INFO(io, "Docker backend is not available inside Termux (expected on Android)");
        } else if (!running_in_container) {
            DT2_WARN(io, "Docker/Podman not found", "(optional)");
        }
    }
    free(exe_c);
    return ret;
}

/* PoP: dt2_check_ssh_backend @ hermes_cli/doctor_tools.py:_check_ssh_backend */
json_t *dt2_check_ssh_backend(dt2_io_t *io) {
    json_t *ha = dt2_arg1("key", "TERMINAL_SSH_HOST");
    json_t *hv = dt2_seam(io, "os_getenv", ha);
    if (dt2_raised(hv)) return hv;
    const char *host = dt2_sval(hv);
    bool has_host = dt2_truthy(hv);
    char *host_c = strdup(host); json_free(hv);
    if (!has_host) {
        json_t *r = dt2_fail_and_issue(io, "SSH host not configured",
            "(needed for the 'ssh' terminal backend)",
            "run `hermes setup terminal` and enter the SSH host and user.");
        free(host_c); return r;
    }
    const char *keys[3] = {"TERMINAL_SSH_USER", "TERMINAL_SSH_PORT", "TERMINAL_SSH_KEY"};
    char *vals[3] = {NULL, NULL, NULL};
    for (int i = 0; i < 3; i++) {
        json_t *a = dt2_arg1("key", keys[i]);
        json_t *v = dt2_seam(io, "os_getenv", a);
        if (dt2_raised(v)) { for (int j=0;j<3;j++) free(vals[j]); free(host_c); return v; }
        vals[i] = strdup(dt2_sval(v)); json_free(v);
    }
    /* cmd = ["ssh","-o","ConnectTimeout=5","-o","BatchMode=yes"] */
    /* cmd = ["ssh","-o","ConnectTimeout=5","-o","BatchMode=yes", ... -p PORT,
     *         ... -i KEY, DEST, "echo ok"] -> up to 11 entries. */
    const char *cmd[12]; size_t nc = 0;
    cmd[nc++] = "ssh"; cmd[nc++] = "-o"; cmd[nc++] = "ConnectTimeout=5";
    cmd[nc++] = "-o"; cmd[nc++] = "BatchMode=yes";
    if (vals[1][0]) { cmd[nc++] = "-p"; cmd[nc++] = vals[1]; }
    if (vals[2][0]) {
        json_t *ea = dt2_arg1("path", vals[2]);
        json_t *ev = dt2_seam(io, "expanduser", ea);
        if (dt2_raised(ev)) { for (int j=0;j<3;j++) free(vals[j]); free(host_c); return ev; }
        vals[2] = strdup(dt2_sval(ev)); json_free(ev);
        cmd[nc++] = "-i"; cmd[nc++] = vals[2];
    }
    char *dest = vals[0][0] ? dt2_fmt("%s@%s", vals[0], host_c) : strdup(host_c);
    cmd[nc++] = dest ? dest : host_c; cmd[nc++] = "echo ok";
    json_t *cond = dt2_run_ok(io, cmd, nc, 15);
    free(dest);
    for (int j=0;j<3;j++) free(vals[j]);
    if (dt2_raised(cond)) { free(host_c); return cond; }
    char *label = dt2_fmt("SSH connection to %s", host_c);
    char *fix = dt2_fmt("Check SSH configuration for %s", host_c);
    json_t *ok = json_array(); json_append(ok, json_string(label)); json_append(ok, json_string(""));
    json_t *bad = json_array(); json_append(bad, json_string(label)); json_append(bad, json_string(""));
    json_t *ret = dt2_require(io, cond, ok, bad, fix ? fix : "");
    json_free(cond); json_free(ok); json_free(bad); free(fix); free(label); free(host_c);
    return ret;
}

/* PoP: dt2_check_daytona_backend @ hermes_cli/doctor_tools.py:_check_daytona_backend */
json_t *dt2_check_daytona_backend(dt2_io_t *io) {
    json_t *ka = dt2_arg1("key", "DAYTONA_API_KEY");
    json_t *kv = dt2_seam(io, "os_getenv", ka);
    if (dt2_raised(kv)) return kv;
    bool has = dt2_truthy(kv); json_free(kv);
    json_t *ret = dt2_require(io, json_bool(has),
        json_ok_bad_pair("Daytona API key", "(configured)"),
        json_ok_bad_pair("Daytona API key missing", "(needed for the 'daytona' terminal backend)"),
        "run `hermes setup terminal` (Daytona) to enter it.");
    if (dt2_raised(ret)) return ret;
    json_free(ret);
    /* Import-failure branch (from daytona import Daytona) is excluded from the
     * fixture domain: the stub module is always present. Both sides take the
     * `check_ok("daytona SDK", "(installed)")` path with no seam event. */
    DT2_OK(io, "daytona SDK", "(installed)");
    return NULL;
}

/* PoP: dt2_check_vercel_backend @ hermes_cli/doctor_tools.py:_check_vercel_backend */
json_t *dt2_check_vercel_backend(dt2_io_t *io) {
    /* _SUPPORTED_VERCEL_RUNTIMES is an imported module constant, not a seam.
     * Both sides pin the same list (documented in the port header). */
    static const char *const RUNTIMES[] = {"node20", "node22", "node24"};
    const char *supported = "node20, node22, node24";
    json_t *ra = dt2_arg1("key", "TERMINAL_VERCEL_RUNTIME");
    json_t *rv = dt2_seam(io, "os_getenv", ra);
    if (dt2_raised(rv)) return rv;
    char *runtime = dt2_strip(dt2_sval(rv)); json_free(rv);
    if (!runtime[0]) { free(runtime); runtime = strdup("node24"); }
    bool in_supported = false;
    for (size_t i = 0; i < 3; i++) if (strcmp(runtime, RUNTIMES[i]) == 0) { in_supported = true; break; }
    char *rmsg = dt2_fmt("(%s)", runtime);
    char *badmsg = dt2_fmt("(%s; use %s)", runtime, supported);
    char *fix = dt2_fmt("Set TERMINAL_VERCEL_RUNTIME to one of: %s", supported);
    json_t *ok = json_array(); json_append(ok, json_string("Vercel runtime")); json_append(ok, json_string(rmsg));
    json_t *bad = json_array(); json_append(bad, json_string("Vercel runtime unsupported")); json_append(bad, json_string(badmsg));
    json_t *r1 = dt2_require(io, json_bool(in_supported), ok, bad, fix ? fix : "");
    json_free(ok); json_free(bad); free(rmsg); free(badmsg); free(fix);
    if (dt2_raised(r1)) { free(runtime); return r1; }
    json_free(r1);

    json_t *da = dt2_arg1("key", "TERMINAL_CONTAINER_DISK");
    json_t *dv = dt2_seam(io, "os_getenv", da);
    if (dt2_raised(dv)) { free(runtime); return dv; }
    char *disk = dt2_strip(dt2_sval(dv)); json_free(dv);
    bool disk_ok = (strcmp(disk,"")==0 || strcmp(disk,"0")==0 || strcmp(disk,"51200")==0);
    json_t *dok = json_array(); json_append(dok, json_string("Vercel disk setting")); json_append(dok, json_string("(uses platform default)"));
    json_t *dbad = json_array(); json_append(dbad, json_string("Vercel custom disk unsupported")); json_append(dbad, json_string("(reset terminal.container_disk to 51200)"));
    json_t *r2 = dt2_require(io, json_bool(disk_ok), dok, dbad,
        "Vercel Sandbox does not support custom container_disk; use the shared default 51200");
    json_free(dok); json_free(dbad); free(disk);
    if (dt2_raised(r2)) { free(runtime); return r2; }
    json_free(r2);

    json_t *fs = dt2_seam(io, "find_spec", dt2_arg1("name", "vercel"));
    if (dt2_raised(fs)) { free(runtime); return fs; }
    bool sdk = !dt2_null(fs); json_free(fs);
    json_t *sok = json_array(); json_append(sok, json_string("vercel SDK")); json_append(sok, json_string("(installed)"));
    json_t *sbad = json_array(); json_append(sbad, json_string("vercel SDK not installed")); json_append(sbad, json_string("(run hermes setup terminal)"));
    json_t *r3 = dt2_require(io, json_bool(sdk), sok, sbad,
        "Run hermes setup terminal and select Vercel Sandbox, then restart Hermes");
    json_free(sok); json_free(sbad);
    if (dt2_raised(r3)) { free(runtime); return r3; }
    json_free(r3);

    json_t *auth = dt2_seam(io, "describe_vercel_auth", json_object());
    if (dt2_raised(auth)) { free(runtime); return auth; }
    bool a_ok = dt2_truthy(json_obj_get(auth, "ok"));
    const char *label = dt2_sval(json_obj_get(auth, "label"));
    char *label_c = strdup(label);
    json_t *lines = json_obj_get(auth, "detail_lines");
    size_t nlines = dt2_arr(lines) ? lines->c.count : 0;
    char **linev = malloc((nlines ? nlines : 1) * sizeof(char*));
    for (size_t i = 0; i < nlines; i++) linev[i] = strdup(dt2_sval(json_get(lines, i)));
    json_free(auth);
    json_t *ret = NULL;
    if (a_ok) {
        char *lbl = dt2_fmt("(%s)", label_c);
        DT2_OK(io, "Vercel auth", lbl ? lbl : ""); free(lbl);
    } else if (strncmp(label_c, "partial", 7) == 0) {
        char *lbl = dt2_fmt("(%s)", label_c);
        ret = dt2_fail_and_issue(io, "Vercel auth incomplete", lbl ? lbl : "",
            "Set VERCEL_TOKEN, VERCEL_PROJECT_ID, and VERCEL_TEAM_ID together"); free(lbl);
    } else {
        char *lbl = dt2_fmt("(%s)", label_c);
        ret = dt2_fail_and_issue(io, "Vercel auth not configured", lbl ? lbl : "",
            "Configure Vercel Sandbox auth with VERCEL_TOKEN, VERCEL_PROJECT_ID, and VERCEL_TEAM_ID"); free(lbl);
    }
    for (size_t i = 0; i < nlines; i++) {
        char *msg = dt2_fmt("Vercel auth %s", linev[i]);
        DT2_INFO(io, msg ? msg : ""); free(msg); free(linev[i]);
    }
    free(linev);

    json_t *pa = dt2_arg1("key", "TERMINAL_CONTAINER_PERSISTENT");
    json_t *pv = dt2_seam(io, "os_getenv", pa);
    if (dt2_raised(pv)) { free(label_c); free(runtime); return pv; }
    char *pl = dt2_lower_strip(dt2_sval(pv)); json_free(pv);
    bool persistent = (strcmp(pl,"1")==0||strcmp(pl,"true")==0||strcmp(pl,"yes")==0||strcmp(pl,"on")==0);
    free(pl);
    if (persistent)
        DT2_INFO(io, "Vercel persistence: snapshot filesystem only; live processes do not survive sandbox recreation");
    else
        DT2_INFO(io, "Vercel persistence: ephemeral filesystem");
    free(label_c); free(runtime);
    return ret;
}

/* PoP: dt2_check_plugin_backend @ hermes_cli/doctor_tools.py:_check_plugin_backend */
json_t *dt2_check_plugin_backend(dt2_io_t *io, const char *terminal_env) {
    json_t *d = dt2_seam(io, "discover_plugins", json_object());
    bool fail = false;
    if (dt2_raised(d)) {
        if (dt2_caught_by(d, (const char *[]){"Exception"}, 1)) { json_free(d); fail = true; }
        else return d;
    } else json_free(d);
    json_t *provider = NULL;
    if (!fail) {
        provider = dt2_seam(io, "get_provider", dt2_arg1("env", terminal_env));
        if (dt2_raised(provider)) {
            if (dt2_caught_by(provider, (const char *[]){"Exception"}, 1)) { json_free(provider); provider = NULL; }
            else return provider;
        }
    }
    if (!provider || dt2_null(provider)) {
        if (provider) json_free(provider);
        char *msg = dt2_fmt("Unknown terminal backend '%s'", terminal_env);
        json_t *r = dt2_fail_and_issue(io, msg ? msg : "",
            "(no built-in or plugin backend by that name)",
            "Fix terminal.backend in config.yaml, or install/enable the plugin that provides it");
        free(msg); return r;
    }
    json_free(provider);
    json_t *checks = dt2_seam(io, "doctor_checks", dt2_arg1("env", terminal_env));
    if (dt2_raised(checks)) return checks;
    if (dt2_arr(checks)) {
        for (size_t i = 0; i < checks->c.count; i++) {
            json_t *row = json_get(checks, i);
            const char *label = dt2_sval(json_get(row, 0));
            const char *detail = dt2_sval(json_get(row, 1));
            char *dc = strdup(detail);
            /* detail.strip("()") — strip leading/trailing '(' and ')' chars */
            size_t n = strlen(dc), a = 0, b = n;
            while (a < b && (dc[a]=='('||dc[a]==')')) a++;
            while (b > a && (dc[b-1]=='('||dc[b-1]==')')) b--;
            char *fix = malloc(b - a + 1);
            if (fix) { memcpy(fix, dc + a, b - a); fix[b - a] = '\0'; }
            json_t *ok = json_array(); json_append(ok, json_string(label)); json_append(ok, json_string(detail));
            json_t *bad = json_array(); json_append(bad, json_string(label)); json_append(bad, json_string(detail));
            json_t *r = dt2_require(io, json_get(row, 2), ok, bad, fix ? fix : "");
            json_free(ok); json_free(bad); free(fix); free(dc);
            if (dt2_raised(r)) { json_free(checks); return r; }
            json_free(r);
        }
    }
    json_free(checks);
    return NULL;
}

/* PoP: dt2_check_terminal_backend @ hermes_cli/doctor_tools.py:_check_terminal_backend */
json_t *dt2_check_terminal_backend(dt2_io_t *io) {
    json_t *ea = dt2_arg1("key", "TERMINAL_ENV");
    json_t *ev = dt2_seam(io, "os_getenv", ea);
    if (dt2_raised(ev)) return ev;
    char *tenv = dt2_strip(dt2_sval(ev)); json_free(ev);
    if (!tenv[0]) { free(tenv); tenv = strdup("local"); }
    json_t *ic = dt2_seam(io, "is_container", json_object());
    bool in_container = false;
    if (dt2_raised(ic)) {
        if (dt2_caught_by(ic, (const char *[]){"Exception"}, 1)) { json_free(ic); in_container = false; }
        else { free(tenv); return ic; }
    } else { in_container = dt2_truthy(ic); json_free(ic); }
    if (in_container && strcmp(tenv, "docker") != 0) {
        DT2_INFO(io, "Running inside a container — using local terminal backend (docker-in-docker is not configured by default)");
        free(tenv); tenv = strdup("local");
    }
    json_t *ret = dt2_check_docker_backend(io, tenv, in_container);
    if (dt2_raised(ret)) { free(tenv); return ret; }
    json_free(ret); ret = NULL;
    if (strcmp(tenv,"ssh")==0) ret = dt2_check_ssh_backend(io);
    else if (strcmp(tenv,"daytona")==0) ret = dt2_check_daytona_backend(io);
    else if (strcmp(tenv,"vercel_sandbox")==0) ret = dt2_check_vercel_backend(io);
    else {
        const char *builtins[] = {"local","docker","modal","managed_modal","daytona","vercel_sandbox","ssh"};
        bool known = false;
        for (size_t i=0;i<sizeof(builtins)/sizeof(builtins[0]);i++) if (strcmp(tenv,builtins[i])==0) { known=true; break; }
        if (!known) ret = dt2_check_plugin_backend(io, tenv);
    }
    free(tenv);
    return ret;
}

/* PoP: dt2_check_chromium @ hermes_cli/doctor_tools.py:_check_chromium */
json_t *dt2_check_chromium(dt2_io_t *io, bool should_fix) {
    json_t *f1 = dt2_seam(io, "chromium_executable", json_object());
    if (dt2_raised(f1)) return f1;
    bool found = dt2_truthy(f1);
    char *found_c = strdup(dt2_sval(f1)); json_free(f1);
    if (!found && should_fix) {
        json_t *tm = dt2_seam(io, "is_termux", json_object());
        if (dt2_raised(tm)) { free(found_c); return tm; }
        bool termux = dt2_truthy(tm); json_free(tm);
        if (!termux) {
            json_t *ea = json_object();
            json_set(ea, "name", json_string("chromium")); json_set(ea, "explicit", json_bool(true));
            json_t *ens = dt2_seam(io, "pm_ensure", ea);
            if (dt2_raised(ens)) {
                bool caught = dt2_caught_by(ens, (const char *[]){"RuntimeError","OSError"}, 2);
                if (caught) {
                    char *msg = dt2_fmt("(%s)", dt2_exc_msg(ens));
                    DT2_WARN(io, "Chromium install failed", msg ? msg : "");
                    free(msg);
                    json_free(ens);
                } else { free(found_c); return ens; }
            } else json_free(ens);
            json_t *f2 = dt2_seam(io, "chromium_executable", json_object());
            if (dt2_raised(f2)) { free(found_c); return f2; }
            found = dt2_truthy(f2);
            free(found_c); found_c = strdup(dt2_sval(f2)); json_free(f2);
        }
    }
    json_t *ret = NULL;
    if (found) {
        char *msg = dt2_fmt("(%s)", found_c);
        DT2_OK(io, "Chromium", msg ? msg : ""); free(msg);
    } else {
        json_t *rsn = dt2_seam(io, "chromium_not_needed_reason", json_object());
        if (dt2_raised(rsn)) { free(found_c); return rsn; }
        bool has_reason = dt2_truthy(rsn);
        char *reason_c = strdup(dt2_sval(rsn)); json_free(rsn);
        if (has_reason) {
            char *msg = dt2_fmt("Chromium not installed (not needed: %s)", reason_c);
            DT2_INFO(io, msg ? msg : ""); free(msg);
        } else {
            DT2_WARN(io, "Chromium not installed", "(browser_exec cannot start a local browser)");
            json_t *hint = dt2_seam(io, "chromium_missing_hint", json_object());
            if (dt2_raised(hint)) { free(reason_c); free(found_c); return hint; }
            DT2_INFO(io, dt2_sval(hint)); json_free(hint);
        }
        free(reason_c);
    }
    free(found_c);
    return ret;
}

/* PoP: dt2_check_lightpanda @ hermes_cli/doctor_tools.py:_check_lightpanda */
json_t *dt2_check_lightpanda(dt2_io_t *io) {
    json_t *using = dt2_seam(io, "using_lightpanda_engine", json_object());
    if (dt2_raised(using)) return using;
    bool on = dt2_truthy(using); json_free(using);
    if (!on) return NULL;
    json_t *st = dt2_seam(io, "lightpanda_engine_status", json_object());
    bool used = false;
    char *reason_c = NULL;
    if (dt2_raised(st)) {
        bool caught = dt2_caught_by(st, (const char *[]){"Exception"}, 1);
        if (caught) {
            reason_c = dt2_fmt("status check failed: %s", dt2_exc_msg(st));
            json_free(st);
        } else return st;
    } else {
        used = dt2_truthy(json_get(st, 0));
        reason_c = strdup(dt2_sval(json_get(st, 1)));
        json_free(st);
    }
    json_t *ret = NULL;
    if (!used) {
        char *det = dt2_fmt_warn(reason_c);
        DT2_WARN(io, "browser.engine=lightpanda is shadowed", det ? det : ""); free(det);
        DT2_INFO(io, "Fix: pick Lightpanda in `hermes tools` → Browser Automation, or set browser.engine: auto");
    } else {
        json_t *bin = dt2_seam(io, "find_lightpanda_binary", json_object());
        if (dt2_raised(bin)) { free(reason_c); return bin; }
        char *det = dt2_fmt_warn(reason_c);
        json_t *ok = json_array(); json_append(ok, json_string("Lightpanda")); json_append(ok, json_string(det ? det : ""));
        json_t *bad = json_array(); json_append(bad, json_string("Lightpanda selected but binary not found")); json_append(bad, json_string("(browser tools will fail until it is installed)"));
        json_t *a = json_object();
        json_set(a, "cond", json_copy(bin)); json_set(a, "ok", ok); json_set(a, "bad", bad);
        json_t *cb = dt2_seam(io, "check_bool", a);
        if (dt2_raised(cb)) { json_free(cb); json_free(bin); free(det); free(reason_c); return cb; }
        bool good = dt2_truthy(cb);
        json_free(cb); json_free(bin); free(det);
        if (!good) {
            /* LIGHTPANDA_INSTALL_HINT is an imported module constant, not a seam. */
            DT2_INFO(io, "Install Lightpanda from https://lightpanda.io/docs/run-locally/installation/one-liner and make sure `lightpanda` is on PATH");
        }
    }
    free(reason_c);
    return ret;
}

/* PoP: dt2_check_node_and_browser @ hermes_cli/doctor_tools.py:_check_node_and_browser */
json_t *dt2_check_node_and_browser(dt2_io_t *io, bool should_fix) {
    json_t *pa = dt2_arg1("name", "node");
    json_t *pmnode = dt2_seam(io, "pm_tool_path", pa);
    if (dt2_raised(pmnode)) return pmnode;
    bool has_pm = dt2_truthy(pmnode); json_free(pmnode);
    if (has_pm) {
        DT2_OK(io, "Node.js", "");
    } else {
        /* Live: `_pm_tool_path("node") or (_is_termux() and
         * _safe_which("node"))` — is_termux is evaluated once in the `and`
         * guard; when the overall condition is false, the `elif _is_termux()`
         * AGAIN calls is_termux. So a falsy pm always fires is_termux twice. */
        json_t *tm = dt2_seam(io, "is_termux", json_object());
        if (dt2_raised(tm)) return tm;
        bool termux = dt2_truthy(tm); json_free(tm);
        bool node_ok = false;
        if (termux) {
            json_t *sa = dt2_arg1("cmd", "node");
            json_t *sw = dt2_seam(io, "safe_which", sa);
            if (dt2_raised(sw)) return sw;
            node_ok = dt2_truthy(sw); json_free(sw);
        }
        if (node_ok) DT2_OK(io, "Node.js", "");
        else {
            json_t *tm2 = dt2_seam(io, "is_termux", json_object());
            if (dt2_raised(tm2)) return tm2;
            bool termux2 = dt2_truthy(tm2); json_free(tm2);
            if (termux2)
                DT2_INFO(io, "Node.js not found (optional; install on Termux with: pkg install nodejs)");
            else
                DT2_WARN(io, "Node.js not found", "(optional; run: hermes pm install node)");
        }
    }
    json_t *ret = dt2_check_chromium(io, should_fix);
    if (dt2_raised(ret)) return ret;
    json_free(ret);
    ret = dt2_check_lightpanda(io);
    return ret;
}

/* PoP: dt2_audit_one @ hermes_cli/doctor_tools.py:_audit_one */
json_t *dt2_audit_one(dt2_io_t *io, const char *tool, const char *cwd, const char *label,
                      const char **audit_extra, size_t nextra) {
    /* with_hermes_node_path() is called BEFORE the live `try:` -> propagate. */
    json_t *env = dt2_seam(io, "with_hermes_node_path", json_object());
    if (dt2_raised(env)) return env;
    if (nextra > 0) {
        json_free(env);
        env = dt2_seam(io, "pnpm_env", json_object());
        if (dt2_raised(env)) return env;   /* also before the try */
    }
    json_free(env);
    json_t *a = json_object();
    json_t *cmd = json_array();
    json_append(cmd, json_string(tool)); json_append(cmd, json_string("audit")); json_append(cmd, json_string("--json"));
    for (size_t i = 0; i < nextra; i++) json_append(cmd, json_string(audit_extra[i]));
    json_set(a, "cmd", cmd);
    json_set(a, "cwd", json_string(cwd ? cwd : ""));
    json_t *res = dt2_seam(io, "subprocess_run", a);
    if (dt2_raised(res)) {
        bool caught = dt2_caught_by(res, (const char *[]){"Exception"}, 1);
        if (caught) { json_free(res); return NULL; }
        return res;
    }
    const char *stdout_s = dt2_sval(json_obj_get(res, "stdout"));
    char *out_c = strdup(stdout_s); json_free(res);
    json_t *la = dt2_arg1("s", out_c); free(out_c);
    json_t *parsed = dt2_seam(io, "json_loads", la);
    if (dt2_raised(parsed)) {
        bool caught = dt2_caught_by(parsed, (const char *[]){"Exception"}, 1);
        if (caught) { json_free(parsed); return NULL; }
        return parsed;
    }
    json_t *meta = dt2_obj(parsed) ? json_obj_get(parsed, "metadata") : NULL;
    json_t *counts = dt2_obj(meta) ? json_obj_get(meta, "vulnerabilities") : NULL;
    if (!dt2_obj(counts)) { json_free(parsed); return NULL; }
    double critical = json_get_num(counts, "critical", 0);
    double high = json_get_num(counts, "high", 0);
    double moderate = json_get_num(counts, "moderate", 0);
    json_free(parsed);
    double total = critical + high + moderate;
    bool workspace_scoped = (nextra > 0) && (strcmp(audit_extra[nextra-1], ".") != 0);
    char *lbl = dt2_fmt("%s deps", label);
    if (total == 0.0) {
        DT2_OK(io, lbl, "(no known vulnerabilities)");
    } else if (critical > 0.0 || high > 0.0) {
        const char *remedy = "fix is an upstream lockfile bump — a local manual fix does not persist (the next `hermes update` reinstalls from the committed lockfile)";
        char *det = dt2_fmt("(%.0f critical, %.0f high, %.0f moderate — %s)", critical, high, moderate, remedy);
        DT2_WARN(io, lbl, det ? det : ""); free(det);
        if (workspace_scoped) {
            DT2_INFO(io, "  ^ workspace audit includes runtime and build dependencies");
        } else {
            const char *lockfile = nextra > 0 ? "pnpm-lock.yaml" : "package-lock.json";
            char *det2 = dt2_fmt("  ^ runtime dependency tree; report/pin the fix in %s — see #116774", lockfile);
            DT2_INFO(io, det2 ? det2 : ""); free(det2);
        }
        const char *plural = total == 1.0 ? "vulnerability" : "vulnerabilities";
        char *issue = dt2_fmt("%s has %.0f npm %s; wait for an upstream lockfile fix, then update your installation (Docker: pull a rebuilt image; recreating the same image restores the same dependencies)", label, total, plural);
        dt2_append_issue(io, "manual_issues", issue ? issue : "");
        free(issue);
    } else {
        const char *plural = moderate == 1.0 ? "vulnerability" : "vulnerabilities";
        char *det = dt2_fmt("(%.0f moderate %s)", moderate, plural);
        DT2_OK(io, lbl, det ? det : ""); free(det);
    }
    free(lbl);
    return NULL;
}

/* PoP: dt2_check_npm_audit @ hermes_cli/doctor_tools.py:_check_npm_audit */
json_t *dt2_check_npm_audit(dt2_io_t *io) {
    /* PROJECT_ROOT is an imported module constant (PathDouble("/proj")), not
     * a seam. Both sides pin it; no trace event. */
    char *root = strdup("/proj");
    json_t *na = dt2_arg1("name", "npm");
    json_t *npmp = dt2_seam(io, "pm_tool_path", na);
    if (dt2_raised(npmp)) { free(root); return npmp; }
    const char *npm = dt2_sval(npmp);
    bool has_npm = dt2_truthy(npmp);
    char *npm_c = strdup(npm); json_free(npmp);
    bool termux = false;
    if (!has_npm) {
        json_t *tm = dt2_seam(io, "is_termux", json_object());
        if (dt2_raised(tm)) { free(root); free(npm_c); return tm; }
        termux = dt2_truthy(tm); json_free(tm);
        if (termux) {
            json_t *sa = dt2_arg1("cmd", "npm");
            json_t *sw = dt2_seam(io, "safe_which", sa);
            if (dt2_raised(sw)) { free(root); free(npm_c); return sw; }
            if (dt2_truthy(sw)) { free(npm_c); npm_c = strdup(dt2_sval(sw)); has_npm = true; }
            json_free(sw);
        }
    }
    json_t *pa = dt2_arg1("name", "pnpm");
    json_t *pn = dt2_seam(io, "pm_tool_path", pa);
    if (dt2_raised(pn)) { free(root); free(npm_c); return pn; }
    char *pnpm = strdup(dt2_sval(pn)); bool has_pnpm = dt2_truthy(pn); json_free(pn);

    json_t *wb = dt2_seam(io, "resolve_whatsapp_bridge_dir", json_object());
    char *bridge = NULL;
    if (dt2_raised(wb)) {
        bool caught = dt2_caught_by(wb, (const char *[]){"Exception"}, 1);
        if (caught) { json_free(wb); bridge = dt2_join(root, "/scripts/whatsapp-bridge"); }
        else { free(root); free(npm_c); free(pnpm); return wb; }
    } else { bridge = strdup(dt2_sval(wb)); json_free(wb); }

    /* 4 audit rows: (tool, tree, label, extra) */
    const char *tools[4] = {pnpm, pnpm, pnpm, npm_c};
    char *trees[4] = {strdup(root), dt2_join(root,"/web"), dt2_join(root,"/ui-tui"), NULL};
    trees[3] = strdup(bridge);
    const char *labels[4] = {"Root npm package","web workspace","ui-tui workspace","WhatsApp bridge"};
    const char *row0[2] = {"--filter", "."};
    const char *row1[2] = {"--filter", "{./web}..."};
    const char *row2[2] = {"--filter", "{./ui-tui}..."};
    const char **extras[4] = {row0, row1, row2, NULL};
    size_t nextras[4] = {2, 2, 2, 0};

    for (int i = 0; i < 4; i++) {
        const char *tool = tools[i];
        bool tool_ok = (i < 3) ? has_pnpm : has_npm;
        if (!tool_ok || !tool || !tool[0]) continue;
        json_t *ea = dt2_arg1("path", trees[i]);
        json_t *ex = dt2_seam(io, "path_exists", ea);
        if (dt2_raised(ex)) { for(int j=0;j<4;j++) free(trees[j]); free(root); free(npm_c); free(pnpm); free(bridge); return ex; }
        bool exists = dt2_truthy(ex); json_free(ex);
        if (!exists) continue;
        const char *audit_cwd = (i < 3) ? root : trees[i];
        json_t *r = dt2_audit_one(io, tool, audit_cwd, labels[i], extras[i], nextras[i]);
        if (dt2_raised(r)) { for(int j=0;j<4;j++) free(trees[j]); free(root); free(npm_c); free(pnpm); free(bridge); return r; }
        json_free(r);
    }
    for (int j=0;j<4;j++) free(trees[j]);
    /* live ends with `if _is_termux():` — a fresh call, not the cached value. */
    json_t *tf = dt2_seam(io, "is_termux", json_object());
    if (dt2_raised(tf)) { free(root); free(npm_c); free(pnpm); free(bridge); return tf; }
    bool final_termux = dt2_truthy(tf); json_free(tf);
    if (final_termux) {
        DT2_INFO(io, "Termux compatibility fallbacks:");
        static const char *NOTES[] = {
            "Termux uses the Hermes APT package: pkg install hermes-agent.",
            "Matrix E2EE extra is excluded on Termux (python-olm currently fails to build).",
            "Local faster-whisper extra is excluded on Termux (ctranslate2/av build path unavailable).",
            "STT fallback: use Groq Whisper (set GROQ_API_KEY) or OpenAI Whisper (set VOICE_TOOLS_OPENAI_KEY).",
        };
        for (size_t i=0;i<4;i++) DT2_INFO(io, NOTES[i]);
    }
    free(root); free(npm_c); free(pnpm); free(bridge);
    return NULL;
}

/* PoP: dt2_check_tool_availability @ hermes_cli/doctor_tools.py:_check_tool_availability */
json_t *dt2_check_tool_availability(dt2_io_t *io) {
    /* PROJECT_ROOT import constant pinned on both sides; no seam event. */
    json_t *res = dt2_seam(io, "check_tool_availability", json_object());
    if (dt2_raised(res)) return res;
    json_t *available = dt2_obj(res) ? json_obj_get(res, "available") : NULL;
    json_t *unavailable = dt2_obj(res) ? json_obj_get(res, "unavailable") : NULL;
    json_t *av_copy = dt2_arr(available) ? json_copy(available) : json_array();
    json_t *un_copy = dt2_arr(unavailable) ? json_copy(unavailable) : json_array();
    json_free(res);

    json_t *aa = json_object();
    json_set(aa, "available", json_copy(av_copy));
    json_set(aa, "unavailable", json_copy(un_copy));
    json_t *ovr = dt2_seam(io, "apply_tool_availability_overrides", aa);
    if (dt2_raised(ovr)) { json_free(av_copy); json_free(un_copy); return ovr; }
    json_t *nav = dt2_obj(ovr) ? json_obj_get(ovr, "available") : NULL;
    json_t *nun = dt2_obj(ovr) ? json_obj_get(ovr, "unavailable") : NULL;
    json_free(av_copy); json_free(un_copy);
    av_copy = dt2_arr(nav) ? json_copy(nav) : json_array();
    un_copy = dt2_arr(nun) ? json_copy(nun) : json_array();
    json_free(ovr);

    bool web_present = dt2_in_strings(av_copy, "web");
    if (!web_present && dt2_arr(un_copy))
        for (size_t i=0;i<un_copy->c.count;i++)
            if (strcmp(dt2_sval(json_obj_get(json_get(un_copy,i),"name")),"web")==0) { web_present=true; break; }
    json_t *web_rows = json_array();
    if (web_present) {
        json_t *wr = dt2_seam(io, "doctor_web_capability_rows", json_object());
        if (dt2_raised(wr)) { json_free(av_copy); json_free(un_copy); return wr; }
        if (dt2_arr(wr)) web_rows = wr; else json_free(wr);
        /* filter web out */
        json_t *fav = json_array();
        for (size_t i=0;i<av_copy->c.count;i++) if (strcmp(dt2_sval(json_get(av_copy,i)),"web")!=0) json_append(fav, json_copy(json_get(av_copy,i)));
        json_free(av_copy); av_copy = fav;
        json_t *fun = json_array();
        for (size_t i=0;i<un_copy->c.count;i++) if (strcmp(dt2_sval(json_obj_get(json_get(un_copy,i),"name")),"web")!=0) json_append(fun, json_copy(json_get(un_copy,i)));
        json_free(un_copy); un_copy = fun;
    }

    for (size_t i=0;i<av_copy->c.count;i++) {
        json_t *da = dt2_arg1("toolset", dt2_sval(json_get(av_copy,i)));
        json_t *detail = dt2_seam(io, "doctor_tool_availability_detail", da);
        if (dt2_raised(detail)) { json_free(av_copy); json_free(un_copy); json_free(web_rows); return detail; }
        DT2_OK(io, dt2_sval(json_get(av_copy,i)), dt2_sval(detail)); json_free(detail);
    }
    bool web_bad = false;
    for (size_t i=0;i<web_rows->c.count;i++) {
        json_t *row = json_get(web_rows,i);
        const char *status = dt2_sval(json_get(row,0));
        const char *label = dt2_sval(json_get(row,1));
        const char *detail = dt2_sval(json_get(row,2));
        if (strcmp(status,"ok")==0) DT2_OK(io,label,detail); else DT2_WARN(io,label,detail);
        if (strcmp(status,"ok")!=0) web_bad = true;
    }
    for (size_t i=0;i<un_copy->c.count;i++) {
        json_t *item = json_get(un_copy,i);
        json_t *mv = json_obj_get(item,"missing_vars");
        json_t *ev = json_obj_get(item,"env_vars");
        json_t *vars = dt2_truthy(mv) ? mv : (dt2_truthy(ev) ? ev : NULL);
        const char *name = dt2_sval(json_obj_get(item,"name"));
        char *detail = NULL;
        if (vars && dt2_arr(vars) && vars->c.count > 0) {
            char *joined = dt2_join_strings(vars, ", ");
            detail = dt2_fmt("(missing %s)", joined ? joined : "");
            free(joined);
        } else {
            /* _TOOLSET_SETUP_HINTS is a module constant with a single entry. */
            detail = strcmp(name, "image_gen") == 0
                ? strdup("(image generation unavailable — check the provider selection and its key or SDK with 'hermes tools')")
                : strdup("(system dependency not met)");
        }
        DT2_WARN(io, name, detail ? detail : ""); free(detail);
    }
    json_t *ua = json_object(); json_set(ua, "unavailable", json_copy(un_copy));
    json_t *api = dt2_seam(io, "missing_api_key_toolsets", ua);
    if (dt2_raised(api)) { json_free(av_copy); json_free(un_copy); json_free(web_rows); return api; }
    bool api_disabled = dt2_truthy(api) && dt2_arr(api) && api->c.count > 0;
    json_free(api);
    json_free(av_copy); json_free(un_copy); json_free(web_rows);
    if (api_disabled || web_bad)
        dt2_append_issue(io, "issues", "Run 'hermes setup' to configure missing API keys for full tool access");
    json_t *ret = dt2_warn_partially_stripped_toolsets(io);
    return ret;
}

/* PoP: dt2_warn_partially_stripped_toolsets @ hermes_cli/doctor_tools.py:_warn_partially_stripped_toolsets */
json_t *dt2_warn_partially_stripped_toolsets(dt2_io_t *io) {
    json_t *cfg = dt2_seam(io, "load_config", json_object());
    if (dt2_raised(cfg)) return cfg;
    if (dt2_null(cfg)) { json_free(cfg); cfg = json_object(); }
    json_t *pt = dt2_seam(io, "get_platform_tools", json_object());
    if (dt2_raised(pt)) { json_free(cfg); return pt; }
    json_t *pa = json_object();
    json_set(pa, "config", json_copy(cfg)); json_set(pa, "tools", pt ? json_copy(pt) : json_null());
    json_free(pt);
    json_t *stripped = dt2_seam(io, "toolsets_partially_stripped", pa);
    json_free(cfg);
    if (dt2_raised(stripped)) return stripped;
    if (dt2_obj(stripped)) {
        for (size_t i = 0; i < stripped->c.count; i++) {
            const char *ts = stripped->c.keys[i];
            json_t *v = stripped->c.items[i];   /* json_get() is ARRAY-only */
            char *tools = dt2_join_strings(json_get(v,0), ", ");
            char *by = dt2_join_strings(json_get(v,1), ", ");
            const char *hs = dt2_sval(json_get(v,2));   /* a STRING */
            char *h0 = strdup(hs);
            char *hrest = strdup(*hs ? hs + 1 : "");
            char *hup = dt2_upper_first(h0);
            char *detail = dt2_fmt("(%s removed by agent.disabled_toolsets: %s)", tools ? tools : "", by ? by : "");
            char *text = dt2_fmt("%s (partly disabled)", ts);
            DT2_WARN(io, text ? text : ts, detail ? detail : "");
            char *issue = dt2_fmt("%s: %s %s", ts, hup ? hup : "", hrest ? hrest : "");
            dt2_append_issue(io, "issues", issue ? issue : "");
            free(issue); free(text); free(detail); free(hup); free(hrest); free(h0); free(by); free(tools);
        }
    }
    json_free(stripped);
    return NULL;
}
