/*
 * backup_cron_prompts.c — real C11 port of upstream Hermes
 *   hermes_cli/backup_cron_prompts.py pure helpers.
 * PoP: w18_backupcron_66b2ef_prompt_degraded @ hermes_cli/backup_cron_prompts.py:_prompt_degraded
 * PoP: w18_backupcron_66b2ef_cron_jobs_list @ hermes_cli/backup_cron_prompts.py:_cron_jobs_list
 * PoP: w18_backupcron_66b2ef_snapshot_prompts_by_id @ hermes_cli/backup_cron_prompts.py:_snapshot_prompts_by_id
 * PoP: w18_backupcron_66b2ef_restore_degraded_prompts @ hermes_cli/backup_cron_prompts.py:_restore_degraded_prompts
 */
#include "hermes_json.h"
#include <stdio.h>
#include <math.h>
#include "noc_w18_backupcron_66b2ef.h"

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *py_strip(const char *s) {
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && is_ws(*a)) a++;
    const char *b = s + strlen(s);
    while (b > a && is_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *r = (char *)malloc(n + 1);
    if (r) { memcpy(r, a, n); r[n] = '\0'; }
    return r;
}

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

/* str() coercion for JSON values; a missing key (NULL) maps to the "" default. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupstr("");
    switch (v->type) {
        case JSON_NULL: return dupstr("None");
        case JSON_BOOL: return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else
                snprintf(buf, sizeof(buf), "%g", v->num_val);
            return dupstr(buf);
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        default: return dupstr("");
    }
}

int w18_backupcron_66b2ef_prompt_degraded(const struct json_t *job) {
    if (!job || job->type != JSON_OBJECT) return 1;
    if (py_truthy(json_obj_get(job, "no_agent"))) return 0;
    const json_t *prompt = json_obj_get(job, "prompt");
    if (!prompt || prompt->type != JSON_STRING) return 1;
    char *ps = py_strip(prompt->str_val ? prompt->str_val : "");
    if (!*ps) { free(ps); return 1; }
    char *name = py_str(json_obj_get(job, "name"));
    char *ns = py_strip(name);
    int degraded = (strcmp(ps, ns) == 0);
    free(ps); free(ns); free(name);
    return degraded;
}

struct json_t *w18_backupcron_66b2ef_cron_jobs_list(const struct json_t *doc) {
    if (doc && doc->type == JSON_ARRAY) return json_copy(doc);
    if (doc && doc->type == JSON_OBJECT) {
        const json_t *jobs = json_obj_get(doc, "jobs");
        if (jobs && jobs->type == JSON_ARRAY) return json_copy(jobs);
    }
    return json_array();
}

struct json_t *w18_backupcron_66b2ef_snapshot_prompts_by_id(const struct json_t *snap_doc) {
    json_t *out = json_object();
    json_t *jobs = w18_backupcron_66b2ef_cron_jobs_list(snap_doc);
    for (size_t i = 0; i < jobs->c.count; i++) {
        const json_t *job = jobs->c.items[i];
        if (!job || job->type != JSON_OBJECT) continue;
        const json_t *prompt = json_obj_get(job, "prompt");
        if (!prompt || prompt->type != JSON_STRING) continue;
        char *ps = py_strip(prompt->str_val ? prompt->str_val : "");
        if (*ps) {
            char *id = py_str(json_obj_get(job, "id"));
            json_set(out, id, json_string(prompt->str_val ? prompt->str_val : ""));
            free(id);
        }
        free(ps);
    }
    json_free(jobs);
    return out;
}

struct json_t *w18_backupcron_66b2ef_restore_degraded_prompts(const struct json_t *jobs,
                                                              const struct json_t *snap_prompts,
                                                              int apply) {
    json_t *jc = (jobs && jobs->type == JSON_ARRAY) ? json_copy(jobs) : json_array();
    json_t *ids = json_array();
    for (size_t i = 0; i < jc->c.count; i++) {
        json_t *job = jc->c.items[i];
        if (!job || job->type != JSON_OBJECT) continue;
        if (!w18_backupcron_66b2ef_prompt_degraded(job)) continue;
        char *jid = py_str(json_obj_get(job, "id"));
        const json_t *sp = json_obj_get(snap_prompts, jid);
        if (!sp || sp->type == JSON_NULL) { free(jid); continue; }
        if (apply) json_set(job, "prompt", json_copy(sp));
        json_append(ids, json_string(jid));
        free(jid);
    }
    json_t *result = json_object();
    json_set(result, "ids", ids);
    json_set(result, "jobs", jc);
    return result;
}
