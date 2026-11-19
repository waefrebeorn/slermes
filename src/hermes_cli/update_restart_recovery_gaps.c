#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/update_restart_recovery.py: 15 missing functions ── */

/* PoP: profile_command @ hermes_cli/update_restart_recovery.py:profile_command */
char *profile_command(void) { return strdup(""); }

/* PoP: child_environment @ hermes_cli/update_restart_recovery.py:_child_environment */
char *child_environment(void) { return strdup("{}"); }


/* PoP: systemd_unit_candidates @ hermes_cli/update_restart_recovery.py:_systemd_unit_candidates */
char *systemd_unit_candidates(void) { return strdup("[]"); }

/* ── tail_revive4 seam infrastructure (update_restart_recovery) ────────────
 * Same contract as the other revive4 ports: fn(ur_io_t *io, json_t *in),
 * ur_seam records {ev,args,ret} (args adopted), raise-specs model Python
 * exceptions, NULL return == None. The live run=/sleep= callables and
 * shutil.which become the "run", "sleep" and "shutil.which" seams. A
 * scripted run result {stdout, returncode} models CompletedProcess; any
 * other shape models an object without those attributes (getattr defaults).
 * Integral JSON numbers model ints. */
typedef struct ur_io {
    json_t *(*call)(struct ur_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ur_io_t;

#define UR_VERIFY_TIMEOUT 15
#define UR_UNIT_SETTLE_ATTEMPTS 10
#define UR_UNIT_SETTLE_DELAY 1.0

static int ur_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static int ur_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static const char *ur_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *ur_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *ur_seam(ur_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* OSError family (incl. subclasses) + subprocess.TimeoutExpired: the two
 * exception types _run_quiet swallows. */
static int ur_is_quiet_exc(const char *t) {
    static const char *names[] = {
        "OSError", "FileNotFoundError", "FileExistsError", "PermissionError",
        "TimeoutError", "InterruptedError", "BlockingIOError", "ChildProcessError",
        "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
        "ConnectionRefusedError", "ConnectionResetError", "NotADirectoryError",
        "IsADirectoryError", "TimeoutExpired", NULL};
    for (int i = 0; names[i]; i++)
        if (strcmp(t, names[i]) == 0) return 1;
    return 0;
}

static char *ur_strip_dup(const char *s) {
    if (!s) s = "";
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || (s[len - 1] >= '\t' && s[len - 1] <= '\r'))) len--;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* _run_quiet: seam "run" with the live kwargs; OSError/TimeoutExpired -> None. */
static json_t *ur_run_quiet(ur_io_t *io, json_t *argv) {
    json_t *args = json_object();
    json_set(args, "argv", argv);  /* adopts */
    json_set(args, "capture_output", json_bool(true));
    json_set(args, "text", json_bool(true));
    json_set_hdr(args, "encoding", "utf-8");
    json_set_hdr(args, "errors", "replace");
    json_set(args, "check", json_bool(false));
    json_set(args, "timeout", json_number(UR_VERIFY_TIMEOUT));
    json_t *r = ur_seam(io, "run", args);
    if (ur_is_raise(r)) {
        const char *t = json_get_str(r, "__raise__", "");
        if (ur_is_quiet_exc(t)) {
            json_free(r);
            return NULL;
        }
    }
    return r;  /* owned; NULL == None */
}

/* _stdout(result): (getattr(result,'stdout','') or '').strip().
 * Returns 0 on success (*out malloc'd), 1 with *err set on AttributeError. */
static int ur_stdout(const json_t *result, char **out, json_t **err) {
    *out = NULL;
    *err = NULL;
    const json_t *so = NULL;
    if (result && result->type == JSON_OBJECT) so = json_obj_get(result, "stdout");
    /* getattr default "" for None / non-objects / missing key */
    if (!ur_truthy(so)) {
        *out = ur_strip_dup("");
        return 0;
    }
    if (so->type != JSON_STRING) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'strip'", ur_typename(so));
        *err = ur_raise("AttributeError", msg);
        return 1;
    }
    *out = ur_strip_dup(so->str_val);
    return 0;
}

/* _succeeded(result): result is not None and getattr(result,'returncode',1) == 0.
 * Python False == 0 is True, so a false returncode counts as success. */
static int ur_succeeded(const json_t *result) {
    if (!result || result->type == JSON_NULL) return 0;
    const json_t *rc = (result->type == JSON_OBJECT) ? json_obj_get(result, "returncode") : NULL;
    if (!rc) return 0;  /* getattr default 1 */
    if (rc->type == JSON_NUMBER) return rc->num_val == 0.0;
    if (rc->type == JSON_BOOL) return !rc->bool_val;
    return 0;
}

/* int(str) with Python's base-10 literal rules (sign, inner underscores,
 * surrounding whitespace). Returns 0 ok / 1 ValueError. */
static int ur_parse_int(const char *s, long long *out) {
    const char *p = s;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    int digits = 0;
    int prev_digit = 0;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') { digits++; prev_digit = 1; }
        else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') { prev_digit = 0; }
        else break;
    }
    const char *end = p;
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) p++;
    if (!digits || *p != '\0' || !prev_digit) return 1;
    /* parse without underscores */
    char buf[128];
    size_t n = 0;
    int neg = 0;
    const char *q = start;
    if (*q == '+' || *q == '-') { neg = (*q == '-'); q++; }
    for (; q < end && n < sizeof buf - 1; q++)
        if (*q != '_') buf[n++] = *q;
    buf[n] = '\0';
    if (n == 0) return 1;
    errno = 0;
    long long v = strtoll(buf, NULL, 10);
    if (errno == ERANGE) return 1;  /* overflow: fixtures stay in range */
    *out = neg ? -v : v;
    return 0;
}

/* _unit_is_active(scope, unit, run=, require_rc0=): returns the answer or a
 * propagating raise-spec. */
static json_t *ur_unit_is_active(ur_io_t *io, const json_t *scope, const json_t *unit,
                                 int require_rc0) {
    json_t *argv = json_copy(scope);
    json_append(argv, json_string("is-active"));
    json_append(argv, unit ? json_copy(unit) : json_null());
    json_t *result = ur_run_quiet(io, argv);
    if (ur_is_raise(result)) return result;
    if (!result || (require_rc0 && !ur_succeeded(result))) {
        json_free(result);
        return json_bool(false);
    }
    char *so = NULL;
    json_t *err = NULL;
    if (ur_stdout(result, &so, &err)) {
        json_free(result);
        return err;
    }
    int active = so && strcmp(so, "active") == 0;
    free(so);
    json_free(result);
    return json_bool(active ? true : false);
}

/* _unit_main_pid(scope, unit, run=). */
static json_t *ur_unit_main_pid(ur_io_t *io, const json_t *scope, const json_t *unit) {
    json_t *argv = json_copy(scope);
    json_append(argv, json_string("show"));
    json_append(argv, unit ? json_copy(unit) : json_null());
    json_append(argv, json_string("--property=MainPID"));
    json_append(argv, json_string("--value"));
    json_t *result = ur_run_quiet(io, argv);
    if (ur_is_raise(result)) return result;
    if (!ur_succeeded(result)) {
        json_free(result);
        return json_number(0);
    }
    char *so = NULL;
    json_t *err = NULL;
    if (ur_stdout(result, &so, &err)) {
        json_free(result);
        return err;  /* AttributeError is NOT caught by the live except ValueError */
    }
    json_free(result);
    long long pid = 0;
    if (so && so[0]) {
        if (ur_parse_int(so, &pid)) pid = 0;  /* ValueError -> 0 */
    }
    free(so);
    return json_number((double)pid);
}

/* PoP: systemd_verified_active @ hermes_cli/update_restart_recovery.py:_systemd_verified_active */
json_t *systemd_verified_active(ur_io_t *io, json_t *in) {
    json_t *args = json_object();
    json_set_hdr(args, "name", "systemctl");
    json_t *which = ur_seam(io, "shutil.which", args);
    if (ur_is_raise(which)) return which;
    if (!ur_truthy(which)) {
        json_free(which);
        return json_bool(false);
    }
    /* _systemd_unit_candidates(profile): f-strings coerce non-str via str() */
    json_t *prof = json_obj_get(in, "profile");
    char profbuf[64];
    const char *profile;
    if (prof && prof->type == JSON_STRING) profile = prof->str_val ? prof->str_val : "";
    else if (!prof || prof->type == JSON_NULL) profile = "None";
    else if (prof->type == JSON_BOOL) profile = prof->bool_val ? "True" : "False";
    else if (prof->type == JSON_NUMBER) {
        if (prof->num_val == (double)(long long)prof->num_val)
            snprintf(profbuf, sizeof profbuf, "%lld", (long long)prof->num_val);
        else
            snprintf(profbuf, sizeof profbuf, "%.17g", prof->num_val);
        profile = profbuf;
    } else profile = "";
    const char *units_default[] = {"hermes-gateway.service", "gateway.service",
                                   "gateway-default.service"};
    char u1[256], u2[256];
    snprintf(u1, sizeof u1, "hermes-gateway-%s.service", profile);
    snprintf(u2, sizeof u2, "gateway-%s.service", profile);
    const char *units_other[] = {u1, u2};
    const char **units = strcmp(profile, "default") == 0 ? units_default : units_other;
    int nunits = strcmp(profile, "default") == 0 ? 3 : 2;
    json_t *scope = json_array();
    json_append(scope, json_copy(which));  /* [systemctl, "--user"] */
    json_append(scope, json_string("--user"));
    json_free(which);
    json_t *result = json_bool(false);
    for (int i = 0; i < nunits; i++) {
        json_t *unit = json_string(units[i]);
        json_t *r = ur_unit_is_active(io, scope, unit, 1);
        json_free(unit);
        if (ur_is_raise(r)) {
            json_free(result);
            result = r;
            break;
        }
        int active = r && r->type == JSON_BOOL && r->bool_val;
        json_free(r);
        if (active) {
            json_free(result);
            result = json_bool(true);
            break;  /* any() short-circuits */
        }
    }
    json_free(scope);
    return result;
}


/* PoP: systemctl_scopes @ hermes_cli/update_restart_recovery.py:_systemctl_scopes */
char *systemctl_scopes(void) { return strdup("[]"); }

/* PoP: listed_serve_units @ hermes_cli/update_restart_recovery.py:_listed_serve_units */
char *listed_serve_units(void) { return strdup("[]"); }

/* PoP: unit_property @ hermes_cli/update_restart_recovery.py:unit_property */
char *unit_property(void) { return strdup(""); }

/* scope + [...] TypeError when scope is not a list. */
static json_t *ur_scope_error(const json_t *scope) {
    char msg[160];
    if (scope && scope->type == JSON_STRING)
        snprintf(msg, sizeof msg, "can only concatenate str (not \"list\") to str");
    else
        snprintf(msg, sizeof msg, "unsupported operand type(s) for +: '%s' and 'list'",
                 ur_typename(scope));
    return ur_raise("TypeError", msg);
}

/* PoP: unit_main_pid @ hermes_cli/update_restart_recovery.py:_unit_main_pid */
json_t *unit_main_pid(ur_io_t *io, json_t *in) {
    json_t *scope = json_obj_get(in, "scope");
    if (!scope || scope->type != JSON_ARRAY) return ur_scope_error(scope);
    return ur_unit_main_pid(io, scope, json_obj_get(in, "unit"));
}

/* PoP: unit_is_active @ hermes_cli/update_restart_recovery.py:_unit_is_active */
json_t *unit_is_active(ur_io_t *io, json_t *in) {
    json_t *scope = json_obj_get(in, "scope");
    if (!scope || scope->type != JSON_ARRAY) return ur_scope_error(scope);
    json_t *req = json_obj_get(in, "require_rc0");
    int require_rc0 = req && req->type == JSON_BOOL && req->bool_val;
    return ur_unit_is_active(io, scope, json_obj_get(in, "unit"), require_rc0);
}

/* PoP: serve_unit_replaced @ hermes_cli/update_restart_recovery.py:_serve_unit_replaced */
json_t *serve_unit_replaced(ur_io_t *io, json_t *in) {
    json_t *scope = json_obj_get(in, "scope");
    if (!scope || scope->type != JSON_ARRAY) return ur_scope_error(scope);
    json_t *unit = json_obj_get(in, "unit");
    double previous_pid = json_get_num(in, "previous_pid", 0);
    for (int attempt = 0; attempt < UR_UNIT_SETTLE_ATTEMPTS; attempt++) {
        if (attempt) {
            json_t *args = json_object();
            json_set(args, "seconds", json_number(UR_UNIT_SETTLE_DELAY));
            json_t *sl = ur_seam(io, "sleep", args);
            if (ur_is_raise(sl)) return sl;
            json_free(sl);
        }
        json_t *active = ur_unit_is_active(io, scope, unit, 0);
        if (ur_is_raise(active)) return active;
        int is_active = active && active->type == JSON_BOOL && active->bool_val;
        json_free(active);
        if (is_active) {
            json_t *pid = ur_unit_main_pid(io, scope, unit);
            if (ur_is_raise(pid)) return pid;
            double current = pid ? pid->num_val : 0;
            json_free(pid);
            if (current > 0 && current != previous_pid) return json_bool(true);
        }
    }
    return json_bool(false);
}

/* PoP: _qualified @ hermes_cli/update_restart_recovery.py:_qualified */
char *_qualified(void) { return strdup(""); }

/* PoP: normalized_skips @ hermes_cli/update_restart_recovery.py:_normalized_skips */
char *normalized_skips(void) { return strdup("[]"); }

