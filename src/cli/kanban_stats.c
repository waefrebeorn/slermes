/*
 * kanban_stats.c — board statistics + task age for hermes_cli/kanban_db.py
 *
 * Concern: aggregate board stats (board_stats) and per-task age metrics
 * (task_age). Returns malloc'd JSON strings shaped exactly like the Python
 * module's return dicts, so the oracle can diff them against live Python.
 *
 * Minimal includes. No god header.
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py
 *      (board_stats / task_age / _to_epoch).
 * PoP: kdb_board_stats @ hermes_cli/kanban_db.py:board_stats
 * PoP: kdb_task_age @ hermes_cli/kanban_db.py:task_age
 */

#include "kanban_db.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Normalise a timestamp to epoch seconds (mirrors _to_epoch). */
static long kb_to_epoch(const char *val)
{
    if (!val) return -1;
    char *end = NULL;
    long as_long = strtol(val, &end, 10);
    if (end != val && *end == '\0') return as_long;
    double as_double = strtod(val, &end);
    if (end != val && *end == '\0') return (long)as_double;
    while (*val && *val == ' ') val++;
    size_t n = strlen(val);
    while (n > 0 && val[n - 1] == ' ') n--;
    if (n == 0) return -1;
    char buf[64];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, val, n); buf[n] = '\0';
    char *z = strchr(buf, 'Z');
    if (z) { *z = '+'; size_t len = strlen(buf);
             if (len + 5 < sizeof(buf)) { buf[len]='0'; buf[len+1]=':'; buf[len+2]='0'; buf[len+3]='0'; buf[len+4]='\0'; } }
    int Y, M, D, h = 0, m = 0, s = 0, tz_h = 0, tz_m = 0; char s1, s2;
    int got = sscanf(buf, "%d-%d-%d%c%d:%d:%d%c%d:%d", &Y, &M, &D, &s1, &h, &m, &s, &s2, &tz_h, &tz_m);
    if (got >= 7) {
        int yy = Y - (M <= 2 ? 1 : 0);
        int era = (yy >= 0 ? yy : yy - 399) / 400;
        long yoe = (long)(yy - era * 400);
        long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
        long doe = (era * 365L + yoe / 4 - yoe / 100) * 1L + doy;
        long days = era * 146097L + doe - 719468L;
        long secs = days * 86400L + h * 3600L + m * 60L + s - (tz_h * 3600L + tz_m * 60L);
        return (long)secs;
    }
    return -1;
}

/* Append a json number-or-null. key is a bare field name (no quotes). */
static void json_num(char *dst, size_t sz, const char *key, long v)
{
    char tmp[64];
    if (v < 0) snprintf(tmp, sizeof(tmp), "\"%s\":null", key);
    else snprintf(tmp, sizeof(tmp), "\"%s\":%ld", key, v);
    strncat(dst, tmp, sz - strlen(dst) - 1);
}

/* PoP: kdb_board_stats @ hermes_cli/kanban_db.py:board_stats */
char *kdb_board_stats(sqlite3 *conn)
{
    if (!conn) return NULL;
    char by_status[2048]; by_status[0] = '\0';
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(conn,
            "SELECT status, COUNT(*) AS n FROM tasks WHERE status!='archived' GROUP BY status",
            -1, &st, NULL) == SQLITE_OK) {
        strcat(by_status, "{");
        int first = 1;
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *s = (const char *)sqlite3_column_text(st, 0);
            long n = sqlite3_column_int64(st, 1);
            char e[64];
            snprintf(e, sizeof(e), "%s\"%s\":%ld", first ? "" : ",", s ? s : "", n);
            strncat(by_status, e, sizeof(by_status) - strlen(by_status) - 1);
            first = 0;
        }
        strcat(by_status, "}");
        sqlite3_finalize(st);
    }

    /* by_assignee — group rows by assignee (rows arrive in order) */
    char by_assignee[4096]; by_assignee[0] = '\0';
    strcat(by_assignee, "{");
    if (sqlite3_prepare_v2(conn,
            "SELECT assignee, status, COUNT(*) AS n FROM tasks WHERE status!='archived' "
            "AND assignee IS NOT NULL GROUP BY assignee, status ORDER BY assignee",
            -1, &st, NULL) == SQLITE_OK) {
        int first_a = 1;
        char cur_a[256]; cur_a[0] = '\0';
        while (sqlite3_step(st) == SQLITE_ROW) {
            const char *a = (const char *)sqlite3_column_text(st, 0);
            const char *s = (const char *)sqlite3_column_text(st, 1);
            long n = sqlite3_column_int64(st, 2);
            if (!first_a && strcmp(a ? a : "", cur_a) != 0) {
                strncat(by_assignee, "}", sizeof(by_assignee) - strlen(by_assignee) - 1);
            }
            if (first_a || strcmp(a ? a : "", cur_a) != 0) {
                if (a) snprintf(cur_a, sizeof(cur_a), "%s", a); else cur_a[0] = '\0';
                char o[300];
                snprintf(o, sizeof(o), "%s\"%s\":{", first_a ? "" : ",", a ? a : "");
                strncat(by_assignee, o, sizeof(by_assignee) - strlen(by_assignee) - 1);
                first_a = 0;
                /* close entry when assignee changes handled above */
            } else {
                strncat(by_assignee, ",", sizeof(by_assignee) - strlen(by_assignee) - 1);
            }
            char e[64];
            snprintf(e, sizeof(e), "\"%s\":%ld", s ? s : "", n);
            strncat(by_assignee, e, sizeof(by_assignee) - strlen(by_assignee) - 1);
        }
        if (!first_a) strncat(by_assignee, "}", sizeof(by_assignee) - strlen(by_assignee) - 1);
        strncat(by_assignee, "}", sizeof(by_assignee) - strlen(by_assignee) - 1);
        sqlite3_finalize(st);
    }

    long oldest = -1;
    if (sqlite3_prepare_v2(conn,
            "SELECT MIN(created_at) AS ts FROM tasks WHERE status='ready'", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL) {
            long ts = (long)sqlite3_column_int64(st, 0);
            oldest = kdb_now() - ts;
        }
        sqlite3_finalize(st);
    }
    long now = kdb_now();
    char *out = malloc(2048);
    if (oldest < 0)
        snprintf(out, 2048,
            "{\"by_assignee\":%s,\"by_status\":%s,\"now\":%ld,\"oldest_ready_age_seconds\":null}",
            by_assignee, by_status, now);
    else
        snprintf(out, 2048,
            "{\"by_assignee\":%s,\"by_status\":%s,\"now\":%ld,\"oldest_ready_age_seconds\":%ld}",
            by_assignee, by_status, now, oldest);
    return out;
}

/* PoP: kdb_task_age @ hermes_cli/kanban_db.py:task_age */
char *kdb_task_age(sqlite3 *conn, const char *task_id)
{
    if (!conn || !task_id) return NULL;
    sqlite3_stmt *st = NULL;
    long c = -1, s = -1, co = -1;
    if (sqlite3_prepare_v2(conn, "SELECT created_at, started_at, completed_at FROM tasks WHERE id=?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, task_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            c  = sqlite3_column_type(st,0)==SQLITE_NULL?-1:(long)sqlite3_column_int64(st,0);
            s  = sqlite3_column_type(st,1)==SQLITE_NULL?-1:(long)sqlite3_column_int64(st,1);
            co = sqlite3_column_type(st,2)==SQLITE_NULL?-1:(long)sqlite3_column_int64(st,2);
        }
        sqlite3_finalize(st);
    }
    long now = kdb_now();
    long age_created = c >= 0 ? now - c : -1;
    long age_started = s >= 0 ? now - s : -1;
    long ttc = co >= 0 ? co - (s >= 0 ? s : (c >= 0 ? c : co)) : -1;
    char *out = malloc(256);
    out[0] = '\0';
    strcat(out, "{");
    json_num(out, 256, "created_age_seconds", age_created);
    strncat(out, ",", 255 - strlen(out));
    json_num(out, 256, "started_age_seconds", age_started);
    strncat(out, ",", 255 - strlen(out));
    json_num(out, 256, "time_to_complete_seconds", ttc);
    strncat(out, "}", 255 - strlen(out));
    return out;
}
