/*
 * curator_flow.c — FLOW cores + leaves of hermes_cli/curator.py (LIVE authority)
 * that NO existing PoP claims: the timestamp-parse leaf (_parse_ts), the confirm
 * prompt (_confirm), the skill-row / config print leaves (_print_skill_rows,
 * _print_curator_config), the pause/pin/lock setters (_set_paused, _set_pin,
 * _set_lock, _cmd_lock, _cmd_unlock), the ledger-actor wrapper (_as_user), the
 * ledger / purge / rollback orchestrators (_cmd_ledger, _cmd_purge,
 * _rollback_ledger_entry, _report_rollback), the argparse helper (_arg), the
 * nested closures (_freq in _cmd_status, _archived_ts in _cmd_purge) and the
 * three _USAGE_SORTS key lambdas (name / recent / activity).
 *
 * Self-contained C11, libjson-only. The claimed sibling (_fmt_ts) and every
 * foreign collaborator (agent.curator load_state/is_enabled/set_paused/
 * get_interval_hours/get_stale_after_days/get_archive_after_days/get_consolidate,
 * tools.skill_usage, tools.skill_ledger, tools.skill_manager_tool._find_skill,
 * hermes_cli.config cfg_get/load_config, shutil.rmtree, time.time, builtin
 * print/input, pathlib.Path) enter through ONE universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; cur_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is delivered
 * as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised exception).
 * The harness records {"ev": name, "args": args, "ret": result} per call in
 * order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (cur_cmd_lock/cur_cmd_unlock ->
 * cur_set_lock, cur_cmd_purge -> cur_confirm + cur_archived_ts,
 * cur_rollback_ledger_entry -> cur_confirm + cur_report_rollback); the oracle
 * keeps the extracted defs real on the same paths, so their internal
 * print/input seam events appear in BOTH traces.
 *
 * Exception propagation: every cur flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except (EOFError, KeyboardInterrupt)`
 * (_confirm) swallows those two kinds only; `except OSError` (_cmd_purge
 * rmtree) swallows the OSError family only; the try/finally (_as_user) runs the
 * reset seam even on a body raise. The BaseException trio (SystemExit /
 * KeyboardInterrupt / GeneratorExit) ALWAYS passes through every
 * `except Exception`-equivalent site. Fail-closed: a missing/empty seam script
 * yields a RuntimeError raise spec (recorded, then propagated/swallowed exactly
 * as live); no seam installed at all yields the same spec.
 *
 * Projections (documented, applied identically by the oracle):
 *  - _parse_ts returns a datetime; both sides project it to .isoformat(). The
 *    fixture domain is the ISO subset: extended/basic date, T/t/space separator,
 *    HH[:MM[:SS]] (extended) or HH[MM[SS]] (basic) time, optional .ffffff
 *    fraction on the SECONDS field (truncated/padded to 6), optional
 *    Z / +-HH:MM / +-HHMM / +-HH offset (a zero offset renders +00:00). Week
 *    dates, arbitrary separators, comma decimals, fractional hours/minutes,
 *    hour-24 rollover, tz-seconds and tz-minute overflow are OUT of scope.
 *  - _arg returns (flags_tuple, kwargs_dict) -> [flags_list, kwargs_obj].
 *  - _freq returns a 2-tuple -> a 2-element array.
 *  - Path collaborators are POSIX strings; .name / join are pure, .exists /
 *    .is_dir / .iterdir / .stat fire seams. _parse_iso_timestamp returns null or
 *    {"ts": epoch}; .timestamp() reads ts (pure). Archive iterdir children are
 *    same-depth absolute paths so string sort == Path parts sort.
 *  - integral floats normalize to ints on both sides and are kept OUT of
 *    str()-computed positions; ledger byte counts are clean multiples of 2**20.
 *
 * Seam name table (name -> live call):
 *   print                     builtin print(*values, end=, sep=)  args{args:[...],end,sep}
 *   input                     builtin input(prompt)               args{prompt}
 *   _fmt_ts                   hermes_cli.curator._fmt_ts [claimed] args{ts}
 *   load_state / is_enabled / set_paused / get_interval_hours /
 *   get_stale_after_days / get_archive_after_days / get_consolidate
 *                             agent.curator [claimed]
 *   exists / is_dir / iterdir / stat   pathlib.Path methods  args{path}
 *   is_agent_created / set_pinned / is_curator_managed / set_locked /
 *   _archive_dir / load_usage / read_skill_name / _parse_iso_timestamp
 *                             tools.skill_usage
 *   _find_skill               tools.skill_manager_tool._find_skill
 *   set_ledger_actor / reset_ledger_actor / compact_ledger / gc_blobs /
 *   list_entries / get_entry / rollback_entry / capture_before / append_entry
 *                             tools.skill_ledger
 *   fn                        _as_user's mutation callback        args{skill}
 *   load_config / cfg_get     hermes_cli.config
 *   rmtree                    shutil.rmtree                       args{path}
 *   time                      time.time                           args{}
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

typedef json_t *(*cur_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct cur_io {
    cur_seam_fn call;
    void *ctx;
    const json_t *env;   /* borrowed ambient values (STATE_ARCHIVED) */
} cur_io_t;

static int cur_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *cur_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *cur_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* BaseException passthrough: the ONLY except sites in this cluster are specific
 * — _parse_ts (TypeError, ValueError), _confirm (EOFError, KeyboardInterrupt),
 * _cmd_purge (OSError). None of them matches SystemExit / GeneratorExit, so those
 * kinds always propagate; KeyboardInterrupt is caught only by _confirm (as live).
 * There is no general `except Exception` site, so no Exception-hierarchy table is
 * needed. The OSError family table below is the complete builtin subclass set. */

/* `except (EOFError, KeyboardInterrupt)` in _confirm. */
static int cur_is_eof_or_keyboard(const char *k) {
    return strcmp(k, "EOFError") == 0 || strcmp(k, "KeyboardInterrupt") == 0;
}

/* `except OSError` in _cmd_purge (rmtree) — the FULL OSError family. */
static int cur_is_oserror(const char *k) {
    return strcmp(k, "OSError") == 0 || strcmp(k, "FileNotFoundError") == 0 ||
           strcmp(k, "PermissionError") == 0 || strcmp(k, "FileExistsError") == 0 ||
           strcmp(k, "IsADirectoryError") == 0 || strcmp(k, "NotADirectoryError") == 0 ||
           strcmp(k, "InterruptedError") == 0 || strcmp(k, "BlockingIOError") == 0 ||
           strcmp(k, "ChildProcessError") == 0 || strcmp(k, "ConnectionError") == 0 ||
           strcmp(k, "BrokenPipeError") == 0 || strcmp(k, "ConnectionAbortedError") == 0 ||
           strcmp(k, "ConnectionRefusedError") == 0 || strcmp(k, "ConnectionResetError") == 0 ||
           strcmp(k, "TimeoutError") == 0 || strcmp(k, "ProcessLookupError") == 0 ||
           strcmp(k, "UnsupportedOperation") == 0;
}

static json_t *cur_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *cur_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return cur_raise(kind, buf);
}

static char *cur_fmt(const char *fmt, ...) {
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
static json_t *cur_call(cur_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = cur_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

static const char *cur_env_str(const cur_io_t *io, const char *key, const char *def) {
    const json_t *v = (io && io->env) ? json_obj_get(io->env, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : def;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int cur_truthy(const json_t *v) {
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

/* Python `x is not None` (a present, non-null node). */
static int cur_present(const json_t *v) { return v && v->type != JSON_NULL; }

/* Scalar string reader (json_get_str is an OBJECT+key accessor). */
static const char *cur_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static const char *cur_type_name(const json_t *v) {
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

/* str() of a scalar JSON value (containers are not interpolated in this
 * cluster; integral floats are out of the str() fixture domain). */
static char *cur_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) {
            snprintf(buf, sizeof buf, "%.0f", d);
            return strdup(buf);
        }
        snprintf(buf, sizeof buf, "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* ── Python format-spec mini engine (subset: [<|>][width][.prec][s|d|f]) ── */

static char *cur_format(const json_t *val, const char *spec) {
    char align = 0; int width = 0; int prec = -1; char type = 0;
    const char *p = spec ? spec : "";
    if (*p == '<' || *p == '>') align = *p++;
    while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
    if (*p == '.') {
        p++; prec = 0;
        while (*p >= '0' && *p <= '9') { prec = prec * 10 + (*p - '0'); p++; }
    }
    if (*p) type = *p;
    char *rendered = NULL;
    if (type == 'd') {
        double d = (val && val->type == JSON_NUMBER) ? val->num_val : 0.0;
        rendered = cur_fmt("%.0f", d);
    } else if (type == 'f') {
        double d = (val && val->type == JSON_NUMBER) ? val->num_val : 0.0;
        int pr = (prec >= 0) ? prec : 6;
        rendered = cur_fmt("%.*f", pr, d);
    } else {
        rendered = cur_py_str(val);
    }
    if (!rendered) rendered = strdup("");
    if (!align) align = (type == 'd' || type == 'f') ? '>' : '<';
    int len = (int)strlen(rendered);
    if (width <= len) return rendered;
    int pad = width - len;
    char *out = (char *)malloc((size_t)width + 1);
    if (!out) { free(rendered); return NULL; }
    if (align == '>') {
        memset(out, ' ', (size_t)pad);
        memcpy(out + pad, rendered, (size_t)len);
    } else {
        memcpy(out, rendered, (size_t)len);
        memset(out + len, ' ', (size_t)pad);
    }
    out[width] = '\0';
    free(rendered);
    return out;
}

/* ── string helpers (ASCII fixture domain) ── */

static char *cur_strip(const char *s) {
    if (!s) return strdup("");
    const char *ws = " \t\n\r\v\f";
    while (*s && strchr(ws, *s)) s++;
    int n = (int)strlen(s);
    while (n > 0 && strchr(ws, s[n - 1])) n--;
    char *out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    memcpy(out, s, (size_t)n);
    out[n] = '\0';
    return out;
}

static char *cur_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *q = out; *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    return out;
}

/* replace every occurrence of `needle` with `repl` (str.replace). */
static char *cur_str_replace(const char *s, const char *needle, const char *repl) {
    if (!s) s = "";
    if (!needle || !*needle) return strdup(s);
    if (!repl) repl = "";
    size_t nl = strlen(needle), rl = strlen(repl), sl = strlen(s);
    size_t cap = sl + 1;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t on = 0;
    const char *q = s;
    while (*q) {
        if (strncmp(q, needle, nl) == 0) {
            if (on + rl + 1 > cap) {
                cap = (on + rl + 1) * 2;
                char *nb = (char *)realloc(out, cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            memcpy(out + on, repl, rl);
            on += rl;
            q += nl;
        } else {
            if (on + 2 > cap) {
                cap *= 2;
                char *nb = (char *)realloc(out, cap);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            out[on++] = *q++;
        }
    }
    out[on] = '\0';
    return out;
}

/* str.splitlines() on \n / \r / \r\n (no trailing empty element). JSON array. */
static json_t *cur_splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    int len = (int)strlen(s);
    int start = 0;
    for (int i = 0; i < len; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            char *ln = (char *)malloc((size_t)(i - start) + 1);
            if (ln) { memcpy(ln, s + start, (size_t)(i - start)); ln[i - start] = '\0'; }
            json_append(arr, json_string(ln ? ln : ""));
            free(ln);
            if (s[i] == '\r' && i + 1 < len && s[i + 1] == '\n') i++;
            start = i + 1;
        }
    }
    if (start < len) {
        char *ln = (char *)malloc((size_t)(len - start) + 1);
        if (ln) { memcpy(ln, s + start, (size_t)(len - start)); ln[len - start] = '\0'; }
        json_append(arr, json_string(ln ? ln : ""));
        free(ln);
    }
    return arr;
}

/* ── pathlib helpers (POSIX strings; pure, no seam) ── */

static char *cur_path_name(const char *s) {
    if (!s) return strdup("");
    int end = (int)strlen(s);
    while (end > 0 && s[end - 1] == '/') end--;
    if (end == 0) return strdup("");
    int start = end;
    while (start > 0 && s[start - 1] != '/') start--;
    int n = end - start;
    char *out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    memcpy(out, s + start, (size_t)n);
    out[n] = '\0';
    return out;
}

static char *cur_path_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    int la = (int)strlen(a);
    while (la > 0 && a[la - 1] == '/') la--;
    if (la == 0) {
        if (a[0] == '/') return cur_fmt("/%s", b);
        return strdup(b);
    }
    return cur_fmt("%.*s/%s", la, a, b);
}

/* ── int(x or 0) coercion (number trunc toward zero; falsy -> 0) ── */

static long long cur_to_int_or_zero(const json_t *v) {
    if (!cur_truthy(v)) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        errno = 0;
        char *end = NULL;
        long long x = strtoll(v->str_val, &end, 10);
        if (errno == ERANGE || !end || *end) return 0;
        return x;
    }
    return 0;
}

/* ── _parse_ts: ISO subset parser -> .isoformat() projection (NULL == None) ── */

static int cur_read_digits(const char *s, int n, int *v) {
    if (n <= 0) return 0;
    *v = 0;
    for (int i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        *v = *v * 10 + (s[i] - '0');
    }
    return 1;
}

static char *cur_iso_parse(const json_t *ts) {
    if (!ts || ts->type != JSON_STRING || !ts->str_val) return NULL; /* TypeError */
    const char *s = ts->str_val;
    int len = (int)strlen(s);
    int i = 0, year, month, day;
    if (len >= 10 && s[4] == '-' && s[7] == '-') {
        if (!cur_read_digits(s, 4, &year)) return NULL;
        if (!cur_read_digits(s + 5, 2, &month)) return NULL;
        if (!cur_read_digits(s + 8, 2, &day)) return NULL;
        i = 10;
    } else if (len >= 8) {
        if (!cur_read_digits(s, 4, &year)) return NULL;
        if (!cur_read_digits(s + 4, 2, &month)) return NULL;
        if (!cur_read_digits(s + 6, 2, &day)) return NULL;
        i = 8;
    } else {
        return NULL;
    }
    if (year < 1 || year > 9999 || month < 1 || month > 12) return NULL;
    int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (month == 2 && leap) mdays[1] = 29;
    if (day < 1 || day > mdays[month - 1]) return NULL;
    int hour = 0, min = 0, sec = 0, us = 0;
    int off_sign = 1, off_h = 0, off_m = 0, has_tz = 0;
    if (i < len) {
        char sep = s[i];
        if (sep != 'T' && sep != 't' && sep != ' ') return NULL;
        i++;
        if (i + 2 > len) return NULL;
        if (!cur_read_digits(s + i, 2, &hour)) return NULL;
        i += 2;
        if (i < len && s[i] == ':') {
            i++;
            if (i + 2 > len) return NULL;
            if (!cur_read_digits(s + i, 2, &min)) return NULL;
            i += 2;
            if (i < len && s[i] == ':') {
                i++;
                if (i + 2 > len) return NULL;
                if (!cur_read_digits(s + i, 2, &sec)) return NULL;
                i += 2;
            }
        } else {
            if (i + 2 <= len && s[i] >= '0' && s[i] <= '9' && s[i + 1] >= '0' && s[i + 1] <= '9') {
                if (!cur_read_digits(s + i, 2, &min)) return NULL;
                i += 2;
                if (i + 2 <= len && s[i] >= '0' && s[i] <= '9' && s[i + 1] >= '0' && s[i + 1] <= '9') {
                    if (!cur_read_digits(s + i, 2, &sec)) return NULL;
                    i += 2;
                }
            }
        }
        if (i < len && s[i] == '.') {
            i++;
            int fd = 0, fn = 0;
            char fbuf[8];
            while (i < len && s[i] >= '0' && s[i] <= '9') {
                if (fn < 6) fbuf[fn++] = s[i];
                fd++; i++;
            }
            if (fd == 0) return NULL;
            while (fn < 6) fbuf[fn++] = '0';
            fbuf[6] = '\0';
            us = atoi(fbuf);
        }
        if (i < len) {
            if (s[i] == 'Z') {
                has_tz = 1; off_sign = 1; off_h = 0; off_m = 0; i++;
            } else if (s[i] == '+' || s[i] == '-') {
                off_sign = (s[i] == '-') ? -1 : 1;
                i++; has_tz = 1;
                if (i + 2 > len) return NULL;
                if (!cur_read_digits(s + i, 2, &off_h)) return NULL;
                i += 2;
                if (i < len && s[i] == ':') {
                    i++;
                    if (i + 2 > len) return NULL;
                    if (!cur_read_digits(s + i, 2, &off_m)) return NULL;
                    i += 2;
                } else if (i + 2 <= len && s[i] >= '0' && s[i] <= '9' && s[i + 1] >= '0' && s[i + 1] <= '9') {
                    if (!cur_read_digits(s + i, 2, &off_m)) return NULL;
                    i += 2;
                }
            } else {
                return NULL;
            }
        }
    }
    if (i != len) return NULL;
    if (hour > 23 || min > 59 || sec > 59) return NULL;
    if (has_tz && (off_h > 23 || off_m > 59)) return NULL;
    if (!has_tz) { off_sign = 1; off_h = 0; off_m = 0; }
    if (off_h == 0 && off_m == 0) off_sign = 1;   /* zero offset always renders +00:00 */
    char base[64];
    if (us > 0)
        snprintf(base, sizeof base, "%04d-%02d-%02dT%02d:%02d:%02d.%06d",
                 year, month, day, hour, min, sec, us);
    else
        snprintf(base, sizeof base, "%04d-%02d-%02dT%02d:%02d:%02d",
                 year, month, day, hour, min, sec);
    return cur_fmt("%s%c%02d:%02d", base, off_sign < 0 ? '-' : '+', off_h, off_m);
}

/* ── output / input seam helpers ── */

/* print(*values): values is a borrowed array (NULL == bare print()). */
static json_t *cur_print(cur_io_t *io, const json_t *values) {
    json_t *a = json_object();
    json_set(a, "args", values ? json_copy(values) : json_array());
    json_t *r = cur_call(io, "print", a);
    if (cur_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

static json_t *cur_print_str(cur_io_t *io, const char *msg) {
    json_t *va = json_array();
    json_append(va, json_string(msg ? msg : ""));
    json_t *r = cur_print(io, va);
    json_free(va);
    return r;
}

/* print(value, end=end). */
static json_t *cur_print_end(cur_io_t *io, const char *msg, const char *end) {
    json_t *a = json_object();
    json_t *va = json_array();
    json_append(va, json_string(msg ? msg : ""));
    json_set(a, "args", va);
    json_set(a, "end", json_string(end ? end : ""));
    json_t *r = cur_call(io, "print", a);
    if (cur_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

static json_t *cur_input(cur_io_t *io, const char *prompt) {
    json_t *a = json_object();
    json_set(a, "prompt", json_string(prompt ? prompt : ""));
    return cur_call(io, "input", a);
}

/* _fmt_ts(ts) — claimed sibling, seamed. Result owned by the caller. */
static json_t *cur_fmt_ts_seam(cur_io_t *io, const json_t *ts) {
    json_t *a = json_object();
    json_set(a, "ts", ts ? json_copy(ts) : json_null());
    return cur_call(io, "_fmt_ts", a);
}
/* ── UTF-8 punctuation constants (byte-exact from the live module) ── */
#define CUR_EMDASH "\xe2\x80\x94"   /* — */
#define CUR_ARROW  "\xe2\x86\x92"   /* → */

/* ── ported leaves ── */

/* PoP: cur_parse_ts @ hermes_cli/curator.py:_parse_ts */
static json_t *cur_parse_ts(const json_t *ts) {
    char *iso = cur_iso_parse(ts);
    if (!iso) return NULL;                 /* TypeError/ValueError -> None */
    json_t *out = json_string(iso);
    free(iso);
    return out;
}

/* PoP: cur_arg @ hermes_cli/curator.py:_arg */
static json_t *cur_arg(const json_t *flags, const json_t *kwargs) {
    json_t *out = json_array();
    json_append(out, flags ? json_copy(flags) : json_array());
    json_append(out, kwargs ? json_copy(kwargs) : json_object());
    return out;
}

/* PoP: cur_freq @ hermes_cli/curator.py:_freq */
static json_t *cur_freq(const json_t *r) {
    const json_t *ac = r ? json_obj_get(r, "activity_count") : NULL;
    const json_t *la = r ? json_obj_get(r, "last_activity_at") : NULL;
    json_t *out = json_array();
    json_append(out, cur_truthy(ac) ? json_copy(ac) : json_number(0));
    json_append(out, cur_truthy(la) ? json_copy(la) : json_string(""));
    return out;
}

/* The three _USAGE_SORTS sort-key lambdas (curator.py lines 604-606). Lambdas
 * have no standalone def name, so their PoP py_fn names below are descriptive
 * (_usage_key_<sort>); each is the first element of the corresponding
 * _USAGE_SORTS[<sort>] tuple. They are genuine unclaimed callable leaves. */

/* PoP: cur_usage_key_name @ hermes_cli/curator.py:_usage_key_name */
static json_t *cur_usage_key_name(const json_t *r) {
    const json_t *n = r ? json_obj_get(r, "name") : NULL;
    if (!n) return cur_raise("KeyError", "'name'");
    return json_copy(n);
}

/* PoP: cur_usage_key_recent @ hermes_cli/curator.py:_usage_key_recent */
static json_t *cur_usage_key_recent(const json_t *r) {
    const json_t *v = r ? json_obj_get(r, "last_activity_at") : NULL;
    return cur_truthy(v) ? json_copy(v) : json_string("");
}

/* PoP: cur_usage_key_activity @ hermes_cli/curator.py:_usage_key_activity */
static json_t *cur_usage_key_activity(const json_t *r) {
    const json_t *v = r ? json_obj_get(r, "activity_count") : NULL;
    if (!v) return json_number(0);         /* absent -> .get default 0 */
    return json_copy(v);                   /* present (even null) -> value */
}

/* PoP: cur_report_rollback @ hermes_cli/curator.py:_report_rollback */
static json_t *cur_report_rollback(cur_io_t *io, int ok, const char *msg) {
    char *m;
    if (ok) m = cur_fmt("curator: %s", msg ? msg : "");
    else m = cur_fmt("curator: rollback failed " CUR_EMDASH " %s", msg ? msg : "");
    json_t *r = cur_print_str(io, m);
    free(m);
    if (cur_is_raise(r)) return r;
    return json_number(ok ? 0 : 1);
}

/* PoP: cur_confirm @ hermes_cli/curator.py:_confirm */
static json_t *cur_confirm(cur_io_t *io, const char *prompt, const char *cancel,
                           const char *eof_prefix) {
    json_t *ans = cur_input(io, prompt);
    if (cur_is_raise(ans)) {
        if (cur_is_eof_or_keyboard(cur_raise_kind(ans))) {
            json_free(ans);
            json_t *r = cur_print_end(io, eof_prefix, "");
            if (cur_is_raise(r)) return r;
        } else {
            return ans;                    /* other kinds propagate */
        }
    } else {
        char *st = cur_strip(cur_jstr(ans));
        char *lo = cur_lower(st);
        int yes = (strcmp(lo, "y") == 0 || strcmp(lo, "yes") == 0);
        free(st); free(lo);
        json_free(ans);
        if (yes) return json_bool(true);
    }
    json_t *r = cur_print_str(io, cancel);
    if (cur_is_raise(r)) return r;
    return json_bool(false);
}

/* PoP: cur_print_skill_rows @ hermes_cli/curator.py:_print_skill_rows */
static json_t *cur_print_skill_rows(cur_io_t *io, const char *title, const json_t *rows) {
    char *hdr = cur_fmt("\n%s:", title ? title : "");
    json_t *r = cur_print_str(io, hdr);
    free(hdr);
    if (cur_is_raise(r)) return r;
    if (!rows || rows->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        const json_t *name = row ? json_obj_get(row, "name") : NULL;
        if (!name) return cur_raise("KeyError", "'name'");
        char *name40 = cur_format(name, "40s");
        json_t *acn = json_number(json_get_num(row, "activity_count", 0));
        json_t *usn = json_number(json_get_num(row, "use_count", 0));
        json_t *vin = json_number(json_get_num(row, "view_count", 0));
        json_t *pan = json_number(json_get_num(row, "patch_count", 0));
        char *ac3 = cur_format(acn, "3d");
        char *us3 = cur_format(usn, "3d");
        char *vi3 = cur_format(vin, "3d");
        char *pa3 = cur_format(pan, "3d");
        json_free(acn); json_free(usn); json_free(vin); json_free(pan);
        const json_t *la = json_obj_get(row, "last_activity_at");
        json_t *fts = cur_fmt_ts_seam(io, la);
        if (cur_is_raise(fts)) {
            free(name40); free(ac3); free(us3); free(vi3); free(pa3);
            return fts;
        }
        char *line = cur_fmt("  %s  activity=%s  use=%s  view=%s  patches=%s  last_activity=%s",
                             name40, ac3, us3, vi3, pa3, cur_jstr(fts));
        json_free(fts);
        free(name40); free(ac3); free(us3); free(vi3); free(pa3);
        r = cur_print_str(io, line);
        free(line);
        if (cur_is_raise(r)) return r;
    }
    return NULL;
}

/* PoP: cur_print_curator_config @ hermes_cli/curator.py:_print_curator_config */
static json_t *cur_print_curator_config(cur_io_t *io) {
    json_t *state = cur_call(io, "load_state", json_object());
    if (cur_is_raise(state)) return state;
    json_t *r;
    char *m;
    const json_t *paused = json_obj_get(state, "paused");
    const char *status_line;
    if (cur_truthy(paused)) {
        status_line = "PAUSED";
    } else {
        json_t *en = cur_call(io, "is_enabled", json_object());
        if (cur_is_raise(en)) { json_free(state); return en; }
        status_line = cur_truthy(en) ? "ENABLED" : "DISABLED";
        json_free(en);
    }
    m = cur_fmt("curator: %s", status_line);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(state); return r; }

    json_t *rc = json_number(json_get_num(state, "run_count", 0));
    char *rcs = cur_py_str(rc); json_free(rc);
    m = cur_fmt("  runs:           %s", rcs); free(rcs);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(state); return r; }

    const json_t *lra = json_obj_get(state, "last_run_at");
    json_t *fts = cur_fmt_ts_seam(io, lra);
    if (cur_is_raise(fts)) { json_free(state); return fts; }
    m = cur_fmt("  last run:       %s", cur_jstr(fts)); json_free(fts);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(state); return r; }

    const json_t *summ = json_obj_get(state, "last_run_summary");
    char *summary = cur_truthy(summ) ? strdup(cur_jstr(summ)) : strdup("(none)");
    json_t *lines;
    if (strstr(summary, "\n")) {
        lines = cur_splitlines(summary);
    } else {
        lines = json_array();
        json_append(lines, json_string(summary));
    }
    free(summary);
    m = cur_fmt("  last summary:   %s", cur_jstr(json_get(lines, 0)));
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(lines); json_free(state); return r; }
    for (size_t i = 1; i < json_len(lines); i++) {
        m = cur_fmt("                  %s", cur_jstr(json_get(lines, i)));
        r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) { json_free(lines); json_free(state); return r; }
    }
    json_free(lines);

    const json_t *report = json_obj_get(state, "last_report_path");
    if (cur_truthy(report)) {
        const char *rep = cur_jstr(report);
        json_t *ea = json_object();
        json_set(ea, "path", json_string(rep));
        json_t *ex = cur_call(io, "exists", ea);
        if (cur_is_raise(ex)) { json_free(state); return ex; }
        int exists = cur_truthy(ex);
        json_free(ex);
        m = cur_fmt("  last report:    %s%s", rep, exists ? "" : " (missing)");
        r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) { json_free(state); return r; }
    }

    json_t *ihj = cur_call(io, "get_interval_hours", json_object());
    if (cur_is_raise(ihj)) { json_free(state); return ihj; }
    long long ih = (long long)((ihj && ihj->type == JSON_NUMBER) ? ihj->num_val : 0);
    json_free(ihj);
    char *iv;
    if (ih % 24 == 0 && ih >= 24) iv = cur_fmt("%lldd", ih / 24);
    else iv = cur_fmt("%lldh", ih);
    m = cur_fmt("  interval:       every %s", iv); free(iv);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(state); return r; }

    json_t *sad = cur_call(io, "get_stale_after_days", json_object());
    if (cur_is_raise(sad)) { json_free(state); return sad; }
    char *sads = cur_py_str(sad); json_free(sad);
    m = cur_fmt("  stale after:    %sd unused", sads); free(sads);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(state); return r; }

    json_t *aad = cur_call(io, "get_archive_after_days", json_object());
    if (cur_is_raise(aad)) { json_free(state); return aad; }
    char *aads = cur_py_str(aad); json_free(aad);
    m = cur_fmt("  archive after:  %sd unused", aads); free(aads);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(state); return r; }

    json_t *cons = cur_call(io, "get_consolidate", json_object());
    if (cur_is_raise(cons)) { json_free(state); return cons; }
    int c = cur_truthy(cons); json_free(cons);
    m = cur_fmt("  consolidate:    %s%s", c ? "on" : "off",
                c ? "" : " (prune-only; LLM merge pass opt-in)");
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(state); return r; }

    json_free(state);
    return NULL;
}
/* ── _PIN_MESSAGES (byte-exact from the live module) ── */
static const char *CUR_PIN_MSG_TRUE[4] = {
    "cannot pin (only agent-created skills participate in curation)",
    "could not pin '{skill}' " CUR_EMDASH " the skill is not curation-eligible (protected built-in or "
    "external), or skills/.usage.json is damaged or not writable. `hermes curator list-unmanaged` "
    "shows which skills the curator tracks.",
    "pinned '{skill}' (recorded; this skill is unmanaged " CUR_EMDASH " auto-transitions never consider "
    "it. Run `hermes curator adopt {skill}` to put it under curator management)",
    "pinned '{skill}' (will bypass auto-transitions)"};
static const char *CUR_PIN_MSG_FALSE[4] = {
    "there's nothing to unpin (curator only tracks agent-created skills)",
    "could not unpin '{skill}' " CUR_EMDASH " the skill is not curation-eligible (protected built-in or "
    "external), or skills/.usage.json is damaged or not writable.",
    "unpinned '{skill}' (recorded; this skill is unmanaged " CUR_EMDASH " it was never under "
    "auto-transitions to begin with)",
    "unpinned '{skill}'"};

/* ── ported setter flows ── */

/* PoP: cur_set_paused @ hermes_cli/curator.py:_set_paused */
static json_t *cur_set_paused(cur_io_t *io, int paused) {
    json_t *a = json_object();
    json_set(a, "paused", json_bool(paused ? true : false));
    json_t *r = cur_call(io, "set_paused", a);
    if (cur_is_raise(r)) return r;
    json_free(r);
    r = cur_print_str(io, paused ? "curator: paused" : "curator: resumed");
    if (cur_is_raise(r)) return r;
    return json_number(0);
}

/* PoP: cur_set_pin @ hermes_cli/curator.py:_set_pin */
static json_t *cur_set_pin(cur_io_t *io, const char *skill, int pinned) {
    const char **msgs = pinned ? CUR_PIN_MSG_TRUE : CUR_PIN_MSG_FALSE;
    const char *not_agent = msgs[0], *not_eligible = msgs[1];
    const char *unmanaged = msgs[2], *done = msgs[3];
    json_t *a = json_object();
    json_set(a, "skill", json_string(skill));
    json_t *ac = cur_call(io, "is_agent_created", a);
    if (cur_is_raise(ac)) return ac;
    int is_ac = cur_truthy(ac); json_free(ac);
    if (!is_ac) {
        char *m = cur_fmt("curator: '%s' is bundled or hub-installed " CUR_EMDASH " %s",
                          skill, not_agent);
        json_t *r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) return r;
        return json_number(1);
    }
    a = json_object();
    json_set(a, "skill", json_string(skill));
    json_set(a, "pinned", json_bool(pinned ? true : false));
    json_t *sp = cur_call(io, "set_pinned", a);
    if (cur_is_raise(sp)) return sp;
    int ok = cur_truthy(sp); json_free(sp);
    if (!ok) {
        char *rep = cur_str_replace(not_eligible, "{skill}", skill);
        char *m = cur_fmt("curator: %s", rep); free(rep);
        json_t *r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) return r;
        return json_number(1);
    }
    a = json_object();
    json_set(a, "skill", json_string(skill));
    json_t *cm = cur_call(io, "is_curator_managed", a);
    if (cur_is_raise(cm)) return cm;
    int managed = cur_truthy(cm); json_free(cm);
    const char *tmpl = managed ? done : unmanaged;
    char *rep = cur_str_replace(tmpl, "{skill}", skill);
    char *m = cur_fmt("curator: %s", rep); free(rep);
    json_t *r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) return r;
    return json_number(0);
}

/* PoP: cur_set_lock @ hermes_cli/curator.py:_set_lock */
static json_t *cur_set_lock(cur_io_t *io, const char *skill, int locked) {
    const char *verb = locked ? "lock" : "unlock";
    json_t *a = json_object();
    json_set(a, "skill", json_string(skill));
    json_t *existing = cur_call(io, "_find_skill", a);
    if (cur_is_raise(existing)) return existing;
    if (!cur_present(existing)) {
        char *m = cur_fmt("curator: no skill named '%s' in the active profile " CUR_EMDASH
                          " nothing to %s. `hermes skills list` shows the names.", skill, verb);
        json_t *r = cur_print_str(io, m); free(m);
        json_free(existing);
        if (cur_is_raise(r)) return r;
        return json_number(1);
    }
    const json_t *pathnode = json_obj_get(existing, "path");
    if (!pathnode) { json_free(existing); return cur_raise("KeyError", "'path'"); }
    char *key = cur_path_name(cur_jstr(pathnode));
    json_free(existing);
    a = json_object();
    json_set(a, "key", json_string(key));
    json_set(a, "locked", json_bool(locked ? true : false));
    json_t *sl = cur_call(io, "set_locked", a);
    if (cur_is_raise(sl)) { free(key); return sl; }
    int ok = cur_truthy(sl); json_free(sl);
    if (!ok) {
        char *m = cur_fmt("curator: could not record the %s for '%s': skills/.usage.json is damaged or not "
                          "writable. Repair it or move it aside.", verb, key);
        json_t *r = cur_print_str(io, m); free(m); free(key);
        if (cur_is_raise(r)) return r;
        return json_number(1);
    }
    char *m;
    if (locked) {
        m = cur_fmt("curator: locked '%s'. The agent can no longer change it: skill_manage, write_file, "
                    "patch and terminal refuse, in normal turns and the background passes alike, and the "
                    "curator won't archive it. It stays readable and auto-loadable, and you can still edit it "
                    "yourself. Unlock with `hermes curator unlock %s`.", key, key);
    } else {
        m = cur_fmt("curator: unlocked '%s'. The agent may edit it again.", key);
    }
    json_t *r = cur_print_str(io, m); free(m); free(key);
    if (cur_is_raise(r)) return r;
    return json_number(0);
}

/* PoP: cur_cmd_lock @ hermes_cli/curator.py:_cmd_lock */
static json_t *cur_cmd_lock(cur_io_t *io, const char *skill) {
    return cur_set_lock(io, skill, 1);
}

/* PoP: cur_cmd_unlock @ hermes_cli/curator.py:_cmd_unlock */
static json_t *cur_cmd_unlock(cur_io_t *io, const char *skill) {
    return cur_set_lock(io, skill, 0);
}

/* PoP: cur_as_user @ hermes_cli/curator.py:_as_user */
static json_t *cur_as_user(cur_io_t *io, const char *skill) {
    json_t *a = json_object();
    json_set(a, "actor", json_string("user"));
    json_t *tok = cur_call(io, "set_ledger_actor", a);
    if (cur_is_raise(tok)) return tok;
    a = json_object();
    json_set(a, "skill", json_string(skill));
    json_t *fnr = cur_call(io, "fn", a);
    /* finally: reset_ledger_actor(tok) runs even when fn raised. */
    json_t *ra = json_object();
    json_set(ra, "token", tok ? json_copy(tok) : json_null());
    json_t *reset = cur_call(io, "reset_ledger_actor", ra);
    if (cur_is_raise(reset)) {           /* a finally raise replaces the body raise */
        json_free(tok); json_free(fnr);
        return reset;
    }
    json_free(reset);
    if (cur_is_raise(fnr)) { json_free(tok); return fnr; }
    int ok = 0;
    const char *msg = "";
    if (fnr && fnr->type == JSON_ARRAY && json_len(fnr) >= 2) {
        ok = cur_truthy(json_get(fnr, 0));
        msg = cur_jstr(json_get(fnr, 1));
    }
    char *m = cur_fmt("curator: %s", msg);
    json_t *r = cur_print_str(io, m); free(m);
    json_free(tok); json_free(fnr);
    if (cur_is_raise(r)) return r;
    return json_number(ok ? 0 : 1);
}

/* PoP: cur_cmd_ledger @ hermes_cli/curator.py:_cmd_ledger */
static json_t *cur_cmd_ledger(cur_io_t *io, int compact, const char *skill, const json_t *limit) {
    json_t *r;
    char *m;
    if (compact) {
        json_t *cl = cur_call(io, "compact_ledger", json_object());
        if (cur_is_raise(cl)) return cl;
        json_t *gc = cur_call(io, "gc_blobs", json_object());
        if (cur_is_raise(gc)) { json_free(cl); return gc; }
        char *ens = cur_py_str(json_get(cl, 0));
        char *blos = cur_py_str(json_get(gc, 0));
        const json_t *bef = json_get(cl, 1);
        const json_t *aft = json_get(cl, 2);
        const json_t *fre = json_get(gc, 1);
        json_t *bmb = json_number((bef && bef->type == JSON_NUMBER ? bef->num_val : 0) / 1048576.0);
        json_t *amb = json_number((aft && aft->type == JSON_NUMBER ? aft->num_val : 0) / 1048576.0);
        json_t *fmb = json_number((fre && fre->type == JSON_NUMBER ? fre->num_val : 0) / 1048576.0);
        char *bms = cur_format(bmb, ".1f");
        char *ams = cur_format(amb, ".1f");
        char *fms = cur_format(fmb, ".1f");
        json_free(bmb); json_free(amb); json_free(fmb);
        m = cur_fmt("curator: ledger compacted " CUR_EMDASH " %s entries, %s MB " CUR_ARROW
                    " %s MB; %s unreferenced blob(s) removed (%s MB)", ens, bms, ams, blos, fms);
        free(ens); free(blos); free(bms); free(ams); free(fms);
        json_free(cl); json_free(gc);
        r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) return r;
        return json_number(0);
    }
    double lim = (cur_truthy(limit) && limit->type == JSON_NUMBER) ? limit->num_val : 20;
    json_t *la = json_object();
    json_set(la, "skill", skill ? json_string(skill) : json_null());
    json_set(la, "limit", json_number(lim));
    json_t *rows = cur_call(io, "list_entries", la);
    if (cur_is_raise(rows)) return rows;
    if (!cur_truthy(rows)) {
        json_free(rows);
        r = cur_print_str(io, "curator: ledger is empty (or skills.ledger is disabled).");
        if (cur_is_raise(r)) return r;
        return json_number(0);
    }
    json_t *n_id = json_string("id"); char *f_id = cur_format(n_id, "<14"); json_free(n_id);
    json_t *n_when = json_string("when"); char *f_when = cur_format(n_when, "<12"); json_free(n_when);
    json_t *n_actor = json_string("actor"); char *f_actor = cur_format(n_actor, "<8"); json_free(n_actor);
    json_t *n_action = json_string("action"); char *f_action = cur_format(n_action, "<12"); json_free(n_action);
    m = cur_fmt("%s %s %s %s skill", f_id, f_when, f_actor, f_action);
    free(f_id); free(f_when); free(f_actor); free(f_action);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(rows); return r; }
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        const json_t *ev = json_obj_get(row, "evidence");
        char *extra = strdup("");
        if (cur_truthy(ev)) {
            const json_t *ai = json_obj_get(ev, "absorbed_into");
            const json_t *rt = json_obj_get(ev, "rollback_target");
            if (cur_truthy(ai)) {
                free(extra);
                extra = cur_fmt("  " CUR_ARROW " absorbed into '%s'", cur_jstr(ai));
            } else if (cur_truthy(rt)) {
                free(extra);
                extra = cur_fmt("  " CUR_ARROW " rollback of %s", cur_jstr(rt));
            }
        }
        const json_t *idn = json_obj_get(row, "id");
        json_t *idval = idn ? json_copy(idn) : json_string("?");
        char *rid = cur_format(idval, "<14"); json_free(idval);
        const json_t *tsn = json_obj_get(row, "ts");
        json_t *fts = cur_fmt_ts_seam(io, tsn);
        if (cur_is_raise(fts)) { free(extra); free(rid); json_free(rows); return fts; }
        char *rwhen = cur_format(fts, "<12"); json_free(fts);
        const json_t *actn = json_obj_get(row, "actor");
        json_t *actval = actn ? json_copy(actn) : json_string("?");
        char *ractor = cur_format(actval, "<8"); json_free(actval);
        const json_t *actionn = json_obj_get(row, "action");
        json_t *actionval = actionn ? json_copy(actionn) : json_string("?");
        char *raction = cur_format(actionval, "<12"); json_free(actionval);
        const json_t *skn = json_obj_get(row, "skill");
        char *sk = skn ? cur_py_str(skn) : strdup("?");
        m = cur_fmt("%s %s %s %s %s%s", rid, rwhen, ractor, raction, sk, extra);
        free(rid); free(rwhen); free(ractor); free(raction); free(sk); free(extra);
        r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) { json_free(rows); return r; }
    }
    json_free(rows);
    r = cur_print_str(io, "\nRoll back a single mutation with `hermes curator rollback <id>`; "
                          "whole-tree snapshots remain available via `hermes curator rollback --list`.");
    if (cur_is_raise(r)) return r;
    return json_number(0);
}

/* PoP: cur_rollback_ledger_entry @ hermes_cli/curator.py:_rollback_ledger_entry */
static json_t *cur_rollback_ledger_entry(cur_io_t *io, int yes, const char *entry_id) {
    json_t *a = json_object();
    json_set(a, "entry_id", json_string(entry_id));
    json_t *entry = cur_call(io, "get_entry", a);
    if (cur_is_raise(entry)) return entry;
    json_t *r;
    char *m;
    if (!cur_present(entry)) {
        m = cur_fmt("curator: no ledger entry '%s'. See `hermes curator ledger` for entry ids, or use "
                    "`--id <snapshot>` for whole-tree snapshot rollback.", entry_id);
        r = cur_print_str(io, m); free(m);
        json_free(entry);
        if (cur_is_raise(r)) return r;
        return json_number(1);
    }
    m = cur_fmt("Rollback target: ledger entry %s", entry_id);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(entry); return r; }
    const char *fields[4] = {"action", "skill", "actor", "ts"};
    const char *prefixes[4] = {"  action: ", "  skill:  ", "  actor:  ", "  when:   "};
    for (int k = 0; k < 4; k++) {
        const json_t *v = json_obj_get(entry, fields[k]);
        char *vs = v ? cur_py_str(v) : strdup("?");
        m = cur_fmt("%s%s", prefixes[k], vs); free(vs);
        r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) { json_free(entry); return r; }
    }
    const json_t *before = json_obj_get(entry, "before");
    const json_t *after = json_obj_get(entry, "after");
    json_t *seen = json_array();
    const json_t *bl[2] = {before, after};
    for (int L = 0; L < 2; L++) {
        const json_t *lst = bl[L];
        if (!lst || lst->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < json_len(lst); j++) {
            const json_t *item = json_get(lst, j);
            const json_t *p = item ? json_obj_get(item, "path") : NULL;
            const char *ps = cur_jstr(p);
            int found = 0;
            for (size_t k = 0; k < json_len(seen); k++) {
                if (strcmp(cur_jstr(json_get(seen, k)), ps) == 0) { found = 1; break; }
            }
            if (!found) json_append(seen, json_string(ps));
        }
    }
    int ntouched = (int)json_len(seen);
    json_free(seen);
    m = cur_fmt("  files:  %d", ntouched);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { json_free(entry); return r; }
    if (!yes) {
        json_t *cf = cur_confirm(io, "Restore this mutation's before-state? [y/N] ", "cancelled", "\n");
        if (cur_is_raise(cf)) { json_free(entry); return cf; }
        int confirmed = cur_truthy(cf); json_free(cf);
        if (!confirmed) { json_free(entry); return json_number(1); }
    }
    a = json_object();
    json_set(a, "entry_id", json_string(entry_id));
    json_t *rb = cur_call(io, "rollback_entry", a);
    if (cur_is_raise(rb)) { json_free(entry); return rb; }
    int ok = 0;
    const char *msg = "";
    if (rb && rb->type == JSON_ARRAY && json_len(rb) >= 2) {
        ok = cur_truthy(json_get(rb, 0));
        msg = cur_jstr(json_get(rb, 1));
    }
    json_free(entry);
    r = cur_report_rollback(io, ok, msg);
    json_free(rb);
    return r;
}
/* ── sorted() over path strings (same-depth domain: string sort == parts sort) ── */

static int cur_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static json_t *cur_sorted_strs(const json_t *arr) {
    json_t *out = json_array();
    size_t n = arr ? json_len(arr) : 0;
    if (n == 0) return out;
    char **tmp = (char **)malloc(n * sizeof(char *));
    if (!tmp) return out;
    for (size_t i = 0; i < n; i++) tmp[i] = strdup(cur_jstr(json_get(arr, i)));
    qsort(tmp, n, sizeof(char *), cur_cmp_str);
    for (size_t i = 0; i < n; i++) {
        json_append(out, json_string(tmp[i] ? tmp[i] : ""));
        free(tmp[i]);
    }
    free(tmp);
    return out;
}

/* PoP: cur_archived_ts @ hermes_cli/curator.py:_archived_ts */
static json_t *cur_archived_ts(cur_io_t *io, const char *path, const json_t *usage) {
    char *skillmd = cur_path_join(path, "SKILL.md");
    char *pname = cur_path_name(path);
    json_t *ra = json_object();
    json_set(ra, "path", json_string(skillmd ? skillmd : ""));
    json_set(ra, "fallback", json_string(pname ? pname : ""));
    free(skillmd); free(pname);
    json_t *namej = cur_call(io, "read_skill_name", ra);
    if (cur_is_raise(namej)) return namej;
    const json_t *rec = NULL;
    if (namej && namej->type == JSON_STRING) rec = json_obj_get(usage, namej->str_val);
    json_free(namej);
    if (!cur_truthy(rec)) rec = NULL;              /* usage.get(name) or {} */
    const char *state_archived = cur_env_str(io, "STATE_ARCHIVED", "archived");
    const json_t *st = rec ? json_obj_get(rec, "state") : NULL;
    int archived = (st && st->type == JSON_STRING && strcmp(st->str_val, state_archived) == 0);
    json_t *at = NULL;
    if (archived) {
        const json_t *aa = rec ? json_obj_get(rec, "archived_at") : NULL;
        json_t *pa = json_object();
        json_set(pa, "value", aa ? json_copy(aa) : json_null());
        at = cur_call(io, "_parse_iso_timestamp", pa);
        if (cur_is_raise(at)) return at;
    }
    json_t *sa = json_object();
    json_set(sa, "path", json_string(path ? path : ""));
    json_t *stat = cur_call(io, "stat", sa);
    if (cur_is_raise(stat)) { json_free(at); return stat; }
    double mtime = json_get_num(stat, "st_mtime", 0);
    json_free(stat);
    double result;
    if (cur_truthy(at)) {
        double ts = json_get_num(at, "ts", 0);
        result = (ts > mtime) ? ts : mtime;
    } else {
        result = mtime;
    }
    json_free(at);
    return json_number(result);
}

/* PoP: cur_cmd_purge @ hermes_cli/curator.py:_cmd_purge */
static json_t *cur_cmd_purge(cur_io_t *io, const json_t *days, int dry_run, int yes) {
    json_t *ret = NULL, *r = NULL;
    char *m = NULL;
    char *archive_root = NULL;
    json_t *usage = NULL, *candidates = NULL;
    long long ttl_days;

    if (!cur_present(days)) {
        json_t *lc = cur_call(io, "load_config", json_object());
        if (cur_is_raise(lc)) { ret = lc; goto done; }
        json_t *ca = json_object();
        json_set(ca, "config", lc ? json_copy(lc) : json_null());
        json_t *keys = json_array();
        json_append(keys, json_string("curator"));
        json_append(keys, json_string("archive_ttl_days"));
        json_set(ca, "keys", keys);
        json_set(ca, "default", json_number(0));
        json_t *cg = cur_call(io, "cfg_get", ca);
        if (cur_is_raise(cg)) { json_free(lc); ret = cg; goto done; }
        ttl_days = cur_to_int_or_zero(cg);
        json_free(cg); json_free(lc);
    } else {
        ttl_days = (long long)(days->type == JSON_NUMBER ? days->num_val : 0);
    }
    if (ttl_days <= 0) {
        r = cur_print_str(io, "curator: purge disabled (curator.archive_ttl_days is 0). Set the "
                              "config key or pass --days N to purge archives older than N days.");
        if (cur_is_raise(r)) { ret = r; goto done; }
        ret = json_number(1); goto done;
    }
    json_t *ar = cur_call(io, "_archive_dir", json_object());
    if (cur_is_raise(ar)) { ret = ar; goto done; }
    archive_root = strdup(cur_jstr(ar));
    json_free(ar);
    json_t *ea = json_object();
    json_set(ea, "path", json_string(archive_root));
    json_t *ex = cur_call(io, "exists", ea);
    if (cur_is_raise(ex)) { ret = ex; goto done; }
    int exists = cur_truthy(ex); json_free(ex);
    if (!exists) {
        r = cur_print_str(io, "curator: no archive directory " CUR_EMDASH " nothing to purge.");
        if (cur_is_raise(r)) { ret = r; goto done; }
        ret = json_number(0); goto done;
    }
    json_t *tj = cur_call(io, "time", json_object());
    if (cur_is_raise(tj)) { ret = tj; goto done; }
    double now = (tj && tj->type == JSON_NUMBER) ? tj->num_val : 0;
    json_free(tj);
    double cutoff = now - (double)ttl_days * 86400.0;
    usage = cur_call(io, "load_usage", json_object());
    if (cur_is_raise(usage)) { ret = usage; usage = NULL; goto done; }
    json_t *ia = json_object();
    json_set(ia, "path", json_string(archive_root));
    json_t *children = cur_call(io, "iterdir", ia);
    if (cur_is_raise(children)) { ret = children; goto done; }
    candidates = json_array();
    if (children && children->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(children); i++) {
            char *p = strdup(cur_jstr(json_get(children, i)));
            json_t *da = json_object();
            json_set(da, "path", json_string(p));
            json_t *dj = cur_call(io, "is_dir", da);
            if (cur_is_raise(dj)) { free(p); ret = dj; goto done; }
            int isdir = cur_truthy(dj); json_free(dj);
            if (!isdir) { free(p); continue; }
            json_t *ats = cur_archived_ts(io, p, usage);
            if (cur_is_raise(ats)) { free(p); ret = ats; goto done; }
            double ts = (ats && ats->type == JSON_NUMBER) ? ats->num_val : 0;
            json_free(ats);
            if (ts < cutoff) json_append(candidates, json_string(p));
            free(p);
        }
    }
    json_free(children);
    {
        json_t *sorted = cur_sorted_strs(candidates);
        json_free(candidates);
        candidates = sorted;
    }
    if (json_len(candidates) == 0) {
        m = cur_fmt("curator: no archived skills older than %lldd.", ttl_days);
        r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) { ret = r; goto done; }
        ret = json_number(0); goto done;
    }
    m = cur_fmt("Archived skills older than %lldd:", ttl_days);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { ret = r; goto done; }
    for (size_t i = 0; i < json_len(candidates); i++) {
        char *pname = cur_path_name(cur_jstr(json_get(candidates, i)));
        m = cur_fmt("  %s", pname ? pname : ""); free(pname);
        r = cur_print_str(io, m); free(m);
        if (cur_is_raise(r)) { ret = r; goto done; }
    }
    if (dry_run) {
        r = cur_print_str(io, "(dry run " CUR_EMDASH " nothing deleted)");
        if (cur_is_raise(r)) { ret = r; goto done; }
        ret = json_number(0); goto done;
    }
    if (!yes) {
        char *prompt = cur_fmt("Permanently delete %d archived skill(s)? [y/N] ",
                               (int)json_len(candidates));
        json_t *cf = cur_confirm(io, prompt, "cancelled", "\n");
        free(prompt);
        if (cur_is_raise(cf)) { ret = cf; goto done; }
        int confirmed = cur_truthy(cf); json_free(cf);
        if (!confirmed) { ret = json_number(1); goto done; }
    }
    int purged = 0;
    for (size_t i = 0; i < json_len(candidates); i++) {
        const char *p = cur_jstr(json_get(candidates, i));
        char *pname = cur_path_name(p);
        json_t *cba = json_object();
        json_set(cba, "path", json_string(p));
        json_set(cba, "complete_package", json_bool(true));
        json_set(cba, "skill", json_string(pname ? pname : ""));
        json_t *before = cur_call(io, "capture_before", cba);
        if (cur_is_raise(before)) { free(pname); ret = before; goto done; }
        json_t *rma = json_object();
        json_set(rma, "path", json_string(p));
        json_t *rm = cur_call(io, "rmtree", rma);
        if (cur_is_raise(rm)) {
            if (cur_is_oserror(cur_raise_kind(rm))) {
                char *em = cur_fmt("curator: failed to purge %s: %s",
                                   pname ? pname : "", cur_raise_msg(rm));
                json_free(rm);
                json_t *pr = cur_print_str(io, em); free(em);
                json_free(before); free(pname);
                if (cur_is_raise(pr)) { ret = pr; goto done; }
                continue;
            }
            free(pname); json_free(before); ret = rm; goto done;
        }
        json_free(rm);
        json_t *aea = json_object();
        json_set(aea, "action", json_string("purge"));
        json_set(aea, "skill", json_string(pname ? pname : ""));
        json_set(aea, "before", cur_truthy(before) ? json_copy(before) : json_array());
        json_set(aea, "after", json_array());
        json_set(aea, "actor", json_string("user"));
        json_t *ev = json_object();
        json_set(ev, "ttl_days", json_number((double)ttl_days));
        json_set(aea, "evidence", ev);
        json_t *ae = cur_call(io, "append_entry", aea);
        json_free(before); free(pname);
        if (cur_is_raise(ae)) { ret = ae; goto done; }
        json_free(ae);
        purged++;
    }
    m = cur_fmt("curator: purged %d archived skill(s). Ledger entries recorded.", purged);
    r = cur_print_str(io, m); free(m);
    if (cur_is_raise(r)) { ret = r; goto done; }
    ret = json_number(0);
done:
    free(archive_root);
    json_free(usage);
    json_free(candidates);
    return ret;
}