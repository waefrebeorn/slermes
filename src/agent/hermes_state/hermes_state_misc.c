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
