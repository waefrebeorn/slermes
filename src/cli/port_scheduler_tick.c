/*
 * port_scheduler_tick.c — pure helpers (see header).
 */

#include "port_scheduler_tick.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int js_job_recurring(const json_t *job)
{
    const json_t *sched, *kind;
    if (!job || job->type != JSON_OBJECT) return 0;
    sched = json_obj_get(job, "schedule");
    if (!sched || sched->type != JSON_OBJECT) return 0;
    kind = json_obj_get(sched, "kind");
    if (!kind || kind->type != JSON_STRING || !kind->str_val) return 0;
    return strcmp(kind->str_val, "cron") == 0 ||
           strcmp(kind->str_val, "interval") == 0;
}

int js_is_recurring(const char *job_json)
{
    json_t *job = job_json ? json_parse(job_json, NULL) : NULL;
    int r = js_job_recurring(job);
    if (job) json_free(job);
    return r;
}

/* _log_tick_yield_once decision + message. The Python body reads the module
 * global _last_yield_log for last_reason/last_at and time.monotonic() for now;
 * those are passed in here (the caller keeps the once-per-episode state). */
#define JS_YIELD_LOG_INTERVAL_SECONDS 3600.0

int js_tick_yield_should_log(const char *last_reason, double last_at,
                             const char *reason, double now)
{
    /* if last_reason != reason or (now - float(last_at)) >= _YIELD_LOG_INTERVAL */
    if (strcmp(last_reason ? last_reason : "", reason ? reason : "") != 0)
        return 1;
    return (now - last_at) >= JS_YIELD_LOG_INTERVAL_SECONDS;
}

/* PoP: _log_tick_yield_once @ cron/scheduler_tick.py:_log_tick_yield_once */
char *js_tick_yield_message(const char *reason)
{
    const char *r = reason ? reason : "";
    size_t n =
        strlen("Cron tick yielded: this process is running stale code (") +
        strlen(r) +
        strlen(") and a fresher gateway owns the runtime lock \xe2\x80\x94 jobs "
               "will fire from that process. Restart this one to reclaim its ticks.");
    char *out = malloc(n + 1);
    if (!out) return NULL;
    sprintf(out,
            "Cron tick yielded: this process is running stale code (%s) and a "
            "fresher gateway owns the runtime lock \xe2\x80\x94 jobs will fire "
            "from that process. Restart this one to reclaim its ticks.", r);
    return out;
}

char *js_advance_or_drop(const char *due_jobs_json, int advance_ok)
{
    json_t *arr = due_jobs_json ? json_parse(due_jobs_json, NULL) : NULL;
    char *ser;
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return strdup("[]");
    }
    if (advance_ok) {
        ser = json_serialize(arr);
        json_free(arr);
        return ser;
    }
    {
        /* [j for j in due_jobs if not is_recurring(j)] */
        json_t *out = json_array();
        size_t i;
        for (i = 0; i < arr->c.count; i++) {
            const json_t *job = arr->c.items[i];
            if (!js_job_recurring(job)) {
                char *s = json_serialize(job);
                json_t *copy = s ? json_parse(s, NULL) : NULL;
                if (copy) json_array_append(out, copy);
                free(s);
            }
        }
        ser = json_serialize(out);
        json_free(out);
        json_free(arr);
        return ser;
    }
}
