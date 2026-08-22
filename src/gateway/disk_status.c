/*
 * disk_status.c — C11 port of gateway/disk_status.py (NS-656).
 *
 * Disk-usage rollup for /api/status. Companion to memory_status.c: a hosted
 * agent can fill its data volume completely while the dashboard looks
 * healthy. Sampled live via statvfs (the same thing shutil.disk_usage does).
 * Everything is best-effort and read-only; never raises.
 */
#define _POSIX_C_SOURCE 200809L
#include "disk_status.h"
#include "hermes_json.h"

#include <sys/statvfs.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Disk-pressure thresholds (gateway/disk_status.py module constants). */
#define DISK_CRITICAL_FREE_MB      256    /* < 256 MB free: critical on any volume */
#define DISK_CRITICAL_PERCENT      95.0   /* >= 95% used AND < 1 GB free */
#define DISK_CRITICAL_HEADROOM_MB  1024
#define DISK_ELEVATED_FREE_MB      512    /* < 512 MB free: elevated on any volume */
#define DISK_ELEVATED_PERCENT      85.0   /* >= 85% used AND < 4 GB free */
#define DISK_ELEVATED_HEADROOM_MB  4096
#define DISK_BYTES_PER_MB          (1024ULL * 1024ULL)

/* PoP: _coerce_mb @ gateway/disk_status.py:_coerce_mb */
int _coerce_mb(const json_t *value, long long *out)
{
    if (!value)
        return 0;
    /* Python: reject bool, accept int only (floats rejected even when
     * integral), reject negatives. */
    if (value->type != JSON_NUMBER)
        return 0;
    double v = value->num_val;
    if (v < 0.0)
        return 0;
    if (v != floor(v))
        return 0;
    if (out)
        *out = (long long)v;
    return 1;
}

/* PoP: classify_disk_pressure @ gateway/disk_status.py:classify_disk_pressure */
const char *classify_disk_pressure(const json_t *free_mb, const json_t *total_mb)
{
    long long free_v = 0, total_v = 0;
    if (!_coerce_mb(free_mb, &free_v) || !_coerce_mb(total_mb, &total_v)
        || total_v <= 0)
        return "unknown";
    double used_percent = (1.0 - (double)free_v / (double)total_v) * 100.0;
    if (free_v < DISK_CRITICAL_FREE_MB
        || (used_percent >= DISK_CRITICAL_PERCENT
            && free_v < DISK_CRITICAL_HEADROOM_MB))
        return "critical";
    if (free_v < DISK_ELEVATED_FREE_MB
        || (used_percent >= DISK_ELEVATED_PERCENT
            && free_v < DISK_ELEVATED_HEADROOM_MB))
        return "elevated";
    return "ok";
}

/* Test seam: build the status block from an injected statvfs-style sample.
 * Mirrors collect_disk_status's post-sample logic exactly. */
json_t *disk_status_collect_from_usage(unsigned long long total_bytes,
                                       unsigned long long free_bytes,
                                       unsigned long long used_bytes)
{
    json_t *status = json_object();
    json_set(status, "pressure", json_string("unknown"));
    json_set(status, "total_mb", json_null());
    json_set(status, "free_mb", json_null());
    json_set(status, "used_percent", json_null());
    if (total_bytes == 0)
        return status;  /* Python: usage.total <= 0 -> unknown block */
    long long total_mb = (long long)(total_bytes / DISK_BYTES_PER_MB);
    long long free_mb = (long long)(free_bytes / DISK_BYTES_PER_MB);
    /* Python: round((usage.used / usage.total) * 100, 1) — round-half-even.
     * nearbyint under the default FP rounding mode is round-half-even. */
    double pct = ((double)used_bytes / (double)total_bytes) * 100.0;
    double rounded = nearbyint(pct * 10.0) / 10.0;
    json_set(status, "total_mb", json_number((double)total_mb));
    json_set(status, "free_mb", json_number((double)free_mb));
    json_set(status, "used_percent", json_number(rounded));
    json_t *f = json_number((double)free_mb);
    json_t *t = json_number((double)total_mb);
    json_set(status, "pressure",
             json_string(classify_disk_pressure(f, t)));
    json_free(f);
    json_free(t);
    return status;
}

/* PoP: collect_disk_status @ gateway/disk_status.py:collect_disk_status */
json_t *collect_disk_status(const char *home)
{
    const char *target = home;
    char fallback[4096];
    if (!target || !*target) {
        const char *hh = getenv("HERMES_HOME");
        if (hh && *hh) {
            target = hh;
        } else {
            const char *h = getenv("HOME");
            snprintf(fallback, sizeof(fallback), "%s/.hermes",
                     (h && *h) ? h : ".");
            target = fallback;
        }
    }
    struct statvfs vfs;
    if (statvfs(target, &vfs) != 0)
        return disk_status_collect_from_usage(0, 0, 0);
    unsigned long long total =
        (unsigned long long)vfs.f_blocks * (unsigned long long)vfs.f_frsize;
    /* shutil.disk_usage: free = f_bavail * f_frsize (available to this uid),
     * used = total - free. */
    unsigned long long free_b =
        (unsigned long long)vfs.f_bavail * (unsigned long long)vfs.f_frsize;
    unsigned long long used = total > free_b ? total - free_b : 0;
    return disk_status_collect_from_usage(total, free_b, used);
}
