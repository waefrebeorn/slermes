#ifndef LIBDB_H
#define LIBDB_H

/*
 * libdb.h — File-based JSON session store for C.
 * Zero external deps. Each session is a .json file on disk
 * with an optional .meta.json sidecar for metadata.
 * Replaces Python's SQLite-based session store + hermes_state.
 *
 * MIT License — WuBu Hermes Project
 *
 * Usage:
 *   db_t *db = db_open("/home/user/.hermes/sessions", NULL);
 *   db_save(db, "session_123", "{\"messages\":[...]}");
 *   char *data = db_load(db, "session_123", NULL);
 *   db_delete(db, "session_123");
 *   db_close(db);
 */

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque database handle */
typedef struct db_t db_t;

/* ================================================================
 *  P141: Session metadata structure
 * ================================================================ */
#define SESSION_SCHEMA_VERSION 3
#define SESSION_TAGS_MAX 32
#define SESSION_TAG_LEN 64

typedef struct {
    char     title[256];          /* session title */
    char     model[128];          /* model used */
    int      schema_version;      /* P150: schema version for migration */
    int      token_count;         /* total tokens used */
    int      input_tokens;        /* total input tokens */
    int      output_tokens;       /* total output tokens */
    int      cache_read_tokens;   /* total cache read tokens */
    int      cache_write_tokens;  /* total cache write tokens */
    int      tool_call_count;     /* total tool calls */
    int      reasoning_tokens;    /* G04: reasoning-only tokens */
    double   estimated_cost;      /* G07: estimated USD cost */
    char     source[32];          /* source platform (cli, telegram, etc.) */
    int      message_count;       /* total messages */
    time_t   created_at;          /* creation timestamp */
    time_t   updated_at;          /* last update timestamp */
    /* P146: Tags */
    char     tags[SESSION_TAGS_MAX][SESSION_TAG_LEN];
    int      tag_count;
    /* P149: Branch parent info */
    char     parent_id[64];       /* empty if root session */
    int      branch_point;        /* message index where branch happened, -1 if root */
    /* SE01: Session lifecycle tracking */
    time_t   ended_at;            /* 0 if session is still active */
    char     end_reason[64];      /* reason for ending: "user_exit", "compression", etc. */
    /* SE09: Meta key-value store — arbitrary JSON key-value pairs */
    char     meta_json[4096];     /* JSON object string: {"key": "value", ...} */
} session_meta_t;

/* === Core operations === */

/* Open/create database in directory. Returns NULL on error. */
db_t *db_open(const char *dir_path, char **error_msg);

/* Close database and flush all pending writes. */
void db_close(db_t *db);

/* Save a session by ID. Overwrites existing. */
bool db_save(db_t *db, const char *session_id, const char *json_data);

/* Load a session by ID. Returns malloc'd string, caller must free. */
char *db_load(const db_t *db, const char *session_id, char **error_msg);

/* Delete a session. Returns true if existed. */
bool db_delete(db_t *db, const char *session_id);

/* Check if session exists. */
bool db_exists(const db_t *db, const char *session_id);

/* === P141: Metadata operations === */

/* Save metadata alongside session data. Sidecar .meta.json file. */
bool db_save_meta(db_t *db, const char *session_id, const session_meta_t *meta);

/* Load metadata for a session. Returns true if meta file exists. */
bool db_load_meta(const db_t *db, const char *session_id, session_meta_t *meta);

/* Initialize metadata with defaults (title, model, timestamps). */
void db_meta_init(session_meta_t *meta);

/* SE01: Mark a session ended (first writer wins — no-op if already ended).
 * Persists ended_at + end_reason into the session sidecar meta. Port of
 * SessionDB.end_session. */
bool db_end_session(db_t *db, const char *session_id, const char *end_reason);

/* === L19: Tag CRUD operations === */

/* Add a tag to a session. Returns true if added. Max SESSION_TAGS_MAX per session. */
bool db_tag_add(db_t *db, const char *session_id, const char *tag);

/* Remove a tag from a session. Returns true if tag existed. */
bool db_tag_remove(db_t *db, const char *session_id, const char *tag);

/* List all tags for a session. Returns malloc'd array of strings, caller must free. */
char **db_tag_list(const db_t *db, const char *session_id, int *count);

/* Find sessions by tag. Returns NULL-terminated array of session IDs, caller must free. */
char **db_tag_find(const db_t *db, const char *tag, size_t *count);

/* === Listing === */

/* List all session IDs. Returns NULL-terminated array. Caller free each + array. */
char **db_list(const db_t *db, size_t *count);

/* Get total number of sessions. */
size_t db_count(const db_t *db);

/* === P143: List with metadata === */
typedef struct {
    char id[64];
    session_meta_t meta;
} db_session_entry_t;

/* Canonical session list entry (id + metadata). Owned by libdb; the
 * app-state subsystem uses its own app_session_entry_t to avoid a
 * name collision. */
typedef struct {
    char           id[64];
    session_meta_t meta;
} session_entry_t;

/* List sessions with metadata. Returns malloc'd array. Caller must free each + array. */
db_session_entry_t *db_list_with_meta(const db_t *db, size_t *count);

/* === P145: Prune === */

/* Remove sessions older than retention_days. Returns number removed. */
int db_prune_by_age(db_t *db, int retention_days);

/* === P148: Export === */

/* Export session as JSON string (full data + metadata merged). Caller must free. */
char *db_export_json(db_t *db, const char *session_id);

/* Export session as Markdown string. Caller must free. */
char *db_export_markdown(db_t *db, const char *session_id);

/* === P149: Branch === */

/* Branch a session: copy messages [0..branch_point] into new session_id.
 * Returns NULL on error or malloc'd string with new session data. */
char *db_branch(db_t *db, const char *source_id, const char *new_id, int branch_point);

/* === P150: Migration === */

/* Check and upgrade schema version for all sessions. Returns number migrated. */
int migrate(db_t *db);


/* === P151: Message-level queries === */

/* Tool call statistics across sessions. */
typedef struct {
    char name[64];
    int  count;
} db_tool_stat_t;

/* Query tool call statistics. Returns NULL-terminated pair array. Caller must free. */
db_tool_stat_t *db_query_tool_stats(const db_t *db, int days_only, const char *source_filter);
/* === MS03: Compression lock === */

/* Try to acquire a compression lock for a session.
 * Uses flock() on a per-session lock file for atomicity.
 * Returns true if lock acquired, false if already held by another process. */
bool db_try_acquire_compression_lock(db_t *db, const char *session_id,
                                      const char *holder_id);

/* Release a compression lock. Returns true on success. */
bool db_release_compression_lock(db_t *db, const char *session_id,
                                  const char *holder_id);

/* Get the current lock holder for a session.
 * Returns malloc'd string with holder ID, or NULL if no lock. Caller must free. */
char *db_get_compression_lock_holder(db_t *db, const char *session_id);

/* === Maintenance === */

/* Get storage size in bytes. */
long long db_storage_size(const db_t *db);

/* Remove all sessions. */
bool db_clear(db_t *db);

/* Flush pending writes to disk. */
bool db_flush(db_t *db);

#ifdef __cplusplus
}
#endif

#endif /* LIBDB_H */
