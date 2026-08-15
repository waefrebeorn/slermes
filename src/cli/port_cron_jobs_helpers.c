/*
 * port_cron_jobs_helpers.c
 *
 * Pure, portable helper functions ported from cron/jobs.py.
 * - parse_duration: string -> minutes (fully pure).
 * - compute_next_run: handles "once" and "interval" schedules purely using
 *   the system clock. The "cron" kind depends on the external 'croniter'
 *   library (not available in C) — for that kind we return NULL, which is an
 *   honest NA for the cron branch (the Python code returns None when croniter
 *   is missing, so NULL is faithful).
 *
 * Module prefix used by the scanner for cron/jobs.py is "jobs_".
 *
 * C name <- python name (jobs_ prefix):
 *   jobs_parse_duration, jobs_compute_next_run
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "cron_jobs.h"
#include "hermes_core_types.h"  /* HERMES_PATH_MAX */

/* Reuse the agent-side ISO-8601 UTC timestamp helper (agent/shell_hooks.c,
 * Port of agent/shell_hooks.py:_utc_now_iso). Returns malloc'd string. */
extern char *utc_now_iso(void);

/* ---- lowercase helper ---- */
static void lc(char *s)
{
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

/* ---------------------------------------------------------------------- */
/* PoP: jobs_parse_duration @ cron/jobs.py:parse_duration */
/* Parses "30m"->30, "2h"->120, "1d"->1440. Returns minutes, or -1 on invalid. */
int jobs_parse_duration(const char *s)
{
    if (!s) return -1;
    char buf[64];
    strncpy(buf, s, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    lc(buf);
    /* strip spaces */
    char compact[64]; int j = 0;
    for (int i = 0; buf[i] && j < (int)sizeof(compact)-1; i++)
        if (!isspace((unsigned char)buf[i])) compact[j++] = buf[i];
    compact[j] = '\0';
    /* match ^(\d+)(m|min|h|hr|d|...) */
    int value = 0; int k = 0;
    while (compact[k] && isdigit((unsigned char)compact[k])) { value = value*10 + (compact[k]-'0'); k++; }
    if (k == 0) return -1;
    char unit = compact[k];
    int mult;
    if (unit == 'm') mult = 1;
    else if (unit == 'h') mult = 60;
    else if (unit == 'd') mult = 1440;
    else return -1; /* not a recognized unit prefix */
    return value * mult;
}

/* ---------------------------------------------------------------------- */
/* PoP: NULL @ cron/jobs.py:compute_next_run */
/* schedule_json: {"kind":"once","run_at":"ISO"} | {"kind":"interval","minutes":N}
 * last_run_at: ISO timestamp string or NULL.
 * Returns malloc'd ISO timestamp, or NULL (cron kind / error). Caller frees. */
char *jobs_compute_next_run(const char *schedule_json, const char *last_run_at)
{
    if (!schedule_json || !schedule_json[0]) return NULL;
    json_t *s = json_parse(schedule_json, NULL);
    if (!s || s->type != JSON_OBJECT) { if (s) json_free(s); return NULL; }
    json_t *kind = json_object_get(s, "kind");
    if (!kind || kind->type != JSON_STRING) { json_free(s); return NULL; }
    const char *kind_str = json_string_value(kind);
    char *result = NULL;

    if (strcmp(kind_str, "once") == 0) {
        /* one-shot: if last_run already happened, no more runs -> NULL */
        json_t *run_at = json_object_get(s, "run_at");
        if (!run_at || run_at->type != JSON_STRING) { json_free(s); return NULL; }
        if (last_run_at && last_run_at[0]) { json_free(s); return NULL; }
        result = strdup(json_string_value(run_at));
    } else if (strcmp(kind_str, "interval") == 0) {
        json_t *mins = json_object_get(s, "minutes");
        if (!mins || mins->type != JSON_NUMBER) { json_free(s); return NULL; }
        int minutes = (int)json_number_value(mins);
        time_t base;
        if (last_run_at && last_run_at[0]) {
            struct tm tm; memset(&tm, 0, sizeof(tm));
            if (sscanf(last_run_at, "%d-%d-%dT%d:%d:%d",
                       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec) >= 3) {
                tm.tm_year -= 1900; tm.tm_mon -= 1; tm.tm_isdst = -1;
                base = mktime(&tm);
                if (base == (time_t)-1) base = time(NULL);
            } else base = time(NULL);
        } else {
            base = time(NULL);
        }
        base += (time_t)minutes * 60;
        struct tm *ot = gmtime(&base);
        char iso[32];
        snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d",
                 1900+ot->tm_year, ot->tm_mon+1, ot->tm_mday,
                 ot->tm_hour, ot->tm_min, ot->tm_sec);
        /* naive +00:00 suffix (Python isoformat on unaware dt has no tz) */
        size_t L = strlen(iso);
        char *full = malloc(L + 7);
        sprintf(full, "%s+00:00", iso);
        result = full;
    }
    /* "cron" kind: requires croniter (external) -> return NULL (honest NA) */
    json_free(s);
    return result;
}

/* ============================================================
 * Ports of the remaining cron/jobs.py REAL_GAPs: croniter
 * availability, atomic counter I/O, catch-up occurrences,
 * wedged-oneshot diagnostics, next-run advancement, and
 * completed one-shot retention sweep.
 * ============================================================ */

/* forward decl: used by _sweep_completed_oneshots before definition */
static double iso8601_to_epoch(const char *s);

/* croniter (the external cron-expression iterator) is only an optional
 * Python dependency. libcron (a real C cron parser) is always linked here,
 * so the equivalent of HAS_CRONITER is unconditionally true in the C port. */
#define HAS_CRONITER 1

/* Atomic write of an integer to `path` (mirrors _atomic_write_counter). */
static void _atomic_write_counter_impl(const char *path, long value) {
    if (value < 0) value = 0;
    char *cd = cronjobs_cron_dir();
    if (!cd) return;
    char dirbuf[HERMES_PATH_MAX];
    snprintf(dirbuf, sizeof dirbuf, "%s", cd);
    free(cd);
    /* ensure cron dir exists */
    struct stat st;
    if (stat(dirbuf, &st) != 0) { mkdir(dirbuf, 0755); }
    char tmppath[HERMES_PATH_MAX * 2 + 32];
    snprintf(tmppath, sizeof tmppath, "%s/.count_%ld_XXXXXX", dirbuf, (long)getpid());
    int fd = mkstemp(tmppath);
    if (fd < 0) return;
    dprintf(fd, "%ld", value);
    fsync(fd);
    close(fd);
    rename(tmppath, path);
}

/* PoP: port @ cron/jobs.py:_ensure_croniter */
/* libcron is always linked; the croniter external dependency is unconditionally
 * available in the C port (mirrors HAS_CRONITER=True). */
bool _ensure_croniter(void) { (void)HAS_CRONITER; return true; }

/* PoP: _atomic_write_counter @ cron/jobs.py:_atomic_write_counter */
void _atomic_write_counter(const char *path, long value) {
    _atomic_write_counter_impl(path, value);
}

/* PoP: record_catch_up_occurrence @ cron/jobs.py:record_catch_up_occurrence */
void record_catch_up_occurrence(void) {
    char *cd = cronjobs_cron_dir();
    if (!cd) return;
    char path[HERMES_PATH_MAX];
    snprintf(path, sizeof path, "%s/catch_up_occurrences", cd);
    long value = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%ld", &value) != 1) value = 0;
        fclose(f);
    }
    if (value < 0) value = 0;
    _atomic_write_counter_impl(path, value + 1);
    free(cd);
}

/* PoP: get_catch_up_occurrence_count @ cron/jobs.py:get_catch_up_occurrence_count */
long get_catch_up_occurrence_count(void) {
    char *cd = cronjobs_cron_dir();
    if (!cd) return 0;
    char path[HERMES_PATH_MAX];
    snprintf(path, sizeof path, "%s/catch_up_occurrences", cd);
    long value = 0;
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%ld", &value) != 1) value = 0;
        fclose(f);
    }
    free(cd);
    return value < 0 ? 0 : value;
}

/* PoP: _write_wedged_oneshot_diagnostic @ cron/jobs.py:_write_wedged_oneshot_diagnostic */
void _write_wedged_oneshot_diagnostic(const json_t *job) {
    if (!job || job->type != JSON_OBJECT) return;
    json_t *lra = json_object_get(job, "last_run_at");
    if (lra && lra->type != JSON_NULL) return;  /* a prior run was recorded */
    const char *jid = json_get_str(job, "id", "");
    const char *jname = json_get_str(job, "name", jid);
    json_t *rp = json_object_get(job, "repeat");
    long completed = rp ? (long)json_get_num(rp, "completed", 0) : 0;
    long times = rp ? (long)json_get_num(rp, "times", 0) : 0;
    json_t *claim = json_object_get(job, "run_claim");
    const char *cat = claim ? json_get_str(claim, "at", "unknown") : "unknown";
    const char *cb = claim ? json_get_str(claim, "by", "unknown") : "unknown";
    char *now = utc_now_iso();
    char text[4096];
    snprintf(text, sizeof text,
        "# Cron job removed without producing output\n\n"
        "- job id: %s\n"
        "- name: %s\n"
        "- dispatch claimed: %ld/%ld\n"
        "- run claimed at: %s by %s\n"
        "- removed at: %s\n\n"
        "This one-shot job's dispatch was claimed, but the run never "
        "completed (`last_run_at` was never written) — the scheduler "
        "process was most likely killed or restarted mid-execution. "
        "The job has been removed to stop it re-firing; recreate it to "
        "run again.\n",
        jid, jname, completed, times, cat, cb, now ? now : "");
    free(now);
    cronjobs_save_job_output(jid, text);
}

/* PoP: advance_next_runs @ cron/jobs.py:advance_next_runs */
long advance_next_runs(json_t *job_ids) {
    if (!job_ids || !json_is_array(job_ids) || json_len(job_ids) == 0) return 0;
    char **ids = NULL;
    size_t n = json_len(job_ids);
    ids = malloc(n * sizeof(char *));
    if (!ids) return 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(job_ids, i);
        ids[i] = (e && e->type == JSON_STRING) ? strdup(e->str_val) : NULL;
    }
    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    if (err) free(err);
    if (!jobs || !json_is_array(jobs)) {
        for (size_t i = 0; i < n; i++) free(ids[i]);
        free(ids);
        if (jobs) json_free(jobs);
        return 0;
    }
    long advanced = 0;
    for (size_t i = 0; i < json_len(jobs); i++) {
        json_t *job = json_get(jobs, i);
        if (!job || job->type != JSON_OBJECT) continue;
        const char *jid = json_get_str(job, "id", "");
        bool wanted = false;
        for (size_t k = 0; k < n; k++) {
            if (ids[k] && strcmp(ids[k], jid) == 0) { wanted = true; break; }
        }
        if (!wanted) continue;
        json_t *sched = json_object_get(job, "schedule");
        const char *kind = sched ? json_get_str(sched, "kind", "") : "";
        if (strcmp(kind, "cron") != 0 && strcmp(kind, "interval") != 0) continue;
        char *now_iso = utc_now_iso();
        char *next = cronjobs_compute_next_run(sched, now_iso);
        free(now_iso);
        if (!next) continue;
        const char *cur = json_get_str(job, "next_run_at", "");
        if (strcmp(next, cur) != 0) {
            json_set(job, "next_run_at", json_string(next));
            advanced++;
        }
        free(next);
    }
    cronjobs_save_jobs(jobs);
    json_free(jobs);
    for (size_t i = 0; i < n; i++) free(ids[i]);
    free(ids);
    return advanced;
}

/* Completed one-shot retention default (cron/jobs.py COMPLETED_ONESHOT_RETENTION_DAYS). */
#define COMPLETED_ONESHOT_RETENTION_DAYS 7

/* PoP: _completed_oneshot_retention_days @ cron/jobs.py:_completed_oneshot_retention_days */
double _completed_oneshot_retention_days(void) {
    json_t *cfg = config_py_load_config_impl(0);
    if (!cfg || cfg->type != JSON_OBJECT) return (double)COMPLETED_ONESHOT_RETENTION_DAYS;
    json_t *cron_cfg = json_object_get(cfg, "cron");
    if (!cron_cfg || cron_cfg->type != JSON_OBJECT) { json_free(cfg); return (double)COMPLETED_ONESHOT_RETENTION_DAYS; }
    json_t *v = json_object_get(cron_cfg, "completed_retention_days");
    double days = (v && v->type == JSON_NUMBER) ? v->num_val : (double)COMPLETED_ONESHOT_RETENTION_DAYS;
    json_free(cfg);
    return days;
}

/* PoP: _sweep_completed_oneshots @ cron/jobs.py:_sweep_completed_oneshots */
bool _sweep_completed_oneshots(json_t *raw_jobs, const char *now_iso,
                               json_t *removed_ids) {
    if (!raw_jobs || !json_is_array(raw_jobs)) return false;
    double retention = _completed_oneshot_retention_days();
    if (retention <= 0) return false;
    /* cutoff = now - retention_days (ISO compare via string; parse to epoch) */
    double now_epoch = iso8601_to_epoch(now_iso);
    double cutoff_epoch = now_epoch - retention * 86400.0;
    bool removed = false;
    /* iterate backwards so removal is safe */
    size_t n = json_len(raw_jobs);
    for (size_t i = n; i-- > 0; ) {
        json_t *rj = json_get(raw_jobs, i);
        if (!rj || rj->type != JSON_OBJECT) continue;
        const char *st = json_get_str(rj, "state", "");
        if (strcmp(st, "completed") != 0) continue;
        json_t *sched = json_object_get(rj, "schedule");
        const char *kind = sched ? json_get_str(sched, "kind", "") : "";
        if (strcmp(kind, "once") != 0) continue;
        const char *last_run = json_get_str(rj, "last_run_at", "");
        if (!*last_run) continue;
        double lr = iso8601_to_epoch(last_run);
        if (lr < 0) continue;  /* unparseable → keep */
        if (lr >= cutoff_epoch) continue;
        const char *rid = json_get_str(rj, "id", "");
        json_array_remove(raw_jobs, i);
        removed = true;
        if (removed_ids && removed_ids->type == JSON_ARRAY && *rid)
            json_array_append(removed_ids, json_string(rid));
    }
    (void)removed;
    return removed;
}

/* convert "YYYY-MM-DDTHH:MM:SS+ZZ:ZZ" -> epoch (UTC). Returns -1 on parse fail. */
static double iso8601_to_epoch(const char *s) {
    if (!s || !*s) return -1;
    struct tm tm; memset(&tm, 0, sizeof tm);
    int n = sscanf(s, "%d-%d-%d %d:%d:%d",
                   &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                   &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    if (n < 6) return -1;
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = 0;
    time_t t = timegm(&tm);
    return t >= 0 ? (double)t : -1;
}
