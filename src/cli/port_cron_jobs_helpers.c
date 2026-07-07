/*
 * port_cron_jobs_helpers.c
 *
 * Pure, portable helper functions ported from cron/jobs.py.
 * - parse_duration: string -> minutes (fully pure).
 * - compute_next_run: handles "once" and "interval" schedules purely using
 *   the system clock. The "cron" kind depends on the external 'croniter'
 *   library (not available in C) — for that kind we return NULL, which is an
 *   honest NA for the cron branch (the Python code returns None when croniter
 *   is missing, so NULL is faithful).
 *
 * Module prefix used by the scanner for cron/jobs.py is "jobs_".
 *
 * C name <- python name (jobs_ prefix):
 *   jobs_parse_duration, jobs_compute_next_run
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"

/* ---- lowercase helper ---- */
static void lc(char *s)
{
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

/* ---------------------------------------------------------------------- */
/* PoP: parse_duration @ cron/jobs.py:parse_duration
 * Parses "30m"->30, "2h"->120, "1d"->1440. Returns minutes, or -1 on invalid. */
int jobs_parse_duration(const char *s)
{
    if (!s) return -1;
    char buf[64];
    strncpy(buf, s, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    lc(buf);
    /* strip spaces */
    char compact[64]; int j = 0;
    for (int i = 0; buf[i] && j < (int)sizeof(compact)-1; i++)
        if (!isspace((unsigned char)buf[i])) compact[j++] = buf[i];
    compact[j] = '\0';
    /* match ^(\d+)(m|min|h|hr|d|...) */
    int value = 0; int k = 0;
    while (compact[k] && isdigit((unsigned char)compact[k])) { value = value*10 + (compact[k]-'0'); k++; }
    if (k == 0) return -1;
    char unit = compact[k];
    int mult;
    if (unit == 'm') mult = 1;
    else if (unit == 'h') mult = 60;
    else if (unit == 'd') mult = 1440;
    else return -1; /* not a recognized unit prefix */
    return value * mult;
}

/* ---------------------------------------------------------------------- */
/* PoP: compute_next_run @ cron/jobs.py:compute_next_run
 * schedule_json: {"kind":"once","run_at":"ISO"} | {"kind":"interval","minutes":N}
 * last_run_at: ISO timestamp string or NULL.
 * Returns malloc'd ISO timestamp, or NULL (cron kind / error). Caller frees. */
char *jobs_compute_next_run(const char *schedule_json, const char *last_run_at)
{
    if (!schedule_json || !schedule_json[0]) return NULL;
    json_t *s = json_parse(schedule_json, NULL);
    if (!s || s->type != JSON_OBJECT) { if (s) json_free(s); return NULL; }
    json_t *kind = json_object_get(s, "kind");
    if (!kind || kind->type != JSON_STRING) { json_free(s); return NULL; }
    const char *kind_str = json_string_value(kind);
    char *result = NULL;

    if (strcmp(kind_str, "once") == 0) {
        /* one-shot: if last_run already happened, no more runs -> NULL */
        json_t *run_at = json_object_get(s, "run_at");
        if (!run_at || run_at->type != JSON_STRING) { json_free(s); return NULL; }
        if (last_run_at && last_run_at[0]) { json_free(s); return NULL; }
        result = strdup(json_string_value(run_at));
    } else if (strcmp(kind_str, "interval") == 0) {
        json_t *mins = json_object_get(s, "minutes");
        if (!mins || mins->type != JSON_NUMBER) { json_free(s); return NULL; }
        int minutes = (int)json_number_value(mins);
        time_t base;
        if (last_run_at && last_run_at[0]) {
            struct tm tm; memset(&tm, 0, sizeof(tm));
            if (sscanf(last_run_at, "%d-%d-%dT%d:%d:%d",
                       &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       &tm.tm_hour, &tm.tm_min, &tm.tm_sec) >= 3) {
                tm.tm_year -= 1900; tm.tm_mon -= 1; tm.tm_isdst = -1;
                base = mktime(&tm);
                if (base == (time_t)-1) base = time(NULL);
            } else base = time(NULL);
        } else {
            base = time(NULL);
        }
        base += (time_t)minutes * 60;
        struct tm *ot = gmtime(&base);
        char iso[32];
        snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d",
                 1900+ot->tm_year, ot->tm_mon+1, ot->tm_mday,
                 ot->tm_hour, ot->tm_min, ot->tm_sec);
        /* naive +00:00 suffix (Python isoformat on unaware dt has no tz) */
        size_t L = strlen(iso);
        char *full = malloc(L + 7);
        sprintf(full, "%s+00:00", iso);
        result = full;
    }
    /* "cron" kind: requires croniter (external) -> return NULL (honest NA) */
    json_free(s);
    return result;
}
