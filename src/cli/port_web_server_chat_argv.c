/*
 * port_web_server_chat_argv.c — chat PTY env assembly + session descendant
 * resolution. Faithful port of _resolve_profile_dir,
 * _session_latest_descendant, HermesState.resolve_session_id (the exact/
 * unique-prefix contract the resume path needs), and the env-assembly body
 * of _resolve_chat_argv from hermes_cli/web_server.py.
 *
 * Reuses: profile_validate_name / profile_dir_exists / profile_dir_for
 * (port_cli_profiles.c) and sqlite3 (lib/libdb) against the real session DB.
 */

#include "web_server_chat_argv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"
#include "hermes_json.h"

/* From port_cli_profiles.c */
extern bool profile_validate_name(const char *name, char **err);
extern int profile_dir_exists(const char *name);
extern char *profile_dir_for(const char *name);

/* ── _resolve_profile_dir ───────────────────────────────────────────────── */
/* PoP: ws_chat_resolve_profile_dir @ hermes_cli/web_server.py:_resolve_profile_dir */
char *ws_chat_resolve_profile_dir(const char *name, int *status,
                                  char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    char *err = NULL;
    if (!profile_validate_name(name, &err)) {
        if (status) *status = 400;
        if (detail) *detail = err ? err : strdup("invalid profile name");
        else free(err);
        return NULL;
    }
    free(err);
    if (!profile_dir_exists(name)) {
        if (status) *status = 404;
        if (detail) {
            char buf[512];
            snprintf(buf, sizeof(buf), "Profile '%s' does not exist.", name);
            *detail = strdup(buf);
        }
        return NULL;
    }
    return profile_dir_for(name);
}

/* ── resolve_session_id (hermes_state.py) ───────────────────────────────── */

static bool session_exists(sqlite3 *db, const char *sid) {
    sqlite3_stmt *st = NULL;
    bool found = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sessions WHERE id = ?", -1,
                           &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, sid, -1, SQLITE_TRANSIENT);
        found = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    return found;
}

/* PoP: ws_chat_resolve_session_id @ hermes_state.py:resolve_session_id */
char *ws_chat_resolve_session_id(const char *db_path, const char *prefix) {
    if (!db_path || !prefix || !*prefix) return NULL;
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    char *result = NULL;
    if (session_exists(db, prefix)) {
        result = strdup(prefix);
        sqlite3_close(db);
        return result;
    }
    /* LIKE-escape \, %, _ with backslash (Python .replace chain). */
    size_t plen = strlen(prefix);
    char *escaped = malloc(plen * 2 + 2);
    size_t j = 0;
    for (size_t i = 0; i < plen; i++) {
        char c = prefix[i];
        if (c == '\\' || c == '%' || c == '_') escaped[j++] = '\\';
        escaped[j++] = c;
    }
    escaped[j++] = '%';
    escaped[j] = '\0';

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT id FROM sessions WHERE id LIKE ? ESCAPE '\\' "
            "ORDER BY started_at DESC LIMIT 2",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, escaped, -1, SQLITE_TRANSIENT);
        char *first = NULL;
        int count = 0;
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (count == 0) {
                const unsigned char *t = sqlite3_column_text(st, 0);
                first = strdup(t ? (const char *)t : "");
            }
            count++;
        }
        if (count == 1) result = first;
        else free(first);
    }
    sqlite3_finalize(st);
    free(escaped);
    sqlite3_close(db);
    return result;
}

/* ── _session_latest_descendant ─────────────────────────────────────────── */

typedef struct {
    char *id;
    char *parent;
    double started_at;
} desc_row_t;

/* PoP: ws_chat_session_latest_descendant @ hermes_cli/web_server.py:_session_latest_descendant */
json_t *ws_chat_session_latest_descendant(const char *db_path,
                                          const char *session_id) {
    json_t *out = json_object();
    json_set(out, "latest", json_null());
    json_set(out, "path", json_array());

    char *sid = ws_chat_resolve_session_id(db_path, session_id);
    if (!sid) return out;

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        free(sid);
        return out;
    }

    desc_row_t *rows = NULL;
    size_t nrows = 0, cap = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "WITH RECURSIVE descendants(id, parent_session_id, started_at) AS ("
            " SELECT id, parent_session_id, started_at FROM sessions WHERE id = ?"
            " UNION"
            " SELECT s.id, s.parent_session_id, s.started_at"
            " FROM sessions s JOIN descendants d ON s.parent_session_id = d.id)"
            " SELECT id, parent_session_id, started_at FROM descendants",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, sid, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (nrows == cap) {
                cap = cap ? cap * 2 : 16;
                rows = realloc(rows, cap * sizeof *rows);
            }
            const unsigned char *id = sqlite3_column_text(st, 0);
            const unsigned char *pa = sqlite3_column_text(st, 1);
            rows[nrows].id = strdup(id ? (const char *)id : "");
            rows[nrows].parent = pa ? strdup((const char *)pa) : NULL;
            /* Python float(started_at or 0) with exception → 0.0 */
            rows[nrows].started_at =
                sqlite3_column_type(st, 2) == SQLITE_NULL
                    ? 0.0
                    : sqlite3_column_double(st, 2);
            nrows++;
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);

    /* Walk: current = sid; repeatedly pick unseen child with max started_at
     * (candidates.sort reverse → first). Python keeps SQL row order for
     * ties; sort is stable, so max-scan taking the FIRST max matches. */
    json_t *path = json_object_get(out, "path");
    json_append(path, json_string(sid));

    char **seen = malloc((nrows + 1) * sizeof *seen);
    size_t nseen = 0;
    seen[nseen++] = strdup(sid);
    char *current = strdup(sid);

    for (;;) {
        int best = -1;
        for (size_t i = 0; i < nrows; i++) {
            if (!rows[i].parent || !rows[i].id[0]) continue;
            if (strcmp(rows[i].parent, current) != 0) continue;
            bool was_seen = false;
            for (size_t k = 0; k < nseen && !was_seen; k++)
                if (strcmp(seen[k], rows[i].id) == 0) was_seen = true;
            if (was_seen) continue;
            if (best < 0 || rows[i].started_at > rows[best].started_at)
                best = (int)i;
        }
        if (best < 0) break;
        free(current);
        current = strdup(rows[best].id);
        json_append(path, json_string(current));
        seen = realloc(seen, (nseen + 1) * sizeof *seen);
        seen[nseen++] = strdup(current);
    }

    json_set(out, "latest", json_string(current));
    free(current);
    for (size_t k = 0; k < nseen; k++) free(seen[k]);
    free(seen);
    for (size_t i = 0; i < nrows; i++) {
        free(rows[i].id);
        free(rows[i].parent);
    }
    free(rows);
    free(sid);
    return out;
}

/* ── _resolve_chat_argv env assembly ────────────────────────────────────── */

static void env_setdefault(json_t *env, const char *k, const char *v) {
    if (!json_object_get(env, k)) json_set(env, k, json_string(v));
}

/* PoP: ws_chat_build_env @ hermes_cli/web_server.py:_resolve_chat_argv */
json_t *ws_chat_build_env(const json_t *base_env, const char *resume,
                          const char *sidecar_url, const char *profile_dir,
                          const char *active_session_file,
                          const char *gateway_ws_url) {
    json_t *env = base_env ? json_copy((json_t *)base_env) : json_object();

    env_setdefault(env, "NODE_ENV", "production");
    env_setdefault(env, "HERMES_TUI_DISABLE_MOUSE", "1");
    env_setdefault(env, "HERMES_TUI_INLINE", "1");
    env_setdefault(env, "COLORTERM", "truecolor");
    json_set(env, "HERMES_TUI_DASHBOARD", json_string("1"));

    if (profile_dir && *profile_dir)
        json_set(env, "HERMES_HOME", json_string(profile_dir));

    if (resume && *resume)
        json_set(env, "HERMES_TUI_RESUME", json_string(resume));

    if (sidecar_url && *sidecar_url)
        json_set(env, "HERMES_TUI_SIDECAR_URL", json_string(sidecar_url));

    if (active_session_file && *active_session_file)
        json_set(env, "HERMES_TUI_ACTIVE_SESSION_FILE",
                 json_string(active_session_file));

    /* Profile-scoped chats must NOT attach to the dashboard's in-memory
     * gateway. */
    if ((!profile_dir || !*profile_dir) && gateway_ws_url && *gateway_ws_url)
        json_set(env, "HERMES_TUI_GATEWAY_URL", json_string(gateway_ws_url));

    return env;
}
