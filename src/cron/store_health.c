/*
 * store_health.c — C home for upstream Hermes cron/store_health.py.
 *
 * PoP: w6_storehealth_0f6948_probe_interval @ cron/store_health.py:PROBE_INTERVAL_SECONDS
 * PoP: w6_storehealth_0f6948_fix_hint @ cron/store_health.py:FIX_HINT
 * PoP: w6_storehealth_0f6948_describe_error @ cron/store_health.py:describe_error
 * PoP: w6_storehealth_0f6948_unwritable_errno @ cron/store_health.py:UNWRITABLE_ERRNOS
 * PoP: w6_storehealth_0f6948_outage_covers @ cron/store_health.py:outage_covers
 * PoP: w6_storehealth_0f6948_run_keys @ cron/store_health.py:_run_keys
 * PoP: w6_storehealth_0f6948_skipped_runs @ cron/store_health.py:StoreDegraded.skipped_runs
 *
 * Only the pure decision helpers are ported; filesystem probing, the global
 * degraded registry and the listener/thread machinery stay in Python.
 */
#include "noc_w6_storehealth_0f6948.h"
#include "hermes_json.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W6_PROBE_INTERVAL 60.0
#define W6_FIX_HINT "free disk space, remount it read-write, or fix permissions on {store}"
/* Windows Python maps errno.EDQUOT to WSAEDQUOT (10069); MinGW has no EDQUOT. */
#define W6_EDQUOT 10069

double w6_storehealth_0f6948_probe_interval(void) { return W6_PROBE_INTERVAL; }

char *w6_storehealth_0f6948_fix_hint(const char *store)
{
    const char *key = "{store}";
    const char *s = store ? store : "";
    const char *at = strstr(W6_FIX_HINT, key);
    size_t before = (size_t)(at - W6_FIX_HINT);
    size_t n = before + strlen(s) + strlen(key + 7);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, W6_FIX_HINT, before);
    memcpy(out + before, s, strlen(s));
    strcpy(out + before + strlen(s), key + 7);
    return out;
}

static const char *errno_code(int e)
{
    switch (e) {
    case EPERM:   return "EPERM";
    case ENOENT:  return "ENOENT";
    case EINTR:   return "EINTR";
    case EIO:     return "EIO";
    case EEXIST:  return "EEXIST";
    case EINVAL:  return "EINVAL";
    case EACCES:  return "EACCES";
    case ENOSPC:  return "ENOSPC";
    case EROFS:   return "EROFS";
    case W6_EDQUOT: return "WSAEDQUOT";
    default:      return "";
    }
}

char *w6_storehealth_0f6948_describe_error(int errnum, const char *text)
{
    const char *code = errno_code(errnum);
    const char *t = (text && *text) ? text : "";
    size_t n = strlen(code) + (code[0] ? 2 : 0) + strlen(t);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (code[0]) snprintf(out, n + 1, "%s: %s", code, t);
    else { memcpy(out, t, strlen(t)); out[strlen(t)] = '\0'; }
    return out;
}

int w6_storehealth_0f6948_unwritable_errno(int errnum)
{
    return errnum == EROFS || errnum == EACCES || errnum == EPERM ||
           errnum == ENOSPC || errnum == W6_EDQUOT;
}

int w6_storehealth_0f6948_outage_covers(const struct json_t *records,
                                        double due_at, double grace)
{
    if (!records || records->type != JSON_ARRAY) return 0;
    size_t n = json_len(records);
    double limit = due_at + grace;
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_get(records, i);
        if (!r || r->type != JSON_OBJECT) continue;
        json_t *sv = json_obj_get(r, "since");
        if (!sv || sv->type != JSON_NUMBER) continue;
        int recovered = 0;
        double recovered_at = 0;
        json_t *rv = json_obj_get(r, "recovered_at");
        if (rv && rv->type == JSON_NUMBER) { recovered = 1; recovered_at = rv->num_val; }
        if (sv->num_val <= limit && (!recovered || due_at <= recovered_at)) return 1;
    }
    return 0;
}

struct json_t *w6_storehealth_0f6948_run_keys(const struct json_t *jobs)
{
    struct json_t *out = json_array();
    if (!jobs || jobs->type != JSON_ARRAY) return out;
    size_t n = json_len(jobs);
    for (size_t i = 0; i < n; i++) {
        json_t *job = json_get(jobs, i);
        if (!job || job->type != JSON_OBJECT) continue;
        json_t *idv = json_obj_get(job, "id");
        json_t *nv = json_obj_get(job, "next_run_at");
        const char *id = (idv && idv->type == JSON_STRING) ? idv->str_val : "";
        const char *nx = (nv && nv->type == JSON_STRING) ? nv->str_val : "";
        int dup = 0;
        for (size_t j = 0; j < json_len(out); j++) {
            json_t *p = json_get(out, j);
            json_t *a = json_get(p, 0), *b = json_get(p, 1);
            if (a && b && !strcmp(a->str_val ? a->str_val : "", id) &&
                !strcmp(b->str_val ? b->str_val : "", nx)) { dup = 1; break; }
        }
        if (dup) continue;
        struct json_t *pair = json_array();
        json_append(pair, json_string(id));
        json_append(pair, json_string(nx));
        json_append(out, pair);
    }
    /* insertion sort by (id, next_run_at) — Python set order is arbitrary */
    for (size_t i = 1; i < json_len(out); i++) {
        size_t j = i;
        while (j > 0) {
            json_t *a = json_get(out, j - 1), *b = json_get(out, j);
            const char *a0 = json_get(a, 0)->str_val, *b0 = json_get(b, 0)->str_val;
            const char *a1 = json_get(a, 1)->str_val, *b1 = json_get(b, 1)->str_val;
            int cmp = strcmp(a0 ? a0 : "", b0 ? b0 : "");
            if (cmp == 0) cmp = strcmp(a1 ? a1 : "", b1 ? b1 : "");
            if (cmp <= 0) break;
            json_t *tmp = out->c.items[j - 1];
            out->c.items[j - 1] = out->c.items[j];
            out->c.items[j] = tmp;
            j--;
        }
    }
    return out;
}

int w6_storehealth_0f6948_skipped_runs(const struct json_t *skipped)
{
    if (!skipped || skipped->type != JSON_ARRAY) return 0;
    size_t n = json_len(skipped), count = 0;
    char **seen = n ? (char **)calloc(n, sizeof(char *)) : NULL;
    if (!seen) return 0;
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(skipped, i);
        char *s = json_dumps(v, 0);
        if (!s) { s = (char *)malloc(1); if (s) s[0] = '\0'; }
        int dup = 0;
        for (size_t j = 0; j < count; j++)
            if (s && seen[j] && !strcmp(s, seen[j])) { dup = 1; break; }
        if (!dup) seen[count++] = s;
        else free(s);
    }
    for (size_t j = 0; j < count; j++) free(seen[j]);
    free(seen);
    return (int)count;
}

