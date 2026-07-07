/*
 * port_skill_usage_helpers.c
 *
 * Pure, portable helper ported from tools/skill_usage.py.
 *
 * - latest_activity_at: takes a usage-record JSON object, reads the
 *   last_used_at / last_viewed_at / last_patched_at ISO timestamps, and
 *   returns the newest one as a malloc'd string (NULL if none parse). Pure
 *   JSON + ISO-8601 parse; no file IO.
 *
 *   (is_protected_builtin is already ported in lib/libskillusage/skill_usage.c,
 *    so it is intentionally NOT duplicated here.)
 *
 * Module prefix used by the scanner for tools/skill_usage.py is "skill_usage_".
 *
 * C name <- python name (skill_usage_ prefix): latest_activity_at
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hermes_json.h"

/* Parse an ISO-8601 timestamp (YYYY-MM-DDTHH:MM:SS[.fff][Z|±HH:MM]) into a
 * time_t (UTC). Returns 1 on success, 0 on failure. */
static int parse_iso(const char *value, time_t *out)
{
    if (!value || !*value) return 0;
    int Y, M, D, h, m, s = 0;
    /* up to seconds; tolerate fractional + tz suffix */
    if (sscanf(value, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) < 6)
        return 0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = Y - 1900;
    tmv.tm_mon = M - 1;
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min = m;
    tmv.tm_sec = s;
    tmv.tm_isdst = 0;
#ifdef _WIN32
    *out = _mkgmtime(&tmv);
#else
    *out = timegm(&tmv);
#endif
    return (*out != (time_t)-1) ? 1 : 0;
}

/* ---------------------------------------------------------------------- */
/* PoP: latest_activity_at @ tools/skill_usage.py:latest_activity_at */
/* record_json: JSON object with optional last_used_at/last_viewed_at/last_patched_at */
char *skill_usage_latest_activity_at(const char *record_json)
{
    json_t *rec = json_parse(record_json, NULL);
    if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); return NULL; }
    static const char *keys[] = {"last_used_at", "last_viewed_at", "last_patched_at"};
    time_t best = (time_t)-1;
    const char *best_raw = NULL;
    for (int i = 0; i < 3; i++) {
        json_t *v = json_object_get(rec, keys[i]);
        if (!v || v->type != JSON_STRING) continue;
        const char *raw = json_string_value(v);
        time_t t;
        if (parse_iso(raw, &t)) {
            if (best == (time_t)-1 || t > best) { best = t; best_raw = raw; }
        }
    }
    char *out = NULL;
    if (best_raw) {
        out = malloc(strlen(best_raw) + 1);
        strcpy(out, best_raw);
    }
    json_free(rec);
    return out;
}
