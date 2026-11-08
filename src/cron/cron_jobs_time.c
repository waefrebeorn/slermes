/*
 * cron_jobs_time.c — ISO8601 / wall-clock helpers.
 *
 * Split from port_cron_jobs.c (soul.md: self-contained modules). Pure
 * time parsing/conversion; no file-scope state.
 */
#include "cron_jobs_time.h"
#include "libdatetime/datetime.h"
#include <stdio.h>
#include <string.h>

time_t iso_to_ts(const char *iso) {
    if (!iso || !*iso) return (time_t)-1;
    return datetime_parse_iso8601(iso);
}

int iso_utc_offset(const char *iso) {
    if (!iso) return INT_MIN;
    size_t n = strlen(iso);
    /* look at the tail after the time portion */
    for (size_t i = 10; i < n; i++) {
        if (iso[i] == 'Z' || iso[i] == 'z') return 0;
        if ((iso[i] == '+' || iso[i] == '-') && i >= 12) {
            int h = 0, m = 0;
            if (sscanf(iso + i + 1, "%d:%d", &h, &m) >= 1) {
                int off = h * 3600 + m * 60;
                return iso[i] == '-' ? -off : off;
            }
        }
    }
    return INT_MIN; /* naive */
}

time_t iso_wall_clock(const char *iso) {
    struct tm tm; memset(&tm, 0, sizeof(tm));
    char sep;
    if (sscanf(iso, "%d-%d-%d%c%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &sep, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) < 3)
        return (time_t)-1;
    tm.tm_year -= 1900; tm.tm_mon -= 1; tm.tm_isdst = -1;
    return mktime(&tm);
}
