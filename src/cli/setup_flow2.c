/*
 * setup_flow2.c — FLOW cores + leaves of hermes_cli/setup.py (LIVE authority)
 * that NO existing PoP claims: the print/info/banner emit leaves, the env-var
 * prompt-and-save leaf, the module-probe leaf, the number/int-setting prompt
 * leaves, the consent-completion leaf, the wizard orchestrators (run_setup_wizard,
 * _run_setup_wizard_impl, _run_setup_section, _run_full_setup), the navigation
 * state ctors/reset (_SetupNavigationState.__init__/reset, _SetupGoBack.__init__)
 * and the wizard's nested closures (_record_answers, _skip, _gateway_step).
 *
 * Self-contained C11, libjson-only. Claimed siblings (prompt, prompt_choice,
 * print_header, is_interactive_stdin, print_noninteractive_setup_guidance,
 * _apply_default_agent_settings, _run_setup_steps, _setup_navigation_scope,
 * _skip_configured_section, setup_gateway, the SETUP_SECTIONS functions) and
 * every foreign collaborator (config helpers, cli_output, colors.color,
 * importlib.util.find_spec, the observability consent/event writers,
 * backup_config, get_active_provider, offer_agent_import, the setup_quick
 * runners, _print_setup_summary, ensure_gateway_service) enter through ONE
 * universal seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam copies what it keeps; st2_call frees args); the result is a
 * NEW json_t* the caller frees (NULL == None; a scripted JSON null is delivered
 * as a NULL pointer; {"__raise__": kind, "msg": ...} == a raised exception).
 * The harness records {"ev": name, "args": args, "ret": result} per call in
 * order; the oracle's recording stubs emit the identical shape driving LIVE
 * Python with the same scripts. The ports perform NO real I/O.
 *
 * Ported-to-ported calls compose REAL (st2_info inside the orchestrators,
 * st2_prompt_number inside st2_prompt_int_setting, st2_run_setup_wizard_impl
 * inside st2_run_setup_wizard, st2_skip inside st2_gateway_step, ...); the
 * oracle keeps the extracted defs real on the same paths, so their internal
 * print/print_info/color seam events appear in BOTH traces.
 *
 * Exception propagation: every st2 flow returns json_t* — a normal result
 * (NULL == None) or a raise spec. `except ValueError` (_prompt_number) swallows
 * ValueError kinds only; `except Exception` (_module_installed) swallows every
 * non-BaseException kind; `except _SetupCancelled` (run_setup_wizard) swallows
 * that kind only. The BaseException trio (SystemExit / KeyboardInterrupt /
 * GeneratorExit) and the _SetupControlFlow family ALWAYS pass through.
 * Fail-closed: a missing/empty seam script yields a RuntimeError raise spec
 * (recorded, then propagated/swallowed exactly as live); no seam installed at
 * all yields the same spec.
 *
 * _setup_navigation_scope is modeled as a context-manager seam pair:
 * setup_navigation_scope.enter {} and setup_navigation_scope.exit {exc}. An
 * enter raise skips the body AND the exit; an exit raise replaces the body's
 * exception (contextlib semantics); exit never suppresses.
 *
 * Steps lists travel as [label, action_name] pairs; action names project the
 * live closures (_step -> "step:<key>", _gateway_step -> "gateway_step",
 * section func -> its live name, _run_quick_setup lambda -> "run_quick_setup",
 * getattr(setup_quick, runner) lambda -> "first_time:<runner>"). The claimed
 * _run_setup_steps seam records them; the closures never execute on that path.
 *
 * Fixture domain (ASCII): paths are verbatim POSIX strings (the oracle seeds
 * PurePosixPath so str() keeps separators); backup_config returns null or
 * {"path": str} (exists() is the backup_path.exists seam); load_config returns
 * objects; prompt results are strings; int/float parse inputs stay in the
 * int64 / finite-float range (bigint and inf/nan results are out of scope);
 * integral floats normalize to ints on both sides and are kept OUT of
 * str()-computed positions (prompt defaults); _FIRST_TIME_MODES index scripts
 * are integers (negative indexes count from the end, live tuple semantics).
 *
 * Seam name table (name -> live call):
 *   print                       builtin print(*values)      args{args:[...]}
 *   print_info                  cli_output.print_info(msg)  args{msg}
 *   print_success               cli_output.print_success(msg)
 *   print_warning               cli_output.print_warning(msg)
 *   print_error                 cli_output.print_error(msg)
 *   color                       colors.color(text, *codes)  args{text,codes}
 *   prompt                      setup.prompt(q, default=, password=) [claimed]
 *   prompt_choice               setup.prompt_choice(q, choices, default) [claimed]
 *   print_header                setup.print_header(title, gap=) [claimed]
 *   is_interactive_stdin        setup.is_interactive_stdin() [claimed]
 *   print_noninteractive_setup_guidance  [claimed] args{reason}
 *   _apply_default_agent_settings        [claimed] args{config}
 *   run_setup_steps             setup._run_setup_steps(steps) [claimed] args{steps}
 *   setup_navigation_scope.enter/exit    [claimed CM] args{} / {exc}
 *   _skip_configured_section    setup_migration._skip_configured_section [claimed]
 *   setup_gateway               setup_platforms.setup_gateway [claimed] {config}
 *   load_config / save_config / get_config_path / get_env_path /
 *   get_hermes_home / get_env_value / ensure_hermes_home / save_env_value
 *                               hermes_cli.config helpers
 *   is_managed / managed_error  hermes_cli.config (in-function import)
 *   backup_config               hermes_cli.config_backups.backup_config
 *   backup_path.exists          the returned backup path's .exists()
 *   get_active_provider         hermes_cli.auth.get_active_provider
 *   offer_agent_import          setup_agent_import.offer_agent_import
 *   run_portal_one_shot         setup_quick._run_portal_one_shot
 *   _print_setup_summary        setup_summary._print_setup_summary
 *   offer_consent_if_undecided  observability.shared_metrics_consent
 *   record_setup_completed      observability.shared_metrics_events
 *   ensure_gateway_service      gateway_setup_service.ensure_gateway_service
 *   find_spec                   importlib.util.find_spec(name)
 *   accept                      _prompt_int_setting's accept callback
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

typedef json_t *(*st2_seam_fn)(void *ctx, const char *name, const json_t *args);

typedef struct st2_io {
    st2_seam_fn call;
    void *ctx;
    const json_t *env;   /* borrowed ambient values (DEFAULT_CONFIG, PROJECT_ROOT) */
} st2_io_t;

typedef struct st2_nav {
    int section_index;
    int prompt_index;
    int active_prompt_index;
    json_t *resolved_choices;   /* owned array */
    json_t *replay_choices;     /* owned array */
} st2_nav_t;

static int st2_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *st2_raise_kind(const json_t *r) {
    const json_t *k = r ? json_obj_get(r, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}

static const char *st2_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}

/* BaseException-only kinds: NOT caught by `except Exception`. */
static int st2_kind_is_base_only(const char *k) {
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0 ||
           strcmp(k, "GeneratorExit") == 0 || strcmp(k, "_SetupCancelled") == 0 ||
           strcmp(k, "_SetupGoBack") == 0 || strcmp(k, "_SetupControlFlow") == 0;
}

static int st2_caught_by_exception(const char *k) {
    return !st2_kind_is_base_only(k);
}

/* `except ValueError` also catches its subclasses. */
static int st2_is_valueerror(const char *k) {
    return strcmp(k, "ValueError") == 0 || strcmp(k, "UnicodeError") == 0 ||
           strcmp(k, "UnicodeDecodeError") == 0 || strcmp(k, "UnicodeEncodeError") == 0 ||
           strcmp(k, "UnicodeTranslateError") == 0;
}

static json_t *st2_raise(const char *kind, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *st2_raise_fmt(const char *kind, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return st2_raise(kind, buf);
}

static char *st2_fmt(const char *fmt, ...) {
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
static json_t *st2_call(st2_io_t *io, const char *name, json_t *args) {
    json_t *r;
    if (io && io->call) {
        r = io->call(io->ctx, name, args);
    } else {
        r = st2_raise_fmt("RuntimeError", "no script for %s", name);
    }
    json_free(args);
    if (r && r->type == JSON_NULL) { json_free(r); r = NULL; }
    return r;
}

static const json_t *st2_env(const st2_io_t *io, const char *key) {
    return (io && io->env) ? json_obj_get(io->env, key) : NULL;
}

static const char *st2_env_str(const st2_io_t *io, const char *key, const char *def) {
    const json_t *v = st2_env(io, key);
    return (v && v->type == JSON_STRING) ? v->str_val : def;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static int st2_truthy(const json_t *v) {
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
static int st2_present(const json_t *v) { return v && v->type != JSON_NULL; }

/* Scalar string reader (json_get_str is an OBJECT+key accessor — never call it
 * on a string value node with a NULL key). */
static const char *st2_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static const char *st2_type_name(const json_t *v) {
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

/* Shortest round-trip float repr matching Python repr(): find the shortest
 * scientific precision that round-trips, then render positional for decimal
 * exponents in [-4, 16) and scientific otherwise (Python's repr rule — %g
 * alone diverges at exponent 16 with 17-digit precision). */
static char *st2_float_repr(double v) {
    char sci[64], buf[64];
    int p;
    for (p = 1; p <= 17; p++) {
        snprintf(sci, sizeof sci, "%.*e", p - 1, v);
        if (strtod(sci, NULL) == v) break;
    }
    const char *epos = strchr(sci, 'e');
    int exp = epos ? atoi(epos + 1) : 0;
    if (exp < -4 || exp >= 16) return strdup(sci);
    int decimals = p - 1 - exp;
    if (decimals < 0) decimals = 0;
    snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return strdup(buf);
}

/* str() of a scalar JSON value (containers are not interpolated in this
 * cluster; integral floats are out of the str() fixture domain). */
static char *st2_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) {
            snprintf(buf, sizeof buf, "%.0f", d);
            return strdup(buf);
        }
        return st2_float_repr(d);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}

/* ── Python int()/float() string parsers (ASCII domain) ── */

static const char ST2_PARSE_WS[] = " \t\n\r\v\f";

static const char *st2_parse_strip(const char *s, size_t *n) {
    while (*s && strchr(ST2_PARSE_WS, *s)) s++;
    *n = strlen(s);
    while (*n > 0 && strchr(ST2_PARSE_WS, s[*n - 1])) (*n)--;
    return s;
}

static int st2_is_digit(char c) { return c >= '0' && c <= '9'; }

/* int(s): optional sign, digits with underscores ONLY between digits, base 10.
 * *ok=1 on success. Out-of-int64 inputs fail (fixture domain restriction). */
static long long st2_parse_int(const char *s, int *ok) {
    *ok = 0;
    if (!s) return 0;
    size_t n;
    s = st2_parse_strip(s, &n);
    if (n == 0) return 0;
    size_t i = 0;
    int neg = 0;
    if (s[0] == '+' || s[0] == '-') { neg = (s[0] == '-'); i = 1; }
    char buf[512];
    size_t bn = 0;
    int any = 0;
    for (; i < n; i++) {
        char c = s[i];
        if (st2_is_digit(c)) {
            if (bn >= sizeof buf - 1) return 0;
            buf[bn++] = c;
            any = 1;
        } else if (c == '_') {
            if (!any) return 0;
            if (i + 1 >= n || !st2_is_digit(s[i + 1])) return 0;
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

/* float(s): sign, inf/infinity/nan specials, mantissa digits/underscores with
 * optional fraction, optional exponent. *ok: 1 finite success, 0 ValueError,
 * 2 non-finite result (inf/nan/overflow — outside the JSON fixture domain). */
static double st2_parse_float(const char *s, int *ok) {
    *ok = 0;
    if (!s) return 0.0;
    size_t n;
    s = st2_parse_strip(s, &n);
    if (n == 0) return 0.0;
    size_t i = 0;
    char sign = 0;
    if (s[0] == '+' || s[0] == '-') { sign = s[i]; i = 1; }
    /* inf / infinity / nan (case-insensitive, no underscores) */
    {
        char low[16];
        size_t rn = n - i;
        if (rn < sizeof low) {
            for (size_t j = 0; j < rn; j++) {
                char c = s[i + j];
                low[j] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
            }
            low[rn] = '\0';
            if (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0) {
                *ok = 2;
                return sign == '-' ? -INFINITY : INFINITY;
            }
            if (strcmp(low, "nan") == 0) { *ok = 2; return NAN; }
        }
    }
    char buf[512];
    size_t bn = 0;
    if (sign) buf[bn++] = sign;
    int intdigits = 0, fracdigits = 0, prev_digit = 0;
    while (i < n) {
        char c = s[i];
        if (st2_is_digit(c)) {
            if (bn >= sizeof buf - 1) return 0.0;
            buf[bn++] = c; intdigits++; prev_digit = 1; i++;
        } else if (c == '_') {
            if (!prev_digit) return 0.0;
            if (i + 1 >= n || !st2_is_digit(s[i + 1])) return 0.0;
            prev_digit = 0; i++;
        } else break;
    }
    if (i < n && s[i] == '.') {
        if (bn >= sizeof buf - 1) return 0.0;
        buf[bn++] = '.'; i++;
        prev_digit = 0;
        while (i < n) {
            char c = s[i];
            if (st2_is_digit(c)) {
                if (bn >= sizeof buf - 1) return 0.0;
                buf[bn++] = c; fracdigits++; prev_digit = 1; i++;
            } else if (c == '_') {
                if (!prev_digit) return 0.0;
                if (i + 1 >= n || !st2_is_digit(s[i + 1])) return 0.0;
                prev_digit = 0; i++;
            } else break;
        }
    }
    if (intdigits + fracdigits == 0) return 0.0;
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        if (bn >= sizeof buf - 1) return 0.0;
        buf[bn++] = 'e'; i++;
        if (i < n && (s[i] == '+' || s[i] == '-')) {
            if (bn >= sizeof buf - 1) return 0.0;
            buf[bn++] = s[i++];
        }
        int expdigits = 0;
        prev_digit = 0;
        while (i < n) {
            char c = s[i];
            if (st2_is_digit(c)) {
                if (bn >= sizeof buf - 1) return 0.0;
                buf[bn++] = c; expdigits++; prev_digit = 1; i++;
            } else if (c == '_') {
                if (!prev_digit) return 0.0;
                if (i + 1 >= n || !st2_is_digit(s[i + 1])) return 0.0;
                prev_digit = 0; i++;
            } else break;
        }
        if (!expdigits) return 0.0;
    }
    if (i != n) return 0.0;
    buf[bn] = '\0';
    char *end = NULL;
    double v = strtod(buf, &end);
    if (!end || *end) return 0.0;
    if (!isfinite(v)) { *ok = 2; return v; }
    *ok = 1;
    return v;
}

/* ── seam argument builders ── */

static json_t *st2_msg_args(const char *msg) {
    json_t *a = json_object();
    json_set(a, "msg", json_string(msg ? msg : ""));
    return a;
}

static json_t *st2_name_args(const char *name) {
    json_t *a = json_object();
    json_set(a, "name", json_string(name ? name : ""));
    return a;
}

static json_t *st2_config_args(const json_t *config) {
    json_t *a = json_object();
    json_set(a, "config", config ? json_copy(config) : json_null());
    return a;
}

static json_t *st2_steps_args(const json_t *steps) {
    json_t *a = json_object();
    json_set(a, "steps", json_copy(steps));
    return a;
}

static json_t *st2_step_pair(const char *label, const char *action) {
    json_t *p = json_array();
    json_append(p, json_string(label));
    json_append(p, json_string(action));
    return p;
}

static json_t *st2_exc_json(const json_t *raise_marker) {
    json_t *e = json_object();
    json_set(e, "kind", json_string(st2_raise_kind(raise_marker)));
    json_set(e, "msg", json_string(st2_raise_msg(raise_marker)));
    return e;
}

/* ── emit leaves (print / print_info / color) ── */

/* print(*values): values is a borrowed array (NULL == bare print()). */
static json_t *st2_print(st2_io_t *io, const json_t *values) {
    json_t *a = json_object();
    json_set(a, "args", values ? json_copy(values) : json_array());
    json_t *r = st2_call(io, "print", a);
    if (st2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* print_info / print_success / print_warning / print_error (msg). */
static json_t *st2_print_msg(st2_io_t *io, const char *name, const char *msg) {
    json_t *r = st2_call(io, name, st2_msg_args(msg));
    if (st2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* color(text, code) — result owned by the caller. */
static json_t *st2_color(st2_io_t *io, const json_t *text, const char *code) {
    json_t *a = json_object();
    json_set(a, "text", text ? json_copy(text) : json_null());
    json_t *codes = json_array();
    json_append(codes, json_string(code));
    json_set(a, "codes", codes);
    return st2_call(io, "color", a);
}

static json_t *st2_print_header(st2_io_t *io, const char *title, int gap) {
    json_t *a = json_object();
    json_set(a, "title", json_string(title));
    json_set(a, "gap", json_bool(gap ? true : false));
    json_t *r = st2_call(io, "print_header", a);
    if (st2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* Text tables transcribed byte-exact from the live quarry module. */
#define ST2_MAGENTA "\x1b[35m"
#define ST2_CYAN    "\x1b[36m"
#define ST2_DASH    "\xe2\x94\x80"
#define ST2_EMDASH  "\xe2\x80\x94"
#define ST2_BAR     "\xe2\x94\x82"
#define ST2_CADUCEUS "\xe2\x98\xa4"

static const char ST2_BANNER_TOP[] =
    "\xe2\x94\x8c"
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    "\xe2\x94\x90";
static const char ST2_BANNER_BOTTOM[] =
    "\xe2\x94\x94"
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    "\xe2\x94\x98";

static const char *const ST2_WIZARD_BANNER[] = {
    ST2_BAR "             " ST2_CADUCEUS " Hermes Agent Setup Wizard                " ST2_BAR,
    "\xe2\x94\x9c"
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH ST2_DASH
    "\xe2\x94\xa4",
    ST2_BAR "  Let's configure your Hermes Agent installation.       " ST2_BAR,
    ST2_BAR "  Press Ctrl+C at any time to exit.                     " ST2_BAR
};

static const char *const ST2_RECONFIG_INFO[] = {
    "Running the full wizard " ST2_EMDASH " each prompt shows your current value.",
    "Press Enter to keep it, or type a new value to change it.",
    "",
    "Tip: jump straight to a section with 'hermes setup model|terminal|",
    "     gateway|tools|agent', or fill only missing items with --quick."
};

static const struct { const char *key; const char *label; const char *fn; } ST2_SECTIONS[] = {
    {"model", "Model & Provider", "setup_model_provider"},
    {"tts", "Text-to-Speech", "setup_tts"},
    {"terminal", "Terminal Backend", "setup_terminal_backend"},
    {"gateway", "Messaging Platforms (Gateway)", "setup_gateway"},
    {"tools", "Tools", "setup_tools"},
    {"telemetry", "Shared Metrics", "setup_telemetry"},
    {"agent", "Agent Settings", "setup_agent_settings"}
};
#define ST2_NSECTIONS ((int)(sizeof ST2_SECTIONS / sizeof ST2_SECTIONS[0]))

static const char ST2_AVAILABLE[] =
    "Available sections: model, tts, terminal, gateway, tools, telemetry, agent";

static const struct { const char *label; const char *runner; } ST2_MODES[] = {
    {"Quick Setup (Nous Portal) " ST2_EMDASH " free OAuth login, no API keys, model + tools (recommended)",
     "_run_first_time_quick_setup"},
    {"Full setup " ST2_EMDASH " configure every provider, tool & option yourself (bring your own keys)",
     NULL},
    {"Blank Slate " ST2_EMDASH " everything off except the bare minimum; opt in to each capability",
     "_run_blank_slate_setup"}
};
#define ST2_NMODES ((int)(sizeof ST2_MODES / sizeof ST2_MODES[0]))

/* ── navigation state helpers ── */

/* _SetupNavigationState.reset: deepcopy(replay or []) into fresh counters. */
static void st2_nav_reset_core(st2_nav_t *s, int section_index, const json_t *replay) {
    s->section_index = section_index;
    s->prompt_index = 0;
    s->active_prompt_index = -1;
    json_free(s->resolved_choices);
    s->resolved_choices = json_array();
    json_free(s->replay_choices);
    s->replay_choices = st2_truthy(replay) ? json_copy(replay) : json_array();
}

static json_t *st2_nav_to_json(const st2_nav_t *s) {
    json_t *o = json_object();
    json_set(o, "section_index", json_number((double)s->section_index));
    json_set(o, "prompt_index", json_number((double)s->prompt_index));
    json_set(o, "active_prompt_index", json_number((double)s->active_prompt_index));
    json_set(o, "resolved_choices", json_copy(s->resolved_choices));
    json_set(o, "replay_choices", json_copy(s->replay_choices));
    return o;
}

/* Load a nav double from fixture JSON (NULL -> defaults). Arrays copied. */
static void st2_nav_load(const json_t *j, st2_nav_t *s) {
    s->section_index = (int)json_get_num(j, "section_index", -1.0);
    s->prompt_index = (int)json_get_num(j, "prompt_index", 0.0);
    s->active_prompt_index = (int)json_get_num(j, "active_prompt_index", -1.0);
    const json_t *rc = j ? json_obj_get(j, "resolved_choices") : NULL;
    const json_t *rp = j ? json_obj_get(j, "replay_choices") : NULL;
    s->resolved_choices = (rc && rc->type == JSON_ARRAY) ? json_copy(rc) : json_array();
    s->replay_choices = (rp && rp->type == JSON_ARRAY) ? json_copy(rp) : json_array();
}

static void st2_nav_free(st2_nav_t *s) {
    json_free(s->resolved_choices);
    json_free(s->replay_choices);
    s->resolved_choices = NULL;
    s->replay_choices = NULL;
}

/* ── ported leaves ── */

/* PoP: st2_info @ hermes_cli/setup.py:_info */
json_t *st2_info(st2_io_t *io, const json_t *lines) {
    if (!lines || lines->type != JSON_ARRAY) return NULL;
    for (size_t i = 0; i < json_len(lines); i++) {
        const json_t *ln = json_get(lines, i);
        json_t *a = json_object();
        json_t *r;
        if (!ln || ln->type == JSON_NULL) {
            json_set(a, "args", json_array());
            r = st2_call(io, "print", a);
        } else {
            json_set(a, "msg", json_copy(ln));
            r = st2_call(io, "print_info", a);
        }
        if (st2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* PoP: st2_sub_dict @ hermes_cli/setup.py:_sub_dict */
json_t *st2_sub_dict(json_t *parent, const char *key) {
    if (!parent || parent->type != JSON_OBJECT)
        return st2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                             st2_type_name(parent));
    json_t *child = json_obj_get(parent, key);
    if (!child || child->type != JSON_OBJECT) {
        child = json_object();
        json_set(parent, key, child);
    }
    return json_copy(child);
}

/* PoP: st2_section_rule @ hermes_cli/setup.py:_section_rule */
json_t *st2_section_rule(st2_io_t *io, const char *title) {
    json_t *r = st2_print(io, NULL);
    if (st2_is_raise(r)) return r;
    char *t = st2_fmt("  " ST2_DASH ST2_DASH ST2_DASH " %s " ST2_DASH ST2_DASH ST2_DASH,
                      title ? title : "");
    json_t *text = json_string(t ? t : "");
    free(t);
    json_t *c = st2_color(io, text, ST2_CYAN);
    json_free(text);
    if (st2_is_raise(c)) return c;
    json_t *va = json_array();
    json_append(va, c ? c : json_null());   /* seam NULL == None: print(None) keeps it */
    r = st2_print(io, va);
    json_free(va);
    if (st2_is_raise(r)) return r;
    r = st2_print(io, NULL);
    if (st2_is_raise(r)) return r;
    return NULL;
}

/* PoP: st2_print_banner @ hermes_cli/setup.py:_print_banner */
json_t *st2_print_banner(st2_io_t *io, const json_t *lines) {
    json_t *r = st2_print(io, NULL);
    if (st2_is_raise(r)) return r;
    json_t *top = json_string(ST2_BANNER_TOP);
    json_t *c = st2_color(io, top, ST2_MAGENTA);
    json_free(top);
    if (st2_is_raise(c)) return c;
    json_t *va = json_array();
    json_append(va, c ? c : json_null());   /* seam NULL == None: print(None) keeps it */
    r = st2_print(io, va);
    json_free(va);
    if (st2_is_raise(r)) return r;
    if (lines && lines->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(lines); i++) {
            c = st2_color(io, json_get(lines, i), ST2_MAGENTA);
            if (st2_is_raise(c)) return c;
            va = json_array();
            json_append(va, c ? c : json_null());   /* seam NULL == None: print(None) keeps it */
            r = st2_print(io, va);
            json_free(va);
            if (st2_is_raise(r)) return r;
        }
    }
    json_t *bottom = json_string(ST2_BANNER_BOTTOM);
    c = st2_color(io, bottom, ST2_MAGENTA);
    json_free(bottom);
    if (st2_is_raise(c)) return c;
    va = json_array();
    json_append(va, c ? c : json_null());   /* seam NULL == None: print(None) keeps it */
    r = st2_print(io, va);
    json_free(va);
    if (st2_is_raise(r)) return r;
    return NULL;
}

/* PoP: st2_module_installed @ hermes_cli/setup.py:_module_installed */
json_t *st2_module_installed(st2_io_t *io, const char *name) {
    json_t *r = st2_call(io, "find_spec", st2_name_args(name));
    if (st2_is_raise(r)) {
        if (!st2_caught_by_exception(st2_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false);
    }
    int found = st2_present(r);
    json_free(r);
    return json_bool(found);
}

/* PoP: st2_prompt_number @ hermes_cli/setup.py:_prompt_number */
json_t *st2_prompt_number(st2_io_t *io, const char *label, const json_t *current,
                          const char *cast) {
    char *def = st2_py_str(current);
    json_t *a = json_object();
    json_set(a, "question", json_string(label ? label : ""));
    json_set(a, "default", json_string(def ? def : ""));
    free(def);
    json_t *r = st2_call(io, "prompt", a);
    if (st2_is_raise(r)) {
        /* except ValueError: return None — other kinds propagate. */
        if (st2_is_valueerror(st2_raise_kind(r))) { json_free(r); return NULL; }
        return r;
    }
    /* Fixture domain: the prompt result is a string. */
    const char *s = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *out = NULL;
    if (s) {
        int ok = 0;
        if (cast && strcmp(cast, "float") == 0) {
            double v = st2_parse_float(s, &ok);
            if (ok == 1) out = json_number(v);   /* ok==2 non-finite: out of domain */
        } else {
            long long v = st2_parse_int(s, &ok);
            if (ok) out = json_number((double)v);
        }
    }
    json_free(r);
    return out;   /* NULL == parse ValueError -> None */
}

/* PoP: st2_prompt_int_setting @ hermes_cli/setup.py:_prompt_int_setting */
json_t *st2_prompt_int_setting(st2_io_t *io, json_t *section, const char *key,
                               const char *label, const json_t *current) {
    json_t *v = st2_prompt_number(io, label, current, "int");
    if (st2_is_raise(v)) return v;
    if (!v) return NULL;                     /* value is None: accept never runs */
    json_t *a = json_object();
    json_set(a, "value", json_copy(v));
    json_t *r = st2_call(io, "accept", a);
    if (st2_is_raise(r)) { json_free(v); return r; }
    int acc = st2_truthy(r);
    json_free(r);
    if (acc) {
        if (!section || section->type != JSON_OBJECT) {
            json_free(v);
            return st2_raise_fmt("TypeError", "'%s' object does not support item assignment",
                                 st2_type_name(section));
        }
        json_set(section, key, v);           /* takes ownership of v */
        return NULL;
    }
    json_free(v);
    return NULL;
}

/* PoP: st2_prompt_and_save_env_var @ hermes_cli/setup.py:_prompt_and_save_env_var */
json_t *st2_prompt_and_save_env_var(st2_io_t *io, const json_t *var,
                                    const char *saved_msg, const char *skipped_msg) {
    if (!var || var->type != JSON_OBJECT)
        return st2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                             st2_type_name(var));
    const json_t *name = json_obj_get(var, "name");
    if (!name) return st2_raise("KeyError", "'name'");   /* var['name'] in the f-string */
    const json_t *pr = json_obj_get(var, "prompt");
    char *qs = pr ? st2_py_str(pr) : st2_py_str(name);
    char *q = st2_fmt("  %s", qs ? qs : "");
    free(qs);
    int password = st2_truthy(json_obj_get(var, "password"));
    json_t *a = json_object();
    json_set(a, "question", json_string(q ? q : ""));
    free(q);
    json_set(a, "password", json_bool(password ? true : false));
    json_t *r = st2_call(io, "prompt", a);
    if (st2_is_raise(r)) return r;
    if (st2_truthy(r)) {
        json_t *sa = json_object();
        json_set(sa, "name", json_copy(name));
        json_set(sa, "value", json_copy(r));
        json_free(r);
        json_t *sr = st2_call(io, "save_env_value", sa);
        if (st2_is_raise(sr)) return sr;
        json_free(sr);
        return st2_print_msg(io, "print_success", saved_msg);
    }
    json_free(r);
    return st2_print_msg(io, "print_warning", skipped_msg);
}

/* PoP: st2_record_setup_completed @ hermes_cli/setup.py:_record_setup_completed */
json_t *st2_record_setup_completed(st2_io_t *io, const json_t *config) {
    json_t *r = st2_call(io, "offer_consent_if_undecided", st2_config_args(config));
    if (st2_is_raise(r)) return r;
    json_free(r);
    if (!config || config->type != JSON_OBJECT)
        return st2_raise_fmt("AttributeError", "'%s' object has no attribute 'get'",
                             st2_type_name(config));
    const json_t *model = json_obj_get(config, "model");
    const json_t *prov = (model && model->type == JSON_OBJECT)
                         ? json_obj_get(model, "provider") : NULL;
    json_t *a = json_object();
    json_set(a, "surface", json_string("cli"));
    json_set(a, "provider", prov ? json_copy(prov) : json_null());
    r = st2_call(io, "record_setup_completed", a);
    if (st2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── navigation state ctors / reset / nested closures ── */

/* PoP: st2_nav_state_reset @ hermes_cli/setup.py:reset */
void st2_nav_state_reset(st2_nav_t *s, int section_index, const json_t *replay) {
    st2_nav_reset_core(s, section_index, replay);
}

/* PoP: st2_nav_state_init @ hermes_cli/setup.py:_SetupNavigationState.__init__ */
void st2_nav_state_init(st2_nav_t *s, int section_index, int prompt_index) {
    st2_nav_reset_core(s, section_index, NULL);
    s->prompt_index = prompt_index;
}

/* PoP: st2_go_back_init @ hermes_cli/setup.py:_SetupGoBack.__init__ */
json_t *st2_go_back_init(int prompt_index) {
    json_t *o = json_object();
    json_set(o, "prompt_index", json_number((double)prompt_index));
    json_t *args = json_array();
    json_append(args, json_number((double)prompt_index));
    json_set(o, "args", args);
    return o;
}

/* PoP: st2_record_answers @ hermes_cli/setup.py:_record_answers */
void st2_record_answers(const st2_nav_t *state, int section_index,
                        json_t *answers_by_section) {
    if (!state) return;
    char key[32];
    snprintf(key, sizeof key, "%d", section_index);
    json_set(answers_by_section, key, json_copy(state->resolved_choices));
}

/* PoP: st2_skip @ hermes_cli/setup.py:_skip */
json_t *st2_skip(st2_io_t *io, const json_t *config, const char *imported_from,
                 const char *key, const char *label) {
    /* bool(imported_from) and _skip_configured_section(config, key, label)
     * — Python `and` returns the OPERAND, not a bool: the raw seam result. */
    if (!imported_from || !imported_from[0]) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "config", config ? json_copy(config) : json_null());
    json_set(a, "key", json_string(key ? key : ""));
    json_set(a, "label", json_string(label ? label : ""));
    json_t *r = st2_call(io, "_skip_configured_section", a);
    if (st2_is_raise(r)) return r;
    return r;
}

/* PoP: st2_gateway_step @ hermes_cli/setup.py:_gateway_step */
json_t *st2_gateway_step(st2_io_t *io, json_t *config, const char *imported_from) {
    json_t *s = st2_skip(io, config, imported_from, "gateway", "Messaging Platforms");
    if (st2_is_raise(s)) return s;
    int skip = st2_truthy(s);
    json_free(s);
    if (!skip) {
        json_t *r = st2_call(io, "setup_gateway", st2_config_args(config));
        if (st2_is_raise(r)) return r;
        json_free(r);
        return NULL;
    }
    /* from hermes_cli.gateway_setup_service import ensure_gateway_service */
    json_t *a = json_object();
    json_set(a, "context", json_string("setup"));
    json_set(a, "interactive", json_bool(true));
    json_set(a, "config", config ? json_copy(config) : json_null());
    json_t *r = st2_call(io, "ensure_gateway_service", a);
    if (st2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* ── orchestrators ── */

/* PoP: st2_run_setup_section @ hermes_cli/setup.py:_run_setup_section */
json_t *st2_run_setup_section(st2_io_t *io, json_t *config, const json_t *section) {
    const char *secstr = (section && section->type == JSON_STRING) ? section->str_val : NULL;
    const char *label = NULL;
    const char *fn = NULL;
    if (secstr) {
        for (int i = 0; i < ST2_NSECTIONS; i++) {
            if (strcmp(ST2_SECTIONS[i].key, secstr) == 0) {
                label = ST2_SECTIONS[i].label;
                fn = ST2_SECTIONS[i].fn;
                break;
            }
        }
    }
    if (!label) {
        char *ss = st2_py_str(section);
        char *m = st2_fmt("Unknown setup section: %s", ss ? ss : "");
        free(ss);
        json_t *r = st2_print_msg(io, "print_error", m);
        free(m);
        if (st2_is_raise(r)) return r;
        r = st2_print_msg(io, "print_info", ST2_AVAILABLE);
        if (st2_is_raise(r)) return r;
        return NULL;
    }
    char *banner = st2_fmt(ST2_BAR "     " ST2_CADUCEUS " Hermes Setup " ST2_EMDASH " %-34s " ST2_BAR,
                           label);
    json_t *bl = json_array();
    json_append(bl, json_string(banner ? banner : ""));
    free(banner);
    json_t *r = st2_print_banner(io, bl);
    json_free(bl);
    if (st2_is_raise(r)) return r;
    json_t *steps = json_array();
    json_append(steps, st2_step_pair(label, fn));
    r = st2_call(io, "run_setup_steps", st2_steps_args(steps));
    json_free(steps);
    if (st2_is_raise(r)) return r;
    json_free(r);
    r = st2_call(io, "save_config", st2_config_args(config));
    if (st2_is_raise(r)) return r;
    json_free(r);
    r = st2_print(io, NULL);
    if (st2_is_raise(r)) return r;
    char *m = st2_fmt("%s configuration complete!", label);
    r = st2_print_msg(io, "print_success", m);
    free(m);
    if (st2_is_raise(r)) return r;
    return NULL;
}

/* PoP: st2_run_full_setup @ hermes_cli/setup.py:_run_full_setup */
json_t *st2_run_full_setup(st2_io_t *io, json_t *config, const char *hermes_home,
                           int is_existing, const char *imported_from) {
    json_t *r = st2_print_header(io, "Configuration Location", 0);
    if (st2_is_raise(r)) return r;
    json_t *cp = st2_call(io, "get_config_path", json_object());
    if (st2_is_raise(cp)) return cp;
    json_t *ep = st2_call(io, "get_env_path", json_object());
    if (st2_is_raise(ep)) { json_free(cp); return ep; }
    char *l1 = st2_fmt("Config file:  %s", st2_jstr(cp));
    char *l2 = st2_fmt("Secrets file: %s", st2_jstr(ep));
    char *l3 = st2_fmt("Data folder:  %s", hermes_home ? hermes_home : "");
    char *l4 = st2_fmt("Install dir:  %s", st2_env_str(io, "PROJECT_ROOT", ""));
    json_t *lines = json_array();
    json_append(lines, json_string(l1 ? l1 : ""));
    json_append(lines, json_string(l2 ? l2 : ""));
    json_append(lines, json_string(l3 ? l3 : ""));
    json_append(lines, json_string(l4 ? l4 : ""));
    json_append(lines, json_null());
    json_append(lines, json_string("You can edit these files directly or use 'hermes config edit'"));
    free(l1); free(l2); free(l3); free(l4);
    json_free(cp); json_free(ep);
    r = st2_info(io, lines);
    json_free(lines);
    if (st2_is_raise(r)) return r;
    if (imported_from && imported_from[0]) {
        char *im = st2_fmt("Settings were imported from %s.", imported_from);
        json_t *il = json_array();
        json_append(il, json_null());
        json_append(il, json_string(im ? im : ""));
        json_append(il, json_string("Each section below shows what is already set up " ST2_EMDASH " keep it,"));
        json_append(il, json_string("or choose to reconfigure it."));
        free(im);
        r = st2_info(io, il);
        json_free(il);
        if (st2_is_raise(r)) return r;
    }
    if (!is_existing) {
        r = st2_call(io, "_apply_default_agent_settings", st2_config_args(config));
        if (st2_is_raise(r)) return r;
        json_free(r);
    }
    /* _step("model"/"terminal"/"tools", ...) + ("Messaging Platforms", _gateway_step) */
    json_t *steps = json_array();
    json_append(steps, st2_step_pair("Model & Provider", "step:model"));
    json_append(steps, st2_step_pair("Terminal Backend", "step:terminal"));
    json_append(steps, st2_step_pair("Messaging Platforms", "gateway_step"));
    json_append(steps, st2_step_pair("Tools", "step:tools"));
    r = st2_call(io, "run_setup_steps", st2_steps_args(steps));
    json_free(steps);
    if (st2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: st2_run_setup_wizard_impl @ hermes_cli/setup.py:_run_setup_wizard_impl */
json_t *st2_run_setup_wizard_impl(st2_io_t *io, const json_t *args) {
    json_t *ret = NULL;
    json_t *config = NULL, *home = NULL, *cp = NULL, *backup = NULL;
    json_t *r;
    char *imported_owned = NULL;
    const char *imported_from = "";

    /* from hermes_cli.config import is_managed, managed_error */
    r = st2_call(io, "is_managed", json_object());
    if (st2_is_raise(r)) { ret = r; goto done; }
    int managed = st2_truthy(r);
    json_free(r);
    if (managed) {
        r = st2_call(io, "managed_error", st2_msg_args("run setup wizard"));
        if (st2_is_raise(r)) { ret = r; goto done; }
        json_free(r);
        goto done;
    }
    r = st2_call(io, "ensure_hermes_home", json_object());
    if (st2_is_raise(r)) { ret = r; goto done; }
    json_free(r);
    cp = st2_call(io, "get_config_path", json_object());
    if (st2_is_raise(cp)) { ret = cp; cp = NULL; goto done; }
    /* backup BEFORE --reset */
    {
        json_t *ba = json_object();
        json_set(ba, "config_path", cp ? json_copy(cp) : json_null());
        json_set(ba, "label", json_string("pre-setup"));
        backup = st2_call(io, "backup_config", ba);
        if (st2_is_raise(backup)) { ret = backup; backup = NULL; goto done; }
    }
    if (st2_truthy(json_obj_get(args, "reset"))) {
        const json_t *dc = st2_env(io, "DEFAULT_CONFIG");
        json_t *sa = json_object();
        json_set(sa, "config", dc ? json_copy(dc) : json_null());
        r = st2_call(io, "save_config", sa);
        if (st2_is_raise(r)) { ret = r; goto done; }
        json_free(r);
        r = st2_print_msg(io, "print_success", "Configuration reset to defaults.");
        if (st2_is_raise(r)) { ret = r; goto done; }
        if (st2_truthy(backup)) {
            char *m = st2_fmt("Previous config backed up to: %s",
                              json_get_str(backup, "path", ""));
            json_t *la = json_array();
            json_append(la, json_string(m ? m : ""));
            free(m);
            r = st2_info(io, la);
            json_free(la);
            if (st2_is_raise(r)) { ret = r; goto done; }
        }
    }
    int reconfigure_requested = st2_truthy(json_obj_get(args, "reconfigure"));
    int quick_requested = st2_truthy(json_obj_get(args, "quick"));
    config = st2_call(io, "load_config", json_object());
    if (st2_is_raise(config)) { ret = config; config = NULL; goto done; }
    home = st2_call(io, "get_hermes_home", json_object());
    if (st2_is_raise(home)) { ret = home; home = NULL; goto done; }

    /* Non-interactive environments: args flag short-circuits the TTY probe. */
    int noninteractive = st2_truthy(json_obj_get(args, "non_interactive"));
    if (!noninteractive) {
        r = st2_call(io, "is_interactive_stdin", json_object());
        if (st2_is_raise(r)) { ret = r; goto done; }
        noninteractive = !st2_truthy(r);
        json_free(r);
    }
    if (noninteractive) {
        json_t *ga = json_object();
        json_set(ga, "reason", json_string("Running in a non-interactive environment (no TTY detected)."));
        r = st2_call(io, "print_noninteractive_setup_guidance", ga);
        if (st2_is_raise(r)) { ret = r; goto done; }
        json_free(r);
        goto done;
    }
    if (st2_truthy(json_obj_get(args, "portal"))) {
        r = st2_call(io, "run_portal_one_shot", st2_config_args(config));
        if (st2_is_raise(r)) { ret = r; goto done; }
        json_free(r);
        goto done;
    }
    {
        const json_t *sec = json_obj_get(args, "section");
        if (st2_truthy(sec)) {
            r = st2_run_setup_section(io, config, sec);
            if (st2_is_raise(r)) { ret = r; goto done; }
            json_free(r);
            goto done;
        }
    }
    /* is_existing: env keys short-circuit, then the active-provider probe. */
    int is_existing = 0;
    r = st2_call(io, "get_env_value", st2_name_args("OPENROUTER_API_KEY"));
    if (st2_is_raise(r)) { ret = r; goto done; }
    if (st2_truthy(r)) is_existing = 1;
    json_free(r);
    if (!is_existing) {
        r = st2_call(io, "get_env_value", st2_name_args("OPENAI_BASE_URL"));
        if (st2_is_raise(r)) { ret = r; goto done; }
        if (st2_truthy(r)) is_existing = 1;
        json_free(r);
    }
    if (!is_existing) {
        r = st2_call(io, "get_active_provider", json_object());
        if (st2_is_raise(r)) { ret = r; goto done; }
        is_existing = st2_present(r);
        json_free(r);
    }
    {
        json_t *bl = json_array();
        for (size_t i = 0; i < sizeof ST2_WIZARD_BANNER / sizeof ST2_WIZARD_BANNER[0]; i++)
            json_append(bl, json_string(ST2_WIZARD_BANNER[i]));
        r = st2_print_banner(io, bl);
        json_free(bl);
        if (st2_is_raise(r)) { ret = r; goto done; }
    }
    if (is_existing) {
        if (quick_requested) {
            json_t *steps = json_array();
            json_append(steps, st2_step_pair("Quick Setup", "run_quick_setup"));
            r = st2_call(io, "run_setup_steps", st2_steps_args(steps));
            json_free(steps);
            if (st2_is_raise(r)) { ret = r; goto done; }
            json_free(r);
            r = st2_record_setup_completed(io, config);
            if (st2_is_raise(r)) { ret = r; goto done; }
            json_free(r);
            goto done;
        }
        r = st2_print_header(io, "Reconfigure", 1);
        if (st2_is_raise(r)) { ret = r; goto done; }
        r = st2_print_msg(io, "print_success", "You already have Hermes configured.");
        if (st2_is_raise(r)) { ret = r; goto done; }
        {
            json_t *il = json_array();
            for (size_t i = 0; i < sizeof ST2_RECONFIG_INFO / sizeof ST2_RECONFIG_INFO[0]; i++)
                json_append(il, json_string(ST2_RECONFIG_INFO[i]));
            r = st2_info(io, il);
            json_free(il);
            if (st2_is_raise(r)) { ret = r; goto done; }
        }
    } else {
        r = st2_print(io, NULL);
        if (st2_is_raise(r)) { ret = r; goto done; }
        if (reconfigure_requested || quick_requested) {
            json_t *il = json_array();
            json_append(il, json_string("No existing configuration found " ST2_EMDASH " running first-time setup."));
            json_append(il, json_null());
            r = st2_info(io, il);
            json_free(il);
            if (st2_is_raise(r)) { ret = r; goto done; }
        }
        {
            json_t *ha = json_object();
            json_set(ha, "hermes_home", home ? json_copy(home) : json_null());
            r = st2_call(io, "offer_agent_import", ha);
            if (st2_is_raise(r)) { ret = r; goto done; }
            if (r && r->type == JSON_STRING && r->str_val) {
                imported_owned = strdup(r->str_val);
                imported_from = imported_owned ? imported_owned : "";
            }
            json_free(r);
        }
        json_free(config);
        config = st2_call(io, "load_config", json_object());
        if (st2_is_raise(config)) { ret = config; config = NULL; goto done; }
        json_t *choices = json_array();
        for (int i = 0; i < ST2_NMODES; i++)
            json_append(choices, json_string(ST2_MODES[i].label));
        json_t *pa = json_object();
        json_set(pa, "question", json_string("How would you like to set up Hermes?"));
        json_set(pa, "choices", choices);
        json_set(pa, "default", json_number(0.0));
        r = st2_call(io, "prompt_choice", pa);
        if (st2_is_raise(r)) { ret = r; goto done; }
        double mode_d = (r && r->type == JSON_NUMBER) ? r->num_val : 0.0;
        json_free(r);
        long long mi = (long long)mode_d;
        if (mode_d != (double)mi) {
            ret = st2_raise("TypeError", "tuple indices must be integers or slices, not float");
            goto done;
        }
        if (mi < 0) mi += ST2_NMODES;
        if (mi < 0 || mi >= ST2_NMODES) {
            ret = st2_raise("IndexError", "tuple index out of range");
            goto done;
        }
        const char *label = ST2_MODES[mi].label;
        const char *runner = ST2_MODES[mi].runner;
        if (runner) {
            /* from hermes_cli import setup_quick; getattr(setup_quick, runner)(*run_args) */
            char *act = st2_fmt("first_time:%s", runner);
            json_t *steps = json_array();
            json_append(steps, st2_step_pair(label, act ? act : ""));
            free(act);
            r = st2_call(io, "run_setup_steps", st2_steps_args(steps));
            json_free(steps);
            if (st2_is_raise(r)) { ret = r; goto done; }
            json_free(r);
            r = st2_record_setup_completed(io, config);
            if (st2_is_raise(r)) { ret = r; goto done; }
            json_free(r);
            goto done;
        }
    }
    r = st2_run_full_setup(io, config, st2_jstr(home),
                           is_existing, imported_from);
    if (st2_is_raise(r)) { ret = r; goto done; }
    json_free(r);
    r = st2_call(io, "save_config", st2_config_args(config));
    if (st2_is_raise(r)) { ret = r; goto done; }
    json_free(r);
    if (st2_truthy(backup)) {
        r = st2_call(io, "backup_path.exists", json_object());
        if (st2_is_raise(r)) { ret = r; goto done; }
        int exists = st2_truthy(r);
        json_free(r);
        if (exists) {
            const char *bp = json_get_str(backup, "path", "");
            char *m1 = st2_fmt("Previous config backed up to: %s", bp);
            char *m3 = st2_fmt("  cp %s %s", bp, st2_jstr(cp));
            json_t *il = json_array();
            json_append(il, json_string(m1 ? m1 : ""));
            json_append(il, json_string("If setup changed a value you customized, restore it with:"));
            json_append(il, json_string(m3 ? m3 : ""));
            free(m1); free(m3);
            r = st2_info(io, il);
            json_free(il);
            if (st2_is_raise(r)) { ret = r; goto done; }
        }
    }
    {
        json_t *sa = json_object();
        json_set(sa, "config", config ? json_copy(config) : json_null());
        json_set(sa, "hermes_home", home ? json_copy(home) : json_null());
        r = st2_call(io, "_print_setup_summary", sa);
        if (st2_is_raise(r)) { ret = r; goto done; }
        json_free(r);
    }
    r = st2_record_setup_completed(io, config);
    if (st2_is_raise(r)) { ret = r; goto done; }
    json_free(r);
done:
    json_free(config);
    json_free(home);
    json_free(cp);
    json_free(backup);
    free(imported_owned);
    return ret;
}

/* PoP: st2_run_setup_wizard @ hermes_cli/setup.py:run_setup_wizard */
json_t *st2_run_setup_wizard(st2_io_t *io, const json_t *args) {
    /* with _setup_navigation_scope(): — claimed CM, seamed enter/exit. */
    json_t *enter = st2_call(io, "setup_navigation_scope.enter", json_object());
    if (st2_is_raise(enter)) return enter;   /* body + finally never run */
    json_free(enter);
    json_t *r = st2_run_setup_wizard_impl(io, args);
    if (st2_is_raise(r) && strcmp(st2_raise_kind(r), "_SetupCancelled") == 0) {
        json_free(r);
        json_t *lines = json_array();
        json_append(lines, json_null());
        json_append(lines, json_string("Setup cancelled. Remaining sections were not changed."));
        r = st2_info(io, lines);
        json_free(lines);
    }
    /* finally: the scope's __exit__ sees the in-flight exception (or None). */
    json_t *xa = json_object();
    json_set(xa, "exc", st2_is_raise(r) ? st2_exc_json(r) : json_null());
    json_t *x = st2_call(io, "setup_navigation_scope.exit", xa);
    if (st2_is_raise(x)) { json_free(r); return x; }
    json_free(x);
    return r;
}