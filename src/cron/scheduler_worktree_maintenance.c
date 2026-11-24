/*
 * scheduler_worktree_maintenance.c — C11 port of upstream Hermes
 *                                     cron/scheduler_worktree_maintenance.py
 *                                     (gate w25_schedulerw_4f882f).
 *
 * PoP: w25_schedulerw_4f882f_claim_due @ cron/scheduler_worktree_maintenance.py:claim_due_worktree_repos
 * PoP: w25_schedulerw_4f882f_sweep_slot_timeout @ cron/scheduler_worktree_maintenance.py:claim_sweep_slot
 * PoP: w25_schedulerw_4f882f_constants @ cron/scheduler_worktree_maintenance.py:SWEEP_WAIT_SECONDS
 *
 * The semaphore, the prune dict's lifetime, ``time.monotonic`` and the
 * filesystem (``os.path.realpath``) are seams.  The harness supplies the
 * monotonic stamp, the recorded prune dict and the canonical repo key, so what
 * is ported is the due computation and the wait budget.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_schedulerw_4f882f.h"

#define SWEEP_WAIT_SECONDS 120.0

static json_t *sw_error(const char *name) {
    json_t *o = json_object();
    json_set(o, "__error__", json_string(name));
    return o;
}

/* Python's numeric view: an int, a float or a bool. */
static int sw_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL)   { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

json_t *w25_schedulerw_4f882f_claim_due(const json_t *repos,
                                        const json_t *interval_s,
                                        const json_t *state) {
    json_t *due, *book;
    const json_t *last_prune;
    double now = 0.0, interval = 0.0;
    int have_interval;
    size_t i, n;

    due = json_array();
    if (!state || state->type != JSON_OBJECT)
        return sw_error("TypeError");
    if (!sw_number(json_obj_get(state, "now"), &now))
        return sw_error("TypeError");
    if (!repos || repos->type != JSON_ARRAY) return sw_error("TypeError");
    last_prune = json_obj_get(state, "last_prune");
    if (last_prune && last_prune->type != JSON_OBJECT && last_prune->type != JSON_NULL)
        return sw_error("AttributeError");
    /* The module dict, as this call sees it: a claim is visible to the rest of
     * the same sweep, exactly as the live ``_last_worktree_prune_at`` update is. */
    book = last_prune && last_prune->type == JSON_OBJECT ? json_copy(last_prune) : json_object();
    if (!book) { json_free(due); return sw_error("MemoryError"); }
    have_interval = sw_number(interval_s, &interval);
    n = json_len(repos);
    for (i = 0; i < n; i++) {
        const json_t *repo = json_get(repos, i);
        const json_t *lastv;
        double last = 0.0;
        if (!repo || repo->type != JSON_STRING) {
            json_free(due); json_free(book); return sw_error("AttributeError");
        }
        lastv = json_obj_get(book, repo->str_val);
        if (!lastv || lastv->type == JSON_NULL) {
            json_set(book, repo->str_val, json_number(now));
            json_append(due, json_string(repo->str_val));
            continue;
        }
        /* now - last >= interval_s: only reached when a stamp exists, exactly
         * as Python short-circuits the ``last is None`` branch first. */
        if (!have_interval) {
            json_free(due); json_free(book); return sw_error("TypeError");
        }
        if (!sw_number(lastv, &last)) {
            json_free(due); json_free(book); return sw_error("TypeError");
        }
        if (now - last >= interval) {
            json_set(book, repo->str_val, json_number(now));
            json_append(due, json_string(repo->str_val));
        }
    }
    json_free(book);
    return due;
}

json_t *w25_schedulerw_4f882f_sweep_slot_timeout(const json_t *wait_seconds) {
    if (!wait_seconds || wait_seconds->type == JSON_NULL) return json_number(SWEEP_WAIT_SECONDS);
    return json_copy(wait_seconds);
}

json_t *w25_schedulerw_4f882f_constants(void) {
    json_t *o = json_object();
    json_set(o, "sweep_wait_seconds", json_number(SWEEP_WAIT_SECONDS));
    return o;
}
