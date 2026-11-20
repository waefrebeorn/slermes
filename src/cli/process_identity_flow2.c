/*
 * process_identity_flow2.c — hermes_cli/process_identity.py UNCLAIMED remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/process_identity.py
 *
 * Sibling of src/hermes_cli/process_identity.c, process_identity_gaps3.c and
 * src/gateway/session_kanban_flow3.c (sk3_process_birth / sk3_worker_birth).
 * Every function those PoP sets claim is SEAMED here, never re-ported:
 *   install_id build_spawn_tag spawn_env parse_spawn_tag _ledger_path _read_ledger
 *   _quarantine_ledger _pid_alive_matches register_self ledger_entries spawner_is_dead
 *   register_child reap_orphaned_mcp_helpers process_birth worker_birth
 * This file ports the defs NO existing PoP claims (CRLF-aware grep PoP first):
 *
 *   boot_start                 Linux boot-relative birth via proc._proc.create_time(monotonic=True)
 *   _process_create_time       process_birth(psutil.Process(pid or self)); except Exception -> None
 *   _live_boot_id              /proc/sys/kernel/random/boot_id read_text().strip() or None;
 *                              except OSError -> None; lru_cache(1) modeled as caller-held state
 *   _entry_boot                boot-id-gated entry birth key; _EARLIER_BOOT(-1.0) on foreign boot
 *   _verified_boot_start       boot_start while the incarnation still matches; unproven -> None
 *   _same_incarnation          exact boot-tick match else 2 s epoch tolerance (None matches)
 *   spawned_by_this_process    spawn-tag pid+create proof; except (psutil.Error, OSError) -> False
 *   _read_ledger_or_quarantine _read_ledger; corrupt -> os.replace park + logger.warning
 *   is_desktop_owned_backend   HERMES_DESKTOP + token / SSH-argv credential
 *   local_desktop_client       declared is True + provider local + native_bootstrap is True
 *   _desktop_spawner_identity  HERMES_PARENT_PID (+ winms:<ms> marker) -> (pid, create)
 *   _new_entry                 LedgerEntry projection (install_id/time.time/_verified_boot_start)
 *   entry_still_running        _pid_alive_matches(...) is not False
 *   is_owner_death_supervisor  argv[1:2] == [str(_SUPERVISOR_SCRIPT)]
 *   _terminate_then_kill       SIGTERM, 2 s grace, SIGKILL; incarnation guard
 *   _reparented_orphan         ppid in (0,1), no SSH-lock claim, old enough
 *   reap_orphaned_backend_owner HELD_BY_OTHER reap rung (ledger proof + dead spawner)
 *
 * Self-contained, libjson-only. Every live effect (psutil/os/time/logger calls, the
 * CLAIMED siblings, and the foreign-module imports) enters through ONE universal seam;
 * the cores perform NO I/O:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object
 * (the seam records a copy and must not free it); the result is a NEW json_t* the
 * caller frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == a raised exception. The harness records
 * {"ev": name, "args": args, "ret": result} per call in order; the oracle's recording
 * stubs emit the identical shape driving LIVE Python with the same scripts.
 * BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit) PASS THROUGH
 * every `except Exception` / `except OSError` / named-family catch site; only the
 * SPECIFIC named clause swallows its own class family (full builtin hierarchy below,
 * plus the psutil.Error family). With no seam installed the dispatcher is fail-closed
 * (RuntimeError raise spec); the harness script_call is ALSO fail-closed: a MISSING
 * script returns a RuntimeError raise spec (a scripted null is a valid None result).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception; tuple returns
 * (_desktop_spawner_identity) project to a JSON array; _new_entry projects to the
 * LedgerEntry asdict() field order; _live_boot_id additionally reads/writes the
 * caller-held memo state (in.state / out.state) for its lru_cache(1).
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   os.getpid                  os.getpid()                       args{}            ret int
 *   os.replace                 os.replace(src, dst)              args{src,dst}     ret null|raise
 *   psutil.Process             psutil.Process(pid)               args{pid}         ret proc|raise
 *   proc.create_time           proc.create_time()                args{}            ret num|raise
 *   proc._proc.create_time     proc._proc.create_time(monotonic=True) args{monotonic:true} ret num|raise
 *   proc.environ               proc.environ()                    args{}            ret dict|raise
 *   proc.terminate             proc.terminate()                  args{}            ret null|raise
 *   proc.wait                  proc.wait(timeout=2.0)            args{timeout}     ret num|raise
 *   proc.kill                  proc.kill()                       args{}            ret null|raise
 *   process_birth              CLAIMED process_birth(proc)       args{proc}        ret num|raise
 *   parse_spawn_tag            CLAIMED parse_spawn_tag(raw)      args{raw}         ret tag|null|raise
 *   _read_ledger               CLAIMED _read_ledger(path)        args{path}        ret list|null|raise
 *   _pid_alive_matches         CLAIMED _pid_alive_matches(pid,create_time,boot=) args{pid,create_time,boot} ret bool|null|raise
 *   install_id                 CLAIMED install_id(project_root)  args{project_root} ret str|raise
 *   ledger_entries             CLAIMED ledger_entries()          args{}            ret list|raise
 *   spawner_is_dead            CLAIMED spawner_is_dead(entry)    args{entry}       ret bool|null|raise
 *   live_boot_id               _live_boot_id()                   args{}            ret str|null|raise
 *                              (SEAMED collaborator in the composing flows; the
 *                               standalone _live_boot_id port covers read/strip/cache)
 *   path.read_text             Path(p).read_text(encoding="utf-8") args{path,encoding} ret str|raise
 *   time.time                  time.time()                       args{}            ret num|raise
 *   logger.warning             logger.warning(fmt, *args)        args{fmt,args}    ret null
 *   logger.debug               logger.debug(fmt, *args, exc_info=True) args{fmt,args,exc_info:true} ret null
 *   logger.info                logger.info(fmt, *args)           args{fmt,args}    ret null
 *   is_desktop_ssh_backend_argv  hermes_cli._startup_fast.is_desktop_ssh_backend_argv(argv) args{argv} ret bool|raise
 *   _process_ppid              hermes_cli.dashboard_procs._process_ppid(pid) args{pid} ret int|raise
 *   _lock_owned_serve_pids     hermes_cli.dashboard_procs._lock_owned_serve_pids() args{} ret list|null|raise
 *   kill_fn                    kill_fn(pid) callback             args{pid}         ret null|raise
 *
 * Non-seam inputs (data, read directly — no trace event):
 *   env                os.environ dict (HERMES_DESKTOP / HERMES_DASHBOARD_SESSION_TOKEN /
 *                      HERMES_PARENT_PID / HERMES_PARENT_START_MARKER)
 *   sys_argv           sys.argv list (is_desktop_owned_backend default)
 *   psutil             {LINUX:bool, MACOS:bool} getattr(psutil, flag, False)
 *   reap_min_age       _REAP_MIN_AGE_SECONDS imported constant (_reparented_orphan)
 *   script_path        str(_SUPERVISOR_SCRIPT) (is_owner_death_supervisor)
 *   proc               the psutil.Process double dict (boot_start / _same_incarnation arg)
 *
 * Representation contract (fixture domain, identical on both sides):
 *  - ASCII text; POSIX lexical paths (backslash->slash, trailing slashes stripped).
 *  - integral JSON numbers read as int (libjson has no int/float identity); integral
 *    floats == ints on BOTH sides. Non-integral floats are excluded from str()-computed
 *    positions (pi2_py_str prints integral numbers as ints).
 *  - float()/int() string parsers strip the ASCII White_Space set (space \t \n \v \f \r);
 *    the narrower-than-str.strip file separators \x1c-\x1f and non-ASCII \xa0 are outside
 *    the ASCII domain. Underscores allowed only between digits; float accepts inf/nan
 *    (excluded from fixtures: non-finite); int is base-10 only, within long long.
 *  - the in-function imports (psutil, hermes_cli._startup_fast, hermes_cli.dashboard_procs,
 *    time) resolve silently via sys.modules stubs; import FAILURE is outside the domain.
 *  - psutil.Error family (Error/NoSuchProcess/AccessDenied/ZombieProcess/TimeoutExpired)
 *    is carried in the exc table; scripted psutil kinds raise the stub-module class so
 *    `except psutil.TimeoutExpired` / `except (psutil.Error, OSError)` bind by identity.
 *  - _live_boot_id fixtures start with an EMPTY memo (in.state {}); "calls" (default 1)
 *    drives the function N times to exercise the lru_cache hit (2nd call fires no seam).
 *  - kill_fn presence is a TRUTHY marker (input kill_fn:true -> callable; null -> None).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool pi2_jtruth(const json_t *v) {
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
static bool pi2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool pi2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool pi2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool pi2_is_null_node(const json_t *v) { return v && v->type == JSON_NULL; }
static char *pi2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct pi2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} pi2_seams_t;

static json_t *pi2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool pi2_is_raise(const json_t *r) {
    return pi2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *pi2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: pi2_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *pi2_call(const pi2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return pi2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *pi2_args0(void) { return json_object(); }
static void pi2_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
/* set a key to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL.
 * (json_set(obj,key,NULL) is a no-op in libjson — never pass NULL.) */
static void pi2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (child -> parent) for isinstance-style
 *    catch folding, plus the psutil.Error family. BaseException trio stays above
 *    Exception so `except Exception` never swallows SystemExit/KeyboardInterrupt/
 *    GeneratorExit. ExceptionGroup's second base (Exception) is modelled by its
 *    primary BaseExceptionGroup parent; it is outside the fixture domain. ── */
typedef struct pi2_exc_pair { const char *child; const char *parent; } pi2_exc_pair_t;
static const pi2_exc_pair_t PI2_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
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
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
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
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    /* psutil exception family (psutil.Error subclasses Exception) */
    {"Error", "Exception"},
    {"NoSuchProcess", "Error"},
    {"AccessDenied", "Error"},
    {"ZombieProcess", "Error"},
    {"TimeoutExpired", "Error"},
};
static const char *pi2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(PI2_EXC_TREE) / sizeof(PI2_EXC_TREE[0]); i++)
        if (!strcmp(PI2_EXC_TREE[i].child, c)) return PI2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool pi2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = pi2_exc_parent(c);
    }
    return false;
}

/* ── Python numeric string parsers (ASCII White_Space domain) ── */
static bool pi2_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
static bool pi2_ieq(const char *s, size_t n, const char *lit) {
    size_t l = strlen(lit);
    if (n != l) return false;
    for (size_t i = 0; i < n; i++) {
        char a = s[i]; if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (a != lit[i]) return false;
    }
    return true;
}
/* Validate an underscore-free numeric grammar. is_float allows one '.' and an
 * exponent; int is digits only. Returns true on a well-formed number. */
static bool pi2_num_grammar(const char *s, bool is_float) {
    size_t n = strlen(s), i = 0;
    bool any_digit = false, dot = false, exp = false;
    /* mantissa integer digits */
    while (i < n && s[i] >= '0' && s[i] <= '9') { any_digit = true; i++; }
    if (is_float && i < n && s[i] == '.') {
        dot = true; i++;
        while (i < n && s[i] >= '0' && s[i] <= '9') { any_digit = true; i++; }
    }
    if (!any_digit) return false;
    if (is_float && i < n && (s[i] == 'e' || s[i] == 'E')) {
        exp = true; i++;
        if (i < n && (s[i] == '+' || s[i] == '-')) i++;
        bool exp_digit = false;
        while (i < n && s[i] >= '0' && s[i] <= '9') { exp_digit = true; i++; }
        if (!exp_digit) return false;
    }
    (void)dot; (void)exp;
    return i == n;
}
/* Strip underscores, requiring each '_' to sit between two digits. Returns a
 * malloc'd cleaned copy, or NULL on a misplaced underscore. */
static char *pi2_strip_underscores(const char *s, size_t n) {
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            bool prev_digit = (i > 0 && s[i - 1] >= '0' && s[i - 1] <= '9');
            bool next_digit = (i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '9');
            if (!prev_digit || !next_digit) { free(out); return NULL; }
            continue;
        }
        out[j++] = s[i];
    }
    out[j] = '\0';
    return out;
}
/* float(v) over a JSON value. 0 on success (*out set); -1 with *err raise spec. */
static int pi2_py_float(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = pi2_raise("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 0; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s; while (*p && pi2_py_ws(*p)) p++;
        const char *end = s + strlen(s); while (end > p && pi2_py_ws(end[-1])) end--;
        size_t len = (size_t)(end - p);
        int neg = 0; const char *q = p;
        if (len > 0 && (*q == '+' || *q == '-')) { neg = (*q == '-'); q++; len--; }
        if (pi2_ieq(q, len, "inf") || pi2_ieq(q, len, "infinity")) {
            *out = neg ? -HUGE_VAL : HUGE_VAL; return 0;
        }
        if (pi2_ieq(q, len, "nan")) { *out = NAN; return 0; }
        char *clean = pi2_strip_underscores(q, len);
        bool ok = clean && pi2_num_grammar(clean, true);
        if (ok) {
            char *full = pi2_xsf("%s%s", neg ? "-" : "", clean);
            *out = strtod(full, NULL);
            free(full); free(clean);
            return 0;
        }
        free(clean);
        char *m = pi2_xsf("could not convert string to float: '%s'", s);
        *err = pi2_raise("ValueError", m); free(m);
        return -1;
    }
    const char *tn = (v->type == JSON_ARRAY) ? "list" : "dict";
    char *m = pi2_xsf("float() argument must be a string or a real number, not '%s'", tn);
    *err = pi2_raise("TypeError", m); free(m);
    return -1;
}
/* int(v) over a JSON value (base 10, long long domain). */
static int pi2_py_int(const json_t *v, long long *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = pi2_raise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_NUMBER) { *out = (long long)trunc(v->num_val); return 0; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *p = s; while (*p && pi2_py_ws(*p)) p++;
        const char *end = s + strlen(s); while (end > p && pi2_py_ws(end[-1])) end--;
        size_t len = (size_t)(end - p);
        int neg = 0; const char *q = p;
        if (len > 0 && (*q == '+' || *q == '-')) { neg = (*q == '-'); q++; len--; }
        char *clean = pi2_strip_underscores(q, len);
        bool ok = clean && pi2_num_grammar(clean, false);
        if (ok) {
            char *full = pi2_xsf("%s%s", neg ? "-" : "", clean);
            *out = strtoll(full, NULL, 10);
            free(full); free(clean);
            return 0;
        }
        free(clean);
        char *m = pi2_xsf("invalid literal for int() with base 10: '%s'", s);
        *err = pi2_raise("ValueError", m); free(m);
        return -1;
    }
    const char *tn = (v->type == JSON_ARRAY) ? "list" : "dict";
    char *m = pi2_xsf("int() argument must be a string, a bytes-like object or a real number, not '%s'", tn);
    *err = pi2_raise("TypeError", m); free(m);
    return -1;
}
/* str(v) for JSON values (integral numbers read as int per the domain). */
static char *pi2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == trunc(d) && fabs(d) < 9.2233720368547758e18)
            return pi2_xsf("%lld", (long long)d);
        return pi2_xsf("%.17g", d);
    }
    char *s = json_serialize(v);
    return s ? s : strdup("");
}

/* ── POSIX lexical path helpers (mirror pathlib / PathDouble) ── */
static char *pi2_path_posix(const char *p) {
    if (!p) return strdup("");
    char *s = strdup(p);
    for (char *q = s; *q; q++) if (*q == '\\') *q = '/';
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') s[--n] = '\0';
    return s;
}
/* basename (after the last '/'); "" for a root-only path. */
static const char *pi2_path_name(const char *p) {
    const char *slash = strrchr(p, '/');
    return slash ? slash + 1 : p;
}
/* Path.suffix: the last '.' extension of the basename; "" when the basename has
 * no non-leading dot (dotfiles like ".hidden" have no suffix). */
static char *pi2_path_suffix(const char *p) {
    const char *name = pi2_path_name(p);
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) return strdup("");
    return strdup(dot);
}
/* Path.with_suffix(new): stem (basename minus suffix) + new, keeping the parent. */
static char *pi2_path_with_suffix(const char *p, const char *new_suffix) {
    char *norm = pi2_path_posix(p);
    char *suf = pi2_path_suffix(norm);
    size_t nlen = strlen(norm), slen = strlen(suf);
    char *stem = malloc(nlen - slen + 1);
    memcpy(stem, norm, nlen - slen); stem[nlen - slen] = '\0';
    char *res = pi2_xsf("%s%s", stem, new_suffix);
    free(stem); free(suf); free(norm);
    return res;
}

/* ═══════════════════════ boot / incarnation probes ═══════════════════════ */

/* PoP: pi2_boot_start @ hermes_cli/process_identity.py:boot_start */
json_t *pi2_boot_start(const pi2_seams_t *s, const json_t *proc, const json_t *psutil) {
    /* import psutil (silent); if getattr(psutil, "LINUX", False):
     *     return float(proc._proc.create_time(monotonic=True))
     * return None */
    bool linux_flag = psutil && psutil->type == JSON_OBJECT
                      && json_get_bool(psutil, "LINUX", false);
    if (!linux_flag) return NULL;
    /* proc._proc attribute access: None proc / absent _proc / present-null _proc
     * raise AttributeError before the create_time seam fires. */
    if (!proc || proc->type == JSON_NULL)
        return pi2_raise("AttributeError", "'NoneType' object has no attribute '_proc'");
    if (proc->type != JSON_OBJECT)
        return pi2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute '_proc'");
    const json_t *pp = json_obj_get(proc, "_proc");
    if (!pp)
        return pi2_raise("AttributeError", "'types.SimpleNamespace' object has no attribute '_proc'");
    if (pp->type == JSON_NULL)
        return pi2_raise("AttributeError", "'NoneType' object has no attribute 'create_time'");
    json_t *a = pi2_args0();
    json_set(a, "monotonic", json_bool(true));
    json_t *r = pi2_call(s, "proc._proc.create_time", a);
    if (pi2_is_raise(r)) return r;
    double f; json_t *ferr = NULL;
    if (pi2_py_float(r, &f, &ferr) != 0) { json_free(r); return ferr; }
    json_free(r);
    return json_number(f);
}

/* PoP: pi2_process_create_time @ hermes_cli/process_identity.py:_process_create_time */
json_t *pi2_process_create_time(const pi2_seams_t *s, const json_t *pid) {
    /* try: import psutil; return process_birth(psutil.Process(os.getpid() if pid is None else pid))
     * except Exception: return None */
    json_t *pnode;
    if (!pid || pid->type == JSON_NULL) {
        json_t *r = pi2_call(s, "os.getpid", pi2_args0());
        if (pi2_is_raise(r)) {
            if (pi2_cls_is(pi2_raise_class(r), "Exception")) { json_free(r); return NULL; }
            return r;  /* BaseException passes through */
        }
        pnode = r;
    } else {
        pnode = json_copy(pid);
    }
    json_t *a = pi2_args0();
    pi2_set_node(a, "pid", pnode);
    json_free(pnode);
    json_t *proc = pi2_call(s, "psutil.Process", a);
    if (pi2_is_raise(proc)) {
        if (pi2_cls_is(pi2_raise_class(proc), "Exception")) { json_free(proc); return NULL; }
        return proc;
    }
    json_t *ba = pi2_args0();
    pi2_set_node(ba, "proc", proc);
    json_free(proc);
    json_t *r = pi2_call(s, "process_birth", ba);
    if (pi2_is_raise(r)) {
        if (pi2_cls_is(pi2_raise_class(r), "Exception")) { json_free(r); return NULL; }
        return r;
    }
    return r;  /* process_birth already returns a float */
}

/* PoP: pi2_live_boot_id @ hermes_cli/process_identity.py:_live_boot_id */
json_t *pi2_live_boot_id(const pi2_seams_t *s, json_t *state) {
    /* @functools.lru_cache(maxsize=1) — memo modeled as caller-held state:
     *   state {"boot_id_cached": bool, "boot_id_value": str|null}
     * try: return Path("/proc/sys/kernel/random/boot_id").read_text(encoding="utf-8").strip() or None
     * except OSError: return None */
    if (state && json_get_bool(state, "boot_id_cached", false)) {
        const json_t *v = json_obj_get(state, "boot_id_value");
        return (v && v->type != JSON_NULL) ? json_copy(v) : NULL;
    }
    json_t *a = pi2_args0();
    pi2_set_str(a, "path", "/proc/sys/kernel/random/boot_id");
    pi2_set_str(a, "encoding", "utf-8");
    json_t *r = pi2_call(s, "path.read_text", a);
    json_t *result = NULL;
    if (pi2_is_raise(r)) {
        if (pi2_cls_is(pi2_raise_class(r), "OSError")) {
            json_free(r); result = NULL;  /* except OSError: return None */
        } else {
            /* BaseException / non-OSError passes through (memo NOT written) */
            if (state) {
                json_set(state, "boot_id_cached", json_bool(false));
            }
            return r;
        }
    } else {
        /* .strip() then `or None` */
        const char *txt = (r && r->type == JSON_STRING && r->str_val) ? r->str_val : "";
        const char *p = txt; while (*p && pi2_py_ws(*p)) p++;
        const char *end = txt + strlen(txt); while (end > p && pi2_py_ws(end[-1])) end--;
        if (end > p) {
            size_t n = (size_t)(end - p);
            char *buf = malloc(n + 1); memcpy(buf, p, n); buf[n] = '\0';
            result = json_string(buf); free(buf);
        } else {
            result = NULL;  /* empty/whitespace -> None */
        }
        json_free(r);
    }
    if (state) {
        json_set(state, "boot_id_cached", json_bool(true));
        pi2_set_node(state, "boot_id_value", result);
    }
    return result;
}

/* PoP: pi2_entry_boot @ hermes_cli/process_identity.py:_entry_boot */
json_t *pi2_entry_boot(const pi2_seams_t *s, const json_t *entry, const char *key) {
    /* recorded, live = entry.get("boot_id"), _live_boot_id()
     * if not isinstance(recorded, str) or live is None: return None
     * return entry.get(key) if recorded == live else _EARLIER_BOOT (-1.0) */
    const json_t *recorded = pi2_is_obj(entry) ? json_obj_get(entry, "boot_id") : NULL;
    json_t *live = pi2_call(s, "live_boot_id", pi2_args0());
    if (pi2_is_raise(live)) return live;
    bool rec_is_str = pi2_is_str(recorded);
    bool live_none = !live || live->type == JSON_NULL;
    if (!rec_is_str || live_none) { json_free(live); return NULL; }
    const char *rec_s = recorded->str_val ? recorded->str_val : "";
    const char *live_s = (live->type == JSON_STRING && live->str_val) ? live->str_val : "";
    bool match = !strcmp(rec_s, live_s);
    json_free(live);
    if (match) {
        const json_t *v = pi2_is_obj(entry) ? json_obj_get(entry, key) : NULL;
        return (v && v->type != JSON_NULL) ? json_copy(v) : NULL;
    }
    return json_number(-1.0);  /* _EARLIER_BOOT */
}

/* forward decls for real composition */
static json_t *pi2_same_incarnation(const pi2_seams_t *s, const json_t *proc,
                                    const json_t *create_time, const json_t *boot,
                                    const json_t *psutil);

/* PoP: pi2_verified_boot_start @ hermes_cli/process_identity.py:_verified_boot_start */
json_t *pi2_verified_boot_start(const pi2_seams_t *s, const json_t *pid,
                                const json_t *create_time, const json_t *psutil) {
    /* if not isinstance(pid, int) or pid <= 0 or create_time is None or _live_boot_id() is None:
     *     return None
     * try: proc = psutil.Process(pid)
     *      return boot_start(proc) if _same_incarnation(proc, create_time) else None
     * except Exception: return None */
    bool pid_is_int = pid && pid->type == JSON_NUMBER && pid->num_val == trunc(pid->num_val);
    /* bool is not int in Python: isinstance(True, int) is True actually! bool subclasses int.
     * But pid<=0 gate: True==1>0 passes. Keep number-only domain (fixtures use ints). */
    if (!pid_is_int || pid->num_val <= 0 || !create_time || create_time->type == JSON_NULL)
        return NULL;
    json_t *live = pi2_call(s, "live_boot_id", pi2_args0());
    if (pi2_is_raise(live)) {
        if (pi2_cls_is(pi2_raise_class(live), "Exception")) { json_free(live); return NULL; }
        return live;
    }
    if (!live || live->type == JSON_NULL) { json_free(live); return NULL; }
    json_free(live);
    json_t *a = pi2_args0();
    pi2_set_node(a, "pid", pid);
    json_t *proc = pi2_call(s, "psutil.Process", a);
    if (pi2_is_raise(proc)) {
        if (pi2_cls_is(pi2_raise_class(proc), "Exception")) { json_free(proc); return NULL; }
        return proc;
    }
    json_t *si = pi2_same_incarnation(s, proc, create_time, NULL, psutil);
    if (pi2_is_raise(si)) {
        json_free(proc);
        if (pi2_cls_is(pi2_raise_class(si), "Exception")) { json_free(si); return NULL; }
        return si;
    }
    bool matches = pi2_jtruth(si);
    json_free(si);
    if (!matches) { json_free(proc); return NULL; }
    json_t *bs = pi2_boot_start(s, proc, psutil);
    json_free(proc);
    if (pi2_is_raise(bs)) {
        if (pi2_cls_is(pi2_raise_class(bs), "Exception")) { json_free(bs); return NULL; }
        return bs;
    }
    return bs;
}

/* PoP: pi2_same_incarnation @ hermes_cli/process_identity.py:_same_incarnation */
static json_t *pi2_same_incarnation(const pi2_seams_t *s, const json_t *proc,
                                    const json_t *create_time, const json_t *boot,
                                    const json_t *psutil) {
    /* if boot is not None and (live_boot := boot_start(proc)) is not None:
     *     return live_boot == float(boot)
     * return create_time is None or abs(process_birth(proc) - float(create_time)) < 2.0 */
    if (boot && boot->type != JSON_NULL) {
        json_t *lb = pi2_boot_start(s, proc, psutil);
        if (pi2_is_raise(lb)) return lb;
        if (lb && lb->type != JSON_NULL) {
            double b; json_t *berr = NULL;
            if (pi2_py_float(boot, &b, &berr) != 0) { json_free(lb); return berr; }
            bool eq = (lb->num_val == b);
            json_free(lb);
            return json_bool(eq);
        }
        json_free(lb);
    }
    if (!create_time || create_time->type == JSON_NULL) return json_bool(true);
    json_t *ba = pi2_args0();
    pi2_set_node(ba, "proc", proc);
    json_t *pb = pi2_call(s, "process_birth", ba);
    if (pi2_is_raise(pb)) return pb;
    double live_b; json_t *lerr = NULL;
    if (pi2_py_float(pb, &live_b, &lerr) != 0) { json_free(pb); return lerr; }
    json_free(pb);
    double ct; json_t *cerr = NULL;
    if (pi2_py_float(create_time, &ct, &cerr) != 0) return cerr;
    return json_bool(fabs(live_b - ct) < 2.0);
}

/* ═══════════════════════ spawn tags / desktop ownership ═══════════════════════ */

/* PoP: pi2_spawned_by_this_process @ hermes_cli/process_identity.py:spawned_by_this_process */
json_t *pi2_spawned_by_this_process(const pi2_seams_t *s, const json_t *pid) {
    /* import psutil
     * try: tag = parse_spawn_tag(psutil.Process(int(pid)).environ().get(SPAWN_ENV_VAR))
     * except (psutil.Error, OSError): return False
     * if tag is None or tag.spawner_pid != os.getpid(): return False
     * mine = _process_create_time()
     * return tag.spawner_create is None or mine is None or abs(tag.spawner_create - mine) < 2.0 */
    long long ipid; json_t *ierr = NULL;
    if (pi2_py_int(pid, &ipid, &ierr) != 0) {
        /* int(pid) is OUTSIDE the try in live? No — it is inside the try:
         * psutil.Process(int(pid)). int() TypeError/ValueError is NOT psutil.Error/OSError,
         * so it propagates (not caught). */
        return ierr;
    }
    json_t *a = pi2_args0();
    json_set(a, "pid", json_number((double)ipid));
    json_t *proc = pi2_call(s, "psutil.Process", a);
    if (pi2_is_raise(proc)) {
        const char *k = pi2_raise_class(proc);
        if (pi2_cls_is(k, "Error") || pi2_cls_is(k, "OSError")) { json_free(proc); return json_bool(false); }
        return proc;
    }
    json_t *envr = pi2_call(s, "proc.environ", pi2_args0());
    if (pi2_is_raise(envr)) {
        const char *k = pi2_raise_class(envr);
        if (pi2_cls_is(k, "Error") || pi2_cls_is(k, "OSError")) { json_free(envr); return json_bool(false); }
        json_free(proc); return envr;
    }
    json_free(proc);
    /* .environ().get(SPAWN_ENV_VAR) — environ is a dict; absent -> None */
    const json_t *raw = pi2_is_obj(envr) ? json_obj_get(envr, "HERMES_SPAWN") : NULL;
    json_free(envr);
    json_t *ta = pi2_args0();
    pi2_set_node(ta, "raw", raw);
    json_t *tag = pi2_call(s, "parse_spawn_tag", ta);
    if (pi2_is_raise(tag)) {
        const char *k = pi2_raise_class(tag);
        if (pi2_cls_is(k, "Error") || pi2_cls_is(k, "OSError")) { json_free(tag); return json_bool(false); }
        return tag;
    }
    if (!tag || tag->type == JSON_NULL) { json_free(tag); return json_bool(false); }
    /* tag.spawner_pid != os.getpid() — os.getpid() is OUTSIDE the try in live,
     * so its raise propagates (not folded into the (psutil.Error, OSError) catch). */
    json_t *gp = pi2_call(s, "os.getpid", pi2_args0());
    if (pi2_is_raise(gp)) { json_free(tag); return gp; }
    const json_t *sp = pi2_is_obj(tag) ? json_obj_get(tag, "spawner_pid") : NULL;
    double sp_pid = (sp && sp->type == JSON_NUMBER) ? sp->num_val : 0.0;
    bool pid_match = (gp->type == JSON_NUMBER && gp->num_val == sp_pid);
    json_free(gp);
    if (!pid_match) { json_free(tag); return json_bool(false); }
    /* mine = _process_create_time() (ported, real composition; pid=None) */
    json_t *mine = pi2_process_create_time(s, NULL);
    if (pi2_is_raise(mine)) { json_free(tag); return mine; }
    const json_t *sc = pi2_is_obj(tag) ? json_obj_get(tag, "spawner_create") : NULL;
    bool sc_none = !sc || sc->type == JSON_NULL;
    bool mine_none = !mine || mine->type == JSON_NULL;
    bool result;
    if (sc_none || mine_none) {
        result = true;
    } else {
        double sc_v = sc->num_val, mine_v = mine->num_val;
        result = fabs(sc_v - mine_v) < 2.0;
    }
    json_free(tag); json_free(mine);
    return json_bool(result);
}

/* PoP: pi2_is_desktop_owned_backend @ hermes_cli/process_identity.py:is_desktop_owned_backend */
json_t *pi2_is_desktop_owned_backend(const pi2_seams_t *s, const json_t *env,
                                   const json_t *sys_argv, const json_t *argv) {
    /* if os.environ.get("HERMES_DESKTOP") != "1": return False
     * if os.environ.get("HERMES_DASHBOARD_SESSION_TOKEN"): return True
     * from hermes_cli._startup_fast import is_desktop_ssh_backend_argv
     * return is_desktop_ssh_backend_argv(list(sys.argv[1:] if argv is None else argv)) */
    const char *desk = pi2_is_obj(env) ? json_get_str(env, "HERMES_DESKTOP", NULL) : NULL;
    if (!desk || strcmp(desk, "1") != 0) return json_bool(false);
    const json_t *tok = pi2_is_obj(env) ? json_obj_get(env, "HERMES_DASHBOARD_SESSION_TOKEN") : NULL;
    if (pi2_jtruth(tok)) return json_bool(true);
    /* build the argv list: sys.argv[1:] when argv is None, else argv */
    json_t *list = json_array();
    if (!argv || argv->type == JSON_NULL) {
        if (pi2_is_arr(sys_argv)) {
            for (size_t i = 1; i < json_len(sys_argv); i++)
                json_append(list, json_copy(json_get(sys_argv, i)));
        }
    } else if (pi2_is_arr(argv)) {
        for (size_t i = 0; i < json_len(argv); i++)
            json_append(list, json_copy(json_get(argv, i)));
    }
    json_t *a = pi2_args0();
    json_set(a, "argv", list);
    json_t *r = pi2_call(s, "is_desktop_ssh_backend_argv", a);
    return r;
}

/* PoP: pi2_local_desktop_client @ hermes_cli/process_identity.py:local_desktop_client */
json_t *pi2_local_desktop_client(const json_t *identity, const json_t *declared) {
    /* return (declared is True and isinstance(identity, dict)
     *         and identity.get("provider") == "local"
     *         and identity.get("native_bootstrap") is True) */
    bool decl_true = declared && declared->type == JSON_BOOL && declared->bool_val;
    if (!decl_true) return json_bool(false);
    if (!pi2_is_obj(identity)) return json_bool(false);
    const char *prov = json_get_str(identity, "provider", NULL);
    if (!prov || strcmp(prov, "local") != 0) return json_bool(false);
    const json_t *nb = json_obj_get(identity, "native_bootstrap");
    bool nb_true = nb && nb->type == JSON_BOOL && nb->bool_val;
    return json_bool(nb_true);
}

/* PoP: pi2_desktop_spawner_identity @ hermes_cli/process_identity.py:_desktop_spawner_identity */
json_t *pi2_desktop_spawner_identity(const json_t *env) {
    /* try: spawner_pid = int(os.environ.get("HERMES_PARENT_PID", ""))
     * except (TypeError, ValueError): spawner_pid = 0
     * if spawner_pid <= 0: return None, None
     * marker = os.environ.get("HERMES_PARENT_START_MARKER", "")
     * if not marker.startswith("winms:"): return spawner_pid, None
     * try: return spawner_pid, float(marker.split(":", 1)[1]) / 1000.0
     * except (ValueError, IndexError): return spawner_pid, None */
    const char *pid_s = pi2_is_obj(env) ? json_get_str(env, "HERMES_PARENT_PID", "") : "";
    json_t *pid_node = json_string(pid_s ? pid_s : "");
    long long spawner_pid; json_t *ierr = NULL;
    if (pi2_py_int(pid_node, &spawner_pid, &ierr) != 0) {
        const char *k = pi2_raise_class(ierr);
        if (pi2_cls_is(k, "TypeError") || pi2_cls_is(k, "ValueError")) {
            json_free(ierr); spawner_pid = 0;
        } else { json_free(pid_node); return ierr; }
    }
    json_free(pid_node);
    json_t *out = json_array();
    if (spawner_pid <= 0) {
        json_append(out, json_null()); json_append(out, json_null());
        return out;
    }
    const char *marker = pi2_is_obj(env) ? json_get_str(env, "HERMES_PARENT_START_MARKER", "") : "";
    if (!marker) marker = "";
    json_append(out, json_number((double)spawner_pid));
    if (strncmp(marker, "winms:", 6) != 0) {
        json_append(out, json_null());
        return out;
    }
    /* marker.split(":", 1)[1] — always present after the "winms:" prefix */
    const char *rest = marker + 6;
    json_t *rest_node = json_string(rest);
    double f; json_t *ferr = NULL;
    if (pi2_py_float(rest_node, &f, &ferr) != 0) {
        const char *k = pi2_raise_class(ferr);
        if (pi2_cls_is(k, "ValueError") || pi2_cls_is(k, "IndexError")) {
            json_free(ferr); json_append(out, json_null());
        } else { json_free(rest_node); json_free(out); return ferr; }
    } else {
        json_append(out, json_number(f / 1000.0));
    }
    json_free(rest_node);
    return out;
}

/* ═══════════════════════ ledger ═══════════════════════ */

/* PoP: pi2_read_ledger_or_quarantine @ hermes_cli/process_identity.py:_read_ledger_or_quarantine */
json_t *pi2_read_ledger_or_quarantine(const pi2_seams_t *s, const char *path) {
    /* entries = _read_ledger(path)
     * if entries is None:
     *     parked = path.with_suffix(path.suffix + ".corrupt")
     *     try: os.replace(path, parked); logger.warning("spawn ledger was unreadable; moved to %s", parked)
     *     except OSError: pass
     * return entries */
    char *norm = pi2_path_posix(path);
    json_t *a = pi2_args0();
    pi2_set_str(a, "path", norm);
    json_t *entries = pi2_call(s, "_read_ledger", a);
    if (pi2_is_raise(entries)) { free(norm); return entries; }
    if (entries && entries->type != JSON_NULL) { free(norm); return entries; }
    json_free(entries);
    /* corrupt: park */
    char *suf = pi2_path_suffix(norm);
    char *new_suf = pi2_xsf("%s.corrupt", suf);
    char *parked = pi2_path_with_suffix(norm, new_suf);
    free(suf); free(new_suf);
    json_t *ra = pi2_args0();
    pi2_set_str(ra, "src", norm);
    pi2_set_str(ra, "dst", parked);
    json_t *rr = pi2_call(s, "os.replace", ra);
    if (pi2_is_raise(rr)) {
        if (pi2_cls_is(pi2_raise_class(rr), "OSError")) {
            json_free(rr);  /* except OSError: pass */
        } else {
            free(norm); free(parked); return rr;  /* BaseException / other passes through */
        }
    } else {
        json_free(rr);
        /* logger.warning("spawn ledger was unreadable; moved to %s", parked) */
        json_t *la = pi2_args0();
        pi2_set_str(la, "fmt", "spawn ledger was unreadable; moved to %s");
        json_t *largs = json_array();
        json_append(largs, json_string(parked));
        json_set(la, "args", largs);
        json_t *lr = pi2_call(s, "logger.warning", la);
        if (pi2_is_raise(lr)) {
            if (pi2_cls_is(pi2_raise_class(lr), "OSError")) { json_free(lr); }
            else { free(norm); free(parked); return lr; }
        } else json_free(lr);
    }
    free(norm); free(parked);
    return NULL;  /* entries was None */
}

/* PoP: pi2_new_entry @ hermes_cli/process_identity.py:_new_entry */
json_t *pi2_new_entry(const pi2_seams_t *s, const json_t *pid, const json_t *create_time,
                      const char *purpose, const json_t *project_root,
                      const json_t *spawner_pid, const json_t *spawner_create,
                      const json_t *psutil) {
    /* return LedgerEntry(pid, create_time, purpose, install_id(project_root),
     *     spawner_pid, spawner_create, time.time(), argv="",
     *     boot_start=_verified_boot_start(pid, create_time), boot_id=_live_boot_id())
     * Argument evaluation order (left-to-right): install_id, time.time,
     * _verified_boot_start (real), _live_boot_id (seam). Projection = asdict field order. */
    json_t *ia = pi2_args0();
    pi2_set_node(ia, "project_root", project_root);
    json_t *install = pi2_call(s, "install_id", ia);
    if (pi2_is_raise(install)) return install;
    json_t *tt = pi2_call(s, "time.time", pi2_args0());
    if (pi2_is_raise(tt)) { json_free(install); return tt; }
    json_t *vbs = pi2_verified_boot_start(s, pid, create_time, psutil);
    if (pi2_is_raise(vbs)) { json_free(install); json_free(tt); return vbs; }
    json_t *bid = pi2_call(s, "live_boot_id", pi2_args0());
    if (pi2_is_raise(bid)) { json_free(install); json_free(tt); json_free(vbs); return bid; }
    /* LedgerEntry asdict field order */
    json_t *o = json_object();
    pi2_set_node(o, "pid", pid);
    pi2_set_node(o, "create_time", create_time);
    pi2_set_str(o, "purpose", purpose);
    pi2_set_node(o, "install", install);
    pi2_set_node(o, "spawner_pid", spawner_pid);
    pi2_set_node(o, "spawner_create", spawner_create);
    pi2_set_node(o, "registered_at", tt);
    pi2_set_str(o, "argv", "");
    pi2_set_str(o, "host", "");
    json_set(o, "port", json_null());
    pi2_set_str(o, "profile", "");
    pi2_set_str(o, "hermes_home", "");
    json_set(o, "isolated", json_bool(false));
    pi2_set_node(o, "boot_start", vbs);
    json_set(o, "spawner_boot", json_null());
    pi2_set_node(o, "boot_id", bid);
    json_free(install); json_free(tt); json_free(vbs); json_free(bid);
    return o;
}

/* PoP: pi2_entry_still_running @ hermes_cli/process_identity.py:entry_still_running */
json_t *pi2_entry_still_running(const pi2_seams_t *s, const json_t *entry) {
    /* pid = entry.get("pid")
     * if not isinstance(pid, int) or pid <= 0: return False
     * return _pid_alive_matches(pid, entry.get("create_time"), boot=_entry_boot(entry, "boot_start")) is not False */
    const json_t *pid = pi2_is_obj(entry) ? json_obj_get(entry, "pid") : NULL;
    bool pid_is_int = pid && pid->type == JSON_NUMBER && pid->num_val == trunc(pid->num_val);
    if (!pid_is_int || pid->num_val <= 0) return json_bool(false);
    const json_t *ct = pi2_is_obj(entry) ? json_obj_get(entry, "create_time") : NULL;
    json_t *boot = pi2_entry_boot(s, entry, "boot_start");
    if (pi2_is_raise(boot)) return boot;
    json_t *a = pi2_args0();
    pi2_set_node(a, "pid", pid);
    pi2_set_node(a, "create_time", ct);
    pi2_set_node(a, "boot", boot);
    json_free(boot);
    json_t *r = pi2_call(s, "_pid_alive_matches", a);
    if (pi2_is_raise(r)) return r;
    /* `is not False`: None -> True, True -> True, False -> False */
    bool is_false = r && r->type == JSON_BOOL && !r->bool_val;
    json_free(r);
    return json_bool(!is_false);
}

/* PoP: pi2_is_owner_death_supervisor @ hermes_cli/process_identity.py:is_owner_death_supervisor */
json_t *pi2_is_owner_death_supervisor(const json_t *argv, const char *script_path) {
    /* return list(argv[1:2]) == [str(_SUPERVISOR_SCRIPT)] */
    char *norm = pi2_path_posix(script_path);
    bool match = false;
    if (pi2_is_arr(argv) && json_len(argv) >= 2) {
        const json_t *second = json_get(argv, 1);
        char *s = pi2_py_str(second);
        match = !strcmp(s, norm);
        free(s);
    }
    free(norm);
    return json_bool(match);
}

/* ═══════════════════════ reap flows ═══════════════════════ */

/* PoP: pi2_terminate_then_kill @ hermes_cli/process_identity.py:_terminate_then_kill */
json_t *pi2_terminate_then_kill(const pi2_seams_t *s, const json_t *pid,
                                const json_t *create_time, const json_t *boot,
                                const json_t *psutil) {
    /* try: import psutil; proc = psutil.Process(pid)
     *      if not _same_incarnation(proc, create_time, boot): return False
     *      proc.terminate()
     *      try: proc.wait(timeout=2.0)
     *      except psutil.TimeoutExpired: proc.kill()
     *      return True
     * except Exception: logger.debug("orphan terminate failed for pid %s", pid, exc_info=True); return False */
    json_t *a = pi2_args0();
    pi2_set_node(a, "pid", pid);
    json_t *proc = pi2_call(s, "psutil.Process", a);
    if (pi2_is_raise(proc)) {
        if (pi2_cls_is(pi2_raise_class(proc), "Exception")) {
            json_free(proc); goto fail;
        }
        return proc;
    }
    json_t *si = pi2_same_incarnation(s, proc, create_time, boot, psutil);
    if (pi2_is_raise(si)) {
        json_free(proc);
        if (pi2_cls_is(pi2_raise_class(si), "Exception")) { json_free(si); goto fail; }
        return si;
    }
    bool matches = pi2_jtruth(si);
    json_free(si);
    if (!matches) { json_free(proc); return json_bool(false); }
    json_t *tr = pi2_call(s, "proc.terminate", pi2_args0());
    if (pi2_is_raise(tr)) {
        json_free(proc);
        if (pi2_cls_is(pi2_raise_class(tr), "Exception")) { json_free(tr); goto fail; }
        return tr;
    }
    json_free(tr);
    json_t *wa = pi2_args0();
    json_set(wa, "timeout", json_number(2.0));
    json_t *wr = pi2_call(s, "proc.wait", wa);
    if (pi2_is_raise(wr)) {
        if (pi2_cls_is(pi2_raise_class(wr), "TimeoutExpired")) {
            json_free(wr);
            json_t *kr = pi2_call(s, "proc.kill", pi2_args0());
            if (pi2_is_raise(kr)) {
                json_free(proc);
                if (pi2_cls_is(pi2_raise_class(kr), "Exception")) { json_free(kr); goto fail; }
                return kr;
            }
            json_free(kr);
        } else if (pi2_cls_is(pi2_raise_class(wr), "Exception")) {
            json_free(proc); json_free(wr); goto fail;
        } else {
            json_free(proc); return wr;
        }
    } else {
        json_free(wr);
    }
    json_free(proc);
    return json_bool(true);
fail:
    ;
    json_t *la = pi2_args0();
    pi2_set_str(la, "fmt", "orphan terminate failed for pid %s");
    json_t *largs = json_array();
    json_append(largs, pid ? json_copy(pid) : json_null());  /* raw pid arg (%s is internal to logging) */
    json_set(la, "args", largs);
    json_set(la, "exc_info", json_bool(true));
    json_t *lr = pi2_call(s, "logger.debug", la);
    if (pi2_is_raise(lr)) {
        if (pi2_cls_is(pi2_raise_class(lr), "Exception")) { json_free(lr); return json_bool(false); }
        return lr;
    }
    json_free(lr);
    return json_bool(false);
}

/* PoP: pi2_reparented_orphan @ hermes_cli/process_identity.py:_reparented_orphan */
json_t *pi2_reparented_orphan(const pi2_seams_t *s, const json_t *pid, const json_t *reap_min_age) {
    /* try: from hermes_cli.dashboard_procs import _REAP_MIN_AGE_SECONDS, _lock_owned_serve_pids, _process_ppid
     *      if _process_ppid(pid) not in (0, 1): return False
     *      if pid in (_lock_owned_serve_pids() or ()): return False
     *      import time as _time; import psutil
     *      return max(0.0, _time.time() - psutil.Process(pid).create_time()) >= _REAP_MIN_AGE_SECONDS
     * except Exception: return False */
    json_t *pa = pi2_args0();
    pi2_set_node(pa, "pid", pid);
    json_t *ppid = pi2_call(s, "_process_ppid", pa);
    if (pi2_is_raise(ppid)) {
        if (pi2_cls_is(pi2_raise_class(ppid), "Exception")) { json_free(ppid); return json_bool(false); }
        return ppid;
    }
    double ppid_v = (ppid && ppid->type == JSON_NUMBER) ? ppid->num_val : -999.0;
    json_free(ppid);
    if (!(ppid_v == 0.0 || ppid_v == 1.0)) return json_bool(false);
    json_t *lp = pi2_call(s, "_lock_owned_serve_pids", pi2_args0());
    if (pi2_is_raise(lp)) {
        if (pi2_cls_is(pi2_raise_class(lp), "Exception")) { json_free(lp); return json_bool(false); }
        return lp;
    }
    /* pid in (list or ()) */
    bool claimed = false;
    double pid_v = (pid && pid->type == JSON_NUMBER) ? pid->num_val : 0.0;
    if (pi2_is_arr(lp)) {
        for (size_t i = 0; i < json_len(lp); i++) {
            const json_t *e = json_get(lp, i);
            if (e && e->type == JSON_NUMBER && e->num_val == pid_v) { claimed = true; break; }
        }
    }
    json_free(lp);
    if (claimed) return json_bool(false);
    json_t *tt = pi2_call(s, "time.time", pi2_args0());
    if (pi2_is_raise(tt)) {
        if (pi2_cls_is(pi2_raise_class(tt), "Exception")) { json_free(tt); return json_bool(false); }
        return tt;
    }
    json_t *proc_a = pi2_args0();
    pi2_set_node(proc_a, "pid", pid);
    json_t *proc = pi2_call(s, "psutil.Process", proc_a);
    if (pi2_is_raise(proc)) {
        json_free(tt);
        if (pi2_cls_is(pi2_raise_class(proc), "Exception")) { json_free(proc); return json_bool(false); }
        return proc;
    }
    json_t *ct = pi2_call(s, "proc.create_time", pi2_args0());
    json_free(proc);
    if (pi2_is_raise(ct)) {
        json_free(tt);
        if (pi2_cls_is(pi2_raise_class(ct), "Exception")) { json_free(ct); return json_bool(false); }
        return ct;
    }
    double now = tt->num_val, birth = ct->num_val;
    json_free(tt); json_free(ct);
    double age = now - birth; if (age < 0.0) age = 0.0;  /* max(0.0, ...) */
    double min_age = (reap_min_age && reap_min_age->type == JSON_NUMBER) ? reap_min_age->num_val : 0.0;
    return json_bool(age >= min_age);
}

/* PoP: pi2_reap_orphaned_backend_owner @ hermes_cli/process_identity.py:reap_orphaned_backend_owner */
json_t *pi2_reap_orphaned_backend_owner(const pi2_seams_t *s, const json_t *pid,
                                      const json_t *create_time, const json_t *kill_fn,
                                      const json_t *psutil, const json_t *reap_min_age) {
    /* try: pid = int(pid)
     * except (TypeError, ValueError): return None
     * if pid <= 0 or pid == os.getpid(): return None
     * try: entry = next((e for e in ledger_entries() if e.get("pid") == pid
     *         and e.get("purpose") in _BACKEND_OWNER_PURPOSES
     *         and (e.get("create_time") is None or create_time is None
     *              or abs(float(e["create_time"]) - float(create_time)) < 2.0)), None)
     * except Exception: return None
     * if entry is None: return None
     * if spawner_is_dead(entry) is not True and not (entry.get("spawner_pid") is None
     *         and _reparented_orphan(pid)): return None
     * try: if kill_fn is not None: kill_fn(pid)
     *      elif not _terminate_then_kill(pid, entry.get("create_time"), _entry_boot(entry, "boot_start")): return None
     * except Exception: logger.debug("backend owner reap failed for %s", entry, exc_info=True); return None
     * logger.info("reaped orphaned %s backend owner pid %s", entry.get("purpose"), pid); return pid */
    long long ipid; json_t *ierr = NULL;
    if (pi2_py_int(pid, &ipid, &ierr) != 0) {
        const char *k = pi2_raise_class(ierr);
        if (pi2_cls_is(k, "TypeError") || pi2_cls_is(k, "ValueError")) { json_free(ierr); return NULL; }
        return ierr;
    }
    if (ipid <= 0) return NULL;
    json_t *gp = pi2_call(s, "os.getpid", pi2_args0());
    if (pi2_is_raise(gp)) return gp;  /* outside try in live? os.getpid is before the ledger try; propagate */
    double own = (gp->type == JSON_NUMBER) ? gp->num_val : -1.0;
    json_free(gp);
    if ((double)ipid == own) return NULL;
    /* ledger_entries() + generator filter */
    json_t *entries = pi2_call(s, "ledger_entries", pi2_args0());
    if (pi2_is_raise(entries)) {
        if (pi2_cls_is(pi2_raise_class(entries), "Exception")) { json_free(entries); return NULL; }
        return entries;
    }
    json_t *entry = NULL;
    if (pi2_is_arr(entries)) {
        for (size_t i = 0; i < json_len(entries); i++) {
            const json_t *e = json_get(entries, i);
            if (!pi2_is_obj(e)) continue;
            const json_t *epid = json_obj_get(e, "pid");
            if (!(epid && epid->type == JSON_NUMBER && epid->num_val == (double)ipid)) continue;
            const char *purpose = json_get_str(e, "purpose", NULL);
            bool purpose_ok = purpose &&
                (!strcmp(purpose, "serve") || !strcmp(purpose, "dashboard") || !strcmp(purpose, "gateway"));
            if (!purpose_ok) continue;
            const json_t *ect = json_obj_get(e, "create_time");
            bool ct_ok;
            if (!ect || ect->type == JSON_NULL || !create_time || create_time->type == JSON_NULL) {
                ct_ok = true;
            } else if (ect->type != JSON_NUMBER || create_time->type != JSON_NUMBER) {
                /* float(e["create_time"]) / float(create_time) raises ValueError on a
                 * non-numeric -> the generator's except Exception folds to None. */
                json_free(entries);
                return NULL;
            } else {
                double a = ect->num_val, b = create_time->num_val;
                ct_ok = fabs(a - b) < 2.0;
            }
            if (!ct_ok) continue;
            entry = json_copy(e);
            break;
        }
    }
    json_free(entries);
    if (!entry) return NULL;
    /* spawner_is_dead(entry) is not True and not (spawner_pid is None and _reparented_orphan(pid)) */
    json_t *sa = pi2_args0();
    pi2_set_node(sa, "entry", entry);
    json_t *sd = pi2_call(s, "spawner_is_dead", sa);
    if (pi2_is_raise(sd)) { json_free(entry); return sd; }
    bool sd_true = sd && sd->type == JSON_BOOL && sd->bool_val;
    json_free(sd);
    if (!sd_true) {
        const json_t *spid = json_obj_get(entry, "spawner_pid");
        bool spid_none = !spid || spid->type == JSON_NULL;
        bool rep = false;
        if (spid_none) {
            json_t *pid_node = json_number((double)ipid);
            json_t *ro = pi2_reparented_orphan(s, pid_node, reap_min_age);
            json_free(pid_node);
            if (pi2_is_raise(ro)) { json_free(entry); return ro; }
            rep = pi2_jtruth(ro);
            json_free(ro);
        }
        if (!rep) { json_free(entry); return NULL; }
    }
    /* kill */
    const json_t *ect = json_obj_get(entry, "create_time");
    bool kill_ok = true;
    json_t *kill_raise = NULL;
    if (pi2_jtruth(kill_fn)) {
        json_t *ka = pi2_args0();
        json_set(ka, "pid", json_number((double)ipid));
        json_t *kr = pi2_call(s, "kill_fn", ka);
        if (pi2_is_raise(kr)) {
            if (pi2_cls_is(pi2_raise_class(kr), "Exception")) { kill_raise = kr; kill_ok = false; }
            else { json_free(entry); return kr; }
        } else json_free(kr);
    } else {
        json_t *boot = pi2_entry_boot(s, entry, "boot_start");
        if (pi2_is_raise(boot)) {
            if (pi2_cls_is(pi2_raise_class(boot), "Exception")) { kill_raise = boot; kill_ok = false; }
            else { json_free(entry); return boot; }
        } else {
            json_t *pid_node = json_number((double)ipid);
            json_t *tk = pi2_terminate_then_kill(s, pid_node, ect, boot, psutil);
            json_free(pid_node); json_free(boot);
            if (pi2_is_raise(tk)) {
                if (pi2_cls_is(pi2_raise_class(tk), "Exception")) { kill_raise = tk; kill_ok = false; }
                else { json_free(entry); return tk; }
            } else {
                kill_ok = pi2_jtruth(tk);
                json_free(tk);
            }
        }
    }
    if (kill_raise) {
        /* except Exception: logger.debug("backend owner reap failed for %s", entry, exc_info=True); return None */
        json_t *la = pi2_args0();
        pi2_set_str(la, "fmt", "backend owner reap failed for %s");
        json_t *largs = json_array();
        json_append(largs, json_copy(entry));  /* raw entry dict arg (%s is internal to logging) */
        json_set(la, "args", largs);
        json_set(la, "exc_info", json_bool(true));
        json_t *lr = pi2_call(s, "logger.debug", la);
        json_free(kill_raise); json_free(entry);
        if (pi2_is_raise(lr)) {
            if (pi2_cls_is(pi2_raise_class(lr), "Exception")) { json_free(lr); return NULL; }
            return lr;
        }
        json_free(lr);
        return NULL;
    }
    if (!kill_ok) { json_free(entry); return NULL; }
    /* logger.info("reaped orphaned %s backend owner pid %s", entry.get("purpose"), pid) */
    json_t *ia = pi2_args0();
    pi2_set_str(ia, "fmt", "reaped orphaned %s backend owner pid %s");
    json_t *iargs = json_array();
    const char *purpose = json_get_str(entry, "purpose", "");
    json_append(iargs, json_string(purpose ? purpose : ""));
    json_append(iargs, json_number((double)ipid));
    json_set(ia, "args", iargs);
    json_t *ir = pi2_call(s, "logger.info", ia);
    if (pi2_is_raise(ir)) { json_free(entry); return ir; }
    json_free(ir);
    json_free(entry);
    return json_number((double)ipid);
}