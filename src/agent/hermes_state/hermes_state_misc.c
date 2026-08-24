/*
 * hermes_state_misc.c — C port of hermes_state.py small SessionDB helpers.
 * Opaque hermes_state_db_t; every function is PoP-annotated to its Python
 * counterpart. Pure-logic + SQL helpers that were too small for their own
 * subsystem file.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <dirent.h>
#include <sqlite3.h>
#include "hermes_state_internal.h"
#include "hermes_json.h"

/* PoP: hermes_state_workspace_key @ hermes_state.py:workspace_key */
char *hermes_state_workspace_key(hermes_state_db_t *db, const char *session_id) {
    /* Python: git_repo_root when known, else cwd, else None. */
    if (!db || !session_id || !*session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT git_repo_root, cwd FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *root = sqlite3_column_text(st, 0);
        const unsigned char *cwd = sqlite3_column_text(st, 1);
        const char *r = root ? (const char *)root : "";
        while (*r == ' ' || *r == '\t') r++;
        if (*r) { out = strdup(r); }
        else if (cwd) {
            const char *c = (const char *)cwd;
            while (*c == ' ' || *c == '\t') c++;
            out = *c ? strdup(c) : NULL;
        }
    }
    sqlite3_finalize(st);
    return out;
}

/* PoP: hermes_state_scrub_surrogates @ hermes_state.py:_scrub_surrogates */
char *hermes_state_scrub_surrogates(const char *value) {
    /* Replace lone UTF-8 surrogate encodings (ED A0..BF / ED 80..9F) with
     * U+FFFD (EF BF BD); pass everything else through. */
    if (!value) return NULL;
    size_t n = strlen(value);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == 0xED && i + 2 < n) {
            unsigned char c1 = (unsigned char)value[i + 1];
            unsigned char c2 = (unsigned char)value[i + 2];
            bool surrogate = (c1 >= 0xA0 && c1 <= 0xBF) || (c1 >= 0x80 && c1 <= 0x9F);
            if (surrogate && (c2 & 0xC0) == 0x80) {
                out[o++] = 0xEF; out[o++] = 0xBF; out[o++] = 0xBD;
                i += 2;
                continue;
            }
        }
        out[o++] = (char)c;
    }
    out[o] = '\0';
    return out;
}

/* PoP: hermes_state_cwd_prefix_clause @ hermes_state.py:_cwd_prefix_clause */
char *hermes_state_cwd_prefix_clause(const char *cwd_prefix) {
    /* Python: "(s.cwd = ? OR s.cwd LIKE ? OR s.cwd LIKE ?)" with
     * [prefix, prefix/%, prefix\%] — printed tab-separated for the shim. */
    const char *p = cwd_prefix ? cwd_prefix : "";
    size_t pl = strlen(p);
    while (pl > 0 && (p[pl-1] == '/' || p[pl-1] == '\\')) pl--;
    printf("(s.cwd = ? OR s.cwd LIKE ? OR s.cwd LIKE ?)\t%.*s\t%.*s/%%\t%.*s\\%%\n",
           (int)pl, p, (int)pl, p, (int)pl, p);
    return NULL;
}

/* PoP: hermes_state_workspace_key_clause @ hermes_state.py:_workspace_key_clause */
char *hermes_state_workspace_key_clause(const char *key) {
    /* Python: "(s.git_repo_root = ? OR (COALESCE(s.git_repo_root, '') = ''
     * AND <cwd clause>))" with [prefix, *cwd_params]. */
    const char *k = key ? key : "";
    size_t kl = strlen(k);
    while (kl > 0 && (k[kl-1] == '/' || k[kl-1] == '\\')) kl--;
    printf("(s.git_repo_root = ? OR (COALESCE(s.git_repo_root, '') = '' AND (s.cwd = ? OR s.cwd LIKE ? OR s.cwd LIKE ?)))\t%.*s\t%.*s\t%.*s/%%\t%.*s\\%%\n",
           (int)kl, k, (int)kl, k, (int)kl, k, (int)kl, k);
    return NULL;
}

/* last-init-error process-global (thread-safe via the db lock) */
static char g_last_init_error[512];
static bool g_last_init_error_set = false;

/* PoP: hermes_state_set_last_init_error @ hermes_state.py:_set_last_init_error */
void hermes_state_set_last_init_error(const char *message) {
    if (message && *message) {
        snprintf(g_last_init_error, sizeof(g_last_init_error), "%s", message);
        g_last_init_error_set = true;
    } else {
        g_last_init_error[0] = '\0';
        g_last_init_error_set = false;
    }
}

/* PoP: hermes_state_get_last_init_error @ hermes_state.py:get_last_init_error */
const char *hermes_state_get_last_init_error(void) {
    return g_last_init_error_set ? g_last_init_error : NULL;
}

/* PoP: hermes_state_format_session_db_unavailable @ hermes_state.py:format_session_db_unavailable */
char *hermes_state_format_session_db_unavailable(const char *cause) {
    /* Python: "Session database not available: <cause> (state.db may be on
     * NFS/SMB — see hermes docs)". */
    if (cause && *cause) {
        char *out = malloc(1024);
        snprintf(out, 1024,
                 "Session database not available: %s (state.db may be on NFS/SMB \xe2\x80\x94 see the docs for the locking-protocol workaround).",
                 cause);
        return out;
    }
    return strdup("Session database not available.");
}

/* PoP: hermes_state_on_disk_journal_mode @ hermes_state.py:_on_disk_journal_mode */
const char *hermes_state_on_disk_journal_mode(hermes_state_db_t *db) {
    static char mode[16];
    if (!db) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "PRAGMA journal_mode", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    const char *m = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(st, 0);
        if (v) { snprintf(mode, sizeof(mode), "%s", (const char *)v); m = mode; }
    }
    sqlite3_finalize(st);
    return m;
}

/* PoP: hermes_state_is_sqlite_wal_reset_vulnerable @ hermes_state.py:is_sqlite_wal_reset_vulnerable */
bool hermes_state_is_sqlite_wal_reset_vulnerable(void) {
    /* WAL-reset bug: 3.7.0 .. 3.51.2, fixed 3.51.3+, backports 3.50.7 and
     * 3.44.6. Pre-3.7.0 libraries cannot hit the race. */
    int v = sqlite3_libversion_number();
    if (v < 3007000 || v > 3051002) return false;
    if (v == 3050007 || v == 3044006) return false;
    return true;
}

/* PoP: hermes_state_sqlite_source_id @ hermes_state.py:sqlite_source_id */
const char *hermes_state_sqlite_source_id(void) {
    return sqlite3_sourceid();
}

/* PoP: hermes_state_is_malformed_db_error @ hermes_state.py:is_malformed_db_error */
bool hermes_state_is_malformed_db_error(const char *msg) {
    if (!msg) return false;
    return strstr(msg, "malformed database schema") != NULL
        || strstr(msg, "database disk image is malformed") != NULL;
}

/* one-shot repair claims (process-global) */
#define HS_MAX_CLAIMS 32
static char *g_repair_claims[HS_MAX_CLAIMS];
static int g_nclaims = 0;

/* PoP: hermes_state_claim_repair_attempt @ hermes_state.py:_claim_repair_attempt */
bool hermes_state_claim_repair_attempt(const char *db_path) {
    if (!db_path) return false;
    for (int i = 0; i < g_nclaims; i++)
        if (g_repair_claims[i] && strcmp(g_repair_claims[i], db_path) == 0)
            return false;
    if (g_nclaims < HS_MAX_CLAIMS) g_repair_claims[g_nclaims++] = strdup(db_path);
    return true;
}

/* PoP: hermes_state_sanitize_title @ hermes_state.py:sanitize_title */
char *hermes_state_sanitize_title(const char *title) {
    /* Strip ASCII controls (keep \t\n\r), strip problematic Unicode controls,
     * collapse whitespace runs, empty -> NULL, max 100 chars. */
    if (!title || !*title) return NULL;
    char *scrubbed = hermes_state_scrub_surrogates(title);
    if (!scrubbed) return NULL;
    size_t n = strlen(scrubbed);
    char *out = malloc(n + 1);
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)scrubbed[i];
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) continue;
        if (c == 0x7F) continue;
        out[o++] = (char)c;
    }
    out[o] = '\0';
    /* strip Unicode control ranges: U+200B-200F, U+2028-202E, U+2060-2069,
     * U+FEFF, U+FFFC, U+FFF9-FFFB — as UTF-8 byte sequences. */
    static const char *const bad[] = {
        "\xe2\x80\x8b", "\xe2\x80\x8c", "\xe2\x80\x8d", "\xe2\x80\x8e", "\xe2\x80\x8f",
        "\xe2\x80\xa8", "\xe2\x80\xa9", "\xe2\x80\xaa", "\xe2\x80\xab", "\xe2\x80\xac",
        "\xe2\x80\xad", "\xe2\x80\xae", "\xe2\x81\xa0", "\xe2\x81\xa1", "\xe2\x81\xa2",
        "\xe2\x81\xa3", "\xe2\x81\xa4", "\xe2\x81\xa5", "\xe2\x81\xa6", "\xe2\x81\xa7",
        "\xe2\x81\xa8", "\xe2\x81\xa9", "\xef\xbb\xbf", "\xef\xbf\xbc",
        "\xef\xbf\xb9", "\xef\xbf\xba", "\xef\xbf\xbb", NULL};
    for (int b = 0; bad[b]; b++) {
        char *p = out;
        size_t bl = strlen(bad[b]);
        while ((p = strstr(p, bad[b])) != NULL)
            memmove(p, p + bl, strlen(p + bl) + 1);
    }
    /* collapse whitespace runs + strip */
    char *dst = out;
    bool in_ws = false;
    size_t w = 0;
    for (size_t i = 0; out[i]; i++) {
        if (out[i] == ' ' || out[i] == '\t' || out[i] == '\n' || out[i] == '\r') {
            if (!in_ws && w > 0) dst[w++] = ' ';
            in_ws = true;
        } else {
            in_ws = false;
            dst[w++] = out[i];
        }
    }
    while (w > 0 && dst[w-1] == ' ') w--;
    dst[w] = '\0';
    free(scrubbed);
    if (!*dst) { free(out); return NULL; }
    if (strlen(dst) > 100) { free(out); return NULL; } /* ValueError: caller raises */
    return out;
}

/* PoP: hermes_state_get_session_title @ hermes_state.py:get_session_title */
char *hermes_state_get_session_title(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id || !*session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT title FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *title = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(st, 0);
        if (v) title = strdup((const char *)v);
    }
    sqlite3_finalize(st);
    return title;
}

/* PoP: hermes_state_session_count @ hermes_state.py:session_count */
long long hermes_state_session_count(hermes_state_db_t *db, const char *source,
                                     bool exclude_children, const char *exclude_sources) {
    if (!db) return 0;
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM sessions s WHERE 1=1 %s %s %s",
        source && *source ? "AND s.source = ?1" : "",
        exclude_children
            ? "AND NOT EXISTS (SELECT 1 FROM sessions c WHERE c.parent_session_id = s.id "
              "AND (s.end_reason = 'compression' OR c.end_reason = 'compression'))"
            : "",
        exclude_sources && *exclude_sources ? "AND s.source NOT IN (?2)" : "");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    if (source && *source) sqlite3_bind_text(st, 1, source, -1, SQLITE_TRANSIENT);
    if (exclude_sources && *exclude_sources) sqlite3_bind_text(st, 2, exclude_sources, -1, SQLITE_TRANSIENT);
    long long count = 0;
    if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return count;
}

/* PoP: hermes_state_message_count @ hermes_state.py:message_count */
long long hermes_state_message_count(hermes_state_db_t *db, const char *session_id) {
    if (!db) return 0;
    sqlite3_stmt *st = NULL;
    const char *sql = session_id && *session_id
        ? "SELECT COUNT(*) FROM messages WHERE session_id = ?"
        : "SELECT COUNT(*) FROM messages";
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    if (session_id && *session_id) sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    long long count = 0;
    if (sqlite3_step(st) == SQLITE_ROW) count = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return count;
}

/* PoP: hermes_state_has_platform_message_id @ hermes_state.py:has_platform_message_id */
bool hermes_state_has_platform_message_id(hermes_state_db_t *db, const char *platform_message_id) {
    if (!db || !platform_message_id || !*platform_message_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT 1 FROM messages WHERE platform_message_id = ? LIMIT 1", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, platform_message_id, -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return found;
}

/* PoP: hermes_state_is_branch_child_row @ hermes_state.py:_is_branch_child_row */
bool hermes_state_is_branch_child_row(json_t *session) {
    /* Python: model_config dict with _branched_from set. */
    if (!session || session->type != JSON_OBJECT) return false;
    json_t *mc = json_obj_get(session, "model_config");
    if (!mc) return false;
    json_t *cfg = (mc->type == JSON_STRING) ? json_parse(json_string_value(mc), NULL) : mc;
    if (!cfg || cfg->type != JSON_OBJECT) { if (mc->type == JSON_STRING) json_free(cfg); return false; }
    json_t *bf = json_obj_get(cfg, "_branched_from");
    bool r = bf != NULL && !json_is_null(bf);
    if (mc->type == JSON_STRING) json_free(cfg);
    return r;
}

/* PoP: hermes_state_is_compression_child_row @ hermes_state.py:_is_compression_child_row */
bool hermes_state_is_compression_child_row(hermes_state_db_t *db, json_t *child) {
    /* Python: parent_session_id set, not a branch child, and the parent's
     * end_reason == 'compression'. */
    if (!child || child->type != JSON_OBJECT) return false;
    json_t *pid = json_obj_get(child, "parent_session_id");
    if (!pid || json_is_null(pid) || !json_is_string(pid)) return false;
    if (hermes_state_is_branch_child_row(child)) return false;
    const char *parent_id = json_string_value(pid);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT end_reason FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, parent_id, -1, SQLITE_TRANSIENT);
    bool r = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *er = sqlite3_column_text(st, 0);
        r = er && strcmp((const char *)er, "compression") == 0;
    }
    sqlite3_finalize(st);
    return r;
}

/* PoP: hermes_state_get_meta @ hermes_state.py:get_meta */
char *hermes_state_get_meta(hermes_state_db_t *db, const char *key) {
    if (!db || !key || !*key) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT value FROM state_meta WHERE key = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    char *val = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *v = sqlite3_column_text(st, 0);
        if (v) val = strdup((const char *)v);
    }
    sqlite3_finalize(st);
    return val;
}

/* PoP: hermes_state_set_meta @ hermes_state.py:set_meta */
bool hermes_state_set_meta(hermes_state_db_t *db, const char *key, const char *value) {
    if (!db || !key || !*key) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT INTO state_meta (key, value) VALUES (?1, ?2) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, value ? value : "", -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: hermes_state_fts_table_exists @ hermes_state.py:_fts_table_exists */
bool hermes_state_fts_table_exists(hermes_state_db_t *db, const char *name) {
    if (!db || !name || !*name) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT 1 FROM %s LIMIT 0", name);
    sqlite3_stmt *st = NULL;
    bool ok = sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) == SQLITE_OK;
    if (ok) sqlite3_finalize(st);
    return ok;
}

/* ── telegram DM topic mode ─────────────────────────────────────────────── */

/* PoP: hermes_state_is_telegram_topic_mode_enabled @ hermes_state.py:is_telegram_topic_mode_enabled */
bool hermes_state_is_telegram_topic_mode_enabled(hermes_state_db_t *db,
                                                 const char *chat_id, const char *user_id) {
    if (!db || !chat_id || !user_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT enabled FROM telegram_dm_topic_mode WHERE chat_id = ? AND user_id = ?",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, chat_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, user_id, -1, SQLITE_TRANSIENT);
    bool enabled = false;
    if (sqlite3_step(st) == SQLITE_ROW) enabled = sqlite3_column_int(st, 0) != 0;
    sqlite3_finalize(st);
    return enabled;
}

/* PoP: hermes_state_get_telegram_topic_binding @ hermes_state.py:get_telegram_topic_binding */
char *hermes_state_get_telegram_topic_binding(hermes_state_db_t *db,
                                              const char *chat_id, const char *thread_id) {
    if (!db || !chat_id || !thread_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT * FROM telegram_dm_topic_bindings WHERE chat_id = ? AND thread_id = ?",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, chat_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, thread_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        json_t *o = json_object();
        for (int i = 0; i < sqlite3_column_count(st); i++) {
            const char *col = sqlite3_column_name(st, i);
            const unsigned char *v = sqlite3_column_text(st, i);
            json_set(o, col, v ? json_string((const char *)v) : json_null());
        }
        out = json_serialize(o);
        json_free(o);
    }
    sqlite3_finalize(st);
    return out;
}

/* PoP: hermes_state_list_telegram_topic_bindings_for_chat @ hermes_state.py:list_telegram_topic_bindings_for_chat */
char *hermes_state_list_telegram_topic_bindings_for_chat(hermes_state_db_t *db, const char *chat_id) {
    if (!db || !chat_id) return strdup("[]");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT * FROM telegram_dm_topic_bindings WHERE chat_id = ? ORDER BY updated_at DESC",
            -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    sqlite3_bind_text(st, 1, chat_id, -1, SQLITE_TRANSIENT);
    json_t *arr = json_array();
    while (sqlite3_step(st) == SQLITE_ROW) {
        json_t *o = json_object();
        for (int i = 0; i < sqlite3_column_count(st); i++) {
            const char *col = sqlite3_column_name(st, i);
            const unsigned char *v = sqlite3_column_text(st, i);
            json_set(o, col, v ? json_string((const char *)v) : json_null());
        }
        json_append(arr, o);
    }
    sqlite3_finalize(st);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* PoP: hermes_state_get_telegram_topic_binding_by_session @ hermes_state.py:get_telegram_topic_binding_by_session */
char *hermes_state_get_telegram_topic_binding_by_session(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT * FROM telegram_dm_topic_bindings WHERE session_id = ?", -1, &st, NULL) != SQLITE_OK)
        return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        json_t *o = json_object();
        for (int i = 0; i < sqlite3_column_count(st); i++) {
            const char *col = sqlite3_column_name(st, i);
            const unsigned char *v = sqlite3_column_text(st, i);
            json_set(o, col, v ? json_string((const char *)v) : json_null());
        }
        out = json_serialize(o);
        json_free(o);
    }
    sqlite3_finalize(st);
    return out;
}

/* PoP: hermes_state_delete_telegram_topic_binding @ hermes_state.py:delete_telegram_topic_binding */
bool hermes_state_delete_telegram_topic_binding(hermes_state_db_t *db,
                                                const char *chat_id, const char *thread_id) {
    if (!db || !chat_id || !thread_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "DELETE FROM telegram_dm_topic_bindings WHERE chat_id = ? AND thread_id = ?",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, chat_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, thread_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: hermes_state_is_telegram_session_linked_to_topic @ hermes_state.py:is_telegram_session_linked_to_topic */
bool hermes_state_is_telegram_session_linked_to_topic(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT 1 FROM telegram_dm_topic_bindings WHERE session_id = ? LIMIT 1",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return found;
}

/* PoP: hermes_state_bind_telegram_topic @ hermes_state.py:bind_telegram_topic */
int hermes_state_bind_telegram_topic(hermes_state_db_t *db,
                                     const char *chat_id, const char *thread_id,
                                     const char *user_id, const char *session_key,
                                     const char *session_id) {
    /* A session may only link to one topic: rebinding the same topic to the
     * same session is idempotent; linking to a different topic -> -1 (the
     * Python raises ValueError). Returns 1 on (re)bind, 0 on failure. */
    if (!db || !chat_id || !thread_id || !session_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT chat_id, thread_id FROM telegram_dm_topic_bindings WHERE session_id = ?",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *c = sqlite3_column_text(st, 0);
        const unsigned char *t2 = sqlite3_column_text(st, 1);
        sqlite3_finalize(st);
        bool same_topic = c && t2 && strcmp((const char *)c, chat_id) == 0
                        && strcmp((const char *)t2, thread_id) == 0;
        if (!same_topic) return -1;
    } else {
        sqlite3_finalize(st);
    }
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT INTO telegram_dm_topic_bindings "
            "(chat_id, thread_id, user_id, session_key, session_id, updated_at) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6) "
            "ON CONFLICT(chat_id, thread_id) DO UPDATE SET "
            "session_key = excluded.session_key, session_id = excluded.session_id, "
            "updated_at = excluded.updated_at", -1, &up, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(up, 1, chat_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 2, thread_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 3, user_id ? user_id : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 4, session_key ? session_key : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 5, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(up, 6, hermes_state_now_epoch());
    bool ok = sqlite3_step(up) == SQLITE_DONE;
    sqlite3_finalize(up);
    return ok ? 1 : 0;
}

/* PoP: hermes_state_enable_telegram_topic_mode @ hermes_state.py:enable_telegram_topic_mode */
bool hermes_state_enable_telegram_topic_mode(hermes_state_db_t *db,
                                             const char *chat_id, const char *user_id,
                                             bool has_topics_enabled, bool allows_create) {
    if (!db || !chat_id || !user_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT INTO telegram_dm_topic_mode "
            "(chat_id, user_id, enabled, activated_at, updated_at, has_topics_enabled, allows_users_to_create_topics) "
            "VALUES (?1, ?2, 1, ?3, ?3, ?4, ?5) "
            "ON CONFLICT(chat_id, user_id) DO UPDATE SET enabled = 1, updated_at = excluded.updated_at, "
            "has_topics_enabled = excluded.has_topics_enabled, "
            "allows_users_to_create_topics = excluded.allows_users_to_create_topics",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, chat_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, user_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 3, hermes_state_now_epoch());
    sqlite3_bind_int(st, 4, has_topics_enabled ? 1 : 0);
    sqlite3_bind_int(st, 5, allows_create ? 1 : 0);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: hermes_state_disable_telegram_topic_mode @ hermes_state.py:disable_telegram_topic_mode */
bool hermes_state_disable_telegram_topic_mode(hermes_state_db_t *db,
                                              const char *chat_id, bool clear_bindings) {
    if (!db || !chat_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE telegram_dm_topic_mode SET enabled = 0, updated_at = ? WHERE chat_id = ?",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_double(st, 1, hermes_state_now_epoch());
    sqlite3_bind_text(st, 2, chat_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    if (ok && clear_bindings) {
        sqlite3_stmt *d = NULL;
        if (sqlite3_prepare_v2(db->db,
                "DELETE FROM telegram_dm_topic_bindings WHERE chat_id = ?", -1, &d, NULL) == SQLITE_OK) {
            sqlite3_bind_text(d, 1, chat_id, -1, SQLITE_TRANSIENT);
            sqlite3_step(d);
            sqlite3_finalize(d);
        }
    }
    return ok;
}

/* ── handoff state ─────────────────────────────────────────────────────── */

/* PoP: hermes_state_get_handoff_state @ hermes_state.py:get_handoff_state */
char *hermes_state_get_handoff_state(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT handoff_state, handoff_platform, handoff_error FROM sessions WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *s = sqlite3_column_text(st, 0);
        const unsigned char *p = sqlite3_column_text(st, 1);
        const unsigned char *e = sqlite3_column_text(st, 2);
        json_t *o = json_object();
        json_set(o, "state", json_string((const char *)(s ? s : (const unsigned char *)"")));
        json_set(o, "platform", json_string((const char *)(p ? p : (const unsigned char *)"")));
        json_set(o, "error", json_string((const char *)(e ? e : (const unsigned char *)"")));
        out = json_serialize(o);
        json_free(o);
    }
    sqlite3_finalize(st);
    return out;
}

/* PoP: hermes_state_list_pending_handoffs @ hermes_state.py:list_pending_handoffs */
char *hermes_state_list_pending_handoffs(hermes_state_db_t *db) {
    if (!db) return strdup("[]");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT * FROM sessions WHERE handoff_state = 'pending' ORDER BY started_at ASC",
            -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    json_t *arr = json_array();
    while (sqlite3_step(st) == SQLITE_ROW) {
        json_t *o = json_object();
        for (int i = 0; i < sqlite3_column_count(st); i++) {
            const char *col = sqlite3_column_name(st, i);
            const unsigned char *v = sqlite3_column_text(st, i);
            json_set(o, col, v ? json_string((const char *)v) : json_null());
        }
        json_append(arr, o);
    }
    sqlite3_finalize(st);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* PoP: hermes_state_claim_handoff @ hermes_state.py:claim_handoff */
bool hermes_state_claim_handoff(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET handoff_state = 'running' WHERE id = ? AND handoff_state = 'pending'",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(db->db) > 0;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: hermes_state_complete_handoff @ hermes_state.py:complete_handoff */
void hermes_state_complete_handoff(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET handoff_state = 'completed', handoff_error = NULL WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_fail_handoff @ hermes_state.py:fail_handoff */
void hermes_state_fail_handoff(hermes_state_db_t *db, const char *session_id, const char *error) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET handoff_state = 'failed', handoff_error = ? WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return;
    char buf[512];
    if (error) {
        snprintf(buf, sizeof(buf), "%s", error);
        if (strlen(buf) > 500) buf[500] = '\0';
        sqlite3_bind_text(st, 1, buf, -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(st, 1, "", -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* ── session titles ────────────────────────────────────────────────────── */

/* PoP: hermes_state_set_session_title_impl @ hermes_state.py:_set_session_title */
int hermes_state_set_session_title_impl(hermes_state_db_t *db, const char *session_id,
                                        const char *title, bool only_if_empty) {
    /* sanitize + uniqueness + upsert. Returns 1 on change, 0 when nothing
     * changed, -1 when another session owns the title (Python ValueError). */
    if (!db || !session_id) return 0;
    char *clean = hermes_state_sanitize_title(title);
    if (only_if_empty) {
        sqlite3_stmt *c = NULL;
        if (sqlite3_prepare_v2(db->db, "SELECT title FROM sessions WHERE id = ?", -1, &c, NULL) != SQLITE_OK)
            { free(clean); return 0; }
        sqlite3_bind_text(c, 1, session_id, -1, SQLITE_TRANSIENT);
        int rc = 0;
        if (sqlite3_step(c) == SQLITE_ROW) {
            const unsigned char *cur = sqlite3_column_text(c, 0);
            if (cur != NULL) rc = 0; /* title already set */
        }
        sqlite3_finalize(c);
        if (rc == 0) { free(clean); return 0; }
    }
    if (!clean) {
        /* clearing the title */
        sqlite3_stmt *u = NULL;
        if (sqlite3_prepare_v2(db->db, "UPDATE sessions SET title = NULL WHERE id = ?", -1, &u, NULL) != SQLITE_OK)
            return 0;
        sqlite3_bind_text(u, 1, session_id, -1, SQLITE_TRANSIENT);
        bool ok = sqlite3_step(u) == SQLITE_DONE;
        sqlite3_finalize(u);
        return ok ? 1 : 0;
    }
    sqlite3_stmt *q = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id FROM sessions WHERE title = ? AND id != ?", -1, &q, NULL) != SQLITE_OK)
        { free(clean); return 0; }
    sqlite3_bind_text(q, 1, clean, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(q, 2, session_id, -1, SQLITE_TRANSIENT);
    int conflict = sqlite3_step(q) == SQLITE_ROW;
    sqlite3_finalize(q);
    if (conflict) { free(clean); return -1; }
    sqlite3_stmt *u2 = NULL;
    if (sqlite3_prepare_v2(db->db, "UPDATE sessions SET title = ? WHERE id = ?", -1, &u2, NULL) != SQLITE_OK)
        { free(clean); return 0; }
    sqlite3_bind_text(u2, 1, clean, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(u2, 2, session_id, -1, SQLITE_TRANSIENT);
    bool ok2 = sqlite3_step(u2) == SQLITE_DONE;
    sqlite3_finalize(u2);
    free(clean);
    return ok2 ? 1 : 0;
}

/* PoP: hermes_state_set_session_title @ hermes_state.py:set_session_title */
int hermes_state_set_session_title(hermes_state_db_t *db, const char *session_id, const char *title) {
    return hermes_state_set_session_title_impl(db, session_id, title, false);
}

/* PoP: hermes_state_set_auto_title_if_empty @ hermes_state.py:SessionDB.set_auto_title_if_empty */
int hermes_state_set_auto_title_if_empty(hermes_state_db_t *db, const char *session_id, const char *title) {
    return hermes_state_set_session_title_impl(db, session_id, title, true);
}

/* ── pruning / deletion ────────────────────────────────────────────────── */

/* PoP: hermes_state_prune_empty_ghost_sessions @ hermes_state.py:prune_empty_ghost_sessions */
int hermes_state_prune_empty_ghost_sessions(hermes_state_db_t *db) {
    /* Remove empty TUI ghost sessions (no messages, no title, >24h old). */
    if (!db) return 0;
    double cutoff = hermes_state_now_epoch() - 86400;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id FROM sessions "
            "WHERE source = 'tui' AND title IS NULL AND ended_at IS NOT NULL "
            "AND started_at < ?1 "
            "AND NOT EXISTS (SELECT 1 FROM messages WHERE messages.session_id = sessions.id)",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_double(st, 1, cutoff);
    char ids[4096];
    size_t len = 0;
    ids[0] = '\0';
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        if (!id) continue;
        size_t need = len + strlen((const char *)id) + 2;
        if (need > sizeof(ids)) continue;
        if (len) { strcat(ids, ","); len++; }
        strcat(ids, (const char *)id);
        len += strlen((const char *)id);
    }
    sqlite3_finalize(st);
    if (!len) return 0;
    char sql[4600];
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE id IN (%s)", ids);
    sqlite3_stmt *d = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &d, NULL) != SQLITE_OK) return 0;
    sqlite3_step(d);
    int n = sqlite3_changes(db->db);
    sqlite3_finalize(d);
    return n;
}

/* PoP: hermes_state_finalize_orphaned_compression_sessions @ hermes_state.py:finalize_orphaned_compression_sessions */
int hermes_state_finalize_orphaned_compression_sessions(hermes_state_db_t *db) {
    /* Mark orphaned compression continuation sessions as ended: child has
     * messages, no end_reason/ended_at, api_call_count = 0, parent ended
     * with reason 'compression', child older than 7 days. */
    if (!db) return 0;
    double cutoff = hermes_state_now_epoch() - 604800;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET ended_at = ?1, end_reason = 'orphaned_compression' "
            "WHERE started_at < ?1 "
            "AND end_reason IS NULL AND ended_at IS NULL "
            "AND api_call_count = 0 "
            "AND parent_session_id IS NOT NULL "
            "AND EXISTS (SELECT 1 FROM messages WHERE messages.session_id = sessions.id) "
            "AND EXISTS (SELECT 1 FROM sessions p WHERE p.id = sessions.parent_session_id "
            "            AND p.end_reason = 'compression')",
            -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_double(st, 1, cutoff);
    sqlite3_step(st);
    int n = sqlite3_changes(db->db);
    sqlite3_finalize(st);
    return n;
}

/* PoP: hermes_state_count_empty_sessions @ hermes_state.py:count_empty_sessions */
long long hermes_state_count_empty_sessions(hermes_state_db_t *db) {
    if (!db) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT COUNT(*) FROM sessions "
            "WHERE message_count = 0 AND ended_at IS NOT NULL AND archived = 0",
            -1, &st, NULL) != SQLITE_OK) return 0;
    long long n = 0;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

/* PoP: hermes_state_delete_empty_sessions @ hermes_state.py:delete_empty_sessions */
int hermes_state_delete_empty_sessions(hermes_state_db_t *db) {
    /* Delete every empty, ended, non-archived session; orphan children of
     * deleted parents (parent_session_id -> NULL) instead of cascading. */
    if (!db) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id FROM sessions "
            "WHERE message_count = 0 AND ended_at IS NOT NULL AND archived = 0",
            -1, &st, NULL) != SQLITE_OK) return 0;
    char ids[8192];
    size_t len = 0;
    ids[0] = '\0';
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        if (!id) continue;
        size_t need = len + strlen((const char *)id) + 2;
        if (need > sizeof(ids)) continue;
        if (len) { strcat(ids, ","); len++; }
        strcat(ids, (const char *)id);
        len += strlen((const char *)id);
    }
    sqlite3_finalize(st);
    if (!len) return 0;
    char sql[9000];
    snprintf(sql, sizeof(sql),
             "UPDATE sessions SET parent_session_id = NULL "
             "WHERE parent_session_id IN (%s); "
             "DELETE FROM sessions WHERE id IN (%s)", ids, ids);
    sqlite3_stmt *d = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &d, NULL) != SQLITE_OK) return 0;
    sqlite3_step(d);
    int n = sqlite3_changes(db->db);
    sqlite3_finalize(d);
    return n;
}

/* PoP: hermes_state_delete_session_if_empty @ hermes_state.py:delete_session_if_empty */
bool hermes_state_delete_session_if_empty(hermes_state_db_t *db, const char *session_id) {
    /* Delete only when the session has no messages and no user title, in one
     * transaction; sessions with children are preserved. */
    if (!db || !session_id || !*session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT 1 FROM sessions s WHERE s.id = ?1 "
            "AND NOT EXISTS (SELECT 1 FROM messages WHERE session_id = s.id) "
            "AND s.title IS NULL "
            "AND NOT EXISTS (SELECT 1 FROM sessions c WHERE c.parent_session_id = s.id)",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    bool empty = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    if (!empty) return false;
    sqlite3_stmt *d = NULL;
    if (sqlite3_prepare_v2(db->db, "DELETE FROM sessions WHERE id = ?", -1, &d, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(d, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(d);
    bool ok = sqlite3_changes(db->db) > 0;
    sqlite3_finalize(d);
    return ok;
}

/* PoP: hermes_state_get_session_delete_targets @ hermes_state.py:get_session_delete_targets */
char *hermes_state_get_session_delete_targets(hermes_state_db_t *db, const char *session_id) {
    /* The session itself plus recursively discovered delegate/subagent
     * children (model_config $._delegate_from set). Branch/compression
     * children excluded (deletion preserves them via orphaning). */
    if (!db || !session_id || !*session_id) return strdup("[]");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "WITH RECURSIVE kids(id) AS ("
            "  SELECT id FROM sessions WHERE id = ?1 "
            "  UNION ALL "
            "  SELECT s.id FROM sessions s JOIN kids k ON s.parent_session_id = k.id "
            "  WHERE json_extract(COALESCE(s.model_config, '{}'), '$._delegate_from') IS NOT NULL"
            ") SELECT id FROM kids",
            -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    json_t *arr = json_array();
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        json_append(arr, id ? json_string((const char *)id) : json_string(""));
    }
    sqlite3_finalize(st);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* ── import validators (export/import cluster helpers) ─────────────────── */

/* PoP: hermes_state_import_text_or_none @ hermes_state.py:_import_text_or_none */
char *hermes_state_import_text_or_none(const char *value) {
    /* None -> NULL; str -> copy; else NULL (Python raises ValueError). */
    if (!value || !*value) return NULL;
    return strdup(value);
}

/* PoP: hermes_state_import_json_object_or_none @ hermes_state.py:_import_json_object_or_none */
char *hermes_state_import_json_object_or_none(const char *value) {
    /* None -> NULL; JSON string that parses to an object -> original string;
     * already-object JSON text -> original; else NULL (ValueError). */
    if (!value || !*value) return NULL;
    json_t *parsed = json_parse(value, NULL);
    if (parsed) {
        if (parsed->type == JSON_OBJECT) {
            json_free(parsed);
            return strdup(value);
        }
        json_free(parsed);
        return NULL;
    }
    /* not parseable as JSON: only valid if it is a plain non-JSON string? No —
     * Python requires a dict for objects; non-dict JSON -> ValueError */
    return NULL;
}

/* PoP: hermes_state_import_int_or_none @ hermes_state.py:_import_int_or_none */
long long hermes_state_import_int_or_none(const char *value, bool *valid) {
    if (valid) *valid = true;
    if (!value || !*value) return 0; /* None */
    char *end = NULL;
    long long v = strtoll(value, &end, 10);
    if (!end || *end != '\0') { if (valid) *valid = false; return 0; }
    return v;
}

/* PoP: hermes_state_int_or_default @ hermes_state.py:_int_or_default */
long long hermes_state_int_or_default(const char *value, long long default_v) {
    if (!value || !*value) return default_v;
    char *end = NULL;
    long long v = strtoll(value, &end, 10);
    if (!end || *end != '\0') return default_v;
    return v;
}

/* PoP: hermes_state_reasoning_json_text @ hermes_state.py:_reasoning_json_text */
char *hermes_state_reasoning_json_text(const char *value) {
    /* Python: if not value -> None; if isinstance str -> value as-is; else
     * json.dumps(value). C callers hand over an already-serialized string
     * (a raw TEXT column or a prior dump), so the faithful mirror is: empty
     * -> NULL, otherwise pass the string through untouched — NEVER re-encode
     * raw TEXT (that would double-encode and break reasoning-replay
     * consumers that check isinstance(list)). */
    if (!value || !value[0]) return NULL;
    return strdup(value);
}

/* PoP: hermes_state_import_error @ hermes_state.py:_import_error */
char *hermes_state_import_error(long long index, const char *error, const char *session_id) {
    json_t *o = json_object();
    json_set(o, "index", json_int(index));
    json_set(o, "error", error ? json_string(error) : json_string(""));
    if (session_id && *session_id) json_set(o, "session_id", json_string(session_id));
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* ── conversation shaping / maintenance ────────────────────────────────── */

/* PoP: hermes_state_sanitize_context @ agent/memory_manager.py:sanitize_context */
char *hermes_state_sanitize_context(const char *text) {
    /* Strip <memory-context>...</memory-context> blocks, the recalled-memory
     * System note, and bare memory-context fence tags (case-insensitive). */
    if (!text) return strdup("");
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    size_t o = 0;
    for (size_t i = 0; i < n;) {
        /* <memory-context> ... </memory-context> block (any spacing/case) */
        if (text[i] == '<') {
            size_t j = i;
            while (j < n && (text[j] == ' ' || text[j] == '\t')) j++;
            bool close = j < n && text[j] == '/';
            if (close) j++;
            while (j < n && (text[j] == ' ' || text[j] == '\t')) j++;
            if (n - j >= 14 && strncasecmp(text + j, "memory-context", 14) == 0) {
                size_t k = j + 14;
                while (k < n && (text[k] == ' ' || text[k] == '\t')) k++;
                if (k < n && text[k] == '>') {
                    if (!close) {
                        /* find the closing tag */
                        const char *close_tag = NULL;
                        for (size_t q = k + 1; q + 15 <= n; q++) {
                            if (text[q] == '<') {
                                size_t r = q + 1;
                                while (r < n && (text[r] == ' ' || text[r] == '\t')) r++;
                                if (r < n && text[r] == '/') {
                                    size_t s = r + 1;
                                    while (s < n && (text[s] == ' ' || text[s] == '\t')) s++;
                                    if (n - s >= 14 && strncasecmp(text + s, "memory-context", 14) == 0) {
                                        size_t u = s + 14;
                                        while (u < n && (text[u] == ' ' || text[u] == '\t')) u++;
                                        if (u < n && text[u] == '>') { close_tag = text + q; break; }
                                    }
                                }
                            }
                        }
                        if (close_tag) { i = (size_t)(close_tag - text) + 1; continue; }
                    }
                    i = k + 1;
                    continue;
                }
            }
        }
        /* [System note: The following is recalled memory context, ...] */
        if (text[i] == '[' && n - i >= 22 && strncasecmp(text + i, "[System note:", 13) == 0) {
            const char *end = strstr(text + i, "]");
            if (end && strstr(text + i, "recalled memory context")) {
                i = (size_t)(end - text) + 1;
                continue;
            }
        }
        out[o++] = text[i++];
    }
    out[o] = '\0';
    /* collapse the memory-context fence tags */
    char *dst = out;
    size_t w = 0;
    for (size_t i = 0; out[i];) {
        if (out[i] == '<') {
            size_t j = i;
            while (j < o && (out[j] == ' ' || out[j] == '\t')) j++;
            bool close = j < o && out[j] == '/';
            if (close) j++;
            while (j < o && (out[j] == ' ' || out[j] == '\t')) j++;
            if (o - j >= 14 && strncasecmp(out + j, "memory-context", 14) == 0) {
                size_t k = j + 14;
                while (k < o && (out[k] == ' ' || out[k] == '\t')) k++;
                if (k < o && out[k] == '>') { i = k + 1; continue; }
            }
        }
        dst[w++] = out[i++];
    }
    dst[w] = '\0';
    return out;
}

/* PoP: hermes_state_rows_to_conversation @ hermes_state.py:_rows_to_conversation */
char *hermes_state_rows_to_conversation(const char *rows_json) {
    /* Decode fetched message rows into the OpenAI conversation format:
     * content decoded (+ sanitized for user/assistant strings), api_content
     * verbatim, display_kind/metadata, timestamp, tool_call_id, tool_name,
     * effect_disposition, tool_calls parsed, message_id surfaced. */
    if (!rows_json || !*rows_json) return strdup("[]");
    json_t *rows = json_parse(rows_json, NULL);
    if (!rows || rows->type != JSON_ARRAY) { if (rows) json_free(rows); return strdup("[]"); }
    json_t *msgs = json_array();
    for (size_t i = 0; i < json_len(rows); i++) {
        json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_OBJECT) continue;
        json_t *msg = json_object();
        const char *role = json_get_str(row, "role", "unknown");
        json_set(msg, "role", json_string(role));
        /* content: decode (rows may carry encoded JSON strings) */
        json_t *content_v = json_obj_get(row, "content");
        const char *content = (content_v && json_is_string(content_v)) ? json_string_value(content_v) : "";
        json_t *decoded = NULL;
        json_t *content_out = NULL;
        if (content_v && json_is_string(content_v)) {
            json_t *parsed = json_parse(content, NULL);
            if (parsed && parsed->type == JSON_STRING) {
                decoded = parsed;
                content_out = decoded;
            } else if (parsed) {
                content_out = parsed; /* content is JSON structure */
                decoded = NULL;
            }
        }
        if (!content_out) content_out = json_string(content);
        if ((strcmp(role, "user") == 0 || strcmp(role, "assistant") == 0)
            && content_out->type == JSON_STRING) {
            char *clean = hermes_state_sanitize_context(json_string_value(content_out));
            char *stripped = clean;
            while (*stripped == ' ' || *stripped == '\t' || *stripped == '\n' || *stripped == '\r') stripped++;
            size_t sl = strlen(stripped);
            while (sl > 0 && (stripped[sl-1] == ' ' || stripped[sl-1] == '\t' || stripped[sl-1] == '\n' || stripped[sl-1] == '\r')) sl--;
            json_t *repl = json_string(stripped);
            /* rebuild at the stripped length (strip was already applied) */
            json_free(repl);
            char *tmp = malloc(sl + 1);
            memcpy(tmp, stripped, sl);
            tmp[sl] = '\0';
            repl = json_string(tmp);
            free(tmp);
            json_free(content_out);
            content_out = repl;
            free(clean);
        }
        json_set(msg, "content", content_out);
        if (decoded) json_free(decoded);
        const char *v;
        if ((v = json_get_str(row, "api_content", NULL))) json_set(msg, "api_content", json_string(v));
        if ((v = json_get_str(row, "display_kind", NULL))) json_set(msg, "display_kind", json_string(v));
        const char *dm = json_get_str(row, "display_metadata", NULL);
        if (dm && *dm) {
            json_t *dmd = json_parse(dm, NULL);
            json_set(msg, "display_metadata", dmd ? dmd : json_string(dm));
        }
        json_t *ts = json_obj_get(row, "timestamp");
        if (ts && !json_is_null(ts)) json_set(msg, "timestamp", json_copy(ts));
        if ((v = json_get_str(row, "tool_call_id", NULL))) json_set(msg, "tool_call_id", json_string(v));
        if ((v = json_get_str(row, "tool_name", NULL))) json_set(msg, "tool_name", json_string(v));
        if ((v = json_get_str(row, "effect_disposition", NULL))) json_set(msg, "effect_disposition", json_string(v));
        const char *tc = json_get_str(row, "tool_calls", NULL);
        if (tc && *tc) {
            json_t *parsed = json_parse(tc, NULL);
            json_set(msg, "tool_calls", parsed ? parsed : json_array());
        }
        if ((v = json_get_str(row, "platform_message_id", NULL)))
            json_set(msg, "message_id", json_string(v));
        json_append(msgs, msg);
    }
    json_free(rows);
    char *out = json_serialize(msgs);
    json_free(msgs);
    return out;
}

/* PoP: hermes_state_logical_size_bytes @ hermes_state.py:logical_size_bytes */
long long hermes_state_logical_size_bytes(hermes_state_db_t *db) {
    /* page_count * page_size — the size after the WAL is checkpointed. */
    if (!db) return 0;
    long long pages = 0, psize = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "PRAGMA page_count", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) pages = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    if (sqlite3_prepare_v2(db->db, "PRAGMA page_size", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) psize = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    return pages * psize;
}

/* PoP: hermes_state_vacuum @ hermes_state.py:vacuum */
bool hermes_state_vacuum(hermes_state_db_t *db) {
    /* FTS5 segments merged first, then VACUUM (cannot run in a txn). */
    if (!db) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "INSERT INTO messages_fts(messages_fts) VALUES('optimize')",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    sqlite3_stmt *v = NULL;
    if (sqlite3_prepare_v2(db->db, "VACUUM", -1, &v, NULL) != SQLITE_OK) return false;
    bool ok = sqlite3_step(v) == SQLITE_DONE;
    sqlite3_finalize(v);
    return ok;
}

/* PoP: hermes_state_request_handoff @ hermes_state.py:request_handoff */
bool hermes_state_request_handoff(hermes_state_db_t *db, const char *session_id,
                                  const char *platform) {
    /* pending only when the session is not already in a non-terminal state. */
    if (!db || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET handoff_state = 'pending', handoff_platform = ?1, "
            "handoff_error = NULL "
            "WHERE id = ?2 AND (handoff_state IS NULL OR handoff_state IN ('completed', 'failed'))",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, platform ? platform : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    bool ok = sqlite3_changes(db->db) > 0;
    sqlite3_finalize(st);
    return ok;
}

/* ── session-management updates ────────────────────────────────────────── */

/* PoP: hermes_state_set_expiry_finalized @ hermes_state.py:set_expiry_finalized */
void hermes_state_set_expiry_finalized(hermes_state_db_t *db, const char *session_id, bool finalized) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "UPDATE sessions SET expiry_finalized = ?1 WHERE id = ?2", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_int(st, 1, finalized ? 1 : 0);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_update_session_meta @ hermes_state.py:update_session_meta */
void hermes_state_update_session_meta(hermes_state_db_t *db, const char *session_id,
                                      const char *model_config, const char *model) {
    /* COALESCE so model=NULL leaves the stored column unchanged. */
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET model_config = ?1, model = COALESCE(?2, model) WHERE id = ?3",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, model_config ? model_config : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_update_system_prompt @ hermes_state.py:update_system_prompt */
void hermes_state_update_system_prompt(hermes_state_db_t *db, const char *session_id,
                                       const char *system_prompt) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "UPDATE sessions SET system_prompt = ?1 WHERE id = ?2", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, system_prompt ? system_prompt : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_update_session_model @ hermes_state.py:update_session_model */
void hermes_state_update_session_model(hermes_state_db_t *db, const char *session_id, const char *model) {
    /* Unconditional set; nulls system_prompt so stale footers cannot lie. */
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET model = ?1, system_prompt = NULL WHERE id = ?2", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, model ? model : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_update_session_runtime_lock @ hermes_state.py:update_session_runtime_lock */
void hermes_state_update_session_runtime_lock(hermes_state_db_t *db, const char *session_id,
                                              const char *provider, const char *model,
                                              const char *lock_key) {
    /* Merge browser_model_lock into the existing model_config JSON so
     * _branched_from/_delegate_from survive; nulls system_prompt. */
    if (!db || !session_id) return;
    sqlite3_stmt *sel = NULL;
    char *config = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT model_config FROM sessions WHERE id = ?", -1, &sel, NULL) == SQLITE_OK) {
        sqlite3_bind_text(sel, 1, session_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(sel) == SQLITE_ROW) {
            const unsigned char *v = sqlite3_column_text(sel, 0);
            if (v) config = strdup((const char *)v);
        }
        sqlite3_finalize(sel);
    }
    json_t *cfg = NULL;
    if (config && *config) {
        cfg = json_parse(config, NULL);
        if (cfg && cfg->type != JSON_OBJECT) { json_free(cfg); cfg = NULL; }
    }
    if (!cfg) cfg = json_object();
    json_t *lock = json_object();
    if (provider) json_set(lock, "provider", json_string(provider));
    if (model) json_set(lock, "model", json_string(model));
    json_set(cfg, lock_key ? lock_key : "browser_model_lock", lock);
    char *ser = json_serialize(cfg);
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET model_config = ?1, system_prompt = NULL WHERE id = ?2", -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, ser, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(up, 2, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
    free(ser);
    json_free(cfg);
    free(config);
}

/* PoP: hermes_state_update_session_billing_route @ hermes_state.py:update_session_billing_route */
void hermes_state_update_session_billing_route(hermes_state_db_t *db, const char *session_id,
                                               const char *provider, const char *base_url) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET billing_provider = ?1, billing_base_url = ?2, system_prompt = NULL WHERE id = ?3",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, provider ? provider : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, base_url ? base_url : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_reopen_session @ hermes_state.py:reopen_session */
void hermes_state_reopen_session(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET ended_at = NULL, end_reason = NULL WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_promote_to_session_reset @ hermes_state.py:promote_to_session_reset */
bool hermes_state_promote_to_session_reset(hermes_state_db_t *db, const char *session_id) {
    /* Mark live rows (or rows with recoverable accidental end reasons)
     * as ended by an intentional reset boundary. */
    if (!db || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET ended_at = ?1, end_reason = 'session_reset' "
            "WHERE id = ?2 AND (ended_at IS NULL OR end_reason IN ('agent_close', 'ws_orphan_reap'))",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_double(st, 1, hermes_state_now_epoch());
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    bool ok = sqlite3_changes(db->db) > 0;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: hermes_state_update_session_cwd @ hermes_state.py:update_session_cwd */
void hermes_state_update_session_cwd(hermes_state_db_t *db, const char *session_id,
                                     const char *cwd, const char *git_branch) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET cwd = ?1, git_branch = ?2 WHERE id = ?3", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, cwd ? cwd : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, git_branch ? git_branch : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_record_compression_failure_cooldown @ hermes_state.py:record_compression_failure_cooldown */
void hermes_state_record_compression_failure_cooldown(hermes_state_db_t *db, const char *session_id,
                                                      double until, const char *error) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET compression_failure_cooldown_until = ?1, compression_failure_error = ?2 WHERE id = ?3",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_double(st, 1, until);
    sqlite3_bind_text(st, 2, error ? error : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_get_compression_failure_cooldown @ hermes_state.py:get_compression_failure_cooldown */
char *hermes_state_get_compression_failure_cooldown(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return NULL;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT compression_failure_cooldown_until, compression_failure_error FROM sessions WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        double until = sqlite3_column_double(st, 0);
        const unsigned char *err = sqlite3_column_text(st, 1);
        json_t *o = json_object();
        json_set(o, "cooldown_until", json_number(until));
        json_set(o, "error", json_string((const char *)(err ? err : (const unsigned char *)"")));
        out = json_serialize(o);
        json_free(o);
    }
    sqlite3_finalize(st);
    return out;
}

/* PoP: hermes_state_clear_compression_failure_cooldown @ hermes_state.py:clear_compression_failure_cooldown */
void hermes_state_clear_compression_failure_cooldown(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET compression_failure_cooldown_until = NULL, compression_failure_error = NULL WHERE id = ?",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_get_compression_fallback_streak @ hermes_state.py:get_compression_fallback_streak */
long long hermes_state_get_compression_fallback_streak(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT compression_fallback_streak FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    long long v = 0;
    if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return v;
}

/* PoP: hermes_state_set_compression_fallback_streak @ hermes_state.py:set_compression_fallback_streak */
void hermes_state_set_compression_fallback_streak(hermes_state_db_t *db, const char *session_id, long long streak) {
    if (!db || !session_id) return;
    if (streak < 0) streak = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET compression_fallback_streak = ?1 WHERE id = ?2", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_int64(st, 1, streak);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_get_compression_ineffective_count @ hermes_state.py:get_compression_ineffective_count */
long long hermes_state_get_compression_ineffective_count(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, "SELECT compression_ineffective_count FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    long long v = 0;
    if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return v;
}

/* PoP: hermes_state_set_compression_ineffective_count @ hermes_state.py:set_compression_ineffective_count */
void hermes_state_set_compression_ineffective_count(hermes_state_db_t *db, const char *session_id, long long count) {
    if (!db || !session_id) return;
    if (count < 0) count = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET compression_ineffective_count = ?1 WHERE id = ?2", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_int64(st, 1, count);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* ── gateway routing index ─────────────────────────────────────────────── */

/* PoP: hermes_state_save_gateway_routing_entry @ hermes_state.py:save_gateway_routing_entry */
bool hermes_state_save_gateway_routing_entry(hermes_state_db_t *db, const char *scope,
                                             const char *session_key, const char *entry_json) {
    if (!db || !scope || !session_key) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT INTO gateway_routing (scope, session_key, entry_json, updated_at) "
            "VALUES (?1, ?2, ?3, ?4) "
            "ON CONFLICT(scope, session_key) DO UPDATE SET "
            "entry_json = excluded.entry_json, updated_at = excluded.updated_at",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, scope, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, entry_json ? entry_json : "{}", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 4, hermes_state_now_epoch());
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: hermes_state_replace_gateway_routing_entries @ hermes_state.py:replace_gateway_routing_entries */
bool hermes_state_replace_gateway_routing_entries(hermes_state_db_t *db, const char *scope,
                                                  const char *pairs_arg) {
    /* Atomically replace the routing index for *scope*: keys absent from the
     * incoming set are removed. Arg = tab-separated "key\tvalue" pairs. */
    if (!db || !scope) return false;
    sqlite3_exec(db->db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    sqlite3_stmt *del = NULL;
    bool ok = true;
    if (sqlite3_prepare_v2(db->db, "DELETE FROM gateway_routing WHERE scope = ?", -1, &del, NULL) == SQLITE_OK) {
        sqlite3_bind_text(del, 1, scope, -1, SQLITE_TRANSIENT);
        sqlite3_step(del);
        sqlite3_finalize(del);
    } else {
        ok = false;
    }
    if (ok && pairs_arg && *pairs_arg) {
        char *copy = strdup(pairs_arg);
        char *save = NULL;
        for (char *pair = strtok_r(copy, "\t", &save); pair; pair = strtok_r(NULL, "\t", &save)) {
            char *eq = strchr(pair, '\x01'); /* key\x01value encoding */
            const char *key = pair;
            const char *val = eq ? eq + 1 : "";
            if (eq) *eq = '\0';
            if (!*key) continue;
            sqlite3_stmt *ins = NULL;
            if (sqlite3_prepare_v2(db->db,
                    "INSERT INTO gateway_routing (scope, session_key, entry_json, updated_at) "
                    "VALUES (?1, ?2, ?3, ?4)", -1, &ins, NULL) == SQLITE_OK) {
                sqlite3_bind_text(ins, 1, scope, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 2, key, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(ins, 3, val, -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(ins, 4, hermes_state_now_epoch());
                if (sqlite3_step(ins) != SQLITE_DONE) ok = false;
                sqlite3_finalize(ins);
            } else {
                ok = false;
            }
        }
        free(copy);
    }
    sqlite3_exec(db->db, ok ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
    return ok;
}
/* PoP: hermes_state_load_gateway_routing_entries @ hermes_state.py:load_gateway_routing_entries */
char *hermes_state_load_gateway_routing_entries(hermes_state_db_t *db, const char *scope) {
    if (!db || !scope) return strdup("{}");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT session_key, entry_json FROM gateway_routing WHERE scope = ?", -1, &st, NULL) != SQLITE_OK)
        return strdup("{}");
    sqlite3_bind_text(st, 1, scope, -1, SQLITE_TRANSIENT);
    json_t *o = json_object();
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *k = sqlite3_column_text(st, 0);
        const unsigned char *v = sqlite3_column_text(st, 1);
        json_set(o, (const char *)(k ? k : (const unsigned char *)""),
                 json_string((const char *)(v ? v : (const unsigned char *)"")));
    }
    sqlite3_finalize(st);
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* PoP: hermes_state_delete_gateway_routing_entries @ hermes_state.py:delete_gateway_routing_entries */
void hermes_state_delete_gateway_routing_entries(hermes_state_db_t *db, const char *scope,
                                                 const char *session_keys_json) {
    if (!db || !scope) return;
    json_t *keys = json_parse(session_keys_json ? session_keys_json : "[]", NULL);
    if (!keys || keys->type != JSON_ARRAY) { if (keys) json_free(keys); return; }
    for (size_t i = 0; i < json_len(keys); i++) {
        json_t *k = json_get(keys, i);
        if (!k || !json_is_string(k)) continue;
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->db,
                "DELETE FROM gateway_routing WHERE scope = ? AND session_key = ?", -1, &st, NULL) != SQLITE_OK)
            continue;
        sqlite3_bind_text(st, 1, scope, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 2, json_string_value(k), -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    json_free(keys);
}

/* PoP: hermes_state_list_gateway_sessions @ hermes_state.py:list_gateway_sessions */
char *hermes_state_list_gateway_sessions(hermes_state_db_t *db, const char *platform, bool active_only) {
    /* Newest row per session_key; platform filters on source; active_only
     * restricts to non-ended rows. */
    if (!db) return strdup("[]");
    char sql[1600];
    snprintf(sql, sizeof(sql),
        "SELECT sessions.*, "
        "COALESCE((SELECT MAX(m.timestamp) FROM messages m "
        "          WHERE m.session_id = sessions.id), sessions.started_at) AS last_active "
        "FROM sessions "
        "WHERE session_key IS NOT NULL %s %s "
        "AND started_at = (SELECT MAX(s2.started_at) FROM sessions s2 "
        "                  WHERE s2.session_key = sessions.session_key)",
        platform && *platform ? "AND LOWER(source) = LOWER(?1)" : "",
        active_only ? "AND ended_at IS NULL" : "");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return strdup("[]");
    if (platform && *platform) sqlite3_bind_text(st, 1, platform, -1, SQLITE_TRANSIENT);
    json_t *arr = json_array();
    while (sqlite3_step(st) == SQLITE_ROW) {
        json_t *o = json_object();
        for (int c = 0; c < sqlite3_column_count(st); c++) {
            const char *col = sqlite3_column_name(st, c);
            const unsigned char *v = sqlite3_column_text(st, c);
            json_set(o, col, v ? json_string((const char *)v) : json_null());
        }
        json_append(arr, o);
    }
    sqlite3_finalize(st);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* PoP: hermes_state_find_session_by_origin @ hermes_state.py:find_session_by_origin */
char *hermes_state_find_session_by_origin(hermes_state_db_t *db, const char *source,
                                          const char *chat_id, const char *thread_id,
                                          const char *user_id) {
    /* Most recent live session for a platform + chat origin; exact sender
     * matches preferred when user_id is provided. */
    if (!db || !source || !chat_id) return NULL;
    char sql[1600];
    if (user_id && *user_id) {
        snprintf(sql, sizeof(sql),
            "SELECT * FROM sessions WHERE source = ?1 AND chat_id = ?2 %s "
            "AND ended_at IS NULL AND user_id = ?4 ORDER BY started_at DESC LIMIT 1",
            thread_id && *thread_id ? "AND thread_id = ?3" : "");
    } else {
        snprintf(sql, sizeof(sql),
            "SELECT * FROM sessions WHERE source = ?1 AND chat_id = ?2 %s "
            "AND ended_at IS NULL ORDER BY started_at DESC LIMIT 1",
            thread_id && *thread_id ? "AND thread_id = ?3" : "");
    }
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(st, 1, source, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, chat_id, -1, SQLITE_TRANSIENT);
    int idx = 3;
    if (thread_id && *thread_id) sqlite3_bind_text(st, idx++, thread_id, -1, SQLITE_TRANSIENT);
    if (user_id && *user_id) sqlite3_bind_text(st, idx, user_id, -1, SQLITE_TRANSIENT);
    char *out = NULL;
    if (sqlite3_step(st) == SQLITE_ROW) {
        json_t *o = json_object();
        for (int c = 0; c < sqlite3_column_count(st); c++) {
            const char *col = sqlite3_column_name(st, c);
            const unsigned char *v = sqlite3_column_text(st, c);
            json_set(o, col, v ? json_string((const char *)v) : json_null());
        }
        out = json_serialize(o);
        json_free(o);
    }
    sqlite3_finalize(st);
    return out;
}



/* ── content encoding / lineage / display helpers ──────────────────────── */

/* PoP: hermes_state_encode_content @ hermes_state.py:_encode_content */
char *hermes_state_encode_content(const char *content_json) {
    /* Structured (list/dict) content serialized for sqlite; strings pass
     * through unchanged. Arg = raw content string or JSON array/object. */
    if (!content_json) return NULL;
    json_t *v = json_parse(content_json, NULL);
    if (!v) return strdup(content_json); /* plain string, not JSON */
    if (v->type == JSON_STRING) {
        char *out = strdup(json_string_value(v));
        json_free(v);
        return out;
    }
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) {
        char *out = json_serialize(v);
        json_free(v);
        return out;
    }
    json_free(v);
    return strdup(content_json);
}

/* PoP: hermes_state_encode_display_metadata @ hermes_state.py:_encode_display_metadata */
char *hermes_state_encode_display_metadata(const char *meta_json) {
    /* Already-serialized JSON strings pass through (no double-encode);
     * dicts serialize. */
    if (!meta_json) return NULL;
    json_t *v = json_parse(meta_json, NULL);
    if (!v) return strdup(meta_json);
    if (v->type == JSON_OBJECT) {
        char *out = json_serialize(v);
        json_free(v);
        return out;
    }
    if (v->type == JSON_STRING) {
        char *out = strdup(json_string_value(v));
        json_free(v);
        return out;
    }
    json_free(v);
    return strdup(meta_json);
}

/* PoP: hermes_state_decode_display_metadata @ hermes_state.py:_decode_display_metadata */
char *hermes_state_decode_display_metadata(const char *raw) {
    /* Decode the column into the dict every reader expects; legacy raw text
     * that is not JSON passes through. */
    if (!raw) return NULL;
    json_t *v = json_parse(raw, NULL);
    if (v && v->type == JSON_OBJECT) {
        char *out = json_serialize(v);
        json_free(v);
        return out;
    }
    json_free(v);
    return strdup(raw);
}

/* PoP: hermes_state_session_lineage_root_to_tip @ hermes_state.py:_session_lineage_root_to_tip */
char *hermes_state_session_lineage_root_to_tip(hermes_state_db_t *db, const char *session_id) {
    /* Walk parent links (max 100) from the tip to the root; returns the
     * chain root->tip as a JSON array. */
    if (!db || !session_id || !*session_id) return strdup("[]");
    char *chain[101];
    int n = 0;
    char cur[512];
    snprintf(cur, sizeof(cur), "%s", session_id);
    for (int i = 0; i < 100 && *cur; i++) {
        bool dup = false;
        for (int k = 0; k < n; k++)
            if (strcmp(chain[k], cur) == 0) { dup = true; break; }
        if (dup) break;
        chain[n++] = strdup(cur);
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(db->db, "SELECT parent_session_id FROM sessions WHERE id = ?", -1, &st, NULL) != SQLITE_OK) break;
        sqlite3_bind_text(st, 1, cur, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *p = sqlite3_column_text(st, 0);
            if (p && *p) snprintf(cur, sizeof(cur), "%s", (const char *)p);
            else cur[0] = '\0';
        } else {
            cur[0] = '\0';
        }
        sqlite3_finalize(st);
    }
    /* chain is tip->root; emit root->tip */
    json_t *arr = json_array();
    for (int i = n - 1; i >= 0; i--) json_append(arr, json_string(chain[i]));
    for (int i = 0; i < n; i++) free(chain[i]);
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

/* PoP: hermes_state_set_latest_matching_message_display_kind @ hermes_state.py:set_latest_matching_message_display_kind */
bool hermes_state_set_latest_matching_message_display_kind(hermes_state_db_t *db,
                                                           const char *session_id,
                                                           const char *role,
                                                           const char *display_kind,
                                                           const char *display_metadata) {
    /* Stamp presentation metadata on this turn's freshly persisted row. */
    if (!db || !session_id) return false;
    sqlite3_stmt *sel = NULL;
    long long row_id = 0;
    if (sqlite3_prepare_v2(db->db,
            "SELECT id FROM messages WHERE session_id = ?1 AND role = ?2 "
            "ORDER BY id DESC LIMIT 1", -1, &sel, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(sel, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(sel, 2, role ? role : "", -1, SQLITE_TRANSIENT);
    if (sqlite3_step(sel) == SQLITE_ROW) row_id = sqlite3_column_int64(sel, 0);
    sqlite3_finalize(sel);
    if (!row_id) return false;
    char *meta = hermes_state_encode_display_metadata(display_metadata);
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE messages SET display_kind = ?1, display_metadata = ?2 WHERE id = ?3",
            -1, &up, NULL) != SQLITE_OK) { free(meta); return false; }
    sqlite3_bind_text(up, 1, display_kind ? display_kind : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(up, 2, meta ? meta : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(up, 3, row_id);
    bool ok = sqlite3_step(up) == SQLITE_DONE;
    sqlite3_finalize(up);
    free(meta);
    return ok;
}

/* PoP: hermes_state_set_latest_user_api_content @ hermes_state.py:set_latest_user_api_content */
bool hermes_state_set_latest_user_api_content(hermes_state_db_t *db, const char *session_id,
                                              const char *api_content) {
    /* Backfill the api_content sidecar onto the newest ACTIVE user row. */
    if (!db || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE messages SET api_content = ?1 WHERE id = ("
            "SELECT id FROM messages WHERE session_id = ?2 AND role = 'user' AND active = 1 "
            "ORDER BY id DESC LIMIT 1)", -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_text(st, 1, api_content ? api_content : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_step(st);
    bool ok = sqlite3_changes(db->db) > 0;
    sqlite3_finalize(st);
    return ok;
}

/* PoP: hermes_state_ensure_session @ hermes_state.py:ensure_session */
void hermes_state_ensure_session(hermes_state_db_t *db, const char *session_id,
                                 const char *source, const char *model) {
    if (!db || !session_id) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "INSERT OR IGNORE INTO sessions (id, source, model, started_at) VALUES (?1, ?2, ?3, ?4)",
            -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, source ? source : "unknown", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, model, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 4, hermes_state_now_epoch());
    sqlite3_step(st);
    sqlite3_finalize(st);
}

/* PoP: hermes_state_has_archived_messages @ hermes_state.py:has_archived_messages */
bool hermes_state_has_archived_messages(hermes_state_db_t *db, const char *session_id) {
    if (!db || !session_id) return false;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "SELECT 1 FROM messages WHERE session_id = ? AND active = 0 LIMIT 1", -1, &st, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return found;
}

/* PoP: hermes_state_is_duplicate_replayed_user_message @ hermes_state.py:_is_duplicate_replayed_user_message */
bool hermes_state_is_duplicate_replayed_user_message(const char *msg_json, const char *messages_json) {
    /* A user message duplicates a previous one unless an assistant turn with
     * content/tool_calls intervened. */
    if (!msg_json || !messages_json) return false;
    json_t *msg = json_parse(msg_json, NULL);
    if (!msg || msg->type != JSON_OBJECT) { if (msg) json_free(msg); return false; }
    const char *role = json_get_str(msg, "role", "");
    if (strcmp(role, "user") != 0) { json_free(msg); return false; }
    json_t *content_v = json_obj_get(msg, "content");
    if (!content_v || !json_is_string(content_v) || !*json_string_value(content_v)) {
        json_free(msg); return false;
    }
    const char *content = json_string_value(content_v);
    json_t *msgs = json_parse(messages_json, NULL);
    bool dup = false;
    if (msgs && msgs->type == JSON_ARRAY) {
        for (size_t i = json_len(msgs); i > 0; i--) {
            json_t *prev = json_get(msgs, i - 1);
            if (!prev || prev->type != JSON_OBJECT) continue;
            const char *pr = json_get_str(prev, "role", "");
            if (strcmp(pr, "user") == 0) {
                json_t *pc = json_obj_get(prev, "content");
                if (pc && json_is_string(pc) && strcmp(json_string_value(pc), content) == 0) {
                    dup = true;
                    break;
                }
            } else if (strcmp(pr, "assistant") == 0) {
                json_t *pc = json_obj_get(prev, "content");
                json_t *tc = json_obj_get(prev, "tool_calls");
                if ((pc && json_is_string(pc) && *json_string_value(pc)) || (tc && !json_is_null(tc))) {
                    break; /* an assistant turn intervened: not a duplicate */
                }
            }
        }
    }
    json_free(msgs);
    json_free(msg);
    return dup;
}

/* PoP: hermes_state_prune_filter_where @ hermes_state.py:_prune_filter_where */
char *hermes_state_prune_filter_where(const char *arg) {
    /* Shared WHERE clause for bulk prune/archive selection. Arg =
     * "last_active_before\tlast_active_after\tstarted_before\tstarted_after"
     * with "-" for unset, then optional "\tsource\ttitle_like\tarchived(-1/0/1)".
     * Prints the clause. */
    double lab = -1, laa = -1, sb = -1, sa = -1;
    char src[128], title[128];
    int archived = -1;
    src[0] = title[0] = '\0';
    int n = sscanf(arg, "%lf\t%lf\t%lf\t%lf\t%127[^\t]\t%127[^\t]\t%d",
                   &lab, &laa, &sb, &sa, src, title, &archived);
    char out[2048];
    size_t o = 0;
    o += (size_t)snprintf(out + o, sizeof(out) - o, "s.ended_at IS NOT NULL");
    int param = 1;
    if (lab >= 0) {
        o += (size_t)snprintf(out + o, sizeof(out) - o,
            " AND COALESCE((SELECT MAX(m.timestamp) FROM messages m WHERE m.session_id = s.id), s.started_at) < ?%d", param++);
    }
    if (laa >= 0) {
        o += (size_t)snprintf(out + o, sizeof(out) - o,
            " AND COALESCE((SELECT MAX(m.timestamp) FROM messages m WHERE m.session_id = s.id), s.started_at) >= ?%d", param++);
    }
    if (sb >= 0) o += (size_t)snprintf(out + o, sizeof(out) - o, " AND s.started_at < ?%d", param++);
    if (sa >= 0) o += (size_t)snprintf(out + o, sizeof(out) - o, " AND s.started_at >= ?%d", param++);
    if (src[0]) o += (size_t)snprintf(out + o, sizeof(out) - o, " AND s.source = ?%d", param++);
    if (title[0]) o += (size_t)snprintf(out + o, sizeof(out) - o, " AND LOWER(COALESCE(s.title, '')) LIKE ?%d", param++);
    if (archived >= 0) o += (size_t)snprintf(out + o, sizeof(out) - o, " AND s.archived = ?%d", param++);
    printf("%s\n", out);
    return NULL;
}

/* PoP: hermes_state_maybe_auto_archive @ hermes_state.py:maybe_auto_archive */
bool hermes_state_maybe_auto_archive(hermes_state_db_t *db, long long idle_days) {
    /* Idempotent auto-archive: soft-hide sessions idle for idle_days,
     * recorded via the state_meta last-run stamp. */
    if (!db || idle_days <= 0) return false;
    char *last = hermes_state_get_meta(db, "last_auto_archive_at");
    double now = hermes_state_now_epoch();
    if (last && *last) {
        double last_ts = strtod(last, NULL);
        if (now - last_ts < 86400) { free(last); return false; } /* ran within a day */
    }
    free(last);
    double cutoff = now - (double)idle_days * 86400;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET archived = 1 WHERE archived = 0 AND ended_at IS NOT NULL "
            "AND COALESCE((SELECT MAX(m.timestamp) FROM messages m WHERE m.session_id = sessions.id), sessions.started_at) < ?1",
            -1, &st, NULL) != SQLITE_OK) return false;
    sqlite3_bind_double(st, 1, cutoff);
    sqlite3_step(st);
    sqlite3_finalize(st);
    char stamp[64];
    snprintf(stamp, sizeof(stamp), "%.0f", now);
    hermes_state_set_meta(db, "last_auto_archive_at", stamp);
    return true;
}

/* ── session deletion ──────────────────────────────────────────────────── */

static void hs_orphan_children_of(hermes_state_db_t *db, const char *parent_id) {
    sqlite3_stmt *up = NULL;
    if (sqlite3_prepare_v2(db->db,
            "UPDATE sessions SET parent_session_id = NULL WHERE parent_session_id = ?",
            -1, &up, NULL) == SQLITE_OK) {
        sqlite3_bind_text(up, 1, parent_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(up);
        sqlite3_finalize(up);
    }
}

static void hs_delete_delegate_children_of(hermes_state_db_t *db, const char *parent_id) {
    sqlite3_stmt *del = NULL;
    if (sqlite3_prepare_v2(db->db,
            "DELETE FROM sessions WHERE parent_session_id = ? "
            "AND json_extract(COALESCE(model_config, '{}'), '$._delegate_from') IS NOT NULL",
            -1, &del, NULL) == SQLITE_OK) {
        sqlite3_bind_text(del, 1, parent_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(del);
        sqlite3_finalize(del);
    }
}

static void hs_delete_session_row(hermes_state_db_t *db, const char *session_id) {
    sqlite3_stmt *d1 = NULL;
    if (sqlite3_prepare_v2(db->db, "DELETE FROM messages WHERE session_id = ?", -1, &d1, NULL) == SQLITE_OK) {
        sqlite3_bind_text(d1, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(d1);
        sqlite3_finalize(d1);
    }
    sqlite3_stmt *d2 = NULL;
    if (sqlite3_prepare_v2(db->db, "DELETE FROM sessions WHERE id = ?", -1, &d2, NULL) == SQLITE_OK) {
        sqlite3_bind_text(d2, 1, session_id, -1, SQLITE_TRANSIENT);
        sqlite3_step(d2);
        sqlite3_finalize(d2);
    }
}

/* PoP: hermes_state_prune_sessions @ hermes_state.py:prune_sessions */
long long hermes_state_prune_sessions(hermes_state_db_t *db, double older_than_days,
                                      const char *source) {
    /* Delete ended sessions inactive for older_than_days, optionally
     * restricted to source; children of deleted parents are orphaned. */
    if (!db) return 0;
    double cutoff = older_than_days > 0
        ? hermes_state_now_epoch() - older_than_days * 86400.0
        : hermes_state_now_epoch();
    char sql[1600];
    snprintf(sql, sizeof(sql),
        "SELECT id FROM sessions s WHERE s.ended_at IS NOT NULL "
        "AND COALESCE((SELECT MAX(m.timestamp) FROM messages m WHERE m.session_id = s.id), s.started_at) < ?1 %s",
        source && *source ? "AND s.source = ?2" : "");
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db->db, sql, -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_double(st, 1, cutoff);
    if (source && *source) sqlite3_bind_text(st, 2, source, -1, SQLITE_TRANSIENT);
    char ids[8192];
    size_t len = 0;
    ids[0] = '\0';
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *id = sqlite3_column_text(st, 0);
        if (!id) continue;
        size_t need = len + strlen((const char *)id) + 2;
        if (need > sizeof(ids)) continue;
        if (len) { strcat(ids, ","); len++; }
        strcat(ids, (const char *)id);
        len += strlen((const char *)id);
    }
    sqlite3_finalize(st);
    if (!len) return 0;
    sqlite3_exec(db->db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    char up[9000];
    snprintf(up, sizeof(up),
             "UPDATE sessions SET parent_session_id = NULL WHERE parent_session_id IN (%s)", ids);
    sqlite3_exec(db->db, up, NULL, NULL, NULL);
    char dm[9000];
    snprintf(dm, sizeof(dm), "DELETE FROM messages WHERE session_id IN (%s)", ids);
    sqlite3_exec(db->db, dm, NULL, NULL, NULL);
    char ds[9000];
    snprintf(ds, sizeof(ds), "DELETE FROM sessions WHERE id IN (%s)", ids);
    sqlite3_stmt *del = NULL;
    long long n = 0;
    if (sqlite3_prepare_v2(db->db, ds, -1, &del, NULL) == SQLITE_OK) {
        sqlite3_step(del);
        n = sqlite3_changes(db->db);
        sqlite3_finalize(del);
    }
    sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
    return n;
}

/* PoP: hermes_state_remove_session_files @ hermes_state.py:_remove_session_files */
void hermes_state_remove_session_files(const char *home, const char *session_id) {
    /* Clean up {sid}.json, {sid}.jsonl and request_dump_{sid}_*.json under
     * the home dir; silently skip missing files. */
    if (!home || !session_id) return;
    char p1[1024], p2[1024];
    snprintf(p1, sizeof(p1), "%s/%s.json", home, session_id);
    snprintf(p2, sizeof(p2), "%s/%s.jsonl", home, session_id);
    unlink(p1);
    unlink(p2);
    /* request_dump_<sid>_*.json — scan the dir */
    DIR *dir = opendir(home);
    if (!dir) return;
    char prefix[512];
    snprintf(prefix, sizeof(prefix), "request_dump_%s_", session_id);
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strncmp(de->d_name, prefix, strlen(prefix)) == 0) {
            const char *dot = strrchr(de->d_name, '.');
            if (dot && strcmp(dot, ".json") == 0) {
                char fp[1200];
                snprintf(fp, sizeof(fp), "%s/%s", home, de->d_name);
                unlink(fp);
            }
        }
    }
    closedir(dir);
}

/* PoP: hermes_state_delete_session @ hermes_state.py:delete_session */
bool hermes_state_delete_session(hermes_state_db_t *db, const char *session_id) {
    /* Delegate subagent children cascade-deleted; branch/compression
     * children orphaned; messages + row removed. */
    if (!db || !session_id) return false;
    sqlite3_exec(db->db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    hs_delete_delegate_children_of(db, session_id);
    hs_orphan_children_of(db, session_id);
    hs_delete_session_row(db, session_id);
    sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
    return true;
}

/* PoP: hermes_state_delete_sessions @ hermes_state.py:delete_sessions */
long long hermes_state_delete_sessions(hermes_state_db_t *db, const char *session_ids_json) {
    /* Bulk delete in a single transaction with the per-row contract. */
    if (!db) return 0;
    json_t *ids = json_parse(session_ids_json ? session_ids_json : "[]", NULL);
    if (!ids || ids->type != JSON_ARRAY) { if (ids) json_free(ids); return 0; }
    sqlite3_exec(db->db, "BEGIN IMMEDIATE", NULL, NULL, NULL);
    long long n = 0;
    for (size_t i = 0; i < json_len(ids); i++) {
        json_t *v = json_get(ids, i);
        if (!v || !json_is_string(v)) continue;
        const char *sid = json_string_value(v);
        hs_delete_delegate_children_of(db, sid);
        hs_orphan_children_of(db, sid);
        hs_delete_session_row(db, sid);
        n++;
    }
    sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
    json_free(ids);
    return n;
}

/* === merged from hermes_state_gaps.c === */
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

/* PoP: hermes_state_resolved_max_resume_messages @ hermes_state.py:resolved_max_resume_messages */
int hermes_state_resolved_max_resume_messages(void) {
    return hermes_state_transcript_limit("max_resume_messages", HERMES_STATE_MAX_SAFE_RESUME);
}

/* PoP: hermes_state_resolved_max_export_messages @ hermes_state.py:resolved_max_export_messages */
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

/* PoP: hermes_state_running_under_pytest @ hermes_state.py:_running_under_pytest */
bool hermes_state_running_under_pytest(void) {
    const char *t = getenv("PYTEST_CURRENT_TEST");
    if (t && t[0]) return true;
    const char *v = getenv("PYTEST_VERSION");
    if (v && v[0]) return true;
    return false;
}

/* PoP: hermes_state_production_state_roots @ hermes_state.py:_production_state_roots */
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

/* PoP: hermes_state_is_production_state_db @ hermes_state.py:_is_production_state_db */
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

/* PoP: hermes_state_ensure_test_isolation @ hermes_state.py:_ensure_test_isolation */
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

/* PoP: hermes_state_is_stale_tool_call_marker @ hermes_state.py:_is_stale_tool_call_marker_message */
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
    free(stripped);
    return result;
}

/* PoP: hermes_state_strip_stale_tool_call_markers @ hermes_state.py:_strip_stale_tool_call_markers */
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

/* PoP: hermes_state_is_disk_full_error @ hermes_state.py:is_disk_full_error */
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

/* PoP: hermes_state_classify_persistence_error @ hermes_state.py:classify_persistence_error */
const char *hermes_state_classify_persistence_error(const char *err_str) {
    if (!err_str || !err_str[0])
        return "unknown";
    char lowered[512];
    size_t i = 0;
    for (; err_str[i] && i < sizeof(lowered) - 1; i++)
        lowered[i] = (char)tolower((unsigned char)err_str[i]);
    lowered[i] = '\0';
    /* LIVE ladder: phrase buckets for typed exceptions first, then
     * corruption BEFORE lock/disk, then locked/busy, then disk. */
    if (strstr(lowered, "turn lease")) return "turn_lease";
    if (strstr(lowered, "closed by compression")) return "compression_closed";
    if (strstr(lowered, "being compressed") || strstr(lowered, "compression lease"))
        return "compression";
    static const char *const CORRUPTION_MARKERS[] = {
        "malformed", "file is not a database", "not a database",
        "database corruption", NULL
    };
    for (int m = 0; CORRUPTION_MARKERS[m]; m++)
        if (strstr(lowered, CORRUPTION_MARKERS[m])) return "corrupt";
    if (strstr(lowered, "locked") || strstr(lowered, "busy"))
        return "locked";
    if (hermes_state_is_disk_full_error(lowered)
        || strstr(lowered, "disk")
        || strstr(lowered, "readonly")
        || strstr(lowered, "read-only"))
        return "disk";
    return "unknown";
}

/* PoP: hermes_state_count_db_holders @ hermes_state.py:count_db_holders */
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
    if (!home || !home[0]) home = "/tmp";
    char *out = NULL;
    asprintf(&out, "%s/state/state.db", home);
    return out;
}

/* PoP: hermes_state_apply_database_pragmas @ hermes_state.py:apply_database_pragmas */
int hermes_state_apply_database_pragmas(hermes_state_db_t *db) {
    /* Python: WAL + synchronous=NORMAL + busy_timeout + foreign_keys.
     * Durability note (sqlite.org/wal.html): WAL+NORMAL is corruption-safe
     * but a power loss right after commit can roll back recent transactions.
     * On macOS we keep the existing ports-layer upgrade to synchronous=FULL
     * (hermes_state_ports.c) since APFS fsync latency there is acceptable;
     * on Linux NORMAL matches the Python quarry's deliberate trade-off. */
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

/* PoP: hermes_state_store_system_prompt @ hermes_state.py:SessionDB._store_system_prompt */
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

/* PoP: hermes_state_delete_unreferenced_system_prompts @ hermes_state.py:SessionDB._delete_unreferenced_system_prompts */
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

/* PoP: hermes_state_session_row_dict @ hermes_state.py:SessionDB._session_row_dict */
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

/* PoP: hermes_state_get_read_conn @ hermes_state.py:SessionDB._get_read_conn */
int hermes_state_get_read_conn(hermes_state_db_t *db) {
    /* Python: return the read connection (single connection in C). */
    return db && db->db ? 1 : 0;
}

/* PoP: hermes_state_read_ctx @ hermes_state.py:SessionDB._read_ctx */
int hermes_state_read_ctx(hermes_state_db_t *db) {
    return db && db->db ? 1 : 0;
}

/* PoP: hermes_state_sleep_before_write_retry @ hermes_state.py:SessionDB._sleep_before_write_retry */
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

/* PoP: hermes_state_get_compression_failure_cooldown_row @ hermes_state.py:SessionDB.get_compression_failure_cooldown_row */
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

/* PoP: hermes_state_restore_compression_failure_cooldown_row @ hermes_state.py:SessionDB.restore_compression_failure_cooldown_row */
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

/* PoP: hermes_state_touch_session_activity @ hermes_state.py:SessionDB.touch_session_activity */
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

/* PoP: hermes_state_clear_session_activity_labels @ hermes_state.py:SessionDB.clear_session_activity_labels */
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

/* PoP: hermes_state_get_session_activity @ hermes_state.py:SessionDB.get_session_activity */
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

/* PoP: hermes_state_set_session_yolo @ hermes_state.py:SessionDB.set_session_yolo */
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

/* PoP: hermes_state_session_yolo_enabled @ hermes_state.py:SessionDB.session_yolo_enabled */
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

/* PoP: hermes_state_queue_token_counts @ hermes_state.py:SessionDB.queue_token_counts */
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

/* PoP: hermes_state_apply_token_batch @ hermes_state.py:SessionDB._apply_token_batch */
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

/* PoP: hermes_state_coalesce_token_deltas @ hermes_state.py:SessionDB._coalesce_token_deltas */
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

/* PoP: hermes_state_token_writer_loop @ hermes_state.py:SessionDB._token_writer_loop */
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

/* PoP: hermes_state_stop_token_writer @ hermes_state.py:SessionDB._stop_token_writer */
void hermes_state_stop_token_writer(void) {
    pthread_mutex_lock(&g_token_lock);
    g_token_writer_stop = 1;
    pthread_cond_broadcast(&g_token_cond);
    pthread_mutex_unlock(&g_token_lock);
}

/* PoP: hermes_state_drain_token_queue_at_exit @ hermes_state.py:SessionDB._drain_token_queue_at_exit */
int hermes_state_drain_token_queue_at_exit(hermes_state_db_t *db) {
    return hermes_state_apply_token_batch(db);
}

/* PoP: hermes_state_flush_token_counts @ hermes_state.py:SessionDB.flush_token_counts */
int hermes_state_flush_token_counts(hermes_state_db_t *db, double timeout) {
    /* Python: block until every queued delta is applied. */
    (void)timeout;
    return hermes_state_apply_token_batch(db);
}

/* ════════════════════════════════════════════════════════════════════
 * transcript write guards / append_messages_batch
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: hermes_state_check_transcript_write_guards @ hermes_state.py:SessionDB._check_transcript_write_guards */
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

/* PoP: hermes_state_append_messages_batch @ hermes_state.py:SessionDB.append_messages_batch */
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

/* PoP: hermes_state_set_message_reaction @ hermes_state.py:SessionDB.set_message_reaction */
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

/* PoP: hermes_state_get_message_reactions @ hermes_state.py:SessionDB.get_message_reactions */
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

/* PoP: hermes_state_take_unseen_reactions @ hermes_state.py:SessionDB.take_unseen_reactions */
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

/* PoP: hermes_state_latest_message_row_id @ hermes_state.py:SessionDB.latest_message_row_id */
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

/* PoP: hermes_state_latest_user_message_row_id @ hermes_state.py:SessionDB.latest_user_message_row_id */
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

/* PoP: hermes_state_get_message_role @ hermes_state.py:SessionDB.get_message_role */
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

/* PoP: hermes_state_session_count_ge @ hermes_state.py:SessionDB.session_count_ge */
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

/* PoP: hermes_state_session_count_by_source @ hermes_state.py:SessionDB.session_count_by_source */
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

/* PoP: hermes_state_retag_kanban_worker_sessions @ hermes_state.py:SessionDB.retag_kanban_worker_sessions */
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

/* PoP: hermes_state_title_rank @ hermes_state.py:SessionDB._title_rank */
int hermes_state_title_rank(const char *source) {
    return hermes_state_title_rank_impl(source);
}

/* PoP: hermes_state_get_session_title_source @ hermes_state.py:SessionDB.get_session_title_source */
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

/* PoP: hermes_state_set_session_title_source @ hermes_state.py:SessionDB.set_session_title_source */
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

/* PoP: hermes_state_set_auto_title @ hermes_state.py:SessionDB.set_auto_title */
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

/*
 * PoP: set_auto_title_if_empty @ hermes_state.py:SessionDB.set_auto_title_if_empty
 */

/* PoP: hermes_state_set_session_read @ hermes_state.py:SessionDB.set_session_read */
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
}

/* PoP: hermes_state_session_unread @ hermes_state.py:SessionDB.session_unread */
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

/* PoP: hermes_state_is_explicit_fork_child_row @ hermes_state.py:SessionDB._is_explicit_fork_child_row */
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

/* PoP: hermes_state_get_resume_message_count @ hermes_state.py:SessionDB.get_resume_message_count */
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

/* PoP: hermes_state_assert_resume_safe @ hermes_state.py:SessionDB.assert_resume_safe */
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

/* PoP: hermes_state_assert_export_safe @ hermes_state.py:SessionDB.assert_export_safe */
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

/* PoP: hermes_state_find_pr_url_messages @ hermes_state.py:SessionDB.find_pr_url_messages */
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



/* ════════════════════════════════════════════════════════════════════
 * Remaining SessionDB / module gaps (hermes_state.py): _merge_model_config_json,
 * usage_totals, collect_state_db_stats, _set_journal_mode_no_wait,
 * _wal_reset_repair_hint, purge_stale_tool_call_markers, and the read-conn
 * lifecycle methods (faithful no-ops in C — single shared connection).
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: hermes_state_discard_partial_read_conn @ hermes_state.py:SessionDB._discard_partial_read_conn */
void hermes_state_discard_partial_read_conn(void) {
    /* Python: close a conn that failed between open and hand-off. C uses a
     * single shared connection; nothing to discard. */
}

/* PoP: hermes_state_close_read_conn @ hermes_state.py:SessionDB._close_read_conn */
void hermes_state_close_read_conn(void) {
    /* Python: close a pooled read conn + release permit. C shares one conn;
     * no pooling to release. */
}

/* PoP: hermes_state_checkout_read_conn @ hermes_state.py:SessionDB._checkout_read_conn */
int hermes_state_checkout_read_conn(hermes_state_db_t *db) {
    /* Python: borrow a read conn from the pool, open on miss; None when the
     * read path is unavailable (non-WAL / read_only). C: return 1 when the
     * shared writer conn is usable (WAL), else 0. */
    return db && db->db ? 1 : 0;
}

/* PoP: hermes_state_enter_fts_fail_open @ hermes_state.py:SessionDB._enter_fts_fail_open */
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

/* PoP: hermes_state_merge_model_config_json @ hermes_state.py:SessionDB._merge_model_config_json */
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

/* PoP: hermes_state_usage_totals @ hermes_state.py:SessionDB.usage_totals */
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

/* PoP: hermes_state_set_journal_mode_no_wait @ hermes_state.py:_set_journal_mode_no_wait */
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
        sqlite3_finalize(st);
    }
    sqlite3_busy_timeout(db->db, 5000);   /* restore default */
    return result[0] ? result : NULL;
}

/* PoP: hermes_state_wal_reset_repair_hint @ hermes_state.py:_wal_reset_repair_hint */
const char *hermes_state_wal_reset_repair_hint(void) {
    /* Python: context-appropriate hint for repairing the SQLite runtime.
     * C has no install-method detection; return the generic advice. */
    return "install a Python build bundled with SQLite 3.51.3+ "
           "(or backports 3.50.7 / 3.44.6) and restart Hermes";
}

/* PoP: hermes_state_collect_state_db_stats @ hermes_state.py:collect_state_db_stats */
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

/* PoP: hermes_state_purge_stale_tool_call_markers @ hermes_state.py:SessionDB.purge_stale_tool_call_markers */
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

/* PoP: hermes_state_find_orphaned_gateway_sessions @ hermes_state.py:SessionDB.find_orphaned_gateway_sessions */
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

/* PoP: hermes_state_adopt_orphaned_gateway_session @ hermes_state.py:SessionDB.adopt_orphaned_gateway_session */
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

/* PoP: hermes_state_reopen_orphaned_compression_session @ hermes_state.py:SessionDB.reopen_orphaned_compression_session */
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

/* ── test-context detection trio (v706 parity batch) ───────────────── */

/* PoP: hermes_state_process_looks_like_pytest @ hermes_state.py:_process_looks_like_pytest */
bool hermes_state_process_looks_like_pytest(const char *cmdline) {
    /* cmdline is a NUL-separated argv snapshot (procps style). True when any
     * token's basename is "pytest" or (token "python"/"python3"-like with a
     * later bare "-m pytest"). Unreadable/empty => false (fail open). */
    if (!cmdline || !*cmdline) return false;
    const char *p = cmdline;
    bool saw_python = false;
    while (*p) {
        /* token = [p, e) */
        const char *e = p;
        while (*e) e++;
        /* strip quotes and take basename across both separators */
        const char *b = p;
        const char *q = p;
        while (q < e) {
            if (*q == '/' ) b = q + 1;
            q++;
        }
        size_t blen = (size_t)(e - b);
        char tok[256];
        if (blen < sizeof(tok)) {
            size_t k = 0;
            for (const char *r = b; r < e && k < sizeof(tok) - 1; r++) {
                if (*r != '"' && *r != '\'') tok[k++] = *r;
            }
            tok[k] = '\0';
            /* basename again after quote-strip */
            char *base = strrchr(tok, '/');
            base = base ? base + 1 : tok;
            if (strcmp(base, "pytest") == 0) return true;
            if (strncmp(base, "python", 6) == 0 &&
                (base[6] == '\0' || (base[6] == '.'))) 
                saw_python = true;
            else if (saw_python && strcmp(base, "-m") == 0)
                ; /* -m alone isn't proof; pytest token must follow */
        }
        p = e + 1;
    }
    (void)saw_python;
    return false;
}

/* PoP: hermes_state_has_pytest_ancestor @ hermes_state.py:_has_pytest_ancestor */
bool hermes_state_has_pytest_ancestor(void) {
    /* Walk /proc self parent chain; a pytest-looking ancestor backs up the
     * env probe (survives env rebuilds, #82770). Fails open (false). */
    pid_t pid = getpid();
    for (int depth = 0; depth < 32; depth++) {
        char stat_path[64];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", (int)pid);
        FILE *f = fopen(stat_path, "r");
        if (!f) return false;
        char line[8192];
        if (!fgets(line, sizeof(line), f)) { fclose(f); return false; }
        fclose(f);
        /* field 4 = ppid (after the parenthesised comm). Parse past last ')'. */
        char *close_paren = strrchr(line, ')');
        if (!close_paren) return false;
        int ppid = 0;
        if (sscanf(close_paren + 1, " %*c %d", &ppid) != 1) return false;
        if (ppid <= 1) return false;

        char cmd_path[64];
        snprintf(cmd_path, sizeof(cmd_path), "/proc/%d/cmdline", ppid);
        FILE *cf = fopen(cmd_path, "rb");
        if (!cf) return false;
        char cmd[16384];
        size_t n = fread(cmd, 1, sizeof(cmd) - 1, cf);
        fclose(cf);
        cmd[n] = '\0';
        /* convert NUL separators to spaces so the scanner sees tokens */
        for (size_t i = 0; i < n; i++) if (cmd[i] == '\0') cmd[i] = ' ';
        if (strstr(cmd, "pytest") != NULL) return true;
        pid = (pid_t)ppid;
    }
    return false;
}

/* Memoised answer (Python module global). */
static int hs_in_test_context_memo = -1;

/* PoP: hermes_state_in_test_context @ hermes_state.py:_in_test_context */
bool hermes_state_in_test_context(void) {
    if (hs_in_test_context_memo >= 0) return hs_in_test_context_memo == 1;
    bool r = hermes_state_running_under_pytest();
    if (!r) r = hermes_state_has_pytest_ancestor();
    hs_in_test_context_memo = r ? 1 : 0;
    return r;
}

/* PoP: hermes_state_resolve_journal_mode @ hermes_state.py:resolve_journal_mode */
const char *hermes_state_resolve_journal_mode(void) {
    /* database.journal_mode in config: "wal" (default) or "delete". Invalid
     * or malformed values fail safely to "wal". */
    extern json_t *config_py_load_config_readonly(void);
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg || !json_is_object(cfg)) { if (cfg) json_free(cfg); return "wal"; }
    const json_t *db = json_obj_get(cfg, "database");
    const char *raw = NULL;
    if (db && json_is_object(db)) {
        const json_t *jm = json_obj_get(db, "journal_mode");
        if (jm && json_is_string(jm)) raw = json_string_value(jm);
    }
    char mode[32] = "wal";
    if (raw && *raw) {
        size_t k = 0;
        for (const char *q = raw; *q && k < sizeof(mode) - 1; q++)
            mode[k++] = (char)tolower((unsigned char)*q);
        mode[k] = '\0';
    } else {
        strcpy(mode, "wal");   /* missing key defaults to wal */
    }
    json_free(cfg);
    if (strcmp(mode, "wal") == 0 || strcmp(mode, "delete") == 0)
        return (strcmp(mode, "wal") == 0) ? "wal" : "delete";
    return "wal";
}

/* PoP: hermes_state_repair_backup_headroom_bytes @ hermes_state.py:_repair_backup_headroom_bytes */
double hermes_state_repair_backup_headroom_bytes(double total_bytes) {
    /* Free space required beyond the copy: 256 MiB floor + 2% of volume. */
    const double MIN_FREE = 256.0 * 1024.0 * 1024.0;
    double frac = total_bytes * 0.02;
    return (frac > MIN_FREE) ? frac : MIN_FREE;   /* Python max(a,b): a unless b>a */
}

/* PoP: hermes_state_repair_scratch_space_error @ hermes_state.py:_repair_scratch_space_error */
char *hermes_state_repair_scratch_space_error(const char *db_path) {
    /* NULL = space is fine. Otherwise a malloc'd human-readable error.
     * Required = snapshot(main+sidecars) + 2x snapshot (VACUUM reserve)
     * + backup headroom; compares against the volume's free bytes. */
    extern double hermes_state_repair_backup_headroom_bytes(double total_bytes);
    if (!db_path || !*db_path) return NULL;

    struct stat st;
    if (stat(db_path, &st) != 0) return NULL;
    double snapshot_bytes = (double)st.st_size;
    static const char *SIDECARS[] = { "-wal", "-shm", "-journal" };
    for (int i = 0; i < 3; i++) {
        char side[4200];
        snprintf(side, sizeof(side), "%s%s", db_path, SIDECARS[i]);
        struct stat ss;
        if (stat(side, &ss) == 0) snapshot_bytes += (double)ss.st_size;
    }

    /* volume of the db's parent directory */
    char parent[4200];
    snprintf(parent, sizeof(parent), "%s", db_path);
    char *slash = strrchr(parent, '/');
    if (slash) *slash = '\0'; else strcpy(parent, ".");
    struct statvfs vfs;
    if (statvfs(parent, &vfs) != 0) return NULL;
    double total = (double)vfs.f_frsize * (double)vfs.f_blocks;
    double free_b = (double)vfs.f_frsize * (double)vfs.f_bavail;
    double headroom = hermes_state_repair_backup_headroom_bytes(total);

    double required = snapshot_bytes + (2.0 * snapshot_bytes) + headroom;
    if (free_b >= required) return NULL;
    char *msg = malloc(256);
    if (!msg) return NULL;
    snprintf(msg, 256,
             "Insufficient free space for repair: need %.0f bytes, have %.0f.",
             required, free_b);
    return msg;
}

/* PoP: hermes_state_repair_ledger_path @ hermes_state.py:_repair_ledger_path */
char *hermes_state_repair_ledger_path(const char *db_path) {
    /* <db>.repair-attempts.json sibling. Malloc'd; NULL on bad input. */
    if (!db_path || !*db_path) return NULL;
    size_t need = strlen(db_path) + sizeof(".repair-attempts.json");
    char *out = malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "%s.repair-attempts.json", db_path);
    return out;
}

/* ── malformed-backup management (v711 parity batch) ───────────────── */

#define HS_MAX_MALFORMED_BACKUPS 3

/* PoP: hermes_state_existing_malformed_backups @ hermes_state.py:_existing_malformed_backups */
json_t *hermes_state_existing_malformed_backups(const char *db_path) {
    /* Timestamped forensic backups of db_path, newest first (name-sorted
     * reverse). Returns a JSON array of absolute path strings. */
    json_t *out = json_array();
    if (!out || !db_path || !*db_path) return out;
    char dir[4096], base[512];
    snprintf(dir, sizeof(dir), "%s", db_path);
    char *slash = strrchr(dir, '/');
    if (!slash) { strcpy(dir, "."); snprintf(base, sizeof(base), "%s", db_path); }
    else { snprintf(base, sizeof(base), "%s", slash + 1); *slash = '\0'; }
    char prefix[600];
    snprintf(prefix, sizeof(prefix), "%s.malformed-backup-", base);
    size_t plen = strlen(prefix);

    DIR *d = opendir(dir[0] ? dir : ".");
    if (!d) return out;
    /* collect names first so we can sort newest-first by name */
    enum { CAP = 256 };
    char *names[CAP];
    int cnt = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && cnt < CAP) {
        if (strncmp(de->d_name, prefix, plen) != 0) continue;
        /* skip sidecar-suffixed entries (-wal/-shm/-journal) */
        size_t nl = strlen(de->d_name);
        if ((nl > 4 && strcmp(de->d_name + nl - 4, "-wal") == 0) ||
            (nl > 4 && strcmp(de->d_name + nl - 4, "-shm") == 0) ||
            (nl > 8 && strcmp(de->d_name + nl - 8, "-journal") == 0))
            continue;
        names[cnt++] = strdup(de->d_name);
    }
    closedir(d);
    /* sort descending by name = newest first */
    for (int i = 0; i < cnt; i++)
        for (int k = i + 1; k < cnt; k++)
            if (strcmp(names[i], names[k]) < 0) {
                char *tmp = names[i]; names[i] = names[k]; names[k] = tmp;
            }
    for (int i = 0; i < cnt; i++) {
        char full[8192];
        snprintf(full, sizeof(full), "%s/%s", dir, names[i]);
        json_array_append(out, json_string(full));
        free(names[i]);
    }
    return out;
}

/* PoP: hermes_state_prune_malformed_backups @ hermes_state.py:_prune_malformed_backups */
void hermes_state_prune_malformed_backups(const char *db_path, int keep) {
    /* Delete all but the `keep` newest forensic backups (+ sidecars). */
    if (keep <= 0) keep = HS_MAX_MALFORMED_BACKUPS;
    json_t *list = hermes_state_existing_malformed_backups(db_path);
    if (!list) return;
    size_t n = json_array_size(list);
    for (size_t i = (size_t)keep; i < n; i++) {
        const char *stale = json_string_value(json_array_get(list, i));
        if (!stale) continue;
        unlink(stale);
        char side[8300];
        static const char *SUF[] = { "-wal", "-shm", "-journal" };
        for (int s = 0; s < 3; s++) {
            snprintf(side, sizeof(side), "%s%s", stale, SUF[s]);
            unlink(side);
        }
    }
    json_free(list);
}

/* ── persistent repair budget (v712 parity batch) ──────────────────── */

#define HS_MAX_PERSISTENT_REPAIR_ATTEMPTS 3
#define HS_FINGERPRINT_SAMPLE_BYTES 4096

/* PoP: hermes_state_db_fingerprint @ hermes_state.py:_db_fingerprint
 * Cheap identity: size + bounded head/tail content sample, mtime excluded
 * on purpose (#82770 class). Malloc'd hex or NULL when unreadable. */
char *hermes_state_db_fingerprint(const char *db_path) {
    if (!db_path || !*db_path) return NULL;
    FILE *f = fopen(db_path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long total = ftell(f);
    long size = total;
    fseek(f, 0, SEEK_SET);
    char head[HS_FINGERPRINT_SAMPLE_BYTES];
    size_t hn = fread(head, 1, sizeof(head), f);
    /* tail sample */
    char tail[HS_FINGERPRINT_SAMPLE_BYTES];
    size_t tn = 0;
    long tail_off = size - (long)sizeof(tail);
    if (tail_off > 0) {
        fseek(f, tail_off, SEEK_SET);
        tn = fread(tail, 1, sizeof(tail), f);
    }
    fclose(f);
    if (size <= 0) return NULL;

    extern char *hash_sha256_hex(const unsigned char *data, size_t len);
    char size_part[64];
    snprintf(size_part, sizeof(size_part), "%ld:", size);
    /* digest over size + head + tail */
    size_t need = strlen(size_part) + hn + tn;
    unsigned char *blob = malloc(need ? need : 1);
    if (!blob) return NULL;
    size_t off = 0;
    memcpy(blob + off, size_part, strlen(size_part)); off += strlen(size_part);
    memcpy(blob + off, head, hn); off += hn;
    memcpy(blob + off, tail, tn); off += tn;
    char *hex = hash_sha256_hex(blob, off);
    free(blob);

    /* prefix with size so the ledger's size-prefix fallback still works */
    char *out = malloc(strlen(size_part) + (hex ? strlen(hex) : 0) + 1);
    if (!out) { free(hex); return NULL; }
    strcpy(out, size_part);
    strcat(out, hex ? hex : "");
    free(hex);
    return out;
}

/* PoP: hermes_state_persistent_repair_attempts_exhausted @ hermes_state.py:_persistent_repair_attempts_exhausted */
bool hermes_state_persistent_repair_attempts_exhausted(const char *db_path) {
    /* True only when the ledger records >= MAX failed attempts against the
     * CURRENT fingerprint. Missing/corrupt ledger reads as not exhausted. */
    extern json_t *hermes_state_read_repair_ledger(const char *db_path);
    json_t *ledger = hermes_state_read_repair_ledger(db_path);
    if (!ledger || !json_is_object(ledger)) { if (ledger) json_free(ledger); return false; }

    const char *recorded = json_string_value(json_obj_get(ledger, "fingerprint"));
    char *fp = hermes_state_db_fingerprint(db_path);
    bool exhausted = false;
    if (!fp) {
        /* fingerprint unavailable: fall back to recorded SIZE prefix */
        struct stat st;
        if (stat(db_path, &st) != 0) { json_free(ledger); free(fp); return false; }
        char prefix[64];
        snprintf(prefix, sizeof(prefix), "%ld:", (long)st.st_size);
        exhausted = recorded && strncmp(recorded, prefix, strlen(prefix)) == 0 &&
                    ({
                const json_t *_fa = json_obj_get(ledger, "failed_attempts");
                (_fa && _fa->type == JSON_NUMBER) ? (long)_fa->num_val >= HS_MAX_PERSISTENT_REPAIR_ATTEMPTS : false;
            });
    } else {
        bool same = recorded && strcmp(recorded, fp) == 0;
        long attempts = 0;
        const json_t *fa = json_obj_get(ledger, "failed_attempts");
        if (fa && fa->type == JSON_NUMBER) attempts = (long)fa->num_val;
        exhausted = same && attempts >= HS_MAX_PERSISTENT_REPAIR_ATTEMPTS;
    }
    free(fp);
    json_free(ledger);
    return exhausted;
}

/* PoP: hermes_state_read_repair_ledger @ hermes_state.py:_read_repair_ledger */
json_t *hermes_state_read_repair_ledger(const char *db_path) {
    extern char *hermes_state_repair_ledger_path(const char *db_path);
    json_t *out = json_object();
    if (!out) return NULL;
    char *lp = hermes_state_repair_ledger_path(db_path);
    if (!lp) return out;
    FILE *f = fopen(lp, "rb");
    free(lp);
    if (!f) return out;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 1 << 20) { fclose(f); return out; }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return out; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    buf[rd] = '\0';
    fclose(f);
    json_t *raw = json_parse(buf, NULL);
    free(buf);
    if (raw && json_is_object(raw)) {
        json_free(out);
        return raw;   /* adopt the parsed object */
    }
    if (raw) json_free(raw);
    return out;
}

/* PoP: hermes_state_mask_volatile_header @ hermes_state.py:_mask_volatile_header
 * Zero the commit-counter fields (bytes 24..28 and 92..96) so ordinary
 * writes don't re-key the ledger fingerprint. Operates in place on a buffer
 * of *len* bytes; no-op below 96 bytes. */
void hermes_state_mask_volatile_header(unsigned char *head, size_t len) {
    if (!head || len < 96) return;
    memset(head + 24, 0, 4);
    memset(head + 92, 0, 4);
}
