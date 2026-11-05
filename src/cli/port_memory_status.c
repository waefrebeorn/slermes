/*
 * port_memory_status.c — Port of Python: gateway/memory_status.py (pure core).
 */

#include "port_memory_status.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* PoP: ms_nonneg_int @ gateway/memory_status.py:_nonneg_int */
long long ms_nonneg_int(const json_t *value)
{
    if (!value || value->type != JSON_NUMBER)
        return -1;
    double d = value->num_val;
    if (d < 0.0 || floor(d) != d)
        return -1;
    return (long long)d;
}

/* PoP: ms_mb @ gateway/memory_status.py:_mb */
long long ms_mb(const json_t *kib)
{
    long long v = ms_nonneg_int(kib);
    return v < 0 ? -1 : v / 1024;
}

/* days-since-1970-01-01 from a civil date (Howard Hinnant's algorithm). */
static long long days_from_civil(long long y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? (unsigned)-3u : 9u)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

/* PoP: ms_parse_iso_epoch @ gateway/memory_status.py:_parse_iso */
double ms_parse_iso_epoch(const char *value)
{
    /* "YYYY-MM-DD[T ]HH:MM[:SS[.ffffff]][+HH:MM|Z]" — the ISO subset the
     * heartbeat writer produces and Python fromisoformat() accepts. Returns
     * epoch-seconds as a UTC-aware time (naive input normalised to UTC). */
    if (!value || !value[0])
        return -1.0;
    int Y, Mo, D, h, mi, s = 0;
    unsigned long long frac = 0;
    int frac_digits = 0;
    long long offset_sec = 0;

    if (value[4] != '-' || value[7] != '-')
        return -1.0;
    Y = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
        (value[2] - '0') * 10 + (value[3] - '0');
    Mo = (value[5] - '0') * 10 + (value[6] - '0');
    D = (value[8] - '0') * 10 + (value[9] - '0');
    if (Y < 1970 || Mo < 1 || Mo > 12 || D < 1 || D > 31)
        return -1.0;
    char sep = value[10];
    if (sep == '\0' || sep == '\n') {
        /* date-only -> midnight (Python fromisoformat accepts "YYYY-MM-DD") */
        h = mi = s = 0;
        return (double)days_from_civil(Y, (unsigned)Mo, (unsigned)D) * 86400.0;
    }
    if (sep != 'T' && sep != ' ' && sep != 't')
        return -1.0;
    const char *q = value + 11;
    if (!(q[0] >= '0' && q[0] <= '9') || !(q[1] >= '0' && q[1] <= '9') ||
        q[2] != ':' || !(q[3] >= '0' && q[3] <= '9') ||
        !(q[4] >= '0' && q[4] <= '9'))
        return -1.0;
    h = (q[0] - '0') * 10 + (q[1] - '0');
    mi = (q[3] - '0') * 10 + (q[4] - '0');
    q += 5;
    if (h > 23 || mi > 59)
        return -1.0;
    if (q[0] == ':') {
        q++;
        if (!(q[0] >= '0' && q[0] <= '9') || !(q[1] >= '0' && q[1] <= '9'))
            return -1.0;
        s = (q[0] - '0') * 10 + (q[1] - '0');
        q += 2;
        if (s > 59)
            return -1.0;
        if (q[0] == '.') {
            q++;
            while (q[0] >= '0' && q[0] <= '9' && frac_digits < 18) {
                frac = frac * 10 + (unsigned long long)(q[0] - '0');
                frac_digits++;
                q++;
            }
        }
    }
    if (*q == 'Z' || *q == 'z') {
        q++;
    } else if (*q == '+' || *q == '-') {
        int sign = (*q == '-') ? -1 : 1;
        q++;
        if (!(q[0] >= '0' && q[0] <= '9' && q[1] >= '0' && q[1] <= '9' &&
              q[2] == ':' && q[3] >= '0' && q[3] <= '9' && q[4] >= '0' && q[4] <= '9'))
            return -1.0;
        int oh = (q[0] - '0') * 10 + (q[1] - '0');
        int om = (q[3] - '0') * 10 + (q[4] - '0');
        if (oh > 23 || om > 59)
            return -1.0;
        offset_sec = (long long)sign * (oh * 3600 + om * 60);
        q += 5;
    }
    if (*q != '\0' && *q != '\n')
        return -1.0;

    long long days = days_from_civil(Y, (unsigned)Mo, (unsigned)D);
    double epoch = (double)days * 86400.0 + (double)(h * 3600 + mi * 60 + s) -
                   (double)offset_sec;
    if (frac_digits > 0)
        epoch += (double)frac / pow(10.0, (double)frac_digits);
    return epoch;
}

/* PoP: ms_classify_pressure @ gateway/memory_status.py:classify_pressure */
char *ms_classify_pressure(const json_t *available_kib, const json_t *total_kib)
{
    long long available = ms_nonneg_int(available_kib);
    if (available < 0)
        return dup_("unknown");
    long long total = ms_nonneg_int(total_kib);
    /* Python: fraction = available / total if total else None */
    int have_total = total > 0;
    double fraction = have_total ? (double)available / (double)total : 0.0;

    if (available < MS_CRITICAL_AVAILABLE_KIB ||
        (have_total && fraction < MS_CRITICAL_AVAILABLE_FRACTION))
        return dup_("critical");
    if (available < MS_ELEVATED_AVAILABLE_KIB ||
        (have_total && fraction < MS_ELEVATED_AVAILABLE_FRACTION))
        return dup_("elevated");
    return dup_("ok");
}

static int json_truthy(const json_t *node)
{
    if (!node)
        return 0;
    switch (node->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return node->bool_val ? 1 : 0;
    case JSON_NUMBER: return node->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return node->str_val && node->str_val[0] ? 1 : 0;
    default: return node->c.count > 0 ? 1 : 0;
    }
}

/* Python forwards sampled_at through .isoformat() after normalising to UTC:
 * trailing Z -> "+00:00", a naive time gets "+00:00" appended, an explicit
 * ±HH:MM is kept. malloc'd. */
static char *sampled_at_iso(const char *input)
{
    if (!input || !input[0])
        return dup_("");
    size_t len = strlen(input);
    int ends_z = (input[len - 1] == 'Z' || input[len - 1] == 'z');
    int has_offset = 0;
    for (size_t i = 0; i + 5 < len; i++) {
        char c = input[i];
        /* an offset is "±HH:MM" — a sign TWO digits then a colon (the date's
         * dashes are "-m-m[T]" and must not count) */
        if ((c == '+' || c == '-') && input[i + 1] >= '0' && input[i + 1] <= '9' &&
            input[i + 2] >= '0' && input[i + 2] <= '9' && input[i + 3] == ':') {
            has_offset = 1;
            break;
        }
    }
    if (ends_z) {
        char *o = (char *)malloc(len + 6);
        if (!o) return NULL;
        memcpy(o, input, len - 1);
        strcpy(o + len - 1, "+00:00");
        return o;
    }
    if (has_offset)
        return dup_(input);
    char *o = (char *)malloc(len + 7);
    if (!o) return NULL;
    memcpy(o, input, len);
    strcpy(o + len, "+00:00");
    return o;
}

static void set_mb(json_t *status, const char *dst, const json_t *mem, const char *src)
{
    json_t *v = mem ? json_obj_get(mem, src) : NULL;
    long long mb = ms_mb(v);
    json_set(status, dst, mb < 0 ? json_null() : json_number((double)mb));
}

/* PoP: ms_collect_memory_status @ gateway/memory_status.py:collect_memory_status */
json_t *ms_collect_memory_status(const char *heartbeat_json,
                                 const char *sentinel_json,
                                 double now_seconds)
{
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

    json_t *heartbeat = heartbeat_json ? json_parse(heartbeat_json, NULL) : NULL;
    int hb_truthy = heartbeat && heartbeat->type == JSON_OBJECT &&
                    heartbeat->c.count > 0; /* Python `if heartbeat:` on a dict */
    if (hb_truthy) {
        const char *raw_sampled = json_get_str(heartbeat, "updated_at", NULL);
        double sampled_epoch = ms_parse_iso_epoch(raw_sampled);
        json_t *mem = json_obj_get(heartbeat, "mem");
        if (mem && mem->type == JSON_OBJECT) {
            set_mb(status, "gateway_rss_mb", mem, "rss_kib");
            set_mb(status, "system_total_mb", mem, "mem_total_kib");
            set_mb(status, "system_available_mb", mem, "mem_available_kib");
            set_mb(status, "swap_used_mb", mem, "swap_used_kib");
            /* Python gates sampled_at + pressure on isinstance(mem, dict) */
            if (sampled_epoch >= 0.0) {
                char *iso = sampled_at_iso(raw_sampled);
                json_set(status, "sampled_at", json_string(iso ? iso : ""));
                free(iso);
                double ago = now_seconds - sampled_epoch;
                if (ago >= 0.0 && ago <= MS_HEARTBEAT_FRESH_TTL_S) {
                    json_t *avail = json_obj_get(mem, "mem_available_kib");
                    json_t *tot = json_obj_get(mem, "mem_total_kib");
                    char *pr = ms_classify_pressure(avail, tot);
                    json_set(status, "pressure", json_string(pr ? pr : "unknown"));
                    free(pr);
                }
            }
        }
    }
    if (heartbeat)
        json_free(heartbeat);

    json_t *sentinel = sentinel_json ? json_parse(sentinel_json, NULL) : NULL;
    if (sentinel && sentinel->type == JSON_OBJECT && sentinel->c.count > 0) {
        json_set(status, "last_boot_unclean",
                 json_bool(json_truthy(json_obj_get(sentinel, "prior_unclean_exit"))));
        json_set(status, "last_boot_suspected_oom",
                 json_bool(json_truthy(json_obj_get(sentinel, "prior_suspected_oom"))));
        const char *started = json_get_str(sentinel, "started_at", NULL);
        json_set(status, "boot_id",
                 (started && started[0]) ? json_string(started) : json_null());
    }
    if (sentinel)
        json_free(sentinel);

    return status;
}
