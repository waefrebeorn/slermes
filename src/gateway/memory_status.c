/*
 * memory_status.c — C11 port of gateway/memory_status.py (NS-656).
 *
 * Read side of the gateway memory-pressure story: the heartbeat written by
 * gateway.shutdown_watchdog and the lifecycle sentinel written by
 * gateway.lifecycle_ledger are distilled into the public "memory" block for
 * /api/status. No new sampling, no IPC — two small file reads. Everything is
 * best-effort: a missing/corrupt file degrades to pressure="unknown".
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "memory_status.h"
#include "hermes_json.h"
#include "lifecycle_ledger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define MEM_CRITICAL_AVAILABLE_KIB     (64LL * 1024)   /* < 64 MiB available */
#define MEM_CRITICAL_AVAILABLE_FRACTION 0.05           /* < 5% of MemTotal */
#define MEM_ELEVATED_AVAILABLE_KIB     (128LL * 1024)  /* < 128 MiB available */
#define MEM_ELEVATED_AVAILABLE_FRACTION 0.15           /* < 15% of MemTotal */
#define MEM_HEARTBEAT_FRESH_TTL_S      150.0
#define MEM_KIB_PER_MB                 1024LL

/* PoP: _mb @ gateway/memory_status.py:_mb */
int _mb(const json_t *kib, long long *out)
{
    if (!kib || kib->type != JSON_NUMBER)
        return 0;
    double v = kib->num_val;
    if (v < 0.0 || v != floor(v))
        return 0;
    if (out)
        *out = (long long)v / MEM_KIB_PER_MB;
    return 1;
}

/* PoP: _parse_iso @ gateway/memory_status.py:_parse_iso */
int _parse_iso(const char *value, long long *epoch_s, int *usec)
{
    if (!value || !*value)
        return 0;
    int y = 0, mo = 0, d = 0, hh = 0, mm = 0;
    double sec = 0.0;
    int n = sscanf(value, "%d-%d-%dT%d:%d:%lf", &y, &mo, &d, &hh, &mm, &sec);
    int has_time = 0;
    if (n >= 3) {
        has_time = (n >= 6);
        if (!has_time) { sec = 0.0; }
    } else {
        return 0;
    }
    const char *p = value;
    /* advance past the time portion to the timezone suffix */
    while (*p && *p != 'Z' && *p != '+' && *p != '-'
           && !(*p >= '0' && *p <= '9') && *p != '.')
        p++;
    /* skip the date/time digits themselves */
    while (*p && ((*p >= '0' && *p <= '9') || *p == '-' || *p == 'T'
                  || *p == ':' || *p == '.'))
        p++;
    long tz_offset_s = 0;
    if (*p == 'Z' || *p == 'z') {
        tz_offset_s = 0;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int th = 0, tm = 0;
        int got = sscanf(p, "%2d:%2d", &th, &tm);
        if (got < 1)
            return 0;
        if (got == 1)
            tm = 0;
        tz_offset_s = sign * ((long)th * 3600 + (long)tm * 60);
    }
    /* else: naive — Python replaces tzinfo with UTC */
    double whole = floor(sec);
    int us = (int)llround((sec - whole) * 1e6);
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = y - 1900;
    tmv.tm_mon = mo - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = hh;
    tmv.tm_min = mm;
    tmv.tm_sec = (int)whole;
    time_t e = timegm(&tmv);
    if (e == (time_t)-1)
        return 0;
    if (epoch_s)
        *epoch_s = (long long)e - tz_offset_s;
    if (usec)
        *usec = us;
    return 1;
}

/* PoP: classify_pressure @ gateway/memory_status.py:classify_pressure */
const char *classify_pressure(const json_t *available_kib, const json_t *total_kib)
{
    long long avail = 0;
    /* Python guard: int (not bool), >= 0. */
    if (!available_kib || available_kib->type != JSON_NUMBER
        || available_kib->num_val < 0.0
        || available_kib->num_val != floor(available_kib->num_val))
        return "unknown";
    avail = (long long)available_kib->num_val;
    double fraction = -1.0;
    if (total_kib && total_kib->type == JSON_NUMBER
        && total_kib->num_val == floor(total_kib->num_val)
        && total_kib->num_val > 0.0)
        fraction = available_kib->num_val / total_kib->num_val;
    if (avail < MEM_CRITICAL_AVAILABLE_KIB
        || (fraction >= 0.0 && fraction < MEM_CRITICAL_AVAILABLE_FRACTION))
        return "critical";
    if (avail < MEM_ELEVATED_AVAILABLE_KIB
        || (fraction >= 0.0 && fraction < MEM_ELEVATED_AVAILABLE_FRACTION))
        return "elevated";
    return "ok";
}

/* Process HERMES_HOME fallback (mirrors _process_hermes_home). */
static char *mem_status_home(const char *home)
{
    if (home && *home)
        return strdup(home);
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh)
        return strdup(hh);
    const char *h = getenv("HOME");
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s/.hermes", (h && *h) ? h : ".");
    return strdup(buf);
}

/* PoP: _read_heartbeat @ gateway/memory_status.py:_read_heartbeat */
json_t *_read_heartbeat(const char *home)
{
    /* get_loop_heartbeat_path: <home>/state/gateway.heartbeat */
    char *base = mem_status_home(home);
    if (!base)
        return NULL;
    char path[8192];
    snprintf(path, sizeof(path), "%s/state/gateway.heartbeat", base);
    free(base);
    return llg_read_json(path);
}

/* PoP: _read_sentinel @ gateway/memory_status.py:_read_sentinel */
json_t *_read_sentinel(const char *home)
{
    char *path = llg_get_lifecycle_sentinel_path(home);
    if (!path)
        return NULL;
    json_t *out = llg_read_json(path);
    free(path);
    return out;
}

/* Python bool(x) over a JSON value. */
static int mem_json_truthy(const json_t *v)
{
    if (!v || v->type == JSON_NULL)
        return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return v->c.count > 0;
    default:          return 0;
    }
}

/* PoP: collect_memory_status @ gateway/memory_status.py:collect_memory_status */
json_t *collect_memory_status(const char *home, double now_epoch_s)
{
    struct timespec ts;
    double moment = now_epoch_s;
    if (moment == 0.0) {
        clock_gettime(CLOCK_REALTIME, &ts);
        moment = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }

    json_t *status = json_object();
    json_set(status, "pressure", json_string("unknown"));
    json_set(status, "gateway_rss_mb", json_null());
    json_set(status, "system_total_mb", json_null());
    json_set(status, "system_available_mb", json_null());
    json_set(status, "swap_used_mb", json_null());
    json_set(status, "sampled_at", json_null());
    json_set(status, "last_boot_unclean", json_bool(false));
    json_set(status, "last_boot_suspected_oom", json_bool(false));
    json_set(status, "boot_id", json_null());

    json_t *heartbeat = _read_heartbeat(home);
    if (heartbeat) {
        json_t *updated = json_obj_get(heartbeat, "updated_at");
        long long epoch = 0;
        int usec = 0;
        int have_sampled = (updated && updated->type == JSON_STRING
                            && _parse_iso(updated->str_val, &epoch, &usec));
        json_t *mem = json_obj_get(heartbeat, "mem");
        if (mem && mem->type == JSON_OBJECT) {
            long long v;
            if (_mb(json_obj_get(mem, "rss_kib"), &v))
                json_set(status, "gateway_rss_mb", json_number((double)v));
            if (_mb(json_obj_get(mem, "mem_total_kib"), &v))
                json_set(status, "system_total_mb", json_number((double)v));
            if (_mb(json_obj_get(mem, "mem_available_kib"), &v))
                json_set(status, "system_available_mb", json_number((double)v));
            if (_mb(json_obj_get(mem, "swap_used_kib"), &v))
                json_set(status, "swap_used_mb", json_number((double)v));
            if (have_sampled) {
                char iso[128];
                /* re-emit via gmtime of the parsed epoch (UTC-normalized) */
                time_t e = (time_t)epoch;
                struct tm g;
                gmtime_r(&e, &g);
                if (usec)
                    snprintf(iso, sizeof(iso),
                             "%04d-%02d-%02dT%02d:%02d:%02d.%06d+00:00",
                             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
                             g.tm_hour, g.tm_min, g.tm_sec, usec);
                else
                    snprintf(iso, sizeof(iso),
                             "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
                             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
                             g.tm_hour, g.tm_min, g.tm_sec);
                json_set(status, "sampled_at", json_string(iso));
                double age_s = moment - ((double)epoch + (double)usec / 1e6);
                if (age_s >= 0.0 && age_s <= MEM_HEARTBEAT_FRESH_TTL_S) {
                    json_set(status, "pressure", json_string(
                        classify_pressure(json_obj_get(mem, "mem_available_kib"),
                                          json_obj_get(mem, "mem_total_kib"))));
                }
                /* else: stale sample — numbers reported, pressure stays unknown */
            }
        }
        json_free(heartbeat);
    }

    json_t *sentinel = _read_sentinel(home);
    if (sentinel) {
        json_set(status, "last_boot_unclean", json_bool(
            mem_json_truthy(json_obj_get(sentinel, "prior_unclean_exit"))));
        json_set(status, "last_boot_suspected_oom", json_bool(
            mem_json_truthy(json_obj_get(sentinel, "prior_suspected_oom"))));
        json_t *started = json_obj_get(sentinel, "started_at");
        if (started && started->type == JSON_STRING && started->str_val
            && started->str_val[0])
            json_set(status, "boot_id", json_string(started->str_val));
        json_free(sentinel);
    }
    return status;
}
