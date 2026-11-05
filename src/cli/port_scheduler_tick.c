/*
 * port_scheduler_tick.c — pure helpers (see header).
 */

#include "port_scheduler_tick.h"
#include "hermes_json.h"

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
