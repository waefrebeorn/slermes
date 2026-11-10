/*
 * cron_jobs_time2.c - cron wall-clock / ticker-age helpers.
 *
 * Split from port_cron_jobs.c (soul.md: self-contained modules). ISO
 * timestamps, epoch file ages and wall-clock consistency checks. No
 * file-scope state.
 */
#include "cron_jobs_time2.h"
#include "cron_jobs.h"
#include "cron_jobs_time.h"
#include "hermes_strutil.h"
#include "hermes_fs.h"
#include "slermes_home.h"
#include "datetime.h"
#include "cron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <limits.h>
#include "uuid.h"
#include "cron_jobs_paths.h"   /* ticker file paths */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>



/* ── ISO helpers (local wall-clock with numeric offset, like Python) ── */

/* Format a time_t as local ISO8601 with numeric offset: 2026-07-16T04:16:17+02:00.
 * Matches Python datetime.isoformat() on an aware local datetime. */ char *iso_local(time_t ts) {
    struct tm lt;
    if (!localtime_r(&ts, &lt)) return NULL;
    char base[32];
    if (strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", &lt) == 0) return NULL;
    long off = hermes_tm_gmtoff(&lt);               /* seconds east of UTC */
    char sign = off < 0 ? '-' : '+';
    long ao = off < 0 ? -off : off;
    int oh = (int)(ao / 3600), om = (int)((ao % 3600) / 60);
    char *out = malloc(40);
    if (!out) return NULL;
    snprintf(out, 40, "%s%c%02d:%02d", base, sign, oh, om);
    return out;
}


/* PoP: now_iso @ tools/skill_usage.py:_now_iso */
/* Current time ISO (local, aware). Mirrors _hermes_now().isoformat(). */
char *now_iso(void) { return iso_local(time(NULL)); }


/* PoP: cronjobs_recoverable_oneshot_run_at @ cron/jobs.py:_recoverable_oneshot_run_at */ char *recoverable_oneshot_run_at(const json_t *schedule, time_t now,
                                        const char *last_run_at) {
    const char *kind = json_object_get_string(schedule, "kind", "");
    if (strcmp(kind, "once") != 0) return NULL;
    if (last_run_at && *last_run_at) return NULL;
    const char *run_at = json_object_get_string(schedule, "run_at", NULL);
    if (!run_at || !*run_at) return NULL;
    time_t run_ts = iso_to_ts(run_at);
    if (run_ts == (time_t)-1) return NULL;
    if (run_ts >= now - CRONJOBS_ONESHOT_GRACE_SECONDS)
        return xstrdup(run_at);
    return NULL;
}


/* ── Ticker heartbeat ─────────────────────────────────────────────── */

/* PoP: cronjobs_atomic_write_epoch @ cron/jobs.py:_atomic_write_epoch */
static void atomic_write_epoch(const char *path) {
    cronjobs_ensure_dirs();
    char *dir = cronjobs_cron_dir();
    if (!dir) return;
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/.hb_XXXXXX", dir);
    free(dir);
    int fd = mkstemp(tmp);
    if (fd < 0) return;
    char buf[64];
    int L = snprintf(buf, sizeof(buf), "%ld", (long)time(NULL));
    if (write(fd, buf, (size_t)L) == L) {
        fsync(fd);
        close(fd);
        if (rename(tmp, path) != 0) unlink(tmp);
    } else {
        close(fd);
        unlink(tmp);
    }
}


/* PoP: cronjobs_record_ticker_heartbeat @ cron/jobs.py:record_ticker_heartbeat */
void cronjobs_record_ticker_heartbeat(bool success) {
    char *hb = cronjobs_ticker_heartbeat_file();
    if (hb) { atomic_write_epoch(hb); free(hb); }
    if (success) {
        char *sc = cronjobs_ticker_success_file();
        if (sc) { atomic_write_epoch(sc); free(sc); }
    }
}


/* PoP: cronjobs_epoch_file_age @ cron/jobs.py:_epoch_file_age */
static double epoch_file_age(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1.0;
    char buf[64];
    if (!fgets(buf, sizeof(buf), f)) { fclose(f); return -1.0; }
    fclose(f);
    char *stripped = str_strip_dup(buf);
    if (!stripped || !*stripped) { free(stripped); return -1.0; }
    char *endp = NULL;
    double stamp = strtod(stripped, &endp);
    bool ok = (endp && endp != stripped);
    free(stripped);
    if (!ok) return -1.0;
    double age = (double)time(NULL) - stamp;
    return age < 0.0 ? 0.0 : age;
}


/* PoP: cronjobs_ticker_heartbeat_age @ cron/jobs.py:get_ticker_heartbeat_age */
double cronjobs_ticker_heartbeat_age(void) {
    char *hb = cronjobs_ticker_heartbeat_file();
    if (!hb) return -1.0;
    double a = epoch_file_age(hb);
    free(hb);
    return a;
}


/* PoP: cronjobs_ticker_success_age @ cron/jobs.py:get_ticker_success_age */
double cronjobs_ticker_success_age(void) {
    char *sc = cronjobs_ticker_success_file();
    if (!sc) return -1.0;
    double a = epoch_file_age(sc);
    free(sc);
    return a;
}


/* ── Timezone helpers for due-check (mirror _timezone_offset_mismatch etc.) ── */

/* Extract the numeric UTC offset (seconds) from an ISO string's trailing
 * +HH:MM / -HH:MM / Z. Returns INT_MIN when the timestamp is naive (no tz). */

/* PoP: cronjobs_timezone_offset_mismatch @ cron/jobs.py:_timezone_offset_mismatch */ bool timezone_offset_mismatch(const char *stored_iso, int now_off) {
    int so = iso_utc_offset(stored_iso);
    if (so == INT_MIN || now_off == INT_MIN) return false;
    return so != now_off;
}


/* Parse an ISO timestamp's local wall-clock fields into a naive time_t
 * (interpreted in the local zone regardless of the stored offset). */

/* PoP: cronjobs_stored_wall_clock_is_future @ cron/jobs.py:_stored_wall_clock_is_future */ bool stored_wall_clock_is_future(const char *stored_iso, time_t now) {
    time_t sw = iso_wall_clock(stored_iso);
    struct tm nl; localtime_r(&now, &nl); nl.tm_isdst = -1;
    time_t nw = mktime(&nl);
    if (sw == (time_t)-1) return false;
    return sw > nw;
}
