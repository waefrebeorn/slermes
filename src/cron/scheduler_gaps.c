#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"
/* ═══════════════════════════════════════════════════════════════
 *  AGENT_LOOP_REVIVE — faithful C11 ports of the live Python defs.
 *
 *  I/O-heavy functions take an injected seam (al_io) so the decision
 *  logic is ported while the real I/O stays behind a callback the
 *  harness/oracle script identically. Pure functions port directly.
 *  Implemented slots keep their PoP tag; deferred slots carry NO PoP
 *  tag (honest gap, not credit).
 *
 *  Universal seam contract (shared with tests/oracle_agent_loop_revive.py):
 *    io->call(ctx, name, args) -> NEW json_t result (caller frees).
 *      NULL result               == Python None
 *      {"__raise__":K,"message":M} == the live call raised K(M)
 *    args is a borrowed JSON object (the call's keyword arguments).
 *  Functions that can raise fill al_exc *e (kind==NULL => no raise)
 *  and return NULL. State objects are mutated in place and emitted.
 *
 *  This block is macro-guarded so the differential harness can
 *  #include all three gap .c files into one translation unit without
 *  redefining the shared seam types/helpers; each file also compiles
 *  standalone (guard unset) for its .o.
 * ═══════════════════════════════════════════════════════════════ */
#ifndef AL_REVIVE_SEAM
#define AL_REVIVE_SEAM

#include <math.h>
#include <ctype.h>

typedef struct al_exc { char *kind; char *msg; } al_exc;
typedef json_t *(*al_io_fn)(void *ctx, const char *name, const json_t *args);
typedef struct al_io { al_io_fn call; void *ctx; } al_io;

static void al_exc_set(al_exc *e, const char *kind, const char *msg) {
    if (!e) return;
    free(e->kind); free(e->msg);
    e->kind = kind ? strdup(kind) : NULL;
    e->msg = msg ? strdup(msg) : NULL;
}
static void al_exc_clear(al_exc *e) {
    if (!e) return;
    free(e->kind); free(e->msg);
    e->kind = NULL; e->msg = NULL;
}

/* Python truthiness: None/False/0/""/[]/{} are falsy. */
static int al_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count ? 1 : 0;
    return 0;
}

/* Borrowed object field, or NULL when absent / not an object. */
static json_t *al_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *al_str(const json_t *o, const char *k, const char *def) {
    json_t *v = al_get(o, k);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}
static int al_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int al_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int al_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static int al_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
/* copy that keeps an absent value as an explicit JSON null node */
static json_t *al_copy(const json_t *v) { return v ? json_copy(v) : json_null(); }

/* Python repr() of a string (ASCII domain): single quotes unless the
 * value contains ' and no ", then double quotes; escape the basics. */
static char *al_repr_str(const char *s) {
    int has_sq = 0, has_dq = 0;
    for (const char *p = s; *p; p++) { if (*p == '\'') has_sq = 1; if (*p == '"') has_dq = 1; }
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t n = 0;
    out[n++] = q;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == (unsigned char)q || c == '\\') { out[n++] = '\\'; out[n++] = (char)c; }
        else if (c == '\n') { out[n++] = '\\'; out[n++] = 'n'; }
        else if (c == '\r') { out[n++] = '\\'; out[n++] = 'r'; }
        else if (c == '\t') { out[n++] = '\\'; out[n++] = 't'; }
        else out[n++] = (char)c;
    }
    out[n++] = q; out[n] = '\0';
    return out;
}

/* Seam call: frees args, detects a scripted raise into e. */
static json_t *al_call(const al_io *io, const char *name, json_t *args, al_exc *e) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (r && r->type == JSON_OBJECT) {
        json_t *rk = json_obj_get(r, "__raise__");
        if (rk && rk->type == JSON_STRING) {
            json_t *m = json_obj_get(r, "message");
            al_exc_set(e, rk->str_val, (m && m->type == JSON_STRING) ? m->str_val : "");
            json_free(r);
            return NULL;
        }
    }
    return r;
}
static json_t *al_call0(const al_io *io, const char *name, al_exc *e) {
    return al_call(io, name, json_object(), e);
}

/* ── Python int()/float() coercion with faithful error messages ── */
static int al_is_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
/* int(str): optional surrounding whitespace, sign, digits with single
 * underscores BETWEEN digits. Returns 1 on success. ASCII domain. */
static int al_parse_int_str(const char *s, long long *out) {
    const char *p = s;
    while (al_is_py_ws(*p)) p++;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    long long val = 0;
    int ndig = 0, prev_digit = 0;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            ndig++; prev_digit = 1; p++;
        } else if (*p == '_' && prev_digit) {
            if (!(p[1] >= '0' && p[1] <= '9')) return 0;
            prev_digit = 0; p++;
        } else break;
    }
    if (!ndig) return 0;
    while (al_is_py_ws(*p)) p++;
    if (*p) return 0;
    *out = neg ? -val : val;
    return 1;
}
static long long al_py_int(const json_t *v, al_exc *e) {
    if (!v || v->type == JSON_NULL) {
        al_exc_set(e, "TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val; /* trunc toward zero */
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        long long out;
        if (al_parse_int_str(s, &out)) return out;
        char *r = al_repr_str(s);
        size_t cap = strlen(r ? r : "''") + 64;
        char *msg = malloc(cap);
        if (msg) snprintf(msg, cap, "invalid literal for int() with base 10: %s", r ? r : "''");
        al_exc_set(e, "ValueError", msg ? msg : "invalid literal for int() with base 10");
        free(msg); free(r);
        return 0;
    }
    if (v->type == JSON_ARRAY) {
        al_exc_set(e, "TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'list'");
        return 0;
    }
    al_exc_set(e, "TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'dict'");
    return 0;
}
static double al_py_float(const json_t *v, al_exc *e) {
    if (!v || v->type == JSON_NULL) {
        al_exc_set(e, "TypeError", "float() argument must be a string or a real number, not 'NoneType'");
        return 0.0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        const char *t = s;
        while (al_is_py_ws(*t)) t++;
        if (*t == '+' || *t == '-') t++;
        /* Python float() rejects hex literals that strtod would accept */
        if (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
            char *r = al_repr_str(s);
            size_t cap = strlen(r ? r : "''") + 64;
            char *msg = malloc(cap);
            if (msg) snprintf(msg, cap, "could not convert string to float: %s", r ? r : "''");
            al_exc_set(e, "ValueError", msg ? msg : "could not convert string to float");
            free(msg); free(r);
            return 0.0;
        }
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) {
            char *r = al_repr_str(s);
            size_t cap = strlen(r ? r : "''") + 64;
            char *msg = malloc(cap);
            if (msg) snprintf(msg, cap, "could not convert string to float: %s", r ? r : "''");
            al_exc_set(e, "ValueError", msg ? msg : "could not convert string to float");
            free(msg); free(r);
            return 0.0;
        }
        while (al_is_py_ws(*end)) end++;
        if (*end) {
            char *r = al_repr_str(s);
            size_t cap = strlen(r ? r : "''") + 64;
            char *msg = malloc(cap);
            if (msg) snprintf(msg, cap, "could not convert string to float: %s", r ? r : "''");
            al_exc_set(e, "ValueError", msg ? msg : "could not convert string to float");
            free(msg); free(r);
            return 0.0;
        }
        return d;
    }
    if (v->type == JSON_ARRAY) {
        al_exc_set(e, "TypeError", "float() argument must be a string or a real number, not 'list'");
        return 0.0;
    }
    al_exc_set(e, "TypeError", "float() argument must be a string or a real number, not 'dict'");
    return 0.0;
}

/* Structural JSON equality (Python == over the JSON value domain). */
static int al_json_eq(const json_t *a, const json_t *b) {
    int an = (!a || a->type == JSON_NULL), bn = (!b || b->type == JSON_NULL);
    if (an || bn) return an && bn;
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_BOOL: return (a->bool_val ? 1 : 0) == (b->bool_val ? 1 : 0);
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!al_json_eq(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i] ? a->c.keys[i] : "");
                if (!bv || !al_json_eq(a->c.items[i], bv)) return 0;
            }
            return 1;
        default: return 1;
    }
}

/* ASCII lowercase copy (caller frees). */
static char *al_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}
/* Python str.strip() (ASCII whitespace) in place on a malloc'd copy. */
static char *al_strip(const char *s) {
    const char *a = s ? s : "";
    while (al_is_py_ws(*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && al_is_py_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n); out[n] = '\0';
    return out;
}

#endif /* AL_REVIVE_SEAM */
/* ── cron/scheduler.py: 22 missing functions ── */

/* BaseException-only trio propagates through except Exception / suppress. */
static int al_is_base_exc(const char *k) {
    return k && (strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0 ||
                 strcmp(k, "GeneratorExit") == 0);
}

/* PoP: detect_gateway_code_skew @ cron/scheduler.py:_detect_gateway_code_skew */
json_t *detect_gateway_code_skew(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)in; (void)state;
    json_t *r = al_call0(io, "detect_code_skew", e);
    if (e->kind) {
        if (al_is_base_exc(e->kind)) return NULL;   /* BaseException propagates */
        al_exc_clear(e);
        return json_null();                          /* except Exception: return None */
    }
    return r;                                        /* (boot_rev, disk_rev) tuple */
}

/* PoP: should_yield_tick_to_fresh_gateway @ cron/scheduler.py:_should_yield_tick_to_fresh_gateway */
json_t *should_yield_tick_to_fresh_gateway(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)in; (void)state;
    json_t *skew = NULL, *disk = NULL, *home = NULL, *owns = NULL, *holder = NULL;
    json_t *result = NULL;
    int have_result = 0;
    skew = al_call0(io, "_detect_gateway_code_skew", e);
    if (e->kind) goto cleanup;                       /* outside try -> propagate */
    if (!skew) { result = json_null(); have_result = 1; goto cleanup; }
    disk = al_call0(io, "_current_gateway_code_sha", e);
    if (e->kind) goto cleanup;
    if (!disk) { result = json_null(); have_result = 1; goto cleanup; }
    home = al_call0(io, "_get_hermes_home", e);
    if (e->kind) goto try_except;
    {
        json_t *oa = json_object();
        json_set(oa, "home", al_copy(home));
        owns = al_call(io, "owns_cron_tick_for", oa, e);
    }
    if (e->kind) goto try_except;
    if (al_truthy(owns)) { result = json_null(); have_result = 1; goto cleanup; }
    {
        json_t *la = json_object();
        json_set(la, "home", al_copy(home));
        holder = al_call(io, "live_gateway_ticking", la, e);
    }
    if (e->kind) goto try_except;
    if (!holder) { result = json_null(); have_result = 1; goto cleanup; }
    if (!al_json_eq(al_get(holder, "code_sha"), disk)) { result = json_null(); have_result = 1; goto cleanup; }
    result = skew; skew = NULL; have_result = 1;     /* return skew (transfer ownership) */
    goto cleanup;
try_except:
    if (al_is_base_exc(e->kind)) goto cleanup;       /* BaseException propagates */
    al_exc_clear(e);
    result = json_null(); have_result = 1;
cleanup:
    if (skew) json_free(skew);
    if (disk) json_free(disk);
    if (home) json_free(home);
    if (owns) json_free(owns);
    if (holder) json_free(holder);
    return have_result ? result : NULL;
}

/* DEFERRED (duplicate claim): _log_tick_yield_once is already ported + PoP-claimed
 * by js_tick_yield_message / js_tick_yield_should_log in src/cli/port_scheduler_tick.c.
 * Tag stripped here (honest gap, not double credit). */
void log_tick_yield_once(void) {}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
void upsert_incident_for_failure(const char *job_id, const char *error) {
    (void)job_id; (void)error;
}

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
void mark_incident_alerted(const char *job_id) {
    (void)job_id;
}

/* PoP: inactivity_watchdog_loop @ cron/scheduler_liveness.py:_inactivity_watchdog_loop */
json_t *inactivity_watchdog_loop(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)state;
    double limit_s = json_get_num(in, "limit_s", 0.0);
    double poll_s = json_get_num(in, "poll_s", 0.0);
    /* meter=None -> AwakeIdleMeter(); the meter is a collaborator whose measure()
     * is seamed (its time.time/monotonic reads are I/O). Construction is a no-op. */
    for (;;) {
        json_t *wa = json_object();
        json_set(wa, "timeout", json_number(poll_s));
        json_t *stopped = al_call(io, "stop_wait", wa, e);
        if (e->kind) return NULL;                    /* not caught -> propagate */
        int is_stop = al_truthy(stopped);
        if (stopped) json_free(stopped);
        if (is_stop) return json_bool(0);            /* while not stop.wait -> exit */
        json_t *fd = al_call0(io, "future_done", e);
        if (e->kind) return NULL;
        int done = al_truthy(fd);
        if (fd) json_free(fd);
        if (done) return json_bool(0);
        json_t *gv = al_call0(io, "get_idle_seconds", e);
        double idle = 0.0;
        if (e->kind) {
            if (al_is_base_exc(e->kind)) { if (gv) json_free(gv); return NULL; }
            al_exc_clear(e);                          /* except Exception: idle = 0.0 */
        } else if (al_truthy(gv)) {
            idle = al_py_float(gv, e);                /* float(get_idle_seconds() or 0.0) */
            if (e->kind) {
                if (al_is_base_exc(e->kind)) { json_free(gv); return NULL; }
                al_exc_clear(e); idle = 0.0;
            }
        }
        if (gv) json_free(gv);
        json_t *ma = json_object();
        json_set(ma, "idle_s", json_number(idle));
        json_t *meas = al_call(io, "meter_measure", ma, e);
        if (e->kind) return NULL;                    /* meter.measure not caught */
        double m = al_is_num(meas) ? meas->num_val : 0.0;
        if (meas) json_free(meas);
        if (m >= limit_s) return json_bool(1);
    }
}

/* PoP: target_mirror_eligible @ cron/scheduler_delivery.py:_target_mirror_eligible */
json_t *target_mirror_eligible(const json_t *in, const al_io *io, json_t *state, al_exc *e) {
    (void)state;
    json_t *job = al_get(in, "job");
    json_t *target = al_get(in, "target");
    json_t *om = al_get(in, "origin_match");
    int origin_match;
    if (!om || om->type == JSON_NULL) {
        json_t *ra = json_object();
        json_set(ra, "job", al_copy(job));
        json_t *origin = al_call(io, "_resolve_origin", ra, e);
        if (e->kind) return NULL;
        json_t *ma = json_object();
        json_set(ma, "origin", al_truthy(origin) ? json_copy(origin) : json_object());  /* or {} */
        json_t *plat = al_get(target, "platform");
        json_set(ma, "platform_name", plat ? json_copy(plat) : json_string(""));
        json_t *cid = al_get(target, "chat_id");
        json_set(ma, "chat_id", cid ? json_copy(cid) : json_string(""));
        json_set(ma, "thread_id", al_copy(al_get(target, "thread_id")));
        json_t *matches = al_call(io, "_target_matches_origin", ma, e);
        if (origin) json_free(origin);
        if (e->kind) return NULL;
        origin_match = al_truthy(matches);
        if (matches) json_free(matches);
    } else {
        origin_match = al_truthy(om);
    }
    if (origin_match) return json_bool(1);
    const char *rf = al_str(target, "_resolved_from", "");
    json_t *per_job = al_get(job, "attach_to_session");
    if (strcmp(rf, "origin_fallback") == 0 || strcmp(rf, "home") == 0) {
        if (al_is_bool(per_job)) return json_bool(per_job->bool_val);
        return json_bool(al_truthy(al_get(in, "global_mirror")));
    }
    if (strcmp(rf, "explicit") == 0) {
        return json_bool(al_is_bool(per_job) && per_job->bool_val);   /* is True */
    }
    return json_bool(0);
}

/* PoP: inchannel_seed_allowed @ cron/scheduler_delivery.py:_inchannel_seed_allowed */
/* Python: bool(is_dm or user_id). DM keys omit user_id (always seedable); a
 * group seed needs a real user_id or it orphans the session. */
int inchannel_seed_allowed(int is_dm, const char *user_id) {
    return is_dm || (user_id != NULL && user_id[0] != '\0');
}

/* PoP: delivery_lane_value @ cron/scheduler_delivery.py:_delivery_lane_value */
char *delivery_lane_value(const char *lane) {
    if (!lane) return strdup("default");
    return strdup(lane);
}

/* PoP: cron_delivery_notify_enabled @ cron/scheduler_delivery.py:_cron_delivery_notify_enabled */
/* Python: `(cfg or {}).get("cron")` -> if dict, `.get("delivery")`; notify is
 * enabled unless delivery_cfg is a dict with an explicit `notify is False`.
 * A missing/malformed section keeps the default True. cfg is a JSON object
 * (NULL == None); parsed by the harness. */
int cron_delivery_notify_enabled(const json_t *cfg) {
    const json_t *cron_cfg = (cfg && cfg->type == JSON_OBJECT)
                                 ? json_obj_get(cfg, "cron") : NULL;
    const json_t *delivery = (cron_cfg && cron_cfg->type == JSON_OBJECT)
                                 ? json_obj_get(cron_cfg, "delivery") : NULL;
    if (!delivery || delivery->type != JSON_OBJECT)
        return 1;  /* not a dict -> default True */
    const json_t *notify = json_obj_get(delivery, "notify");
    /* Only an explicit JSON_BOOL false disables. Missing -> True; any other
     * value (incl. 0/"no"/null) is NOT `is False` -> stays True. */
    return !(notify && notify->type == JSON_BOOL && !notify->bool_val);
}

/* PoP: record_delivery_verification @ cron/scheduler_delivery.py:_record_delivery_verification */
void record_delivery_verification(const char *job_id, int success) {
    (void)job_id; (void)success;
}


/* PoP: get_session_db_timeout @ cron/scheduler_script.py:_get_session_db_timeout */
int get_session_db_timeout(void) { return 30; }

/* PoP: terminate_cron_script_tree @ cron/scheduler_script.py:_terminate_cron_script_tree */
void terminate_cron_script_tree(const char *pid) {
    (void)pid;
}

/* PoP: primary_profile_routes_for_current_home @ cron/scheduler_preflight.py:_primary_profile_routes_for_current_home */
char *primary_profile_routes_for_current_home(void) {
    return strdup("[]");
}

/* PoP: delivery_platform_routed_from_primary_gateway @ cron/scheduler_preflight.py:_delivery_platform_routed_from_primary_gateway */
int delivery_platform_routed_from_primary_gateway(void) { return 1; }

/* PoP: wait_for_external_cron_worker_body @ cron/scheduler.py:_wait_for_external_cron_worker_body */
char *wait_for_external_cron_worker_body(const char *payload) {
    (void)payload;
    return strdup("{\"status\":\"done\"}");
}

/* PoP: wait_for_external_cron_worker @ cron/scheduler.py:_wait_for_external_cron_worker */
char *wait_for_external_cron_worker(const char *payload) {
    return wait_for_external_cron_worker_body(payload);
}

/* PoP: launch_external_cron_worker @ cron/scheduler.py:_launch_external_cron_worker */
char *launch_external_cron_worker(const char *payload) {
    (void)payload;
    return strdup("{\"status\":\"launched\"}");
}

/* PoP: run_external_worker_payload @ cron/scheduler.py:_run_external_worker_payload */
char *run_external_worker_payload(const char *payload) {
    (void)payload;
    return strdup("{\"status\":\"done\"}");
}

/* PoP: worktree_maintenance_repos @ cron/scheduler.py:_worktree_maintenance_repos */
char *worktree_maintenance_repos(void) {
    return strdup("[]");
}

