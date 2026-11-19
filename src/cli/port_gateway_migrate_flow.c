/*
 * port_gateway_migrate_flow.c — hermes_cli/gateway_migrate.py migration FLOW
 * cores and leaves. Decision sequences, step ordering, version/resume gates,
 * profile/service migration decision chains, backup/rollback branches and
 * error paths; all file/service/clock I/O via the universal seam
 * (gmf_seams_t.call). See the header for the seam table and plan model.
 * No I/O is performed here.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "port_gateway_migrate_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define GMF_MANIFEST_NAME "gateway_migration.json"
#define GMF_MIGRATE_COMMAND "hermes gateway migrate --multiplex"
#define GMF_NOTHING_RECORDED "no gateway was recorded; nothing to restore"
#define GMF_COMPENSATOR_WAIT 30.0

/* ── JSON / Python-value helpers ── */

static bool jtruth(const json_t *v) {
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
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static const char *jstr(const json_t *v) {
    return (is_str(v) && v->str_val) ? v->str_val : NULL;
}
static double jnum(const json_t *v) { return is_num(v) ? v->num_val : 0.0; }

static char *xsf(const char *fmt, ...) {
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

static const char *py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

static bool py_eq(const json_t *a, const json_t *b) {
    bool ap = jpresent(a), bp = jpresent(b);
    if (!ap || !bp) return ap == bp;
    if (is_bool(a) && is_bool(b)) return a->bool_val == b->bool_val;
    if ((is_bool(a) || is_num(a)) && (is_bool(b) || is_num(b))) {
        double av = is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (is_str(a) && is_str(b)) return strcmp(a->str_val, b->str_val) == 0;
    if (is_arr(a) && is_arr(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (is_obj(a) && is_obj(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

static char *py_float_str(double d) {
    if (d == (double)(long long)d) return xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s%s", out, i ? ", " : "", item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s]", out); free(out); return next;
        }
        case JSON_OBJECT: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = py_str(v->c.items[i]);
                char *next = xsf("%s%s'%s': %s", out, i ? ", " : "", v->c.keys[i], item);
                free(out); free(item); out = next;
            }
            char *next = xsf("%s}", out); free(out); return next;
        }
    }
    return strdup("");
}

/* ── path helpers (Path.as_posix semantics) ── */

static char *path_norm(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    if (!out) return strdup("");
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char ch = (s[i] == '\\') ? '/' : s[i];
        if (ch == '/' && k > 0 && out[k - 1] == '/') continue;
        out[k++] = ch;
    }
    out[k] = '\0';
    while (k > 1 && out[k - 1] == '/') out[--k] = '\0';
    return out;
}
static char *path_join(const char *base, const char *name) {
    char *b = path_norm(base);
    size_t bl = strlen(b);
    char *out = xsf("%s%s%s", b, (bl > 0 && b[bl - 1] == '/') ? "" : "/", name);
    free(b);
    return out;
}
/* PoP: gmf_manifest_path @ hermes_cli/gateway_migrate.py:_manifest_path */
static char *gmf_manifest_path(const char *home) {
    return path_join(home, GMF_MANIFEST_NAME);
}

/* ── trace / seam / raise plumbing ── */

void gmf_trace_add(gmf_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
void gmf_trace_free(gmf_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
json_t *gmf_call(const gmf_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
const char *gmf_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
bool gmf_is_raise(const json_t *r) { return gmf_raise_class(r) != NULL; }

/* Seam call that CONSUMES (frees) the freshly built args object. */
static json_t *scall(const gmf_seams_t *s, const char *name, json_t *args) {
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static json_t *raise_attr(const json_t *v, const char *attr) {
    char *m = xsf("'%s' object has no attribute '%s'", py_type_name(v), attr);
    json_t *r = raise_obj("AttributeError", m);
    free(m);
    return r;
}
static json_t *raise_stopiteration(void) { return raise_obj("StopIteration", ""); }

#define GMF_PROP(r) do { if (gmf_is_raise(r)) return (r); } while (0)

static bool kind_base_only(const char *k) {
    return !strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt")
        || !strcmp(k, "GeneratorExit") || !strcmp(k, "BaseException");
}
static bool kind_oserror(const char *k) {
    static const char *fam[] = {
        "OSError", "BlockingIOError", "ChildProcessError", "ConnectionError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "FileExistsError", "FileNotFoundError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "PermissionError", "ProcessLookupError", "TimeoutError",
        "UnsupportedOperation",
    };
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (!strcmp(k, fam[i])) return true;
    return false;
}
static bool kind_valueerror(const char *k) {
    return !strcmp(k, "ValueError") || !strcmp(k, "JSONDecodeError")
        || !strcmp(k, "UnicodeError") || !strcmp(k, "UnicodeDecodeError")
        || !strcmp(k, "UnicodeEncodeError") || !strcmp(k, "UnicodeTranslateError");
}
static bool kind_runtimeerror(const char *k) { return !strcmp(k, "RuntimeError"); }
static bool kind_keyerror(const char *k) { return !strcmp(k, "KeyError"); }

/* contextlib.suppress(Exception): swallow Exception raises, keep BaseException. */
static json_t *suppress_exception(json_t *r) {
    if (gmf_is_raise(r) && !kind_base_only(gmf_raise_class(r))) {
        json_free(r);
        return NULL;
    }
    return r;
}

/* ── args builders (null-valued keys dropped on BOTH sides) ── */

static json_t *args0(void) { return json_object(); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    if (jpresent(v)) json_set(a, k, json_copy(v));
}
static void aset_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── string-array helpers ── */

static char *join_strs(const json_t *arr, const char *sep) {
    char *out = strdup("");
    if (is_arr(arr)) {
        for (size_t i = 0; i < arr->c.count; i++) {
            const char *e = jstr(arr->c.items[i]);
            if (!e) e = "";
            char *next = xsf("%s%s%s", out, i ? sep : "", e);
            free(out); out = next;
        }
    }
    return out;
}
static bool in_arr(const json_t *arr, const json_t *v) {
    if (!is_arr(arr)) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (py_eq(arr->c.items[i], v)) return true;
    return false;
}
static bool subset(const json_t *expected, const json_t *served) {
    if (!is_arr(expected)) return true;
    for (size_t i = 0; i < expected->c.count; i++)
        if (!in_arr(served, expected->c.items[i])) return false;
    return true;
}
static void dedup_add(json_t *arr, const char *name) {
    for (size_t i = 0; i < arr->c.count; i++) {
        const char *e = jstr(arr->c.items[i]);
        if (e && !strcmp(e, name)) return;
    }
    json_append(arr, json_string(name));
}
static int cmp_cstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *sorted_diff(const json_t *expected, const json_t *served) {
    char **tmp = NULL;
    size_t n = 0;
    if (is_arr(expected)) {
        for (size_t i = 0; i < expected->c.count; i++) {
            const json_t *e = expected->c.items[i];
            if (!in_arr(served, e)) {
                const char *es = jstr(e);
                tmp = realloc(tmp, (n + 1) * sizeof(char *));
                tmp[n++] = strdup(es ? es : "");
            }
        }
    }
    if (n > 1) qsort(tmp, n, sizeof(char *), cmp_cstr);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) { json_append(out, json_string(tmp[i])); free(tmp[i]); }
    free(tmp);
    return out;
}

/* ── service leaves ── */

/* PoP: gmf_service_dict @ hermes_cli/gateway_migrate.py:_service_dict */
static json_t *gmf_service_dict(const json_t *svc) {
    json_t *o = json_object();
    json_set(o, "kind", json_copy(svc->c.items[0]));
    json_set(o, "system", json_copy(svc->c.items[1]));
    return o;
}

json_t *gmf_flow_service_label(const json_t *service) {
    if (!is_arr(service)) {
        char *m = xsf("cannot unpack non-iterable %s object", py_type_name(service));
        json_t *r = raise_obj("TypeError", m);
        free(m);
        return r;
    }
    if (service->c.count < 2) {
        char *m = xsf("not enough values to unpack (expected 2, got %zu)", service->c.count);
        json_t *r = raise_obj("ValueError", m);
        free(m);
        return r;
    }
    if (service->c.count > 2) {
        char *m = xsf("too many values to unpack (expected 2, got %zu)", service->c.count);
        json_t *r = raise_obj("ValueError", m);
        free(m);
        return r;
    }
    const json_t *kind = service->c.items[0];
    const json_t *system = service->c.items[1];
    const char *k = jstr(kind);
    if (k && !strcmp(k, "systemd"))
        return json_string(jtruth(system) ? "systemd (system)" : "systemd (user)");
    if (k && !strcmp(k, "s6"))
        return json_string("s6 slot");
    if (k && !strcmp(k, "windows"))
        return json_string("Windows scheduled task");
    return json_copy(kind);
}

json_t *gmf_flow_remove_verb(const json_t *service) {
    if (!is_arr(service)) {
        char *m = xsf("'%s' object is not subscriptable", py_type_name(service));
        json_t *r = raise_obj("TypeError", m);
        free(m);
        return r;
    }
    if (service->c.count == 0)
        return raise_obj("IndexError", "list index out of range");
    const char *k = jstr(service->c.items[0]);
    return json_string((k && !strcmp(k, "s6")) ? "park" : "uninstall");
}

json_t *gmf_flow_service_from_dict(const json_t *service) {
    if (!is_obj(service)) return NULL;
    const json_t *kind = json_obj_get(service, "kind");
    if (!jtruth(kind)) return NULL;
    char *ks = py_str(kind);
    json_t *out = json_array();
    json_append(out, json_string(ks));
    json_append(out, json_bool(jtruth(json_obj_get(service, "system"))));
    free(ks);
    return out;
}

json_t *gmf_flow_recorded_services(const json_t *rec) {
    if (!is_obj(rec)) return raise_attr(rec, "get");
    const json_t *recorded = json_obj_get(rec, "services");
    json_t *out = json_array();
    if (is_arr(recorded)) {
        for (size_t i = 0; i < recorded->c.count; i++) {
            json_t *s = gmf_flow_service_from_dict(recorded->c.items[i]);
            if (s) json_append(out, s);
        }
        return out;
    }
    json_t *s = gmf_flow_service_from_dict(json_obj_get(rec, "service"));
    if (s) json_append(out, s);
    return out;
}

/* PoP: gmf_recorded_run_as_user @ hermes_cli/gateway_migrate.py:_recorded_run_as_user */
static json_t *gmf_recorded_run_as_user(const json_t *rec) {
    const json_t *u = json_obj_get(rec, "run_as_user");
    if (is_str(u) && u->str_val && u->str_val[0]) return json_copy(u);
    return NULL;
}

json_t *gmf_flow_manifest_secondaries(const json_t *manifest) {
    if (!is_obj(manifest)) return raise_attr(manifest, "get");
    const json_t *recs = json_obj_get(manifest, "secondaries");
    if (!recs) return json_array();           /* .get("secondaries", []) */
    if (!is_arr(recs)) return NULL;
    for (size_t i = 0; i < recs->c.count; i++) {
        const json_t *r = recs->c.items[i];
        if (!is_obj(r) || !jtruth(json_obj_get(r, "profile"))
            || !jtruth(json_obj_get(r, "home")))
            return NULL;
    }
    return json_copy(recs);
}

/* Build [default_rec, *secondaries] filtered to dict records (new array). */
static json_t *manifest_recs(const json_t *manifest) {
    json_t *recs = json_array();
    const json_t *d = json_obj_get(manifest, "default");
    if (is_obj(d)) json_append(recs, json_copy(d));
    json_t *secs = gmf_flow_manifest_secondaries(manifest);
    if (gmf_is_raise(secs)) { json_free(recs); return secs; }
    if (secs) {
        for (size_t i = 0; i < secs->c.count; i++)
            if (is_obj(secs->c.items[i])) json_append(recs, json_copy(secs->c.items[i]));
        json_free(secs);
    }
    return recs;
}

json_t *gmf_flow_target_from_manifest(const json_t *manifest) {
    json_t *recs = manifest_recs(manifest);
    GMF_PROP(recs);
    json_t *target = NULL, *user = NULL;
    for (size_t i = 0; i < recs->c.count && !target; i++) {
        json_t *svcs = gmf_flow_recorded_services(recs->c.items[i]);
        if (svcs->c.count > 0) target = json_copy(svcs->c.items[0]);
        json_free(svcs);
    }
    for (size_t i = 0; i < recs->c.count && !user; i++)
        user = gmf_recorded_run_as_user(recs->c.items[i]);
    json_free(recs);
    json_t *out = json_array();
    json_append(out, target ? target : json_null());
    json_append(out, user ? user : json_null());
    return out;
}

/* ── ProfileGateway / MigrationPlan property helpers ── */

static bool pg_is_default(const json_t *p) {
    const char *n = jstr(json_obj_get(p, "name"));
    return n && !strcmp(n, "default");
}
/* PoP: gmf_pg_service @ hermes_cli/gateway_migrate.py:service */
static json_t *pg_service(const json_t *p) {
    const json_t *svcs = json_obj_get(p, "services");
    if (is_arr(svcs) && svcs->c.count > 0) return json_copy(svcs->c.items[0]);
    return NULL;
}
/* PoP: gmf_pg_has_gateway @ hermes_cli/gateway_migrate.py:has_gateway */
static bool pg_has_gateway(const json_t *p) {
    const json_t *svcs = json_obj_get(p, "services");
    return jpresent(json_obj_get(p, "pid")) || (is_arr(svcs) && svcs->c.count > 0);
}
static bool system_true(const json_t *v) {
    return (is_bool(v) && v->bool_val) || (is_num(v) && v->num_val == 1.0);
}
/* PoP: gmf_pg_has_system_unit @ hermes_cli/gateway_migrate.py:has_system_unit */
static bool pg_has_system_unit(const json_t *p) {
    const json_t *svcs = json_obj_get(p, "services");
    if (!is_arr(svcs)) return false;
    for (size_t i = 0; i < svcs->c.count; i++) {
        const json_t *s = svcs->c.items[i];
        if (!is_arr(s) || s->c.count < 2) continue;
        const char *k = jstr(s->c.items[0]);
        if (k && !strcmp(k, "systemd") && system_true(s->c.items[1])) return true;
    }
    return false;
}
/* PoP: gmf_pg_service_label @ hermes_cli/gateway_migrate.py:service_label */
static char *pg_service_label(const json_t *p) {
    const json_t *svcs = json_obj_get(p, "services");
    if (!is_arr(svcs) || svcs->c.count == 0) return strdup("none");
    char *out = strdup("");
    for (size_t i = 0; i < svcs->c.count; i++) {
        json_t *lab = gmf_flow_service_label(svcs->c.items[i]);
        const char *ls = jstr(lab);
        char *next = xsf("%s%s%s", out, i ? " + " : "", ls ? ls : "");
        free(out); out = next;
        json_free(lab);
    }
    return out;
}
/* PoP: gmf_pg_to_dict @ hermes_cli/gateway_migrate.py:to_dict */
static json_t *pg_to_dict(const json_t *p) {
    json_t *o = json_object();
    json_set(o, "profile", json_copy(json_obj_get(p, "name")));
    char *h = path_norm(jstr(json_obj_get(p, "home")));
    json_set(o, "home", json_string(h));
    free(h);
    const json_t *pid = json_obj_get(p, "pid");
    json_set(o, "pid", jpresent(pid) ? json_copy(pid) : json_null());
    json_t *svc = pg_service(p);
    json_set(o, "service", svc ? gmf_service_dict(svc) : json_null());
    json_free(svc);
    json_t *svcs = json_array();
    const json_t *in = json_obj_get(p, "services");
    if (is_arr(in))
        for (size_t i = 0; i < in->c.count; i++)
            json_append(svcs, gmf_service_dict(in->c.items[i]));
    json_set(o, "services", svcs);
    const json_t *rau = json_obj_get(p, "run_as_user");
    json_set(o, "run_as_user", jpresent(rau) ? json_copy(rau) : json_null());
    const json_t *uid = json_obj_get(p, "uid");
    json_set(o, "uid", jpresent(uid) ? json_copy(uid) : json_null());
    const json_t *rh = json_obj_get(p, "runtime_home");
    if (jpresent(rh)) {
        char *r = path_norm(jstr(rh));
        json_set(o, "runtime_home", json_string(r));
        free(r);
    } else {
        json_set(o, "runtime_home", json_null());
    }
    return o;
}

static const json_t *plan_default(const json_t *plan) {
    const json_t *ps = json_obj_get(plan, "profiles");
    if (is_arr(ps))
        for (size_t i = 0; i < ps->c.count; i++)
            if (pg_is_default(ps->c.items[i])) return ps->c.items[i];
    return NULL;
}
/* PoP: gmf_plan_secondaries @ hermes_cli/gateway_migrate.py:secondaries */
static json_t *plan_secondaries(const json_t *plan) {
    json_t *out = json_array();
    const json_t *ps = json_obj_get(plan, "profiles");
    if (is_arr(ps))
        for (size_t i = 0; i < ps->c.count; i++)
            if (!pg_is_default(ps->c.items[i])) json_append(out, json_copy(ps->c.items[i]));
    return out;
}
/* PoP: gmf_plan_standalone_secondaries @ hermes_cli/gateway_migrate.py:standalone_secondaries */
static json_t *plan_standalone_secondaries(const json_t *plan) {
    json_t *sec = plan_secondaries(plan);
    json_t *out = json_array();
    for (size_t i = 0; i < sec->c.count; i++)
        if (pg_has_gateway(sec->c.items[i])) json_append(out, json_copy(sec->c.items[i]));
    json_free(sec);
    return out;
}
/* PoP: gmf_plan_blocked @ hermes_cli/gateway_migrate.py:blocked */
static bool plan_blocked(const json_t *plan) {
    const json_t *b = json_obj_get(plan, "blockers");
    return is_arr(b) && b->c.count > 0;
}
/* PoP: gmf_plan_already_multiplexed @ hermes_cli/gateway_migrate.py:already_multiplexed */
static bool plan_already_multiplexed(const json_t *plan) {
    if (jtruth(json_obj_get(plan, "interrupted"))) return false;
    json_t *ss = plan_standalone_secondaries(plan);
    bool any = ss->c.count > 0;
    json_free(ss);
    if (any) return false;
    const json_t *ls = json_obj_get(plan, "live_served");
    bool multi = is_arr(ls) && ls->c.count > 1;
    return jtruth(json_obj_get(plan, "multiplex_flag_on")) || multi;
}
/* PoP: gmf_plan_target_service_kind @ hermes_cli/gateway_migrate.py:target_service_kind */
static json_t *plan_target_service_kind(const json_t *plan) {
    const json_t *d = plan_default(plan);
    if (!d) return raise_stopiteration();
    json_t *svc = pg_service(d);
    if (svc) return svc;
    json_t *sec = plan_secondaries(plan);
    for (size_t i = 0; i < sec->c.count; i++) {
        svc = pg_service(sec->c.items[i]);
        if (svc) { json_free(sec); return svc; }
    }
    json_free(sec);
    return NULL;
}
/* PoP: gmf_plan_target_run_as_user @ hermes_cli/gateway_migrate.py:target_run_as_user */
static json_t *plan_target_run_as_user(const json_t *plan) {
    const json_t *d = plan_default(plan);
    if (!d) return raise_stopiteration();
    const json_t *u = json_obj_get(d, "run_as_user");
    if (jtruth(u)) return json_copy(u);
    json_t *sec = plan_secondaries(plan);
    for (size_t i = 0; i < sec->c.count; i++) {
        const json_t *su = json_obj_get(sec->c.items[i], "run_as_user");
        if (jtruth(su)) { json_t *c = json_copy(su); json_free(sec); return c; }
    }
    json_free(sec);
    return NULL;
}
/* PoP: gmf_plan_expected_served @ hermes_cli/gateway_migrate.py:expected_served_names */
static json_t *plan_expected_served(const json_t *plan, const gmf_seams_t *s) {
    json_t *out = json_array();
    const json_t *ps = json_obj_get(plan, "profiles");
    if (!is_arr(ps)) return out;
    for (size_t i = 0; i < ps->c.count; i++) {
        const json_t *p = ps->c.items[i];
        const char *name = jstr(json_obj_get(p, "name"));
        if (pg_is_default(p)) { dedup_add(out, name ? name : ""); continue; }
        char *h = path_norm(jstr(json_obj_get(p, "home")));
        json_t *a = args0(); aset_s(a, "home", h); free(h);
        json_t *r = scall(s, "profile_is_parked", a);
        GMF_PROP(r);
        if (!jtruth(r)) dedup_add(out, name ? name : "");
        json_free(r);
    }
    return out;
}

/* ── internal I/O wrappers (real on both sides) ── */

/* PoP: gmf_live_gateway_pid @ hermes_cli/gateway_migrate.py:_live_gateway_pid */
static json_t *gmf_live_gateway_pid(const char *home, const gmf_seams_t *s) {
    char *h = path_norm(home);
    json_t *a = args0(); aset_s(a, "home", h); free(h);
    return suppress_exception(scall(s, "live_gateway_pid_for_home", a));
}
/* PoP: gmf_host_gateway_owner @ hermes_cli/gateway_migrate.py:_host_gateway_owner */
static json_t *gmf_host_gateway_owner(const gmf_seams_t *s) {
    return suppress_exception(scall(s, "host_gateway", args0()));
}
/* PoP: gmf_default_home @ hermes_cli/gateway_migrate.py:_default_home */
static json_t *gmf_default_home(const gmf_seams_t *s) {
    return scall(s, "get_default_hermes_root", args0());
}
/* PoP: gmf_print_line @ hermes_cli/gateway_migrate.py:_print */
static json_t *gmf_print_line(const gmf_seams_t *s, const char *line) {
    json_t *a = args0();
    aset_s(a, "line", line);
    return scall(s, "print", a);
}
static json_t *gmf_print_lines(const gmf_seams_t *s, const json_t *lines) {
    if (is_arr(lines)) {
        for (size_t i = 0; i < lines->c.count; i++) {
            json_t *r = gmf_print_line(s, jstr(lines->c.items[i]));
            GMF_PROP(r);
            json_free(r);
        }
    }
    return NULL;
}
/* PoP: gmf_write_manifest @ hermes_cli/gateway_migrate.py:_write_manifest */
static json_t *gmf_write_manifest(const char *home, const json_t *data, const gmf_seams_t *s) {
    char *p = gmf_manifest_path(home);
    json_t *a = args0();
    aset_s(a, "path", p); free(p);
    aset_j(a, "data", data);
    return scall(s, "atomic_json_write", a);
}

/* ── ownership / manifest reading ── */

json_t *gmf_flow_own_gateway_pid(const char *home, const json_t *owner, const gmf_seams_t *s) {
    json_t *pid = gmf_live_gateway_pid(home, s);
    GMF_PROP(pid);
    if (!jpresent(pid)) return pid;
    if (!jpresent(owner)) return pid;
    if (!py_eq(pid, json_obj_get(owner, "pid"))) return pid;
    const json_t *oh = json_obj_get(owner, "home");
    if (!jpresent(oh) || !is_str(oh)) { json_free(pid); return NULL; }
    char *l = path_norm(oh->str_val);
    char *r = path_norm(home);
    json_t *a = args0(); aset_s(a, "left", l); aset_s(a, "right", r);
    free(l); free(r);
    json_t *res = scall(s, "same_hermes_home", a);
    if (gmf_is_raise(res)) {
        if (kind_base_only(gmf_raise_class(res))) { json_free(pid); return res; }
        json_free(res); json_free(pid); return NULL;
    }
    bool same = jtruth(res);
    json_free(res);
    if (same) return pid;
    json_free(pid);
    return NULL;
}

json_t *gmf_flow_read_manifest(const char *default_home, const gmf_seams_t *s) {
    char *path = gmf_manifest_path(default_home);
    json_t *a = args0(); aset_s(a, "path", path);
    json_t *r = scall(s, "path_exists", a);
    GMF_PROP(r);
    if (!jtruth(r)) { json_free(r); free(path); return NULL; }
    json_free(r);
    a = args0(); aset_s(a, "path", path); aset_s(a, "encoding", "utf-8-sig");
    r = scall(s, "read_text", a);
    free(path);
    if (gmf_is_raise(r)) {
        const char *k = gmf_raise_class(r);
        if (kind_oserror(k) || kind_valueerror(k)) { json_free(r); return NULL; }
        return r;
    }
    if (!is_str(r)) {
        char *m = xsf("the JSON object must be str, bytes or bytearray, not %s", py_type_name(r));
        json_free(r);
        json_t *e = raise_obj("TypeError", m);
        free(m);
        return e;
    }
    char *err = NULL;
    json_t *data = json_parse(r->str_val, &err);
    free(err);
    json_free(r);
    if (!data) return NULL;
    if (is_obj(data)) return data;
    json_free(data);
    return NULL;
}

json_t *gmf_flow_manifest_not_yet_served(const json_t *manifest, const json_t *live_served,
                                         const gmf_seams_t *s) {
    if (!jpresent(manifest)) return json_bool(false);
    json_t *recs = manifest_recs(manifest);
    GMF_PROP(recs);
    json_t *migrated = json_array();
    for (size_t i = 0; i < recs->c.count; i++) {
        const json_t *r = recs->c.items[i];
        const json_t *home = json_obj_get(r, "home");
        bool include;
        if (!jtruth(home)) {
            include = true;
        } else if (!is_str(home)) {
            json_free(recs); json_free(migrated);
            char *m = xsf("expected str, bytes or os.PathLike object, not %s", py_type_name(home));
            json_t *e = raise_obj("TypeError", m);
            free(m);
            return e;
        } else {
            char *h = path_norm(home->str_val);
            json_t *a = args0(); aset_s(a, "home", h); free(h);
            json_t *rk = scall(s, "profile_is_parked", a);
            if (gmf_is_raise(rk)) { json_free(recs); json_free(migrated); return rk; }
            include = !jtruth(rk);
            json_free(rk);
        }
        if (include) {
            const json_t *p = json_obj_get(r, "profile");
            char *name = jtruth(p) ? py_str(p) : strdup("default");
            dedup_add(migrated, name);
            free(name);
        }
    }
    dedup_add(migrated, "default");
    bool sub = subset(migrated, live_served);
    json_free(recs);
    json_free(migrated);
    return json_bool(!sub);
}

/* ── preflight / bring-up / removal ── */

static bool target_is_system_true(const json_t *target) {
    if (!is_arr(target) || target->c.count != 2) return false;
    const char *k = jstr(target->c.items[0]);
    return k && !strcmp(k, "systemd") && system_true(target->c.items[1]);
}

json_t *gmf_flow_preflight_apply(const json_t *plan, const json_t *target,
                                 const char *run_as_user, const gmf_seams_t *s) {
    const char *home = jstr(json_obj_get(plan, "default_home"));
    char *cfg = path_join(home ? home : "", "config.yaml");
    json_t *a = args0(); aset_s(a, "path", cfg); free(cfg);
    json_t *r = scall(s, "require_readable_config", a);
    if (gmf_is_raise(r)) {
        if (kind_base_only(gmf_raise_class(r))) return r;
        char *m = xsf("default: config.yaml cannot be updated (%s)", json_get_str(r, "msg", ""));
        json_free(r);
        json_t *out = json_string(m);
        free(m);
        return out;
    }
    json_free(r);
    bool touches = target_is_system_true(target);
    if (!touches) {
        json_t *ss = plan_standalone_secondaries(plan);
        for (size_t i = 0; i < ss->c.count; i++)
            if (pg_has_system_unit(ss->c.items[i])) { touches = true; break; }
        json_free(ss);
    }
    if (touches) {
        a = args0(); aset_s(a, "context", "migration");
        r = scall(s, "require_root", a);
        if (gmf_is_raise(r)) {
            if (kind_base_only(gmf_raise_class(r))) return r;
            json_t *out = json_string(json_get_str(r, "msg", ""));
            json_free(r);
            return out;
        }
        json_free(r);
    }
    const json_t *d = plan_default(plan);
    if (!d) return raise_stopiteration();
    json_t *svc = pg_service(d);
    bool has_svc = svc != NULL;
    json_free(svc);
    if (!has_svc && target_is_system_true(target)) {
        if (!run_as_user) {
            r = scall(s, "system_service_identity", args0());
            if (gmf_is_raise(r)) {
                if (kind_valueerror(gmf_raise_class(r))) {
                    char *m = xsf("default: %s", json_get_str(r, "msg", ""));
                    json_free(r);
                    json_t *out = json_string(m);
                    free(m);
                    return out;
                }
                return r;
            }
            json_free(r);
        } else {
            a = args0(); aset_s(a, "name", run_as_user);
            r = scall(s, "getpwnam", a);
            if (gmf_is_raise(r)) {
                if (kind_keyerror(gmf_raise_class(r))) {
                    char *m = xsf("default: the recorded service user '%s' does not exist on this host",
                                  run_as_user);
                    json_free(r);
                    json_t *out = json_string(m);
                    free(m);
                    return out;
                }
                return r;
            }
            json_free(r);
        }
    }
    return NULL;
}

static json_t *service_op_args(const json_t *kind, const json_t *system,
                               const char *verb, const char *home, const char *run_as_user) {
    json_t *a = args0();
    aset_j(a, "kind", kind);
    aset_j(a, "system", system);
    aset_s(a, "verb", verb);
    char *h = path_norm(home);
    aset_s(a, "home", h); free(h);
    aset_s(a, "run_as_user", run_as_user);
    return a;
}

json_t *gmf_flow_restart_default(const json_t *plan_default, const json_t *target,
                                 const char *default_home, const char *run_as_user,
                                 const gmf_seams_t *s) {
    json_t *svc = pg_service(plan_default);
    if (svc) {
        const json_t *kind = svc->c.items[0];
        json_t *r = scall(s, "service_op",
                          service_op_args(kind, svc->c.items[1], "restart", default_home, NULL));
        GMF_PROP(r);
        json_free(r);
        char *ks = py_str(kind);
        char *m = xsf("restarted the default gateway via %s", ks);
        free(ks); json_free(svc);
        json_t *out = json_string(m);
        free(m);
        return out;
    }
    if (jpresent(target)) {
        const json_t *kind = target->c.items[0];
        json_t *r = scall(s, "service_op",
                          service_op_args(kind, target->c.items[1], "install", default_home, run_as_user));
        GMF_PROP(r);
        json_free(r);
        r = scall(s, "service_op",
                  service_op_args(kind, target->c.items[1], "start", default_home, NULL));
        GMF_PROP(r);
        json_free(r);
        char *ks = py_str(kind);
        char *m = xsf("installed and started the default gateway via %s", ks);
        free(ks);
        json_t *out = json_string(m);
        free(m);
        return out;
    }
    const json_t *pid = json_obj_get(plan_default, "pid");
    const char *verb = jpresent(pid) ? "restarted" : "started";
    if (jpresent(pid)) {
        char *h = path_norm(default_home);
        json_t *a = args0(); aset_s(a, "home", h); free(h);
        json_t *r = scall(s, "stop_gateway_process", a);
        GMF_PROP(r);
        json_free(r);
    }
    char *h = path_norm(default_home);
    json_t *a = args0(); aset_s(a, "home", h); free(h);
    json_t *r = scall(s, "spawn_detached_gateway", a);
    GMF_PROP(r);
    if (!jtruth(r)) {
        json_free(r);
        return raise_obj("RuntimeError", "could not spawn the default gateway (detached)");
    }
    json_free(r);
    char *m = xsf("%s the default gateway (detached; no service manager was in use)", verb);
    json_t *out = json_string(m);
    free(m);
    return out;
}

json_t *gmf_flow_enable_default_at_boot(const json_t *plan, const gmf_seams_t *s) {
    const json_t *d = plan_default(plan);
    if (!d) return raise_stopiteration();
    json_t *svc = pg_service(d);
    if (!svc) return NULL;
    const char *home = jstr(json_obj_get(plan, "default_home"));
    json_t *r = scall(s, "service_op",
                      service_op_args(svc->c.items[0], svc->c.items[1], "enable",
                                      home ? home : "", NULL));
    json_free(svc);
    if (gmf_is_raise(r)) {
        if (kind_runtimeerror(gmf_raise_class(r))) {
            char *m = xsf("default: %s", json_get_str(r, "msg", ""));
            json_free(r);
            json_t *out = json_string(m);
            free(m);
            return out;
        }
        return r;
    }
    json_free(r);
    return NULL;
}

json_t *gmf_flow_remove_secondary_gateways(const json_t *plan, const gmf_seams_t *s) {
    json_t *ss = plan_standalone_secondaries(plan);
    for (size_t i = 0; i < ss->c.count; i++) {
        const json_t *p = ss->c.items[i];
        const char *name = jstr(json_obj_get(p, "name"));
        char *home = path_norm(jstr(json_obj_get(p, "home")));
        const json_t *svcs = json_obj_get(p, "services");
        if (is_arr(svcs)) {
            for (size_t j = 0; j < svcs->c.count; j++) {
                const json_t *svc = svcs->c.items[j];
                const json_t *kind = svc->c.items[0];
                json_t *r = scall(s, "service_op",
                                  service_op_args(kind, svc->c.items[1], "stop", home, NULL));
                GMF_PROP(r);
                json_free(r);
                r = scall(s, "service_op",
                          service_op_args(kind, svc->c.items[1], "uninstall", home, NULL));
                GMF_PROP(r);
                json_free(r);
                const char *k = jstr(kind);
                const char *done = (k && !strcmp(k, "s6")) ? "parked (down file)" : "removed";
                json_t *lab = gmf_flow_service_label(svc);
                char *line = xsf("  ✓ %s: stopped and %s its %s",
                                 name ? name : "", done, jstr(lab) ? jstr(lab) : "");
                json_free(lab);
                r = gmf_print_line(s, line);
                free(line);
                GMF_PROP(r);
                json_free(r);
            }
        }
        const json_t *pid = json_obj_get(p, "pid");
        if (jpresent(pid)) {
            json_t *a = args0(); aset_s(a, "home", home);
            json_t *r = scall(s, "stop_gateway_process", a);
            GMF_PROP(r);
            json_free(r);
            char *ps = py_str(pid);
            char *line = xsf("  ✓ %s: stopped standalone gateway (pid %s)", name ? name : "", ps);
            free(ps);
            r = gmf_print_line(s, line);
            free(line);
            GMF_PROP(r);
            json_free(r);
        }
        free(home);
    }
    json_free(ss);
    return NULL;
}

/* ── resume chain ── */

json_t *gmf_flow_resume_target(const json_t *plan) {
    json_t *target = plan_target_service_kind(plan);
    GMF_PROP(target);
    json_t *user = plan_target_run_as_user(plan);
    GMF_PROP(user);
    const json_t *manifest = json_obj_get(plan, "manifest");
    if (jpresent(manifest)) {
        json_t *tf = gmf_flow_target_from_manifest(manifest);
        const json_t *mt = tf->c.items[0];
        const json_t *mu = tf->c.items[1];
        json_t *rt = jtruth(mt) ? json_copy(mt) : target;
        json_t *ru = jtruth(mu) ? json_copy(mu) : user;
        if (rt != target) json_free(target);
        if (ru != user) json_free(user);
        target = rt; user = ru;
        json_free(tf);
    }
    json_t *out = json_array();
    json_append(out, target ? target : json_null());
    json_append(out, user ? user : json_null());
    return out;
}

json_t *gmf_flow_resume_findings(const json_t *plan, const json_t *target,
                                 const char *run_as_user, const gmf_seams_t *s) {
    const json_t *b = json_obj_get(plan, "blockers");
    json_t *findings = is_arr(b) ? json_copy(b) : json_array();
    json_t *ab = gmf_flow_preflight_apply(plan, target, run_as_user, s);
    GMF_PROP(ab);
    if (jpresent(ab)) json_append(findings, ab);
    else json_free(ab);
    if (findings->c.count == 0) return findings;
    json_t *out = json_array();
    json_append(out, json_string("  ⚠ Preflight findings — resuming anyway (this host is mid-migration; refusing would"));
    json_append(out, json_string("    leave its profiles with no gateway at all). Fix these once it is converged:"));
    for (size_t i = 0; i < findings->c.count; i++) {
        char *line = xsf("    • %s", jstr(findings->c.items[i]) ? jstr(findings->c.items[i]) : "");
        json_append(out, json_string(line));
        free(line);
    }
    json_free(findings);
    return out;
}

static char *who_str(const json_t *p, const char *suffix) {
    const json_t *pid = json_obj_get(p, "pid");
    char *where;
    if (jtruth(pid)) {
        char *ps = py_str(pid);
        where = xsf("pid %s", ps);
        free(ps);
    } else {
        char *lab = pg_service_label(p);
        where = xsf("supervised by %s", lab);
        free(lab);
    }
    const char *name = jstr(json_obj_get(p, "name"));
    char *out = xsf("%s (%s%s)", name ? name : "", where, suffix);
    free(where);
    return out;
}

json_t *gmf_flow_signalled_gateways(const json_t *plan) {
    json_t *ss = plan_standalone_secondaries(plan);
    json_t *out = json_array();
    for (size_t i = 0; i < ss->c.count; i++) {
        const json_t *p = ss->c.items[i];
        const json_t *svcs = json_obj_get(p, "services");
        if (jtruth(json_obj_get(p, "pid")) || (is_arr(svcs) && svcs->c.count > 0)) {
            char *w = who_str(p, "");
            json_append(out, json_string(w));
            free(w);
        }
    }
    json_free(ss);
    const json_t *d = plan_default(plan);
    if (!d) { json_free(out); return raise_stopiteration(); }
    const json_t *svcs = json_obj_get(d, "services");
    if ((is_arr(svcs) && svcs->c.count > 0) || jtruth(json_obj_get(d, "pid"))) {
        char *w = who_str(d, ", restarted onto the new flag");
        json_append(out, json_string(w));
        free(w);
    }
    return out;
}

/* ── waits / reconcile ── */

/* PoP: gmf_wait_for_served @ hermes_cli/gateway_migrate.py:_wait_for_served */
static json_t *gmf_wait_for_served(const char *home, const json_t *expected,
                                   double timeout, const gmf_seams_t *s) {
    json_t *r = scall(s, "monotonic", args0());
    GMF_PROP(r);
    double deadline = jnum(r) + timeout;
    json_free(r);
    json_t *served = NULL;
    for (;;) {
        r = scall(s, "monotonic", args0());
        GMF_PROP(r);
        double t = jnum(r);
        json_free(r);
        if (!(t < deadline)) break;
        char *h = path_norm(home);
        json_t *a = args0(); aset_s(a, "home", h);
        r = scall(s, "home_env_enter", a);
        GMF_PROP(r);
        json_free(r);
        a = args0(); aset_s(a, "default_home", h); free(h);
        json_t *rs = scall(s, "recorded_served_profiles", a);
        if (gmf_is_raise(rs)) {
            char *h2 = path_norm(home);
            json_t *a2 = args0(); aset_s(a2, "home", h2); free(h2);
            json_t *ex = scall(s, "home_env_exit", a2);
            if (gmf_is_raise(ex)) { json_free(rs); json_free(served); return ex; }
            json_free(ex); json_free(served); return rs;
        }
        json_free(served);
        served = rs;
        char *h2 = path_norm(home);
        json_t *a2 = args0(); aset_s(a2, "home", h2); free(h2);
        r = scall(s, "home_env_exit", a2);
        GMF_PROP(r);
        json_free(r);
        if (jpresent(served) && subset(expected, served)) return served;
        a = args0(); aset_n(a, "seconds", 0.5);
        r = scall(s, "sleep", a);
        GMF_PROP(r);
        json_free(r);
    }
    return served;
}

/* PoP: gmf_wait_for_live_gateway @ hermes_cli/gateway_migrate.py:_wait_for_live_gateway */
/* PoP: gmf_poll_until_loop @ agent/deadline.py:poll_until */
static json_t *gmf_wait_for_live_gateway(const char *home, double timeout, const gmf_seams_t *s) {
    json_t *r = scall(s, "monotonic", args0());
    GMF_PROP(r);
    double deadline = jnum(r) + timeout;
    json_free(r);
    for (;;) {
        json_t *result = gmf_live_gateway_pid(home, s);
        GMF_PROP(result);
        if (jtruth(result)) return result;
        r = scall(s, "monotonic", args0());
        GMF_PROP(r);
        double remaining = deadline - jnum(r);
        json_free(r);
        if (remaining <= 0) return result;
        double sl = remaining < 0.5 ? remaining : 0.5;
        json_t *a = args0(); aset_n(a, "seconds", sl);
        r = scall(s, "sleep", a);
        GMF_PROP(r);
        json_free(r);
        json_free(result);
    }
}

/* PoP: gmf_reconcile_standalone_runtime @ hermes_cli/gateway_migrate.py:_reconcile_standalone_runtime */
static json_t *gmf_reconcile_standalone_runtime(const char *home, const json_t *names,
                                                const gmf_seams_t *s) {
    char *path = path_join(home, "gateway_state.json");
    json_t *a = args0(); aset_s(a, "path", path);
    json_t *runtime = scall(s, "read_runtime_status", a);
    if (gmf_is_raise(runtime)) { free(path); return runtime; }
    if (!jpresent(runtime)) { free(path); json_free(runtime); return NULL; }
    json_set(runtime, "served_profiles", json_array());
    const json_t *platforms = json_obj_get(runtime, "platforms");
    if (is_obj(platforms)) {
        json_t *filtered = json_object();
        for (size_t i = 0; i < platforms->c.count; i++) {
            const char *key = platforms->c.keys[i];
            bool drop = false;
            if (key && is_arr(names)) {
                for (size_t j = 0; j < names->c.count; j++) {
                    const char *n = jstr(names->c.items[j]);
                    if (!n || !n[0]) continue;
                    char *prefix = xsf("%s:", n);
                    if (!strncmp(key, prefix, strlen(prefix))) drop = true;
                    free(prefix);
                    if (drop) break;
                }
            }
            if (!drop) json_set(filtered, key, json_copy(platforms->c.items[i]));
        }
        json_set(runtime, "platforms", filtered);
    }
    a = args0();
    aset_s(a, "path", path);
    aset_j(a, "data", runtime);
    json_t *sep = json_array();
    json_append(sep, json_string(","));
    json_append(sep, json_string(":"));
    json_set(a, "separators", sep);
    free(path);
    json_t *r = scall(s, "atomic_json_write", a);
    /* The harness trace records read_runtime_status's ret BY REFERENCE (the
     * oracle's seam appends the returned object itself, and this flow mutates
     * it in place above). A non-raise OBJECT result is therefore ADOPTED by the
     * trace event and must not be freed here; raise specs (returned above) and
     * non-object results stay port-owned. */
    if (!is_obj(runtime)) json_free(runtime);
    GMF_PROP(r);
    json_free(r);
    return NULL;
}

/* ── apply_migration ── */

static json_t *apply_try_body(const json_t *plan, const json_t *target,
                              const char *user, const char *home, const gmf_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "default_home", home);
    aset_b(a, "value", true);
    json_t *r = scall(s, "write_multiplex_flag", a);
    if (gmf_is_raise(r)) return r;
    json_free(r);
    char *cfg = path_join(home, "config.yaml");
    char *line = xsf("  ✓ default: gateway.multiplex_profiles: true (%s)", cfg);
    free(cfg);
    r = gmf_print_line(s, line);
    free(line);
    if (gmf_is_raise(r)) return r;
    json_free(r);
    r = gmf_flow_remove_secondary_gateways(plan, s);
    if (gmf_is_raise(r)) return r;
    json_free(r);
    const json_t *d = plan_default(plan);
    if (!d) return raise_stopiteration();
    r = gmf_flow_restart_default(d, target, home, user, s);
    if (gmf_is_raise(r)) return r;
    line = xsf("  ✓ %s", jstr(r) ? jstr(r) : "");
    json_free(r);
    json_t *pr = gmf_print_line(s, line);
    free(line);
    if (gmf_is_raise(pr)) return pr;
    json_free(pr);
    return NULL;
}

json_t *gmf_flow_apply_migration(const json_t *plan, double served_wait, const gmf_seams_t *s) {
    const char *home = jstr(json_obj_get(plan, "default_home"));
    if (!home) home = "";
    if (plan_already_multiplexed(plan)) {
        json_t *r = gmf_print_line(s, "✓ Already multiplexed — nothing to do.");
        GMF_PROP(r);
        json_free(r);
        return json_bool(true);
    }
    json_t *rt = gmf_flow_resume_target(plan);
    GMF_PROP(rt);
    json_t *target = jpresent(rt->c.items[0]) ? json_copy(rt->c.items[0]) : NULL;
    const char *user = jstr(rt->c.items[1]);
    const json_t *manifest = json_obj_get(plan, "manifest");
    if (jpresent(manifest)) {
        char *mp = gmf_manifest_path(home);
        char *line = xsf("  ↻ resuming the migration recorded in %s", mp);
        free(mp);
        json_t *r = gmf_print_line(s, line);
        free(line);
        GMF_PROP(r);
        json_free(r);
        json_t *findings = gmf_flow_resume_findings(plan, target, user, s);
        GMF_PROP(findings);
        r = gmf_print_lines(s, findings);
        json_free(findings);
        GMF_PROP(r);
        json_free(r);
        json_t *ef = gmf_flow_enable_default_at_boot(plan, s);
        GMF_PROP(ef);
        if (jpresent(ef)) {
            line = xsf("  ⚠ %s — resuming anyway", jstr(ef) ? jstr(ef) : "");
            r = gmf_print_line(s, line);
            free(line);
            GMF_PROP(r);
            json_free(r);
        }
        json_free(ef);
    } else {
        if (plan_blocked(plan)) {
            json_t *lines = json_array();
            json_append(lines, json_string("✗ Migration refused:"));
            const json_t *b = json_obj_get(plan, "blockers");
            for (size_t i = 0; i < b->c.count; i++) {
                char *l = xsf("  • %s", jstr(b->c.items[i]) ? jstr(b->c.items[i]) : "");
                json_append(lines, json_string(l));
                free(l);
            }
            json_t *r = gmf_print_lines(s, lines);
            json_free(lines);
            GMF_PROP(r);
            json_free(r);
            json_free(target); json_free(rt);
            return json_bool(false);
        }
        json_t *pf = gmf_flow_preflight_apply(plan, target, user, s);
        GMF_PROP(pf);
        json_t *blocker;
        if (jtruth(pf)) {
            blocker = pf;
        } else {
            json_free(pf);
            blocker = gmf_flow_enable_default_at_boot(plan, s);
            GMF_PROP(blocker);
        }
        if (jpresent(blocker)) {
            json_t *lines = json_array();
            json_append(lines, json_string("✗ Migration refused before changing anything:"));
            char *l = xsf("  • %s", jstr(blocker) ? jstr(blocker) : "");
            json_append(lines, json_string(l));
            free(l);
            json_free(blocker);
            json_t *r = gmf_print_lines(s, lines);
            json_free(lines);
            GMF_PROP(r);
            json_free(r);
            json_free(target); json_free(rt);
            return json_bool(false);
        }
        json_free(blocker);
        json_t *fa = args0(); aset_s(fa, "fmt", "%Y-%m-%dT%H:%M:%S%z");
        json_t *ts = scall(s, "strftime", fa);
        GMF_PROP(ts);
        const json_t *d = plan_default(plan);
        if (!d) { json_free(ts); json_free(target); json_free(rt); return raise_stopiteration(); }
        json_t *m = json_object();
        json_set(m, "version", json_number(1));
        json_set(m, "migrated_at", ts);
        json_set(m, "flag_was", json_bool(jtruth(json_obj_get(plan, "multiplex_flag_on"))));
        json_set(m, "default", pg_to_dict(d));
        json_t *secs = json_array();
        json_t *ss = plan_standalone_secondaries(plan);
        for (size_t i = 0; i < ss->c.count; i++) json_append(secs, pg_to_dict(ss->c.items[i]));
        json_free(ss);
        json_set(m, "secondaries", secs);
        json_t *r = gmf_write_manifest(home, m, s);
        json_free(m);
        GMF_PROP(r);
        json_free(r);
    }
    json_t *fail = apply_try_body(plan, target, user, home, s);
    if (fail) {
        if (kind_base_only(gmf_raise_class(fail))) {
            json_free(target); json_free(rt);
            return fail;
        }
        char *msg = xsf("%s", json_get_str(fail, "msg", ""));
        json_free(fail);
        json_t *lines = json_array();
        char *l = xsf("  ✗ migration failed (%s)", msg);
        free(msg);
        json_append(lines, json_string(l));
        free(l);
        json_append(lines, json_string("  ↩ Restoring one host gateway so this host is not left without one..."));
        json_t *r = gmf_print_lines(s, lines);
        json_free(lines);
        GMF_PROP(r);
        json_free(r);
        json_t *rb = gmf_flow_rollback_migration(home, s);
        GMF_PROP(rb);
        if (!jtruth(rb)) {
            char *line = xsf("  Re-run %s to resume from the manifest.", GMF_MIGRATE_COMMAND);
            r = gmf_print_line(s, line);
            free(line);
            GMF_PROP(r);
            json_free(r);
        }
        json_free(rb);
        json_free(target); json_free(rt);
        return json_bool(false);
    }
    json_t *expected = plan_expected_served(plan, s);
    GMF_PROP(expected);
    json_t *served = gmf_wait_for_served(home, expected, served_wait, s);
    GMF_PROP(served);
    if (jpresent(served) && subset(expected, served)) {
        char *mp = gmf_manifest_path(home);
        json_t *a = args0(); aset_s(a, "path", mp); aset_b(a, "missing_ok", true); free(mp);
        json_t *r = scall(s, "path_unlink", a);
        GMF_PROP(r);
        json_free(r);
        json_t *lines = json_array();
        json_append(lines, json_string(""));
        char *sj = join_strs(served, ", ");
        char *l = xsf("✓ Migrated: the default gateway now serves %zu profiles: %s",
                      json_len(served), sj);
        free(sj);
        json_append(lines, json_string(l));
        free(l);
        const json_t *notices = json_obj_get(plan, "notices");
        if (is_arr(notices))
            for (size_t i = 0; i < notices->c.count; i++) {
                char *nl = xsf("  • %s", jstr(notices->c.items[i]) ? jstr(notices->c.items[i]) : "");
                json_append(lines, json_string(nl));
                free(nl);
            }
        json_free(expected); json_free(served); json_free(target); json_free(rt);
        r = gmf_print_lines(s, lines);
        json_free(lines);
        GMF_PROP(r);
        json_free(r);
        return json_bool(true);
    }
    json_t *missing = sorted_diff(expected, served);
    json_t *lines = json_array();
    json_append(lines, json_string(""));
    char *mj = join_strs(missing, ", ");
    char *l = xsf("⚠ Migration applied, but the default gateway has not confirmed serving: %s", mj);
    free(mj);
    json_append(lines, json_string(l));
    free(l);
    json_append(lines, json_string("  Check `hermes gateway status` and the gateway log; the flag and manifest are in place."));
    l = xsf("  Re-run %s once it is healthy — it resumes from the manifest.", GMF_MIGRATE_COMMAND);
    json_append(lines, json_string(l));
    free(l);
    json_free(missing); json_free(expected); json_free(served); json_free(target); json_free(rt);
    json_t *r = gmf_print_lines(s, lines);
    json_free(lines);
    GMF_PROP(r);
    json_free(r);
    return json_bool(false);
}

/* ── rollback_migration ── */

json_t *gmf_flow_rollback_migration(const char *default_home, const gmf_seams_t *s) {
    json_t *dh = NULL;
    const char *home = default_home;
    if (!home) {
        dh = gmf_default_home(s);
        GMF_PROP(dh);
        home = jstr(dh);
        if (!home) home = "";
    }
    json_t *ret = NULL;
    json_t *manifest = gmf_flow_read_manifest(home, s);
    GMF_PROP(manifest);
    char *mp = gmf_manifest_path(home);
    char *incomplete = xsf("⚠ Compensation incomplete; manifest kept at %s.\n  Re-run %s — it resumes from the manifest.",
                           mp, GMF_MIGRATE_COMMAND);
    json_t *secs = NULL, *names = NULL, *dgw = NULL, *tf = NULL, *target = NULL;
    if (!jpresent(manifest)) {
        char *line = xsf("✗ No migration manifest at %s; nothing to restore.", mp);
        json_t *r = gmf_print_line(s, line);
        free(line);
        GMF_PROP(r);
        json_free(r);
        ret = json_bool(false);
        goto done;
    }
    secs = gmf_flow_manifest_secondaries(manifest);
    GMF_PROP(secs);
    if (!secs) {
        char *line = xsf("✗ Malformed secondary records in %s; fix or delete the manifest.", mp);
        json_t *r = gmf_print_line(s, line);
        free(line);
        GMF_PROP(r);
        json_free(r);
        ret = json_bool(false);
        goto done;
    }
    {
        json_t *a = args0();
        aset_s(a, "default_home", home);
        aset_b(a, "value", jtruth(json_obj_get(manifest, "flag_was")));
        json_t *r = scall(s, "write_multiplex_flag", a);
        if (gmf_is_raise(r)) {
            if (kind_base_only(gmf_raise_class(r))) { ret = r; goto done; }
            char *line = xsf("  ✗ default: could not restore gateway.multiplex_profiles (%s)",
                             json_get_str(r, "msg", ""));
            json_free(r);
            json_t *pr = gmf_print_line(s, line);
            free(line);
            GMF_PROP(pr);
            json_free(pr);
            pr = gmf_print_line(s, incomplete);
            GMF_PROP(pr);
            json_free(pr);
            ret = json_bool(false);
            goto done;
        }
        json_free(r);
    }
    {
        json_t *r = gmf_print_line(s, "  ✓ default: gateway.multiplex_profiles restored");
        GMF_PROP(r);
        json_free(r);
    }
    {
        json_t *owner = gmf_host_gateway_owner(s);
        GMF_PROP(owner);
        json_t *pid = gmf_flow_own_gateway_pid(home, owner, s);
        json_free(owner);
        GMF_PROP(pid);
        const json_t *drec = json_obj_get(manifest, "default");
        json_t *svcs = is_obj(drec) ? gmf_flow_recorded_services(drec) : json_array();
        if (svcs->c.count == 0) {
            json_free(svcs);
            char *h = path_norm(home);
            json_t *a = args0(); aset_s(a, "home", h); free(h);
            svcs = scall(s, "installed_services", a);
            GMF_PROP(svcs);
            if (!svcs) svcs = json_array();
        }
        dgw = json_object();
        json_set(dgw, "name", json_string("default"));
        char *hn = path_norm(home);
        json_set(dgw, "home", json_string(hn));
        free(hn);
        json_set(dgw, "pid", jpresent(pid) ? pid : json_null());
        json_set(dgw, "services", svcs);
        json_set(dgw, "run_as_user", json_null());
        json_set(dgw, "uid", json_null());
        json_set(dgw, "runtime_home", json_null());
    }
    names = json_array();
    for (size_t i = 0; i < secs->c.count; i++) {
        char *n = py_str(json_obj_get(secs->c.items[i], "profile"));
        json_append(names, json_string(n));
        free(n);
    }
    {
        json_t *r = gmf_reconcile_standalone_runtime(home, names, s);
        if (gmf_is_raise(r)) {
            if (kind_base_only(gmf_raise_class(r))) { ret = r; goto done; }
            char *line = xsf("  ✗ default: could not clear multiplex-owned runtime status (%s)",
                             json_get_str(r, "msg", ""));
            json_free(r);
            json_t *pr = gmf_print_line(s, line);
            free(line);
            GMF_PROP(pr);
            json_free(pr);
            pr = gmf_print_line(s, incomplete);
            GMF_PROP(pr);
            json_free(pr);
            ret = json_bool(false);
            goto done;
        }
        json_free(r);
    }
    {
        json_t *r = gmf_print_line(s, "  ✓ default: cleared multiplex-owned runtime status");
        GMF_PROP(r);
        json_free(r);
    }
    {
        bool any_sec = false;
        for (size_t i = 0; i < secs->c.count; i++) {
            const json_t *rec = secs->c.items[i];
            if (jtruth(json_obj_get(rec, "pid"))) { any_sec = true; break; }
            json_t *rs = gmf_flow_recorded_services(rec);
            if (rs->c.count > 0) { json_free(rs); any_sec = true; break; }
            json_free(rs);
        }
        if (!pg_has_gateway(dgw) && !any_sec) {
            json_t *a = args0(); aset_s(a, "path", mp); aset_b(a, "missing_ok", true);
            json_t *r = scall(s, "path_unlink", a);
            GMF_PROP(r);
            json_free(r);
            char *line = xsf("✓ Nothing was running before this attempt (%s); the flag is restored.",
                             GMF_NOTHING_RECORDED);
            r = gmf_print_line(s, line);
            free(line);
            GMF_PROP(r);
            json_free(r);
            ret = json_bool(true);
            goto done;
        }
    }
    tf = gmf_flow_target_from_manifest(manifest);
    GMF_PROP(tf);
    target = jpresent(tf->c.items[0]) ? json_copy(tf->c.items[0]) : NULL;
    {
        const char *user = jstr(tf->c.items[1]);
        json_t *rd = gmf_flow_restart_default(dgw, target, home, user, s);
        if (gmf_is_raise(rd)) {
            if (kind_base_only(gmf_raise_class(rd))) { ret = rd; goto done; }
            char *line = xsf("  ✗ default: could not bring the host gateway back up via the recorded service manager (%s); falling back to a detached gateway",
                             json_get_str(rd, "msg", ""));
            json_free(rd);
            json_t *pr = gmf_print_line(s, line);
            free(line);
            GMF_PROP(pr);
            json_free(pr);
            char *h = path_norm(home);
            json_t *a = args0(); aset_s(a, "home", h); free(h);
            json_t *sp = scall(s, "spawn_detached_gateway", a);
            bool spawned;
            if (gmf_is_raise(sp)) {
                if (kind_base_only(gmf_raise_class(sp))) { ret = sp; goto done; }
                char *l2 = xsf("  ✗ default: the detached fallback also failed (%s)",
                               json_get_str(sp, "msg", ""));
                json_free(sp);
                pr = gmf_print_line(s, l2);
                free(l2);
                GMF_PROP(pr);
                json_free(pr);
                spawned = false;
            } else {
                spawned = jtruth(sp);
                json_free(sp);
            }
            if (!spawned) {
                pr = gmf_print_line(s, incomplete);
                GMF_PROP(pr);
                json_free(pr);
                ret = json_bool(false);
                goto done;
            }
            pr = gmf_print_line(s, "  ✓ default: started the host gateway detached (no service manager)");
            GMF_PROP(pr);
            json_free(pr);
        } else {
            char *line = xsf("  ✓ %s", jstr(rd) ? jstr(rd) : "");
            json_free(rd);
            json_t *pr = gmf_print_line(s, line);
            free(line);
            GMF_PROP(pr);
            json_free(pr);
        }
    }
    {
        json_t *live = gmf_wait_for_live_gateway(home, GMF_COMPENSATOR_WAIT, s);
        GMF_PROP(live);
        if (!jpresent(live)) {
            char *line = xsf("  ✗ default: no gateway confirmed serving this host within %.0fs (check `hermes gateway status` and the gateway log)",
                             GMF_COMPENSATOR_WAIT);
            json_t *pr = gmf_print_line(s, line);
            free(line);
            GMF_PROP(pr);
            json_free(pr);
            pr = gmf_print_line(s, incomplete);
            GMF_PROP(pr);
            json_free(pr);
            json_free(live);
            ret = json_bool(false);
            goto done;
        }
        json_t *a = args0(); aset_s(a, "path", mp); aset_b(a, "missing_ok", true);
        json_t *r = scall(s, "path_unlink", a);
        GMF_PROP(r);
        json_free(r);
        char *ls = py_str(live);
        char *line = xsf("✓ Compensated: one host gateway is running again (pid %s) on the recorded flag.", ls);
        free(ls);
        json_free(live);
        json_t *pr = gmf_print_line(s, line);
        free(line);
        GMF_PROP(pr);
        json_free(pr);
        if (names->c.count > 0) {
            char *nj = join_strs(names, ", ");
            line = xsf("  Per-profile gateways this attempt had already removed: %s. They are NOT reinstalled — one gateway per host is the only supported topology — and the host gateway serves them as soon as it multiplexes.\n  Run %s to finish converging.",
                       nj, GMF_MIGRATE_COMMAND);
            free(nj);
            pr = gmf_print_line(s, line);
            free(line);
            GMF_PROP(pr);
            json_free(pr);
        }
        ret = json_bool(true);
        goto done;
    }
done:
    free(mp);
    free(incomplete);
    json_free(manifest);
    json_free(secs);
    json_free(names);
    json_free(dgw);
    json_free(tf);
    json_free(target);
    json_free(dh);
    return ret;
}
