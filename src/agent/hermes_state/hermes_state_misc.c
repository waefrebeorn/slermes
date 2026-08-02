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

/* PoP: hermes_state_set_auto_title_if_empty @ hermes_state.py:set_auto_title_if_empty */
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

/* PoP: hermes_state_reasoning_json_value @ hermes_state.py:_reasoning_json_value */
char *hermes_state_reasoning_json_value(const char *value) {
    /* Non-string -> value as-is; string that parses as JSON -> the parsed
     * JSON re-serialized (normalized); unparseable string -> value as-is. */
    if (!value) return NULL;
    json_t *parsed = json_parse(value, NULL);
    if (parsed) {
        char *ser = json_serialize(parsed);
        json_free(parsed);
        return ser;
    }
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
