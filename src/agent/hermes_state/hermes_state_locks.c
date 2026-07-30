/* hermes_state_locks.c — compression-lock + compression-child publication
 * surface of the SessionDB port. Faithful C11 port of hermes_state.py:
 *   _compression_lock_holder_process_is_dead
 *   try_acquire_compression_lock / release_compression_lock
 *   refresh_compression_lock / get_compression_lock_holder
 *   find_live_compression_child / publish_compression_child
 * All SQL mirrors the Python method bodies byte-for-byte in semantics.
 */
#define _GNU_SOURCE
#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

/* Transactional bulk message insert for the compression-child handoff.
 * Mirrors Python _insert_message_rows: sequential idx, per-row token count
 * pass-through, tool_calls counted for the parent UPDATE. Returns message
 * count (>=0) or -1 on parse/SQL failure. Caller owns the transaction. */
static int insert_handoff_messages(hermes_state_db_t *db,
                                   const char *session_id,
                                   const char *messages_json,
                                   int *out_tool_calls) {
    json_t *arr = json_parse(messages_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { json_free(arr); return -1; }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT INTO messages (session_id, role, content,"
            " tool_name, tool_call_id, token_count, timestamp)"
            " VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &st, NULL) != SQLITE_OK) {
        json_free(arr);
        return -1;
    }
    double now = hermes_state_now_epoch();
    int n = (int)json_len(arr);
    int tool_calls = 0;
    for (int i = 0; i < n; i++) {
        const json_t *m = json_get(arr, i);
        if (!m || m->type != JSON_OBJECT) continue;
        const char *role = json_get_str(m, "role", "user");
        const char *content = json_get_str(m, "content", "");
        const char *tool_name = json_get_str(m, "tool_name", NULL);
        const char *tool_call_id = json_get_str(m, "tool_call_id", NULL);
        if (json_obj_get(m, "tool_calls")) tool_calls++;
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, role, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, content ? content : "", -1, SQLITE_TRANSIENT);
        if (tool_name) sqlite3_bind_text(st, 4, tool_name, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(st, 4);
        if (tool_call_id)
            sqlite3_bind_text(st, 5, tool_call_id, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(st, 5);
        sqlite3_bind_int(st, 6, (int)json_get_num(m, "token_count", 0));
        sqlite3_bind_double(st, 7, now);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            json_free(arr);
            return -1;
        }
    }
    sqlite3_finalize(st);
    json_free(arr);
    if (out_tool_calls) *out_tool_calls = tool_calls;
    return n;
}

/* ── PID liveness ─────────────────────────────────────────────────────── */

/* PoP: hermes_state_lock_holder_process_is_dead @ hermes_state.py:_compression_lock_holder_process_is_dead */
bool hermes_state_lock_holder_process_is_dead(const char *holder) {
    if (!holder || !holder[0]) return false;
    /* match (?:^|:)pid=(\d+)(?::|$) */
    const char *p = holder;
    long pid = -1;
    while (p && *p) {
        if ((p == holder || p[-1] == ':') && strncmp(p, "pid=", 4) == 0) {
            const char *digits = p + 4;
            if (isdigit((unsigned char)*digits)) {
                char *end = NULL;
                long v = strtol(digits, &end, 10);
                if (end > digits && (*end == ':' || *end == '\0')) {
                    pid = v;
                    break;
                }
            }
        }
        p = strchr(p + 1, ':');
        if (p) p++; /* char after ':' */
    }
    if (pid <= 0) return false;
    if ((pid_t)pid == getpid()) return false; /* same-process holder */
    /* POSIX liveness probe (Python uses psutil; kill(pid,0) is the
     * canonical POSIX equivalent — ESRCH proves the PID is gone;
     * EPERM/any doubt keeps the lease until TTL expiry). */
    if (kill((pid_t)pid, 0) == 0) return false;
    return errno == ESRCH;
}

/* ── try_acquire_compression_lock ─────────────────────────────────────── */

/* PoP: hermes_state_try_acquire_compression_lock @ hermes_state.py:try_acquire_compression_lock */
bool hermes_state_try_acquire_compression_lock(hermes_state_db_t *db,
                                               const char *session_id,
                                               const char *holder,
                                               double ttl_seconds) {
    if (!db || !session_id || !session_id[0]) return false;
    double now = hermes_state_now_epoch();
    double expires_at = now + (ttl_seconds > 0 ? ttl_seconds : 300.0);
    bool acquired = false;

    sqlite3_exec(db->db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    /* reclaim expired or dead-PID holder */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT holder, expires_at FROM compression_locks "
            "WHERE session_id = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *cur_holder = (const char *)sqlite3_column_text(st, 0);
            double cur_expires = sqlite3_column_double(st, 1);
            if (cur_expires < now ||
                hermes_state_lock_holder_process_is_dead(cur_holder)) {
                sqlite3_stmt *del = NULL;
                if (sqlite3_prepare_v2(db->db,
                        "DELETE FROM compression_locks "
                        "WHERE session_id = ? AND holder = ?",
                        -1, &del, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(del, 1, session_id, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(del, 2, cur_holder ? cur_holder : "",
                                      -1, SQLITE_TRANSIENT);
                    sqlite3_step(del);
                    sqlite3_finalize(del);
                }
            }
        }
        sqlite3_finalize(st);
    }

    /* INSERT OR IGNORE, then SELECT to confirm ownership */
    st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR IGNORE INTO compression_locks "
            "(session_id, holder, acquired_at, expires_at) "
            "VALUES (?, ?, ?, ?)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, holder ? holder : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 3, now);
        sqlite3_bind_double(st, 4, expires_at);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT holder FROM compression_locks WHERE session_id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *got = (const char *)sqlite3_column_text(st, 0);
            acquired = got && holder && strcmp(got, holder) == 0;
        }
        sqlite3_finalize(st);
    }

    sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
    return acquired;
}

/* ── release_compression_lock ─────────────────────────────────────────── */

/* PoP: hermes_state_release_compression_lock @ hermes_state.py:release_compression_lock */
void hermes_state_release_compression_lock(hermes_state_db_t *db,
                                           const char *session_id,
                                           const char *holder) {
    if (!db || !session_id || !session_id[0]) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "DELETE FROM compression_locks "
            "WHERE session_id = ? AND holder = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, holder ? holder : "", -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* ── refresh_compression_lock ─────────────────────────────────────────── */

/* PoP: hermes_state_refresh_compression_lock @ hermes_state.py:refresh_compression_lock */
bool hermes_state_refresh_compression_lock(hermes_state_db_t *db,
                                           const char *session_id,
                                           const char *holder,
                                           double ttl_seconds) {
    if (!db || !session_id || !session_id[0] || !holder || !holder[0])
        return false;
    double expires_at = hermes_state_now_epoch() +
                        (ttl_seconds > 0 ? ttl_seconds : 300.0);
    /* Ownership by holder column ALONE — deliberately not expires_at
     * (see Python docstring: a starved-but-live owner must revive). */
    sqlite3_stmt *st = NULL;
    bool ok = false;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE compression_locks SET expires_at = ? "
            "WHERE session_id = ? AND holder = ?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_double(st, 1, expires_at);
        sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, holder, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        ok = sqlite3_changes(db->db) > 0;
        sqlite3_finalize(st);
    }
    return ok;
}

/* ── get_compression_lock_holder ──────────────────────────────────────── */

/* PoP: hermes_state_get_compression_lock_holder @ hermes_state.py:get_compression_lock_holder */
char *hermes_state_get_compression_lock_holder(hermes_state_db_t *db,
                                               const char *session_id) {
    if (!db || !session_id || !session_id[0]) return NULL;
    double now = hermes_state_now_epoch();
    sqlite3_stmt *st = NULL;
    char *out = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT holder FROM compression_locks "
            "WHERE session_id = ? AND expires_at >= ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 2, now);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const char *h = (const char *)sqlite3_column_text(st, 0);
            if (h) out = strdup(h);
        }
        sqlite3_finalize(st);
    }
    return out;
}

/* ── find_live_compression_child ──────────────────────────────────────── */

/* PoP: hermes_state_find_live_compression_child @ hermes_state.py:find_live_compression_child */
char *hermes_state_find_live_compression_child(hermes_state_db_t *db,
                                               const char *parent_session_id) {
    if (!db || !parent_session_id || !parent_session_id[0]) return NULL;

    /* parent must be ended with end_reason='compression' */
    sqlite3_stmt *st = NULL;
    bool parent_ok = false;
    if (sqlite3_prepare_v2(db->db,
            "SELECT ended_at, end_reason FROM sessions WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, parent_session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            bool ended = sqlite3_column_type(st, 0) != SQLITE_NULL;
            const char *reason = (const char *)sqlite3_column_text(st, 1);
            parent_ok = ended && reason && strcmp(reason, "compression") == 0;
        }
        sqlite3_finalize(st);
    }
    if (!parent_ok) return NULL;

    /* exactly one live direct child, excluding branch/delegate forks and
     * tool sessions; ambiguity (2+) fails closed */
    st = NULL;
    char *child_id = NULL;
    int rows = 0;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id FROM sessions "
            "WHERE parent_session_id = ? "
            "  AND ended_at IS NULL "
            "  AND json_extract(COALESCE(model_config, '{}'), '$._branched_from') IS NULL "
            "  AND json_extract(COALESCE(model_config, '{}'), '$._delegate_from') IS NULL "
            "  AND COALESCE(source, '') != 'tool' "
            "ORDER BY started_at ASC LIMIT 2",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, parent_session_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            rows++;
            if (rows == 1) {
                const char *id = (const char *)sqlite3_column_text(st, 0);
                if (id) child_id = strdup(id);
            }
        }
        sqlite3_finalize(st);
    }
    if (rows != 1) { free(child_id); return NULL; }
    return child_id;
}

/* ── publish_compression_child ────────────────────────────────────────── */

/* PoP: hermes_state_publish_compression_child @ hermes_state.py:publish_compression_child */
int hermes_state_publish_compression_child(hermes_state_db_t *db,
                                           const char *parent_session_id,
                                           const char *child_session_id,
                                           const char *source,
                                           const char *messages_json,
                                           const char *model,
                                           const char *model_config_json,
                                           const char *system_prompt,
                                           const char *cwd,
                                           const char *profile_name,
                                           const char *compression_lock_holder,
                                           bool require_compression_lease) {
    if (!db || !parent_session_id || !child_session_id) return -1;
    double now = hermes_state_now_epoch();
    int rc = -1;

    sqlite3_exec(db->db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    /* lease check */
    if (require_compression_lease) {
        sqlite3_stmt *st = NULL;
        bool lease_ok = false;
        if (sqlite3_prepare_v2(db->db,
                "SELECT holder, expires_at FROM compression_locks "
                "WHERE session_id = ?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, parent_session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const char *h = (const char *)sqlite3_column_text(st, 0);
                double exp = sqlite3_column_double(st, 1);
                lease_ok = h && compression_lock_holder &&
                           strcmp(h, compression_lock_holder) == 0 &&
                           exp > now;
            }
            sqlite3_finalize(st);
        }
        if (!lease_ok) { rc = -2; goto rollback; } /* lease lost */
    }

    /* parent must exist and be live */
    {
        sqlite3_stmt *st = NULL;
        bool found = false, live = false;
        if (sqlite3_prepare_v2(db->db,
                "SELECT ended_at FROM sessions WHERE id = ?",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, parent_session_id, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                found = true;
                live = sqlite3_column_type(st, 0) == SQLITE_NULL;
            }
            sqlite3_finalize(st);
        }
        if (!found) { rc = -3; goto rollback; } /* parent not found */
        if (!live)  { rc = -4; goto rollback; } /* parent already ended */
    }
    if (!messages_json || !messages_json[0] ||
        strcmp(messages_json, "[]") == 0) {
        rc = -5; goto rollback; /* empty handoff */
    }

    /* insert child inheriting parent's routing/origin columns */
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->db,
                "INSERT INTO sessions ("
                " id, source, model, model_config, system_prompt,"
                " parent_session_id, cwd, git_branch, git_repo_root,"
                " profile_name, user_id, session_key, chat_id, chat_type,"
                " thread_id, display_name, origin_json, started_at)"
                " SELECT ?, ?, ?, ?, ?, id, COALESCE(?, cwd), git_branch,"
                "        git_repo_root, COALESCE(?, profile_name), user_id,"
                "        session_key, chat_id, chat_type, thread_id,"
                "        display_name, origin_json, ?"
                " FROM sessions WHERE id = ?",
                -1, &st, NULL) != SQLITE_OK) { rc = -6; goto rollback; }
        sqlite3_bind_text(st, 1, child_session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, source ? source : "cli", -1, SQLITE_TRANSIENT);
        if (model) sqlite3_bind_text(st, 3, model, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(st, 3);
        if (model_config_json)
            sqlite3_bind_text(st, 4, model_config_json, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(st, 4);
        if (system_prompt)
            sqlite3_bind_text(st, 5, system_prompt, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(st, 5);
        if (cwd) sqlite3_bind_text(st, 6, cwd, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(st, 6);
        if (profile_name)
            sqlite3_bind_text(st, 7, profile_name, -1, SQLITE_TRANSIENT);
        else sqlite3_bind_null(st, 7);
        sqlite3_bind_double(st, 8, now);
        sqlite3_bind_text(st, 9, parent_session_id, -1, SQLITE_TRANSIENT);
        int step = sqlite3_step(st);
        sqlite3_finalize(st);
        if (step != SQLITE_DONE) { rc = -6; goto rollback; }
    }

    /* insert compacted handoff messages + update child counts */
    {
        int tool_calls = 0;
        int n = insert_handoff_messages(db, child_session_id, messages_json,
                                        &tool_calls);
        if (n < 0) { rc = -7; goto rollback; }
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->db,
                "UPDATE sessions SET message_count = ?, tool_call_count = ? "
                "WHERE id = ?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_int(st, 1, n);
            sqlite3_bind_int(st, 2, tool_calls);
            sqlite3_bind_text(st, 3, child_session_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
    }

    /* close the parent — must transition exactly one live row */
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->db,
                "UPDATE sessions SET ended_at = ?, end_reason = 'compression' "
                "WHERE id = ? AND ended_at IS NULL",
                -1, &st, NULL) != SQLITE_OK) { rc = -8; goto rollback; }
        sqlite3_bind_double(st, 1, now);
        sqlite3_bind_text(st, 2, parent_session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
        if (sqlite3_changes(db->db) != 1) { rc = -8; goto rollback; }
    }

    sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
    return 0;

rollback:
    sqlite3_exec(db->db, "ROLLBACK", NULL, NULL, NULL);
    return rc;
}
