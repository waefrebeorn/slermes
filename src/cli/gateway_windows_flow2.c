/*
 * gateway_windows_flow2.c — FLOW port of the UNCLAIMED remainder of
 * hermes_cli/gateway_windows.py (LIVE authority), second slice after
 * port_gateway_windows_flow.c. Self-contained: libjson only. All functions
 * static gw2_*; the harness (#include "cli/gateway_windows_flow2.c") wires
 * the universal seam to scripted results and records the ordered trace.
 *
 * Covered flows (PoP per function):
 *   start-attestation lifecycle: _attested_pids_from, _attestation_generation,
 *     _attested_create_time, _read_start_attestation, _clear_start_attestation,
 *     _attestation_within_horizon, _write_start_attestation,
 *     check_start_attestation, _format_attestation_warning, _task_run_hint,
 *     _print_task_run_hint, _print_start_attestation_warning
 *   scheduled-task XML drift: _query_scheduled_task_xml, _task_xml_leaf_values,
 *     compare_scheduled_task_drift, scheduled_task_drift,
 *     _print_scheduled_task_drift, registered_task_launchers
 *   launcher rendering / atomic write: _launcher_owned_exit_line,
 *     _launcher_pythonpath_entries, _render_launchers, _write_launcher_scripts,
 *     _atomic_write
 *   report/path helpers: _report_already_running, _startup_staging_path,
 *     _hermes_home, _mark_planned_restart
 *
 * SEAM DOCTRINE (same as port_gateway_windows_flow.c): every live effect
 * enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it
 * exactly where the live try/except catches it. except Exception does NOT
 * catch KeyboardInterrupt/SystemExit/GeneratorExit/BaseException — those pass
 * through to the harness ret envelope (BaseException passthrough). A MISSING
 * seam key fail-closes: the harness delivers {"__raise__":"RuntimeError",
 * "msg":"unscripted seam: <name>"}; a present JSON null is a valid None.
 * Claimed siblings (_start_attestation_path, _attested_dead, _exec_schtasks,
 * _build_scheduled_task_xml, _resolve_task_user, get_task_script_path,
 * get_startup_entry_path, is_task_registered, get_task_name,
 * _quote_vbs_string, _preserve_hermes_home_path, _build_gateway_cmd_script,
 * _build_gateway_vbs_script) and foreign collaborators (project_root_str,
 * get_hermes_home, _process_create_time, find_gateway_pids,
 * service_definition_launchers, launcher_pid_file,
 * write_planned_restart_marker, datetime.now, uuid4, time.time) are SEAMED;
 * in-function imports fire an "import_<name>" seam on module attribute
 * access, mirroring the oracle's stub modules.
 *
 * Seam name table (name -> live call, args):
 *   print                          print(text)
 *   start_attestation_path         _start_attestation_path(home) -> posix str
 *   path_mkdir                     Path(path).mkdir(parents,exist_ok)
 *   path_read_text                 Path(path).read_text(encoding) (may raise)
 *   path_write_text                Path(path).write_text(content,encoding[,newline])
 *   path_replace                   Path(src).replace(dst)
 *   path_unlink                    Path(path).unlink(missing_ok)
 *   path_exists                    Path(path).exists()
 *   import_process_create_time     from hermes_cli.process_identity import _process_create_time
 *   process_create_time            _process_create_time(pid) -> number|null
 *   datetime_now                   datetime.now(UTC).isoformat() -> str
 *   uuid4_hex                      uuid.uuid4().hex -> str
 *   time_time                      time.time() -> number
 *   import_find_gateway_pids       from hermes_cli.gateway import find_gateway_pids
 *   find_gateway_pids              find_gateway_pids() -> [int]
 *   attested_dead                  _attested_dead(attested,current_pids,data,home) -> bool
 *   is_task_registered             is_task_registered() -> bool
 *   get_task_name                  get_task_name() -> str
 *   exec_schtasks                  _exec_schtasks(argv,timeout) -> {code,out,err}
 *   import_service_definition_launchers  from hermes_cli.gateway_service_owner import ...
 *   service_definition_launchers   service_definition_launchers(definition) -> [str]
 *   get_task_script_path           get_task_script_path() -> posix str
 *   build_scheduled_task_xml       _build_scheduled_task_xml(task_name,script_path,user) -> str
 *   resolve_task_user              _resolve_task_user() -> str|null
 *   import_launcher_pid_file       from hermes_cli.gateway_runtime_service_identity import launcher_pid_file
 *   launcher_pid_file              launcher_pid_file(hermes_home) -> str
 *   quote_vbs_string               _quote_vbs_string(value) -> str
 *   project_root_str               project_root_str() -> str
 *   preserve_hermes_home_path      _preserve_hermes_home_path(value) -> str
 *   build_gateway_cmd_script       _build_gateway_cmd_script(python_path,working_dir,hermes_home,profile_arg)
 *   build_gateway_vbs_script       _build_gateway_vbs_script(same 4)
 *   import_get_hermes_home         from hermes_cli.config import get_hermes_home
 *   get_hermes_home                get_hermes_home() -> str
 *   import_write_planned_restart_marker  from gateway.restart_notice import ...
 *   write_planned_restart_marker   write_planned_restart_marker()
 *
 * Fixture domains (documented exclusions):
 * - Paths are POSIX strings; the path algebra models pathlib PurePosixPath
 *   name/parent/with_suffix/with_name on that domain.
 * - JSON numbers have no int/float identity in libjson: integral-float
 *   literals (5.0) are outside the fixture corpus where live distinguishes
 *   type(x) is int (_attested_pids_from) or json.dumps renders ".0"
 *   (create_times values are integral ints or null).
 * - ISO timestamps: extended YYYY-MM-DD[<sep>HH:MM[:SS[.frac]]][Z|+-HH[:MM]]
 *   (any single separator char, hour 24 only at exact midnight, frac
 *   truncated to 6 digits, offsets strictly < 24h). Basic format
 *   (20261118T123456), week dates (2026-W47-3) and fractional hours/minutes
 *   are excluded.
 * - XML: elements, quoted attributes, the 5 named entities + numeric char
 *   refs, comments, CDATA, PIs, XML declaration, DOCTYPE (no internal-subset
 *   entity expansion), BOM, default/prefixed namespaces. ASCII tags (bytes
 *   >= 0x80 accepted as name chars). ParseError messages are unobservable
 *   (the live helper returns None), so only accept/reject parity is ported.
 * - str.strip()/split() whitespace: ASCII set (inputs are ASCII-scoped).
 * - int() coercion: JSON numbers/bools/decimal strings (no underscores).
 * - logger is silent (no trace events), matching port_gateway_windows_flow.c.
 *
 * Ownership: seam results and returned json_t / char pointers are malloc'd,
 * caller frees. Inputs are borrowed. A propagated raise dict is returned
 * upward as-is; a caught raise dict is freed at the catch site.
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
#include "json.h"

/* ── universal seam + trace ── */

typedef struct gw2_trace { json_t *events; } gw2_trace_t;

typedef struct gw2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gw2_seams_t;

static void gw2_trace_add(gw2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void gw2_trace_free(gw2_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *gw2_call(const gw2_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

/* ── JSON value helpers ── */

static bool gw2_jtruth(const json_t *v) {
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
static const char *gw2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool gw2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static char *gw2_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* growable byte buffer */
typedef struct { char *p; size_t len, cap; } gw2_buf;
static void gw2_binit(gw2_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void gw2_bfree(gw2_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void gw2_bput(gw2_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void gw2_bputs(gw2_buf *b, const char *s) { gw2_bput(b, s, strlen(s)); }
static void gw2_bputc(gw2_buf *b, char c) { gw2_bput(b, &c, 1); }
static char *gw2_btake(gw2_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* ── exception model ── */

static const char *gw2_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool gw2_is_raise(const json_t *r) { return gw2_raise_class(r) != NULL; }

static json_t *gw2_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

/* BaseException-only classes: never caught by except Exception/OSError/... */
static bool gw2_base_only(const char *k) {
    return !strcmp(k, "BaseException") || !strcmp(k, "KeyboardInterrupt") ||
           !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit");
}
static bool gw2_is_exception(const json_t *r) {
    const char *k = gw2_raise_class(r);
    return k && !gw2_base_only(k);
}
static bool gw2_in_family(const char *k, const char *const *fam) {
    for (size_t i = 0; fam[i]; i++) if (!strcmp(k, fam[i])) return true;
    return false;
}
static const char *const GW2_OSERROR_FAMILY[] = {
    "OSError", "FileNotFoundError", "PermissionError", "FileExistsError",
    "IsADirectoryError", "NotADirectoryError", "TimeoutError", "InterruptedError",
    "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
    "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError", NULL
};
static const char *const GW2_VALUEERROR_FAMILY[] = {
    "ValueError", "UnicodeDecodeError", "UnicodeEncodeError", "JSONDecodeError", NULL
};
static const char *const GW2_IMPORTERROR_FAMILY[] = {
    "ImportError", "ModuleNotFoundError", NULL
};
/* caught(r, cls): exact class or its modeled subclass family */
static bool gw2_caught(const json_t *r, const char *cls) {
    const char *k = gw2_raise_class(r);
    if (!k) return false;
    if (!strcmp(k, cls)) return true;
    if (!strcmp(cls, "OSError")) return gw2_in_family(k, GW2_OSERROR_FAMILY);
    if (!strcmp(cls, "ValueError")) return gw2_in_family(k, GW2_VALUEERROR_FAMILY);
    if (!strcmp(cls, "ImportError")) return gw2_in_family(k, GW2_IMPORTERROR_FAMILY);
    if (!strcmp(cls, "Exception")) return !gw2_base_only(k);
    return false;
}

/* Python type name of a JSON value (integral numbers read as int) */
static const char *gw2_typename(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == floor(v->num_val) && isfinite(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* ── args builders ── */
static json_t *gw2_args(void) { return json_object(); }
static void gw2_aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void gw2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gw2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void gw2_aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── Python float repr (shortest round-trip, repr formatting rules) ── */
static char *gw2_float_repr(double v) {
    if (v == 0.0) return strdup("0.0");
    if (!isfinite(v)) return strdup(v > 0 ? "inf" : (isnan(v) ? "nan" : "-inf"));
    char tmp[64];
    int prec;
    for (prec = 0; prec <= 16; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*e", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    if (prec > 16) { prec = 16; snprintf(tmp, sizeof(tmp), "%.*e", prec, v); }
    /* tmp = [-]d[.ddd]e[+-]XX */
    const char *p = tmp;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    char digits[32];
    size_t nd = 0;
    digits[nd++] = *p++;
    if (*p == '.') { p++; while (*p && *p != 'e' && *p != 'E') digits[nd++] = *p++; }
    digits[nd] = '\0';
    int exp = 0;
    if (*p == 'e' || *p == 'E') exp = atoi(p + 1);
    /* strip trailing zeros of the digit string (keep >= 1 digit) */
    while (nd > 1 && digits[nd - 1] == '0') digits[--nd] = '\0';
    gw2_buf b; gw2_binit(&b);
    if (neg) gw2_bputc(&b, '-');
    if (exp >= -4 && exp < 16) {
        if (exp >= 0) {
            size_t point = (size_t)exp + 1;
            if (nd <= point) {
                gw2_bput(&b, digits, nd);
                for (size_t i = nd; i < point; i++) gw2_bputc(&b, '0');
                gw2_bputs(&b, ".0");
            } else {
                gw2_bput(&b, digits, point);
                gw2_bputc(&b, '.');
                gw2_bputs(&b, digits + point);
            }
        } else {
            gw2_bputs(&b, "0.");
            for (int i = 0; i < -exp - 1; i++) gw2_bputc(&b, '0');
            gw2_bputs(&b, digits);
        }
    } else {
        gw2_bputc(&b, digits[0]);
        if (nd > 1) { gw2_bputc(&b, '.'); gw2_bputs(&b, digits + 1); }
        char ex[16];
        snprintf(ex, sizeof(ex), "e%s%02d", exp < 0 ? "-" : "+", exp < 0 ? -exp : exp);
        gw2_bputs(&b, ex);
    }
    return gw2_btake(&b);
}

/* Python repr of a string (quote choice + escapes) */
static char *gw2_repr_string(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    gw2_buf b; gw2_binit(&b);
    gw2_bputc(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { gw2_bputc(&b, '\\'); gw2_bputc(&b, (char)c); }
        else if (c == '\n') gw2_bputs(&b, "\\n");
        else if (c == '\r') gw2_bputs(&b, "\\r");
        else if (c == '\t') gw2_bputs(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char x[8]; snprintf(x, sizeof(x), "\\x%02x", c); gw2_bputs(&b, x); }
        else gw2_bputc(&b, (char)c);
    }
    gw2_bputc(&b, q);
    return gw2_btake(&b);
}

static char *gw2_pyrepr(const json_t *v);
/* Python str() of a JSON value */
static char *gw2_pystr(const json_t *v) {
    if (gw2_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return gw2_xsf("%.0f", d);
            }
            return gw2_float_repr(d);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: return gw2_pyrepr(v);
    }
}
static char *gw2_pyrepr(const json_t *v) {
    if (gw2_is_null(v)) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) return strdup("0");
                return gw2_xsf("%.0f", d);
            }
            return gw2_float_repr(d);
        }
        case JSON_STRING: return gw2_repr_string(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            gw2_buf b; gw2_binit(&b);
            gw2_bputc(&b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gw2_bputs(&b, ", ");
                char *r = gw2_pyrepr(v->c.items[i]);
                gw2_bputs(&b, r); free(r);
            }
            gw2_bputc(&b, ']');
            return gw2_btake(&b);
        }
        case JSON_OBJECT: {
            gw2_buf b; gw2_binit(&b);
            gw2_bputc(&b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gw2_bputs(&b, ", ");
                char *k = gw2_repr_string(v->c.keys[i]);
                char *r = gw2_pyrepr(v->c.items[i]);
                gw2_bputs(&b, k); gw2_bputs(&b, ": "); gw2_bputs(&b, r);
                free(k); free(r);
            }
            gw2_bputc(&b, '}');
            return gw2_btake(&b);
        }
        default: return strdup("None");
    }
}

/* int() coercion: number trunc toward zero, bool 0/1, decimal string.
 * Returns false where Python raises (ValueError/TypeError). */
static bool gw2_pyint(const json_t *v, double *out) {
    if (!v) return false;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return false;
        *out = d >= 0 ? floor(d) : ceil(d);
        return true;
    }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        /* int() strips White_Space minus \x1c-\x1f; ASCII-scoped set */
        while (*s == ' ' || (*s >= 0x09 && *s <= 0x0d)) s++;
        const char *start = s;
        if (*s == '+' || *s == '-') s++;
        size_t nd = 0;
        while (*s >= '0' && *s <= '9') { s++; nd++; }
        if (!nd) return false;
        const char *end = s;
        while (*s == ' ' || (*s >= 0x09 && *s <= 0x0d)) s++;
        if (*s) return false;
        char *tmp = strndup(start, (size_t)(end - start));
        *out = strtod(tmp, NULL);
        free(tmp);
        return true;
    }
    return false;
}

/* ── POSIX path algebra (pathlib PurePosixPath component model) ── */

/* str(PurePosixPath(p)): drop empty and "." components, keep "..", collapse
 * slash runs (POSIX keeps EXACTLY two leading slashes), strip trailing slash */
static char *gw2_path_norm(const char *p) {
    size_t lead = 0;
    while (p[lead] == '/') lead++;
    bool absolute = lead > 0;
    const char *root = (lead == 2) ? "//" : "/";
    gw2_buf b; gw2_binit(&b);
    if (absolute) gw2_bputs(&b, root);
    size_t i = lead, n = strlen(p);
    bool any = false;
    while (i < n) {
        while (i < n && p[i] == '/') i++;
        size_t start = i;
        while (i < n && p[i] != '/') i++;
        size_t len = i - start;
        if (len == 0) continue;
        if (len == 1 && p[start] == '.') continue;
        if (any) gw2_bputc(&b, '/');
        gw2_bput(&b, p + start, len);
        any = true;
    }
    if (!any) { gw2_bfree(&b); return strdup(absolute ? root : "."); }
    return gw2_btake(&b);
}
static bool gw2_path_is_root(const char *n) {
    return n[0] == '/' && n[strspn(n, "/")] == '\0';
}
static char *gw2_path_name(const char *p) {
    char *n = gw2_path_norm(p);
    if (gw2_path_is_root(n) || !strcmp(n, ".")) { free(n); return strdup(""); }
    const char *slash = strrchr(n, '/');
    char *out = strdup(slash ? slash + 1 : n);
    free(n);
    return out;
}
static char *gw2_path_parent(const char *p) {
    char *n = gw2_path_norm(p);
    if (gw2_path_is_root(n)) return n;          /* root is its own parent */
    if (!strcmp(n, ".")) { free(n); return strdup("."); }
    const char *slash = strrchr(n, '/');
    char *out;
    if (!slash) out = strdup(".");
    else {
        size_t k = (size_t)(slash - n);
        bool prefix_root = true;
        for (size_t j = 0; j < k; j++) if (n[j] != '/') { prefix_root = false; break; }
        if (prefix_root) out = strdup((n[0] == '/' && n[1] == '/') ? "//" : "/");
        else out = strndup(n, k);
    }
    free(n);
    return out;
}
static char *gw2_path_join(const char *parent, const char *name) {
    if (!strcmp(parent, ".")) return strdup(name);
    if (!strcmp(parent, "/")) return gw2_xsf("/%s", name);
    return gw2_xsf("%s/%s", parent, name);
}
/* PureWindowsPath.as_posix() keeps a trailing '/' on a UNC share root
 * (//server/share). with_suffix can produce one, e.g. //server/f.txt ->
 * //server/f.vbs, so add the slash the LIVE path stringification has. */
static char *gw2_unc_share_posix(char *out) {
    if (!out) return out;
    size_t n = strlen(out);
    if (n < 4 || out[0] != '/' || out[1] != '/' || out[2] == '/') return out;
    const char *slash = strchr(out + 2, '/');
    if (slash && slash != out + 2 && slash[1]
        && !strchr(slash + 1, '/') && out[n - 1] != '/') {
        char *bigger = realloc(out, n + 2);
        if (bigger) { out = bigger; out[n] = '/'; out[n + 1] = '\0'; }
    }
    return out;
}
/* with_suffix: returns NULL on ValueError (empty name / invalid suffix).
 * suffix scan: last dot at index > 0, unless the name is all dots (".."). */
static char *gw2_with_suffix(const char *p, const char *suffix) {
    char *name = gw2_path_name(p);
    if (!name[0]) { free(name); return NULL; }
    if (suffix[0] && suffix[0] != '.') { free(name); return NULL; }
    const char *dot = strrchr(name, '.');
    bool all_dots = name[strspn(name, ".")] == '\0';
    char *stem = (dot && (dot - name) > 0 && !all_dots)
                     ? strndup(name, (size_t)(dot - name)) : strdup(name);
    char *joined = gw2_xsf("%s%s", stem, suffix);
    char *parent = gw2_path_parent(p);
    char *out = gw2_unc_share_posix(gw2_path_join(parent, joined));
    free(name); free(stem); free(joined); free(parent);
    return out;
}
static char *gw2_with_name(const char *p, const char *name) {
    if (!name[0] || strchr(name, '/') || !strcmp(name, ".")) return NULL;
    char *norm = gw2_path_norm(p);
    bool empty_name = gw2_path_is_root(norm) || !strcmp(norm, ".");
    free(norm);
    if (empty_name) return NULL;
    char *parent = gw2_path_parent(p);
    char *out = gw2_path_join(parent, name);
    free(parent);
    return out;
}

/* ── strict JSON grammar pre-validator (libjson json_parse is lenient) ── */

typedef struct { const char *s; size_t pos, len; int depth; bool ok; } gw2_jv;
static void gw2_jv_ws(gw2_jv *v) {
    while (v->pos < v->len) {
        char c = v->s[v->pos];
        if (c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d) v->pos++;
        else break;
    }
}
static bool gw2_jv_hex4(gw2_jv *v) {
    if (v->pos + 4 > v->len) return false;
    for (int i = 0; i < 4; i++) {
        char c = v->s[v->pos + i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    v->pos += 4;
    return true;
}
static bool gw2_jv_string(gw2_jv *v) {
    if (v->pos >= v->len || v->s[v->pos] != '"') return false;
    v->pos++;
    while (v->pos < v->len) {
        unsigned char c = (unsigned char)v->s[v->pos];
        if (c == '"') { v->pos++; return true; }
        if (c < 0x20) return false;
        if (c == '\\') {
            v->pos++;
            if (v->pos >= v->len) return false;
            char e = v->s[v->pos];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't') v->pos++;
            else if (e == 'u') { v->pos++; if (!gw2_jv_hex4(v)) return false; }
            else return false;
        } else v->pos++;
    }
    return false;
}
static bool gw2_jv_number(gw2_jv *v) {
    size_t start = v->pos;
    if (v->pos < v->len && v->s[v->pos] == '-') v->pos++;
    if (v->pos >= v->len) return false;
    if (v->s[v->pos] == '0') v->pos++;
    else if (v->s[v->pos] >= '1' && v->s[v->pos] <= '9') {
        while (v->pos < v->len && v->s[v->pos] >= '0' && v->s[v->pos] <= '9') v->pos++;
    } else return false;
    if (v->pos < v->len && v->s[v->pos] == '.') {
        v->pos++;
        size_t d = v->pos;
        while (v->pos < v->len && v->s[v->pos] >= '0' && v->s[v->pos] <= '9') v->pos++;
        if (v->pos == d) return false;
    }
    if (v->pos < v->len && (v->s[v->pos] == 'e' || v->s[v->pos] == 'E')) {
        v->pos++;
        if (v->pos < v->len && (v->s[v->pos] == '+' || v->s[v->pos] == '-')) v->pos++;
        size_t d = v->pos;
        while (v->pos < v->len && v->s[v->pos] >= '0' && v->s[v->pos] <= '9') v->pos++;
        if (v->pos == d) return false;
    }
    return v->pos > start;
}
static bool gw2_jv_value(gw2_jv *v) {
    if (++v->depth > 200) { v->ok = false; return false; }
    gw2_jv_ws(v);
    bool r = false;
    if (v->pos >= v->len) { v->depth--; return false; }
    char c = v->s[v->pos];
    if (c == '"') r = gw2_jv_string(v);
    else if (c == '{') {
        v->pos++;
        gw2_jv_ws(v);
        if (v->pos < v->len && v->s[v->pos] == '}') { v->pos++; r = true; }
        else {
            r = true;
            for (;;) {
                gw2_jv_ws(v);
                if (!gw2_jv_string(v)) { r = false; break; }
                gw2_jv_ws(v);
                if (v->pos >= v->len || v->s[v->pos] != ':') { r = false; break; }
                v->pos++;
                if (!gw2_jv_value(v)) { r = false; break; }
                gw2_jv_ws(v);
                if (v->pos < v->len && v->s[v->pos] == ',') { v->pos++; continue; }
                if (v->pos < v->len && v->s[v->pos] == '}') { v->pos++; break; }
                r = false; break;
            }
        }
    } else if (c == '[') {
        v->pos++;
        gw2_jv_ws(v);
        if (v->pos < v->len && v->s[v->pos] == ']') { v->pos++; r = true; }
        else {
            r = true;
            for (;;) {
                if (!gw2_jv_value(v)) { r = false; break; }
                gw2_jv_ws(v);
                if (v->pos < v->len && v->s[v->pos] == ',') { v->pos++; continue; }
                if (v->pos < v->len && v->s[v->pos] == ']') { v->pos++; break; }
                r = false; break;
            }
        }
    } else if (c == 't') {
        r = (v->len - v->pos >= 4 && !strncmp(v->s + v->pos, "true", 4)); if (r) v->pos += 4;
    } else if (c == 'f') {
        r = (v->len - v->pos >= 5 && !strncmp(v->s + v->pos, "false", 5)); if (r) v->pos += 5;
    } else if (c == 'n') {
        r = (v->len - v->pos >= 4 && !strncmp(v->s + v->pos, "null", 4)); if (r) v->pos += 4;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        r = gw2_jv_number(v);
    }
    v->depth--;
    return r;
}
static bool gw2_json_strict_valid(const char *s) {
    gw2_jv v = {s, 0, strlen(s), 0, true};
    if (!gw2_jv_value(&v)) return false;
    gw2_jv_ws(&v);
    return v.pos == v.len;
}

/* ── Python json.dumps (default separators ", " / ": ", ensure_ascii) ── */

static void gw2_dumps_string(gw2_buf *b, const char *s) {
    gw2_bputc(b, '"');
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        if (c == '"') gw2_bputs(b, "\\\"");
        else if (c == '\\') gw2_bputs(b, "\\\\");
        else if (c == '\b') gw2_bputs(b, "\\b");
        else if (c == '\f') gw2_bputs(b, "\\f");
        else if (c == '\n') gw2_bputs(b, "\\n");
        else if (c == '\r') gw2_bputs(b, "\\r");
        else if (c == '\t') gw2_bputs(b, "\\t");
        else if (c < 0x20) { char x[8]; snprintf(x, sizeof(x), "\\u%04x", c); gw2_bputs(b, x); }
        else if (c < 0x80) { gw2_bputc(b, (char)c); p++; continue; }
        else {
            /* decode UTF-8 codepoint, emit \uXXXX (surrogate pair > 0xFFFF) */
            unsigned int cp = 0; int extra = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
            else { cp = 0xFFFD; extra = 0; }
            p++;
            for (int i = 0; i < extra; i++) {
                if ((*p & 0xC0) != 0x80) { cp = 0xFFFD; break; }
                cp = (cp << 6) | (*p & 0x3F);
                p++;
            }
            char x[16];
            if (cp > 0xFFFF) {
                unsigned int v2 = cp - 0x10000;
                snprintf(x, sizeof(x), "\\u%04x\\u%04x", 0xD800 + (v2 >> 10), 0xDC00 + (v2 & 0x3FF));
            } else snprintf(x, sizeof(x), "\\u%04x", cp);
            gw2_bputs(b, x);
            continue;
        }
        p++;
    }
    gw2_bputc(b, '"');
}
static void gw2_dumps_rec(gw2_buf *b, const json_t *v) {
    if (gw2_is_null(v)) { gw2_bputs(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: gw2_bputs(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 1e16) {
                if (d == 0.0) gw2_bputs(b, "0");
                else { char *s = gw2_xsf("%.0f", d); gw2_bputs(b, s); free(s); }
            } else { char *s = gw2_float_repr(d); gw2_bputs(b, s); free(s); }
            break;
        }
        case JSON_STRING: gw2_dumps_string(b, v->str_val ? v->str_val : ""); break;
        case JSON_ARRAY:
            gw2_bputc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gw2_bputs(b, ", ");
                gw2_dumps_rec(b, v->c.items[i]);
            }
            gw2_bputc(b, ']');
            break;
        case JSON_OBJECT:
            gw2_bputc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) gw2_bputs(b, ", ");
                gw2_dumps_string(b, v->c.keys[i]);
                gw2_bputs(b, ": ");
                gw2_dumps_rec(b, v->c.items[i]);
            }
            gw2_bputc(b, '}');
            break;
        default: gw2_bputs(b, "null"); break;
    }
}
static char *gw2_dumps(const json_t *v) {
    gw2_buf b; gw2_binit(&b);
    gw2_dumps_rec(&b, v);
    return gw2_btake(&b);
}

/* ── ISO-8601 extended parser -> epoch seconds (datetime.fromisoformat domain) ── */

static long gw2_days_from_civil(long y, long m, long d) {
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static bool gw2_iso_digits(const char *s, size_t n, long *out) {
    long v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return true;
}
static bool gw2_iso_epoch(const char *s, double *out) {
    size_t len = strlen(s);
    if (len < 10) return false;
    if (s[4] != '-' || s[7] != '-') return false;
    long y, mo, d;
    if (!gw2_iso_digits(s, 4, &y) || !gw2_iso_digits(s + 5, 2, &mo) || !gw2_iso_digits(s + 8, 2, &d))
        return false;
    if (y < 1 || y > 9999 || mo < 1 || mo > 12) return false;
    static const long dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long md = dim[mo - 1];
    if (mo == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) md = 29;
    if (d < 1 || d > md) return false;
    long h = 0, mi = 0, sec = 0;
    double frac = 0.0;
    long off_sec = 0;
    size_t pos = 10;
    if (pos < len) {
        pos++;  /* any single separator char */
        if (pos + 5 > len) return false;
        if (s[pos + 2] != ':') return false;
        if (!gw2_iso_digits(s + pos, 2, &h) || !gw2_iso_digits(s + pos + 3, 2, &mi)) return false;
        pos += 5;
        if (h > 24 || mi > 59) return false;
        if (pos < len && s[pos] == ':') {
            pos++;
            if (pos + 2 > len || !gw2_iso_digits(s + pos, 2, &sec)) return false;
            pos += 2;
            if (sec > 59) return false;
            if (pos < len && (s[pos] == '.' || s[pos] == ',')) {
                pos++;
                size_t fs = pos;
                while (pos < len && s[pos] >= '0' && s[pos] <= '9') pos++;
                if (pos == fs) return false;
                char fb[7] = "000000";
                size_t take = pos - fs; if (take > 6) take = 6;
                memcpy(fb, s + fs, take);
                frac = (double)atol(fb) / 1e6;
            }
        }
        if (h == 24 && (mi != 0 || sec != 0 || frac != 0.0)) return false;
        if (pos < len) {
            char c = s[pos];
            if (c == 'Z') { pos++; }
            else if (c == '+' || c == '-') {
                pos++;
                long oh = 0, om = 0;
                if (pos + 2 > len || !gw2_iso_digits(s + pos, 2, &oh)) return false;
                pos += 2;
                if (pos < len) {
                    if (s[pos] == ':') {
                        pos++;
                        if (pos + 2 > len || !gw2_iso_digits(s + pos, 2, &om)) return false;
                        pos += 2;
                    } else if (pos + 1 < len) {
                        if (pos + 2 > len || !gw2_iso_digits(s + pos, 2, &om)) return false;
                        pos += 2;
                    }
                }
                if (oh > 23 || om > 59) return false;
                off_sec = oh * 3600 + om * 60;
                if (c == '-') off_sec = -off_sec;
            } else return false;
        }
        if (pos != len) return false;
    }
    long days = gw2_days_from_civil(y, mo, d);
    if (h == 24) { days += 1; h = 0; }
    *out = (double)(days * 86400 + h * 3600 + mi * 60 + sec) + frac - (double)off_sec;
    return true;
}

/* ── mini str.format with ONE positional arg ──
 * Models the live call domain: "{}" (auto) / "{0}" (manual index 0) fields and
 * "{{"/"}}" escapes. Replicates Python's auto-index consumption (a second
 * auto field -> IndexError), auto/manual mixing (-> ValueError), named fields
 * (-> KeyError) and lone braces (-> ValueError), all caught by _task_run_hint.
 * Conversions ("!r") and format-specs (":>3") are outside the ported domain
 * (the live callers only ever pass a single plain "{}"); they fail closed. */
static char *gw2_format1(const char *fmt, const char *arg) {
    gw2_buf b; gw2_binit(&b);
    int mode = 0;       /* 0 unset, 1 auto, 2 manual */
    int auto_idx = 0;
    const char *p = fmt;
    while (*p) {
        if (*p == '{') {
            if (p[1] == '{') { gw2_bputc(&b, '{'); p += 2; continue; }
            const char *close = strchr(p + 1, '}');
            if (!close) { gw2_bfree(&b); return NULL; }
            size_t flen = (size_t)(close - (p + 1));
            char *field = strndup(p + 1, flen);
            bool ok = true;
            if (flen == 0) {
                if (mode == 2) ok = false;
                else { mode = 1; if (auto_idx >= 1) ok = false; auto_idx++; }
            } else if (strchr(field, '!') || strchr(field, ':')) {
                ok = false;
            } else if (field[strspn(field, "0123456789")] == '\0') {
                if (mode == 1) ok = false;
                else { mode = 2; if (atoi(field) != 0) ok = false; }
            } else {
                ok = false;
            }
            free(field);
            if (!ok) { gw2_bfree(&b); return NULL; }
            gw2_bputs(&b, arg);
            p = close + 1;
            continue;
        }
        if (*p == '}') {
            if (p[1] == '}') { gw2_bputc(&b, '}'); p += 2; continue; }
            gw2_bfree(&b);
            return NULL;
        }
        gw2_bputc(&b, *p);
        p++;
    }
    return gw2_btake(&b);
}

/* ── strip helpers (ASCII-scoped) ── */
static bool gw2_ws_char(unsigned char c) {
    return c == 0x20 || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f);
}
static bool gw2_stripped_nonempty(const char *s) {
    if (!s) return false;
    size_t n = strlen(s);
    size_t i = 0;
    while (i < n && gw2_ws_char((unsigned char)s[i])) i++;
    while (n > i && gw2_ws_char((unsigned char)s[n - 1])) n--;
    return n > i;
}
static char *gw2_strip_quotes(const char *s) {
    size_t n = strlen(s);
    size_t i = 0;
    while (i < n && s[i] == '"') i++;
    while (n > i && s[n - 1] == '"') n--;
    return strndup(s + i, n - i);
}
/* " ".join(text.split()) — collapse ASCII whitespace runs */
static char *gw2_collapse_ws(const char *s) {
    gw2_buf b; gw2_binit(&b);
    bool in_ws = false, any = false;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        bool ws = (c == 0x20 || (c >= 0x09 && c <= 0x0d));
        if (ws) { in_ws = true; continue; }
        if (in_ws && any) gw2_bputc(&b, ' ');
        in_ws = false; any = true;
        gw2_bputc(&b, (char)c);
    }
    return gw2_btake(&b);
}

/* ── minimal XML parser (ElementTree.fromstring accept/reject domain) ── */

typedef struct gx_attr { char *key; char *val; } gx_attr;
typedef struct gx_node {
    char *tag;                 /* expanded "{uri}local" or "local" */
    char *raw;                 /* original QName (end tags match RAW, expat-style) */
    gx_attr *attrs; size_t nattrs;
    struct gx_node **kids; size_t nkids;
    char *text;                /* text before the first child (NULL if none) */
} gx_node;
typedef struct gx_ns { char *prefix; char *uri; struct gx_ns *next; } gx_ns;

typedef struct {
    const char *s; size_t pos, len;
    bool failed;
} gx_state;

static void gx_free_node(gx_node *n) {
    if (!n) return;
    free(n->tag);
    free(n->raw);
    for (size_t i = 0; i < n->nattrs; i++) { free(n->attrs[i].key); free(n->attrs[i].val); }
    free(n->attrs);
    for (size_t i = 0; i < n->nkids; i++) gx_free_node(n->kids[i]);
    free(n->kids);
    free(n->text);
    free(n);
}
static void gx_free_ns(gx_ns *ns) {
    while (ns) { gx_ns *nx = ns->next; free(ns->prefix); free(ns->uri); free(ns); ns = nx; }
}
static bool gx_name_start(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':' || c >= 0x80;
}
static bool gx_name_char(unsigned char c) {
    return gx_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}
static char *gx_read_name(gx_state *st) {
    size_t start = st->pos;
    if (st->pos >= st->len || !gx_name_start((unsigned char)st->s[st->pos])) { st->failed = true; return NULL; }
    st->pos++;
    while (st->pos < st->len && gx_name_char((unsigned char)st->s[st->pos])) st->pos++;
    return strndup(st->s + start, st->pos - start);
}
static void gx_ws(gx_state *st) {
    while (st->pos < st->len) {
        char c = st->s[st->pos];
        if (c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d) st->pos++;
        else break;
    }
}
static void gx_utf8_put(gw2_buf *b, unsigned int cp) {
    char t[4];
    if (cp < 0x80) { t[0] = (char)cp; gw2_bput(b, t, 1); }
    else if (cp < 0x800) {
        t[0] = (char)(0xC0 | (cp >> 6)); t[1] = (char)(0x80 | (cp & 0x3F)); gw2_bput(b, t, 2);
    } else if (cp < 0x10000) {
        t[0] = (char)(0xE0 | (cp >> 12)); t[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        t[2] = (char)(0x80 | (cp & 0x3F)); gw2_bput(b, t, 3);
    } else {
        t[0] = (char)(0xF0 | (cp >> 18)); t[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        t[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); t[3] = (char)(0x80 | (cp & 0x3F));
        gw2_bput(b, t, 4);
    }
}
/* decode one &...; entity into b; pos at '&'; false on undefined/invalid */
static bool gx_entity(gx_state *st, gw2_buf *b) {
    st->pos++;  /* '&' */
    size_t start = st->pos;
    while (st->pos < st->len && st->s[st->pos] != ';' && st->pos - start < 12) st->pos++;
    if (st->pos >= st->len || st->s[st->pos] != ';') { st->failed = true; return false; }
    size_t n = st->pos - start;
    const char *e = st->s + start;
    st->pos++;  /* ';' */
    if (n == 3 && !strncmp(e, "amp", 3)) { gw2_bputc(b, '&'); return true; }
    if (n == 2 && !strncmp(e, "lt", 2)) { gw2_bputc(b, '<'); return true; }
    if (n == 2 && !strncmp(e, "gt", 2)) { gw2_bputc(b, '>'); return true; }
    if (n == 4 && !strncmp(e, "quot", 4)) { gw2_bputc(b, '"'); return true; }
    if (n == 4 && !strncmp(e, "apos", 4)) { gw2_bputc(b, '\''); return true; }
    if (n >= 2 && e[0] == '#') {
        unsigned int cp = 0;
        bool hex = (e[1] == 'x' || e[1] == 'X');
        size_t i = hex ? 2 : 1;
        if (i >= n) { st->failed = true; return false; }
        for (; i < n; i++) {
            char c = e[i];
            int dv;
            if (c >= '0' && c <= '9') dv = c - '0';
            else if (hex && c >= 'a' && c <= 'f') dv = c - 'a' + 10;
            else if (hex && c >= 'A' && c <= 'F') dv = c - 'A' + 10;
            else { st->failed = true; return false; }
            cp = cp * (hex ? 16 : 10) + (unsigned int)dv;
            if (cp > 0x10FFFF) { st->failed = true; return false; }
        }
        if (cp == 0 || (cp >= 0xD800 && cp <= 0xDFFF)) { st->failed = true; return false; }
        gx_utf8_put(b, cp);
        return true;
    }
    st->failed = true;
    return false;
}
/* skip <!-- ... --> with XML comment rules; false on malformed */
static bool gx_comment(gx_state *st) {
    st->pos += 4;  /* '<!--' */
    size_t start = st->pos;
    while (st->pos + 3 <= st->len) {
        if (st->s[st->pos] == '-' && st->s[st->pos + 1] == '-' && st->s[st->pos + 2] == '>') {
            /* no '--' inside, and no '-' immediately before the terminator */
            for (size_t i = start; i + 1 < st->pos; i++)
                if (st->s[i] == '-' && st->s[i + 1] == '-') { st->failed = true; return false; }
            if (st->pos > start && st->s[st->pos - 1] == '-') { st->failed = true; return false; }
            st->pos += 3;
            return true;
        }
        st->pos++;
    }
    st->failed = true;
    return false;
}
/* skip <? ... ?>; xml-target PIs are only legal as the document declaration */
static bool gx_pi(gx_state *st, bool at_doc_start) {
    st->pos += 2;  /* '<?' */
    size_t ts = st->pos;
    while (st->pos < st->len && gx_name_char((unsigned char)st->s[st->pos])) st->pos++;
    size_t tn = st->pos - ts;
    bool is_xml = (tn == 3 &&
                   (st->s[ts] == 'x' || st->s[ts] == 'X') &&
                   (st->s[ts + 1] == 'm' || st->s[ts + 1] == 'M') &&
                   (st->s[ts + 2] == 'l' || st->s[ts + 2] == 'L'));
    if (is_xml && !at_doc_start) { st->failed = true; return false; }
    if (is_xml) {
        /* XML declaration: whitespace then pseudo-attributes, version required */
        if (st->pos >= st->len || !(st->s[st->pos] == 0x20 || st->s[st->pos] == 0x09 ||
                                    st->s[st->pos] == 0x0a || st->s[st->pos] == 0x0d)) {
            st->failed = true; return false;
        }
        bool have_version = false;
        for (;;) {
            gx_ws(st);
            if (st->pos + 2 <= st->len && st->s[st->pos] == '?' && st->s[st->pos + 1] == '>') {
                st->pos += 2;
                if (!have_version) { st->failed = true; return false; }
                return true;
            }
            char *nm = gx_read_name(st);
            if (!nm) return false;
            if (!strcmp(nm, "version")) have_version = true;
            free(nm);
            gx_ws(st);
            if (st->pos >= st->len || st->s[st->pos] != '=') { st->failed = true; return false; }
            st->pos++;
            gx_ws(st);
            if (st->pos >= st->len || (st->s[st->pos] != '"' && st->s[st->pos] != '\'')) {
                st->failed = true; return false;
            }
            char q = st->s[st->pos++];
            while (st->pos < st->len && st->s[st->pos] != q) st->pos++;
            if (st->pos >= st->len) { st->failed = true; return false; }
            st->pos++;
        }
    }
    while (st->pos + 2 <= st->len) {
        if (st->s[st->pos] == '?' && st->s[st->pos + 1] == '>') { st->pos += 2; return true; }
        st->pos++;
    }
    st->failed = true;
    return false;
}
/* skip <!DOCTYPE ...> (bracket-aware internal subset) */
static bool gx_doctype(gx_state *st) {
    st->pos += 2;  /* '<!' */
    int brackets = 0;
    while (st->pos < st->len) {
        char c = st->s[st->pos];
        if (c == '[') brackets++;
        else if (c == ']') brackets--;
        else if (c == '>' && brackets <= 0) { st->pos++; return true; }
        st->pos++;
    }
    st->failed = true;
    return false;
}
static const char *gx_ns_lookup(gx_ns *ns, const char *prefix, size_t n) {
    for (gx_ns *p = ns; p; p = p->next)
        if (strlen(p->prefix) == n && !strncmp(p->prefix, prefix, n)) return p->uri;
    return NULL;
}
/* expand a QName using the scope; returns malloc'd "{uri}local" or "local" */
static char *gx_expand(gx_state *st, gx_ns *ns, const char *qname) {
    const char *colon = strchr(qname, ':');
    if (colon) {
        const char *uri = gx_ns_lookup(ns, qname, (size_t)(colon - qname));
        if (!uri) { st->failed = true; return NULL; }
        return gw2_xsf("{%s}%s", uri, colon + 1);
    }
    const char *def = gx_ns_lookup(ns, "", 0);
    if (def && def[0]) return gw2_xsf("{%s}%s", def, qname);
    return strdup(qname);
}
/* attribute names: the default namespace does NOT apply to unprefixed
 * attributes (XML Namespaces spec); only prefixed names expand */
static char *gx_expand_attr(gx_state *st, gx_ns *ns, const char *qname) {
    const char *colon = strchr(qname, ':');
    if (colon) {
        const char *uri = gx_ns_lookup(ns, qname, (size_t)(colon - qname));
        if (!uri) { st->failed = true; return NULL; }
        return gw2_xsf("{%s}%s", uri, colon + 1);
    }
    return strdup(qname);
}
/* attribute value: entity decode + literal \t\n\r normalization to space */
static char *gx_attr_value(gx_state *st) {
    if (st->pos >= st->len || (st->s[st->pos] != '"' && st->s[st->pos] != '\'')) {
        st->failed = true; return NULL;
    }
    char q = st->s[st->pos++];
    gw2_buf b; gw2_binit(&b);
    while (st->pos < st->len && st->s[st->pos] != q) {
        char c = st->s[st->pos];
        if (c == '<') { st->failed = true; gw2_bfree(&b); return NULL; }
        if (c == '&') { if (!gx_entity(st, &b)) { gw2_bfree(&b); return NULL; } continue; }
        if (c == '\t' || c == '\n' || c == '\r') gw2_bputc(&b, ' ');
        else gw2_bputc(&b, c);
        st->pos++;
    }
    if (st->pos >= st->len) { st->failed = true; gw2_bfree(&b); return NULL; }
    st->pos++;
    return gw2_btake(&b);
}
static gx_node *gx_element(gx_state *st, gx_ns *parent_ns);
/* element content: text (before first child), children, comments, CDATA, PIs */
static bool gx_content(gx_state *st, gx_node *node, gx_ns *ns) {
    gw2_buf text; gw2_binit(&text);
    bool have_text = false;
    bool after_child = false;
    for (;;) {
        if (st->pos >= st->len) { st->failed = true; gw2_bfree(&text); return false; }
        char c = st->s[st->pos];
        if (c == '<') {
            if (st->pos + 1 >= st->len) { st->failed = true; gw2_bfree(&text); return false; }
            char c2 = st->s[st->pos + 1];
            if (c2 == '/') {
                st->pos += 2;
                char *qn = gx_read_name(st);
                if (!qn) { gw2_bfree(&text); return false; }
                /* expat matches end tags by RAW QName, not expanded name */
                bool match = !strcmp(qn, node->raw);
                free(qn);
                gx_ws(st);
                if (!match || st->pos >= st->len || st->s[st->pos] != '>') {
                    st->failed = true; gw2_bfree(&text); return false;
                }
                st->pos++;
                if (have_text && !after_child) node->text = gw2_btake(&text);
                else gw2_bfree(&text);
                return true;
            }
            if (c2 == '!') {
                if (st->pos + 3 < st->len && st->s[st->pos + 2] == '-' && st->s[st->pos + 3] == '-') {
                    if (!gx_comment(st)) { gw2_bfree(&text); return false; }
                    continue;
                }
                if (st->pos + 8 < st->len && !strncmp(st->s + st->pos, "<![CDATA[", 9)) {
                    st->pos += 9;
                    size_t start = st->pos;
                    while (st->pos + 3 <= st->len &&
                           !(st->s[st->pos] == ']' && st->s[st->pos + 1] == ']' && st->s[st->pos + 2] == '>'))
                        st->pos++;
                    if (st->pos + 3 > st->len) { st->failed = true; gw2_bfree(&text); return false; }
                    if (!after_child) { gw2_bput(&text, st->s + start, st->pos - start); have_text = true; }
                    st->pos += 3;
                    continue;
                }
                st->failed = true; gw2_bfree(&text); return false;
            }
            if (c2 == '?') {
                if (!gx_pi(st, false)) { gw2_bfree(&text); return false; }
                continue;
            }
            /* child element */
            if (!after_child) {
                if (have_text) node->text = gw2_btake(&text);
                after_child = true;
            }
            gx_node *kid = gx_element(st, ns);
            if (!kid) { gw2_bfree(&text); return false; }
            gx_node **nk = realloc(node->kids, (node->nkids + 1) * sizeof(gx_node *));
            if (!nk) { gx_free_node(kid); st->failed = true; gw2_bfree(&text); return false; }
            node->kids = nk;
            node->kids[node->nkids++] = kid;
            continue;
        }
        if (c == '&') {
            gw2_buf dec; gw2_binit(&dec);
            if (!gx_entity(st, &dec)) { gw2_bfree(&dec); gw2_bfree(&text); return false; }
            if (!after_child) { gw2_bput(&text, dec.p ? dec.p : "", dec.len); have_text = true; }
            gw2_bfree(&dec);
            continue;
        }
        if (c == ']' && st->pos + 2 < st->len && st->s[st->pos + 1] == ']' && st->s[st->pos + 2] == '>') {
            st->failed = true; gw2_bfree(&text); return false;
        }
        if (c == '\r') {
            if (!after_child) { gw2_bputc(&text, '\n'); have_text = true; }
            st->pos++;
            if (st->pos < st->len && st->s[st->pos] == '\n') st->pos++;
            continue;
        }
        if (!after_child) { gw2_bputc(&text, c); have_text = true; }
        st->pos++;
    }
}
static gx_node *gx_element(gx_state *st, gx_ns *parent_ns) {
    st->pos++;  /* '<' */
    char *qname = gx_read_name(st);
    if (!qname) return NULL;
    /* raw attributes */
    gx_attr *raw = NULL; size_t nraw = 0;
    bool selfclose = false;
    for (;;) {
        gx_ws(st);
        if (st->pos >= st->len) { st->failed = true; free(qname); goto fail_raw; }
        if (st->s[st->pos] == '>') { st->pos++; break; }
        if (st->s[st->pos] == '/' && st->pos + 1 < st->len && st->s[st->pos + 1] == '>') {
            st->pos += 2; selfclose = true; break;
        }
        char *an = gx_read_name(st);
        if (!an) { free(qname); goto fail_raw; }
        gx_ws(st);
        if (st->pos >= st->len || st->s[st->pos] != '=') { st->failed = true; free(an); free(qname); goto fail_raw; }
        st->pos++;
        gx_ws(st);
        char *av = gx_attr_value(st);
        if (!av) { free(an); free(qname); goto fail_raw; }
        gx_attr *nr = realloc(raw, (nraw + 1) * sizeof(gx_attr));
        if (!nr) { free(an); free(av); free(qname); st->failed = true; goto fail_raw; }
        raw = nr;
        raw[nraw].key = an; raw[nraw].val = av; nraw++;
    }
    /* namespace scope: parent bindings + this element's xmlns declarations */
    gx_ns *ns = NULL;
    for (size_t i = 0; i < nraw; i++) {
        const char *k = raw[i].key;
        const char *prefix = NULL;
        if (!strcmp(k, "xmlns")) prefix = "";
        else if (!strncmp(k, "xmlns:", 6)) prefix = k + 6;
        if (prefix) {
            gx_ns *nb = malloc(sizeof(gx_ns));
            nb->prefix = strdup(prefix);
            nb->uri = strdup(raw[i].val);
            nb->next = ns;
            ns = nb;
        }
    }
    /* chain parent scope below the new bindings */
    gx_ns *tail = ns;
    while (tail && tail->next) tail = tail->next;
    /* copy parent bindings (owned copies so gx_free_ns is uniform) */
    for (gx_ns *p = parent_ns; p; p = p->next) {
        gx_ns *nb = malloc(sizeof(gx_ns));
        nb->prefix = strdup(p->prefix);
        nb->uri = strdup(p->uri);
        nb->next = NULL;
        if (tail) tail->next = nb; else ns = nb;
        tail = nb;
    }
    gx_node *node = calloc(1, sizeof(gx_node));
    node->raw = strdup(qname);
    node->tag = gx_expand(st, ns, qname);
    free(qname);
    if (!node->tag) { gx_free_ns(ns); gx_free_node(node); goto fail_raw; }
    /* expanded attributes (xmlns declarations excluded), duplicate check */
    for (size_t i = 0; i < nraw; i++) {
        const char *k = raw[i].key;
        if (!strcmp(k, "xmlns") || !strncmp(k, "xmlns:", 6)) continue;
        char *ek = gx_expand_attr(st, ns, k);
        if (!ek) { gx_free_ns(ns); gx_free_node(node); goto fail_raw; }
        bool dup = false;
        for (size_t j = 0; j < node->nattrs; j++)
            if (!strcmp(node->attrs[j].key, ek)) { dup = true; break; }
        if (dup) { free(ek); st->failed = true; gx_free_ns(ns); gx_free_node(node); goto fail_raw; }
        gx_attr *na = realloc(node->attrs, (node->nattrs + 1) * sizeof(gx_attr));
        if (!na) { free(ek); st->failed = true; gx_free_ns(ns); gx_free_node(node); goto fail_raw; }
        node->attrs = na;
        node->attrs[node->nattrs].key = ek;
        node->attrs[node->nattrs].val = strdup(raw[i].val);
        node->nattrs++;
    }
    for (size_t i = 0; i < nraw; i++) { free(raw[i].key); free(raw[i].val); }
    free(raw);
    raw = NULL; nraw = 0;
    if (!selfclose) {
        if (!gx_content(st, node, ns)) { gx_free_ns(ns); gx_free_node(node); return NULL; }
    }
    gx_free_ns(ns);
    return node;
fail_raw:
    for (size_t i = 0; i < nraw; i++) { free(raw[i].key); free(raw[i].val); }
    free(raw);
    return NULL;
}
/* parse a whole document; returns root node or NULL (ParseError) */
static gx_node *gx_parse(const char *xml) {
    gx_state st = {xml, 0, strlen(xml), false};
    /* optional UTF-8 BOM */
    if (st.len >= 3 && (unsigned char)xml[0] == 0xEF && (unsigned char)xml[1] == 0xBB &&
        (unsigned char)xml[2] == 0xBF) st.pos = 3;
    size_t decl_start = st.pos;
    gx_ws(&st);
    /* XML declaration must sit at the very start (after BOM) */
    if (st.pos + 5 <= st.len && !strncmp(st.s + st.pos, "<?xml", 5)) {
        if (st.pos != decl_start) { st.failed = true; return NULL; }
        if (!gx_pi(&st, true)) return NULL;
    }
    for (;;) {
        gx_ws(&st);
        if (st.pos >= st.len) { st.failed = true; return NULL; }
        if (st.s[st.pos] != '<') { st.failed = true; return NULL; }
        if (st.pos + 3 < st.len && st.s[st.pos + 1] == '!' && st.s[st.pos + 2] == '-' && st.s[st.pos + 3] == '-') {
            if (!gx_comment(&st)) return NULL;
            continue;
        }
        if (st.pos + 1 < st.len && st.s[st.pos + 1] == '?') {
            if (!gx_pi(&st, st.pos == decl_start)) return NULL;
            continue;
        }
        if (st.pos + 8 < st.len && !strncmp(st.s + st.pos, "<!DOCTYPE", 9)) {
            if (!gx_doctype(&st)) return NULL;
            continue;
        }
        break;
    }
    gx_node *root = gx_element(&st, NULL);
    if (!root) return NULL;
    for (;;) {
        gx_ws(&st);
        if (st.pos >= st.len) break;
        if (st.s[st.pos] != '<') { gx_free_node(root); return NULL; }
        if (st.pos + 3 < st.len && st.s[st.pos + 1] == '!' && st.s[st.pos + 2] == '-' && st.s[st.pos + 3] == '-') {
            if (!gx_comment(&st)) { gx_free_node(root); return NULL; }
            continue;
        }
        if (st.pos + 1 < st.len && st.s[st.pos + 1] == '?') {
            if (!gx_pi(&st, false)) { gx_free_node(root); return NULL; }
            continue;
        }
        gx_free_node(root);
        return NULL;
    }
    return root;
}

/* _task_xml_leaf_values: namespace-agnostic leaf-path -> collapsed text map */
static void gx_leaves_visit(const gx_node *n, const char *path, json_t *out) {
    const char *local = strrchr(n->tag, '}');
    local = local ? local + 1 : n->tag;
    char *cur = path[0] ? gw2_xsf("%s/%s", path, local) : strdup(local);
    if (n->nkids == 0) {
        char *collapsed = gw2_collapse_ws(n->text ? n->text : "");
        json_set(out, cur, json_string(collapsed));
        free(collapsed);
    }
    for (size_t i = 0; i < n->nkids; i++)
        gx_leaves_visit(n->kids[i], cur, out);
    free(cur);
}
/* PoP: gw2_task_xml_leaf_values @ hermes_cli/gateway_windows.py:_task_xml_leaf_values */
static json_t *gw2_task_xml_leaf_values(const json_t *xml) {
    const char *s = gw2_jstr(xml);
    if (!s) {
        /* ElementTree.fromstring(non-str) raises TypeError (only ParseError
         * is caught live) — propagate the exact message. */
        char *msg = gw2_xsf("a bytes-like object is required, not '%s'", gw2_typename(xml));
        json_t *r = gw2_raise_spec("TypeError", msg);
        free(msg);
        return r;
    }
    gx_node *root = gx_parse(s);
    if (!root) return NULL;  /* ParseError -> None */
    json_t *out = json_object();
    const char *version = "";
    for (size_t i = 0; i < root->nattrs; i++)
        if (!strcmp(root->attrs[i].key, "version")) { version = root->attrs[i].val; break; }
    json_set(out, "Task@version", json_string(version));
    gx_leaves_visit(root, "", out);
    gx_free_node(root);
    return out;
}

/* ── pure attestation leaves ── */

/* PoP: gw2_attested_pids_from @ hermes_cli/gateway_windows.py:_attested_pids_from */
static json_t *gw2_attested_pids_from(const json_t *data) {
    json_t *out = json_array();
    if (!data || data->type != JSON_OBJECT) return out;
    const json_t *pids = json_obj_get(data, "pids");
    if (!pids || pids->type != JSON_ARRAY) return out;
    /* exact positive ints only; bool taints; one bad item taints the list.
     * (integral-float literals are outside the fixture domain: libjson has
     * no int/float identity) */
    for (size_t i = 0; i < pids->c.count; i++) {
        const json_t *p = pids->c.items[i];
        if (!p || p->type != JSON_NUMBER) { json_free(out); return json_array(); }
        double d = p->num_val;
        if (d <= 0 || d != floor(d)) { json_free(out); return json_array(); }
    }
    for (size_t i = 0; i < pids->c.count; i++)
        json_append(out, json_number(pids->c.items[i]->num_val));
    return out;
}

/* PoP: gw2_attestation_generation @ hermes_cli/gateway_windows.py:_attestation_generation */
static json_t *gw2_attestation_generation(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) return NULL;
    const json_t *g = json_obj_get(data, "generation");
    if (!gw2_jtruth(g)) return NULL;
    char *s = gw2_pystr(g);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: gw2_attested_create_time @ hermes_cli/gateway_windows.py:_attested_create_time */
static json_t *gw2_attested_create_time(const json_t *data, double pid) {
    if (!data || data->type != JSON_OBJECT) return NULL;
    const json_t *times = json_obj_get(data, "create_times");
    if (!times || times->type != JSON_OBJECT) return NULL;
    char key[32];
    snprintf(key, sizeof(key), "%.0f", pid);
    const json_t *v = json_obj_get(times, key);
    /* type(value) in (int, float) — bool excluded */
    if (v && v->type == JSON_NUMBER) return json_number(v->num_val);
    return NULL;
}

/* ── seam convenience: no-arg call ── */
static json_t *gw2_call0(const gw2_seams_t *s, const char *name) {
    json_t *a = gw2_args();
    json_t *r = gw2_call(s, name, a);
    json_free(a);
    return r;
}

/* ── _task_run_hint ── */
/* PoP: gw2_task_run_hint @ hermes_cli/gateway_windows.py:_task_run_hint */
static json_t *gw2_task_run_hint(const char *fmt, const gw2_seams_t *s) {
    json_t *rr = gw2_call0(s, "is_task_registered");
    if (gw2_is_raise(rr)) {
        if (gw2_is_exception(rr)) { json_free(rr); return NULL; }
        return rr;  /* BaseException passthrough */
    }
    bool reg = gw2_jtruth(rr);
    json_free(rr);
    if (!reg) return NULL;
    json_t *nr = gw2_call0(s, "get_task_name");
    if (gw2_is_raise(nr)) {
        if (gw2_is_exception(nr)) { json_free(nr); return NULL; }
        return nr;
    }
    char *name = gw2_pystr(nr);
    json_free(nr);
    char *formatted = gw2_format1(fmt ? fmt : "", name);
    free(name);
    if (!formatted) return NULL;  /* KeyError/ValueError caught live */
    json_t *out = json_string(formatted);
    free(formatted);
    return out;
}

/* ── _format_attestation_warning ── */
/* PoP: gw2_format_attestation_warning @ hermes_cli/gateway_windows.py:_format_attestation_warning */
static json_t *gw2_format_attestation_warning(const json_t *attested, const json_t *data,
                                              const gw2_seams_t *s) {
    if (!data || data->type != JSON_OBJECT) {
        char *msg = gw2_xsf("'%s' object has no attribute 'get'", gw2_typename(data));
        json_t *r = gw2_raise_spec("AttributeError", msg);
        free(msg);
        return r;
    }
    const json_t *via = json_obj_get(data, "via");
    char *vias = gw2_jtruth(via) ? gw2_pystr(via) : strdup("direct spawn");
    const json_t *ts = json_obj_get(data, "ts");
    char *tss = gw2_jtruth(ts) ? gw2_pystr(ts) : strdup("unknown time");
    gw2_buf jb; gw2_binit(&jb);
    if (attested && attested->type == JSON_ARRAY) {
        for (size_t i = 0; i < attested->c.count; i++) {
            if (i) gw2_bputs(&jb, ", ");
            char *ps = gw2_pystr(attested->c.items[i]);
            gw2_bputs(&jb, ps);
            free(ps);
        }
    }
    char *line1 = gw2_xsf(
        "⚠ The previous gateway start (%s, %s) reported success, but the "
        "process (PID %s) died without a clean shutdown record.",
        vias, tss, jb.p ? jb.p : "");
    free(vias); free(tss); gw2_bfree(&jb);
    const char *line2 =
        "  This usually means the shell that ran `hermes gateway start` was inside "
        "a Windows Job Object that killed the gateway on exit (#91675).";
    json_t *hint = gw2_task_run_hint(
        "  Recovery: schtasks /Run /TN {}   (Task Scheduler starts the gateway outside any Job Object)", s);
    if (gw2_is_raise(hint)) { free(line1); return hint; }
    gw2_buf out; gw2_binit(&out);
    gw2_bputs(&out, line1);
    gw2_bputc(&out, '\n');
    gw2_bputs(&out, line2);
    if (gw2_jtruth(hint)) {
        gw2_bputc(&out, '\n');
        char *hs = gw2_pystr(hint);
        gw2_bputs(&out, hs);
        free(hs);
    }
    json_free(hint);
    free(line1);
    json_t *res = json_string(out.p ? out.p : "");
    gw2_bfree(&out);
    return res;
}

/* ── _read_start_attestation ── */
/* Path(s).as_posix() on Windows for a seam-returned path string: backslashes
 * become '/', empty maps to '.', and a bare UNC share root (//server/share)
 * gains a trailing '/'. */
static char *gw2_win_path_posix(const char *s) {
    if (!s || !*s) return strdup(".");
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    if (!out) return strdup(".");
    for (size_t i = 0; i < n; i++) out[i] = (s[i] == '\\') ? '/' : s[i];
    out[n] = '\0';
    if (out[0] == '/' && out[1] == '/' && out[2] != '/') {
        const char *slash = strchr(out + 2, '/');
        if (slash && slash != out + 2 && slash[1] != '\0') {
            const char *slash2 = strchr(slash + 1, '/');
            if (!slash2) {
                size_t len = strlen(out);
                if (out[len - 1] != '/') { out[len] = '/'; out[len + 1] = '\0'; }
            }
        }
    }
    return out;
}

/* PoP: gw2_read_start_attestation @ hermes_cli/gateway_windows.py:_read_start_attestation */
static json_t *gw2_read_start_attestation(const char *home, const gw2_seams_t *s) {
    json_t *a = gw2_args();
    gw2_aset_s(a, "home", home);
    json_t *pr = gw2_call(s, "start_attestation_path", a);
    json_free(a);
    if (gw2_is_raise(pr)) {
        if (gw2_caught(pr, "OSError") || gw2_caught(pr, "ValueError")) { json_free(pr); return NULL; }
        return pr;
    }
    const char *paths = gw2_jstr(pr);
    if (!paths) {
        json_free(pr);
        return gw2_raise_spec("AttributeError", "'NoneType' object has no attribute 'read_text'");
    }
    char *path = gw2_win_path_posix(paths);
    json_free(pr);
    json_t *ra = gw2_args();
    gw2_aset_s(ra, "path", path);
    gw2_aset_s(ra, "encoding", "utf-8-sig");
    json_t *txt = gw2_call(s, "path_read_text", ra);
    json_free(ra);
    free(path);
    if (gw2_is_raise(txt)) {
        if (gw2_caught(txt, "OSError") || gw2_caught(txt, "ValueError")) { json_free(txt); return NULL; }
        return txt;
    }
    const char *body = gw2_jstr(txt);
    if (!body) {
        /* json.loads(non-str) raises TypeError — NOT caught by (OSError, ValueError) */
        char *msg = gw2_xsf("the JSON object must be str, bytes or bytearray, not %s", gw2_typename(txt));
        json_free(txt);
        json_t *r = gw2_raise_spec("TypeError", msg);
        free(msg);
        return r;
    }
    if (!gw2_json_strict_valid(body)) { json_free(txt); return NULL; }  /* ValueError caught */
    char *err = NULL;
    json_t *data = json_parse(body, &err);
    free(err);
    json_free(txt);
    if (!data) return NULL;
    if (data->type == JSON_NULL) { json_free(data); return NULL; }  /* parsed null == None */
    return data;
}

/* ── _clear_start_attestation ── */
/* PoP: gw2_clear_start_attestation @ hermes_cli/gateway_windows.py:_clear_start_attestation */
static json_t *gw2_clear_start_attestation(const char *home, const gw2_seams_t *s) {
    json_t *a = gw2_args();
    gw2_aset_s(a, "home", home);
    json_t *pr = gw2_call(s, "start_attestation_path", a);
    json_free(a);
    if (gw2_is_raise(pr)) {
        if (gw2_caught(pr, "OSError")) { json_free(pr); return NULL; }
        return pr;
    }
    const char *paths = gw2_jstr(pr);
    if (!paths) {
        json_free(pr);
        return gw2_raise_spec("AttributeError", "'NoneType' object has no attribute 'unlink'");
    }
    json_t *ua = gw2_args();
    gw2_aset_s(ua, "path", paths);
    gw2_aset_b(ua, "missing_ok", true);
    json_t *ur = gw2_call(s, "path_unlink", ua);
    json_free(ua);
    json_free(pr);
    if (gw2_is_raise(ur)) {
        if (gw2_caught(ur, "OSError")) { json_free(ur); return NULL; }
        return ur;
    }
    json_free(ur);
    return NULL;
}

/* ── _attestation_within_horizon ── */
/* PoP: gw2_attestation_within_horizon @ hermes_cli/gateway_windows.py:_attestation_within_horizon */
static json_t *gw2_attestation_within_horizon(const json_t *data, const gw2_seams_t *s) {
    bool verdict = false;
    if (data && data->type == JSON_OBJECT) {
        const json_t *ts = json_obj_get(data, "ts");
        if (ts) {  /* missing ts -> KeyError -> except Exception -> False */
            char *tss = gw2_pystr(ts);
            double epoch;
            if (gw2_iso_epoch(tss, &epoch)) {
                json_t *tr = gw2_call0(s, "time_time");
                if (gw2_is_raise(tr)) {
                    free(tss);
                    if (gw2_is_exception(tr)) { json_free(tr); return json_bool(false); }
                    return tr;  /* BaseException passthrough */
                }
                if (tr && tr->type == JSON_NUMBER) {
                    double age = tr->num_val - epoch;
                    verdict = (-60.0 <= age && age <= 86400.0);
                }
                /* non-number clock: TypeError in live -> except -> False */
                json_free(tr);
            }
            free(tss);
        }
    }
    return json_bool(verdict);
}

/* ── _write_start_attestation ── */
/* PoP: gw2_write_start_attestation @ hermes_cli/gateway_windows.py:_write_start_attestation */
static json_t *gw2_write_start_attestation(const json_t *pids, const char *via, const char *home,
                                           const gw2_seams_t *s) {
    /* the whole body sits in live try/except Exception (logger.debug silent) */
    json_t *a = gw2_args();
    gw2_aset_s(a, "home", home);
    json_t *pr = gw2_call(s, "start_attestation_path", a);
    json_free(a);
    if (gw2_is_raise(pr)) {
        if (gw2_is_exception(pr)) { json_free(pr); return NULL; }
        return pr;
    }
    const char *paths = gw2_jstr(pr);
    if (!paths) { json_free(pr); return NULL; }  /* .parent AttributeError caught */
    char *path = strdup(paths);
    json_free(pr);
    char *parent = gw2_path_parent(path);
    json_t *ma = gw2_args();
    gw2_aset_s(ma, "path", parent);
    gw2_aset_b(ma, "parents", true);
    gw2_aset_b(ma, "exist_ok", true);
    json_t *mr = gw2_call(s, "path_mkdir", ma);
    json_free(ma);
    free(parent);
    if (gw2_is_raise(mr)) {
        if (gw2_is_exception(mr)) { json_free(mr); free(path); return NULL; }
        free(path);
        return mr;
    }
    json_free(mr);
    json_t *ir = gw2_call0(s, "import_process_create_time");
    if (gw2_is_raise(ir)) {
        if (gw2_is_exception(ir)) { json_free(ir); free(path); return NULL; }
        free(path);
        return ir;
    }
    json_free(ir);
    /* payload["pids"] = [int(p) for p in pids] — the FIRST dict field evaluated */
    if (!pids || pids->type != JSON_ARRAY) { free(path); return NULL; }  /* TypeError caught */
    json_t *parr = json_array();
    for (size_t i = 0; i < pids->c.count; i++) {
        double iv;
        if (!gw2_pyint(pids->c.items[i], &iv)) { json_free(parr); free(path); return NULL; }
        json_append(parr, json_number(iv));
    }
    /* ts = datetime.now(UTC).isoformat() */
    json_t *dr = gw2_call0(s, "datetime_now");
    if (gw2_is_raise(dr)) {
        if (gw2_is_exception(dr)) { json_free(dr); json_free(parr); free(path); return NULL; }
        json_free(parr); free(path);
        return dr;
    }
    json_t *ts = dr ? dr : json_null();
    /* generation = uuid.uuid4().hex */
    json_t *ur = gw2_call0(s, "uuid4_hex");
    if (gw2_is_raise(ur)) {
        if (gw2_is_exception(ur)) { json_free(ur); json_free(ts); json_free(parr); free(path); return NULL; }
        json_free(ts); json_free(parr); free(path);
        return ur;
    }
    json_t *gen = ur ? ur : json_null();
    /* create_times = {str(int(p)): _process_create_time(int(p)) for p in pids},
     * then filtered to non-None values (insertion order preserved; duplicate
     * pid keys keep the first position with the last value, like dict) */
    json_t *ct = json_object();
    for (size_t i = 0; i < pids->c.count; i++) {
        double iv;
        gw2_pyint(pids->c.items[i], &iv);  /* validated above */
        char key[32];
        snprintf(key, sizeof(key), "%.0f", iv);
        json_t *ca = gw2_args();
        gw2_aset_n(ca, "pid", iv);
        json_t *cr = gw2_call(s, "process_create_time", ca);
        json_free(ca);
        if (gw2_is_raise(cr)) {
            if (gw2_is_exception(cr)) {
                json_free(cr); json_free(ct); json_free(gen); json_free(ts); json_free(parr); free(path);
                return NULL;
            }
            json_free(ct); json_free(gen); json_free(ts); json_free(parr); free(path);
            return cr;
        }
        if (cr && cr->type != JSON_NULL) json_set(ct, key, cr);
        else json_free(cr);
    }
    json_t *payload = json_object();
    json_set(payload, "pids", parr);
    json_set(payload, "via", via ? json_string(via) : json_null());
    json_set(payload, "ts", ts);
    json_set(payload, "generation", gen);
    json_set(payload, "create_times", ct);
    char *body = gw2_dumps(payload);
    json_free(payload);
    char *tmp = gw2_with_suffix(path, ".json.tmp");
    if (!tmp) { free(body); free(path); return NULL; }  /* ValueError caught */
    json_t *wa = gw2_args();
    gw2_aset_s(wa, "path", tmp);
    gw2_aset_s(wa, "content", body);
    gw2_aset_s(wa, "encoding", "utf-8");
    json_t *wr = gw2_call(s, "path_write_text", wa);
    json_free(wa);
    if (gw2_is_raise(wr)) {
        if (gw2_is_exception(wr)) { json_free(wr); free(tmp); free(body); free(path); return NULL; }
        free(tmp); free(body); free(path);
        return wr;
    }
    json_free(wr);
    json_t *rpa = gw2_args();
    gw2_aset_s(rpa, "src", tmp);
    gw2_aset_s(rpa, "dst", path);
    json_t *rp = gw2_call(s, "path_replace", rpa);
    json_free(rpa);
    free(tmp); free(body); free(path);
    if (gw2_is_raise(rp)) {
        if (gw2_is_exception(rp)) { json_free(rp); return NULL; }
        return rp;
    }
    json_free(rp);
    return NULL;
}

/* ── check_start_attestation ── */
/* PoP: gw2_check_start_attestation @ hermes_cli/gateway_windows.py:check_start_attestation */
static json_t *gw2_check_start_attestation(const json_t *current_pids, const gw2_seams_t *s) {
    json_t *data = gw2_read_start_attestation(NULL, s);
    if (gw2_is_raise(data)) return data;
    if (!data) return NULL;  /* data is None */
    json_t *attested = gw2_attested_pids_from(data);
    if (!gw2_jtruth(attested)) {
        json_free(attested);
        json_free(data);
        json_t *cr = gw2_clear_start_attestation(NULL, s);
        if (gw2_is_raise(cr)) return cr;
        json_free(cr);
        return NULL;
    }
    json_t *cp = NULL;
    bool cp_owned = false;
    if (gw2_is_null(current_pids)) {
        json_t *ir = gw2_call0(s, "import_find_gateway_pids");
        if (gw2_is_raise(ir)) {
            if (gw2_is_exception(ir)) { json_free(ir); json_free(attested); json_free(data); return NULL; }
            json_free(attested); json_free(data);
            return ir;
        }
        json_free(ir);
        json_t *fr = gw2_call0(s, "find_gateway_pids");
        if (gw2_is_raise(fr)) {
            if (gw2_is_exception(fr)) { json_free(fr); json_free(attested); json_free(data); return NULL; }
            json_free(attested); json_free(data);
            return fr;
        }
        if (fr && fr->type == JSON_ARRAY) { cp = fr; cp_owned = true; }
        else {
            /* list(non-iterable) TypeError — caught by except Exception */
            json_free(fr); json_free(attested); json_free(data);
            return NULL;
        }
    } else {
        cp = (json_t *)current_pids;
    }
    json_t *cr = gw2_clear_start_attestation(NULL, s);
    if (gw2_is_raise(cr)) {
        if (cp_owned) json_free(cp);
        json_free(attested); json_free(data);
        return cr;
    }
    json_free(cr);
    json_t *da = gw2_args();
    gw2_aset_j(da, "attested", attested);
    gw2_aset_j(da, "current_pids", cp);
    gw2_aset_j(da, "data", data);
    /* home defaults None -> omitted from the seam args */
    json_t *dr = gw2_call(s, "attested_dead", da);
    json_free(da);
    if (gw2_is_raise(dr)) {
        if (cp_owned) json_free(cp);
        json_free(attested); json_free(data);
        return dr;
    }
    bool dead = gw2_jtruth(dr);
    json_free(dr);
    if (!dead) {
        if (cp_owned) json_free(cp);
        json_free(attested); json_free(data);
        return NULL;
    }
    json_t *warn = gw2_format_attestation_warning(attested, data, s);
    if (cp_owned) json_free(cp);
    json_free(attested);
    json_free(data);
    return warn;
}

/* ── _print_task_run_hint ── */
/* PoP: gw2_print_task_run_hint @ hermes_cli/gateway_windows.py:_print_task_run_hint */
static json_t *gw2_print_task_run_hint(const char *fmt, const gw2_seams_t *s) {
    json_t *hint = gw2_task_run_hint(fmt, s);
    if (gw2_is_raise(hint)) return hint;
    if (gw2_jtruth(hint)) {
        char *hs = gw2_pystr(hint);
        json_t *a = gw2_args();
        gw2_aset_s(a, "text", hs);
        json_t *r = gw2_call(s, "print", a);
        json_free(a);
        free(hs);
        if (gw2_is_raise(r)) { json_free(hint); return r; }
        json_free(r);
    }
    json_free(hint);
    return NULL;
}

/* ── _print_start_attestation_warning ── */
/* PoP: gw2_print_start_attestation_warning @ hermes_cli/gateway_windows.py:_print_start_attestation_warning */
static json_t *gw2_print_start_attestation_warning(const gw2_seams_t *s) {
    json_t *warn = gw2_check_start_attestation(NULL, s);
    if (gw2_is_raise(warn)) {
        if (gw2_is_exception(warn)) { json_free(warn); return NULL; }
        return warn;  /* BaseException passthrough */
    }
    if (gw2_jtruth(warn)) {
        char *ws = gw2_pystr(warn);
        json_t *a = gw2_args();
        gw2_aset_s(a, "text", ws);
        json_t *r = gw2_call(s, "print", a);
        json_free(a);
        free(ws);
        if (gw2_is_raise(r)) { json_free(warn); return r; }
        json_free(r);
    }
    json_free(warn);
    return NULL;
}

/* ── _query_scheduled_task_xml ── */
/* PoP: gw2_query_scheduled_task_xml @ hermes_cli/gateway_windows.py:_query_scheduled_task_xml */
static json_t *gw2_query_scheduled_task_xml(const char *task_name, const gw2_seams_t *s) {
    json_t *a = gw2_args();
    json_t *argv = json_array();
    json_append(argv, json_string("/Query"));
    json_append(argv, json_string("/TN"));
    json_append(argv, json_string(task_name ? task_name : ""));
    json_append(argv, json_string("/XML"));
    json_set(a, "argv", argv);
    gw2_aset_n(a, "timeout", 15);  /* _SCHTASKS_TIMEOUT_S default */
    json_t *r = gw2_call(s, "exec_schtasks", a);
    json_free(a);
    if (gw2_is_raise(r)) return r;  /* no try live */
    if (!r || r->type != JSON_OBJECT) {
        char *msg = gw2_xsf("'%s' object is not subscriptable", gw2_typename(r));
        json_free(r);
        json_t *e = gw2_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    double code = json_get_num(r, "code", 0);
    const json_t *out = json_obj_get(r, "out");
    const char *outs = gw2_jstr(out);
    if (!outs) {
        char *msg = gw2_xsf("'%s' object has no attribute 'strip'", gw2_typename(out));
        json_free(r);
        json_t *e = gw2_raise_spec("AttributeError", msg);
        free(msg);
        return e;
    }
    if (code != 0 || !gw2_stripped_nonempty(outs)) {
        /* logger.debug silent */
        json_free(r);
        return NULL;
    }
    json_t *res = json_string(outs);  /* returned verbatim, NOT stripped */
    json_free(r);
    return res;
}

/* ── compare_scheduled_task_drift ── */
static const struct { const char *path; const char *label; } GW2_DRIFT_LEAVES[] = {
    {"Task/Settings/RestartOnFailure/Interval", "RestartOnFailure"},
    {"Task/Triggers/LogonTrigger/Delay", "LogonTrigger Delay"},
    {"Task/Actions/Exec/Arguments", "launcher arguments"},
};
#define GW2_LOGON_LEAF "Task/Principals/Principal/LogonType"

/* PoP: gw2_compare_scheduled_task_drift @ hermes_cli/gateway_windows.py:compare_scheduled_task_drift */
static json_t *gw2_compare_scheduled_task_drift(const json_t *registered_xml, const json_t *template_xml) {
    json_t *live = gw2_task_xml_leaf_values(registered_xml);
    if (gw2_is_raise(live)) return live;
    json_t *want = gw2_task_xml_leaf_values(template_xml);
    if (gw2_is_raise(want)) { json_free(live); return want; }
    if (!live || !want) { json_free(live); json_free(want); return json_array(); }
    const char *missing[3]; size_t nmissing = 0;
    const char *differs[3]; size_t ndiffers = 0;
    for (size_t i = 0; i < 3; i++) {
        const char *path = GW2_DRIFT_LEAVES[i].path;
        const json_t *wv = json_obj_get(want, path);
        const json_t *lv = json_obj_get(live, path);
        if (wv && !lv) missing[nmissing++] = GW2_DRIFT_LEAVES[i].label;
        else if (wv && lv) {
            const char *ls = gw2_jstr(lv), *ws = gw2_jstr(wv);
            if (strcmp(ls ? ls : "", ws ? ws : "") != 0)
                differs[ndiffers++] = GW2_DRIFT_LEAVES[i].label;
        }
    }
    json_t *drift = json_array();
    if (nmissing) {
        gw2_buf b; gw2_binit(&b);
        gw2_bputs(&b, "missing: ");
        for (size_t i = 0; i < nmissing; i++) {
            if (i) gw2_bputs(&b, ", ");
            gw2_bputs(&b, missing[i]);
        }
        json_append(drift, json_string(b.p ? b.p : ""));
        gw2_bfree(&b);
    }
    for (size_t i = 0; i < ndiffers; i++) {
        char *frag = gw2_xsf("%s differs", differs[i]);
        json_append(drift, json_string(frag));
        free(frag);
    }
    const json_t *live_logon = json_obj_get(live, GW2_LOGON_LEAF);
    if (gw2_jtruth(live_logon)) {
        const json_t *want_logon = json_obj_get(want, GW2_LOGON_LEAF);
        const char *ls = gw2_jstr(live_logon);
        const char *ws = gw2_jstr(want_logon);
        if (!ws || strcmp(ls ? ls : "", ws) != 0) {
            char *frag = gw2_xsf("LogonType %s", ls ? ls : "");
            json_append(drift, json_string(frag));
            free(frag);
        }
    }
    const char *lv = json_get_str(live, "Task@version", "");
    const char *wv = json_get_str(want, "Task@version", "");
    if (strcmp(lv, wv) != 0) {
        char *frag = gw2_xsf("version %s vs %s", lv, wv);
        json_append(drift, json_string(frag));
        free(frag);
    }
    json_free(live);
    json_free(want);
    return drift;
}

/* ── scheduled_task_drift ── */
/* PoP: gw2_scheduled_task_drift @ hermes_cli/gateway_windows.py:scheduled_task_drift */
static json_t *gw2_scheduled_task_drift(const char *task_name, const gw2_seams_t *s) {
    json_t *reg = gw2_query_scheduled_task_xml(task_name, s);
    if (gw2_is_raise(reg)) return reg;
    if (!reg) return json_array();
    /* template args evaluated left-to-right: script path, suffix, user, build */
    json_t *spr = gw2_call0(s, "get_task_script_path");
    if (gw2_is_raise(spr)) { json_free(reg); return spr; }
    const char *sp = gw2_jstr(spr);
    if (!sp) {
        json_free(spr); json_free(reg);
        return gw2_raise_spec("AttributeError", "'NoneType' object has no attribute 'with_suffix'");
    }
    char *vbs = gw2_with_suffix(sp, ".vbs");
    json_free(spr);
    if (!vbs) {
        json_free(reg);
        return gw2_raise_spec("ValueError", "empty name");
    }
    json_t *ur = gw2_call0(s, "resolve_task_user");
    if (gw2_is_raise(ur)) { json_free(reg); free(vbs); return ur; }
    json_t *ba = gw2_args();
    gw2_aset_s(ba, "task_name", task_name);
    gw2_aset_s(ba, "script_path", vbs);
    gw2_aset_j(ba, "user", ur);
    json_t *tr = gw2_call(s, "build_scheduled_task_xml", ba);
    json_free(ba);
    free(vbs);
    json_free(ur);
    if (gw2_is_raise(tr)) { json_free(reg); return tr; }
    json_t *res = gw2_compare_scheduled_task_drift(reg, tr);
    json_free(reg);
    json_free(tr);
    return res;
}

/* ── _print_scheduled_task_drift ── */
/* PoP: gw2_print_scheduled_task_drift @ hermes_cli/gateway_windows.py:_print_scheduled_task_drift */
static json_t *gw2_print_scheduled_task_drift(const char *task_name, const gw2_seams_t *s) {
    json_t *drift = gw2_scheduled_task_drift(task_name, s);
    if (gw2_is_raise(drift)) return drift;
    if (gw2_jtruth(drift)) {
        gw2_buf b; gw2_binit(&b);
        for (size_t i = 0; i < json_len(drift); i++) {
            if (i) gw2_bputs(&b, "; ");
            const char *t = gw2_jstr(json_get(drift, i));
            gw2_bputs(&b, t ? t : "");
        }
        char *msg = gw2_xsf("⚠ Scheduled Task registration predates the current template (%s)",
                            b.p ? b.p : "");
        gw2_bfree(&b);
        json_t *a = gw2_args();
        gw2_aset_s(a, "text", msg);
        json_t *r = gw2_call(s, "print", a);
        json_free(a);
        free(msg);
        if (gw2_is_raise(r)) { json_free(drift); return r; }
        json_free(r);
        json_t *a2 = gw2_args();
        gw2_aset_s(a2, "text", "  Repair: hermes gateway start  (or: hermes gateway install)");
        json_t *r2 = gw2_call(s, "print", a2);
        json_free(a2);
        if (gw2_is_raise(r2)) { json_free(drift); return r2; }
        json_free(r2);
    }
    json_free(drift);
    return NULL;
}

/* ── registered_task_launchers ── */
/* scan ^"\(Hermes_Gateway[^"\]*)"` (MULTILINE) — the run may cross newlines;
 * matches are non-overlapping and continue from the match end */
static void gw2_scan_task_names(const char *listing, json_t *names) {
    const char *prefix = "\"\\Hermes_Gateway";
    size_t plen = strlen(prefix);
    size_t len = strlen(listing);
    size_t pos = 0;
    while (pos < len) {
        if (pos != 0 && listing[pos - 1] != '\n') {
            while (pos < len && listing[pos - 1] != '\n') pos++;
            if (pos >= len) break;
        }
        if (len - pos >= plen && !strncmp(listing + pos, prefix, plen)) {
            size_t e = pos + plen;
            while (e < len && listing[e] != '"' && listing[e] != '\\') e++;
            if (e < len && listing[e] == '"') {
                char *name = strndup(listing + pos + 2, e - (pos + 2));
                bool dup = false;
                for (size_t i = 0; i < json_len(names); i++) {
                    const char *old = gw2_jstr(json_get(names, i));
                    if (old && !strcmp(old, name)) { dup = true; break; }
                }
                if (!dup) json_append(names, json_string(name));
                free(name);
                pos = e + 1;
                continue;
            }
        }
        pos++;
    }
}

/* PoP: gw2_registered_task_launchers @ hermes_cli/gateway_windows.py:registered_task_launchers */
static json_t *gw2_registered_task_launchers(const char *platform, const gw2_seams_t *s) {
    if (!platform || strcmp(platform, "win32") != 0) return json_array();
    json_t *ir = gw2_call0(s, "import_service_definition_launchers");
    if (gw2_is_raise(ir)) return ir;
    json_free(ir);
    json_t *a = gw2_args();
    json_t *argv = json_array();
    json_append(argv, json_string("/Query"));
    json_append(argv, json_string("/FO"));
    json_append(argv, json_string("CSV"));
    json_append(argv, json_string("/NH"));
    json_set(a, "argv", argv);
    gw2_aset_n(a, "timeout", 5);  /* _TASK_PROBE_TIMEOUT_S */
    json_t *lr = gw2_call(s, "exec_schtasks", a);
    json_free(a);
    if (gw2_is_raise(lr)) return lr;
    if (!lr || lr->type != JSON_OBJECT) {
        char *msg = gw2_xsf("'%s' object is not subscriptable", gw2_typename(lr));
        json_free(lr);
        json_t *e = gw2_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    double code = json_get_num(lr, "code", 0);
    const json_t *out = json_obj_get(lr, "out");
    json_t *names = json_array();
    if (code == 0) {
        const char *listing = gw2_jstr(out);
        if (!listing) {
            char *msg = gw2_xsf("expected string or bytes-like object, got '%s'", gw2_typename(out));
            json_free(lr); json_free(names);
            json_t *e = gw2_raise_spec("TypeError", msg);
            free(msg);
            return e;
        }
        gw2_scan_task_names(listing, names);
    }
    json_free(lr);
    json_t *launchers = json_array();
    for (size_t i = 0; i < json_len(names); i++) {
        const char *name = gw2_jstr(json_get(names, i));
        json_t *xa = gw2_args();
        json_t *xargv = json_array();
        json_append(xargv, json_string("/Query"));
        json_append(xargv, json_string("/TN"));
        json_append(xargv, json_string(name ? name : ""));
        json_append(xargv, json_string("/XML"));
        json_set(xa, "argv", xargv);
        gw2_aset_n(xa, "timeout", 5);
        json_t *xr = gw2_call(s, "exec_schtasks", xa);
        json_free(xa);
        if (gw2_is_raise(xr)) { json_free(names); json_free(launchers); return xr; }
        if (!xr || xr->type != JSON_OBJECT) {
            char *msg = gw2_xsf("'%s' object is not subscriptable", gw2_typename(xr));
            json_free(xr); json_free(names); json_free(launchers);
            json_t *e = gw2_raise_spec("TypeError", msg);
            free(msg);
            return e;
        }
        double xcode = json_get_num(xr, "code", 0);
        const json_t *xout = json_obj_get(xr, "out");
        json_t *leaves = NULL;
        if (xcode == 0) {
            leaves = gw2_task_xml_leaf_values(xout);
            if (gw2_is_raise(leaves)) { json_free(xr); json_free(names); json_free(launchers); return leaves; }
        }
        json_free(xr);
        if (!leaves) leaves = json_object();  /* (None) or {} */
        const char *cmd_raw = json_get_str(leaves, "Task/Actions/Exec/Command", "");
        char *command = gw2_strip_quotes(cmd_raw);
        const char *args_s = json_get_str(leaves, "Task/Actions/Exec/Arguments", "");
        char *definition = gw2_xsf("\"%s\" %s", command, args_s);
        free(command);
        json_t *sa = gw2_args();
        gw2_aset_s(sa, "definition", definition);
        json_t *sr = gw2_call(s, "service_definition_launchers", sa);
        json_free(sa);
        free(definition);
        json_free(leaves);
        if (gw2_is_raise(sr)) { json_free(names); json_free(launchers); return sr; }
        if (sr && sr->type == JSON_ARRAY) {
            for (size_t j = 0; j < sr->c.count; j++)
                json_append(launchers, json_copy(sr->c.items[j]));
        } else {
            /* launchers += non-iterable -> TypeError (no try live) */
            char *msg = gw2_xsf("'%s' object is not iterable", gw2_typename(sr));
            json_free(sr); json_free(names); json_free(launchers);
            json_t *e = gw2_raise_spec("TypeError", msg);
            free(msg);
            return e;
        }
        json_free(sr);
    }
    json_free(names);
    return launchers;
}

/* ── _launcher_owned_exit_line ── */
/* PoP: gw2_launcher_owned_exit_line @ hermes_cli/gateway_windows.py:_launcher_owned_exit_line */
static json_t *gw2_launcher_owned_exit_line(const char *hermes_home, const gw2_seams_t *s) {
    json_t *ir = gw2_call0(s, "import_launcher_pid_file");
    if (gw2_is_raise(ir)) return ir;
    json_free(ir);
    json_t *pa = gw2_args();
    gw2_aset_s(pa, "hermes_home", hermes_home);
    json_t *pr = gw2_call(s, "launcher_pid_file", pa);
    json_free(pa);
    if (gw2_is_raise(pr)) return pr;
    json_t *qa = gw2_args();
    gw2_aset_j(qa, "value", pr);
    json_t *qr = gw2_call(s, "quote_vbs_string", qa);
    json_free(qa);
    json_free(pr);
    if (gw2_is_raise(qr)) return qr;
    char *quoted = gw2_pystr(qr);
    json_free(qr);
    char *line = gw2_xsf(
        "If code = 1 And CreateObject(\"Scripting.FileSystemObject\").FileExists(%s) Then code = 0",
        quoted);
    free(quoted);
    json_t *res = json_string(line);
    free(line);
    return res;
}

/* ── _launcher_pythonpath_entries ── */
/* PoP: gw2_launcher_pythonpath_entries @ hermes_cli/gateway_windows.py:_launcher_pythonpath_entries */
static json_t *gw2_launcher_pythonpath_entries(const json_t *extra_pythonpath, const gw2_seams_t *s) {
    json_t *rr = gw2_call0(s, "project_root_str");
    if (gw2_is_raise(rr)) return rr;
    const char *root = gw2_jstr(rr);
    if (!root) {
        char *msg = gw2_xsf(
            "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
            gw2_typename(rr));
        json_free(rr);
        json_t *e = gw2_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    char *norm = gw2_path_norm(root);
    json_free(rr);
    json_t *pa = gw2_args();
    gw2_aset_s(pa, "value", norm);
    json_t *pr = gw2_call(s, "preserve_hermes_home_path", pa);
    json_free(pa);
    free(norm);
    if (gw2_is_raise(pr)) return pr;
    json_t *out = json_array();
    json_append(out, pr);
    if (!extra_pythonpath || extra_pythonpath->type != JSON_ARRAY) {
        char *msg = gw2_xsf("'%s' object is not iterable", gw2_typename(extra_pythonpath));
        json_free(out);
        json_t *e = gw2_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    for (size_t i = 0; i < extra_pythonpath->c.count; i++) {
        json_t *ea = gw2_args();
        gw2_aset_j(ea, "value", extra_pythonpath->c.items[i]);
        json_t *er = gw2_call(s, "preserve_hermes_home_path", ea);
        json_free(ea);
        if (gw2_is_raise(er)) { json_free(out); return er; }
        json_append(out, er);
    }
    return out;
}

/* ── _render_launchers ── */
/* PoP: gw2_render_launchers @ hermes_cli/gateway_windows.py:_render_launchers */
static json_t *gw2_render_launchers(const json_t *settings, const json_t *script_path,
                                    const gw2_seams_t *s) {
    const char *keys[4] = {"python_path", "working_dir", "hermes_home", "profile_arg"};
    const char *vals[4];
    for (int i = 0; i < 4; i++) {
        const json_t *v = (settings && settings->type == JSON_ARRAY) ? json_get(settings, (size_t)i) : NULL;
        vals[i] = gw2_jstr(v);
        if (!vals[i]) vals[i] = "";
    }
    json_t *ca = gw2_args();
    for (int i = 0; i < 4; i++) gw2_aset_s(ca, keys[i], vals[i]);
    json_t *cmd = gw2_call(s, "build_gateway_cmd_script", ca);
    json_free(ca);
    if (gw2_is_raise(cmd)) return cmd;
    /* second tuple: script_path.with_suffix(".vbs") BEFORE the vbs build call */
    const char *sp = gw2_jstr(script_path);
    if (!sp) {
        json_free(cmd);
        return gw2_raise_spec("AttributeError", "'NoneType' object has no attribute 'with_suffix'");
    }
    char *vbs_path = gw2_with_suffix(sp, ".vbs");
    if (!vbs_path) {
        json_free(cmd);
        return gw2_raise_spec("ValueError", "empty name");
    }
    json_t *va = gw2_args();
    for (int i = 0; i < 4; i++) gw2_aset_s(va, keys[i], vals[i]);
    json_t *vbs = gw2_call(s, "build_gateway_vbs_script", va);
    json_free(va);
    if (gw2_is_raise(vbs)) { free(vbs_path); json_free(cmd); return vbs; }
    json_t *out = json_array();
    json_t *t1 = json_array();
    json_append(t1, script_path ? json_copy(script_path) : json_null());
    json_append(t1, cmd ? cmd : json_null());
    json_append(t1, json_string("utf-8"));
    json_append(out, t1);
    json_t *t2 = json_array();
    json_append(t2, json_string(vbs_path));
    json_append(t2, vbs ? vbs : json_null());
    json_append(t2, json_string("utf-16"));
    json_append(out, t2);
    free(vbs_path);
    return out;
}

/* ── _atomic_write ── */
/* PoP: gw2_atomic_write @ hermes_cli/gateway_windows.py:_atomic_write */
static json_t *gw2_atomic_write(const char *path, const json_t *content, const char *tmp,
                                const char *encoding, const gw2_seams_t *s) {
    json_t *pending = NULL;
    json_t *wa = gw2_args();
    gw2_aset_s(wa, "path", tmp);
    gw2_aset_j(wa, "content", content);
    gw2_aset_s(wa, "encoding", encoding ? encoding : "utf-8");
    gw2_aset_s(wa, "newline", "");
    json_t *wr = gw2_call(s, "path_write_text", wa);
    json_free(wa);
    if (gw2_is_raise(wr)) pending = wr;
    else json_free(wr);
    if (!pending) {
        json_t *ra = gw2_args();
        gw2_aset_s(ra, "src", tmp);
        gw2_aset_s(ra, "dst", path);
        json_t *rr = gw2_call(s, "path_replace", ra);
        json_free(ra);
        if (gw2_is_raise(rr)) pending = rr;
        else json_free(rr);
    }
    /* finally: tmp.unlink(missing_ok=True); OSError swallowed; any OTHER
     * raise replaces the in-flight exception (Python finally semantics) */
    json_t *ua = gw2_args();
    gw2_aset_s(ua, "path", tmp);
    gw2_aset_b(ua, "missing_ok", true);
    json_t *ur = gw2_call(s, "path_unlink", ua);
    json_free(ua);
    if (gw2_is_raise(ur)) {
        if (gw2_caught(ur, "OSError")) json_free(ur);
        else { if (pending) json_free(pending); pending = ur; }
    } else json_free(ur);
    return pending;  /* NULL == None */
}

/* ── _write_launcher_scripts ── */
/* PoP: gw2_write_launcher_scripts @ hermes_cli/gateway_windows.py:_write_launcher_scripts */
static json_t *gw2_write_launcher_scripts(const json_t *scripts, const gw2_seams_t *s) {
    if (!scripts || scripts->type != JSON_ARRAY) {
        char *msg = gw2_xsf("'%s' object is not iterable", gw2_typename(scripts));
        json_t *e = gw2_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    for (size_t i = 0; i < scripts->c.count; i++) {
        const json_t *entry = scripts->c.items[i];
        const json_t *pathn = (entry && entry->type == JSON_ARRAY) ? json_get(entry, 0) : NULL;
        const json_t *content = (entry && entry->type == JSON_ARRAY) ? json_get(entry, 1) : NULL;
        const json_t *encn = (entry && entry->type == JSON_ARRAY) ? json_get(entry, 2) : NULL;
        const char *enc = gw2_jstr(encn);
        const char *p = gw2_jstr(pathn);
        if (!p) {
            const char *meth = (enc && !strcmp(enc, "utf-8")) ? "with_suffix" : "with_name";
            char *msg = gw2_xsf("'NoneType' object has no attribute '%s'", meth);
            json_t *e = gw2_raise_spec("AttributeError", msg);
            free(msg);
            return e;
        }
        char *tmp;
        if (enc && !strcmp(enc, "utf-8")) {
            tmp = gw2_with_suffix(p, ".tmp");
            if (!tmp) return gw2_raise_spec("ValueError", "empty name");
        } else {
            char *name = gw2_path_name(p);
            char *tmpname = gw2_xsf("%s.tmp", name);
            tmp = gw2_with_name(p, tmpname);
            free(name);
            free(tmpname);
            if (!tmp) return gw2_raise_spec("ValueError", "Invalid name ''");
        }
        json_t *r = gw2_atomic_write(p, content, tmp, enc, s);
        free(tmp);
        if (gw2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* ── _report_already_running ── */
/* PoP: gw2_report_already_running @ hermes_cli/gateway_windows.py:_report_already_running */
static json_t *gw2_report_already_running(const json_t *pids, const gw2_seams_t *s) {
    if (!pids || pids->type != JSON_ARRAY) {
        char *msg = gw2_xsf("'%s' object is not iterable", gw2_typename(pids));
        json_t *e = gw2_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    gw2_buf b; gw2_binit(&b);
    for (size_t i = 0; i < pids->c.count; i++) {
        if (i) gw2_bputs(&b, ", ");
        char *ps = gw2_pystr(pids->c.items[i]);
        gw2_bputs(&b, ps);
        free(ps);
    }
    char *text = gw2_xsf("✓ Gateway already running (PID: %s)", b.p ? b.p : "");
    gw2_bfree(&b);
    json_t *a = gw2_args();
    gw2_aset_s(a, "text", text);
    json_t *r = gw2_call(s, "print", a);
    json_free(a);
    free(text);
    if (gw2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── _startup_staging_path ── */
/* PoP: gw2_startup_staging_path @ hermes_cli/gateway_windows.py:_startup_staging_path */
static json_t *gw2_startup_staging_path(const gw2_seams_t *s) {
    json_t *pr = gw2_call0(s, "get_startup_entry_path");
    if (gw2_is_raise(pr)) return pr;
    const char *p = gw2_jstr(pr);
    if (!p) {
        json_free(pr);
        return gw2_raise_spec("AttributeError", "'NoneType' object has no attribute 'with_suffix'");
    }
    char *tmp = gw2_with_suffix(p, ".tmp");
    json_free(pr);
    if (!tmp) return gw2_raise_spec("ValueError", "empty name");
    json_t *res = json_string(tmp);
    free(tmp);
    return res;
}

/* ── _hermes_home ── */
/* PoP: gw2_hermes_home @ hermes_cli/gateway_windows.py:_hermes_home */
static json_t *gw2_hermes_home(const gw2_seams_t *s) {
    json_t *ir = gw2_call0(s, "import_get_hermes_home");
    if (gw2_is_raise(ir)) return ir;
    json_free(ir);
    json_t *hr = gw2_call0(s, "get_hermes_home");
    if (gw2_is_raise(hr)) return hr;
    const char *h = gw2_jstr(hr);
    if (!h) {
        char *msg = gw2_xsf(
            "argument should be a str or an os.PathLike object where __fspath__ returns a str, not '%s'",
            gw2_typename(hr));
        json_free(hr);
        json_t *e = gw2_raise_spec("TypeError", msg);
        free(msg);
        return e;
    }
    char *norm = gw2_path_norm(h);
    json_free(hr);
    json_t *res = json_string(norm);
    free(norm);
    return res;
}

/* ── _mark_planned_restart ── */
/* PoP: gw2_mark_planned_restart @ hermes_cli/gateway_windows.py:_mark_planned_restart */
static json_t *gw2_mark_planned_restart(const gw2_seams_t *s) {
    json_t *ir = gw2_call0(s, "import_write_planned_restart_marker");
    if (gw2_is_raise(ir)) return ir;
    json_free(ir);
    json_t *wr = gw2_call0(s, "write_planned_restart_marker");
    if (gw2_is_raise(wr)) return wr;
    json_free(wr);
    return NULL;
}