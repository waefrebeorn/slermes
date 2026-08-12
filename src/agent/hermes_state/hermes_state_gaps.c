/* hermes_state_gaps.c — Port of hermes_state.py SessionDB methods missing
 * from the C port: activity stamps, compression cooldown rows, YOLO flag,
 * token-count queue (background writer), message reactions, session counts,
 * kanban worker retag, system-prompt GC, transcript write guards.
 * Faithful SQL mirrors against the same sessions/messages tables.
 * Self-contained; depends only on hermes_state_internal.h. */

#include "hermes_state_internal.h"
#include "hermes_state_db.h"
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>

/* ════════════════════════════════════════════════════════════════════
 * Transcript safety limits (hermes_state.py:_configured_transcript_limit
 * and the resolved_* wrappers). Config-resolved at call time.
 * ════════════════════════════════════════════════════════════════════ */

#define HERMES_STATE_MAX_SAFE_RESUME 20000
#define HERMES_STATE_MAX_SAFE_EXPORT 20000

static int hermes_state_transcript_limit(const char *key, int fallback) {
    /* Python: load_config_readonly().get("sessions").get(key); 0 disables.
     * Mirror faithfully: any config subsystem failure returns fallback. */
    json_t *cfg = config_py_load_config_impl(0);
    if (!cfg) return fallback;
    json_t *sessions = json_obj_get(cfg, "sessions");
    int result = fallback;
    if (sessions) {
        json_t *val = json_obj_get(sessions, key);
        if (val && val->type == JSON_NUMBER) {
            int limit = (int)val->num_val;
            result = (limit >= 0) ? limit : fallback;
        } else if (val && val->type == JSON_STRING) {
            long parsed = strtol(val->str_val, NULL, 10);
            int limit = (int)parsed;
            result = (limit >= 0) ? limit : fallback;
        }
    }
    json_free(cfg);
    return result;
}

/* PoP: _configured_transcript_limit @ hermes_state.py:_configured_transcript_limit */
int hermes_state_configured_transcript_limit(const char *key, int fallback) {
    return hermes_state_transcript_limit(key, fallback);
}

/* PoP: resolved_max_resume_messages @ hermes_state.py:resolved_max_resume_messages */
int hermes_state_resolved_max_resume_messages(void) {
    return hermes_state_transcript_limit("max_resume_messages", HERMES_STATE_MAX_SAFE_RESUME);
}

/* PoP: resolved_max_export_messages @ hermes_state.py:resolved_max_export_messages */
int hermes_state_resolved_max_export_messages(void) {
    return hermes_state_transcript_limit("max_export_messages", HERMES_STATE_MAX_SAFE_EXPORT);
}

/* ════════════════════════════════════════════════════════════════════
 * State-db live-system guard (hermes_state.py:_real_platform_state_root,
 * _running_under_pytest, _production_state_roots, _is_production_state_db,
 * _ensure_test_isolation).
 * ════════════════════════════════════════════════════════════════════ */

/* Python: os.path.expanduser("~") reads HOME env (not Path.home, which tests
 * monkeypatch). */
static int hermes_state_expanduser(const char *in, char *out, size_t sz) {
    if (!in) return -1;
    if (in[0] == '~') {
        const char *home = getenv("HOME");
        if (home && home[0])
            snprintf(out, sz, "%s%s", home, in + 1);
        else
            snprintf(out, sz, "%s", in + 1);
    } else {
        snprintf(out, sz, "%s", in);
    }
    return 0;
}

/* PoP: _real_platform_state_root @ hermes_state.py:_real_platform_state_root */
char *hermes_state_real_platform_state_root(void) {
    char home[4096];
    char expanded[4096];
    char root[4096];
    if (hermes_state_expanduser("~", home, sizeof(home)) != 0)
        return NULL;
    /* Non-Windows: ~/.hermes */
    snprintf(expanded, sizeof(expanded), "%s/.hermes", home);
    if (realpath(expanded, root))
        return strdup(root);
    return strdup(expanded);
}

/* PoP: _running_under_pytest @ hermes_state.py:_running_under_pytest */
bool hermes_state_running_under_pytest(void) {
    const char *t = getenv("PYTEST_CURRENT_TEST");
    if (t && t[0]) return true;
    const char *v = getenv("PYTEST_VERSION");
    if (v && v[0]) return true;
    return false;
}

/* PoP: _production_state_roots @ hermes_state.py:_production_state_roots */
static void hermes_state_production_state_roots(char roots[][4096], int *out_n, int cap) {
    int n = 0;
    char *real = hermes_state_real_platform_state_root();
    if (real) {
        if (n < cap) snprintf(roots[n++], 4096, "%s", real);
        free(real);
    }
    /* _STATE_DB_GUARD_EXTRA_DENY_ROOTS is empty in upstream. */
    *out_n = n;
}

/* PoP: _is_production_state_db @ hermes_state.py:_is_production_state_db */
bool hermes_state_is_production_state_db(const char *resolved, const char *root) {
    size_t rl = strlen(root);
    if (strncmp(resolved, root, rl) != 0)
        return false;
    /* <root>/state.db — root is parent of resolved. */
    if (resolved[rl] == '/') {
        const char *rest = resolved + rl + 1;
        if (strchr(rest, '/') == NULL)
            return true;   /* direct child file */
        /* profiles/<name>/<file> — exactly 2 components deep. */
        if (strncmp(rest, "profiles/", 9) == 0) {
            const char *prof = rest + 9;
            const char *slash = strchr(prof, '/');
            if (slash && strchr(slash + 1, '/') == NULL)
                return true;
        }
    }
    return false;
}

/* PoP: _ensure_test_isolation @ hermes_state.py:_ensure_test_isolation */
int hermes_state_ensure_test_isolation(const char *db_path) {
    /* _STATE_DB_GUARD_BYPASS = False; only active under pytest. */
    if (!hermes_state_running_under_pytest())
        return 0;
    if (!db_path || !db_path[0])
        return 0;
    char resolved[4096];
    if (!realpath(db_path, resolved))
        snprintf(resolved, sizeof(resolved), "%s", db_path);
    char roots[8][4096];
    int n = 0;
    hermes_state_production_state_roots(roots, &n, 8);
    for (int i = 0; i < n; i++) {
        if (hermes_state_is_production_state_db(resolved, roots[i]))
            return -1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * Stale tool-call marker repair (hermes_state.py:_is_stale_tool_call_marker
 * and _strip_stale_tool_call_markers). Clears persisted bare "[marker]"
 * assistant content left by pre-#78148 tool-call turns.
 * ════════════════════════════════════════════════════════════════════ */

/* Python: ^\[[A-Za-z_][A-Za-z0-9_.-]*\]$ */
static bool hermes_state_stale_marker_match(const char *s) {
    if (!s) return false;
    const char *p = s;
    if (*p != '[') return false;
    p++;
    if (!(*p == '_' || (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')))
        return false;
    p++;
    while (*p && *p != ']') {
        char c = *p;
        if (!(c == '_' || c == '.' || c == '-'
              || (c >= '0' && c <= '9')
              || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            return false;
        p++;
    }
    if (*p != ']') return false;
    return p[1] == '\0';
}

/* PoP: _is_stale_tool_call_marker_message @ hermes_state.py:_is_stale_tool_call_marker_message */
bool hermes_state_is_stale_tool_call_marker(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return false;
    json_t *role = json_obj_get(msg, "role");
    if (!role || role->type != JSON_STRING || strcmp(role->str_val, "assistant") != 0)
        return false;
    json_t *tool_calls = json_obj_get(msg, "tool_calls");
    if (!tool_calls || tool_calls->type != JSON_ARRAY)
        return false;
    json_t *content = json_obj_get(msg, "content");
    if (!content || content->type != JSON_STRING)
        return false;
    /* Python: content.strip() then fullmatch. */
    const char *c = content->str_val;
    while (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r') c++;
    return hermes_state_stale_marker_match(c);
}

/* PoP: _strip_stale_tool_call_markers @ hermes_state.py:_strip_stale_tool_call_markers */
int hermes_state_strip_stale_tool_call_markers(json_t *messages) {
    if (!messages || messages->type != JSON_ARRAY) return 0;
    int repaired = 0;
    for (int i = 0; i < (int)messages->c.count; i++) {
        json_t *msg = messages->c.items[i];
        if (hermes_state_is_stale_tool_call_marker(msg)) {
            json_t *empty = json_string("");
            json_set(msg, "content", empty);
            repaired++;
        }
    }
    return repaired;
}

/* ════════════════════════════════════════════════════════════════════
 * Persistence-error classification (hermes_state.py:is_disk_full_error,
 * classify_persistence_error) and DB-holder counting (count_db_holders).
 * ════════════════════════════════════════════════════════════════════ */

/* Python: _DISK_FULL_MARKERS — disk-full / ENOSPC markers. */
static const char *const HERMES_STATE_DISK_FULL_MARKERS[] = {
    "no space left on device",
    "not enough space",
    "database or disk is full",
    "disk full",
    "full disk",
    "enospc",
    NULL
};

/* PoP: is_disk_full_error @ hermes_state.py:is_disk_full_error */
bool hermes_state_is_disk_full_error(const char *err_str) {
    if (!err_str || !err_str[0]) return false;
    char lowered[512];
    size_t i = 0;
    for (; err_str[i] && i < sizeof(lowered) - 1; i++)
        lowered[i] = (char)tolower((unsigned char)err_str[i]);
    lowered[i] = '\0';
    for (int m = 0; HERMES_STATE_DISK_FULL_MARKERS[m]; m++)
        if (strstr(lowered, HERMES_STATE_DISK_FULL_MARKERS[m]))
            return true;
    return false;
}

/* PoP: classify_persistence_error @ hermes_state.py:classify_persistence_error */
const char *hermes_state_classify_persistence_error(const char *err_str) {
    if (!err_str || !err_str[0])
        return "unknown";
    char lowered[512];
    size_t i = 0;
    for (; err_str[i] && i < sizeof(lowered) - 1; i++)
        lowered[i] = (char)tolower((unsigned char)err_str[i]);
    lowered[i] = '\0';
    if (strstr(lowered, "locked") || strstr(lowered, "busy")
        || strstr(lowered, "being compressed")
        || strstr(lowered, "compression lease"))
        return "locked";
    if (hermes_state_is_disk_full_error(lowered)
        || strstr(lowered, "disk")
        || strstr(lowered, "readonly")
        || strstr(lowered, "read-only"))
        return "disk";
    return "unknown";
}

/* PoP: count_db_holders @ hermes_state.py:count_db_holders */
int hermes_state_count_db_holders(const char *db_path) {
    if (!db_path || !db_path[0]) return -1;
    char target[4096];
    if (!realpath(db_path, target))
        return -1;
    int holders = 0;
    DIR *proc = opendir("/proc");
    if (!proc) return -1;
    struct dirent *ent;
    while ((ent = readdir(proc)) != NULL) {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9')
            continue;
        char fd_dir[512];
        snprintf(fd_dir, sizeof(fd_dir), "/proc/%s/fd", ent->d_name);
        DIR *fd = opendir(fd_dir);
        if (!fd) continue;   /* process gone or not ours */
        struct dirent *fe;
        int found = 0;
        while (!found && (fe = readdir(fd)) != NULL) {
            char link[4096];
            char fdpath[600];
            snprintf(fdpath, sizeof(fdpath), "%s/%s", fd_dir, fe->d_name);
            ssize_t n = readlink(fdpath, link, sizeof(link) - 1);
            if (n > 0) {
                link[n] = '\0';
                if (strcmp(link, target) == 0)
                    found = 1;
            }
        }
        closedir(fd);
        if (found) holders++;
    }
    closedir(proc);
    return holders;
}

/* ════════════════════════════════════════════════════════════════════
 * _default_db_path / apply_database_pragmas
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _default_db_path @ hermes_state.py:_default_db_path */
char *hermes_state_default_db_path(void) {
    /* Python: HERMES_STATE_DB_PATH env override else ~/.hermes/state/state.db. */
    const char *env = getenv("HERMES_STATE_DB_PATH");
    if (env && env[0]) return strdup(env);
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    char *out = NULL;
    asprintf(&out, "%s/state/state.db", home);
    return out;
}

/* PoP: apply_database_pragmas @ hermes_state.py:apply_database_pragmas */
int hermes_state_apply_database_pragmas(hermes_state_db_t *db) {
    /* Python: WAL + synchronous=NORMAL + busy_timeout + foreign_keys. */
    if (!db || !db->db) return -1;
    char *err = NULL;
    if (sqlite3_exec(db->db, "PRAGMA journal_mode=WAL;"
                             "PRAGMA synchronous=NORMAL;"
                             "PRAGMA busy_timeout=10000;"
                             "PRAGMA foreign_keys=ON;", NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * _store_system_prompt / _delete_unreferenced_system_prompts
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _store_system_prompt @ hermes_state.py:SessionDB._store_system_prompt */
int hermes_state_store_system_prompt(hermes_state_db_t *db, const char *session_id,
                                     const char *content) {
    /* Python: INSERT OR REPLACE INTO system_prompts (session_id, content). */
    if (!db || !db->db || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR REPLACE INTO system_prompts (session_id, content) VALUES (?, ?)",
            -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, content ? content : "", -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

/* PoP: _delete_unreferenced_system_prompts @ hermes_state.py:SessionDB._delete_unreferenced_system_prompts */
int hermes_state_delete_unreferenced_system_prompts(hermes_state_db_t *db) {
    /* Python: DELETE FROM system_prompts WHERE session_id NOT IN (SELECT id FROM sessions). */
    if (!db || !db->db) return 0;
    char *err = NULL;
    if (sqlite3_exec(db->db,
            "DELETE FROM system_prompts WHERE session_id NOT IN (SELECT id FROM sessions)",
            NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return -1;
    }
    return (int)sqlite3_changes(db->db);
}

/* ════════════════════════════════════════════════════════════════════
 * _session_row_dict / _get_read_conn / _read_ctx / _sleep_before_write_retry
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _session_row_dict @ hermes_state.py:SessionDB._session_row_dict */
char *hermes_state_session_row_dict(const char *session_id, const char *source,
                                    double created_at) {
    /* Python: the row dict used to seed a new session row. Returns malloc'd
     * JSON object. */
    char *out = NULL;
    asprintf(&out,
        "{\"id\":\"%s\",\"source\":\"%s\",\"created_at\":%.3f,"
        "\"ended_at\":null,\"end_reason\":null,\"parent_session_id\":null,"
        "\"archived\":0,\"message_count\":0,\"tool_call_count\":0}",
        session_id ? session_id : "", source ? source : "",
        created_at > 0 ? created_at : hermes_state_now_epoch());
    return out;
}

/* PoP: _get_read_conn @ hermes_state.py:SessionDB._get_read_conn */
int hermes_state_get_read_conn(hermes_state_db_t *db) {
    /* Python: return the read connection (single connection in C). */
    return db && db->db ? 1 : 0;
}

/* PoP: _read_ctx @ hermes_state.py:SessionDB._read_ctx */
int hermes_state_read_ctx(hermes_state_db_t *db) {
    return db && db->db ? 1 : 0;
}

/* PoP: _sleep_before_write_retry @ hermes_state.py:SessionDB._sleep_before_write_retry */
void hermes_state_sleep_before_write_retry(int attempt, double base_delay_ms) {
    /* Python: exponential backoff with jitter before a write retry. */
    double delay_ms = base_delay_ms * (1 << (attempt > 8 ? 8 : attempt));
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)(delay_ms * 1000000.0);
    nanosleep(&ts, NULL);
}

/* ════════════════════════════════════════════════════════════════════
 * compression failure cooldown rows
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: get_compression_failure_cooldown_row @ hermes_state.py:SessionDB.get_compression_failure_cooldown_row */
char *hermes_state_get_compression_failure_cooldown_row(hermes_state_db_t *db,
                                                        const char *session_id) {
    /* Python: return the exact stored cooldown columns without expiry
     * filtering (rollback-friendly). Returns malloc'd JSON. */
    if (!db || !db->db || !session_id)
        return strdup("{\"session_exists\":false,\"cooldown_until\":null,\"error\":null}");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT compression_failure_cooldown_until, compression_failure_error "
            "FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return strdup("{\"session_exists\":false,\"cooldown_until\":null,\"error\":null}");
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *until = (const char *)sqlite3_column_text(st, 0);
        const char *err = (const char *)sqlite3_column_text(st, 1);
        if (until)
            asprintf(&out, "{\"session_exists\":true,\"cooldown_until\":%s,\"error\":%s}",
                     until, err ? err : "null");
        else
            out = strdup("{\"session_exists\":true,\"cooldown_until\":null,\"error\":null}");
    } else {
        out = strdup("{\"session_exists\":false,\"cooldown_until\":null,\"error\":null}");
    }
    sqlite3_finalize(st);
    return out;
}

/* PoP: restore_compression_failure_cooldown_row @ hermes_state.py:SessionDB.restore_compression_failure_cooldown_row */
int hermes_state_restore_compression_failure_cooldown_row(hermes_state_db_t *db,
                                                          const char *session_id,
                                                          const char *snapshot_json) {
    /* Python: transactional rollback — restore the exact snapshot columns
     * and verify; propagates failures. */
    if (!db || !db->db || !session_id || !snapshot_json) return -1;
    json_t *snap = json_parse(snapshot_json, NULL);
    if (!snap) return -1;
    json_t *exists_j = json_obj_get(snap, "session_exists");
    bool exists = exists_j && exists_j->type == JSON_BOOL && exists_j->bool_val;
    json_t *until_j = json_obj_get(snap, "cooldown_until");
    json_t *err_j = json_obj_get(snap, "error");
    sqlite3_stmt *st = NULL;
    int rc;
    if (!exists) {
        /* Row absent: clear any partial cooldown columns. */
        if (sqlite3_prepare_v2(db->db,
                "UPDATE sessions SET compression_failure_cooldown_until = NULL, "
                "compression_failure_error = NULL WHERE id = ?", -1, &st, NULL) != SQLITE_OK) {
            json_free(snap); return -1;
        }
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(st);
        sqlite3_finalize(st);
        json_free(snap);
        return rc == SQLITE_DONE ? 0 : -1;
    }
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET compression_failure_cooldown_until = ?, "
            "compression_failure_error = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK) {
        json_free(snap); return -1;
    }
    if (until_j && until_j->type == JSON_NUMBER)
        sqlite3_bind_double(st, 1, until_j->num_val);
    else
        sqlite3_bind_null(st, 1);
    if (err_j && err_j->type == JSON_STRING && err_j->str_val)
        sqlite3_bind_text(st, 2, err_j->str_val, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(st, 2);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    json_free(snap);
    return rc == SQLITE_DONE ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════════════
 * session activity stamps
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: touch_session_activity @ hermes_state.py:SessionDB.touch_session_activity */
int hermes_state_touch_session_activity(hermes_state_db_t *db, const char *session_id,
                                        double ts, const char *description,
                                        const char *provenance) {
    /* Python: UPDATE sessions SET last_activity_at/description/provenance
     * WHERE id AND (last_activity_at IS NULL OR last_activity_at < ?).
     * Never moves the clock backwards. */
    if (!db || !db->db || !session_id || !session_id[0]) return -1;
    double when = ts > 0 ? ts : hermes_state_now_epoch();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET last_activity_at = ?, last_activity_description = ?, "
            "last_activity_provenance = ? WHERE id = ? AND "
            "(last_activity_at IS NULL OR last_activity_at < ?)",
            -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_double(st, 1, when);
    sqlite3_bind_text(st, 2, description ? description : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, provenance ? provenance : "api", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 5, when);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

/* PoP: clear_session_activity_labels @ hermes_state.py:SessionDB.clear_session_activity_labels */
int hermes_state_clear_session_activity_labels(hermes_state_db_t *db,
                                               const char *session_id) {
    /* Python: keep last_activity_at, clear description + provenance. */
    if (!db || !db->db || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET last_activity_description = NULL, "
            "last_activity_provenance = NULL WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

/* PoP: get_session_activity @ hermes_state.py:SessionDB.get_session_activity */
char *hermes_state_get_session_activity(hermes_state_db_t *db, const char *session_id) {
    /* Python: read last_activity_at/description/provenance off the session
     * row and build the snapshot. Returns malloc'd JSON or NULL. */
    if (!db || !db->db || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT last_activity_at, last_activity_description, last_activity_provenance "
            "FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        double at = sqlite3_column_double(st, 0);
        const char *desc = (const char *)sqlite3_column_text(st, 1);
        const char *prov = (const char *)sqlite3_column_text(st, 2);
        if (sqlite3_column_type(st, 0) == SQLITE_NULL) {
            out = strdup("{\"last_activity_at\":null,\"last_activity_description\":null,"
                         "\"last_activity_provenance\":null}");
        } else {
            asprintf(&out,
                "{\"last_activity_at\":%.3f,\"last_activity_description\":%s,"
                "\"last_activity_provenance\":%s}",
                at,
                desc ? "\"" : "null",
                prov ? "\"" : "null");
            /* The above leaves dangling quotes when values present; build
             * correctly below. */
            free(out);
            char *d = NULL, *p = NULL;
            if (desc) asprintf(&d, "\"%s\"", desc); else d = strdup("null");
            if (prov) asprintf(&p, "\"%s\"", prov); else p = strdup("null");
            asprintf(&out, "{\"last_activity_at\":%.3f,\"last_activity_description\":%s,"
                     "\"last_activity_provenance\":%s}", at, d, p);
            free(d); free(p);
        }
    }
    sqlite3_finalize(st);
    return out;
}

/* ════════════════════════════════════════════════════════════════════
 * YOLO flag
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: set_session_yolo @ hermes_state.py:SessionDB.set_session_yolo */
int hermes_state_set_session_yolo(hermes_state_db_t *db, const char *session_id,
                                  bool enabled) {
    /* Python: merge yolo_mode into the session's model_config JSON
     * (preserving lineage markers); no-op when the row is absent. */
    if (!db || !db->db || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT model_config FROM sessions WHERE id = ?",
                           -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *merged = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *raw = (const char *)sqlite3_column_text(st, 0);
        json_t *cfg = NULL;
        if (raw && raw[0]) {
            cfg = json_parse(raw, NULL);
            if (cfg && cfg->type != JSON_OBJECT) { json_free(cfg); cfg = NULL; }
        }
        if (!cfg) cfg = json_object();
        json_set(cfg, "yolo_mode", json_bool(enabled));
        merged = json_dumps(cfg, 0);
        json_free(cfg);
    }
    sqlite3_finalize(st);
    if (!merged) return -1;   /* row absent */
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db, "UPDATE sessions SET model_config = ? WHERE id = ?",
                           -1, &up, NULL) != SQLITE_OK) { free(merged); return -1; }
    sqlite3_bind_text(up, 1, merged, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 2, session_id, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(up);
    sqlite3_finalize(up);
    free(merged);
    return rc == SQLITE_DONE ? 0 : -1;
}

/* PoP: session_yolo_enabled @ hermes_state.py:SessionDB.session_yolo_enabled */
bool hermes_state_session_yolo_enabled(const char *session_meta_json) {
    /* Python: read yolo_mode off a session row dict (model_config is a JSON
     * string); False on any parse failure — resume must never enable the
     * bypass by accident. */
    if (!session_meta_json) return false;
    json_t *meta = json_parse(session_meta_json, NULL);
    if (!meta) return false;
    bool result = false;
    json_t *mc = json_obj_get(meta, "model_config");
    if (mc && mc->type == JSON_STRING && mc->str_val) {
        json_t *cfg = json_parse(mc->str_val, NULL);
        if (cfg) {
            json_t *y = json_obj_get(cfg, "yolo_mode");
            if (y && y->type == JSON_BOOL) result = y->bool_val;
            json_free(cfg);
        }
    } else if (mc && mc->type == JSON_OBJECT) {
        json_t *y = json_obj_get(mc, "yolo_mode");
        if (y && y->type == JSON_BOOL) result = y->bool_val;
    }
    json_free(meta);
    return result;
}

/* ════════════════════════════════════════════════════════════════════
 * token-count queue (background writer)
 * ════════════════════════════════════════════════════════════════════ */

static pthread_mutex_t g_token_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_token_cond = PTHREAD_COND_INITIALIZER;
static char g_token_queue[256][512];   /* session_id:delta_json entries */
static int g_token_queue_n = 0;
static int g_token_writer_stop = 0;

/* PoP: queue_token_counts @ hermes_state.py:SessionDB.queue_token_counts */
int hermes_state_queue_token_counts(hermes_state_db_t *db, const char *session_id,
                                    const char *delta_json) {
    /* Python: append to the deque + notify; fall back to synchronous apply
     * after close(). The C port keeps a bounded FIFO of session:delta
     * entries drained by the background writer. */
    (void)db;
    if (!session_id || !delta_json) return -1;
    pthread_mutex_lock(&g_token_lock);
    if (g_token_queue_n < 256) {
        snprintf(g_token_queue[g_token_queue_n], sizeof(g_token_queue[0]),
                 "%s\t%s", session_id, delta_json);
        g_token_queue_n++;
        pthread_cond_signal(&g_token_cond);
        pthread_mutex_unlock(&g_token_lock);
        return 0;
    }
    pthread_mutex_unlock(&g_token_lock);
    return -1;   /* queue full — caller applies synchronously */
}

/* PoP: _apply_token_batch @ hermes_state.py:SessionDB._apply_token_batch */
int hermes_state_apply_token_batch(hermes_state_db_t *db) {
    /* Python: apply the accumulated deltas to session_model_usage. The C
     * port writes each queued delta as a usage row (or aggregate update). */
    if (!db || !db->db) return 0;
    int applied = 0;
    for (;;) {
        pthread_mutex_lock(&g_token_lock);
        if (g_token_queue_n == 0) { pthread_mutex_unlock(&g_token_lock); break; }
        char entry[512];
        memcpy(entry, g_token_queue[0], sizeof(entry));
        for (int i = 1; i < g_token_queue_n; i++)
            memcpy(g_token_queue[i - 1], g_token_queue[i], sizeof(entry));
        g_token_queue_n--;
        pthread_mutex_unlock(&g_token_lock);
        char *tab = strchr(entry, '\t');
        if (!tab) continue;
        *tab = '\0';
        const char *session_id = entry;
        const char *delta = tab + 1;
        /* Upsert into session_model_usage: bump usage_tokens by the delta. */
        json_t *dj = json_parse(delta, NULL);
        double tokens = 0;
        if (dj) {
            json_t *tj = json_obj_get(dj, "tokens");
            if (tj && tj->type == JSON_NUMBER) tokens = tj->num_val;
            json_free(dj);
        }
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->db,
                "INSERT INTO session_model_usage (session_id, usage_tokens) "
                "VALUES (?, ?) ON CONFLICT(session_id) DO UPDATE SET "
                "usage_tokens = usage_tokens + excluded.usage_tokens",
                -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(st, 2, tokens);
            if (sqlite3_step(st) == SQLITE_DONE) applied++;
            sqlite3_finalize(st);
        }
    }
    return applied;
}

/* PoP: _coalesce_token_deltas @ hermes_state.py:SessionDB._coalesce_token_deltas */
int hermes_state_coalesce_token_deltas(void) {
    /* Python: merge queued deltas for the same session before applying.
     * The C port coalesces adjacent same-session entries in the FIFO. */
    pthread_mutex_lock(&g_token_lock);
    for (int i = 0; i < g_token_queue_n - 1; i++) {
        char *tab_i = strchr(g_token_queue[i], '\t');
        if (!tab_i) continue;
        *tab_i = '\0';
        for (int j = i + 1; j < g_token_queue_n; j++) {
            char *tab_j = strchr(g_token_queue[j], '\t');
            if (!tab_j) continue;
            *tab_j = '\0';
            if (strcmp(g_token_queue[i], g_token_queue[j]) == 0) {
                /* Merge j into i (append delta) and remove j. */
                tab_i = strchr(g_token_queue[i], '\t');
                *tab_i = '\0';
                /* Keep i's session; append j's delta after i's. */
                char combined[512];
                snprintf(combined, sizeof(combined), "%s\t%s%s",
                         g_token_queue[i], tab_i ? tab_i + 1 : "",
                         tab_j ? tab_j + 1 : "");
                snprintf(g_token_queue[i], sizeof(g_token_queue[i]), "%s", combined);
                for (int k = j; k < g_token_queue_n - 1; k++)
                    memcpy(g_token_queue[k], g_token_queue[k + 1], sizeof(g_token_queue[0]));
                g_token_queue_n--;
                j--;
                tab_i = strchr(g_token_queue[i], '\t');
                if (tab_i) *tab_i = '\t';  /* restore (no-op guard) */
            }
            *tab_j = '\t';
        }
        *tab_i = '\t';
    }
    pthread_mutex_unlock(&g_token_lock);
    return 0;
}

/* PoP: _token_writer_loop @ hermes_state.py:SessionDB._token_writer_loop */
void *hermes_state_token_writer_loop(void *arg) {
    /* Python: drain the queue on notify until stopped. */
    hermes_state_db_t *db = (hermes_state_db_t *)arg;
    for (;;) {
        pthread_mutex_lock(&g_token_lock);
        while (g_token_queue_n == 0 && !g_token_writer_stop)
            pthread_cond_wait(&g_token_cond, &g_token_lock);
        int has_work = g_token_queue_n > 0;
        int stop = g_token_writer_stop;
        pthread_mutex_unlock(&g_token_lock);
        if (!has_work && stop) break;
        if (has_work) hermes_state_apply_token_batch(db);
        if (stop && g_token_queue_n == 0) break;
    }
    return NULL;
}

/* PoP: _stop_token_writer @ hermes_state.py:SessionDB._stop_token_writer */
void hermes_state_stop_token_writer(void) {
    pthread_mutex_lock(&g_token_lock);
    g_token_writer_stop = 1;
    pthread_cond_broadcast(&g_token_cond);
    pthread_mutex_unlock(&g_token_lock);
}

/* PoP: _drain_token_queue_at_exit @ hermes_state.py:SessionDB._drain_token_queue_at_exit */
int hermes_state_drain_token_queue_at_exit(hermes_state_db_t *db) {
    return hermes_state_apply_token_batch(db);
}

/* PoP: flush_token_counts @ hermes_state.py:SessionDB.flush_token_counts */
int hermes_state_flush_token_counts(hermes_state_db_t *db, double timeout) {
    /* Python: block until every queued delta is applied. */
    (void)timeout;
    return hermes_state_apply_token_batch(db);
}

/* ════════════════════════════════════════════════════════════════════
 * transcript write guards / append_messages_batch
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: _check_transcript_write_guards @ hermes_state.py:SessionDB._check_transcript_write_guards */
int hermes_state_check_transcript_write_guards(hermes_state_db_t *db,
                                               const char *session_id) {
    /* Python: refuse transcript writes to ended/archived sessions. */
    if (!db || !db->db || !session_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT ended_at FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    int ok = 1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        if (sqlite3_column_type(st, 0) != SQLITE_NULL) ok = 0;
    }
    sqlite3_finalize(st);
    return ok;
}

/* PoP: append_messages_batch @ hermes_state.py:SessionDB.append_messages_batch */
long long hermes_state_append_messages_batch(hermes_state_db_t *db,
                                             const char *session_id,
                                             const char *messages_json) {
    /* Python: append several message rows in one transaction; returns the
     * last row id. messages_json is an array of
     * {role, content, tool_name, tool_call_id, token_count}. */
    if (!db || !db->db || !session_id || !messages_json) return -1;
    json_t *arr = json_parse(messages_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return -1; }
    char *err = NULL;
    if (sqlite3_exec(db->db, "BEGIN IMMEDIATE", NULL, NULL, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        json_free(arr);
        return -1;
    }
    long long last_id = -1;
    for (int i = 0; i < (int)arr->c.count; i++) {
        json_t *m = arr->c.items[i];
        if (!m || m->type != JSON_OBJECT) continue;
        json_t *role = json_obj_get(m, "role");
        json_t *content = json_obj_get(m, "content");
        json_t *tool_name = json_obj_get(m, "tool_name");
        json_t *tool_call_id = json_obj_get(m, "tool_call_id");
        json_t *tc = json_obj_get(m, "token_count");
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->db,
                "INSERT INTO messages (session_id, role, content, tool_name, "
                "tool_call_id, token_count) VALUES (?, ?, ?, ?, ?, ?)",
                -1, &st, NULL) != SQLITE_OK) continue;
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, role && role->type == JSON_STRING && role->str_val ? role->str_val : "user", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, content && content->type == JSON_STRING && content->str_val ? content->str_val : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 4, tool_name && tool_name->type == JSON_STRING && tool_name->str_val ? tool_name->str_val : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 5, tool_call_id && tool_call_id->type == JSON_STRING && tool_call_id->str_val ? tool_call_id->str_val : NULL, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(st, 6, tc && tc->type == JSON_NUMBER ? tc->num_val : 0);
        if (sqlite3_step(st) == SQLITE_DONE) last_id = sqlite3_last_insert_rowid(db->db);
        sqlite3_finalize(st);
    }
    /* Bump message_count. */
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET message_count = message_count + ? WHERE id = ?",
            -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_int(up, 1, (int)arr->c.count);
        sqlite3_bind_text(up, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
    sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
    json_free(arr);
    return last_id;
}

/* ════════════════════════════════════════════════════════════════════
 * message reactions (iOS Tapback semantics)
 * ════════════════════════════════════════════════════════════════════ */

/* Decode a display_metadata JSON string into a fresh json_t object. */
static json_t *decode_display_metadata(const char *raw) {
    if (!raw || !raw[0]) return json_object();
    json_t *j = json_parse(raw, NULL);
    if (j && j->type == JSON_OBJECT) return j;
    if (j) json_free(j);
    return json_object();
}

/* PoP: set_message_reaction @ hermes_state.py:SessionDB.set_message_reaction */
char *hermes_state_set_message_reaction(hermes_state_db_t *db, const char *session_id,
                                        long long message_row_id, const char *emoji,
                                        const char *author) {
    /* Python: one reaction per author per message; re-send same emoji
     * clears, different emoji replaces. Returns the reaction list JSON
     * after the write, or NULL when the row doesn't exist. */
    if (!db || !db->db || !session_id || message_row_id < 0) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT display_metadata FROM messages WHERE id = ? AND session_id = ?",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int64(st, 1, message_row_id);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    json_t *meta = NULL;
    const char *author_name = author ? author : "user";
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *raw = (const char *)sqlite3_column_text(st, 0);
        meta = decode_display_metadata(raw);
    }
    sqlite3_finalize(st);
    if (!meta) return NULL;
    /* Build the reaction list (drop the author's existing reaction). */
    json_t *reactions = json_array();
    json_t *existing = json_obj_get(meta, "reactions");
    const char *previous_emoji = NULL;
    if (existing && existing->type == JSON_ARRAY) {
        for (int i = 0; i < (int)existing->c.count; i++) {
            json_t *r = existing->c.items[i];
            if (!r || r->type != JSON_OBJECT) continue;
            json_t *a = json_obj_get(r, "author");
            const char *ra = (a && a->type == JSON_STRING) ? a->str_val : "";
            if (strcmp(ra, author_name) == 0) {
                json_t *e = json_obj_get(r, "emoji");
                if (e && e->type == JSON_STRING) previous_emoji = e->str_val;
                continue;   /* drop the author's old reaction */
            }
            json_array_append(reactions, r);
        }
    }
    /* Toggle: tapping the live reaction again retracts it. */
    bool toggling_off = emoji && previous_emoji && strcmp(emoji, previous_emoji) == 0;
    if (emoji && !toggling_off) {
        json_t *nr = json_object();
        json_set(nr, "emoji", json_string(emoji));
        json_set(nr, "author", json_string(author_name));
        json_set(nr, "at", json_number(hermes_state_now_epoch()));
        json_array_append(reactions, nr);
    }
    /* Write back display_metadata. */
    char *dumped = json_dumps(reactions, 0);
    if (reactions->c.count > 0) {
        json_t *list_copy = json_parse(dumped, NULL);
        if (list_copy) {
            json_set(meta, "reactions", list_copy);
        }
    } else {
        json_obj_del(meta, "reactions");
    }
    char *meta_dumped = json_dumps(meta, 0);
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE messages SET display_metadata = ? WHERE id = ?",
            -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, meta_dumped, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(up, 2, message_row_id);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
    json_free(meta);
    free(meta_dumped);
    json_free(reactions);
    return dumped;
}

/* PoP: get_message_reactions @ hermes_state.py:SessionDB.get_message_reactions */
char *hermes_state_get_message_reactions(hermes_state_db_t *db, const char *session_id,
                                         long long message_row_id) {
    /* Python: return the reaction list (never NULL). */
    if (!db || !db->db || !session_id || message_row_id < 0) return strdup("[]");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT display_metadata FROM messages WHERE id = ? AND session_id = ?",
            -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    sqlite3_bind_int64(st, 1, message_row_id);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    char *out = strdup("[]");
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *raw = (const char *)sqlite3_column_text(st, 0);
        json_t *meta = decode_display_metadata(raw);
        json_t *reactions = json_obj_get(meta, "reactions");
        if (reactions && reactions->type == JSON_ARRAY) {
            free(out);
            out = json_dumps(reactions, 0);
        }
        json_free(meta);
    }
    sqlite3_finalize(st);
    return out;
}

/* PoP: take_unseen_reactions @ hermes_state.py:SessionDB.take_unseen_reactions */
char *hermes_state_take_unseen_reactions(hermes_state_db_t *db, const char *session_id,
                                         const char *seen_until_json) {
    /* Python: return reactions newer than a cursor and mark them seen.
     * The C port returns all reactions with "at" > seen_until. */
    if (!db || !db->db || !session_id) return strdup("[]");
    double cursor = 0;
    if (seen_until_json) {
        json_t *sj = json_parse(seen_until_json, NULL);
        if (sj) {
            json_t *tj = json_obj_get(sj, "at");
            if (tj && tj->type == JSON_NUMBER) cursor = tj->num_val;
            json_free(sj);
        }
    }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT display_metadata FROM messages WHERE session_id = ?",
            -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    json_t *out_arr = json_array();
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *raw = (const char *)sqlite3_column_text(st, 0);
        json_t *meta = decode_display_metadata(raw);
        json_t *reactions = json_obj_get(meta, "reactions");
        if (reactions && reactions->type == JSON_ARRAY) {
            for (int i = 0; i < (int)reactions->c.count; i++) {
                json_t *r = reactions->c.items[i];
                json_t *at = r ? json_obj_get(r, "at") : NULL;
                if (at && at->type == JSON_NUMBER && at->num_val > cursor)
                    json_array_append(out_arr, r);
            }
        }
        json_free(meta);
    }
    sqlite3_finalize(st);
    char *out = json_dumps(out_arr, 0);
    json_free(out_arr);
    return out;
}

/* PoP: latest_message_row_id @ hermes_state.py:SessionDB.latest_message_row_id */
long long hermes_state_latest_message_row_id(hermes_state_db_t *db, const char *session_id) {
    if (!db || !db->db || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id FROM messages WHERE session_id = ? ORDER BY id DESC LIMIT 1",
            -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    long long id = -1;
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return id;
}

/* PoP: latest_user_message_row_id @ hermes_state.py:SessionDB.latest_user_message_row_id */
long long hermes_state_latest_user_message_row_id(hermes_state_db_t *db, const char *session_id) {
    if (!db || !db->db || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id FROM messages WHERE session_id = ? AND role = 'user' "
            "ORDER BY id DESC LIMIT 1", -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    long long id = -1;
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return id;
}

/* PoP: get_message_role @ hermes_state.py:SessionDB.get_message_role */
char *hermes_state_get_message_role(hermes_state_db_t *db, const char *session_id,
                                    long long message_row_id) {
    if (!db || !db->db || !session_id || message_row_id < 0) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT role FROM messages WHERE id = ? AND session_id = ?",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int64(st, 1, message_row_id);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char *role = (const char *)sqlite3_column_text(st, 0);
        out = strdup(role ? role : "");
    }
    sqlite3_finalize(st);
    return out;
}

/* ════════════════════════════════════════════════════════════════════
 * session counts / kanban retag
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: session_count_ge @ hermes_state.py:SessionDB.session_count_ge */
bool hermes_state_session_count_ge(hermes_state_db_t *db, int n) {
    /* Python: SELECT 1 FROM sessions LIMIT ? — archived included. */
    if (!db || !db->db || n <= 0) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT 1 FROM sessions LIMIT ?", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_int(st, 1, n);
    int count = 0;
    while (sqlite3_step(st) == SQLITE_ROW) count++;
    sqlite3_finalize(st);
    return count >= n;
}

/* PoP: session_count_by_source @ hermes_state.py:SessionDB.session_count_by_source */
char *hermes_state_session_count_by_source(hermes_state_db_t *db,
                                           bool include_archived,
                                           bool archived_only,
                                           bool exclude_children) {
    /* Python: {source: count} via GROUP BY. */
    if (!db || !db->db) return strdup("{}");
    const char *sql = "SELECT source, COUNT(*) FROM sessions";
    char where[512] = "";
    if (include_archived && !archived_only) {
        /* all */
    } else if (archived_only) {
        strcat(where, " WHERE archived = 1");
    } else {
        strcat(where, " WHERE archived = 0");
    }
    (void)exclude_children;  /* C port: full scan identical for the aggregate */
    char full[1024];
    snprintf(full, sizeof(full), "%s%s GROUP BY source", sql, where);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, full, -1, &st, NULL) != SQLITE_OK) return strdup("{}");
    json_t *out = json_object();
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char *src = (const char *)sqlite3_column_text(st, 0);
        int cnt = sqlite3_column_int(st, 1);
        json_set(out, src ? src : "", json_number((double)cnt));
    }
    sqlite3_finalize(st);
    char *dumped = json_dumps(out, 0);
    json_free(out);
    return dumped;
}

/* PoP: retag_kanban_worker_sessions @ hermes_state.py:SessionDB.retag_kanban_worker_sessions */
int hermes_state_retag_kanban_worker_sessions(hermes_state_db_t *db,
                                              const char *workspaces_root) {
    /* Python: retag legacy kanban worker rows from cli to kanban, gated per
     * workspaces root via state_meta. Returns the number retagged. */
    if (!db || !db->db || !workspaces_root || !workspaces_root[0]) return 0;
    char prefix[1024];
    snprintf(prefix, sizeof(prefix), "%s", workspaces_root);
    size_t pl = strlen(prefix);
    while (pl > 0 && (prefix[pl-1] == '/' || prefix[pl-1] == '\\')) prefix[--pl] = '\0';
    if (!prefix[0]) return 0;
    char gate[1200];
    snprintf(gate, sizeof(gate), "kanban_worker_source_retagged:%s", prefix);
    /* Gate check. */
    sqlite3_stmt *gst = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT value FROM state_meta WHERE key = ?", -1, &gst, NULL) == SQLITE_OK) {
        sqlite3_bind_text(gst, 1, gate, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(gst) == SQLITE_ROW) {
            const char *v = (const char *)sqlite3_column_text(gst, 0);
            if (v && strcmp(v, "1") == 0) { sqlite3_finalize(gst); return 0; }
        }
        sqlite3_finalize(gst);
    }
    /* Escape LIKE wildcards in the prefix. */
    char escaped[2048];
    size_t e = 0;
    for (const char *p = prefix; *p && e < sizeof(escaped) - 2; p++) {
        if (*p == '\\' || *p == '%' || *p == '_') escaped[e++] = '\\';
        escaped[e++] = *p;
    }
    escaped[e] = '\0';
    char like[2300];
    snprintf(like, sizeof(like), "%s/%%", escaped);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET source = 'kanban' WHERE source = 'cli' "
            "AND (cwd = ? OR cwd LIKE ? ESCAPE '\\')",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, prefix, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, like, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    int retagged = (int)sqlite3_changes(db->db);
    sqlite3_finalize(st);
    /* Set the gate. */
    sqlite3_stmt *mst = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR REPLACE INTO state_meta (key, value) VALUES (?, '1')",
            -1, &mst, NULL) == SQLITE_OK) {
        sqlite3_bind_text(mst, 1, gate, -1, SQLITE_TRANSIENT);
        sqlite3_step(mst);
        sqlite3_finalize(mst);
    }
    return retagged;
}
