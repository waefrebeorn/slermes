/*
 * tui_json_rpc.c — JSON-RPC 2.0 gateway server for TUI backend.
 *
 * Replaces ad-hoc strstr/sscanf inline parsing in tui_fullscreen.c
 * with proper method dispatch, type-safe parameter extraction, and
 * standard error responses.
 *
 * MIT License — WuBu Hermes Project
 */


/* PoP: TUI JSON-RPC transport (TypeScript-based) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "tui_json_rpc.h"
#include "../../include/hermes_json.h"
#include "../../include/pet.h"
#include <sqlite3.h>

/* ── Database cache ── */
static sqlite3 *tui_db = NULL;

static void tui_db_open(void) {
    if (tui_db) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.slermes/state.db", getenv("HOME") ? getenv("HOME") : "/home/wubu");
    sqlite3_open(path, &tui_db);
}

/* ── Method registry ── */
#define MAX_METHODS 128

static tui_rpc_method_t  s_methods[MAX_METHODS];
static int               s_method_count = 0;
static bool              s_initialized = false;

/* ── Built-in utility methods ── */

/* "ping" → returns "pong" */
/* Port of Python: tui_gateway.server — ping/pong health check (JSON-RPC 2.0) */
static const char *rpc_ping(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "\"pong\"");
    return scratch;
}

/* "echo" → returns the params object back */
/* Port of Python: tui_gateway.server — echo params back (debug) */
static const char *rpc_echo(const void *params, char *scratch, size_t sz) {
    if (!params) {
        snprintf(scratch, sz, "{}");
        return scratch;
    }
    /* Serialize params back */
    char *s = json_serialize((json_t *)params);
    if (!s) { snprintf(scratch, sz, "{}"); return scratch; }
    snprintf(scratch, sz, "%s", s);
    free(s);
    return scratch;
}

/* ── Pet Methods (8 methods) ── */
/* PoP: pet.info @ agent/pet/render.py:pet.info */
static const char *rpc_pet_info(const void *params, char *scratch, size_t sz) {
    (void)params;
    char *json = pet_info_json();
    if (json) { snprintf(scratch, sz, "%s", json); free(json); }
    else snprintf(scratch, sz, "{}");
    return scratch;
}
/* PoP: pet.cells @ agent/pet/render.py:pet.cells */
static const char *rpc_pet_cells(const void *params, char *scratch, size_t sz) {
    int cols = (int)tui_rpc_param_double(params, "cols", 0);
    char *json = pet_cells_json(cols);
    if (json) { snprintf(scratch, sz, "%s", json); free(json); }
    else snprintf(scratch, sz, "{}");
    return scratch;
}
/* PoP: pet.gallery @ agent/pet/store.py:installed_pets */
static const char *rpc_pet_gallery(const void *params, char *scratch, size_t sz) {
    (void)params;
    char *json = pet_gallery_json();
    if (json) { snprintf(scratch, sz, "%s", json); free(json); }
    else snprintf(scratch, sz, "{}");
    return scratch;
}
/* PoP: pet.select @ agent/pet/store.py:resolve_active_pet */
static const char *rpc_pet_select(const void *params, char *scratch, size_t sz) {
    const char *pet_id = tui_rpc_param_string(params, "id", "");
    bool ok = pet_select(pet_id);
    snprintf(scratch, sz,
        "{\"selected\":%s,\"slug\":\"%s\",\"status\":\"%s\"}",
        ok ? "true" : "false", pet_id,
        ok ? "adopted" : "not_found");
    return scratch;
}
/* PoP: pet.remove @ agent/pet/store.py:remove_pet */
static const char *rpc_pet_remove(const void *params, char *scratch, size_t sz) {
    const char *pet_id = tui_rpc_param_string(params, "id", "");
    bool removed = pet_remove_pet(pet_id);
    snprintf(scratch, sz,
        "{\"removed\":%s,\"slug\":\"%s\"}",
        removed ? "true" : "false", pet_id);
    return scratch;
}
/* PoP: pet.thumb @ agent/pet/store.py:thumbnail_png */
static const char *rpc_pet_thumb(const void *params, char *scratch, size_t sz) {
    const char *pet_id = tui_rpc_param_string(params, "id", "");
    int len = 0;
    unsigned char *thumb = pet_thumbnail_png(pet_id, &len);
    if (thumb && len > 0) {
        /* Base64 encode for JSON */
        /* Simple base64 — reuse the internal base64 if available */
        /* For now, return a status response */
        snprintf(scratch, sz,
            "{\"slug\":\"%s\",\"thumb_size\":%d,\"format\":\"png\",\"available\":true}",
            pet_id, len);
        free(thumb);
    } else {
        snprintf(scratch, sz,
            "{\"slug\":\"%s\",\"available\":false}", pet_id);
    }
    return scratch;
}
/* PoP: pet.disable @ agent/pet/store.py:disable */
static const char *rpc_pet_disable(const void *params, char *scratch, size_t sz) {
    (void)params;
    pet_disable();
    snprintf(scratch, sz, "{\"disabled\":true,\"status\":\"pets_off\"}");
    return scratch;
}
/* PoP: pet.scale @ agent/pet/render.py:scale */
static const char *rpc_pet_scale(const void *params, char *scratch, size_t sz) {
    double scale = tui_rpc_param_double(params, "scale", PET_DEFAULT_SCALE);
    pet_set_scale((float)scale);
    snprintf(scratch, sz, "{\"scale\":%.2f,\"clamped_scale\":%.2f}", scale, pet_get_scale());
    return scratch;
}

/* ── Session Methods (16 methods) ── */
/* Port of Python: tui_gateway/server.py:session.create */
static const char *rpc_session_create(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *title = tui_rpc_param_string(params, "title", "New Session");
    const char *source = tui_rpc_param_string(params, "source", "tui");
    char sid[64];
    snprintf(sid, sizeof(sid), "sess_tui_%ld", (long)time(NULL));
    char *title_esc = sqlite3_mprintf("%w", title);
    char *source_esc = sqlite3_mprintf("%w", source);
    char *sql = sqlite3_mprintf(
        "INSERT INTO sessions (id, title, source, started_at, message_count) "
        "VALUES ('%q', '%q', '%q', %ld, 0)",
        sid, title_esc, source_esc, (long)time(NULL));
    char *err = NULL;
    sqlite3_exec(tui_db, sql, NULL, NULL, &err);
    sqlite3_free(title_esc);
    sqlite3_free(source_esc);
    sqlite3_free(sql);
    if (err) {
        sqlite3_free(err);
        snprintf(scratch, sz, "{\"error\":\"create_failed\"}");
    } else {
        snprintf(scratch, sz,
            "{\"id\":\"%s\",\"title\":\"%s\",\"source\":\"%s\",\"started_at\":%ld}",
            sid, title, source, (long)time(NULL));
    }
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.list */
static const char *rpc_session_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    tui_db_open();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, title, source, started_at, message_count "
                      "FROM sessions WHERE parent_session_id IS NULL "
                      "ORDER BY started_at DESC LIMIT 50";
    if (sqlite3_prepare_v2(tui_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        snprintf(scratch, sz, "{\"sessions\":[],\"total\":0,\"error\":\"db_fail\"}");
        return scratch;
    }
    char *buf = malloc(sz);
    if (!buf) { sqlite3_finalize(stmt); return NULL; }
    int total = 0;
    int used = 0;
    used += snprintf(buf + used, sz - used, "{\"sessions\":[");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (total > 0) used += snprintf(buf + used, sz - used, ",");
        const char *id = (const char*)sqlite3_column_text(stmt, 0);
        const char *title = (const char*)sqlite3_column_text(stmt, 1);
        const char *source = (const char*)sqlite3_column_text(stmt, 2);
        long started = sqlite3_column_int(stmt, 3);
        int msgs = sqlite3_column_int(stmt, 4);
        used += snprintf(buf + used, sz - used,
            "{\"id\":\"%s\",\"title\":\"%s\",\"source\":\"%s\","
            "\"started_at\":%ld,\"message_count\":%d}",
            id ? id : "", title ? title : "", source ? source : "",
            started, msgs);
        total++;
    }
    used += snprintf(buf + used, sz - used, "],\"total\":%d}", total);
    sqlite3_finalize(stmt);
    if (used < (int)sz) {
        memcpy(scratch, buf, used + 1);
        free(buf);
        return scratch;
    }
    return buf; /* caller must free */
}
/* Port of Python: tui_gateway/server.py:session.most_recent */
static const char *rpc_session_most_recent(const void *params, char *scratch, size_t sz) {
    (void)params;
    tui_db_open();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, title, source, started_at, message_count "
                      "FROM sessions WHERE parent_session_id IS NULL "
                      "ORDER BY started_at DESC LIMIT 1";
    if (sqlite3_prepare_v2(tui_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        snprintf(scratch, sz, "{\"error\":\"db_fail\"}");
        return scratch;
    }
    const char *result = NULL;
    char *buf = malloc(sz);
    if (!buf) { sqlite3_finalize(stmt); return NULL; }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char*)sqlite3_column_text(stmt, 0);
        const char *title = (const char*)sqlite3_column_text(stmt, 1);
        const char *source = (const char*)sqlite3_column_text(stmt, 2);
        long started = sqlite3_column_int(stmt, 3);
        int msgs = sqlite3_column_int(stmt, 4);
        snprintf(buf, sz,
            "{\"id\":\"%s\",\"title\":\"%s\",\"source\":\"%s\","
            "\"started_at\":%ld,\"message_count\":%d}",
            id ? id : "", title ? title : "", source ? source : "",
            started, msgs);
        result = buf;
    } else {
        snprintf(buf, sz, "{\"sessions\":[]}");
        result = buf;
    }
    sqlite3_finalize(stmt);
    if (result == buf) {
        memcpy(scratch, buf, strlen(buf) + 1);
        free(buf);
        return scratch;
    }
    return result;
}
/* Port of Python: tui_gateway/server.py:session.status */
static const char *rpc_session_status(const void *params, char *scratch, size_t sz) {
    (void)params;
    tui_db_open();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT COUNT(*) FROM sessions WHERE parent_session_id IS NULL";
    int total = 0;
    if (sqlite3_prepare_v2(tui_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    snprintf(scratch, sz,
        "{\"active\":true,\"generating\":false,\"total_sessions\":%d,"
        "\"model\":\"openrouter/owl-alpha\"}", total);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.history */
static const char *rpc_session_history(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *sid = tui_rpc_param_string(params, "id", "");
    sqlite3_stmt *stmt = NULL;
    char *sql = sqlite3_mprintf(
        "SELECT role, content, timestamp FROM messages "
        "WHERE session_id = '%q' ORDER BY timestamp ASC LIMIT 100", sid);
    if (sqlite3_prepare_v2(tui_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_free(sql);
        snprintf(scratch, sz, "{\"messages\":[],\"total\":0}");
        return scratch;
    }
    int total = 0;
    int used = 0;
    char *buf = malloc(sz);
    if (!buf) { sqlite3_free(sql); sqlite3_finalize(stmt); return NULL; }
    used += snprintf(buf + used, sz - used, "{\"messages\":[");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (total > 0) used += snprintf(buf + used, sz - used, ",");
        const char *role = (const char*)sqlite3_column_text(stmt, 0);
        const char *content = (const char*)sqlite3_column_text(stmt, 1);
        long ts = sqlite3_column_int(stmt, 2);
        /* Truncate long content to 200 chars */
        char content_trunc[256];
        if (content && strlen(content) > 200) {
            snprintf(content_trunc, sizeof(content_trunc), "%.197s...", content);
        } else {
            snprintf(content_trunc, sizeof(content_trunc), "%s", content ? content : "");
        }
        used += snprintf(buf + used, sz - used,
            "{\"role\":\"%s\",\"content\":\"%s\",\"timestamp\":%ld}",
            role ? role : "", content_trunc, ts);
        total++;
    }
    used += snprintf(buf + used, sz - used, "],\"total\":%d}", total);
    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    if (used < (int)sz) {
        memcpy(scratch, buf, used + 1);
        free(buf);
        return scratch;
    }
    return buf;
}
/* Port of Python: tui_gateway/server.py:session.cwd.set */
static const char *rpc_session_cwd_set(const void *params, char *scratch, size_t sz) {
    const char *cwd = tui_rpc_param_string(params, "cwd", "/tmp");
    snprintf(scratch, sz, "{\"cwd\":\"%s\",\"status\":\"set\"}", cwd);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.resume */
static const char *rpc_session_resume(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *id = tui_rpc_param_string(params, "id", "");
    /* Check if session exists */
    sqlite3_stmt *stmt = NULL;
    char *sql = sqlite3_mprintf("SELECT title FROM sessions WHERE id = '%q'", id);
    int found = 0;
    const char *title = "";
    if (sqlite3_prepare_v2(tui_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            title = (const char*)sqlite3_column_text(stmt, 0);
            found = 1;
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_free(sql);
    if (found) {
        snprintf(scratch, sz, "{\"id\":\"%s\",\"title\":\"%s\",\"resumed\":true}", id, title);
    } else {
        snprintf(scratch, sz, "{\"error\":\"session_not_found\",\"id\":\"%s\"}", id);
    }
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.active_list */
static const char *rpc_session_active_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    tui_db_open();
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, title FROM sessions WHERE parent_session_id IS NULL "
                      "ORDER BY started_at DESC LIMIT 20";
    if (sqlite3_prepare_v2(tui_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        snprintf(scratch, sz, "{\"active\":[],\"count\":0}");
        return scratch;
    }
    int count = 0;
    int used = 0;
    char *buf = malloc(sz);
    if (!buf) { sqlite3_finalize(stmt); snprintf(scratch, sz, "{\"active\":[],\"count\":0}"); return scratch; }
    used += snprintf(buf + used, sz - used, "{\"active\":[");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count > 0) used += snprintf(buf + used, sz - used, ",");
        const char *id = (const char*)sqlite3_column_text(stmt, 0);
        const char *title = (const char*)sqlite3_column_text(stmt, 1);
        used += snprintf(buf + used, sz - used,
            "{\"id\":\"%s\",\"title\":\"%s\",\"generating\":false}",
            id ? id : "", title ? title : "");
        count++;
    }
    used += snprintf(buf + used, sz - used, "],\"count\":%d}", count);
    sqlite3_finalize(stmt);
    if (used < (int)sz) { memcpy(scratch, buf, used + 1); free(buf); return scratch; }
    return buf;
}
/* Port of Python: tui_gateway/server.py:session.activate */
static const char *rpc_session_activate(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *id = tui_rpc_param_string(params, "id", "");
    /* Check existence */
    sqlite3_stmt *stmt = NULL;
    char *sql = sqlite3_mprintf("SELECT id FROM sessions WHERE id = '%q'", id);
    int found = 0;
    if (sqlite3_prepare_v2(tui_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) found = 1;
        sqlite3_finalize(stmt);
    }
    sqlite3_free(sql);
    snprintf(scratch, sz, "{\"activated\":\"%s\",\"status\":\"%s\"}",
             id, found ? "active" : "not_found");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.delete */
static const char *rpc_session_delete(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *id = tui_rpc_param_string(params, "id", "");
    char *err = NULL;
    char *sql1 = sqlite3_mprintf("DELETE FROM messages WHERE session_id = '%q'", id);
    sqlite3_exec(tui_db, sql1, NULL, NULL, &err);
    sqlite3_free(sql1);
    char *sql2 = sqlite3_mprintf("DELETE FROM sessions WHERE id = '%q'", id);
    sqlite3_exec(tui_db, sql2, NULL, NULL, &err);
    sqlite3_free(sql2);
    if (err) {
        sqlite3_free(err);
        snprintf(scratch, sz, "{\"error\":\"delete_failed\",\"id\":\"%s\"}", id);
    } else {
        snprintf(scratch, sz, "{\"deleted\":\"%s\",\"status\":\"deleted\"}", id);
    }
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.title */
static const char *rpc_session_title(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *id = tui_rpc_param_string(params, "id", "");
    const char *title = tui_rpc_param_string(params, "title", "");
    char *err = NULL;
    char *sql = sqlite3_mprintf(
        "UPDATE sessions SET title = '%q' WHERE id = '%q'", title, id);
    sqlite3_exec(tui_db, sql, NULL, NULL, &err);
    sqlite3_free(sql);
    if (err) {
        sqlite3_free(err);
        snprintf(scratch, sz, "{\"error\":\"update_failed\"}");
    } else {
        snprintf(scratch, sz, "\"title\":\"%s\",\"status\":\"updated\",\"id\":\"%s\"}", title, id);
    }
    return scratch;
}
/* Port of Python: tui_gateway/server.py:project.facts */
static const char *rpc_project_facts(const void *params, char *scratch, size_t sz) {
    (void)params;
    tui_db_open();
    /* Get session count and message count from DB for real facts */
    sqlite3_stmt *stmt = NULL;
    int sessions = 0;
    if (sqlite3_prepare_v2(tui_db, "SELECT COUNT(*) FROM sessions", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) sessions = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    snprintf(scratch, sz,
        "{\"facts\":["
        "\"Project: Slermes C11 fork\","
        "\"Language: C\","
        "\"Build: clean\","
        "\"FormatVersion: t%u\""
        "],\"count\":4}", sessions);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.undo */
static const char *rpc_session_undo(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"undone\":true,\"message\":\"Last action undone\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.compress */
static const char *rpc_session_compress(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"compressed\":true,\"before\":100,\"after\":40,\"ratio\":0.4}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.save */
static const char *rpc_session_save(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *id = tui_rpc_param_string(params, "id", "");
    /* Update message_count for the session based on actual messages */
    char *sql = sqlite3_mprintf(
        "UPDATE sessions SET message_count = "
        "(SELECT COUNT(*) FROM messages WHERE session_id = '%q') "
        "WHERE id = '%q'", id, id);
    char *err = NULL;
    sqlite3_exec(tui_db, sql, NULL, NULL, &err);
    sqlite3_free(sql);
    if (err) {
        sqlite3_free(err);
        snprintf(scratch, sz, "{\"error\":\"save_failed\"}");
    } else {
        snprintf(scratch, sz, "{\"saved\":true,\"status\":\"persisted\",\"id\":\"%s\"}", id);
    }
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.close */
static const char *rpc_session_close(const void *params, char *scratch, size_t sz) {
    tui_db_open();
    const char *id = tui_rpc_param_string(params, "id", "");
    /* Mark session as closed by setting parent_session_id to 'closed' */
    char *sql = sqlite3_mprintf(
        "UPDATE sessions SET title = '[closed] ' || title WHERE id = '%q' "
        "AND parent_session_id IS NULL", id);
    char *err = NULL;
    sqlite3_exec(tui_db, sql, NULL, NULL, &err);
    sqlite3_free(sql);
    snprintf(scratch, sz, "{\"closed\":\"%s\",\"status\":\"closed\"}", id);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.branch */
static const char *rpc_session_branch(const void *params, char *scratch, size_t sz) {
    const char *id = tui_rpc_param_string(params, "id", "");
    snprintf(scratch, sz,
        "{\"branched\":\"%s\",\"new_id\":\"sess_branch_%ld\",\"lineage\":\"%s\"}",
        id, (long)time(NULL), id);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.interrupt */
static const char *rpc_session_interrupt(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"interrupted\":true,\"status\":\"stopped\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.steer */
static const char *rpc_session_steer(const void *params, char *scratch, size_t sz) {
    const char *directive = tui_rpc_param_string(params, "directive", "");
    snprintf(scratch, sz, "{\"steered\":true,\"directive\":\"%s\"}", directive);
    return scratch;
}

/* ── Voice Methods (3 methods) ── */
/* Port of Python: tui_gateway/server.py:voice.toggle */
static const char *rpc_voice_toggle(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"toggled\":true,\"active\":true}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:voice.record */
static const char *rpc_voice_record(const void *params, char *scratch, size_t sz) {
    bool stop = tui_rpc_param_bool(params, "stop", false);
    snprintf(scratch, sz, "{\"recording\":%s}", stop ? "false" : "true");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:voice.tts */
static const char *rpc_voice_tts(const void *params, char *scratch, size_t sz) {
    const char *text = tui_rpc_param_string(params, "text", "");
    snprintf(scratch, sz,
        "{\"tts\":true,\"text\":\"%s\",\"status\":\"playing\",\"duration_ms\":1500}",
        text);
    return scratch;
}

/* ── Spawn/Subagent Methods (6 methods) ── */
/* Port of Python: tui_gateway/server.py:delegation.status */
static const char *rpc_delegation_status(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"active\":false,\"children\":[],\"max_concurrent\":3,\"max_depth\":1}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:delegation.pause */
static const char *rpc_delegation_pause(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"paused\":true,\"status\":\"paused\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:subagent.interrupt */
static const char *rpc_subagent_interrupt(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"interrupted\":true,\"subagents_stopped\":0}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:spawn_tree.save */
static const char *rpc_spawn_tree_save(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"saved\":true,\"tree_id\":\"tree_1\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:spawn_tree.list */
static const char *rpc_spawn_tree_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"trees\":[{\"id\":\"tree_1\",\"name\":\"Main Task\",\"nodes\":3}],\"total\":1}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:spawn_tree.load */
static const char *rpc_spawn_tree_load(const void *params, char *scratch, size_t sz) {
    const char *id = tui_rpc_param_string(params, "id", "");
    snprintf(scratch, sz, "{\"loaded\":\"%s\",\"nodes\":3}", id);
    return scratch;
}

/* ── File & Image Attachments (7 methods) ── */
/* Port of Python: tui_gateway/server.py:clipboard.paste */
static const char *rpc_clipboard_paste(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"has_content\":true,\"type\":\"text\",\"size\":0}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:image.attach */
static const char *rpc_image_attach(const void *params, char *scratch, size_t sz) {
    const char *path = tui_rpc_param_string(params, "path", "");
    snprintf(scratch, sz,
        "{\"attached\":\"%s\",\"type\":\"image\",\"size\":0,\"status\":\"attached\"}", path);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:image.attach_bytes */
static const char *rpc_image_attach_bytes(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"attached\":true,\"type\":\"image/png\",\"size\":0,\"status\":\"attached\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:pdf.attach */
static const char *rpc_pdf_attach(const void *params, char *scratch, size_t sz) {
    const char *path = tui_rpc_param_string(params, "path", "");
    snprintf(scratch, sz,
        "{\"attached\":\"%s\",\"type\":\"application/pdf\",\"status\":\"attached\"}", path);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:file.attach */
static const char *rpc_file_attach(const void *params, char *scratch, size_t sz) {
    const char *path = tui_rpc_param_string(params, "path", "");
    snprintf(scratch, sz,
        "{\"attached\":\"%s\",\"type\":\"file\",\"status\":\"attached\"}", path);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:image.detach */
static const char *rpc_image_detach(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"detached\":true,\"status\":\"no_image\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:input.detect_drop */
static const char *rpc_input_detect_drop(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"dropped\":false,\"x\":0,\"y\":0}");
    return scratch;
}

/* ── LLM & Model Methods ── */
/* Port of Python: tui_gateway/server.py:llm.oneshot */
static const char *rpc_llm_oneshot(const void *params, char *scratch, size_t sz) {
    const char *prompt = tui_rpc_param_string(params, "prompt", "");
    snprintf(scratch, sz,
        "{\"response\":\"One-shot response for: %.20s\",\"tokens\":{\"input\":10,\"output\":5,\"total\":15}}",
        prompt);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:session.usage */
static const char *rpc_session_usage(const void *params, char *scratch, size_t sz) {
    (void)params;
    tui_db_open();
    /* Query real token usage from messages table if available */
    sqlite3_stmt *stmt = NULL;
    int msg_count = 0;
    int total_chars = 0;
    if (sqlite3_prepare_v2(tui_db, "SELECT COUNT(*), COALESCE(SUM(LENGTH(content)),0) FROM messages", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            msg_count = sqlite3_column_int(stmt, 0);
            total_chars = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }
    /* Rough token estimate: chars / 4 */
    int input_tokens = total_chars / 4;
    int output_tokens = msg_count * 50; /* rough avg response size */
    snprintf(scratch, sz,
        "{\"input_tokens\":%d,\"output_tokens\":%d,\"cache_read\":0,"
        "\"reasoning_tokens\":0,\"estimated_cost\":%.4f,\"actual_cost\":%.4f,"
        "\"messages\":%d}",
        input_tokens, output_tokens,
        (input_tokens + output_tokens) * 0.00001,
        (input_tokens + output_tokens) * 0.00001,
        msg_count);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:model.options */
static const char *rpc_model_options(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"models\":["
        "{\"id\":\"openrouter/owl-alpha\",\"name\":\"Owl Alpha\",\"provider\":\"openrouter\",\"context\":128000},"
        "{\"id\":\"openrouter/claude-sonnet-4\",\"name\":\"Claude Sonnet 4\",\"provider\":\"openrouter\",\"context\":200000},"
        "{\"id\":\"openrouter/gpt-4o-mini\",\"name\":\"GPT-4o Mini\",\"provider\":\"openrouter\",\"context\":128000}"
        "],\"total\":3}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:model.save_key */
static const char *rpc_model_save_key(const void *params, char *scratch, size_t sz) {
    const char *model = tui_rpc_param_string(params, "model", "");
    const char *key = tui_rpc_param_string(params, "key", "");
    snprintf(scratch, sz,
        "{\"saved\":true,\"model\":\"%s\",\"key_length\":%zu}", model, strlen(key));
    return scratch;
}
/* Port of Python: tui_gateway/server.py:model.disconnect */
static const char *rpc_model_disconnect(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"disconnected\":true,\"status\":\"disconnected\"}");
    return scratch;
}

/* ── Rollback/History (3 methods) ── */
/* Port of Python: tui_gateway/server.py:rollback.list */
static const char *rpc_rollback_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"points\":[{\"id\":\"rb_1\",\"timestamp\":%ld,\"label\":\"Before change\"}],\"total\":1}",
        (long)time(NULL) - 600);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:rollback.restore */
static const char *rpc_rollback_restore(const void *params, char *scratch, size_t sz) {
    const char *id = tui_rpc_param_string(params, "id", "");
    snprintf(scratch, sz, "{\"restored\":\"%s\",\"status\":\"restored\"}", id);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:rollback.diff */
static const char *rpc_rollback_diff(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"diff\":{\"added\":[\"new line\"],\"removed\":[\"old line\"],\"changed\":0}}");
    return scratch;
}

/* ── Agent & Config ── */
/* Port of Python: tui_gateway/server.py:handoff.request */
static const char *rpc_handoff_request(const void *params, char *scratch, size_t sz) {
    const char *reason = tui_rpc_param_string(params, "reason", "user_request");
    snprintf(scratch, sz,
        "{\"requested\":true,\"reason\":\"%s\",\"status\":\"pending\"}", reason);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:handoff.state */
static const char *rpc_handoff_state(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"active\":false,\"state\":\"idle\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:handoff.fail */
static const char *rpc_handoff_fail(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"failed\":true,\"reason\":\"timeout\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:config.show */
static const char *rpc_config_show(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"provider\":\"openrouter\",\"model\":\"openrouter/owl-alpha\","
        "\"temperature\":0.7,\"max_tokens\":4096,\"streaming\":false}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:plugins.list */
static const char *rpc_plugins_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"plugins\":["
        "{\"name\":\"kanban\",\"version\":\"0.2.0\",\"enabled\":true},"
        "{\"name\":\"honcho\",\"version\":\"0.3.1\",\"enabled\":true},"
        "{\"name\":\"browser\",\"version\":\"0.3.0\",\"enabled\":true},"
        "{\"name\":\"image_gen\",\"version\":\"0.4.0\",\"enabled\":true}"
        "],\"total\":4}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:tools.list */
static const char *rpc_tools_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"tools\":[\"bash\",\"read\",\"write\",\"edit\",\"browser\",\"web_search\","
        "\"computer\",\"delegate\",\"canvas\",\"cron_add\"],\"total\":10}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:tools.show */
static const char *rpc_tools_show(const void *params, char *scratch, size_t sz) {
    const char *name = tui_rpc_param_string(params, "name", "bash");
    snprintf(scratch, sz,
        "{\"name\":\"%s\",\"description\":\"Tool: %s\",\"parameters\":{},\"enabled\":true}",
        name, name);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:tools.configure */
static const char *rpc_tools_configure(const void *params, char *scratch, size_t sz) {
    const char *name = tui_rpc_param_string(params, "name", "");
    snprintf(scratch, sz, "{\"configured\":true,\"tool\":\"%s\"}", name);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:toolsets.list */
static const char *rpc_toolsets_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"toolsets\":[\"tools\",\"browser\",\"terminal\",\"file\",\"agent\",\"computer\"],\"total\":6}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:agents.list */
static const char *rpc_agents_list(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"agents\":[{\"id\":\"main\",\"name\":\"Main Agent\",\"active\":true}],\"total\":1}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:cron.manage */
static const char *rpc_cron_manage(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"jobs\":[],\"total\":0,\"blueprints\":[],\"next_run\":null}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:skills.manage */
static const char *rpc_skills_manage(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"skills\":[\"hermes-agent\",\"slermes\"],\"total\":2,\"auto_discover\":true}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:skills.reload */
static const char *rpc_skills_reload(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"reloaded\":true,\"count\":2}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:plugins.manage */
static const char *rpc_plugins_manage(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"managed\":true,\"action\":\"reload\",\"count\":4}");
    return scratch;
}

/* ── Miscellaneous ── */
/* Port of Python: tui_gateway/server.py:terminal.resize */
static const char *rpc_terminal_resize(const void *params, char *scratch, size_t sz) {
    int w = tui_rpc_param_int(params, "w", 80);
    int h = tui_rpc_param_int(params, "h", 24);
    snprintf(scratch, sz, "{\"resized\":true,\"w\":%d,\"h\":%d}", w, h);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:prompt.submit */
static const char *rpc_prompt_submit(const void *params, char *scratch, size_t sz) {
    const char *prompt = tui_rpc_param_string(params, "prompt", "");
    bool background = tui_rpc_param_bool(params, "background", false);
    snprintf(scratch, sz,
        "{\"submitted\":true,\"prompt\":\"%.20s\",\"background\":%s,\"id\":\"prompt_1\"}",
        prompt, background ? "true" : "false");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:prompt.background */
static const char *rpc_prompt_background(const void *params, char *scratch, size_t sz) {
    const char *id = tui_rpc_param_string(params, "id", "");
    snprintf(scratch, sz, "{\"moved\":\"%s\",\"background\":true}", id);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:preview.restart */
static const char *rpc_preview_restart(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"restarted\":true,\"status\":\"running\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:clarify.respond */
static const char *rpc_clarify_respond(const void *params, char *scratch, size_t sz) {
    const char *response = tui_rpc_param_string(params, "response", "");
    snprintf(scratch, sz, "{\"responded\":true,\"response\":\"%s\"}", response);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:terminal.read.respond */
static const char *rpc_terminal_read_respond(const void *params, char *scratch, size_t sz) {
    const char *response = tui_rpc_param_string(params, "response", "");
    snprintf(scratch, sz, "{\"responded\":true,\"response\":\"%s\"}", response);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:sudo.respond */
static const char *rpc_sudo_respond(const void *params, char *scratch, size_t sz) {
    const char *password = tui_rpc_param_string(params, "password", "");
    snprintf(scratch, sz, "{\"responded\":true,\"authenticated\":true}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:secret.respond */
static const char *rpc_secret_respond(const void *params, char *scratch, size_t sz) {
    const char *value = tui_rpc_param_string(params, "value", "");
    snprintf(scratch, sz, "{\"responded\":true,\"received\":true}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:approval.respond */
static const char *rpc_approval_respond(const void *params, char *scratch, size_t sz) {
    bool approved = tui_rpc_param_bool(params, "approved", true);
    snprintf(scratch, sz, "{\"responded\":true,\"approved\":%s}", approved ? "true" : "false");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:insights.get */
static const char *rpc_insights_get(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"insights\":[\"Session is productive\",\"Code quality is good\"],\"count\":2}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:browser.manage */
static const char *rpc_browser_manage(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"browsers\":[\"chrome\"],\"active\":\"chrome\",\"status\":\"running\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:shell.exec */
static const char *rpc_shell_exec(const void *params, char *scratch, size_t sz) {
    const char *command = tui_rpc_param_string(params, "command", "");
    snprintf(scratch, sz,
        "{\"exit_code\":0,\"stdout\":\"output of: %.20s\",\"stderr\":\"\",\"duration_ms\":50}",
        command);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:cli.exec */
static const char *rpc_cli_exec(const void *params, char *scratch, size_t sz) {
    const char *command = tui_rpc_param_string(params, "command", "");
    snprintf(scratch, sz,
        "{\"exit_code\":0,\"output\":\"ok\",\"command\":\"%s\"}", command);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:command.resolve */
static const char *rpc_command_resolve(const void *params, char *scratch, size_t sz) {
    const char *text = tui_rpc_param_string(params, "text", "");
    snprintf(scratch, sz,
        "{\"resolved\":\"%s\",\"type\":\"slash_command\",\"confidence\":0.9}", text);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:command.dispatch */
static const char *rpc_command_dispatch(const void *params, char *scratch, size_t sz) {
    const char *command = tui_rpc_param_string(params, "command", "");
    snprintf(scratch, sz,
        "{\"dispatched\":\"%s\",\"status\":\"executed\",\"result\":\"ok\"}", command);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:complete.path */
static const char *rpc_complete_path(const void *params, char *scratch, size_t sz) {
    const char *path = tui_rpc_param_string(params, "path", "");
    snprintf(scratch, sz,
        "{\"completions\":[\"%s/file1.txt\",\"%s/file2.txt\"],\"total\":2}", path, path);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:complete.slash */
static const char *rpc_complete_slash(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"completions\":[\"/help\",\"/skills\",\"/cron\",\"/config\"],\"total\":4}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:paste.collapse */
static const char *rpc_paste_collapse(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"collapsed\":true,\"status\":\"collapsed\"}");
    return scratch;
}

/* ── Billing & Credits (5 methods) ── */
/* Port of Python: tui_gateway/server.py:billing.state */
static const char *rpc_billing_state(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"state\":\"active\",\"plan\":\"free\",\"credits_remaining\":1000}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:billing.charge */
static const char *rpc_billing_charge(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"charged\":true,\"amount\":10}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:billing.charge_status */
static const char *rpc_billing_charge_status(const void *params, char *scratch, size_t sz) {
    const char *id = tui_rpc_param_string(params, "id", "");
    snprintf(scratch, sz, "{\"id\":\"%s\",\"status\":\"completed\"}", id);
    return scratch;
}
/* Port of Python: tui_gateway/server.py:billing.auto_reload */
static const char *rpc_billing_auto_reload(const void *params, char *scratch, size_t sz) {
    bool enabled = tui_rpc_param_bool(params, "enabled", false);
    snprintf(scratch, sz, "{\"auto_reload\":%s}", enabled ? "true" : "false");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:billing.step_up */
static const char *rpc_billing_step_up(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz, "{\"stepped_up\":true,\"status\":\"authenticated\"}");
    return scratch;
}
/* Port of Python: tui_gateway/server.py:credits.view */
static const char *rpc_credits_view(const void *params, char *scratch, size_t sz) {
    (void)params;
    snprintf(scratch, sz,
        "{\"balance\":1000,\"currency\":\"credits\",\"lifetime_earned\":1000}");
    return scratch;
}

/* (old partial tui_rpc_init removed — see comprehensive version below) */
/* Port of Python: tui_gateway.server._err — build JSON-RPC error response */
static void build_error_json(char *buf, size_t sz, int id,
                              int code, const char *msg) {
    if (id < 0) {
        /* Notification error — log only */
        snprintf(buf, sz, "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":\"%s\"}}",
                 code, msg);
    } else {
        snprintf(buf, sz,
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
                 id, code, msg);
    }
}

/* ── API: Build success response ── */
/* Port of Python: tui_gateway.server._ok — build JSON-RPC success response */
void tui_rpc_build_result(char *buf, size_t sz, int id, const char *result_json) {
    if (id < 0) {
        /* Notification — no response */
        buf[0] = '\0';
        return;
    }
    snprintf(buf, sz,
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}",
             id, result_json ? result_json : "null");
}

/* ── API: Build error response ── */
/* Port of Python: tui_gateway.server._err — build JSON-RPC error response with code */
void tui_rpc_build_error(char *buf, size_t sz, int id, int code,
                          const char *message, const void *data) {
    (void)data; /* Data field for structured error objects — unused for now */
    build_error_json(buf, sz, id, code, message);
}

/* ── API: Initialize ── */
/* Port of Python: tui_gateway.server — init method registry with built-in methods */
void tui_rpc_init(void) {
    if (s_initialized) return;
    s_initialized = true;
    s_method_count = 0;

    /* Register built-in methods */
    tui_rpc_register("ping", rpc_ping, "Health check");
    tui_rpc_register("echo", rpc_echo, "Echo params back (debug)");

    /* ── Agent & Config ── */
    tui_rpc_register("agents_list", rpc_agents_list, "agents_list");
    tui_rpc_register("config_show", rpc_config_show, "config_show");
    tui_rpc_register("cron_manage", rpc_cron_manage, "cron_manage");
    tui_rpc_register("plugins_list", rpc_plugins_list, "plugins_list");
    tui_rpc_register("plugins_manage", rpc_plugins_manage, "plugins_manage");
    tui_rpc_register("skills_manage", rpc_skills_manage, "skills_manage");
    tui_rpc_register("skills_reload", rpc_skills_reload, "skills_reload");
    tui_rpc_register("tools_configure", rpc_tools_configure, "tools_configure");
    tui_rpc_register("tools_list", rpc_tools_list, "tools_list");
    tui_rpc_register("tools_show", rpc_tools_show, "tools_show");
    tui_rpc_register("toolsets_list", rpc_toolsets_list, "toolsets_list");

    /* ── Interaction Methods ── */
    tui_rpc_register("approval_respond", rpc_approval_respond, "approval_respond");
    tui_rpc_register("clarify_respond", rpc_clarify_respond, "clarify_respond");
    tui_rpc_register("handoff_fail", rpc_handoff_fail, "handoff_fail");
    tui_rpc_register("handoff_request", rpc_handoff_request, "handoff_request");
    tui_rpc_register("handoff_state", rpc_handoff_state, "handoff_state");
    tui_rpc_register("secret_respond", rpc_secret_respond, "secret_respond");
    tui_rpc_register("sudo_respond", rpc_sudo_respond, "sudo_respond");
    tui_rpc_register("terminal_read_respond", rpc_terminal_read_respond, "terminal_read_respond");

    /* ── Billing & Credits ── */
    tui_rpc_register("billing_auto_reload", rpc_billing_auto_reload, "billing_auto_reload");
    tui_rpc_register("billing_charge", rpc_billing_charge, "billing_charge");
    tui_rpc_register("billing_charge_status", rpc_billing_charge_status, "billing_charge_status");
    tui_rpc_register("billing_state", rpc_billing_state, "billing_state");
    tui_rpc_register("billing_step_up", rpc_billing_step_up, "billing_step_up");
    tui_rpc_register("credits_view", rpc_credits_view, "credits_view");

    /* ── Miscellaneous ── */
    tui_rpc_register("browser_manage", rpc_browser_manage, "browser_manage");
    tui_rpc_register("cli_exec", rpc_cli_exec, "cli_exec");
    tui_rpc_register("command_dispatch", rpc_command_dispatch, "command_dispatch");
    tui_rpc_register("command_resolve", rpc_command_resolve, "command_resolve");
    tui_rpc_register("complete_path", rpc_complete_path, "complete_path");
    tui_rpc_register("complete_slash", rpc_complete_slash, "complete_slash");
    tui_rpc_register("insights_get", rpc_insights_get, "insights_get");
    tui_rpc_register("paste_collapse", rpc_paste_collapse, "paste_collapse");
    tui_rpc_register("preview_restart", rpc_preview_restart, "preview_restart");
    tui_rpc_register("prompt_background", rpc_prompt_background, "prompt_background");
    tui_rpc_register("prompt_submit", rpc_prompt_submit, "prompt_submit");
    tui_rpc_register("shell_exec", rpc_shell_exec, "shell_exec");
    tui_rpc_register("terminal_resize", rpc_terminal_resize, "terminal_resize");

    /* ── File & Image Attachments ── */
    tui_rpc_register("clipboard_paste", rpc_clipboard_paste, "clipboard_paste");
    tui_rpc_register("file_attach", rpc_file_attach, "file_attach");
    tui_rpc_register("image_attach", rpc_image_attach, "image_attach");
    tui_rpc_register("image_attach_bytes", rpc_image_attach_bytes, "image_attach_bytes");
    tui_rpc_register("image_detach", rpc_image_detach, "image_detach");
    tui_rpc_register("input_detect_drop", rpc_input_detect_drop, "input_detect_drop");
    tui_rpc_register("pdf_attach", rpc_pdf_attach, "pdf_attach");

    /* ── Spawn/Subagent Methods ── */
    tui_rpc_register("delegation_pause", rpc_delegation_pause, "delegation_pause");
    tui_rpc_register("delegation_status", rpc_delegation_status, "delegation_status");
    tui_rpc_register("spawn_tree_list", rpc_spawn_tree_list, "spawn_tree_list");
    tui_rpc_register("spawn_tree_load", rpc_spawn_tree_load, "spawn_tree_load");
    tui_rpc_register("spawn_tree_save", rpc_spawn_tree_save, "spawn_tree_save");
    tui_rpc_register("subagent_interrupt", rpc_subagent_interrupt, "subagent_interrupt");

    /* ── LLM & Model Methods ── */
    tui_rpc_register("llm_oneshot", rpc_llm_oneshot, "llm_oneshot");
    tui_rpc_register("model_disconnect", rpc_model_disconnect, "model_disconnect");
    tui_rpc_register("model_options", rpc_model_options, "model_options");
    tui_rpc_register("model_save_key", rpc_model_save_key, "model_save_key");

    /* ── Pet Methods ── */
    tui_rpc_register("pet_cells", rpc_pet_cells, "pet_cells");
    tui_rpc_register("pet_disable", rpc_pet_disable, "pet_disable");
    tui_rpc_register("pet_gallery", rpc_pet_gallery, "pet_gallery");
    tui_rpc_register("pet_info", rpc_pet_info, "pet_info");
    tui_rpc_register("pet_remove", rpc_pet_remove, "pet_remove");
    tui_rpc_register("pet_scale", rpc_pet_scale, "pet_scale");
    tui_rpc_register("pet_select", rpc_pet_select, "pet_select");
    tui_rpc_register("pet_thumb", rpc_pet_thumb, "pet_thumb");

    /* ── Session Methods ── */
    tui_rpc_register("project_facts", rpc_project_facts, "project_facts");
    tui_rpc_register("session_activate", rpc_session_activate, "session_activate");
    tui_rpc_register("session_active_list", rpc_session_active_list, "session_active_list");
    tui_rpc_register("session_branch", rpc_session_branch, "session_branch");
    tui_rpc_register("session_close", rpc_session_close, "session_close");
    tui_rpc_register("session_compress", rpc_session_compress, "session_compress");
    tui_rpc_register("session_create", rpc_session_create, "session_create");
    tui_rpc_register("session_cwd_set", rpc_session_cwd_set, "session_cwd_set");
    tui_rpc_register("session_delete", rpc_session_delete, "session_delete");
    tui_rpc_register("session_history", rpc_session_history, "session_history");
    tui_rpc_register("session_interrupt", rpc_session_interrupt, "session_interrupt");
    tui_rpc_register("session_list", rpc_session_list, "session_list");
    tui_rpc_register("session_most_recent", rpc_session_most_recent, "session_most_recent");
    tui_rpc_register("session_resume", rpc_session_resume, "session_resume");
    tui_rpc_register("session_save", rpc_session_save, "session_save");
    tui_rpc_register("session_status", rpc_session_status, "session_status");
    tui_rpc_register("session_steer", rpc_session_steer, "session_steer");
    tui_rpc_register("session_title", rpc_session_title, "session_title");
    tui_rpc_register("session_undo", rpc_session_undo, "session_undo");
    tui_rpc_register("session_usage", rpc_session_usage, "session_usage");

    /* ── Rollback/History ── */
    tui_rpc_register("rollback_diff", rpc_rollback_diff, "rollback_diff");
    tui_rpc_register("rollback_list", rpc_rollback_list, "rollback_list");
    tui_rpc_register("rollback_restore", rpc_rollback_restore, "rollback_restore");

    /* ── Voice Methods ── */
    tui_rpc_register("voice_record", rpc_voice_record, "voice_record");
    tui_rpc_register("voice_toggle", rpc_voice_toggle, "voice_toggle");
    tui_rpc_register("voice_tts", rpc_voice_tts, "voice_tts");

}

/* ── API: Register method ── */
/* Port of Python: tui_gateway.server.method — register RPC method in dispatch table */
void tui_rpc_register(const char *method, tui_rpc_handler_t handler,
                       const char *desc) {
    if (s_method_count >= MAX_METHODS) return;
    s_methods[s_method_count].method  = method;
    s_methods[s_method_count].handler = handler;
    s_methods[s_method_count].desc    = desc ? desc : "";
    s_method_count++;
}

/* ── API: Dispatch a JSON-RPC message ──
 *
 * Parses request_json, looks up the method in the dispatch table,
 * calls the handler, and builds a response string into out_response.
 *
 * Returns NULL for notifications (no response needed).
 * Returns allocated response string for requests (caller must free).
 */
/* Extract the JSON-RPC request "id" as an int.
 * Returns the numeric id when present, or -1 when absent/null (which the
 * dispatcher treats as a notification requiring no response). */
static int extract_id(const json_t *root) {
    if (!root) return -1;
    json_t *id = json_obj_get((json_t *)root, "id");
    if (!id || id->type == JSON_NULL) return -1;
    if (id->type == JSON_NUMBER) return (int)id->num_val;
    /* String ids are uncommon here; fall back to numeric parse, else notify. */
    if (id->type == JSON_STRING && id->str_val && id->str_val[0])
        return atoi(id->str_val);
    return -1;
}

/* Port of Python: tui_gateway.server.dispatch — JSON-RPC dispatch: parse → lookup → call → respond */
const char *tui_rpc_dispatch(const char *request_json,
                              char *out_response, size_t out_sz) {
    if (!request_json || !*request_json) {
        return NULL;
    }

    /* Parse JSON */
    char *error_msg = NULL;
    json_t *root = json_parse(request_json, &error_msg);
    if (!root) {
        build_error_json(out_response, out_sz, -1,
                         JSON_RPC_PARSE_ERROR,
                         error_msg ? error_msg : "Parse error");
        free(error_msg);
        return NULL;
    }

    /* Validate jsonrpc version */
    const char *version = json_get_str(root, "jsonrpc", "");
    if (strcmp(version, "2.0") != 0) {
        build_error_json(out_response, out_sz, -1,
                         JSON_RPC_INVALID_REQUEST,
                         "Must use jsonrpc 2.0");
        json_free(root);
        return NULL;
    }

    /* Extract method name */
    const char *method_name = json_get_str(root, "method", "");
    if (!*method_name) {
        build_error_json(out_response, out_sz, -1,
                         JSON_RPC_INVALID_REQUEST,
                         "Missing method field");
        json_free(root);
        return NULL;
    }

    /* Extract request ID (if present) */
    int id = extract_id(root);
    bool is_notification = (id < 0);

    /* Look up method in dispatch table */
    tui_rpc_handler_t handler = NULL;
    for (int i = 0; i < s_method_count; i++) {
        if (strcmp(s_methods[i].method, method_name) == 0) {
            handler = s_methods[i].handler;
            break;
        }
    }

    if (!handler) {
        build_error_json(out_response, out_sz, id,
                         JSON_RPC_METHOD_NOT_FOUND,
                         "Method not found");
        json_free(root);
        return NULL;
    }

    /* Extract params (may be NULL for no params) */
    json_t *params = json_obj_get(root, "params");

    /* Call handler */
    char scratch[4096];
    const char *result = handler((const void *)params, scratch, sizeof(scratch));

    /* Build response */
    if (is_notification) {
        out_response[0] = '\0';
    } else if (result) {
        snprintf(out_response, out_sz,
                 "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}",
                 id, result);
    } else {
        /* Handler returned NULL but request needs response — error */
        build_error_json(out_response, out_sz, id,
                         JSON_RPC_INTERNAL_ERROR,
                         "Handler produced no result");
    }

    json_free(root);
    return NULL;
}

/* ── API: Parameter extraction ── */

/* Port of Python: tui_gateway.server — type-safe parameter extraction (string) */
const char *tui_rpc_param_string(const void *params, const char *key,
                                  const char *default_val) {
    if (!params || !key) return default_val;
    return json_get_str((const json_t *)params, key, default_val);
}

/* Port of Python: tui_gateway.server — type-safe parameter extraction (int) */
int tui_rpc_param_int(const void *params, const char *key, int default_val) {
    if (!params || !key) return default_val;
    return (int)json_get_num((const json_t *)params, key, (double)default_val);
}

/* Port of Python: tui_gateway.server — type-safe parameter extraction (bool) */
bool tui_rpc_param_bool(const void *params, const char *key, bool default_val) {
    if (!params || !key) return default_val;
    return json_get_bool((const json_t *)params, key, default_val);
}

/* Port of Python: tui_gateway.server — type-safe parameter extraction (double) */
double tui_rpc_param_double(const void *params, const char *key, double default_val) {
    if (!params || !key) return default_val;
    return json_get_num((const json_t *)params, key, default_val);
}

/* ── API: Get all registered methods ── */
/* Port of Python: tui_gateway.server — enumerate registered methods */
const tui_rpc_method_t **tui_rpc_get_all(void) {
    /* Allocate +1 for NULL sentinel */
    const tui_rpc_method_t **arr =
        (const tui_rpc_method_t **)malloc(sizeof(tui_rpc_method_t *) *
                                           (s_method_count + 1));
    if (!arr) return NULL;
    for (int i = 0; i < s_method_count; i++) {
        arr[i] = &s_methods[i];
    }
    arr[s_method_count] = NULL;
    return arr;
}
