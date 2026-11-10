/*
 * cron_jobs_sched.c - cron schedule parsing and next-run computation.
 *
 * Split from port_cron_jobs.c (soul.md: self-contained modules). Pure
 * schedule math; no file-scope state and no job-record access.
 */
#include "cron_jobs_sched.h"
#include "cron_jobs.h"
#include "hermes_strutil.h"
#include "hermes_fs.h"
#include "slermes_home.h"
#include "datetime.h"
#include "cron.h"

#include "cron_jobs_paths.h"   /* set_err, xstrdup */
#include "cron_jobs_time.h"     /* iso_to_ts */
#include "cron_jobs_time2.h"    /* iso_local, recoverable_oneshot_run_at */

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



/* Parse an ISO timestamp → time_t (UTC epoch). -1 on failure. */

/* ── Schedule parsing ─────────────────────────────────────────────── */

/* PoP: cronjobs_parse_duration @ cron/jobs.py:parse_duration */
int cronjobs_parse_duration(const char *s) {
    if (!s) return -1;
    char *stripped = str_strip_dup(s);
    if (!stripped) return -1;
    str_lower_inplace(stripped);
    /* ^(\d+)\s*(m|min|mins|minute|minutes|h|hr|hrs|hour|hours|d|day|days)$ */
    const char *p = stripped;
    long value = 0; int digits = 0;
    while (*p && isdigit((unsigned char)*p)) { value = value * 10 + (*p - '0'); p++; digits++; }
    if (!digits) { free(stripped); return -1; }
    while (*p && isspace((unsigned char)*p)) p++;
    /* unit must be one of the accepted whole words */
    static const char *units[] = {
        "m","min","mins","minute","minutes",
        "h","hr","hrs","hour","hours",
        "d","day","days", NULL
    };
    int mult = 0;
    for (int i = 0; units[i]; i++) {
        if (strcmp(p, units[i]) == 0) {
            mult = (units[i][0] == 'm') ? 1 : (units[i][0] == 'h') ? 60 : 1440;
            break;
        }
    }
    free(stripped);
    if (!mult) return -1;
    return (int)(value * mult);
}


/* Return true when every char in s is one of [0-9*\-,/]. */
static bool is_cron_field(const char *s) {
    if (!s || !*s) return false;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s) && *s != '*' && *s != '-' && *s != ',' && *s != '/')
            return false;
    return true;
}


/* PoP: cronjobs_parse_schedule @ cron/jobs.py:parse_schedule */
json_t *cronjobs_parse_schedule(const char *schedule_in, char **err) {
    char *schedule = str_strip_dup(schedule_in ? schedule_in : "");
    if (!schedule) { set_err(err, "out of memory"); return NULL; }
    char *original = xstrdup(schedule);
    char *lower = xstrdup(schedule);
    str_lower_inplace(lower);

    json_t *out = NULL;

    /* "every X" → recurring interval */
    if (strncmp(lower, "every ", 6) == 0) {
        char *dur = str_strip_dup(schedule + 6);
        int minutes = cronjobs_parse_duration(dur);
        free(dur);
        if (minutes < 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Invalid duration in 'every' schedule");
            set_err(err, msg);
            goto done;
        }
        out = json_object();
        json_set(out, "kind", json_string("interval"));
        json_set(out, "minutes", json_number(minutes));
        char disp[32];
        snprintf(disp, sizeof(disp), "every %dm", minutes);
        json_set(out, "display", json_string(disp));
        goto done;
    }

    /* Cron expression: >=5 space-separated fields, first 5 all cron-field chars */
    {
        /* tokenize on whitespace */
        char *dup = xstrdup(schedule);
        char *saveptr = NULL;
        char *parts[8]; int np = 0;
        for (char *tok = strtok_r(dup, " \t", &saveptr); tok && np < 8;
             tok = strtok_r(NULL, " \t", &saveptr))
            parts[np++] = tok;
        bool looks_cron = (np >= 5);
        for (int i = 0; i < 5 && looks_cron; i++)
            if (!is_cron_field(parts[i])) looks_cron = false;
        free(dup);
        if (looks_cron) {
            char *cerr = NULL;
            cron_expr_t *ce = cron_parse(schedule, &cerr);
            if (!ce) {
                char msg[192];
                snprintf(msg, sizeof(msg), "Invalid cron expression '%s': %s",
                         schedule, cerr ? cerr : "parse error");
                free(cerr);
                set_err(err, msg);
                goto done;
            }
            cron_free(ce);
            free(cerr);
            out = json_object();
            json_set(out, "kind", json_string("cron"));
            json_set(out, "expr", json_string(schedule));
            json_set(out, "display", json_string(schedule));
            goto done;
        }
    }

    /* ISO timestamp: contains 'T' or matches ^\d{4}-\d{2}-\d{2} */
    {
        bool has_t = strchr(schedule, 'T') != NULL;
        bool date_like = (strlen(schedule) >= 10 &&
                          isdigit((unsigned char)schedule[0]) && isdigit((unsigned char)schedule[1]) &&
                          isdigit((unsigned char)schedule[2]) && isdigit((unsigned char)schedule[3]) &&
                          schedule[4] == '-' && isdigit((unsigned char)schedule[5]) &&
                          isdigit((unsigned char)schedule[6]) && schedule[7] == '-' &&
                          isdigit((unsigned char)schedule[8]) && isdigit((unsigned char)schedule[9]));
        if (has_t || date_like) {
            time_t ts = iso_to_ts(schedule);
            if (ts == (time_t)-1) {
                char msg[192];
                snprintf(msg, sizeof(msg), "Invalid timestamp '%s'", schedule);
                set_err(err, msg);
                goto done;
            }
            char *run_at = iso_local(ts);
            struct tm lt; localtime_r(&ts, &lt);
            char disp_time[32];
            strftime(disp_time, sizeof(disp_time), "%Y-%m-%d %H:%M", &lt);
            out = json_object();
            json_set(out, "kind", json_string("once"));
            json_set(out, "run_at", json_string(run_at));
            char disp[64];
            snprintf(disp, sizeof(disp), "once at %s", disp_time);
            json_set(out, "display", json_string(disp));
            free(run_at);
            goto done;
        }
    }

    /* Duration like "30m"/"2h"/"1d" → one-shot from now */
    {
        int minutes = cronjobs_parse_duration(schedule);
        if (minutes >= 0) {
            time_t run = time(NULL) + (time_t)minutes * 60;
            char *run_at = iso_local(run);
            out = json_object();
            json_set(out, "kind", json_string("once"));
            json_set(out, "run_at", json_string(run_at));
            char disp[64];
            snprintf(disp, sizeof(disp), "once in %s", original);
            json_set(out, "display", json_string(disp));
            free(run_at);
            goto done;
        }
    }

    {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Invalid schedule '%s'. Use: duration '30m'/'2h'/'1d', "
                 "interval 'every 30m', cron '0 9 * * *', or timestamp "
                 "'2026-02-03T14:00:00'", original);
        set_err(err, msg);
    }

done:
    free(schedule);
    free(original);
    free(lower);
    return out;
}


/* PoP: cronjobs_compute_grace_seconds @ cron/jobs.py:_compute_grace_seconds */
int cronjobs_compute_grace_seconds(const json_t *schedule) {
    const int MIN_GRACE = 120, MAX_GRACE = 7200;
    const char *kind = json_object_get_string(schedule, "kind", "");
    if (strcmp(kind, "interval") == 0) {
        int minutes = (int)json_get_num(schedule, "minutes", 1);
        long period = (long)minutes * 60;
        long grace = period / 2;
        if (grace < MIN_GRACE) grace = MIN_GRACE;
        if (grace > MAX_GRACE) grace = MAX_GRACE;
        return (int)grace;
    }
    if (strcmp(kind, "cron") == 0) {
        const char *expr = json_object_get_string(schedule, "expr", NULL);
        cron_expr_t *ce = expr ? cron_parse(expr, NULL) : NULL;
        if (ce) {
            time_t now = time(NULL);
            struct tm from, first, second;
            localtime_r(&now, &from);
            if (cron_next(ce, &from, &first) && cron_next(ce, &first, &second)) {
                time_t f = mktime(&first), s = mktime(&second);
                long period = (long)(s - f);
                long grace = period / 2;
                cron_free(ce);
                if (grace < MIN_GRACE) grace = MIN_GRACE;
                if (grace > MAX_GRACE) grace = MAX_GRACE;
                return (int)grace;
            }
            cron_free(ce);
        }
    }
    return MIN_GRACE;
}


/* PoP: cronjobs_compute_next_run @ cron/jobs.py:compute_next_run */
char *cronjobs_compute_next_run(const json_t *schedule, const char *last_run_at) {
    if (!json_is_object(schedule)) return NULL;
    time_t now = time(NULL);
    const char *kind = json_object_get_string(schedule, "kind", "");

    if (strcmp(kind, "once") == 0) {
        return recoverable_oneshot_run_at(schedule, now, last_run_at);
    }
    if (strcmp(kind, "interval") == 0) {
        int minutes = (int)json_get_num(schedule, "minutes", 0);
        time_t base;
        if (last_run_at && *last_run_at) {
            base = iso_to_ts(last_run_at);
            if (base == (time_t)-1) base = now;
        } else {
            base = now;
        }
        return iso_local(base + (time_t)minutes * 60);
    }
    if (strcmp(kind, "cron") == 0) {
        const char *expr = json_object_get_string(schedule, "expr", NULL);
        if (!expr) return NULL;
        cron_expr_t *ce = cron_parse(expr, NULL);
        if (!ce) return NULL;
        time_t base = now;
        if (last_run_at && *last_run_at) {
            time_t b = iso_to_ts(last_run_at);
            if (b != (time_t)-1) base = b;
        }
        struct tm from, out;
        localtime_r(&base, &from);
        char *result = NULL;
        if (cron_next(ce, &from, &out)) {
            struct tm tmp = out;
            time_t nt = mktime(&tmp);
            result = iso_local(nt);
        }
        cron_free(ce);
        return result;
    }
    return NULL;
}


/* ── Workdir / optional text ──────────────────────────────────────── */

/* PoP: cronjobs_ensure_aware @ cron/jobs.py:_ensure_aware */
/* Single-home Slermes stores aware ISO timestamps (see iso_local); naive
 * timestamps never occur, so "ensure aware" is identity with a guard that
 * returns NULL for empty input (mirrors fail-open on unparseable input). */
static char *ensure_aware(const char *iso) {
    if (!iso || !*iso) return NULL;
    return xstrdup(iso);
}
