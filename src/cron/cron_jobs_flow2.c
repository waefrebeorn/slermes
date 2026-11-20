/* cron_jobs_flow2.c — FLOW port of the remaining UNCLAIMED cron/jobs.py flows.
 *
 * Complements cron_jobs_flow.c: this module ports the job-UPDATE, run-OUTCOME
 * and config/fence decision chains that carried no PoP claim:
 *  - _normalize_job_updates        (ordered field-normalizer fan-out + repeat)
 *  - _rederive_repeat_for_schedule_change (schedule-kind flip re-derivation)
 *  - _apply_schedule_update        (string schedule parse + next-run refresh)
 *  - _fill_missing_next_run        (compute / past-one-shot rejection)
 *  - _cron_config_number           (config read + full exception fallback)
 *  - _record_run_outcome           (post-run status/alert/claim stamping)
 *  - _advance_after_run            (repeat completion + next-run advance)
 *  - _under_fire_fence             (fire-lock context manager, fail-closed)
 *
 * Seam model (mirrors cron_jobs_flow.c): every module-level call, claimed
 * sibling or inner-imported dependency goes through
 * cj2_io_t.call(io, name, args). The dispatcher appends `name` to io->trace
 * (ordered NAME-ONLY decision sequence), returns the scripted result, applies
 * "state" effects onto io->state, and models a raised exception as
 * {"__raise__": "<Type>", "message": "..."}.
 *
 * Full builtin exception hierarchy: BaseException-only kinds
 * (BaseException/SystemExit/KeyboardInterrupt/GeneratorExit) propagate through
 * every `except Exception` equivalent; all other kinds are caught; the OSError
 * family is recognised for contextlib.suppress(OSError)/except OSError sites.
 *
 * Out-param objects (updates/job/updated) are borrowed and mutated in place
 * (the harness echoes them). Seam results are caller-owned (json_free).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

#define CJ2_ONESHOT_GRACE_SECONDS 120

typedef struct cj2_io_t {
    json_t *(*call)(struct cj2_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} cj2_io_t;

/* ── generic helpers ─────────────────────────────────────────────────────── */
static void cj2_trace(cj2_io_t *io, const char *tok) {
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, json_string(tok));
}
static json_t *cj2_call(cj2_io_t *io, const char *name, json_t *args) {
    return (io && io->call) ? io->call(io, name, args) : json_null();
}
static json_t *cj2_seam(cj2_io_t *io, const char *name, json_t *args) {
    cj2_trace(io, name);
    json_t *r = cj2_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}
static json_t *cj2_args(void) { return json_object(); }

static bool cj2_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}
static const char *cj2_exc_kind(const json_t *e) {
    return json_get_str(e, "__raise__", "");
}
static char *cj2_strdup(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *o = (char *)malloc(n + 1);
    if (o) memcpy(o, p, n + 1);
    return o;
}
static bool cj2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
/* Full hierarchy: these are NOT subclasses of Exception in Python. */
static bool cj2_base_only(const char *k) {
    return strcmp(k, "BaseException") == 0 || strcmp(k, "SystemExit") == 0
        || strcmp(k, "KeyboardInterrupt") == 0 || strcmp(k, "GeneratorExit") == 0;
}
/* OSError family (contextlib.suppress(OSError, ...) / except OSError). */
static bool cj2_oserror(const char *k) {
    static const char *fam[] = {
        "OSError", "FileNotFoundError", "PermissionError", "FileExistsError",
        "InterruptedError", "IsADirectoryError", "NotADirectoryError",
        "BlockingIOError", "ChildProcessError", "ConnectionError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "TimeoutError", "ProcessLookupError",
        "EnvironmentError", "IOError", "WindowsError"
    };
    for (size_t i = 0; i < sizeof fam / sizeof fam[0]; i++)
        if (strcmp(k, fam[i]) == 0) return true;
    return false;
}
static char *cj2_exc_str(const json_t *e) {
    const char *msg = json_get_str(e, "message", "");
    if (strcmp(cj2_exc_kind(e), "KeyError") == 0) {
        size_t n = strlen(msg);
        char *o = (char *)malloc(n + 3);
        if (!o) return cj2_strdup(msg);
        o[0] = '\''; memcpy(o + 1, msg, n); o[n + 1] = '\''; o[n + 2] = '\0';
        return o;
    }
    return cj2_strdup(msg);
}
/* Re-emit a seam exception as the function's raised return value. */
static json_t *cj2_propagate(const json_t *e) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cj2_exc_kind(e)));
    char *m = cj2_exc_str(e);
    json_set(o, "message", json_string(m ? m : ""));
    free(m);
    return o;
}
static json_t *cj2_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type ? type : "Exception"));
    json_set(e, "message", json_string(msg ? msg : ""));
    return e;
}
static const char *cj2_get_str(const json_t *o, const char *k, const char *d) {
    return json_get_str(o, k, d);
}
static json_t *cj2_get_obj(const json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static bool cj2_opt_eq(const json_t *a, const json_t *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->type == JSON_NULL && b->type == JSON_NULL) return true;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
        return a->num_val == b->num_val;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL)
        return a->bool_val == b->bool_val;
    return false;
}
static int cj2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static bool cj2_nonblank_str(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return false;
    const char *p = v->str_val;
    while (*p && cj2_is_ws((unsigned char)*p)) p++;
    return *p != '\0';
}

/* ── 1. _normalize_job_updates ───────────────────────────────────────────── */
/* PoP: cj2_normalize_job_updates @ cron/jobs.py:_normalize_job_updates */
static json_t *cj2_normalize_job_updates(cj2_io_t *io, json_t *job, json_t *updates) {
    static const char *keys[] = {
        "enabled_toolsets", "workdir", "monitor_script", "monitor_url",
        "interpreter", "reasoning_effort", "timezone", "grace_minutes",
        "catch_up", "wrap_response"
    };
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        if (json_obj_get(updates, keys[i])) {
            char tok[48];
            snprintf(tok, sizeof tok, "norm_%s", keys[i]);
            json_t *r = cj2_seam(io, tok, cj2_args());
            if (cj2_raised(r)) return cj2_propagate(r);
            json_set(updates, keys[i], r);
        }
    }
    json_t *rp = json_obj_get(updates, "repeat");
    if (rp) {
        json_t *completed = json_number(0);
        json_t *jr = json_obj_get(job, "repeat");
        if (jr && jr->type == JSON_OBJECT && jr->c.count > 0) {
            json_t *c = json_obj_get(jr, "completed");
            if (c) { json_free(completed); completed = json_copy(c); }
        }
        if (rp->type == JSON_OBJECT) {
            json_t *nr = json_copy(rp);
            json_t *r = cj2_seam(io, "normalize_repeat_value", cj2_args());
            if (cj2_raised(r)) { json_free(nr); json_free(completed); return cj2_propagate(r); }
            json_set(nr, "times", r);
            if (!json_obj_get(nr, "completed")) json_set(nr, "completed", json_copy(completed));
            json_set(updates, "repeat", nr);
        } else {
            json_t *r = cj2_seam(io, "normalize_repeat_value", cj2_args());
            if (cj2_raised(r)) { json_free(completed); return cj2_propagate(r); }
            json_t *nr = json_object();
            json_set(nr, "times", r);
            json_set(nr, "completed", completed);
            json_set(updates, "repeat", nr);
        }
    }
    return json_null();
}

/* ── 2. _rederive_repeat_for_schedule_change ─────────────────────────────── */
/* PoP: cj2_rederive_repeat @ cron/jobs.py:_rederive_repeat_for_schedule_change */
static json_t *cj2_rederive_repeat(cj2_io_t *io, json_t *job, json_t *updates) {
    json_t *us = json_obj_get(updates, "schedule");
    if (!us || json_obj_get(updates, "repeat")) return json_null();
    json_t *new_sched = us;
    if (us->type == JSON_STRING) {
        json_t *r = cj2_seam(io, "parse_schedule", cj2_args());
        if (cj2_raised(r)) return cj2_propagate(r);
        new_sched = r;
        json_set(updates, "schedule", json_copy(r));
    }
    json_t *js = cj2_get_obj(job, "schedule");
    json_t *old_kind = js ? json_obj_get(js, "kind") : NULL;
    json_t *new_kind = (new_sched && new_sched->type == JSON_OBJECT)
        ? json_obj_get(new_sched, "kind") : NULL;
    if (cj2_opt_eq(old_kind, new_kind)) {
        if (new_sched != us) json_free(new_sched);
        return json_null();
    }
    json_t *jr = cj2_get_obj(job, "repeat");
    json_t *rep = (jr && jr->c.count > 0) ? json_copy(jr) : json_object();
    json_t *times = json_obj_get(rep, "times");
    bool is_once_new = (new_kind && new_kind->type == JSON_STRING
                        && strcmp(new_kind->str_val, "once") == 0);
    bool is_once_old = (old_kind && old_kind->type == JSON_STRING
                        && strcmp(old_kind->str_val, "once") == 0);
    bool times_none = !times || times->type == JSON_NULL;
    bool times_one = times && times->type == JSON_NUMBER && times->num_val == 1.0;
    json_t *out = NULL;
    if (is_once_new && times_none) {
        json_set(rep, "times", json_number(1));
        out = rep;
    } else if (!is_once_new && is_once_old && times_one) {
        json_set(rep, "times", json_null());
        out = rep;
    }
    if (new_sched != us) json_free(new_sched);
    if (!out) { json_free(rep); return json_null(); }
    if (!json_obj_get(rep, "completed")) json_set(rep, "completed", json_number(0));
    json_set(updates, "repeat", out);
    return json_null();
}

/* ── 3. _apply_schedule_update ───────────────────────────────────────────── */
/* PoP: cj2_apply_schedule_update @ cron/jobs.py:_apply_schedule_update */
static json_t *cj2_apply_schedule_update(cj2_io_t *io, json_t *updated,
                                         json_t *updates, const char *job_id) {
    json_t *sched = json_obj_get(updated, "schedule");
    if (sched && sched->type == JSON_STRING) {
        json_t *r = cj2_seam(io, "parse_schedule", cj2_args());
        if (cj2_raised(r)) return cj2_propagate(r);
        json_set(updated, "schedule", json_copy(r));
        sched = json_obj_get(updated, "schedule");
    }
    /* schedule_display = updates.get(..., sched.get("display", updated.get(...))) */
    json_t *disp = json_obj_get(updates, "schedule_display");
    if (!disp) {
        json_t *sd = (sched && sched->type == JSON_OBJECT)
            ? json_obj_get(sched, "display") : NULL;
        if (!sd) sd = json_obj_get(updated, "schedule_display");
        disp = sd;
    }
    json_set(updated, "schedule_display", disp ? json_copy(disp) : json_null());
    const char *state = cj2_get_str(updated, "state", "");
    if (strcmp(state, "paused") != 0) {
        json_t *r = cj2_seam(io, "_next_run_or_reject_past_oneshot", cj2_args());
        if (cj2_raised(r)) return cj2_propagate(r);
        json_set(updated, "next_run_at", r);
    }
    (void)job_id;
    return json_null();
}

/* ── 4. _fill_missing_next_run ───────────────────────────────────────────── */
/* PoP: cj2_fill_missing_next_run @ cron/jobs.py:_fill_missing_next_run */
static json_t *cj2_fill_missing_next_run(cj2_io_t *io, json_t *updated) {
    json_t *en = json_obj_get(updated, "enabled");
    if (en && !cj2_truthy(en)) return json_null();
    const char *state = cj2_get_str(updated, "state", "");
    if (strcmp(state, "paused") == 0) return json_null();
    json_t *nr = json_obj_get(updated, "next_run_at");
    if (nr && cj2_truthy(nr)) return json_null();
    json_t *r = cj2_seam(io, "compute_next_run", cj2_args());
    if (cj2_raised(r)) return cj2_propagate(r);
    bool is_null = (r->type == JSON_NULL);
    if (is_null) {
        json_t *sched = cj2_get_obj(updated, "schedule");
        json_t *kind = sched ? json_obj_get(sched, "kind") : NULL;
        if (kind && kind->type == JSON_STRING && strcmp(kind->str_val, "once") == 0) {
            const char *run_at = sched ? cj2_get_str(sched, "run_at", "unknown") : "unknown";
            char msg[512];
            snprintf(msg, sizeof msg,
                     "Requested one-shot time %s is in the past (grace window: %ds) and cannot be scheduled.",
                     run_at, CJ2_ONESHOT_GRACE_SECONDS);
            json_free(r);
            return cj2_raise("ValueError", msg);
        }
    }
    json_set(updated, "next_run_at", r);
    return json_null();
}

/* ── 5. _cron_config_number ──────────────────────────────────────────────── */
/* PoP: cj2_cron_config_number @ cron/jobs.py:_cron_config_number */
static json_t *cj2_cron_config_number(cj2_io_t *io) {
    json_t *r = cj2_seam(io, "load_config", cj2_args());
    if (cj2_raised(r)) {
        if (cj2_base_only(cj2_exc_kind(r))) return cj2_propagate(r);
        json_free(r);
        return cj2_seam(io, "cast", cj2_args());   /* cast(default) */
    }
    json_t *cfg = cj2_truthy(r) ? r : NULL;
    json_t *cron = cfg && cfg->type == JSON_OBJECT ? json_obj_get(cfg, "cron") : NULL;
    json_t *r2 = cj2_seam(io, "cast", cj2_args()); /* cast(cron_cfg.get(key, default)) */
    json_free(r);
    if (cj2_raised(r2)) {
        if (cj2_base_only(cj2_exc_kind(r2))) return cj2_propagate(r2);
        json_free(r2);
        return cj2_seam(io, "cast", cj2_args());   /* cast(default) */
    }
    (void)cron;
    return r2;
}

/* ── 6. _record_run_outcome ──────────────────────────────────────────────── */
/* PoP: cj2_record_run_outcome @ cron/jobs.py:_record_run_outcome */
static json_t *cj2_record_run_outcome(cj2_io_t *io, json_t *job, bool success,
                                     json_t *error, json_t *delivery_error,
                                     json_t *status, json_t *now) {
    json_set(job, "last_run_at", json_copy(now));
    json_obj_del(job, "manual_run_at");
    json_obj_del(job, "manual_run_restore_at");
    json_obj_del(job, "manual_run_prompt");
    bool delivery_failed = cj2_nonblank_str(delivery_error);
    bool cancelled = status && status->type == JSON_STRING
        && strcmp(status->str_val, "cancelled") == 0;
    bool status_truthy = status && cj2_truthy(status);
    json_t *last_status;
    if (status_truthy) {
        last_status = json_copy(status);
    } else if (!success) {
        /* Python: status or ("error" if not success else
         *        ("delivery_failed" if delivery_failed else "ok")) */
        last_status = json_string("error");
    } else if (delivery_failed) {
        last_status = json_string("delivery_failed");
    } else {
        last_status = json_string("ok");
    }
    json_set(job, "last_status", last_status);

    if (success || cancelled) {
        json_set(job, "last_error", json_null());
    } else {
        json_t *r = cj2_seam(io, "sanitize_persisted_error", cj2_args());
        if (cj2_raised(r)) return cj2_propagate(r);
        json_set(job, "last_error", r);
    }
    if (success) {
        json_obj_del(job, "preflight_alerted");
        json_obj_del(job, "last_fire_error");
        json_set(job, "failure_streak", json_number(0));
    } else if (!cancelled) {
        json_t *fs = json_obj_get(job, "failure_streak");
        double n = 0.0;
        if (fs && fs->type == JSON_NUMBER) n = fs->num_val;
        json_set(job, "failure_streak", json_number(n + 1.0));
        json_t *r = cj2_seam(io, "sanitize_persisted_error", cj2_args());
        if (cj2_raised(r)) return cj2_propagate(r);
        json_t *detail;
        if (cj2_truthy(r)) detail = r;
        else {
            json_free(r);
            detail = status_truthy ? json_copy(status) : json_string("run failed");
        }
        json_t *lf = json_object();
        json_set(lf, "at", json_copy(now));
        json_set(lf, "detail", detail);
        json_set(job, "last_failure", lf);
    }
    json_t *r = cj2_seam(io, "sanitize_persisted_error", cj2_args());
    if (cj2_raised(r)) return cj2_propagate(r);
    json_set(job, "last_delivery_error", r);
    json_set(job, "fire_claim", json_null());
    json_obj_del(job, "pending_slot");
    if (json_obj_get(job, "run_claim")) json_set(job, "run_claim", json_null());
    return json_null();
}

/* ── 7. _advance_after_run ───────────────────────────────────────────────── */
/* PoP: cj2_advance_after_run @ cron/jobs.py:_advance_after_run */
static json_t *cj2_advance_after_run(cj2_io_t *io, json_t *job, json_t *now,
                                    bool ladder_rung) {
    json_t *sched = cj2_get_obj(job, "schedule");
    json_t *kind = sched ? json_obj_get(sched, "kind") : NULL;
    const char *kind_s = (kind && kind->type == JSON_STRING) ? kind->str_val : "";
    json_t *rep = json_obj_get(job, "repeat");
    if (rep && cj2_truthy(rep) && rep->type == JSON_OBJECT) {
        json_t *times = json_obj_get(rep, "times");
        bool finite = times && times->type == JSON_NUMBER && times->num_val > 0;
        json_t *cv = json_obj_get(rep, "completed");
        double completed = (cv && cv->type == JSON_NUMBER) ? cv->num_val : 0.0;
        bool once_finite_started = (strcmp(kind_s, "once") == 0 && finite && completed > 0);
        if (!ladder_rung && !once_finite_started) {
            completed += 1.0;
            json_set(rep, "completed", json_number(completed));
        }
        if (finite && completed >= times->num_val) {
            json_t *r = cj2_seam(io, "_complete_job_record", cj2_args());
            if (cj2_raised(r)) return cj2_propagate(r);
            json_free(r);
            return json_null();
        }
    }
    json_t *r = cj2_seam(io, "compute_next_run", cj2_args());
    if (cj2_raised(r)) return cj2_propagate(r);
    json_set(job, "next_run_at", r);
    if (r->type != JSON_NULL) {
        const char *state = cj2_get_str(job, "state", "");
        if (strcmp(state, "paused") != 0) json_set(job, "state", json_string("scheduled"));
    } else if (strcmp(kind_s, "cron") == 0 || strcmp(kind_s, "interval") == 0) {
        json_set(job, "state", json_string("error"));
        json_t *le = json_obj_get(job, "last_error");
        if (!le || !cj2_truthy(le))
            json_set(job, "last_error", json_string(
                "Failed to compute next run for recurring schedule (is the 'croniter' package installed in the gateway's Python env?)"));
        json_t *lr = cj2_seam(io, "logger.error", cj2_args());
        if (cj2_raised(lr)) return cj2_propagate(lr);
        json_free(lr);
    } else {
        json_t *cr = cj2_seam(io, "_complete_job_record", cj2_args());
        if (cj2_raised(cr)) return cj2_propagate(cr);
        json_free(cr);
    }
    return json_null();
}

/* ── 8. _under_fire_fence ────────────────────────────────────────────────── */
/* PoP: cj2_under_fire_fence @ cron/jobs.py:_under_fire_fence */
static json_t *cj2_under_fire_fence(cj2_io_t *io) {
    json_t *enter = cj2_seam(io, "fire_lock.enter", cj2_args());
    if (cj2_raised(enter)) return cj2_propagate(enter);
    bool acquired = cj2_truthy(enter);
    json_free(enter);
    if (!acquired) {
        json_t *ex = cj2_seam(io, "fire_lock.exit", cj2_args());
        if (cj2_raised(ex)) return cj2_propagate(ex);
        json_free(ex);
        return json_bool(false);
    }
    json_t *fnr = cj2_seam(io, "fn", cj2_args());
    json_t *ex = cj2_seam(io, "fire_lock.exit", cj2_args());
    if (cj2_raised(fnr)) { json_free(ex); return cj2_propagate(fnr); }
    if (cj2_raised(ex)) { json_free(fnr); return cj2_propagate(ex); }
    json_free(ex);
    return fnr;
}
