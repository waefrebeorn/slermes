/*
 * update_cmd_fleet_verify.c - C11 port of upstream
 * hermes_cli/update_cmd_fleet_verify.py (gate w33_updatecmdf_6721f6).
 *
 * PoP: w33_updatecmdf_6721f6_fleet_probe_expected @ hermes_cli/update_cmd_fleet.py:_fleet_probe_expected_runtimes
 * PoP: w33_updatecmdf_6721f6_identity_pending @ hermes_cli/update_cmd_fleet_verify.py:_fleet_row_identity_pending
 * PoP: w33_updatecmdf_6721f6_live_gateway_pids @ hermes_cli/update_cmd_fleet_verify.py:_live_gateway_pids_from_fleet
 * PoP: w33_updatecmdf_6721f6_is_serve_unit @ hermes_cli/update_cmd_fleet_verify.py:_is_serve_unit
 * PoP: w33_updatecmdf_6721f6_restart_phase_owes_gateway @ hermes_cli/update_cmd_fleet_verify.py:_restart_phase_owes_gateway
 * PoP: w33_updatecmdf_6721f6_split_unaccounted @ hermes_cli/update_cmd_fleet_verify.py:_split_unaccounted
 * PoP: w33_updatecmdf_6721f6_owed_restart_reason @ hermes_cli/update_cmd_fleet_verify.py:_owed_restart_reason
 * PoP: w33_updatecmdf_6721f6_row_is_external @ hermes_cli/update_receipt.py:row_is_external
 * PoP: w33_updatecmdf_6721f6_restarted_units_gone @ hermes_cli/update_cmd_fleet_verify.py:_restarted_units_gone
 * PoP: w33_updatecmdf_6721f6_gateway_unit_runs_one_of @ hermes_cli/update_cmd_fleet_verify.py:_gateway_unit_runs_one_of
 * PoP: w33_updatecmdf_6721f6_constants @ hermes_cli/update_cmd_fleet_verify.py:<module constants>
 *
 * What is ported is exactly what post-restart verification DECIDES: the
 * rows-expected signal, the identity-pending predicate, the profile->PIDs map,
 * the serve/dashboard name gate, the "restart phase still owes a gateway"
 * verdict, the unaccounted split, the settled-unit/death property reads and the
 * follow-up reason text. The probe (`collect_fleet_versions`), the `systemctl`
 * subprocess, the settle deadline, the obligation marker and the receipt stay
 * seams: their VALUES arrive here as arguments.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w33_updatecmdf_6721f6.h"

#define SERVE_ONLY_REASON_PREFIX "serve/dashboard "
#define GATEWAY_OWED_REASON "gateways may still run pre-update code or were stopped without a successor"
#define GATEWAY_RECOVERY "; recover with `hermes gateway restart`"

/* ---- Python value semantics ------------------------------------------- */

/* bool(v) */
static int jb_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* v == "s" for a str; any non-string, or a missing key, compares unequal. */
static int jb_is_str(const json_t *v, const char *s) {
    if (!v || v->type != JSON_STRING) return 0;
    return strcmp(v->str_val ? v->str_val : "", s) == 0;
}

/* isinstance(v, int): a bool, or an integral number (libjson numbers double). */
static int jb_int_like(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { if (out) *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != (double)(long long)d) return 0;
        if (out) *out = (long long)d;
        return 1;
    }
    return 0;
}

/* str(v) for the shapes a fleet row / restart attribute realistically holds. */
static char *jb_py_str(const json_t *v) {
    char buf[64];
    const char *s;
    if (!v || v->type == JSON_NULL) s = "None";
    else if (v->type == JSON_BOOL) s = v->bool_val ? "True" : "False";
    else if (v->type == JSON_STRING) s = v->str_val ? v->str_val : "";
    else if (v->type == JSON_NUMBER) {
        long long i;
        if (jb_int_like(v, &i)) snprintf(buf, sizeof buf, "%lld", i);
        else snprintf(buf, sizeof buf, "%g", v->num_val);
        return strdup(buf);
    } else if (v->type == JSON_ARRAY) s = "[]";
    else s = "{}";
    return strdup(s);
}

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

static int cmp_cstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* "1, 2, 3" for a pid list. */
static void join_ll(char *dst, size_t cap, const long long *v, size_t n) {
    size_t i, off = 0;
    dst[0] = '\0';
    for (i = 0; i < n; i++) {
        int w = snprintf(dst + off, cap > off ? cap - off : 0, "%s%lld", i ? ", " : "", v[i]);
        if (w < 0 || (size_t)w >= (cap > off ? cap - off : 0)) return;
        off += (size_t)w;
    }
}

/* de-duplicate + ascending sort, in place; returns the new length. */
static size_t uniq_sort_ll(long long *v, size_t n) {
    size_t i, w = 0;
    qsort(v, n, sizeof *v, cmp_ll);
    for (i = 0; i < n; i++)
        if (w == 0 || v[w - 1] != v[i]) v[w++] = v[i];
    return w;
}

/* ---- text builder ----------------------------------------------------- */

typedef struct { char *p; size_t n, cap; } sb_t;

static void sb_init(sb_t *b) { b->p = NULL; b->n = 0; b->cap = 0; }

static void sb_addn(sb_t *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 128;
        char *np;
        while (cap < b->n + len + 1) cap *= 2;
        np = (char *)realloc(b->p, cap);
        if (!np) return;
        b->p = np;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, len);
    b->n += len;
    b->p[b->n] = '\0';
}

static void sb_add(sb_t *b, const char *s) { sb_addn(b, s ? s : "", strlen(s ? s : "")); }

static char *sb_take(sb_t *b) {
    char *out = b->p ? b->p : strdup("");
    b->p = NULL; b->n = b->cap = 0;
    return out;
}

/* ---- systemctl `show -p` property text -------------------------------- */

/* Last "Key=Value" occurrence in the stdout text; caller frees. */
static char *props_get(const char *stdout_text, const char *key) {
    const char *line = stdout_text;
    size_t klen = strlen(key);
    char *found = NULL;
    while (line && *line) {
        const char *end = strchr(line, '\n');
        const char *eq;
        size_t len = end ? (size_t)(end - line) : strlen(line);
        while (len && line[len - 1] == '\r') len--;
        eq = memchr(line, '=', len);
        if (eq && (size_t)(eq - line) == klen && !strncmp(line, key, klen)) {
            size_t vlen = len - klen - 1;
            char *val = (char *)malloc(vlen + 1);
            if (val) {
                memcpy(val, eq + 1, vlen);
                val[vlen] = '\0';
                free(found);
                found = val;
            }
        }
        line = end ? end + 1 : NULL;
    }
    return found;
}

/* int(props.get("MainPID") or 0); 0 on the empty/missing form, -1 when Python
 * would raise ValueError (a non-numeric value). */
static int main_pid_value(const char *stdout_text, long long *out) {
    char *raw = props_get(stdout_text, "MainPID");
    const char *s;
    long long v = 0;
    int neg = 0, any = 0, trailing_ok = 0;
    if (!raw || !raw[0]) { free(raw); *out = 0; return 0; }
    s = raw;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; any = 1; }
    while (*s == ' ' || *s == '\t') s++;
    trailing_ok = (*s == '\0');
    free(raw);
    if (!any || !trailing_ok) return -1;   /* ValueError */
    *out = neg ? -v : v;
    return 0;
}

/* The stubbed `_systemctl(...).stdout` for "<scope>/<unit>"; NULL = the
 * FileNotFoundError / TimeoutExpired / OSError path. */
static const char *seam_stdout(const json_t *responses, const char *scope,
                               const char *unit) {
    char key[512];
    const json_t *v;
    snprintf(key, sizeof key, "%s/%s", scope, unit);
    v = responses ? json_obj_get(responses, key) : NULL;
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val ? v->str_val : "";
}

/* _is_serve_or_dashboard_unit(str(name).removesuffix('.service') + '.service') */
static int serve_unit_name(const char *name) {
    size_t n = strlen(name);
    char buf[512];
    if (n >= 8 && !strcmp(name + n - 8, ".service")) n -= 8;
    if (n > sizeof buf - 10) n = sizeof buf - 10;
    memcpy(buf, name, n);
    memcpy(buf + n, ".service", 9);
    return !strcmp(buf, "hermes-serve.service")
        || !strncmp(buf, "hermes-serve-", 13)
        || !strcmp(buf, "hermes-dashboard.service")
        || !strncmp(buf, "hermes-dashboard-", 17);
}

/* ---- ported functions ------------------------------------------------ */

json_t *w33_updatecmdf_6721f6_fleet_probe_expected(
    const json_t *plan_runtimes, const json_t *pre_restart_pids,
    const json_t *restarted_services, const json_t *killed_pids) {
    if (jb_truthy(restarted_services) || jb_truthy(killed_pids)) return json_bool(true);
    /* ``pre_restart_pids is None or pre_restart_pids`` — same contract as
     * _restart_phase_failure_is_incomplete; windows_resume_token is excluded. */
    if (!pre_restart_pids || pre_restart_pids->type == JSON_NULL) return json_bool(true);
    if (jb_truthy(pre_restart_pids)) return json_bool(true);
    if (plan_runtimes && plan_runtimes->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(plan_runtimes); i++) {
            const json_t *r = json_get(plan_runtimes, i);
            if (r && r->type == JSON_OBJECT && jb_is_str(json_obj_get(r, "kind"), "gateway"))
                return json_bool(true);
        }
    }
    return json_bool(false);
}

json_t *w33_updatecmdf_6721f6_identity_pending(const json_t *row,
                                               const json_t *pre_restart_pids) {
    const json_t *pid;
    long long p;
    size_t i;
    if (!jb_is_str(row ? json_obj_get(row, "state") : NULL, "unknown")) return json_bool(false);
    if (jb_truthy(row ? json_obj_get(row, "code_sha") : NULL)) return json_bool(false);
    if (!pre_restart_pids || pre_restart_pids->type == JSON_NULL) return json_bool(false);
    if (pre_restart_pids->type != JSON_ARRAY) return json_bool(false);
    pid = json_obj_get(row, "pid");
    if (!jb_int_like(pid, &p)) return json_bool(true);
    for (i = 0; i < json_len(pre_restart_pids); i++) {
        long long q;
        if (jb_int_like(json_get(pre_restart_pids, i), &q) && q == p) return json_bool(false);
    }
    return json_bool(true);
}

json_t *w33_updatecmdf_6721f6_live_gateway_pids(const json_t *fleet_rows) {
    json_t *out = json_object();
    size_t i, j;
    if (!fleet_rows || fleet_rows->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(fleet_rows); i++) {
        const json_t *row = json_get(fleet_rows, i);
        const json_t *profile, *pidv;
        char *profile_s;
        json_t *arr;
        long long pid;
        int have = 0;
        if (!row || row->type != JSON_OBJECT) continue;
        profile = json_obj_get(row, "profile");
        pidv = json_obj_get(row, "pid");
        if (!jb_truthy(profile)) continue;
        if (!jb_int_like(pidv, &pid)) continue;
        if (jb_is_str(json_obj_get(row, "state"), "down")) continue;
        profile_s = jb_py_str(profile);
        if (!profile_s) continue;
        arr = (json_t *)json_obj_get(out, profile_s);
        if (!arr) {
            json_set(out, profile_s, json_array());
            arr = (json_t *)json_obj_get(out, profile_s);
        }
        free(profile_s);
        if (!arr) continue;
        for (j = 0; j < json_len(arr); j++) {
            long long seen;
            if (jb_int_like(json_get(arr, j), &seen) && seen == pid) { have = 1; break; }
        }
        if (!have) json_append(arr, json_number((double)pid));
    }
    for (i = 0; i < json_object_size(out); i++) {
        json_t *arr = json_object_get_at(out, i);
        size_t n = arr && arr->type == JSON_ARRAY ? json_len(arr) : 0;
        long long *vals = n ? (long long *)calloc(n, sizeof *vals) : NULL;
        size_t k, w = 0;
        for (k = 0; k < n && vals; k++)
            if (jb_int_like(json_get(arr, k), &vals[w])) w++;
        w = uniq_sort_ll(vals, w);
        if (arr) {
            while (json_len(arr)) json_array_remove(arr, json_len(arr) - 1);
            for (k = 0; k < w; k++) json_append(arr, json_number((double)vals[k]));
        }
        free(vals);
    }
    return out;
}

json_t *w33_updatecmdf_6721f6_is_serve_unit(const json_t *name) {
    char *s = jb_py_str(name);
    int ok;
    if (!s) return json_bool(false);
    ok = serve_unit_name(s);
    free(s);
    return json_bool(ok ? true : false);
}

json_t *w33_updatecmdf_6721f6_restart_phase_owes_gateway(const json_t *restart) {
    const json_t *failed;
    size_t i;
    if (!restart || restart->type != JSON_OBJECT) return json_bool(false);
    if (!jb_truthy(json_obj_get(restart, "incomplete"))) return json_bool(false);
    if (jb_truthy(json_obj_get(restart, "phase_errors"))) return json_bool(true);
    failed = json_obj_get(restart, "failed_or_stale_units");
    if (!failed || failed->type != JSON_ARRAY || json_len(failed) == 0) return json_bool(true);
    for (i = 0; i < json_len(failed); i++) {
        char *s = jb_py_str(json_get(failed, i));
        int serve = s ? serve_unit_name(s) : 0;
        free(s);
        if (!serve) return json_bool(true);
    }
    return json_bool(false);
}

json_t *w33_updatecmdf_6721f6_split_unaccounted(const json_t *outcomes) {
    json_t *out = json_array();
    long long *serve = NULL;
    size_t nserve = 0, i;
    int gateway_missed = 0;
    if (outcomes && outcomes->type == JSON_ARRAY)
        serve = (long long *)calloc(json_len(outcomes) ? json_len(outcomes) : 1, sizeof *serve);
    if (outcomes && outcomes->type == JSON_ARRAY) {
        for (i = 0; i < json_len(outcomes); i++) {
            const json_t *o = json_get(outcomes, i);
            const json_t *kind;
            long long pid;
            if (!o || o->type != JSON_OBJECT) continue;
            if (!jb_is_str(json_obj_get(o, "outcome"), "unaccounted")) continue;
            kind = json_obj_get(o, "kind");
            if (!(jb_is_str(kind, "serve") || jb_is_str(kind, "dashboard"))) gateway_missed = 1;
            if ((jb_is_str(kind, "serve") || jb_is_str(kind, "dashboard"))
                && jb_int_like(json_obj_get(o, "pid"), &pid) && serve)
                serve[nserve++] = pid;
        }
    }
    nserve = uniq_sort_ll(serve, nserve);
    json_append(out, json_bool(gateway_missed ? true : false));
    {
        json_t *pids = json_array();
        for (i = 0; i < nserve; i++) json_append(pids, json_number((double)serve[i]));
        json_append(out, pids);
    }
    free(serve);
    return out;
}

json_t *w33_updatecmdf_6721f6_owed_restart_reason(const json_t *restart,
                                                  const json_t *stopped,
                                                  const json_t *gateway_owed,
                                                  const json_t *stale_serves,
                                                  const json_t *down_serves) {
    sb_t parts, body;
    char nums[4096];
    long long *vals = NULL;
    size_t nvals = 0, i;
    int wrote = 0;

    sb_init(&parts);
    sb_init(&body);

    /* gateway part: reason + optional stopped PIDs + the recovery clamp */
    if (jb_truthy(gateway_owed)) {
        size_t n = (stopped && stopped->type == JSON_ARRAY) ? json_len(stopped) : 0;
        if (n) {
            vals = (long long *)calloc(n, sizeof *vals);
            if (vals) {
                nvals = 0;
                for (i = 0; i < n; i++) {
                    long long v;
                    if (jb_int_like(json_get(stopped, i), &v)) vals[nvals++] = v;
                }
                join_ll(nums, sizeof nums, vals, nvals);
                free(vals);
                vals = NULL;
            }
        }
        sb_add(&parts, GATEWAY_OWED_REASON);
        if (n) {
            sb_add(&parts, " (stopped PIDs ");
            sb_add(&parts, nums);
            sb_add(&parts, ")");
        }
        sb_add(&parts, GATEWAY_RECOVERY);
        wrote = 1;
    }

    /* serve/dashboard units the restart recorded as failed */
    {
        char **units = NULL;
        size_t nunits = 0, cap = 0;
        const json_t *failed = restart && restart->type == JSON_OBJECT
            ? json_obj_get(restart, "failed_or_stale_units") : NULL;
        if (failed && failed->type == JSON_ARRAY) {
            for (i = 0; i < json_len(failed); i++) {
                char *s = jb_py_str(json_get(failed, i));
                size_t n, j;
                int dup = 0;
                if (!s) continue;
                if (!serve_unit_name(s)) { free(s); continue; }
                n = strlen(s);
                if (n >= 8 && !strcmp(s + n - 8, ".service")) s[n - 8] = '\0';
                for (j = 0; j < nunits; j++)
                    if (!strcmp(units[j], s)) { dup = 1; break; }
                if (dup) { free(s); continue; }
                if (nunits == cap) {
                    size_t ncap = cap ? cap * 2 : 8;
                    char **np = (char **)realloc(units, ncap * sizeof *np);
                    if (!np) { free(s); break; }
                    units = np;
                    cap = ncap;
                }
                units[nunits++] = s;
            }
        }
        if (nunits) {
            qsort(units, nunits, sizeof *units, cmp_cstr);
            if (wrote) sb_add(&parts, "; ");
            sb_add(&parts, SERVE_ONLY_REASON_PREFIX "unit(s) ");
            for (i = 0; i < nunits; i++) {
                if (i) sb_add(&parts, ", ");
                sb_add(&parts, units[i]);
            }
            sb_add(&parts, " were not restarted onto the new code; see the systemctl advice above");
            wrote = 1;
        }
        for (i = 0; i < nunits; i++) free(units[i]);
        free(units);
    }

    /* serve/dashboard PIDs stopped and never brought back */
    if (down_serves && down_serves->type == JSON_ARRAY && json_len(down_serves)) {
        size_t n = json_len(down_serves);
        vals = (long long *)calloc(n, sizeof *vals);
        if (vals) {
            nvals = 0;
            for (i = 0; i < n; i++) {
                long long v;
                if (jb_int_like(json_get(down_serves, i), &v)) vals[nvals++] = v;
            }
            nvals = uniq_sort_ll(vals, nvals);
            if (nvals) {
                join_ll(nums, sizeof nums, vals, nvals);
                if (wrote) sb_add(&parts, "; ");
                sb_add(&parts, SERVE_ONLY_REASON_PREFIX "process(es) ");
                sb_add(&parts, nums);
                sb_add(&parts, " were stopped during the update and not brought back;"
                               " relaunch `hermes serve` / `hermes dashboard`");
                wrote = 1;
            }
            free(vals);
            vals = NULL;
        }
    }

    /* surviving pids that still run pre-update code (stale - down) */
    if (stale_serves && stale_serves->type == JSON_ARRAY) {
        size_t n = json_len(stale_serves), m = 0, k;
        long long *stale = n ? (long long *)calloc(n, sizeof *stale) : NULL;
        long long *down = NULL;
        size_t ndown = 0;
        if (down_serves && down_serves->type == JSON_ARRAY) {
            ndown = json_len(down_serves);
            down = ndown ? (long long *)calloc(ndown, sizeof *down) : NULL;
            for (i = 0; i < ndown && down; i++) {
                long long v;
                if (jb_int_like(json_get(down_serves, i), &v)) down[m++] = v;
            }
            ndown = uniq_sort_ll(down, m);
        }
        m = 0;
        for (i = 0; i < n && stale; i++) {
            long long v;
            int in_down = 0;
            if (!jb_int_like(json_get(stale_serves, i), &v)) continue;
            for (k = 0; k < ndown; k++)
                if (down[k] == v) { in_down = 1; break; }
            if (!in_down) stale[m++] = v;
        }
        m = uniq_sort_ll(stale, m);
        if (m) {
            join_ll(nums, sizeof nums, stale, m);
            if (wrote) sb_add(&parts, "; ");
            sb_add(&parts, SERVE_ONLY_REASON_PREFIX "process(es) ");
            sb_add(&parts, nums);
            sb_add(&parts, " still run pre-update code; restart them as advised above");
            wrote = 1;
        }
        free(stale);
        free(down);
    }

    free(sb_take(&body));
    if (!wrote) sb_add(&parts, GATEWAY_OWED_REASON GATEWAY_RECOVERY);
    {
        char *s = sb_take(&parts);
        json_t *out = json_string(s ? s : "");
        free(s);
        return out;
    }
}

json_t *w33_updatecmdf_6721f6_row_is_external(const json_t *row) {
    if (!row || row->type != JSON_OBJECT) return json_bool(false);
    return json_bool(jb_is_str(json_obj_get(row, "state"), "external") ? true : false);
}

json_t *w33_updatecmdf_6721f6_restarted_units_gone(const json_t *scoped_units,
                                                   const json_t *responses) {
    size_t i;
    if (!scoped_units || scoped_units->type != JSON_ARRAY || json_len(scoped_units) == 0)
        return json_bool(false);
    for (i = 0; i < json_len(scoped_units); i++) {
        char *scoped = jb_py_str(json_get(scoped_units, i));
        char *slash, *name, *loads, *active;
        const char *stdout_text;
        int gone = 0;
        if (!scoped) return json_bool(false);
        slash = strchr(scoped, '/');
        if (!slash) { free(scoped); return json_bool(false); }
        *slash = '\0';
        name = slash + 1;
        if (strcmp(scoped, "user") && strcmp(scoped, "system")) { free(scoped); return json_bool(false); }
        stdout_text = seam_stdout(responses, scoped, name);
        if (!stdout_text) { free(scoped); return json_bool(false); }
        loads = props_get(stdout_text, "LoadState");
        if (!loads || strcmp(loads, "loaded")) { free(loads); free(scoped); return json_bool(false); }
        active = props_get(stdout_text, "ActiveState");
        gone = !(active && (!strcmp(active, "active") || !strcmp(active, "activating")
                            || !strcmp(active, "reloading")));
        free(active);
        free(loads);
        free(scoped);
        if (!gone) return json_bool(false);
    }
    return json_bool(true);
}

json_t *w33_updatecmdf_6721f6_gateway_unit_runs_one_of(const json_t *unit,
                                                       const json_t *pids,
                                                       const json_t *responses) {
    static const char *const SCOPES[2] = { "user", "system" };
    char *name = jb_py_str(unit);
    long long *want = NULL;
    size_t nwant = 0, i;
    int loaded = 0, out = 0;

    if (!name) return json_bool(false);
    if (!pids || pids->type != JSON_ARRAY || json_len(pids) == 0) { free(name); return json_bool(false); }
    if (strcmp(name, "hermes-gateway") && strncmp(name, "hermes-gateway-", 15)) {
        free(name);
        return json_bool(false);
    }
    want = (long long *)calloc(json_len(pids), sizeof *want);
    if (!want) { free(name); return json_bool(false); }
    for (i = 0; i < json_len(pids); i++) {
        long long v;
        if (jb_int_like(json_get(pids, i), &v)) want[nwant++] = v;
    }

    for (i = 0; i < 2; i++) {
        const char *stdout_text = seam_stdout(responses, SCOPES[i], name);
        char *loads, *main_pid;
        long long mpid = 0;
        if (!stdout_text) goto done;                 /* OSError / TimeoutExpired */
        loads = props_get(stdout_text, "LoadState");
        if (!loads || strcmp(loads, "loaded")) { free(loads); continue; }
        free(loads);
        loaded = 1;
        main_pid = props_get(stdout_text, "MainPID");
        if (main_pid && main_pid[0] && main_pid_value(stdout_text, &mpid) < 0) {
            free(main_pid);
            goto done;                               /* ValueError */
        }
        if (!main_pid || !main_pid[0]) mpid = 0;
        free(main_pid);
        {
            size_t k;
            int hit = 0;
            for (k = 0; k < nwant; k++)
                if (want[k] == mpid) { hit = 1; break; }
            if (!hit) goto done;
        }
    }
    out = loaded;
done:
    free(want);
    free(name);
    return json_bool(out ? true : false);
}

json_t *w33_updatecmdf_6721f6_constants(void) {
    json_t *o = json_object();
    json_set(o, "serve_only_reason_prefix", json_string(SERVE_ONLY_REASON_PREFIX));
    json_set(o, "gateway_owed_reason", json_string(GATEWAY_OWED_REASON));
    json_set(o, "gateway_recovery", json_string(GATEWAY_RECOVERY));
    return o;
}
