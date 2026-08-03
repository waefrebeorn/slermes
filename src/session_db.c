/*
 * session_db.c — Session Database Operations
 *
 * All database operations for sessions and messages.
 */

#define _GNU_SOURCE
#include "session_db.h"
#include "app_state_internal.h"
#include "slermes_home.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>

static int db_query(app_state_t *app, const char *sql, int (*cb)(void*, int, char**, char**), void *user) {
    if (!app || !app->db) return -1;
    char *err = NULL;
    int rc = sqlite3_exec(app->db, sql, cb, user, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL: %s\nErr: %s\n", sql, err ? err : "?");
        sqlite3_free(err);
    }
    return rc;
}

static int cb_sessions(void *u, int argc, char **argv, char **cn) {
    (void)u;
    app_state_t *app = (app_state_t*)u;
    if (!app || app->session_count >= MAX_SESSIONS) return 0;
    app_session_entry_t *s = &app->sessions[app->session_count];
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < argc; i++) {
        if (!argv[i]) continue;
        if (!strcmp(cn[i],"id")) snprintf(s->id,sizeof(s->id),"%s",argv[i]);
        else if (!strcmp(cn[i],"title")) snprintf(s->title,sizeof(s->title),"%s",argv[i]);
        else if (!strcmp(cn[i],"source")) snprintf(s->source,sizeof(s->source),"%s",argv[i]);
        else if (!strcmp(cn[i],"model")) snprintf(s->model,sizeof(s->model),"%s",argv[i]);
        else if (!strcmp(cn[i],"message_count")) s->msg_count = atoi(argv[i]);
        else if (!strcmp(cn[i],"input_tokens")) s->tokens = atoi(argv[i]);
        else if (!strcmp(cn[i],"started_at")) s->started_at = (long)atof(argv[i]);
    }
    app->session_count++;
    return 0;
}

static int cb_messages(void *u, int argc, char **argv, char **cn) {
    (void)u;
    app_state_t *app = (app_state_t*)u;
    if (!app || app->message_count >= MAX_MESSAGES) return 0;
    message_entry_t *m = &app->messages[app->message_count];
    memset(m, 0, sizeof(*m));
    for (int i = 0; i < argc; i++) {
        if (!argv[i]) continue;
        if (!strcmp(cn[i],"role")) snprintf(m->role,sizeof(m->role),"%s",argv[i]);
        else if (!strcmp(cn[i],"content")) snprintf(m->content,sizeof(m->content),"%s",argv[i]);
        else if (!strcmp(cn[i],"timestamp")) m->timestamp = (long)(atof(argv[i])*1000);
    }
    app->message_count++;
    return 0;
}

static int cb_count(void *u, int c, char **v, char **cn) {
    (void)cn; int *out = (int*)u;
    if (c>0 && v[0]) *out = atoi(v[0]);
    return 0;
}

int session_db_open(app_state_t *app) {
    if (!app) return -1;
    if (!slermes_initialized()) slermes_init();
    snprintf(app->state_db_path, sizeof(app->state_db_path), "%s/%s",
             slermes_home(), SLERMES_FILE_STATE_DB);
    if (access(app->state_db_path, R_OK) != 0) {
        fprintf(stderr, "state.db not found at %s\n", app->state_db_path);
        return -1;
    }
    int rc = sqlite3_open_v2(app->state_db_path, &app->db,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to open state.db: %s\n", sqlite3_errmsg(app->db));
        return -1;
    }
    sqlite3_exec(app->db, "PRAGMA journal_mode=WAL; PRAGMA temp_store=memory;", NULL, NULL, NULL);
    return 0;
}

void session_db_close(app_state_t *app) {
    if (app && app->db) {
        sqlite3_close(app->db);
        app->db = NULL;
    }
}

void session_db_load_sessions(app_state_t *app) {
    if (!app) return;
    app->session_count = 0;
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "SELECT id, COALESCE(NULLIF(title,''),id) AS title, "
        "COALESCE(source,'cli') AS source, COALESCE(model,'') AS model, "
        "COALESCE(message_count,0) AS message_count, "
        "COALESCE(input_tokens,0) AS input_tokens, "
        "COALESCE(started_at,0) AS started_at "
        "FROM sessions WHERE parent_session_id IS NULL "
        "ORDER BY started_at DESC LIMIT %d", MAX_SESSIONS);
    db_query(app, buf, cb_sessions, app);
    if (app->session_count > 0)
        snprintf(app->latest_model, sizeof(app->latest_model), "%s", app->sessions[0].model);
}

void session_db_load_messages(app_state_t *app, int idx) {
    if (!app) return;
    app->message_count = 0;
    if (idx < 0 || idx >= app->session_count) return;
    
    char *sql = sqlite3_mprintf(
        "SELECT role,COALESCE(content,'') AS content,"
        "COALESCE(timestamp,0) AS timestamp "
        "FROM messages WHERE session_id='%q' "
        "ORDER BY timestamp ASC,id ASC LIMIT %d",
        app->sessions[idx].id, MAX_MESSAGES);
    
    if (sql) {
        db_query(app, sql, cb_messages, app);
        sqlite3_free(sql);
    }
}

/* PoP: _get_session @ tools/browser_camofox.py:_get_session */
app_session_entry_t *session_db_get_session(app_state_t *app, int idx) {
    if (!app || idx < 0 || idx >= app->session_count) return NULL;
    return &app->sessions[idx];
}

message_entry_t *session_db_get_message(app_state_t *app, int idx) {
    if (!app || idx < 0 || idx >= app->message_count) return NULL;
    return &app->messages[idx];
}

bool session_db_update_session_model(app_state_t *app, int session_idx, const char *model) {
    if (!app || !model || session_idx < 0 || session_idx >= app->session_count) return false;
    
    app_session_entry_t *s = &app->sessions[session_idx];
    char *zErr = NULL;
    char *sql = sqlite3_mprintf("UPDATE sessions SET model='%q' WHERE id='%q'", model, s->id);
    
    bool result = false;
    if (sql) {
        sqlite3_exec(app->db, sql, NULL, NULL, &zErr);
        sqlite3_free(sql);
        if (zErr) { sqlite3_free(zErr); }
        else {
            strncpy(s->model, model, sizeof(s->model)-1);
            result = true;
        }
    }
    return result;
}

bool session_db_delete_session(app_state_t *app, int session_idx) {
    if (!app || session_idx < 0 || session_idx >= app->session_count) return false;
    
    app_session_entry_t *s = &app->sessions[session_idx];
    char *zErr = NULL;
    char *sql = sqlite3_mprintf("DELETE FROM sessions WHERE id='%q'", s->id);
    
    bool result = false;
    if (sql) {
        sqlite3_exec(app->db, sql, NULL, NULL, &zErr);
        sqlite3_free(sql);
        if (zErr) { sqlite3_free(zErr); }
        else {
            session_db_load_sessions(app);
            result = true;
        }
    }
    return result;
}

int session_db_create_session(app_state_t *app, const char *title, const char *source, const char *model) {
    if (!app) return -1;
    
    double now_t = (double)time(NULL);
    char *zErr = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO sessions (id, title, source, model, started_at, message_count, input_tokens) "
        "VALUES ('%llx', '%q', '%q', '%q', %f, 0, 0)",
        (unsigned long long)now_t, title ? title : "", source ? source : "cli", model ? model : "", now_t);
    
    int result = -1;
    if (sql) {
        sqlite3_exec(app->db, sql, NULL, NULL, &zErr);
        sqlite3_free(sql);
        if (zErr) { sqlite3_free(zErr); }
        else {
            result = 0;
            session_db_load_sessions(app); /* Reload to get the new session */
        }
    }
    return result;
}

/* PoP: desktop_session_import @ apps/desktop/src/app/session/index.tsx */
int session_db_create_named(app_state_t *app, const char *id, const char *title,
                            const char *source, const char *model) {
    if (!id || !*id) return 0;
    double now_t = (double)time(NULL);
    sqlite3 *db = NULL;
    int close_db = 0;
    if (app && app->db) {
        db = app->db;
    } else {
        char db_path[1024];
        snprintf(db_path, sizeof(db_path), "%s/%s", slermes_home(), SLERMES_FILE_STATE_DB);
        if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
            if (db) sqlite3_close(db);
            return 0;
        }
        close_db = 1;
    }
    char *zErr = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT OR REPLACE INTO sessions (id, title, source, model, started_at, message_count, input_tokens) "
        "VALUES ('%q', '%q', '%q', '%q', %f, 0, 0)",
        id, title ? title : "", source ? source : "import", model ? model : "", now_t);
    int ok = 0;
    if (sql) {
        sqlite3_exec(db, sql, NULL, NULL, &zErr);
        sqlite3_free(sql);
        if (zErr) { sqlite3_free(zErr); }
        else ok = 1;
    }
    if (close_db) sqlite3_close(db);
    if (ok && app) session_db_load_sessions(app);
    return ok;
}

int session_db_insert_message(const char *session_id, const char *role,
                              const char *content, double timestamp) {
    if (!session_id || !*session_id || !role) return 0;
    if (!content) content = "";
    /* Open a throwaway read-write connection to the state DB. */
    char db_path[1024];
    snprintf(db_path, sizeof(db_path), "%s/%s", slermes_home(), SLERMES_FILE_STATE_DB);
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }
    char *zErr = NULL;
    char *sql = sqlite3_mprintf(
        "INSERT INTO messages (session_id, role, content, timestamp) "
        "VALUES ('%q', '%q', '%q', %f)",
        session_id, role, content, timestamp);
    int ok = 0;
    if (sql) {
        sqlite3_exec(db, sql, NULL, NULL, &zErr);
        sqlite3_free(sql);
        if (zErr) { sqlite3_free(zErr); }
        else ok = 1;
    }
    sqlite3_close(db);
    return ok;
}

bool session_db_archive_session(app_state_t *app, int session_idx, bool archive) {
    if (!app || session_idx < 0 || session_idx >= app->session_count) return false;
    
    app_session_entry_t *s = &app->sessions[session_idx];
    char *zErr = NULL;
    char *sql = sqlite3_mprintf("UPDATE sessions SET archived=%d WHERE id='%q'", archive ? 1 : 0, s->id);
    
    bool result = false;
    if (sql) {
        sqlite3_exec(app->db, sql, NULL, NULL, &zErr);
        sqlite3_free(sql);
        if (zErr) { sqlite3_free(zErr); }
        else result = true;
    }
    return result;
}

bool session_db_pin_session(app_state_t *app, int session_idx, bool pin) {
    if (!app || session_idx < 0 || session_idx >= app->session_count) return false;
    
    if (pin) {
        if (app->pinned_session_count < MAX_SESSIONS) {
            app->pinned_session_ids[app->pinned_session_count++] = session_idx;
        }
    } else {
        for (int i = 0; i < app->pinned_session_count; i++) {
            if (app->pinned_session_ids[i] == session_idx) {
                memmove(&app->pinned_session_ids[i], &app->pinned_session_ids[i+1],
                        (app->pinned_session_count - i - 1) * sizeof(int));
                app->pinned_session_count--;
                break;
            }
        }
    }
    return true;
}

void session_db_load_skills(app_state_t *app) {
    if (!app) return;
    app->skill_count = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), SLERMES_DIR_SKILLS);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && app->skill_count < MAX_ITEMS) {
        if (de->d_name[0] == '.') continue;
        snprintf(app->skill_names[app->skill_count], sizeof(app->skill_names[0]), "%s", de->d_name);
        app->skill_count++;
    }
    closedir(d);
}

void session_db_load_profiles(app_state_t *app) {
    if (!app) return;
    app->profile_count = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), SLERMES_DIR_PROFILES);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)) && app->profile_count < MAX_ITEMS) {
        if (de->d_name[0] == '.') continue;
        snprintf(app->profile_names[app->profile_count], sizeof(app->profile_names[0]), "%s", de->d_name);
        app->profile_count++;
    }
    closedir(d);
}

void session_db_load_cron(app_state_t *app) {
    if (!app) return;
    app->cron_count = 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", slermes_home(), SLERMES_FILE_CRON);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        const char *p = strstr(line, "\"name\"");
        if (!p) continue;
        p = strchr(p+6, '"'); if (!p) continue;
        p++;
        const char *end = strchr(p, '"'); if (!end) continue;
        if (app->cron_count >= MAX_ITEMS) break;
        size_t len = end - p; if (len > 120) len = 120;
        memcpy(app->cron_names[app->cron_count], p, len);
        app->cron_names[app->cron_count][len] = '\0';
        app->cron_count++;
    }
    fclose(f);
}

void session_db_load_stats(app_state_t *app) {
    if (!app) return;
    db_query(app, "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL", cb_count, &app->total_sessions);
    db_query(app, "SELECT COUNT(*) FROM messages", cb_count, &app->total_messages);
}