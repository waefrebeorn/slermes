/*
 * port_gateway_windows_flow.c — Windows gateway service FLOW cores of
 * hermes_cli/gateway_windows.py. Decision sequences, call order, branch
 * conditions and error/exit paths; all OS/process/registry I/O via the
 * universal seam (gww_seams_t.call). See include/port_gateway_windows_flow.h
 * for the seam name table. No I/O is performed here.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "port_gateway_windows_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/* ── internal helpers (no Python counterpart) ── */

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

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* Mirror the oracle's PathDouble/WindowsPath normalization of a seam-returned
 * path (Path(s).as_posix()): backslashes become '/', an empty string is '.',
 * and a bare UNC share root ("//server/share") gains a trailing '/'. Paths
 * must be normalized BOTH when forwarded to the path_exists seam and when
 * appended to the returned entries list. */
static char *gww_path_posix(const char *s) {
    if (!s) return strdup("");
    if (!*s) return strdup(".");
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    if (!out) return strdup("");
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

/* malloc'd printf (caller frees). */
static char *xsf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

void gww_trace_add(gww_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
void gww_trace_free(gww_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}

json_t *gww_call(const gww_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}

const char *gww_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}

/* Exception hierarchy needed by these flows: FileNotFoundError and
 * PermissionError are OSError subclasses. */
bool gww_caught(const json_t *r, const char *cls) {
    const char *k = gww_raise_class(r);
    if (!k) return false;
    if (!strcmp(cls, k)) return true;
    if (!strcmp(cls, "OSError"))
        return !strcmp(k, "FileNotFoundError") || !strcmp(k, "PermissionError");
    return false;
}

/* convenience: build a borrowed args object quickly */
static json_t *args0(void) { return json_object(); }
static void aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* print(text) seam */
static void se_print(const gww_seams_t *s, const char *text) {
    json_t *a = args0();
    aset_s(a, "text", text);
    json_t *r = gww_call(s, "print", a);
    json_free(a);
    json_free(r);
}

/* ── pure internal helpers (real ports of unclaimed leaves) ── */

json_t *gww_attested_pids_from(const json_t *data) {
    json_t *out = json_array();
    if (!data || data->type != JSON_OBJECT) return out;
    const json_t *pids = json_obj_get(data, "pids");
    if (!pids || pids->type != JSON_ARRAY) return out;
    /* exact positive ints only; bool taints; one bad item taints the list */
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

char *gww_attestation_generation(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) return NULL;
    const json_t *g = json_obj_get(data, "generation");
    if (!jtruth(g)) return NULL;
    /* str(data["generation"]) — fixtures keep it a string */
    const char *s = jstr(g);
    if (s) return strdup(s);
    if (g->type == JSON_NUMBER) return xsf("%.0f", g->num_val);
    return strdup("");
}

double gww_attested_create_time(const json_t *data, int pid, bool *present) {
    *present = false;
    if (!data || data->type != JSON_OBJECT) return 0.0;
    const json_t *times = json_obj_get(data, "create_times");
    if (!times || times->type != JSON_OBJECT) return 0.0;
    char key[32];
    snprintf(key, sizeof(key), "%d", pid);
    const json_t *v = json_obj_get(times, key);
    if (!v) return 0.0;
    /* type(value) in (int, float) — excludes bool */
    if (v->type == JSON_NUMBER) { *present = true; return v->num_val; }
    return 0.0;
}

/* ── _attested_pid_exited_cleanly ── */
/* PoP: gww_attested_pid_exited_cleanly @ hermes_cli/gateway_windows.py:_attested_pid_exited_cleanly */
bool gww_attested_pid_exited_cleanly(int pid, bool create_time_present, double create_time,
                                     const char *home, const gww_seams_t *s) {
    /* sentinel = get_lifecycle_sentinel_path(home or _hermes_home()) */
    const char *h = home;
    char *home_free = NULL;
    if (!h) {
        json_t *a = args0();
        json_t *r = gww_call(s, "hermes_home", a);
        json_free(a);
        if (gww_caught(r, "OSError")) { json_free(r); return false; }
        if (gww_raise_class(r)) { json_free(r); return create_time_present; }
        h = jstr(r);
        if (h) { home_free = strdup(h); h = home_free; }
        json_free(r);
        if (!h) h = "";
    }
    json_t *a = args0();
    aset_s(a, "home", h);
    json_t *sp = gww_call(s, "get_lifecycle_sentinel_path", a);
    json_free(a);
    if (gww_caught(sp, "OSError")) { json_free(sp); free(home_free); return false; }
    if (gww_raise_class(sp)) { json_free(sp); free(home_free); return create_time_present; }
    const char *sentinel = jstr(sp);
    char *sent_free = sentinel ? strdup(sentinel) : NULL;
    json_free(sp);

    json_t *ra = args0();
    aset_s(ra, "path", sent_free ? sent_free : "");
    aset_s(ra, "encoding", "utf-8-sig");
    json_t *txt = gww_call(s, "read_text", ra);
    json_free(ra);
    free(sent_free);
    if (gww_caught(txt, "OSError")) { json_free(txt); free(home_free); return false; }
    if (gww_raise_class(txt)) { json_free(txt); free(home_free); return create_time_present; }
    const char *body = jstr(txt);
    char *err = NULL;
    json_t *data = body ? json_parse(body, &err) : NULL;
    free(err);
    json_free(txt);
    if (!data) { free(home_free); return create_time_present; }  /* json.loads ValueError */
    if (data->type != JSON_OBJECT) { json_free(data); free(home_free); return create_time_present; }

    bool verdict;
    const json_t *jpid = json_obj_get(data, "pid");
    double data_pid = (jpid && (jpid->type == JSON_NUMBER || jpid->type == JSON_BOOL))
                          ? (jpid->type == JSON_BOOL ? (jpid->bool_val ? 1 : 0) : jpid->num_val)
                          : NAN;
    if (create_time_present) {
        if (!(data_pid == (double)pid)) { verdict = true; }
        else {
            const json_t *birth = json_obj_get(data, "create_time");
            if (birth && birth->type == JSON_NUMBER &&
                fabs(birth->num_val - create_time) > 2.0)
                verdict = true;
            else {
                const json_t *phase = json_obj_get(data, "phase");
                verdict = phase && phase->type == JSON_STRING &&
                          !strcmp(phase->str_val ? phase->str_val : "", "exited") &&
                          data_pid == (double)pid;
            }
        }
    } else {
        const json_t *phase = json_obj_get(data, "phase");
        verdict = phase && phase->type == JSON_STRING &&
                  !strcmp(phase->str_val ? phase->str_val : "", "exited") &&
                  data_pid == (double)pid;
    }
    json_free(data);
    free(home_free);
    return verdict;
}

/* ── _attested_dead ── */
/* PoP: gww_attested_dead @ hermes_cli/gateway_windows.py:_attested_dead */
bool gww_attested_dead(const json_t *attested, const json_t *current_pids,
                       const json_t *data, const char *home, const gww_seams_t *s) {
    if (jtruth(current_pids)) return false;
    if (!attested || attested->type != JSON_ARRAY) return true;
    for (size_t i = 0; i < attested->c.count; i++) {
        const json_t *p = attested->c.items[i];
        int pid = (p && p->type == JSON_NUMBER) ? (int)p->num_val : 0;
        bool present = false;
        double ct = gww_attested_create_time(data, pid, &present);
        if (gww_attested_pid_exited_cleanly(pid, present, ct, home, s))
            return false;  /* any() short-circuits on the first clean exit */
    }
    return true;
}

/* ── attested_death_generation ── */
/* PoP: gww_attested_death_generation @ hermes_cli/gateway_windows.py:attested_death_generation */
char *gww_attested_death_generation(const json_t *current_pids, const char *home,
                                    const gww_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "home", home);
    json_t *data = gww_call(s, "read_start_attestation", a);
    json_free(a);
    json_t *attested = gww_attested_pids_from(data);
    if (!jtruth(attested)) { json_free(attested); json_free(data); return NULL; }
    json_t *ha = args0();
    aset_j(ha, "data", data);
    json_t *hr = gww_call(s, "within_horizon", ha);
    json_free(ha);
    bool within = jtruth(hr);
    json_free(hr);
    if (!within) { json_free(attested); json_free(data); return NULL; }
    bool dead = gww_attested_dead(attested, current_pids, data, home, s);
    json_free(attested);
    if (!dead) { json_free(data); return NULL; }
    char *gen = gww_attestation_generation(data);
    json_free(data);
    return gen;
}

/* ── _consume_start_attestation ── */
/* PoP: gww_consume_start_attestation @ hermes_cli/gateway_windows.py:_consume_start_attestation */
void gww_consume_start_attestation(const char *generation, const char *home,
                                   const gww_seams_t *s) {
    json_t *a = args0();
    aset_s(a, "home", home);
    json_t *data = gww_call(s, "read_start_attestation", a);
    json_free(a);
    char *gen = gww_attestation_generation(data);
    json_free(data);
    bool match = gen && generation && !strcmp(gen, generation);
    free(gen);
    if (match) {
        json_t *ca = args0();
        aset_s(ca, "home", home);
        json_t *r = gww_call(s, "clear_start_attestation", ca);
        json_free(ca);
        json_free(r);
    }
}

/* ── _remove_startup_entries ── */
/* PoP: gww_remove_startup_entries @ hermes_cli/gateway_windows.py:_remove_startup_entries */
json_t *gww_remove_startup_entries(const gww_seams_t *s) {
    json_t *done = json_array();
    json_t *warnings = json_array();
    /* the live tuple (get_startup_entry_path(), _legacy_startup_entry_path())
     * is fully built before the loop: both path calls precede any unlink. */
    const char *names[2] = {"get_startup_entry_path", "legacy_startup_entry_path"};
    char *paths[2] = {NULL, NULL};
    for (int i = 0; i < 2; i++) {
        json_t *a = args0();
        json_t *pr = gww_call(s, names[i], a);
        json_free(a);
        const char *path = jstr(pr);
        paths[i] = strdup(path ? path : "");
        json_free(pr);
    }
    for (int i = 0; i < 2; i++) {
        const char *p = paths[i];
        json_t *ua = args0();
        aset_s(ua, "path", p);
        json_t *ur = gww_call(s, "path_unlink", ua);
        json_free(ua);
        if (gww_caught(ur, "FileNotFoundError")) {
            /* pass */
        } else if (gww_caught(ur, "OSError")) {
            char *msg = xsf("Could not remove redundant Windows login item: %s "
                            "(locked or access denied; it still fires at logon)", p);
            json_append(warnings, json_string(msg));
            free(msg);
        } else {
            char *msg = xsf("Removed redundant Windows login item: %s", p);
            json_append(done, json_string(msg));
            free(msg);
        }
        json_free(ur);
    }
    free(paths[0]);
    free(paths[1]);
    json_t *out = json_object();
    json_set(out, "done", done);
    json_set(out, "warnings", warnings);
    return out;
}

/* ── redundant_autostart_entries ── */
/* PoP: gww_redundant_autostart_entries @ hermes_cli/gateway_windows.py:redundant_autostart_entries */
json_t *gww_redundant_autostart_entries(const gww_seams_t *s) {
    /* live builds the (entry, legacy) tuple first, then filters by exists(). */
    const char *names[2] = {"get_startup_entry_path", "legacy_startup_entry_path"};
    char *paths[2] = {NULL, NULL};
    for (int i = 0; i < 2; i++) {
        json_t *a = args0();
        json_t *pr = gww_call(s, names[i], a);
        json_free(a);
        const char *path = jstr(pr);
        paths[i] = gww_path_posix(path);
        json_free(pr);
    }
    json_t *entries = json_array();
    for (int i = 0; i < 2; i++) {
        json_t *ea = args0();
        aset_s(ea, "path", paths[i]);
        json_t *er = gww_call(s, "path_exists", ea);
        json_free(ea);
        if (jtruth(er)) json_append(entries, json_string(paths[i]));
        json_free(er);
    }
    free(paths[0]);
    free(paths[1]);
    json_t *ta = args0();
    json_t *tr = gww_call(s, "is_task_registered", ta);
    json_free(ta);
    bool reg = jtruth(tr);
    json_free(tr);
    if (reg) return entries;
    /* entries[1:] */
    json_t *out = json_array();
    for (size_t i = 1; i < json_len(entries); i++)
        json_append(out, json_copy(json_get(entries, i)));
    json_free(entries);
    return out;
}

/* ── reconcile_autostart_launchers ── */
/* PoP: gww_reconcile_autostart_launchers @ hermes_cli/gateway_windows.py:reconcile_autostart_launchers */
json_t *gww_reconcile_autostart_launchers(const gww_seams_t *s) {
    json_t *ta = args0();
    json_t *tr = gww_call(s, "is_task_registered", ta);
    json_free(ta);
    bool reg = jtruth(tr);
    json_free(tr);
    if (reg) return gww_remove_startup_entries(s);

    json_t *la = args0();
    json_t *lr = gww_call(s, "legacy_startup_entry_path", la);
    json_free(la);
    const char *lp = jstr(lr);
    char *legacy = lp ? strdup(lp) : NULL;
    json_free(lr);

    json_t *ea = args0();
    aset_s(ea, "path", legacy ? legacy : "");
    json_t *er = gww_call(s, "path_exists", ea);
    json_free(ea);
    bool legacy_exists = jtruth(er);
    json_free(er);

    json_t *out = json_object();
    json_t *done = json_array();
    json_t *warnings = json_array();
    if (legacy_exists) {
        json_t *wa = args0();
        json_t *wr = gww_call(s, "write_task_script", wa);
        json_free(wa);
        const char *ws = jstr(wr);
        char *script = ws ? strdup(ws) : NULL;
        json_free(wr);
        json_t *ia = args0();
        aset_s(ia, "script_path", script ? script : "");
        json_t *ir = gww_call(s, "install_startup_entry", ia);
        json_free(ia);
        free(script);
        const char *es = jstr(ir);
        char *entry = es ? strdup(es) : NULL;
        json_free(ir);
        /* re-check legacy.exists() */
        json_t *ea2 = args0();
        aset_s(ea2, "path", legacy ? legacy : "");
        json_t *er2 = gww_call(s, "path_exists", ea2);
        json_free(ea2);
        bool still = jtruth(er2);
        json_free(er2);
        if (still) {
            char *msg = xsf("Could not remove legacy Windows login item: %s "
                            "(locked or access denied; it still fires at logon beside %s)",
                            legacy ? legacy : "", entry ? entry : "");
            json_append(warnings, json_string(msg));
            free(msg);
        } else {
            char *msg = xsf("Migrated legacy Windows login item to: %s", entry ? entry : "");
            json_append(done, json_string(msg));
            free(msg);
        }
        free(entry);
    } else {
        json_t *ga = args0();
        json_t *gr = gww_call(s, "get_startup_entry_path", ga);
        json_free(ga);
        const char *gp = jstr(gr);
        char *gentry = gp ? strdup(gp) : NULL;
        json_free(gr);
        json_t *gea = args0();
        aset_s(gea, "path", gentry ? gentry : "");
        json_t *ger = gww_call(s, "path_exists", gea);
        json_free(gea);
        bool gexists = jtruth(ger);
        json_free(ger);
        if (gexists) {
            json_t *sa = args0();
            json_t *sr = gww_call(s, "get_task_script_path", sa);
            json_free(sa);
            const char *ss = jstr(sr);
            char *spath = ss ? strdup(ss) : NULL;
            json_free(sr);
            json_t *ia = args0();
            aset_s(ia, "script_path", spath ? spath : "");
            json_t *ir = gww_call(s, "install_startup_entry", ia);
            json_free(ia);
            json_free(ir);
            free(spath);
        }
        free(gentry);
    }
    free(legacy);
    json_set(out, "done", done);
    json_set(out, "warnings", warnings);
    return out;
}

/* ── _start_or_report_running ── */
/* PoP: gww_start_or_report_running @ hermes_cli/gateway_windows.py:_start_or_report_running */
void gww_start_or_report_running(const json_t *running_pids, const gww_seams_t *s) {
    json_t *pids = NULL;
    bool owned = false;
    /* running_pids is None: a JSON null node means None, not an empty list
     * (an explicit [] is truthy-checked below without calling gateway_pids). */
    if (!running_pids || running_pids->type == JSON_NULL) {
        json_t *a = args0();
        pids = gww_call(s, "gateway_pids", a);
        json_free(a);
        owned = true;
    } else {
        pids = (json_t *)running_pids;
    }
    if (jtruth(pids)) {
        json_t *a = args0();
        aset_j(a, "pids", pids);
        json_t *r = gww_call(s, "report_already_running", a);
        json_free(a);
        json_free(r);
    } else {
        json_t *a = args0();
        json_t *r = gww_call(s, "spawn_detached", a);
        json_free(a);
        json_free(r);
        json_t *ra = args0();
        aset_s(ra, "via", "direct spawn");
        json_t *rr = gww_call(s, "report_gateway_start", ra);
        json_free(ra);
        json_free(rr);
    }
    if (owned) json_free(pids);
}

/* ── _offer_elevated_install ── */
/* PoP: gww_offer_elevated_install @ hermes_cli/gateway_windows.py:_offer_elevated_install */
bool gww_offer_elevated_install(const char *headline, bool force, bool start_now,
                                bool start_on_login, const gww_seams_t *s) {
    se_print(s, headline);
    se_print(s, "  UAC is Windows' admin approval prompt; it is needed to create/update the Scheduled Task.");
    json_t *pa = args0();
    aset_s(pa, "question", "  Open the UAC prompt now?");
    aset_b(pa, "default", false);
    json_t *pr = gww_call(s, "prompt_yes_no", pa);
    json_free(pa);
    bool yes = jtruth(pr);
    json_free(pr);
    if (yes) {
        json_t *la = args0();
        aset_b(la, "force", force);
        aset_b(la, "start_now", start_now);
        aset_b(la, "start_on_login", start_on_login);
        json_t *lr = gww_call(s, "launch_elevated_install", la);
        json_free(la);
        bool ok = jtruth(lr);
        json_free(lr);
        if (ok) {
            se_print(s, "✓ Launched elevated Hermes gateway install prompt.");
            if (start_now)
                se_print(s, "  Approve the Windows UAC prompt; the elevated install will start the gateway afterwards.");
            else
                se_print(s, "  Approve the Windows UAC prompt, then run: hermes gateway status");
            return true;
        }
        se_print(s, "⚠ Falling back to Startup folder because elevation was unavailable or cancelled.");
    } else {
        se_print(s, "  Skipped elevation. Falling back to Startup folder.");
    }
    return false;
}

/* ── reconcile_scheduled_task ── */
/* PoP: gww_reconcile_scheduled_task @ hermes_cli/gateway_windows.py:reconcile_scheduled_task */
bool gww_reconcile_scheduled_task(const char *task_name, const gww_seams_t *s) {
    json_t *da = args0();
    aset_s(da, "task_name", task_name);
    json_t *dr = gww_call(s, "scheduled_task_drift", da);
    json_free(da);
    if (!jtruth(dr)) { json_free(dr); return false; }
    /* '; '.join(drift) */
    size_t cap = 64;
    for (size_t i = 0; i < json_len(dr); i++) {
        const char *t = jstr(json_get(dr, i));
        cap += (t ? strlen(t) : 0) + 2;
    }
    char *joined = malloc(cap);
    joined[0] = '\0';
    for (size_t i = 0; i < json_len(dr); i++) {
        if (i) strcat(joined, "; ");
        const char *t = jstr(json_get(dr, i));
        strcat(joined, t ? t : "");
    }
    json_free(dr);
    char *msg = xsf("↻ Repairing outdated Scheduled Task registration (%s)", joined);
    se_print(s, msg);
    free(msg);
    free(joined);

    json_t *wa = args0();
    json_t *wr = gww_call(s, "write_task_script", wa);
    json_free(wa);
    const char *ws = jstr(wr);
    char *script = ws ? strdup(ws) : NULL;
    json_free(wr);

    json_t *ia = args0();
    aset_s(ia, "task_name", task_name);
    aset_s(ia, "script_path", script ? script : "");
    json_t *ir = gww_call(s, "install_scheduled_task", ia);
    json_free(ia);
    free(script);
    bool ok = json_get_bool(ir, "ok", false);
    const char *detail = json_get_str(ir, "detail", "");
    char *line = xsf("%s %s", ok ? "✓" : "⚠", detail);
    se_print(s, line);
    free(line);
    json_free(ir);
    if (!ok) se_print(s, "  Repair manually: hermes gateway install");
    return ok;
}

/* ── _live_gateway_pids ── */
/* PoP: gww_live_gateway_pids @ hermes_cli/gateway_windows.py:_live_gateway_pids */
json_t *gww_live_gateway_pids(bool all_profiles, const char *home,
                              bool pid_filter_present, const gww_seams_t *s) {
    json_t *pids = json_array();
    if (home) {
        char *path = xsf("%s/gateway.pid", home);
        json_t *a = args0();
        aset_s(a, "path", path);
        aset_b(a, "cleanup_stale", false);
        json_t *r = gww_call(s, "get_running_pid", a);
        json_free(a);
        free(path);
        if (jtruth(r)) json_append(pids, json_copy(r));
        json_free(r);
    } else {
        json_t *a = args0();
        aset_b(a, "all_profiles", all_profiles);
        json_t *r = gww_call(s, "find_gateway_pids", a);
        json_free(a);
        if (r && r->type == JSON_ARRAY) {
            for (size_t i = 0; i < r->c.count; i++)
                json_append(pids, json_copy(r->c.items[i]));
        }
        json_free(r);
    }
    if (pid_filter_present) {
        json_t *fa = args0();
        aset_j(fa, "pids", pids);
        json_t *fr = gww_call(s, "pid_filter", fa);
        json_free(fa);
        json_free(pids);
        pids = (fr && fr->type == JSON_ARRAY) ? fr : json_array();
        if (fr && fr->type != JSON_ARRAY) json_free(fr);
    }
    return pids;
}

/* ── _gateway_pid_identities ── */
/* PoP: gww_gateway_pid_identities @ hermes_cli/gateway_windows.py:_gateway_pid_identities */
json_t *gww_gateway_pid_identities(const json_t *pids, const gww_seams_t *s) {
    json_t *out = json_object();
    json_t *ia = args0();
    json_t *ir = gww_call(s, "import_start_time", ia);
    json_free(ia);
    bool import_error = gww_caught(ir, "ImportError");
    json_free(ir);
    if (!pids || pids->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < pids->c.count; i++) {
        const json_t *p = pids->c.items[i];
        int pid = (p && p->type == JSON_NUMBER) ? (int)p->num_val : 0;
        char key[32];
        snprintf(key, sizeof(key), "%d", pid);
        if (import_error) {
            json_set(out, key, json_null());
        } else {
            json_t *a = args0();
            json_set(a, "pid", json_number(pid));
            json_t *r = gww_call(s, "get_process_start_time", a);
            json_free(a);
            json_set(out, key, r ? r : json_null());
        }
    }
    return out;
}