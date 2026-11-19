/*
 * skill_usage_flow.c — FLOW cores + leaves of tools/skill_usage.py (LIVE
 * authority) that NO existing PoP claims: the sidecar I/O leaves (_read_lines,
 * _load_usage_strict, is_locked, locked_skill_names), the record helpers
 * (_int_or_zero, _backfilled, _report_row, _bump), the provenance scans
 * (_read_bundled_names, _toggle_suppressed_name, _iter_skill_mds,
 * _scan_local_skills, _match_skill_dir, is_external_only, _external_skill_names),
 * the locked-update orchestrator (_locked_update), the mutator wrappers
 * (_set_field, _mutate_and_emit, reanchor_clock, set_locked), the file-lock
 * context manager (_flock, skill_file_lock) and the archive/restore mover
 * (_relocate).
 *
 * NOTE: the task brief names "hermes_cli/skill_usage.py"; the quarry's defining
 * module is tools/skill_usage.py (hermes_cli/curator.py imports `from tools
 * import skill_usage`). PoPs annotate the REAL defining module verbatim.
 *
 * Self-contained C11, libjson-only. Claimed siblings (_empty_record,
 * latest_activity_at, activity_count, _usage_file, load_usage, save_usage,
 * _non_negative_int, _now_iso, read_suppressed_names, _read_hub_installed_names,
 * is_protected_builtin, read_skill_name, _find_skill_dir, _find_external_skill_dir,
 * _skills_dir, _mutate, _emit_skill_lifecycle, _usage_file_lock, set_state,
 * is_bundled) and every foreign collaborator (Path filesystem probes, open/fd,
 * fcntl/msvcrt, json.loads, utils.atomic_write_text, agent.skill_utils helpers,
 * tools.skill_ledger, shutil.move, os.utime, the logger) enter through ONE
 * universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; su_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is delivered
 * as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised exception).
 * The harness records {"ev": name, "args": args, "ret": result} per call in
 * order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (su_read_lines inside
 * su_read_bundled_names, su_backfilled inside su_report_row,
 * su_load_usage_strict inside su_locked_update / su_is_locked /
 * su_locked_skill_names, su_read_bundled_names inside su_scan_local_skills,
 * su_toggle_suppressed_name inside su_relocate, su_flock inside
 * skill_file_lock); the oracle keeps the extracted defs real on the same paths,
 * so their internal seam events appear in BOTH traces. su_scan_local_skills
 * inlines _iter_skill_mds's lazy rglob walk interleaved with the prot/keep
 * comprehension filter (a generator consumed lazily — trace order matters).
 *
 * Mutator closures passed to the CLAIMED _mutate sibling are PROJECTED to their
 * live __qualname__ (e.g. "reanchor_clock.<locals>._apply") and never execute on
 * either side — the claimed sibling owns running them (same convention as
 * setup_flow2's steps-list closures). _locked_update's op/guard callbacks ARE
 * seamed for direct drive; the oracle scripts their (result, dirty) tuple.
 *
 * Exception propagation: every su flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except Exception` sites (_locked_update,
 * _toggle_suppressed_name write, _relocate ledger/shutil) swallow every
 * non-BaseException kind; `except OSError` (_read_lines, _relocate rename)
 * swallows the OSError family (IOError is an alias); `suppress(OSError)` /
 * `suppress(Exception)` (_relocate) likewise. The BaseException trio
 * (SystemExit / KeyboardInterrupt / GeneratorExit) ALWAYS passes through. The
 * exception-parent table below carries the FULL builtin hierarchy.
 * Fail-closed: a missing/empty seam script yields a RuntimeError raise spec
 * (recorded, then propagated/swallowed exactly as live); no seam installed at
 * all yields the same spec. logger.debug is an inert recording event (scripted
 * null in every fixture; the oracle's RecLogger never raises).
 *
 * skill_file_lock is a contextmanager split into su_skill_file_lock_enter /
 * su_skill_file_lock_exit. The harness fires a "body" seam at the yield point;
 * an enter raise skips body AND exit; an exit raise replaces the body exception
 * (contextlib semantics); the no-lock branch (no fcntl/msvcrt, or re-entrant)
 * yields with no finally. The held-lock set + has_paths flag travel as state.
 *
 * Fixture domain (ASCII): paths are verbatim POSIX strings (the oracle seeds
 * PurePosixPath / Path doubles so str() keeps separators); int()-parse strings
 * use ASCII whitespace only (int() does NOT strip \x1c-\x1f; \xa0 excluded);
 * _read_lines text uses ASCII splitlines boundaries; json.loads is a seamed
 * stdlib double (scripts supply the parsed value or a JSONDecodeError raise);
 * integral floats normalize to ints on both sides and are kept OUT of
 * str()-computed positions; op scripts are 2-element [result, dirty] arrays
 * (unpack-error cases exercise the ValueError/TypeError branches); open returns
 * a string fd tag; stat returns {"st_size": int}; logged raises are restricted
 * to kinds whose str(exc)==msg (OSError/ValueError/RuntimeError/TypeError).
 *
 * Seam name table (name -> live call):
 *   path.exists / path.read_text / path.mkdir / path.stat / path.write_text /
 *   path.rglob / path.rename     Path filesystem probes (args carry {path,...})
 *   open                         builtin open(path, mode, encoding=) -> fd tag
 *   fd.seek / fd.fileno          the fd double's methods (args {fd, ...})
 *   fcntl.flock                  fcntl.flock(fd, op)  args{fd, operation}
 *   msvcrt.locking               msvcrt.locking(fd, op, n) args{fd, lock, nbytes}
 *   json.loads                   json.loads(text)  args{text}
 *   atomic_write_text            utils.atomic_write_text(path, text, tmp_prefix=)
 *   logger.debug                 inert recording event {fmt, args, exc_info?}
 *   body                         skill_file_lock yield point {}
 *   get_all_skills_dirs          agent.skill_utils.get_all_skills_dirs() -> [paths]
 *   is_excluded_skill_path       agent.skill_utils.is_excluded_skill_path(path)
 *   is_external_skill_path       agent.skill_utils.is_external_skill_path(path)
 *   keep / guard / op            _scan_local_skills / _locked_update callbacks
 *   _empty_record / latest_activity_at / activity_count / _usage_file /
 *   load_usage / save_usage / _non_negative_int / _now_iso /
 *   read_suppressed_names / _read_hub_installed_names / is_protected_builtin /
 *   read_skill_name / _find_skill_dir / _find_external_skill_dir / _skills_dir /
 *   _mutate / _emit_skill_lifecycle / set_state / is_bundled   [claimed siblings]
 *   _usage_file_lock.enter / _usage_file_lock.exit   [claimed CM pair]
 *   skill_ledger.capture_before / skill_ledger.record_mutation  tools.skill_ledger
 *   shutil.move                  shutil.move(src, dest)
 *   os.utime                     os.utime(dest)
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
#include <errno.h>
#include "hermes_json.h"

/* ── universal seam plumbing ── */

typedef json_t *(*su_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct su_io {
    su_seam_fn call;
    void *ctx;
    const json_t *env;   /* borrowed ambient values */
} su_io_t;

static int su_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *su_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *su_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* ── FULL builtin exception hierarchy (kind -> parent kind) ── */
static const struct { const char *kind; const char *parent; } SU_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
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
    {"IOError", "Exception"},          /* alias of OSError */
    {"EnvironmentError", "OSError"},   /* alias of OSError */
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
};
#define SU_NEXC ((int)(sizeof SU_EXC_PARENTS / sizeof SU_EXC_PARENTS[0]))

static const char *su_exc_parent(const char *kind) {
    for (int i = 0; i < SU_NEXC; i++)
        if (strcmp(SU_EXC_PARENTS[i].kind, kind) == 0) return SU_EXC_PARENTS[i].parent;
    return "Exception";   /* unknown/custom kinds subclass Exception (make_exc) */
}
/* Walk to Exception (caught by `except Exception`) vs BaseException-only. */
static int su_caught_by_exception(const char *kind) {
    const char *k = kind;
    for (int depth = 0; depth < 16 && k; depth++) {
        if (strcmp(k, "Exception") == 0) return 1;
        if (strcmp(k, "BaseException") == 0) return 0;
        k = su_exc_parent(k);
    }
    return 1;
}
static int su_is_oserror(const char *kind) {
    const char *k = kind;
    for (int depth = 0; depth < 16 && k; depth++) {
        if (strcmp(k, "OSError") == 0 || strcmp(k, "IOError") == 0) return 1;
        if (strcmp(k, "Exception") == 0 || strcmp(k, "BaseException") == 0) return 0;
        k = su_exc_parent(k);
    }
    return 0;
}

static json_t *su_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static json_t *su_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return su_raise(kind, buf);
}
static char *su_fmt(const char *fmt, ...) {
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
static json_t *su_call(su_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = su_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

static int su_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}
static int su_present(const json_t *v) { return v && v->type != JSON_NULL; }
static const char *su_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}
static const char *su_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* ── string helpers (ASCII domain) ── */

/* str.strip() whitespace (ASCII subset; \x85/\xa0 excluded from fixtures). */
static const char SU_STRIP_WS[] = " \t\n\r\v\f\x1c\x1d\x1e";
/* int() strips a NARROWER set (no \x1c-\x1f). */
static const char SU_INT_WS[] = " \t\n\r\v\f";

static char *su_strip(const char *s) {
    if (!s) return strdup("");
    while (*s && strchr(SU_STRIP_WS, *s)) s++;
    size_t n = strlen(s);
    while (n > 0 && strchr(SU_STRIP_WS, s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* str.splitlines() ASCII boundaries: \n \r\n \r \v \f \x1c \x1d \x1e. */
static json_t *su_splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    size_t i = 0, start = 0, n = strlen(s);
    while (i < n) {
        char c = s[i];
        int isbr = (c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
                    c == '\x1c' || c == '\x1d' || c == '\x1e');
        if (isbr) {
            char *ln = (char *)malloc(i - start + 1);
            memcpy(ln, s + start, i - start);
            ln[i - start] = '\0';
            json_append(arr, json_string(ln));
            free(ln);
            if (c == '\r' && i + 1 < n && s[i + 1] == '\n') i++;
            i++;
            start = i;
        } else {
            i++;
        }
    }
    if (start < n) {
        json_append(arr, json_string(s + start));
    }
    return arr;
}

static int su_is_digit(char c) { return c >= '0' && c <= '9'; }

/* int(s) base 10: narrow-ws strip, optional sign, digits with underscores ONLY
 * between digits. *ok=1 on success (int64 domain). */
static long long su_parse_int(const char *s, int *ok) {
    *ok = 0;
    if (!s) return 0;
    while (*s && strchr(SU_INT_WS, *s)) s++;
    size_t n = strlen(s);
    while (n > 0 && strchr(SU_INT_WS, s[n - 1])) n--;
    if (n == 0) return 0;
    size_t i = 0;
    int neg = 0;
    if (s[0] == '+' || s[0] == '-') { neg = (s[0] == '-'); i = 1; }
    char buf[512];
    size_t bn = 0;
    int any = 0;
    for (; i < n; i++) {
        char c = s[i];
        if (su_is_digit(c)) {
            if (bn >= sizeof buf - 1) return 0;
            buf[bn++] = c; any = 1;
        } else if (c == '_') {
            if (!any) return 0;
            if (i + 1 >= n || !su_is_digit(s[i + 1])) return 0;
        } else {
            return 0;
        }
    }
    if (!any) return 0;
    buf[bn] = '\0';
    errno = 0;
    char *end = NULL;
    long long v = strtoll(buf, &end, 10);
    if (errno == ERANGE || !end || *end) return 0;
    *ok = 1;
    return neg ? -v : v;
}

/* ── POSIX path helpers (PurePosixPath semantics for the fixture domain) ── */

/* Normalize leading slashes: POSIX keeps EXACTLY two leading slashes; drop
 * empty + "." components. */
static char *su_posix_norm(const char *p) {
    if (!p) p = "";
    size_t n = strlen(p), lead = 0;
    while (lead < n && p[lead] == '/') lead++;
    char **comps = (char **)calloc(n + 1, sizeof(char *));
    size_t nc = 0, i = lead;
    while (i < n) {
        size_t j = i;
        while (j < n && p[j] != '/') j++;
        if (j > i && !(j - i == 1 && p[i] == '.')) {
            comps[nc] = (char *)malloc(j - i + 1);
            memcpy(comps[nc], p + i, j - i);
            comps[nc][j - i] = '\0';
            nc++;
        }
        i = j + 1;
    }
    char *out = (char *)malloc(n + 3);
    size_t o = 0;
    if (lead == 2) { out[o++] = '/'; out[o++] = '/'; }
    else if (lead >= 1) { out[o++] = '/'; }
    for (size_t k = 0; k < nc; k++) {
        if (k > 0) out[o++] = '/';
        size_t cl = strlen(comps[k]);
        memcpy(out + o, comps[k], cl);
        o += cl;
        free(comps[k]);
    }
    if (o == 0) out[o++] = '.';
    out[o] = '\0';
    free(comps);
    return out;
}

/* PurePosixPath(p).parent */
static char *su_posix_parent(const char *p) {
    char *norm = su_posix_norm(p);
    if (!norm) return strdup(".");
    size_t n = strlen(norm);
    /* root cases */
    if (strcmp(norm, "/") == 0 || strcmp(norm, "//") == 0 || strcmp(norm, ".") == 0) {
        return norm;
    }
    char *last = strrchr(norm, '/');
    if (!last) { free(norm); return strdup("."); }
    if (last == norm) { norm[1] = '\0'; return norm; }          /* "/a" -> "/" */
    if (last == norm + 1 && norm[0] == '/' && norm[1] == '/') {  /* "//a" -> "//" */
        norm[2] = '\0'; return norm;
    }
    *last = '\0';
    /* trim trailing slashes left by the cut (rare in normalized input) */
    size_t m = strlen(norm);
    while (m > 1 && norm[m - 1] == '/') norm[--m] = '\0';
    return norm;
}

/* PurePosixPath(p).name (final component; root -> ""). */
static char *su_posix_name(const char *p) {
    char *norm = su_posix_norm(p);
    if (!norm) return strdup("");
    if (strcmp(norm, "/") == 0 || strcmp(norm, "//") == 0 || strcmp(norm, ".") == 0) {
        free(norm); return strdup("");
    }
    char *last = strrchr(norm, '/');
    char *nm = strdup(last ? last + 1 : norm);
    free(norm);
    return nm;
}

/* PurePosixPath(base) / comp — join by parts (root keeps single separator). */
static char *su_posix_join(const char *base, const char *comp) {
    char *bn = su_posix_norm(base);
    if (!bn) bn = strdup(".");
    size_t bl = strlen(bn);
    int need_sep = (bl > 0 && bn[bl - 1] != '/');
    char *out = (char *)malloc(bl + strlen(comp ? comp : "") + 2);
    if (!out) { free(bn); return NULL; }
    strcpy(out, bn);
    if (need_sep) strcat(out, "/");
    strcat(out, comp ? comp : "");
    free(bn);
    char *res = su_posix_norm(out);
    free(out);
    return res;
}

/* ── set helpers (sorted unique string arrays) ── */

static int su_set_contains(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && strcmp(su_jstr(v), s) == 0) return 1;
    }
    return 0;
}
static void su_set_add(json_t *arr, const char *s) {
    if (!su_set_contains(arr, s)) json_append(arr, json_string(s));
}
static void su_set_discard(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && strcmp(su_jstr(v), s) == 0) {
            json_array_remove(arr, i);
            return;
        }
    }
}
static int su_str_cmp(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}
static json_t *su_sorted_unique(const json_t *arr) {
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY) return out;
    size_t n = json_len(arr);
    const char **tmp = (const char **)calloc(n ? n : 1, sizeof(char *));
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING) tmp[m++] = su_jstr(v);
    }
    qsort(tmp, m, sizeof(char *), su_str_cmp);
    for (size_t i = 0; i < m; i++) {
        if (i == 0 || strcmp(tmp[i], tmp[i - 1]) != 0) json_append(out, json_string(tmp[i]));
    }
    free(tmp);
    return out;
}

/* ── logger (inert recording event; scripted null in fixtures) ── */

static json_t *su_log_debug(su_io_t *io, const char *fmt, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "fmt", json_string(fmt ? fmt : ""));
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_t *r = su_call(io, "logger.debug", a);
    if (su_is_raise(r)) return r;
    json_free(r);
    return NULL;
}
static json_t *su_log_debug_exc(su_io_t *io, const char *fmt, json_t *args_arr) {
    json_t *a = json_object();
    json_set(a, "fmt", json_string(fmt ? fmt : ""));
    json_set(a, "args", args_arr ? args_arr : json_array());
    json_set(a, "exc_info", json_bool(true));
    json_t *r = su_call(io, "logger.debug", a);
    if (su_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── shared filesystem / loader helpers ── */

static json_t *su_path_args(const char *path) {
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    return a;
}
static json_t *su_path_exists(su_io_t *io, const char *path) {
    return su_call(io, "path.exists", su_path_args(path));
}

/* _read_lines(path, fail_log): stripped non-empty lines; [] if missing;
 * OSError -> logger.debug(fail_log, e) + []. Other raises propagate. */
static json_t *su_read_lines(su_io_t *io, const char *path, const char *fail_log) {
    json_t *ex = su_path_exists(io, path);
    if (su_is_raise(ex)) return ex;
    int exists = su_truthy(ex);
    json_free(ex);
    if (!exists) return json_array();
    json_t *a = json_object();
    json_set(a, "path", json_string(path ? path : ""));
    json_set(a, "encoding", json_string("utf-8-sig"));
    json_t *txt = su_call(io, "path.read_text", a);
    if (su_is_raise(txt)) {
        if (su_is_oserror(su_raise_kind(txt))) {
            json_t *la = json_array();
            json_append(la, json_string(su_raise_msg(txt)));
            json_t *lr = su_log_debug(io, fail_log, la);
            json_free(txt);
            if (su_is_raise(lr)) return lr;
            return json_array();
        }
        return txt;   /* non-OSError propagates */
    }
    const char *text = su_jstr(txt);
    json_t *lines = su_splitlines(text);
    json_free(txt);
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        char *s = su_strip(su_jstr(json_get(lines, i)));
        if (s && s[0]) json_append(out, json_string(s));
        free(s);
    }
    json_free(lines);
    return out;
}

/* _load_usage_strict(): the map; raises on unreadable/non-object sidecar. */
static json_t *su_load_usage_strict(su_io_t *io) {
    json_t *pf = su_call(io, "_usage_file", json_object());
    if (su_is_raise(pf)) return pf;
    char *path = strdup(su_jstr(pf));
    json_free(pf);
    json_t *ex = su_path_exists(io, path);
    if (su_is_raise(ex)) { free(path); return ex; }
    int exists = su_truthy(ex);
    json_free(ex);
    json_t *data = NULL;
    if (exists) {
        json_t *a = json_object();
        json_set(a, "path", json_string(path));
        json_set(a, "encoding", json_string("utf-8-sig"));
        json_t *txt = su_call(io, "path.read_text", a);
        if (su_is_raise(txt)) { free(path); return txt; }
        json_t *la = json_object();
        json_set(la, "text", txt ? json_copy(txt) : json_null());
        json_free(txt);
        data = su_call(io, "json.loads", la);
        if (su_is_raise(data)) { free(path); return data; }
    } else {
        data = json_object();
    }
    if (!data || data->type != JSON_OBJECT) {
        json_t *r = su_raise_fmt("ValueError", "%s holds a JSON %s, not an object",
                                 path, su_type_name(data));
        free(path);
        json_free(data);
        return r;
    }
    free(path);
    json_t *out = json_object();
    for (size_t i = 0; i < data->c.count; i++) {
        const json_t *v = data->c.items[i];
        if (v && v->type == JSON_OBJECT)
            json_set(out, data->c.keys[i], json_copy(v));
    }
    json_free(data);
    return out;
}

/* _iter_skill_mds(base, local_only): array of [name, path] pairs. */
static json_t *su_iter_skill_mds(su_io_t *io, const char *base, int local_only) {
    json_t *a = json_object();
    json_set(a, "path", json_string(base ? base : ""));
    json_set(a, "pattern", json_string("SKILL.md"));
    json_t *mds = su_call(io, "path.rglob", a);
    if (su_is_raise(mds)) return mds;
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(mds); i++) {
        const char *md = su_jstr(json_get(mds, i));
        json_t *ea = json_object();
        json_set(ea, "path", json_string(md));
        json_t *excl = su_call(io, "is_excluded_skill_path", ea);
        if (su_is_raise(excl)) { json_free(out); json_free(mds); return excl; }
        int skip = su_truthy(excl);
        json_free(excl);
        if (!skip && local_only) {
            json_t *xa = json_object();
            json_set(xa, "path", json_string(md));
            json_t *ext = su_call(io, "is_external_skill_path", xa);
            if (su_is_raise(ext)) { json_free(out); json_free(mds); return ext; }
            skip = su_truthy(ext);
            json_free(ext);
        }
        if (skip) continue;
        char *par = su_posix_parent(md);
        char *fb = su_posix_name(par);
        free(par);
        json_t *ra = json_object();
        json_set(ra, "skill_md", json_string(md));
        json_set(ra, "fallback", json_string(fb ? fb : ""));
        free(fb);
        json_t *nm = su_call(io, "read_skill_name", ra);
        if (su_is_raise(nm)) { json_free(out); json_free(mds); return nm; }
        json_t *pair = json_array();
        json_append(pair, nm ? nm : json_null());
        json_append(pair, json_string(md));
        json_append(out, pair);
    }
    json_free(mds);
    return out;
}

/* _read_bundled_names(): manifest names | suppressed names (sorted set). */
static json_t *su_read_bundled_names(su_io_t *io) {
    json_t *sd = su_call(io, "_skills_dir", json_object());
    if (su_is_raise(sd)) return sd;
    char *path = su_posix_join(su_jstr(sd), ".bundled_manifest");
    json_free(sd);
    json_t *lines = su_read_lines(io, path, "Failed to read bundled manifest: %s");
    free(path);
    if (su_is_raise(lines)) return lines;
    json_t *names = json_array();
    for (size_t i = 0; i < json_len(lines); i++) {
        const char *ln = su_jstr(json_get(lines, i));
        const char *colon = strchr(ln, ':');
        size_t plen = colon ? (size_t)(colon - ln) : strlen(ln);
        char *pref = (char *)malloc(plen + 1);
        memcpy(pref, ln, plen); pref[plen] = '\0';
        char *n = su_strip(pref);
        free(pref);
        if (n && n[0]) su_set_add(names, n);
        free(n);
    }
    json_free(lines);
    json_t *supp = su_call(io, "read_suppressed_names", json_object());
    if (su_is_raise(supp)) { json_free(names); return supp; }
    for (size_t i = 0; i < json_len(supp); i++) {
        const json_t *v = json_get(supp, i);
        if (v && v->type == JSON_STRING) su_set_add(names, su_jstr(v));
    }
    json_free(supp);
    json_t *out = su_sorted_unique(names);
    json_free(names);
    return out;
}

/* _backfilled(rec): rec with missing default keys appended. */
static json_t *su_backfilled(su_io_t *io, const json_t *rec) {
    if (!rec || rec->type != JSON_OBJECT) {
        return su_call(io, "_empty_record", json_object());
    }
    json_t *empty = su_call(io, "_empty_record", json_object());
    if (su_is_raise(empty)) return empty;
    json_t *out = json_copy(rec);
    if (empty && empty->type == JSON_OBJECT) {
        for (size_t i = 0; i < empty->c.count; i++) {
            const char *k = empty->c.keys[i];
            if (!json_obj_get(rec, k)) json_set(out, k, json_copy(empty->c.items[i]));
        }
    }
    json_free(empty);
    return out;
}

/* _toggle_suppressed_name(skill_name, add): no-op when unchanged. */
static json_t *su_toggle_suppressed_name(su_io_t *io, const char *skill_name, int add) {
    if (!skill_name || !skill_name[0]) return NULL;
    json_t *names = su_call(io, "read_suppressed_names", json_object());
    if (su_is_raise(names)) return names;
    int member = su_set_contains(names, skill_name);
    if (member == (add ? 1 : 0)) { json_free(names); return NULL; }
    if (add) su_set_add(names, skill_name);
    else su_set_discard(names, skill_name);
    /* first arg: _skills_dir() / ".curator_suppressed" (inside the try) */
    json_t *sd = su_call(io, "_skills_dir", json_object());
    if (su_is_raise(sd)) {
        if (!su_caught_by_exception(su_raise_kind(sd))) { json_free(names); return sd; }
        json_t *la = json_array();
        json_append(la, json_string(su_raise_msg(sd)));
        json_t *lr = su_log_debug_exc(io, "Failed to write curator suppression list: %s", la);
        json_free(sd); json_free(names);
        return lr;
    }
    char *path = su_posix_join(su_jstr(sd), ".curator_suppressed");
    json_free(sd);
    /* second arg: "\n".join(sorted(names)) + ("\n" if names else "") */
    json_t *sorted = su_sorted_unique(names);
    size_t total = 0;
    for (size_t i = 0; i < json_len(sorted); i++) total += strlen(su_jstr(json_get(sorted, i))) + 1;
    char *text = (char *)malloc(total + 2);
    text[0] = '\0';
    for (size_t i = 0; i < json_len(sorted); i++) {
        if (i) strcat(text, "\n");
        strcat(text, su_jstr(json_get(sorted, i)));
    }
    if (json_len(sorted) > 0) strcat(text, "\n");
    json_free(sorted);
    json_t *wa = json_object();
    json_set(wa, "path", json_string(path));
    json_set(wa, "text", json_string(text));
    json_set(wa, "tmp_prefix", json_string(".curator_suppressed_"));
    free(path); free(text);
    json_t *wr = su_call(io, "atomic_write_text", wa);
    json_free(names);
    if (su_is_raise(wr)) {
        if (!su_caught_by_exception(su_raise_kind(wr))) return wr;
        json_t *la = json_array();
        json_append(la, json_string(su_raise_msg(wr)));
        json_t *lr = su_log_debug_exc(io, "Failed to write curator suppression list: %s", la);
        json_free(wr);
        return lr;
    }
    json_free(wr);
    return NULL;
}

/* ── ported leaves / flows ── */

/* PoP: su_int_or_zero @ tools/skill_usage.py:_int_or_zero */
static json_t *su_int_or_zero(const json_t *value) {
    /* int(value or 0); except (TypeError, ValueError) -> 0 */
    const json_t *v = su_truthy(value) ? value : NULL;
    if (!v) return json_number(0.0);   /* int(0) */
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return json_number(0.0);   /* OverflowError/ValueError -> 0 */
        return json_number(trunc(d));
    }
    if (v->type == JSON_STRING) {
        int ok = 0;
        long long n = su_parse_int(v->str_val, &ok);
        return json_number(ok ? (double)n : 0.0);
    }
    /* truthy list/dict: int() TypeError -> 0 */
    return json_number(0.0);
}

/* PoP: su_read_lines @ tools/skill_usage.py:_read_lines */
static json_t *su_read_lines_pub(su_io_t *io, const char *path, const char *fail_log) {
    return su_read_lines(io, path, fail_log);
}

/* PoP: su_backfilled @ tools/skill_usage.py:_backfilled */
static json_t *su_backfilled_pub(su_io_t *io, const json_t *rec) {
    return su_backfilled(io, rec);
}

/* PoP: su_report_row @ tools/skill_usage.py:_report_row */
static json_t *su_report_row(su_io_t *io, const char *name, const json_t *raw,
                             const json_t *extra) {
    json_t *bf = su_backfilled(io, raw);
    if (su_is_raise(bf)) return bf;
    json_t *row = json_object();
    json_set(row, "name", json_string(name ? name : ""));
    if (bf && bf->type == JSON_OBJECT) {
        for (size_t i = 0; i < bf->c.count; i++)
            json_set(row, bf->c.keys[i], json_copy(bf->c.items[i]));
    }
    json_free(bf);
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(row, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    /* row.update(last_activity_at=latest_activity_at(row), activity_count=activity_count(row)) */
    json_t *la = json_object();
    json_set(la, "record", json_copy(row));
    json_t *latest = su_call(io, "latest_activity_at", la);
    if (su_is_raise(latest)) { json_free(row); return latest; }
    json_t *aa = json_object();
    json_set(aa, "record", json_copy(row));
    json_t *activity = su_call(io, "activity_count", aa);
    if (su_is_raise(activity)) { json_free(row); json_free(latest); return activity; }
    json_set(row, "last_activity_at", latest ? latest : json_null());
    json_set(row, "activity_count", activity ? activity : json_null());
    return row;
}

/* PoP: su_load_usage_strict @ tools/skill_usage.py:_load_usage_strict */
static json_t *su_load_usage_strict_pub(su_io_t *io) {
    return su_load_usage_strict(io);
}

/* PoP: su_is_locked @ tools/skill_usage.py:is_locked */
static json_t *su_is_locked(su_io_t *io, const char *skill_name) {
    json_t *data = su_load_usage_strict(io);
    if (su_is_raise(data)) return data;
    const json_t *rec = json_obj_get(data, skill_name ? skill_name : "");
    const json_t *locked = (rec && rec->type == JSON_OBJECT) ? json_obj_get(rec, "locked") : NULL;
    int b = su_truthy(locked);
    json_free(data);
    return json_bool(b ? true : false);
}

/* PoP: su_locked_skill_names @ tools/skill_usage.py:locked_skill_names */
static json_t *su_locked_skill_names(su_io_t *io, int strict) {
    json_t *data;
    if (strict) {
        data = su_load_usage_strict(io);
    } else {
        data = su_call(io, "load_usage", json_object());
    }
    if (su_is_raise(data)) return data;
    json_t *names = json_array();
    if (data && data->type == JSON_OBJECT) {
        for (size_t i = 0; i < data->c.count; i++) {
            const json_t *rec = data->c.items[i];
            if (!rec || rec->type != JSON_OBJECT) {
                json_free(names); json_free(data);
                return su_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                                    su_type_name(rec));
            }
            if (su_truthy(json_obj_get(rec, "locked")))
                json_append(names, json_string(data->c.keys[i]));
        }
    }
    json_free(data);
    json_t *out = su_sorted_unique(names);
    json_free(names);
    return out;
}

/* PoP: su_match_skill_dir @ tools/skill_usage.py:_match_skill_dir */
static json_t *su_match_skill_dir(su_io_t *io, const json_t *skill_mds, const char *skill_name) {
    if (!skill_mds || skill_mds->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < json_len(skill_mds); i++) {
        const char *p = su_jstr(json_get(skill_mds, i));
        char *par = su_posix_parent(p);
        char *fb = su_posix_name(par);
        json_t *a = json_object();
        json_set(a, "skill_md", json_string(p));
        json_set(a, "fallback", json_string(fb ? fb : ""));
        free(fb);
        json_t *nm = su_call(io, "read_skill_name", a);
        if (su_is_raise(nm)) { free(par); return nm; }
        int match = (nm && nm->type == JSON_STRING &&
                     strcmp(su_jstr(nm), skill_name ? skill_name : "") == 0);
        json_free(nm);
        if (match) return json_string(par);   /* p.parent (transfer ownership) */
        free(par);
    }
    return NULL;
}

/* PoP: su_is_external_only @ tools/skill_usage.py:is_external_only */
static json_t *su_is_external_only(su_io_t *io, const char *skill_name) {
    json_t *a = json_object();
    json_set(a, "skill_name", json_string(skill_name ? skill_name : ""));
    json_t *local = su_call(io, "_find_skill_dir", a);
    if (su_is_raise(local)) return local;
    int local_none = !su_present(local);
    json_free(local);
    if (!local_none) return json_bool(false);
    json_t *ea = json_object();
    json_set(ea, "skill_name", json_string(skill_name ? skill_name : ""));
    json_t *ext = su_call(io, "_find_external_skill_dir", ea);
    if (su_is_raise(ext)) return ext;
    int b = su_present(ext);
    json_free(ext);
    return json_bool(b ? true : false);
}

/* PoP: su_external_skill_names @ tools/skill_usage.py:_external_skill_names */
static json_t *su_external_skill_names(su_io_t *io) {
    json_t *dirs = su_call(io, "get_all_skills_dirs", json_object());
    if (su_is_raise(dirs)) return dirs;
    json_t *names = json_array();
    for (size_t i = 1; i < json_len(dirs); i++) {   /* [1:] */
        const char *base = su_jstr(json_get(dirs, i));
        json_t *ex = su_path_exists(io, base);
        if (su_is_raise(ex)) { json_free(names); json_free(dirs); return ex; }
        int exists = su_truthy(ex);
        json_free(ex);
        if (!exists) continue;
        json_t *ra = json_object();
        json_set(ra, "path", json_string(base));
        json_set(ra, "pattern", json_string("SKILL.md"));
        json_t *mds = su_call(io, "path.rglob", ra);
        if (su_is_raise(mds)) { json_free(names); json_free(dirs); return mds; }
        for (size_t j = 0; j < json_len(mds); j++) {
            const char *md = su_jstr(json_get(mds, j));
            json_t *ea = json_object();
            json_set(ea, "path", json_string(md));
            json_t *excl = su_call(io, "is_excluded_skill_path", ea);
            if (su_is_raise(excl)) { json_free(mds); json_free(names); json_free(dirs); return excl; }
            int skip = su_truthy(excl);
            json_free(excl);
            if (skip) continue;
            char *par = su_posix_parent(md);
            char *fb = su_posix_name(par);
            free(par);
            json_t *na = json_object();
            json_set(na, "skill_md", json_string(md));
            json_set(na, "fallback", json_string(fb ? fb : ""));
            free(fb);
            json_t *nm = su_call(io, "read_skill_name", na);
            if (su_is_raise(nm)) { json_free(mds); json_free(names); json_free(dirs); return nm; }
            if (nm && nm->type == JSON_STRING) su_set_add(names, su_jstr(nm));
            json_free(nm);
        }
        json_free(mds);
    }
    json_free(dirs);
    json_t *out = su_sorted_unique(names);
    json_free(names);
    return out;
}

/* PoP: su_iter_skill_mds @ tools/skill_usage.py:_iter_skill_mds */
static json_t *su_iter_skill_mds_pub(su_io_t *io, const char *base, int local_only) {
    return su_iter_skill_mds(io, base, local_only);
}

/* PoP: su_read_bundled_names @ tools/skill_usage.py:_read_bundled_names */
static json_t *su_read_bundled_names_pub(su_io_t *io) {
    return su_read_bundled_names(io);
}

/* PoP: su_toggle_suppressed_name @ tools/skill_usage.py:_toggle_suppressed_name */
static json_t *su_toggle_suppressed_name_pub(su_io_t *io, const char *skill_name, int add) {
    return su_toggle_suppressed_name(io, skill_name, add);
}

/* PoP: su_scan_local_skills @ tools/skill_usage.py:_scan_local_skills */
static json_t *su_scan_local_skills(su_io_t *io) {
    json_t *sd = su_call(io, "_skills_dir", json_object());
    if (su_is_raise(sd)) return sd;
    char *base = strdup(su_jstr(sd));
    json_free(sd);
    json_t *ex = su_path_exists(io, base);
    if (su_is_raise(ex)) { free(base); return ex; }
    int exists = su_truthy(ex);
    json_free(ex);
    if (!exists) { free(base); return json_array(); }
    json_t *hub = su_call(io, "_read_hub_installed_names", json_object());
    if (su_is_raise(hub)) { free(base); return hub; }
    json_t *bundled = su_read_bundled_names(io);
    if (su_is_raise(bundled)) { free(base); json_free(hub); return bundled; }
    json_t *usage = su_call(io, "load_usage", json_object());
    if (su_is_raise(usage)) { free(base); json_free(hub); json_free(bundled); return usage; }
    /* _iter_skill_mds is a LAZY generator consumed by the comprehension: its
     * per-item read_skill_name interleaves with the prot/keep filter. Inline the
     * rglob walk + filter per item (local_only=True) to preserve trace order. */
    json_t *ra = json_object();
    json_set(ra, "path", json_string(base));
    json_set(ra, "pattern", json_string("SKILL.md"));
    free(base);
    json_t *mds = su_call(io, "path.rglob", ra);
    if (su_is_raise(mds)) { json_free(hub); json_free(bundled); json_free(usage); return mds; }
    json_t *names = json_array();
    for (size_t i = 0; i < json_len(mds); i++) {
        const char *md = su_jstr(json_get(mds, i));
        json_t *ea = json_object();
        json_set(ea, "path", json_string(md));
        json_t *excl = su_call(io, "is_excluded_skill_path", ea);
        if (su_is_raise(excl)) { json_free(names); json_free(mds); json_free(hub); json_free(bundled); json_free(usage); return excl; }
        int skip = su_truthy(excl);
        json_free(excl);
        if (!skip) {
            json_t *xa = json_object();
            json_set(xa, "path", json_string(md));
            json_t *ext = su_call(io, "is_external_skill_path", xa);
            if (su_is_raise(ext)) { json_free(names); json_free(mds); json_free(hub); json_free(bundled); json_free(usage); return ext; }
            skip = su_truthy(ext);
            json_free(ext);
        }
        if (skip) continue;
        char *par = su_posix_parent(md);
        char *fb = su_posix_name(par);
        free(par);
        json_t *na = json_object();
        json_set(na, "skill_md", json_string(md));
        json_set(na, "fallback", json_string(fb ? fb : ""));
        free(fb);
        json_t *nm = su_call(io, "read_skill_name", na);
        if (su_is_raise(nm)) { json_free(names); json_free(mds); json_free(hub); json_free(bundled); json_free(usage); return nm; }
        const char *name = su_jstr(nm);
        json_free(nm);
        if (su_set_contains(hub, name)) continue;
        json_t *pa = json_object();
        json_set(pa, "skill_name", json_string(name));
        json_t *prot = su_call(io, "is_protected_builtin", pa);
        if (su_is_raise(prot)) { json_free(names); json_free(mds); json_free(hub); json_free(bundled); json_free(usage); return prot; }
        int isprot = su_truthy(prot);
        json_free(prot);
        if (isprot) continue;
        json_t *ka = json_object();
        json_set(ka, "name", json_string(name));
        json_set(ka, "skill_md", json_string(md));
        json_set(ka, "bundled", json_copy(bundled));
        json_set(ka, "usage", json_copy(usage));
        json_t *keep = su_call(io, "keep", ka);
        if (su_is_raise(keep)) { json_free(names); json_free(mds); json_free(hub); json_free(bundled); json_free(usage); return keep; }
        int k = su_truthy(keep);
        json_free(keep);
        if (k) su_set_add(names, name);
    }
    json_free(mds); json_free(hub); json_free(bundled); json_free(usage);
    json_t *out = su_sorted_unique(names);
    json_free(names);
    return out;
}

/* PoP: su_bump @ tools/skill_usage.py:_bump */
static json_t *su_bump(su_io_t *io, json_t *rec, const char *count_key, const char *ts_key) {
    if (!rec || rec->type != JSON_OBJECT)
        return su_raise_fmt("TypeError", "'%s' object does not support item assignment",
                            su_type_name(rec));
    const json_t *cur = json_obj_get(rec, count_key);
    json_t *na = json_object();
    json_set(na, "value", cur ? json_copy(cur) : json_null());
    json_t *nn = su_call(io, "_non_negative_int", na);
    if (su_is_raise(nn)) return nn;
    double base = (nn && nn->type == JSON_NUMBER) ? nn->num_val : 0.0;
    json_free(nn);
    json_set(rec, count_key, json_number(base + 1.0));
    json_t *ts = su_call(io, "_now_iso", json_object());
    if (su_is_raise(ts)) return ts;
    json_set(rec, ts_key, ts ? ts : json_null());
    return NULL;
}

/* _locked_update op/guard callback types (seamed for direct drive). */
typedef json_t *(*su_op_fn)(void *ctx, json_t *data);
typedef json_t *(*su_guard_fn)(void *ctx);

/* PoP: su_locked_update @ tools/skill_usage.py:_locked_update */
static json_t *su_locked_update(su_io_t *io, const char *skill_name,
                                su_op_fn op, void *op_ctx, const char *fail_log,
                                su_guard_fn guard, void *guard_ctx) {
    json_t *ret = NULL, *data = NULL, *body_exc = NULL;
    /* guard runs before locking (inside the try). */
    if (guard) {
        json_t *g = guard(guard_ctx);
        if (su_is_raise(g)) { body_exc = g; goto handle; }
        int ok = su_truthy(g);
        json_free(g);
        if (!ok) goto done;   /* return None, no lock */
    }
    json_t *en = su_call(io, "_usage_file_lock.enter", json_object());
    if (su_is_raise(en)) { body_exc = en; goto handle; }   /* enter raise: no exit */
    json_free(en);
    /* body */
    data = su_load_usage_strict(io);
    if (su_is_raise(data)) { body_exc = data; data = NULL; goto cm_exit; }
    {
        json_t *opr = op(op_ctx, data);
        if (su_is_raise(opr)) { body_exc = opr; goto cm_exit; }
        /* result, dirty = op(data) */
        if (!opr || opr->type != JSON_ARRAY) {
            body_exc = su_raise_fmt("TypeError", "cannot unpack non-iterable %s object",
                                    su_type_name(opr));
            json_free(opr);
            goto cm_exit;
        }
        size_t n = json_len(opr);
        if (n < 2) {
            body_exc = su_raise_fmt("ValueError", "not enough values to unpack (expected 2, got %zu)", n);
            json_free(opr);
            goto cm_exit;
        }
        if (n > 2) {
            body_exc = su_raise("ValueError", "too many values to unpack (expected 2)");
            json_free(opr);
            goto cm_exit;
        }
        json_t *result = json_copy(json_get(opr, 0));
        int dirty = su_truthy(json_get(opr, 1));
        json_free(opr);
        if (dirty) {
            json_t *sa = json_object();
            json_set(sa, "data", json_copy(data));
            json_t *sv = su_call(io, "save_usage", sa);
            if (su_is_raise(sv)) { json_free(result); body_exc = sv; goto cm_exit; }
            int saved = su_truthy(sv);
            json_free(sv);
            if (!saved) { json_free(result); result = NULL; }
        }
        ret = result;
    }
cm_exit:
    {
        json_t *xa = json_object();
        if (body_exc) {
            json_t *e = json_object();
            json_set(e, "kind", json_string(su_raise_kind(body_exc)));
            json_set(e, "msg", json_string(su_raise_msg(body_exc)));
            json_set(xa, "exc", e);
        } else {
            json_set(xa, "exc", json_null());
        }
        json_t *ex = su_call(io, "_usage_file_lock.exit", xa);
        if (su_is_raise(ex)) { json_free(body_exc); body_exc = ex; }
        else json_free(ex);
    }
handle:
    if (body_exc) {
        if (su_caught_by_exception(su_raise_kind(body_exc))) {
            json_t *la = json_array();
            json_append(la, json_string(skill_name ? skill_name : ""));
            json_append(la, json_string(su_raise_msg(body_exc)));
            json_t *lr = su_log_debug_exc(io, fail_log, la);
            json_free(body_exc); body_exc = NULL;
            if (su_is_raise(lr)) { ret = lr; goto done; }
            ret = NULL;
        } else {
            ret = body_exc; body_exc = NULL;   /* BaseException propagates */
        }
    }
done:
    json_free(data);
    return ret;
}

/* PoP: su_set_field @ tools/skill_usage.py:_set_field */
static json_t *su_set_field(su_io_t *io, const char *skill_name, const char *key,
                            const json_t *value) {
    (void)key; (void)value;   /* carried by the projected mutator closure */
    json_t *a = json_object();
    json_set(a, "skill_name", json_string(skill_name ? skill_name : ""));
    json_set(a, "mutator", json_string("_set_field.<locals>.<lambda>"));
    json_set(a, "require_curation_eligible", json_bool(true));
    json_t *r = su_call(io, "_mutate", a);
    if (su_is_raise(r)) return r;
    int b = su_truthy(r);
    json_free(r);
    return json_bool(b ? true : false);
}

/* PoP: su_mutate_and_emit @ tools/skill_usage.py:_mutate_and_emit */
static json_t *su_mutate_and_emit(su_io_t *io, const char *skill_name, const char *action,
                                  const char *mutator_proj, const json_t *hook_kwargs) {
    json_t *a = json_object();
    json_set(a, "skill_name", json_string(skill_name ? skill_name : ""));
    json_set(a, "mutator", json_string(mutator_proj ? mutator_proj : ""));
    json_set(a, "require_curation_eligible", json_bool(false));
    json_t *facts = su_call(io, "_mutate", a);
    if (su_is_raise(facts)) return facts;
    if (facts && facts->type == JSON_OBJECT) {
        json_t *ea = json_object();
        json_set(ea, "skill_name", json_string(skill_name ? skill_name : ""));
        json_set(ea, "action", json_string(action ? action : ""));
        json_set(ea, "record", json_copy(facts));
        json_set(ea, "task_id", json_copy(json_obj_get(hook_kwargs, "task_id")));
        json_set(ea, "session_id", json_copy(json_obj_get(hook_kwargs, "session_id")));
        json_free(facts);
        json_t *er = su_call(io, "_emit_skill_lifecycle", ea);
        if (su_is_raise(er)) return er;
        json_free(er);
        return NULL;
    }
    json_free(facts);
    return NULL;
}

/* PoP: su_reanchor_clock @ tools/skill_usage.py:reanchor_clock */
static json_t *su_reanchor_clock(su_io_t *io, const char *skill_name) {
    json_t *a = json_object();
    json_set(a, "skill_name", json_string(skill_name ? skill_name : ""));
    json_set(a, "mutator", json_string("reanchor_clock.<locals>._apply"));
    json_set(a, "require_curation_eligible", json_bool(false));
    json_t *r = su_call(io, "_mutate", a);
    if (su_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: su_set_locked @ tools/skill_usage.py:set_locked */
static json_t *su_set_locked(su_io_t *io, const char *skill_name, int locked) {
    (void)locked;   /* carried by the projected mutator closure */
    json_t *a = json_object();
    json_set(a, "skill_name", json_string(skill_name ? skill_name : ""));
    json_set(a, "mutator", json_string("set_locked.<locals>.<lambda>"));
    json_set(a, "require_curation_eligible", json_bool(false));
    json_t *r = su_call(io, "_mutate", a);
    if (su_is_raise(r)) return r;
    int b = su_truthy(r);
    json_free(r);
    return json_bool(b ? true : false);
}

/* ── file locking ── */

/* PoP: su_flock @ tools/skill_usage.py:_flock */
static json_t *su_flock(su_io_t *io, const char *fd_tag, int lock,
                        int fcntl_present, int msvcrt_present) {
    if (fcntl_present) {
        json_t *a = json_object();
        json_set(a, "fd", json_string(fd_tag ? fd_tag : ""));
        json_set(a, "operation", json_number(lock ? 2.0 : 8.0));   /* LOCK_EX / LOCK_UN */
        return su_call(io, "fcntl.flock", a);
    }
    json_t *sa = json_object();
    json_set(sa, "fd", json_string(fd_tag ? fd_tag : ""));
    json_set(sa, "pos", json_number(0.0));
    json_t *sr = su_call(io, "fd.seek", sa);
    if (su_is_raise(sr)) return sr;
    json_free(sr);
    if (!msvcrt_present)
        return su_raise("AttributeError", "'NoneType' object has no attribute 'locking'");
    json_t *fa = json_object();
    json_set(fa, "fd", json_string(fd_tag ? fd_tag : ""));
    json_t *fn = su_call(io, "fd.fileno", fa);
    if (su_is_raise(fn)) return fn;
    json_t *la = json_object();
    json_set(la, "fd", fn ? fn : json_null());
    json_set(la, "lock", json_number(lock ? 1.0 : 0.0));   /* LK_LOCK / LK_UNLCK */
    json_set(la, "nbytes", json_number(1.0));
    json_t *lr = su_call(io, "msvcrt.locking", la);
    if (su_is_raise(lr)) return lr;
    json_free(lr);
    return NULL;   /* statement position */
}

/* skill_file_lock enter: pre-yield code. ctx_out gets {noop, fd, path}.
 * held (owned array) + *has_paths mutate in place. */
static json_t *su_skill_file_lock_enter(su_io_t *io, const char *lock_path,
                                        json_t *held, int *has_paths,
                                        int fcntl_present, int msvcrt_present,
                                        json_t **ctx_out) {
    *ctx_out = NULL;
    if (!*has_paths) { *has_paths = 1; }   /* held = _held_locks.paths = set() */
    const char *path = lock_path ? lock_path : "";
    if ((!fcntl_present && !msvcrt_present) || su_set_contains(held, path)) {
        json_t *c = json_object();
        json_set(c, "noop", json_bool(true));
        *ctx_out = c;
        return NULL;
    }
    char *par = su_posix_parent(path);
    json_t *ma = json_object();
    json_set(ma, "path", json_string(par));
    json_set(ma, "parents", json_bool(true));
    json_set(ma, "exist_ok", json_bool(true));
    free(par);
    json_t *mr = su_call(io, "path.mkdir", ma);
    if (su_is_raise(mr)) return mr;
    json_free(mr);
    if (msvcrt_present) {
        json_t *ex = su_path_exists(io, path);
        if (su_is_raise(ex)) return ex;
        int need = !su_truthy(ex);
        json_free(ex);
        if (!need) {
            json_t *sta = su_call(io, "path.stat", su_path_args(path));
            if (su_is_raise(sta)) return sta;
            double sz = json_get_num(sta, "st_size", -1.0);
            json_free(sta);
            need = (sz == 0.0);
        }
        if (need) {
            json_t *wa = json_object();
            json_set(wa, "path", json_string(path));
            json_set(wa, "text", json_string(" "));
            json_set(wa, "encoding", json_string("utf-8"));
            json_t *wr = su_call(io, "path.write_text", wa);
            if (su_is_raise(wr)) return wr;
            json_free(wr);
        }
    }
    json_t *oa = json_object();
    json_set(oa, "path", json_string(path));
    json_set(oa, "mode", json_string(msvcrt_present ? "r+" : "a+"));
    json_set(oa, "encoding", json_string("utf-8"));
    json_t *fd = su_call(io, "open", oa);
    if (su_is_raise(fd)) return fd;
    char *tag = strdup(su_jstr(fd));
    json_free(fd);
    json_t *fr = su_flock(io, tag, 1, fcntl_present, msvcrt_present);
    if (su_is_raise(fr)) { free(tag); return fr; }
    json_free(fr);
    su_set_add(held, path);
    json_t *c = json_object();
    json_set(c, "noop", json_bool(false));
    json_set(c, "fd", json_string(tag));
    json_set(c, "path", json_string(path));
    free(tag);
    *ctx_out = c;
    return NULL;
}

/* skill_file_lock exit: the finally (held.discard + suppress(OSError) unlock). */
static json_t *su_skill_file_lock_exit(su_io_t *io, const json_t *ctx, json_t *held,
                                       int fcntl_present, int msvcrt_present) {
    if (!ctx || su_truthy(json_obj_get(ctx, "noop"))) return NULL;
    const char *path = json_get_str(ctx, "path", "");
    const char *tag = json_get_str(ctx, "fd", "");
    su_set_discard(held, path);
    json_t *fr = su_flock(io, tag, 0, fcntl_present, msvcrt_present);
    if (su_is_raise(fr)) {
        if (su_is_oserror(su_raise_kind(fr))) { json_free(fr); return NULL; }
        return fr;   /* non-OSError replaces the body exception */
    }
    json_free(fr);
    return NULL;
}

/* PoP: su_skill_file_lock @ tools/skill_usage.py:skill_file_lock */
/* (enter/exit pair above; the harness orchestrates body + exception combine) */

/* ── archive / restore mover ── */

static json_t *su_rename_args(const char *src, const char *dest) {
    json_t *a = json_object();
    json_set(a, "src", json_string(src ? src : ""));
    json_set(a, "dest", json_string(dest ? dest : ""));
    return a;
}

/* PoP: su_relocate @ tools/skill_usage.py:_relocate */
static json_t *su_relocate(su_io_t *io, const char *src, const char *dest,
                           const char *skill_name, const char *action,
                           const json_t *capture_kwargs) {
    const char *act = action ? action : "";
    int archiving = (strcmp(act, "archive") == 0);
    /* try: from tools import skill_ledger; capture_before(src, **kwargs) */
    int ledger_present = 0;
    json_t *before = NULL;
    {
        json_t *ca = json_object();
        json_set(ca, "src", json_string(src ? src : ""));
        json_set(ca, "capture_kwargs", capture_kwargs ? json_copy(capture_kwargs) : json_object());
        json_t *cb = su_call(io, "skill_ledger.capture_before", ca);
        if (su_is_raise(cb)) {
            if (!su_caught_by_exception(su_raise_kind(cb))) return cb;
            json_free(cb);   /* _ledger = _ledger_before = None */
        } else {
            ledger_present = 1;
            before = cb;
        }
    }
    /* try: src.rename(dest) except OSError: shutil.move */
    {
        json_t *rr = su_call(io, "path.rename", su_rename_args(src, dest));
        if (su_is_raise(rr)) {
            if (!su_is_oserror(su_raise_kind(rr))) { json_free(before); return rr; }
            json_free(rr);
            json_t *mv = su_call(io, "shutil.move", su_rename_args(src, dest));
            if (su_is_raise(mv)) {
                if (!su_caught_by_exception(su_raise_kind(mv))) { json_free(before); return mv; }
                char *m = su_fmt("failed to %s: %s", act, su_raise_msg(mv));
                json_free(mv); json_free(before);
                json_t *out = json_array();
                json_append(out, json_bool(false));
                json_append(out, json_string(m ? m : ""));
                free(m);
                return out;
            }
            json_free(mv);
        } else {
            json_free(rr);
        }
    }
    if (archiving) {
        json_t *ua = json_object();
        json_set(ua, "path", json_string(dest ? dest : ""));
        json_t *ur = su_call(io, "os.utime", ua);
        if (su_is_raise(ur)) {
            if (!su_is_oserror(su_raise_kind(ur))) { json_free(before); return ur; }
            json_free(ur);   /* suppress(OSError) */
        } else {
            json_free(ur);
        }
    }
    /* if not archiving or is_bundled(skill_name): toggle suppression */
    {
        int bundled = 0;
        if (archiving) {
            json_t *ba = json_object();
            json_set(ba, "skill_name", json_string(skill_name ? skill_name : ""));
            json_t *br = su_call(io, "is_bundled", ba);
            if (su_is_raise(br)) { json_free(before); return br; }
            bundled = su_truthy(br);
            json_free(br);
        }
        if (!archiving || bundled) {
            json_t *tr = su_toggle_suppressed_name(io, skill_name, archiving);
            if (su_is_raise(tr)) { json_free(before); return tr; }
            json_free(tr);
        }
    }
    {
        json_t *ssa = json_object();
        json_set(ssa, "skill_name", json_string(skill_name ? skill_name : ""));
        json_set(ssa, "state", json_string(archiving ? "archived" : "active"));
        json_t *ssr = su_call(io, "set_state", ssa);
        if (su_is_raise(ssr)) { json_free(before); return ssr; }
        json_free(ssr);
    }
    /* with suppress(Exception): if _ledger is not None: record_mutation(...) */
    if (ledger_present) {
        json_t *rma = json_object();
        json_set(rma, "action", json_string(act));
        json_set(rma, "skill_name", json_string(skill_name ? skill_name : ""));
        json_set(rma, "before", su_truthy(before) ? json_copy(before) : json_array());
        json_set(rma, "after_root", json_string(dest ? dest : ""));
        json_t *rmr = su_call(io, "skill_ledger.record_mutation", rma);
        if (su_is_raise(rmr)) {
            if (!su_caught_by_exception(su_raise_kind(rmr))) { json_free(before); return rmr; }
            json_free(rmr);   /* suppress(Exception) */
        } else {
            json_free(rmr);
        }
    }
    json_free(before);
    char *m = su_fmt("%sd to %s", act, dest ? dest : "");
    json_t *out = json_array();
    json_append(out, json_bool(true));
    json_append(out, json_string(m ? m : ""));
    free(m);
    return out;
}