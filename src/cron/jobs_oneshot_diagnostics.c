/*
 * jobs_oneshot_diagnostics.c — real C11 port of upstream Hermes
 * cron/jobs_oneshot_diagnostics.py (gate w17_jobsonesho_343221).
 *
 * PoP: w17_jobsonesho_343221_write_wedged @ cron/jobs_oneshot_diagnostics.py:write_wedged
 * PoP: w17_jobsonesho_343221_write_missed @ cron/jobs_oneshot_diagnostics.py:write_missed
 *
 * The file write (cron.jobs.save_job_output), the clock (cron.jobs._hermes_now)
 * and ONESHOT_GRACE_SECONDS are injected collaborators: their observable values
 * are passed in as now_iso / grace, and the returned string is exactly the text
 * the live function would write. str() coercion of job fields matches Python.
 */
#include "noc_w17_jobsonesho_343221.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } sbuf;

static void sb_addn(sbuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 256;
        char *np;
        while (nc < b->len + n + 1) nc *= 2;
        np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void sb_add(sbuf *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static void sb_num(sbuf *b, long long v) {
    char t[48];
    snprintf(t, sizeof(t), "%lld", v);
    sb_add(b, t);
}

/* Python str() for the scalar JSON types the fixtures exercise. */
static char *py_str(const json_t *v) {
    char buf[96];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        return strdup(buf);
    }
    if (v->type == JSON_ARRAY) return strdup("[]");
    return strdup("{}");
}
static void sb_py(sbuf *b, const json_t *v) {
    char *s = py_str(v);
    if (s) { sb_add(b, s); free(s); }   /* copy out before free */
}

/* `obj.get(key, default)` with Python str() coercion. */
static void sb_field(sbuf *b, const json_t *obj, const char *key, const char *def) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!v) sb_add(b, def);
    else sb_py(b, v);
}
static json_t *truthy_obj(json_t *v) {
    if (!v || v->type != JSON_OBJECT || json_object_size(v) == 0) return NULL;
    return v;
}

static const char WEDGED_TAIL[] =
    "This one-shot job's dispatch was claimed, but the run never "
    "completed (`last_run_at` was never written) \xe2\x80\x94 the scheduler "
    "process was most likely killed or restarted mid-execution. The "
    "job has been removed to stop it re-firing; recreate it to run "
    "again.\n";

static const char MISSED_TAIL[] =
    "This one-shot's run time is more than the grace window in the "
    "past (scheduler down past the window, host asleep, or jobs.json "
    "edited), which is outside the 'will never fire' contract "
    "enforced at create/update/resume time. The job was removed "
    "without running; recreate it (or use the Run button) to "
    "schedule it again.\n";

char *w17_jobsonesho_343221_write_wedged(json_t *job, const char *now_iso) {
    json_t *lra, *rep, *claim;
    sbuf b = {0};
    if (!job || job->type != JSON_OBJECT) return NULL;
    lra = json_obj_get(job, "last_run_at");
    if (lra && lra->type != JSON_NULL) return NULL; /* a run was recorded */
    rep = truthy_obj(json_obj_get(job, "repeat"));
    claim = truthy_obj(json_obj_get(job, "run_claim"));
    sb_add(&b, "# Cron job removed without producing output\n\n");
    sb_add(&b, "- job id: "); sb_field(&b, job, "id", "None"); sb_add(&b, "\n");
    sb_add(&b, "- name: "); sb_field(&b, job, "name", "None"); sb_add(&b, "\n");
    sb_add(&b, "- dispatch claimed: ");
    sb_field(&b, rep, "completed", "?"); sb_add(&b, "/"); sb_field(&b, rep, "times", "?");
    sb_add(&b, "\n");
    sb_add(&b, "- run claimed at: ");
    sb_field(&b, claim, "at", "unknown"); sb_add(&b, " by "); sb_field(&b, claim, "by", "unknown");
    sb_add(&b, "\n");
    sb_add(&b, "- removed at: "); sb_add(&b, now_iso); sb_add(&b, "\n\n");
    sb_add(&b, WEDGED_TAIL);
    return b.p;
}

char *w17_jobsonesho_343221_write_missed(json_t *job, const char *next_run,
                                         const char *now_iso, long long grace) {
    sbuf b = {0};
    if (!job || job->type != JSON_OBJECT) return NULL;
    sb_add(&b, "# Cron job removed before firing (run time outside grace window)\n\n");
    sb_add(&b, "- job id: "); sb_field(&b, job, "id", "None"); sb_add(&b, "\n");
    sb_add(&b, "- name: "); sb_field(&b, job, "name", "None"); sb_add(&b, "\n");
    sb_add(&b, "- scheduled run time: "); sb_add(&b, next_run); sb_add(&b, "\n");
    sb_add(&b, "- grace window: "); sb_num(&b, grace); sb_add(&b, "s\n");
    sb_add(&b, "- removed at: "); sb_add(&b, now_iso); sb_add(&b, "\n\n");
    sb_add(&b, MISSED_TAIL);
    return b.p;
}
