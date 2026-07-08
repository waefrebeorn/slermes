/*
 * port_file_state_helpers.c
 *
 * Pure, portable helpers ported from tools/file_state.py.
 *
 * - _fmt_ts(ts): format an epoch (float seconds) as "HH:MM:SS" local time.
 * - _cap_dict(d, limit): trim a JSON object to the newest `limit` keys,
 *   dropping the oldest by insertion order (mirrors OrderedDict/py-dict).
 *
 * The stateful wrappers (record_read / note_write / check_stale / writes_since)
 * are file-coupled and left as honest REAL_GAP.
 *
 * Module prefix used by the scanner for tools/file_state.py is "file_state_".
 *
 * C name <- python name (file_state_ prefix): fmt_ts, cap_dict
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hermes_json.h"

/* ---------------------------------------------------------------------- */
/* PoP: _fmt_ts @ tools/file_state.py:_fmt_ts */
/* Returns malloc'd "HH:MM:SS" string for the given epoch (seconds). */
char *file_state_fmt_ts(double ts)
{
    time_t t = (time_t)ts;
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tmv);
    return strdup(buf);
}

/* ---------------------------------------------------------------------- */
/* PoP: _cap_dict @ tools/file_state.py:_cap_dict */
/* Trims the JSON object referenced by *obj to the newest `limit` keys by
 * preserving insertion order: drops the oldest (first) keys beyond the limit.
 * hermes_json/libjson has no object-clear, so we rebuild *obj in place via a
 * double pointer (faithful equivalent of Python's in-place dict mutation). */
void file_state_cap_dict(json_t **obj, int limit)
{
    if (!obj || !*obj || (*obj)->type != JSON_OBJECT) return;
    json_t *src = *obj;
    size_t n = json_object_size(src);
    if ((long long)n <= (long long)limit) return;

    json_t *fresh = json_new_object();
    size_t start = n - (size_t)limit;
    for (size_t i = start; i < n; i++) {
        const char *k = json_object_get_key_at(src, i);
        json_t *v = json_object_get_at(src, i);
        if (k) json_object_set(fresh, k, v);
    }
    json_free(src);
    *obj = fresh;
}
