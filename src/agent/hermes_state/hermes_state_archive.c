/* hermes_state_archive.c — archive/pin/sweep/list surface of the SessionDB
 * port. set_session_archived and set_session_pinned flip the WHOLE
 * compression lineage (ancestors through compression-ended parents +
 * descendants of compression-ended parents) via the same recursive CTE the
 * Python uses — archiving a surfaced tip must archive the projected root so
 * Desktop's projected list cannot resurrect it. Self-contained unit.
 */

#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Shared lineage-flip core for archived/pinned columns. Column name is
 * compile-time constant (never user input) so the format is safe. */
static bool lineage_flag_update(hermes_state_db_t *db, const char *session_id,
                                const char *column, bool value) {
    if (!db || !session_id || !*session_id) return false;
    char sql[1024];
    snprintf(sql, sizeof sql,
        "WITH RECURSIVE"
        "  ancestors(id) AS ("
        "    SELECT ?"
        "    UNION"
        "    SELECT parent.id"
        "    FROM ancestors a"
        "    JOIN sessions child ON child.id = a.id"
        "    JOIN sessions parent ON parent.id = child.parent_session_id"
        "    WHERE parent.end_reason = 'compression'"
        "  ),"
        "  descendants(id) AS ("
        "    SELECT ?"
        "    UNION"
        "    SELECT child.id"
        "    FROM descendants d"
        "    JOIN sessions parent ON parent.id = d.id"
        "    JOIN sessions child ON child.parent_session_id = parent.id"
        "    WHERE parent.end_reason = 'compression'"
        "  ),"
        "  lineage(id) AS ("
        "    SELECT id FROM ancestors UNION SELECT id FROM descendants"
        "  )"
        " UPDATE sessions SET %s = ? WHERE id IN (SELECT id FROM lineage)",
        column);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, value ? 1 : 0);
    bool stepped = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    if (!stepped) return false;
    /* Python: rowcount > 0 (at least one row updated). */
    return sqlite3_changes(db->db) > 0;
}

/* PoP: set_session_archived @ hermes_state.py:set_session_archived */
bool hermes_state_set_session_archived(hermes_state_db_t *db,
                                       const char *session_id, bool archived) {
    return lineage_flag_update(db, session_id, "archived", archived);
}

/* PoP: set_session_pinned @ hermes_state.py:set_session_pinned */
bool hermes_state_set_session_pinned(hermes_state_db_t *db,
                                     const char *session_id, bool pinned) {
    return lineage_flag_update(db, session_id, "pinned", pinned);
}

/* PoP: archive_stale_sessions @ hermes_state.py:archive_stale_sessions */
int hermes_state_archive_stale_sessions(hermes_state_db_t *db,
                                        double idle_days, bool exclude_pinned) {
    if (!db || idle_days <= 0) return 0;
    double cutoff = hermes_state_now_epoch() - idle_days * 86400.0;
    char sql[512];
    snprintf(sql, sizeof sql,
        "SELECT s.id FROM sessions s "
        "WHERE s.archived = 0 "
        "AND COALESCE(s.end_reason,'') <> 'compression' "
        "%s"
        "AND COALESCE((SELECT MAX(m.timestamp) FROM messages m "
        "              WHERE m.session_id = s.id), s.started_at) < ?",
        exclude_pinned ? "AND s.pinned = 0 " : "");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_double(st, 1, cutoff);
    char *ids[256];
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW && n < 256) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        if (id) ids[n++] = strdup((const char *)id);
    }
    sqlite3_finalize(st);
    int archived = 0;
    for (int i = 0; i < n; i++) {
        if (hermes_state_set_session_archived(db, ids[i], true)) archived++;
        free(ids[i]);
    }
    return archived;
}

/* PoP: list_prune_candidates @ hermes_state.py:list_prune_candidates */
char *hermes_state_list_prune_candidates(hermes_state_db_t *db,
                                         double older_than_days,
                                         const char *source,
                                         bool require_ended,
                                         int archived_filter /* -1 any, 0, 1 */) {
    if (!db) return strdup("[]");
    double cutoff = older_than_days > 0
        ? hermes_state_now_epoch() - older_than_days * 86400.0
        : 0;
    char sql[1024];
    snprintf(sql, sizeof sql,
        "SELECT s.id, s.source, s.title, "
        "COALESCE((SELECT MAX(m.timestamp) FROM messages m "
        "          WHERE m.session_id = s.id), s.started_at) AS last_active, "
        "s.ended_at IS NOT NULL, s.message_count, s.archived "
        "FROM sessions s WHERE 1=1 "
        "%s%s%s%s"
        "ORDER BY last_active ASC, s.started_at ASC",
        older_than_days > 0 ? "AND last_active < ?1 " : "",
        source && *source ? "AND s.source = ?2 " : "",
        require_ended ? "AND s.ended_at IS NOT NULL " : "",
        archived_filter == 0 ? "AND s.archived = 0 "
            : archived_filter == 1 ? "AND s.archived = 1 " : "");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK)
        return strdup("[]");
    if (older_than_days > 0) sqlite3_bind_double(st, 1, cutoff);
    if (source && *source) sqlite3_bind_text(st, 2, source, -1, SQLITE_TRANSIENT);
    char *buf = malloc(4096);
    size_t cap = 4096, len = 0;
    len = (size_t)snprintf(buf, cap, "[");
    bool first = true;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        const unsigned char *src = sqlite3_column_text(st, 1);
        const unsigned char *title = sqlite3_column_text(st, 2);
        double last_active = sqlite3_column_double(st, 3);
        int ended = sqlite3_column_int(st, 4);
        int mc = sqlite3_column_int(st, 5);
        int arch = sqlite3_column_int(st, 6);
        char titleq[600];
        if (title) snprintf(titleq, sizeof titleq, "\"%s\"", (const char *)title);
        else snprintf(titleq, sizeof titleq, "null");
        char part[1024];
        int n = snprintf(part, sizeof part,
            "%s{\"id\":\"%s\",\"source\":\"%s\",\"title\":%s,"
            "\"last_active\":%.6f,\"ended\":%d,\"message_count\":%d,"
            "\"archived\":%d}",
            first ? "" : ",",
            id ? (const char *)id : "",
            src ? (const char *)src : "",
            titleq, last_active, ended, mc, arch);
        if (len + (size_t)n + 2 >= cap) { cap = (len + (size_t)n + 2) * 2; buf = realloc(buf, cap); }
        memcpy(buf + len, part, (size_t)n); len += (size_t)n; buf[len] = '\0';
        first = false;
    }
    sqlite3_finalize(st);
    len += (size_t)snprintf(buf + len, cap - len, "]");
    return buf;
}

/* PoP: archive_sessions @ hermes_state.py:archive_sessions */
int hermes_state_archive_sessions(hermes_state_db_t *db,
                                  double older_than_days, const char *source) {
    if (!db) return 0;
    char *json = hermes_state_list_prune_candidates(db, older_than_days,
                                                    source, true, 0);
    /* walk the JSON ids: rows are our own emitter's output — extract "id" */
    int count = 0;
    const char *p = json;
    while ((p = strstr(p, "{\"id\":\"")) != NULL) {
        p += 7;
        const char *e = strchr(p, '"');
        if (!e) break;
        char id[256];
        size_t idl = (size_t)(e - p);
        if (idl >= sizeof id) idl = sizeof id - 1;
        memcpy(id, p, idl); id[idl] = '\0';
        hermes_state_set_session_archived(db, id, true);
        count++;
        p = e;
    }
    free(json);
    return count;
}

/* PoP: list_sessions_rich @ hermes_state.py:list_sessions_rich */
char *hermes_state_list_sessions_rich(hermes_state_db_t *db,
                                      bool archived_only) {
    if (!db) return strdup("[]");
    char sql[768];
    snprintf(sql, sizeof sql,
        "SELECT s.id, s.source, s.title, "
        "COALESCE((SELECT MAX(m.timestamp) FROM messages m "
        "          WHERE m.session_id = s.id), s.started_at) AS last_active, "
        "s.archived "
        "FROM sessions s "
        "WHERE s.archived = %d "
        "AND COALESCE(s.end_reason,'') <> 'compression' "
        "ORDER BY last_active DESC, s.started_at DESC",
        archived_only ? 1 : 0);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK)
        return strdup("[]");
    char *buf = malloc(4096);
    size_t cap = 4096, len = 0;
    len = (size_t)snprintf(buf, cap, "[");
    bool first = true;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        const unsigned char *src = sqlite3_column_text(st, 1);
        const unsigned char *title = sqlite3_column_text(st, 2);
        double last_active = sqlite3_column_double(st, 3);
        int arch = sqlite3_column_int(st, 4);
        char titleq[600];
        if (title) snprintf(titleq, sizeof titleq, "\"%s\"", (const char *)title);
        else snprintf(titleq, sizeof titleq, "null");
        char part[1024];
        int n = snprintf(part, sizeof part,
            "%s{\"id\":\"%s\",\"source\":\"%s\",\"title\":%s,"
            "\"last_active\":%.6f,\"archived\":%d}",
            first ? "" : ",",
            id ? (const char *)id : "",
            src ? (const char *)src : "",
            titleq, last_active, arch);
        if (len + (size_t)n + 2 >= cap) { cap = (len + (size_t)n + 2) * 2; buf = realloc(buf, cap); }
        memcpy(buf + len, part, (size_t)n); len += (size_t)n; buf[len] = '\0';
        first = false;
    }
    sqlite3_finalize(st);
    len += (size_t)snprintf(buf + len, cap - len, "]");
    return buf;
}
