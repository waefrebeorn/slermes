/*
 * session_cron.c — faithful C11 port of upstream Hermes gateway/session_cron.py.
 *
 * PoP: w7_sessioncron_e77eee_scheduler_execution_id @ gateway/session_cron.py:scheduler_execution_id
 * PoP: w7_sessioncron_e77eee_validate_create_params @ gateway/session_cron.py:_create
 * PoP: w7_sessioncron_e77eee_no_owner_message @ gateway/session_cron.py:deliver_to_local_chat
 * PoP: w7_sessioncron_e77eee_run_job_flags @ gateway/session_cron.py:execute
 */
#include "hermes_json.h"
#include "noc_w7_sessioncron_e77eee.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CRON_REQUEST_PREFIX = "cron:" (persistence/authority/bindings.py). */
#define SC_CRON_REQUEST_PREFIX "cron:"
/* RUN_JOB_FLAGS = ('_model_unreachable', '_quota_hold_seconds'). */
static const char *const SC_RUN_JOB_FLAGS[] = {"_model_unreachable", "_quota_hold_seconds"};
#define SC_RUN_JOB_FLAGS_COUNT 2

struct json_t *w7_sessioncron_e77eee_scheduler_execution_id(const char *job_id,
                                                             const char *request_id) {
    const char *rid = request_id ? request_id : "";
    size_t prefix_len = strlen(SC_CRON_REQUEST_PREFIX) + strlen(job_id ? job_id : "") + 1;
    char *prefix = (char *)malloc(prefix_len + 1);
    if (!prefix) return json_string(rid);
    snprintf(prefix, prefix_len + 1, "%s%s:", SC_CRON_REQUEST_PREFIX, job_id ? job_id : "");
    size_t n = strlen(prefix);
    struct json_t *r;
    if (strncmp(rid, prefix, n) == 0)
        r = json_string(rid + n);          /* str.removeprefix */
    else
        r = json_string(rid);
    free(prefix);
    return r;
}

int w7_sessioncron_e77eee_validate_create_params(const struct json_t *params) {
    if (!params || params->type != JSON_OBJECT) return 0;
    if (json_object_size(params) != 3) return 0;   /* set(params) == {...} */
    struct json_t *jid = json_obj_get((struct json_t *)params, "job_id");
    struct json_t *rid = json_obj_get((struct json_t *)params, "request_id");
    struct json_t *extra = json_obj_get((struct json_t *)params, "extra_prompt");
    if (!jid || !rid || !extra) return 0;
    if (jid->type != JSON_STRING || !jid->str_val || !*jid->str_val) return 0;
    if (rid->type != JSON_STRING || !rid->str_val || !*rid->str_val) return 0;
    if (strlen(rid->str_val) > 256) return 0;
    if (extra->type != JSON_NULL && extra->type != JSON_STRING) return 0;
    return 1;
}

struct json_t *w7_sessioncron_e77eee_no_owner_message(const char *chat_id) {
    size_t n = strlen(chat_id ? chat_id : "") +
               strlen(": no gateway in this process owns its session") + 16;
    char *buf = (char *)malloc(n);
    if (!buf) return json_string("");
    snprintf(buf, n, "local chat %s: no gateway in this process owns its session",
             chat_id ? chat_id : "");
    struct json_t *r = json_string(buf);
    free(buf);
    return r;
}

struct json_t *w7_sessioncron_e77eee_run_job_flags(const struct json_t *job) {
    struct json_t *flags = json_object();
    if (!job || job->type != JSON_OBJECT) return flags;
    for (size_t i = 0; i < SC_RUN_JOB_FLAGS_COUNT; i++) {
        const char *key = SC_RUN_JOB_FLAGS[i];
        struct json_t *v = json_obj_get((struct json_t *)job, key);
        if (v) json_set(flags, key, json_copy(v));
    }
    return flags;
}
