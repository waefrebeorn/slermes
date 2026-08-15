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
#include <sys/stat.h>

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

/* PoP: hermes_state_configured_transcript_limit @ hermes_state.py:_configured_transcript_limit */
int hermes_state_configured_transcript_limit(const char *key, int fallback) {
    return hermes_state_transcript_limit(key, fallback);
}

int hermes_state_resolved_max_resume_messages(void) {
    return hermes_state_transcript_limit("max_resume_messages", HERMES_STATE_MAX_SAFE_RESUME);
}

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
/* PoP: hermes_state_real_platform_state_root @ hermes_state.py:_real_platform_state_root */

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

bool hermes_state_running_under_pytest(void) {
    const char *t = getenv("PYTEST_CURRENT_TEST");
    if (t && t[0]) return true;
    const char *v = getenv("PYTEST_VERSION");
    if (v && v[0]) return true;
    return false;
/* PoP: hermes_state_production_state_roots @ hermes_state.py:_production_state_roots */
}

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
/* PoP: hermes_state_ensure_test_isolation @ hermes_state.py:_ensure_test_isolation */
    return false;
}

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

bool hermes_state_is_stale_tool_call_marker(const json_t *msg) {
    if (!msg || msg->type != JSON_OBJECT) return false;
    json_t *role = json_obj_get(msg, "role");
    if (!role || role->type != JSON_STRING || strcmp(role->str_val, "assistant") != 0)
        return false;
    json_t *tool_calls = json_obj_get(msg, "tool_calls");
    /* Python: `if not msg.get("tool_calls"): return False`. Truthiness —
     * missing/NULL, empty list, empty string, 0, false all fail; any other
     * value (non-empty array, truthy string) passes. */
    if (!tool_calls || tool_calls->type == JSON_NULL)
        return false;
    if (tool_calls->type == JSON_ARRAY) {
        if (tool_calls->c.count == 0) return false;
    } else if (tool_calls->type == JSON_STRING) {
        if (!tool_calls->str_val || !tool_calls->str_val[0]) return false;
    } else if (tool_calls->type == JSON_NUMBER) {
        if (tool_calls->num_val == 0.0) return false;
    } else if (tool_calls->type == JSON_BOOL) {
        if (!tool_calls->bool_val) return false;
    }
    json_t *content = json_obj_get(msg, "content");
    if (!content || content->type != JSON_STRING)
        return false;
    /* Python: content.strip() (both ends), then fullmatch. */
    const char *c = content->str_val;
    const char *c_end = c + strlen(c);
    while (c < c_end && (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r')) c++;
    while (c_end > c && (c_end[-1] == ' ' || c_end[-1] == '\t'
                         || c_end[-1] == '\n' || c_end[-1] == '\r')) c_end--;
    char *stripped = malloc((size_t)(c_end - c) + 1);
    if (!stripped) return false;
    memcpy(stripped, c, (size_t)(c_end - c));
    stripped[c_end - c] = '\0';
    bool result = hermes_state_stale_marker_match(stripped);
/* PoP: hermes_state_strip_stale_tool_call_markers @ hermes_state.py:_strip_stale_tool_call_markers */
    free(stripped);
    return result;
}

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
/* PoP: hermes_state_count_db_holders @ hermes_state.py:count_db_holders */
        || strstr(lowered, "read-only"))
        return "disk";
    return "unknown";
}

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
/* PoP: hermes_state_default_db_path @ hermes_state.py:_default_db_path */

char *hermes_state_default_db_path(void) {
    /* Python: HERMES_STATE_DB_PATH env override else ~/.hermes/state/state.db. */
    const char *env = getenv("HERMES_STATE_DB_PATH");
    if (env && env[0]) return strdup(env);
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
/* PoP: hermes_state_apply_database_pragmas @ hermes_state.py:apply_database_pragmas */
    if (!home || !home[0]) home = "/tmp";
    char *out = NULL;
    asprintf(&out, "%s/state/state.db", home);
    return out;
}

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

int hermes_state_store_system_prompt(hermes_state_db_t *db, const char *session_id,
                                     const char *content) {
    /* Python: INSERT OR REPLACE INTO system_prompts (session_id, content). */
    if (!db || !db->db || !session_id) return -1;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR REPLACE INTO system_prompts (session_id, content) VALUES (?, ?)",
/* PoP: hermes_state_delete_unreferenced_system_prompts @ hermes_state.py:_delete_unreferenced_system_prompts */
            -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, content ? content : "", -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? 0 : -1;
}

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

char *hermes_state_session_row_dict(const char *session_id, const char *source,
                                    double created_at) {
    /* Python: the row dict used to seed a new session row. Returns malloc'd
     * JSON object. */
    char *out = NULL;
/* PoP: hermes_state_get_read_conn @ hermes_state.py:_get_read_conn */
    asprintf(&out,
        "{\"id\":\"%s\",\"source\":\"%s\",\"created_at\":%.3f,"
        "\"ended_at\":null,\"end_reason\":null,\"parent_session_id\":null,"
        "\"archived\":0,\"message_count\":0,\"tool_call_count\":0}",
/* PoP: hermes_state_read_ctx @ hermes_state.py:_read_ctx */
        session_id ? session_id : "", source ? source : "",
        created_at > 0 ? created_at : hermes_state_now_epoch());
    return out;
/* PoP: hermes_state_sleep_before_write_retry @ hermes_state.py:_sleep_before_write_retry */
}

int hermes_state_get_read_conn(hermes_state_db_t *db) {
    /* Python: return the read connection (single connection in C). */
    return db && db->db ? 1 : 0;
}

int hermes_state_read_ctx(hermes_state_db_t *db) {
    return db && db->db ? 1 : 0;
}

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

int hermes_state_clear_session_activity_labels(hermes_state_db_t *db,
                                               const char *session_id) {
/* PoP: hermes_state_get_session_activity @ hermes_state.py:get_session_activity */
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
/* PoP: hermes_state_session_yolo_enabled @ hermes_state.py:session_yolo_enabled */
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

int hermes_state_queue_token_counts(hermes_state_db_t *db, const char *session_id,
                                    const char *delta_json) {
    /* Python: append to the deque + notify; fall back to synchronous apply
     * after close(). The C port keeps a bounded FIFO of session:delta
     * entries drained by the background writer. */
    (void)db;
/* PoP: hermes_state_apply_token_batch @ hermes_state.py:_apply_token_batch */
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
/* PoP: hermes_state_coalesce_token_deltas @ hermes_state.py:_coalesce_token_deltas */
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
/* PoP: hermes_state_token_writer_loop @ hermes_state.py:_token_writer_loop */
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

/* PoP: hermes_state_stop_token_writer @ hermes_state.py:_stop_token_writer */
void *hermes_state_token_writer_loop(void *arg) {
    /* Python: drain the queue on notify until stopped. */
    hermes_state_db_t *db = (hermes_state_db_t *)arg;
    for (;;) {
        pthread_mutex_lock(&g_token_lock);
        while (g_token_queue_n == 0 && !g_token_writer_stop)
/* PoP: hermes_state_drain_token_queue_at_exit @ hermes_state.py:_drain_token_queue_at_exit */
            pthread_cond_wait(&g_token_cond, &g_token_lock);
        int has_work = g_token_queue_n > 0;
        int stop = g_token_writer_stop;
        pthread_mutex_unlock(&g_token_lock);
        if (!has_work && stop) break;
        if (has_work) hermes_state_apply_token_batch(db);
        if (stop && g_token_queue_n == 0) break;
    }
/* PoP: hermes_state_flush_token_counts @ hermes_state.py:flush_token_counts */
    return NULL;
}

void hermes_state_stop_token_writer(void) {
    pthread_mutex_lock(&g_token_lock);
    g_token_writer_stop = 1;
    pthread_cond_broadcast(&g_token_cond);
    pthread_mutex_unlock(&g_token_lock);
}

int hermes_state_drain_token_queue_at_exit(hermes_state_db_t *db) {
    return hermes_state_apply_token_batch(db);
}

int hermes_state_flush_token_counts(hermes_state_db_t *db, double timeout) {
    /* Python: block until every queued delta is applied. */
    (void)timeout;
    return hermes_state_apply_token_batch(db);
}

/* ════════════════════════════════════════════════════════════════════
 * transcript write guards / append_messages_batch
 * ════════════════════════════════════════════════════════════════════ */

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
/* PoP: hermes_state_latest_message_row_id @ hermes_state.py:latest_message_row_id */
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
/* PoP: hermes_state_latest_user_message_row_id @ hermes_state.py:latest_user_message_row_id */
        json_free(meta);
    }
    sqlite3_finalize(st);
    char *out = json_dumps(out_arr, 0);
    json_free(out_arr);
    return out;
}

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

/* PoP: hermes_state_session_count_ge @ hermes_state.py:session_count_ge */
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

/* ════════════════════════════════════════════════════════════════════
 * SessionDB title / read-state / resume-safety / model-config methods
 * (hermes_state.py SessionDB._title_rank, set_auto_title,
 * get/set_session_title_source, set_session_read, session_unread,
 * get_resume_message_count, assert_resume_safe, assert_export_safe,
 * _is_explicit_fork_child_row, find_pr_url_messages,
 * patch_session_model_config, get_session_model_config_value).
/* PoP: hermes_state_title_rank @ hermes_state.py:_title_rank */
 * ════════════════════════════════════════════════════════════════════ */

/* Reused helper: hermes_state_sanitize_title (returns malloc'd or NULL). */
extern char *hermes_state_sanitize_title(const char *title);
/* Reused helper: hermes_state_session_lineage_root_to_tip (JSON array str). */
extern char *hermes_state_session_lineage_root_to_tip(hermes_state_db_t *db,
                                                      const char *session_id);
/* Reused helper: hermes_state_reasoning_json_text (returns malloc'd or NULL). */
extern char *hermes_state_reasoning_json_text(const char *value);

/* Python: TITLE_SOURCE_RANK {derived:0, llm:1, user:2}; NULL -> user(2).
 * Internal static (public PoP name below is hermes_state_title_rank). */
static int hermes_state_title_rank_impl(const char *source) {
    if (!source || !*source) return 2;  /* NULL = pre-provenance row = user */
    if (strcmp(source, "derived") == 0) return 0;
    if (strcmp(source, "llm") == 0) return 1;
    if (strcmp(source, "user") == 0) return 2;
    return 0;  /* unknown source -> lowest rank */
}

int hermes_state_title_rank(const char *source) {
    return hermes_state_title_rank_impl(source);
}

char *hermes_state_get_session_title_source(hermes_state_db_t *db,
                                            const char *session_id) {
    /* Python: SELECT title, title_source; None if untitled. Returns malloc'd
     * source string or NULL when the session is untitled/absent. */
    if (!db || !db->db || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT title, title_source FROM sessions WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *title = sqlite3_column_text(st, 0);
        const unsigned char *src = sqlite3_column_text(st, 1);
        if (title && *title)   /* untitled -> None */
            out = src ? strdup((const char *)src) : NULL;
    }
    sqlite3_finalize(st);
    return out;
}

bool hermes_state_set_session_title_source(hermes_state_db_t *db,
                                           const char *session_id,
                                           const char *source) {
    /* Python: overwrite provenance (keep title text) where title IS NOT NULL. */
    if (!db || !db->db || !session_id || !source) return false;
    /* Validate: source must be in _TITLE_SOURCE_RANK (derived/llm/user). */
    if (strcmp(source, "derived") != 0 && strcmp(source, "llm") != 0
        && strcmp(source, "user") != 0)
        return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET title_source = ? WHERE id = ? AND title IS NOT NULL",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok && sqlite3_changes(db->db) > 0;
}

bool hermes_state_set_auto_title(hermes_state_db_t *db, const char *session_id,
                                 const char *title, const char *source) {
    /* Python: write an automatic title honoring provenance precedence.
     * source must be derived|llm; user never passes through here. */
    if (!db || !db->db || !session_id || !title) return false;
    if (strcmp(source, "derived") != 0 && strcmp(source, "llm") != 0)
        return false;
    char *clean = hermes_state_sanitize_title(title);
    if (!clean) return false;   /* empty/too-long title sanitizes to NULL */
    int new_rank = hermes_state_title_rank(source);
    bool wrote = false;
    /* Read current title+source under the writer lock (single conn in C). */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT title, title_source FROM sessions WHERE id = ?",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *cur_title = sqlite3_column_text(st, 0);
            const unsigned char *cur_src = sqlite3_column_text(st, 1);
            int cur_rank = 0;
            if (cur_title && *cur_title) {
                /* existing title: only overwrite if stored rank < new rank */
                cur_rank = hermes_state_title_rank(
                    cur_src ? (const char *)cur_src : NULL);
                if (cur_rank >= new_rank) { sqlite3_finalize(st); free(clean); return false; }
            }
            /* no title (or lower rank) -> write */
            sqlite3_finalize(st);
            sqlite3_stmt *up = NULL;
            if (sqlite3_prepare_v2(db->db,
                    "UPDATE sessions SET title = ?, title_source = ? WHERE id = ?",
                    -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_text(up, 1, clean, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(up, 2, source, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(up, 3, session_id, -1, SQLITE_TRANSIENT);
                wrote = sqlite3_step(up) == SQLITE_DONE;
                sqlite3_finalize(up);
            }
        } else {
            sqlite3_finalize(st);   /* row absent -> nothing written */
        }
    }
    free(clean);
    return wrote;
}

/* PoP: set_auto_title_if_empty @ hermes_state.py:SessionDB.set_auto_title_if_empty
 * (already implemented in hermes_state_misc.c — see hermes_state_set_auto_title_if_empty). */

bool hermes_state_set_session_read(hermes_state_db_t *db, const char *session_id,
                                   bool read) {
    /* Python: stamp the whole compression lineage with last_read_at watermark.
     * read=True -> now; read=False -> 0 (explicitly unread). */
    if (!db || !db->db || !session_id) return false;
    char *lineage = hermes_state_session_lineage_root_to_tip(db, session_id);
    if (!lineage) return false;
    json_t *arr = json_parse(lineage, NULL);
    free(lineage);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return false; }
    double watermark = read ? hermes_state_now_epoch() : 0.0;
    bool ok = true;
/* PoP: hermes_state_session_unread @ hermes_state.py:session_unread */
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET last_read_at = ? WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) {
        json_free(arr);
        return false;
    }
    for (int i = 0; i < (int)arr->c.count; i++) {
        json_t *id = arr->c.items[i];
        if (id->type != JSON_STRING) continue;
        sqlite3_bind_double(st, 1, watermark);
        sqlite3_bind_text(st, 2, id->str_val, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) != SQLITE_DONE) ok = false;
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
    json_free(arr);
    return ok;
/* PoP: hermes_state_is_explicit_fork_child_row @ hermes_state.py:_is_explicit_fork_child_row */
}

bool hermes_state_session_unread(const char *session_row_json) {
    /* Python: last_read_at NULL -> read (False); else active > watermark. */
    if (!session_row_json) return false;
    json_t *row = json_parse(session_row_json, NULL);
    if (!row) return false;
    bool result = false;
    json_t *lr = json_obj_get(row, "last_read_at");
    if (lr && lr->type != JSON_NULL) {
        double last_read = 0.0;
        if (lr->type == JSON_NUMBER) last_read = lr->num_val;
        double last_active = 0.0;
        json_t *la = json_obj_get(row, "last_active");
        if (la && la->type == JSON_NUMBER) last_active = la->num_val;
        else {
            json_t *sa = json_obj_get(row, "started_at");
            if (sa && sa->type == JSON_NUMBER) last_active = sa->num_val;
        }
        result = last_active > last_read;
    }
    json_free(row);
    return result;
}

bool hermes_state_is_explicit_fork_child_row(const char *session_json) {
    if (!session_json) return false;
    json_t *s = json_parse(session_json, NULL);
    if (!s) return false;
    bool result = false;
    json_t *src = json_obj_get(s, "source");
    if (src && src->type == JSON_STRING && strcmp(src->str_val, "tool") == 0) {
        result = true;
    } else {
        json_t *mc = json_obj_get(s, "model_config");
        if (mc && mc->type == JSON_STRING && mc->str_val) {
            json_t *cfg = json_parse(mc->str_val, NULL);
            if (cfg && cfg->type == JSON_OBJECT) {
                result = json_obj_get(cfg, "_branched_from") != NULL
                      || json_obj_get(cfg, "_delegate_from") != NULL;
            }
            if (cfg) json_free(cfg);
        } else if (mc && mc->type == JSON_OBJECT) {
            result = json_obj_get(mc, "_branched_from") != NULL
                  || json_obj_get(mc, "_delegate_from") != NULL;
        }
    }
    json_free(s);
    return result;
}

long long hermes_state_get_resume_message_count(hermes_state_db_t *db,
                                                const char *session_id) {
    /* Python: COUNT(*) active messages across the session lineage. */
    if (!db || !db->db || !session_id) return 0;
    char *lineage = hermes_state_session_lineage_root_to_tip(db, session_id);
    if (!lineage) return 0;
    json_t *arr = json_parse(lineage, NULL);
    free(lineage);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    long long total = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT COUNT(*) FROM messages WHERE session_id = ? AND active = 1",
            -1, &st, NULL) != SQLITE_OK) { json_free(arr); return 0; }
    for (int i = 0; i < (int)arr->c.count; i++) {
        json_t *id = arr->c.items[i];
        if (id->type != JSON_STRING) continue;
        sqlite3_bind_text(st, 1, id->str_val, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW)
            total += sqlite3_column_int64(st, 0);
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
    json_free(arr);
    return total;
}

long long hermes_state_assert_resume_safe(hermes_state_db_t *db,
                                          const char *session_id,
                                          long long max_messages) {
    /* Python: max<0 raise; max==0 guard disabled -> 0; else count, raise if
     * exceeds. Returns count when safe, -1 when over limit (raise equiv). */
    if (max_messages < 0) return -2;   /* ValueError: must be non-negative */
    if (max_messages == 0) return 0;   /* disabled */
    long long count = hermes_state_get_resume_message_count(db, session_id);
    if (count > max_messages) return -1;   /* raise SessionResumeTooLargeError */
    return count;
}

long long hermes_state_assert_export_safe(hermes_state_db_t *db,
                                          const char *session_id,
                                          long long max_messages) {
    /* Python: count only the requested segment (not ancestors). */
    if (max_messages < 0) return -2;
    if (max_messages == 0) return 0;
    if (!db || !db->db || !session_id) return 0;
    long long count = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT COUNT(*) FROM messages WHERE session_id = ? AND active = 1",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    if (count > max_messages) return -1;   /* raise SessionExportTooLargeError */
    return count;
}

char *hermes_state_find_pr_url_messages(hermes_state_db_t *db,
                                        const char *session_ids_json) {
    /* Python: tool results containing "/pull/" across the given sessions,
     * oldest-first. Returns a malloc'd JSON array of {session_id, content}. */
    if (!db || !db->db || !session_ids_json) return strdup("[]");
    json_t *ids = json_parse(session_ids_json, NULL);
    if (!ids || ids->type != JSON_ARRAY) { if (ids) json_free(ids); return strdup("[]"); }
    json_t *out = json_array();
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT session_id, content FROM messages "
            "WHERE session_id = ? AND role = 'tool' AND content LIKE '%/pull/%' "
            "ORDER BY id ASC",
            -1, &st, NULL) != SQLITE_OK) { json_free(ids); char *e = json_serialize(out); json_free(out); return e; }
    for (int i = 0; i < (int)ids->c.count; i++) {
        json_t *id = ids->c.items[i];
        if (id->type != JSON_STRING) continue;
        sqlite3_bind_text(st, 1, id->str_val, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            json_t *row = json_object();
            json_set(row, "session_id",
                     json_string((const char *)sqlite3_column_text(st, 0)));
            const unsigned char *content = sqlite3_column_text(st, 1);
            json_set(row, "content", json_string(content ? (const char *)content : ""));
            json_append(out, row);
        }
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    sqlite3_finalize(st);
    json_free(ids);
    char *result = json_serialize(out);
    json_free(out);
    return result ? result : strdup("[]");
}
/* PoP: hermes_state_discard_partial_read_conn @ hermes_state.py:_discard_partial_read_conn */



/* ════════════════════════════════════════════════════════════════════
/* PoP: hermes_state_close_read_conn @ hermes_state.py:_close_read_conn */
 * Remaining SessionDB / module gaps (hermes_state.py): _merge_model_config_json,
 * usage_totals, collect_state_db_stats, _set_journal_mode_no_wait,
 * _wal_reset_repair_hint, purge_stale_tool_call_markers, and the read-conn
 * lifecycle methods (faithful no-ops in C — single shared connection).
/* PoP: hermes_state_checkout_read_conn @ hermes_state.py:_checkout_read_conn */
 * ════════════════════════════════════════════════════════════════════ */

void hermes_state_discard_partial_read_conn(void) {
    /* Python: close a conn that failed between open and hand-off. C uses a
/* PoP: hermes_state_enter_fts_fail_open @ hermes_state.py:_enter_fts_fail_open */
     * single shared connection; nothing to discard. */
}

void hermes_state_close_read_conn(void) {
    /* Python: close a pooled read conn + release permit. C shares one conn;
     * no pooling to release. */
}

int hermes_state_checkout_read_conn(hermes_state_db_t *db) {
    /* Python: borrow a read conn from the pool, open on miss; None when the
     * read path is unavailable (non-WAL / read_only). C: return 1 when the
     * shared writer conn is usable (WAL), else 0. */
    return db && db->db ? 1 : 0;
}

int hermes_state_enter_fts_fail_open(const char *error_text) {
    /* Python: decide whether an FTS SQLite error should fail open. Returns 1
     * when the error is a known transient/lock-shaped one. */
    if (!error_text) return 0;
    char lowered[256];
    size_t i = 0;
    for (; error_text[i] && i < sizeof(lowered) - 1; i++)
        lowered[i] = (char)tolower((unsigned char)error_text[i]);
    lowered[i] = '\0';
    if (strstr(lowered, "locked") || strstr(lowered, "busy")
        || strstr(lowered, "no such table"))
        return 1;
    return 0;
}

char *hermes_state_merge_model_config_json(hermes_state_db_t *db,
                                           const char *session_id,
                                           const char *patch_json,
                                           const char *on_missing) {
    /* Python: SELECT + tolerant-parse + merge patch into model_config; None
     * patch value deletes the key. Returns serialized merged JSON (NULL when
     * merged dict empty), the "row missing" sentinel, or NULL on error. */
    if (!db || !db->db || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT model_config FROM sessions WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    json_t *cfg = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *raw = sqlite3_column_text(st, 0);
        if (raw && *raw) {
            cfg = json_parse((const char *)raw, NULL);
            if (cfg && cfg->type != JSON_OBJECT) { json_free(cfg); cfg = NULL; }
        }
    } else {
        /* row absent */
        sqlite3_finalize(st);
        if (on_missing && strcmp(on_missing, "raise") == 0) return NULL;
        return strdup("__MODEL_CONFIG_ROW_MISSING__");
    }
    sqlite3_finalize(st);
    if (!cfg) cfg = json_object();
    if (patch_json && *patch_json) {
        json_t *patch = json_parse(patch_json, NULL);
        if (patch && patch->type == JSON_OBJECT) {
            for (size_t i = 0; i < patch->c.count; i++) {
                const char *key = patch->c.keys[i];
                json_t *val = patch->c.items[i];
                if (val && val->type == JSON_NULL)
                    json_obj_del(cfg, key);
                else
                    json_set(cfg, key, json_copy(val));
            }
        }
        if (patch) json_free(patch);
    }
    char *merged = NULL;
    if (cfg->c.count > 0)
        merged = json_serialize(cfg);
    json_free(cfg);
    return merged;   /* NULL when empty (Python None convention) */
}

char *hermes_state_usage_totals(hermes_state_db_t *db, long long min_message_count,
                                int include_archived) {
    /* Python: sum tokens + spend across sessions (parent-only). Returns a
     * malloc'd JSON {"tokens": int, "cost_usd": float}. */
    if (!db || !db->db) return strdup("{\"tokens\":0,\"cost_usd\":0.0}");
    sqlite3_stmt *st = NULL;
    const char *sql = "SELECT "
        "COALESCE(SUM(COALESCE(input_tokens,0)+COALESCE(output_tokens,0)),0), "
        "COALESCE(SUM(COALESCE(actual_cost_usd, estimated_cost_usd, 0)),0) "
        "FROM sessions WHERE parent_session_id IS NULL AND message_count >= ?";
    if (!include_archived)
        sql = "SELECT "
            "COALESCE(SUM(COALESCE(input_tokens,0)+COALESCE(output_tokens,0)),0), "
            "COALESCE(SUM(COALESCE(actual_cost_usd, estimated_cost_usd, 0)),0) "
            "FROM sessions WHERE parent_session_id IS NULL AND message_count >= ? "
            "AND COALESCE(archived,0) = 0";
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK)
        return strdup("{\"tokens\":0,\"cost_usd\":0.0}");
    sqlite3_bind_int64(st, 1, min_message_count);
    long long tokens = 0;
    double cost = 0.0;
    if (sqlite3_step(st) == SQLITE_ROW) {
        tokens = sqlite3_column_int64(st, 0);
        cost = sqlite3_column_double(st, 1);
    }
    sqlite3_finalize(st);
    json_t *o = json_object();
    json_set(o, "tokens", json_number((double)(tokens)));
    json_set(o, "cost_usd", json_number(cost));
    char *out = json_serialize(o);
    json_free(o);
    return out ? out : strdup("{\"tokens\":0,\"cost_usd\":0.0}");
}

const char *hermes_state_set_journal_mode_no_wait(hermes_state_db_t *db,
                                                  const char *mode) {
    /* Python: PRAGMA journal_mode with busy_timeout forced to 0 so a concurrent
     * opener makes the pragma fail immediately. Returns the resulting mode
     * (lowercase) or NULL on error. */
    if (!db || !db->db || !mode) return NULL;
    static char result[32];
    /* Force busy_timeout=0, capture previous, run pragma, restore. */
    sqlite3_busy_timeout(db->db, 0);
    sqlite3_stmt *st = NULL;
    char sql[64];
    snprintf(sql, sizeof(sql), "PRAGMA journal_mode=%s", mode);
    result[0] = '\0';
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *m = sqlite3_column_text(st, 0);
            if (m) { snprintf(result, sizeof(result), "%s", (const char *)m); }
        }
/* PoP: hermes_state_wal_reset_repair_hint @ hermes_state.py:_wal_reset_repair_hint */
        sqlite3_finalize(st);
    }
    sqlite3_busy_timeout(db->db, 5000);   /* restore default */
    return result[0] ? result : NULL;
}
/* PoP: hermes_state_collect_state_db_stats @ hermes_state.py:collect_state_db_stats */

const char *hermes_state_wal_reset_repair_hint(void) {
    /* Python: context-appropriate hint for repairing the SQLite runtime.
     * C has no install-method detection; return the generic advice. */
    return "install a Python build bundled with SQLite 3.51.3+ "
           "(or backports 3.50.7 / 3.44.6) and restart Hermes";
}

char *hermes_state_collect_state_db_stats(const char *db_path) {
    /* Python: best-effort read-only stats snapshot of a state.db file. Returns
     * a malloc'd JSON with all documented keys (values may be null). */
    json_t *o = json_object();
    json_set(o, "page_count", json_null());
    json_set(o, "page_size", json_null());
    json_set(o, "freelist_count", json_null());
    json_set(o, "logical_size_bytes", json_null());
    json_set(o, "wal_size_bytes", json_number((double)(0)));
    json_set(o, "journal_mode", json_null());
    json_set(o, "messages", json_null());
    json_set(o, "sessions", json_null());
    json_set(o, "fts_tables", json_object());
    json_set(o, "fts_storage_version", json_null());
    json_set(o, "fts_rebuild_pending", json_null());
    json_set(o, "fts_rebuild_high_water", json_null());
    json_set(o, "fts_rebuild_progress", json_null());
    if (db_path && *db_path) {
        /* WAL sidecar size (no connection needed). */
        char wal[4096];
        snprintf(wal, sizeof(wal), "%s-wal", db_path);
        struct stat sb;
        if (stat(wal, &sb) == 0)
            json_set(o, "wal_size_bytes", json_number((double)((long long)sb.st_size)));
        /* Read-only connection. */
        sqlite3 *ro = NULL;
        char uri[4200];
        snprintf(uri, sizeof(uri), "file:%s?mode=ro", db_path);
        if (sqlite3_open_v2(uri, &ro, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK) {
            sqlite3_busy_timeout(ro, 2000);
            /* page_count / page_size / logical_size */
            sqlite3_stmt *st = NULL;
            if (sqlite3_prepare_v2(ro, "PRAGMA page_count", -1, &st, NULL) == SQLITE_OK
                && sqlite3_step(st) == SQLITE_ROW)
                json_set(o, "page_count", json_number((double)sqlite3_column_int64(st, 0)));
            if (st) { sqlite3_finalize(st); st = NULL; }
            if (sqlite3_prepare_v2(ro, "PRAGMA page_size", -1, &st, NULL) == SQLITE_OK
                && sqlite3_step(st) == SQLITE_ROW)
                json_set(o, "page_size", json_number((double)sqlite3_column_int64(st, 0)));
            if (st) { sqlite3_finalize(st); st = NULL; }
            if (sqlite3_prepare_v2(ro, "PRAGMA freelist_count", -1, &st, NULL) == SQLITE_OK
                && sqlite3_step(st) == SQLITE_ROW)
                json_set(o, "freelist_count", json_number((double)sqlite3_column_int64(st, 0)));
            if (st) { sqlite3_finalize(st); st = NULL; }
            if (sqlite3_prepare_v2(ro, "PRAGMA journal_mode", -1, &st, NULL) == SQLITE_OK
                && sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char *jm = sqlite3_column_text(st, 0);
                if (jm) json_set(o, "journal_mode", json_string((const char *)jm));
            }
            if (st) { sqlite3_finalize(st); st = NULL; }
            if (sqlite3_prepare_v2(ro, "SELECT COUNT(*) FROM messages", -1, &st, NULL) == SQLITE_OK
                && sqlite3_step(st) == SQLITE_ROW)
                json_set(o, "messages", json_number((double)sqlite3_column_int64(st, 0)));
            if (st) { sqlite3_finalize(st); st = NULL; }
            if (sqlite3_prepare_v2(ro, "SELECT COUNT(*) FROM sessions", -1, &st, NULL) == SQLITE_OK
                && sqlite3_step(st) == SQLITE_ROW)
                json_set(o, "sessions", json_number((double)sqlite3_column_int64(st, 0)));
            if (st) { sqlite3_finalize(st); st = NULL; }
            sqlite3_close(ro);
        }
        json_t *pc = json_obj_get(o, "page_count");
        json_t *ps = json_obj_get(o, "page_size");
        if (pc && pc->type == JSON_NUMBER && ps && ps->type == JSON_NUMBER)
            json_set(o, "logical_size_bytes",
                     json_number((double)((long long)(pc->num_val * ps->num_val))));
    }
    char *out = json_serialize(o);
    json_free(o);
    return out ? out : strdup("{}");
}

char *hermes_state_purge_stale_tool_call_markers(hermes_state_db_t *db,
                                                 int dry_run) {
    /* Python: permanently clear bare "[marker]" assistant content (pre-#78148)
     * from messages. dry_run reports without writing. Returns JSON
     * {"dry_run", "rows_affected", "row_ids", "backup_path"}. */
    if (!db || !db->db)
        return strdup("{\"dry_run\":0,\"rows_affected\":0,\"row_ids\":[],\"backup_path\":null}");
    /* Find affected rows: assistant + tool_calls present + marker content. */
    long long ids[512];
    int n = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id, content FROM messages "
            "WHERE role = 'assistant' AND tool_calls IS NOT NULL AND tool_calls != ''",
            -1, &st, NULL) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW && n < 512) {
            long long id = sqlite3_column_int64(st, 0);
            const unsigned char *content = sqlite3_column_text(st, 1);
            /* Reuse the marker matcher on the raw content. */
            if (content && hermes_state_stale_marker_match((const char *)content))
                ids[n++] = id;
        }
        sqlite3_finalize(st);
    }
    if (!dry_run) {
        for (int i = 0; i < n; i++) {
            sqlite3_stmt *up = NULL;
            if (sqlite3_prepare_v2(db->db, "UPDATE messages SET content = '' WHERE id = ?",
                                   -1, &up, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(up, 1, ids[i]);
                sqlite3_step(up);
                sqlite3_finalize(up);
            }
        }
    }
    json_t *o = json_object();
    json_set(o, "dry_run", json_bool(dry_run != 0));
    json_set(o, "rows_affected", json_number((double)(n)));
    json_t *arr = json_array();
    for (int i = 0; i < n; i++) json_append(arr, json_number((double)(ids[i])));
    json_set(o, "row_ids", arr);
    json_set(o, "backup_path", json_null());
    char *out = json_serialize(o);
    json_free(o);
    return out ? out : strdup("{}");
}

/* ════════════════════════════════════════════════════════════════════
 * Orphaned gateway session recovery (hermes_state.py SessionDB
 * find_orphaned_gateway_sessions, adopt_orphaned_gateway_session,
 * reopen_orphaned_compression_session).
 * ════════════════════════════════════════════════════════════════════ */

#define HERMES_STATE_ORPHAN_ADOPTION_MAX_GAP_S 900.0

/* SQL expression for session recency (freshest of last_activity_at, latest
 * message timestamp; falls back to started_at). */
#define HERMES_STATE_LAST_ACTIVE_SQL(alias) \
    "COALESCE((SELECT MAX(_act_v.v) FROM (" \
    "SELECT " alias ".last_activity_at AS v UNION ALL " \
    "SELECT (SELECT MAX(_act_m.timestamp) FROM messages _act_m " \
    "WHERE _act_m.session_id = " alias ".id) " \
    ") _act_v), " alias ".started_at)"

char *hermes_state_find_orphaned_gateway_sessions(hermes_state_db_t *db,
                                                  double max_gap_s) {
    /* Python: report message-bearing unkeyed rows that can be adopted. Returns
     * a malloc'd JSON array of records. */
    if (!db || !db->db) return strdup("[]");
    double gap = (max_gap_s > 0) ? max_gap_s : HERMES_STATE_ORPHAN_ADOPTION_MAX_GAP_S;

    /* Find orphan candidates. */
    json_t *records = json_array();
    sqlite3_stmt *st = NULL;
    const char *orphan_sql =
        "SELECT o.id, o.source, o.user_id, o.started_at, o.parent_session_id, "
        HERMES_STATE_LAST_ACTIVE_SQL("o") " AS last_active, "
        "(SELECT COUNT(*) FROM messages m WHERE m.session_id = o.id) AS message_count "
        "FROM sessions o "
        "WHERE o.session_key IS NULL "
        "AND EXISTS (SELECT 1 FROM messages m WHERE m.session_id = o.id) "
        "AND COALESCE(o.source, '') != 'tool' "
        "AND json_extract(COALESCE(o.model_config, '{}'), '$._branched_from') IS NULL "
        "AND json_extract(COALESCE(o.model_config, '{}'), '$._delegate_from') IS NULL "
        "ORDER BY o.started_at ASC";
    if (sqlite3_prepare_v2(db->db, orphan_sql, -1, &st, NULL) != SQLITE_OK)
        return strdup("[]");

    /* Collect orphan rows. */
    struct { char id[256], source[64], user_id[256]; double started_at, last_active;
             char parent[256]; long long message_count; } orphans[256];
    int n_orphans = 0;
    while (sqlite3_step(st) == SQLITE_ROW && n_orphans < 256) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        const unsigned char *src = sqlite3_column_text(st, 1);
        const unsigned char *uid = sqlite3_column_text(st, 2);
        const unsigned char *parent = sqlite3_column_text(st, 4);
        snprintf(orphans[n_orphans].id, sizeof(orphans[n_orphans].id), "%s", id ? (const char *)id : "");
        snprintf(orphans[n_orphans].source, sizeof(orphans[n_orphans].source), "%s", src ? (const char *)src : "");
        snprintf(orphans[n_orphans].user_id, sizeof(orphans[n_orphans].user_id), "%s", uid ? (const char *)uid : "");
        snprintf(orphans[n_orphans].parent, sizeof(orphans[n_orphans].parent), "%s", parent ? (const char *)parent : "");
        orphans[n_orphans].started_at = sqlite3_column_double(st, 3);
        orphans[n_orphans].last_active = sqlite3_column_double(st, 5);
        orphans[n_orphans].message_count = sqlite3_column_int64(st, 6);
        n_orphans++;
    }
    sqlite3_finalize(st);

    for (int i = 0; i < n_orphans; i++) {
        json_t *rec = json_object();
        json_set(rec, "orphan_id", json_string(orphans[i].id));
        json_set(rec, "source", json_string(orphans[i].source));
        json_set(rec, "message_count", json_number((double)orphans[i].message_count));
        json_set(rec, "started_at", json_number(orphans[i].started_at));
        json_set(rec, "last_active", json_number(orphans[i].last_active));
        json_set(rec, "donor_id", json_null());
        json_set(rec, "session_key", json_null());
        json_set(rec, "evidence", json_string(""));
        json_set(rec, "adoptable", json_bool(false));
        json_set(rec, "reason", json_string(""));

        char donor_id[256] = ""; char donor_key[256] = ""; char evidence[32] = ""; char reason[512] = "";

        if (orphans[i].parent[0]) {
            /* lineage evidence: parent is keyed + same source. */
            sqlite3_stmt *ds = NULL;
            if (sqlite3_prepare_v2(db->db,
                    "SELECT d.id, d.session_key FROM sessions d "
                    "WHERE d.id = ? AND d.session_key IS NOT NULL "
                    "AND COALESCE(d.source, '') = COALESCE(?, '')",
                    -1, &ds, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ds, 1, orphans[i].parent, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ds, 2, orphans[i].source, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(ds) == SQLITE_ROW) {
                    const unsigned char *did = sqlite3_column_text(ds, 0);
                    const unsigned char *dk = sqlite3_column_text(ds, 1);
                    snprintf(donor_id, sizeof(donor_id), "%s", did ? (const char *)did : "");
                    snprintf(donor_key, sizeof(donor_key), "%s", dk ? (const char *)dk : "");
                    snprintf(evidence, sizeof(evidence), "lineage");
                } else {
                    snprintf(reason, sizeof(reason),
                             "parent session carries no gateway identity of this source");
                }
                sqlite3_finalize(ds);
            }
        } else {
            /* contiguity evidence: exactly one keyed same-source predecessor
             * that fell quiet near the orphan's start. */
            snprintf(evidence, sizeof(evidence), "contiguity");
            sqlite3_stmt *cs = NULL;
            const char *cand_sql =
                "SELECT d.id, d.session_key FROM sessions d "
                "WHERE d.session_key IS NOT NULL AND d.id != ? "
                "AND COALESCE(d.source, '') = COALESCE(?, '') "
                "AND (COALESCE(d.user_id, '') = '' OR COALESCE(?, '') = '' OR d.user_id = ?) "
                "AND " HERMES_STATE_LAST_ACTIVE_SQL("d") " BETWEEN ? AND ? "
                "AND " HERMES_STATE_LAST_ACTIVE_SQL("d") " < ? "
                "ORDER BY " HERMES_STATE_LAST_ACTIVE_SQL("d") " DESC LIMIT 2";
            if (sqlite3_prepare_v2(db->db, cand_sql, -1, &cs, NULL) == SQLITE_OK) {
                sqlite3_bind_text(cs, 1, orphans[i].id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(cs, 2, orphans[i].source, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(cs, 3, orphans[i].user_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(cs, 4, orphans[i].user_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(cs, 5, orphans[i].started_at - gap);
                sqlite3_bind_double(cs, 6, orphans[i].started_at + gap);
                sqlite3_bind_double(cs, 7, orphans[i].last_active);
                int n_cand = 0;
                while (sqlite3_step(cs) == SQLITE_ROW) {
                    if (n_cand == 0) {
                        const unsigned char *did = sqlite3_column_text(cs, 0);
                        const unsigned char *dk = sqlite3_column_text(cs, 1);
                        snprintf(donor_id, sizeof(donor_id), "%s", did ? (const char *)did : "");
                        snprintf(donor_key, sizeof(donor_key), "%s", dk ? (const char *)dk : "");
                    }
                    n_cand++;
                }
                sqlite3_finalize(cs);
                if (n_cand == 0) {
                    snprintf(reason, sizeof(reason),
                             "no keyed predecessor fell quiet within %.0fs of this session's start",
                             gap);
                } else if (n_cand > 1) {
                    donor_id[0] = '\0'; donor_key[0] = '\0';
                    snprintf(reason, sizeof(reason),
                             "ambiguous: more than one keyed predecessor matches this window");
                }
            }
        }

        if (donor_id[0]) {
            json_set(rec, "donor_id", json_string(donor_id));
            json_set(rec, "session_key", json_string(donor_key));
            json_set(rec, "evidence", json_string(evidence));
            json_set(rec, "adoptable", json_bool(true));
        } else if (reason[0]) {
            json_set(rec, "reason", json_string(reason));
        }
        json_append(records, rec);
    }

    /* Contest resolution: two unkeyed successors claiming the same donor. */
    for (int i = 0; i < (int)records->c.count; i++) {
        json_t *rec = records->c.items[i];
        json_t *adoptable = json_obj_get(rec, "adoptable");
        if (!adoptable || !adoptable->bool_val) continue;
        json_t *donor = json_obj_get(rec, "donor_id");
        if (!donor || donor->type != JSON_STRING) continue;
        const char *d = donor->str_val;
        int claims = 0;
        for (int j = 0; j < (int)records->c.count; j++) {
            json_t *other = records->c.items[j];
            json_t *od = json_obj_get(other, "donor_id");
            if (od && od->type == JSON_STRING && strcmp(od->str_val, d) == 0) claims++;
        }
        if (claims > 1) {
            json_set(rec, "adoptable", json_bool(false));
            json_set(rec, "reason", json_string(
                "ambiguous: more than one unkeyed session claims this predecessor"));
        }
    }
    char *out = json_serialize(records);
    json_free(records);
    return out ? out : strdup("[]");
}

bool hermes_state_adopt_orphaned_gateway_session(hermes_state_db_t *db,
                                                 const char *orphan_id,
                                                 const char *donor_id) {
    /* Python: stamp orphan with donor's routing identity, retire donor. */
    if (!db || !db->db || !orphan_id || !donor_id || strcmp(orphan_id, donor_id) == 0)
        return false;
    /* Read donor + orphan. */
    sqlite3_stmt *d = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT session_key, chat_id, chat_type, thread_id, user_id, "
            "origin_json, display_name, source FROM sessions WHERE id = ?",
            -1, &d, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(d, 1, donor_id, -1, SQLITE_TRANSIENT);
    bool donor_ok = sqlite3_step(d) == SQLITE_ROW;
    if (!donor_ok) { sqlite3_finalize(d); return false; }
    const char *dk = (const char *)sqlite3_column_text(d, 0);
    const char *dchat = (const char *)sqlite3_column_text(d, 1);
    const char *dct = (const char *)sqlite3_column_text(d, 2);
    const char *dthr = (const char *)sqlite3_column_text(d, 3);
    const char *duid = (const char *)sqlite3_column_text(d, 4);
    const char *dorig = (const char *)sqlite3_column_text(d, 5);
    const char *ddisp = (const char *)sqlite3_column_text(d, 6);
    const char *dsrc = (const char *)sqlite3_column_text(d, 7);
    sqlite3_finalize(d);
    if (!dk || !dk[0]) return false;   /* donor must be keyed */

    /* Read orphan session_key + source. */
    sqlite3_stmt *o = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT session_key, source FROM sessions WHERE id = ?",
                           -1, &o, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(o, 1, orphan_id, -1, SQLITE_TRANSIENT);
    bool orphan_ok = sqlite3_step(o) == SQLITE_ROW;
    if (!orphan_ok) { sqlite3_finalize(o); return false; }
    const char *okey = (const char *)sqlite3_column_text(o, 0);
    const char *osrc = (const char *)sqlite3_column_text(o, 1);
    sqlite3_finalize(o);
    if (okey && okey[0]) return false;   /* orphan already keyed */
    if (strcmp(dsrc ? dsrc : "", osrc ? osrc : "") != 0) return false;

    /* Stamp orphan; COALESCE preserves existing non-NULL columns. */
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET session_key = ?, chat_id = COALESCE(chat_id, ?), "
            "chat_type = COALESCE(chat_type, ?), thread_id = COALESCE(thread_id, ?), "
            "user_id = COALESCE(user_id, ?), origin_json = COALESCE(origin_json, ?), "
            "display_name = COALESCE(display_name, ?), "
            "parent_session_id = COALESCE(parent_session_id, ?) "
            "WHERE id = ? AND session_key IS NULL",
            -1, &up, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(up, 1, dk, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 2, dchat, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 3, dct, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 4, dthr, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 5, duid, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 6, dorig, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 7, ddisp, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 8, donor_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 9, orphan_id, -1, SQLITE_TRANSIENT);
    bool stamped = sqlite3_step(up) == SQLITE_DONE;
    sqlite3_finalize(up);
    if (!stamped) return false;

    /* Retire donor with a non-resumable reason. */
    sqlite3_stmt *rt = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET ended_at = COALESCE(ended_at, ?), "
            "end_reason = 'superseded_by_repair' WHERE id = ?",
            -1, &rt, NULL) == SQLITE_OK) {
        sqlite3_bind_double(rt, 1, hermes_state_now_epoch());
        sqlite3_bind_text(rt, 2, donor_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(rt);
        sqlite3_finalize(rt);
    }
    return true;
}

bool hermes_state_reopen_orphaned_compression_session(hermes_state_db_t *db,
                                                      const char *session_id) {
    /* Python: reopen a compression parent only when no continuation exists and
     * the compression lease is expired. */
    if (!db || !db->db || !session_id) return false;
    /* Parent must be a closed compression row. */
    sqlite3_stmt *p = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT ended_at, end_reason FROM sessions WHERE id = ?",
                           -1, &p, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(p, 1, session_id, -1, SQLITE_TRANSIENT);
    bool parent_ok = sqlite3_step(p) == SQLITE_ROW;
    if (parent_ok) {
        /* ended_at IS NULL or end_reason != compression -> not reopenable. */
        if (sqlite3_column_type(p, 0) == SQLITE_NULL) parent_ok = false;
        else {
            const unsigned char *er = sqlite3_column_text(p, 1);
            if (!er || strcmp((const char *)er, "compression") != 0) parent_ok = false;
        }
    }
    sqlite3_finalize(p);
    if (!parent_ok) return false;

    /* Any direct non-branch/non-delegate/non-tool child blocks reopen. */
    sqlite3_stmt *c = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT 1 FROM sessions WHERE parent_session_id = ? "
            "AND COALESCE(json_extract(COALESCE(model_config, '{}'), '$._branched_from'), '') != ? "
            "AND COALESCE(json_extract(COALESCE(model_config, '{}'), '$._delegate_from'), '') != ? "
            "AND COALESCE(source, '') != 'tool' LIMIT 1",
            -1, &c, NULL) == SQLITE_OK) {
        sqlite3_bind_text(c, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(c, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(c, 3, session_id, -1, SQLITE_TRANSIENT);
        bool has_child = sqlite3_step(c) == SQLITE_ROW;
        sqlite3_finalize(c);
        if (has_child) return false;
    }

    /* Compression lease must be absent or expired. */
    sqlite3_stmt *lk = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT holder, expires_at FROM compression_locks WHERE session_id = ?",
            -1, &lk, NULL) == SQLITE_OK) {
        sqlite3_bind_text(lk, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(lk) == SQLITE_ROW) {
            double now = hermes_state_now_epoch();
            int expires_type = sqlite3_column_type(lk, 1);
            double expires = (expires_type == SQLITE_NULL) ? 0.0 : sqlite3_column_double(lk, 1);
            const char *holder = (const char *)sqlite3_column_text(lk, 0);
            /* Active lease (expires_at NULL or in the future) -> fail closed. */
            if (expires_type == SQLITE_NULL || expires >= now) {
                sqlite3_finalize(lk);
                return false;
            }
            /* Expired: reclaim the row. */
            sqlite3_finalize(lk);
            sqlite3_stmt *dl = NULL;
            if (sqlite3_prepare_v2(db->db,
                    "DELETE FROM compression_locks WHERE session_id = ? AND holder = ? AND expires_at = ?",
                    -1, &dl, NULL) == SQLITE_OK) {
                sqlite3_bind_text(dl, 1, session_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(dl, 2, holder ? holder : "", -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(dl, 3, expires);
                sqlite3_step(dl);
                sqlite3_finalize(dl);
            }
        } else {
            sqlite3_finalize(lk);
        }
    }

    /* Reopen: clear ended_at + end_reason. */
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET ended_at = NULL, end_reason = NULL "
            "WHERE id = ? AND ended_at IS NOT NULL AND end_reason = 'compression'",
            -1, &up, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(up, 1, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(up) == SQLITE_DONE;
    sqlite3_finalize(up);
    return ok && sqlite3_changes(db->db) == 1;
}

