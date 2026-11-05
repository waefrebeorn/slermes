#ifndef PORT_CRON_NOTEPAD_H
#define PORT_CRON_NOTEPAD_H

#include "hermes_json.h"
#include "sqlite3.h"

/* Port of Python: cron/notepad.py
 *
 * Per-job durable KV notepad (cursors, watermarks) in a profile-local SQLite
 * file next to the executions ledger. Caps are a documented contract:
 * 16 KB per value (UTF-8), 64 KB per job (key+value), 128-char keys.
 * Oversized writes raise and leave the store untouched — the notepad is
 * prompt-injected each run.
 */

#define CN_MAX_VALUE_BYTES     (16 * 1024)
#define CN_MAX_KEY_CHARS       128
#define CN_MAX_JOB_TOTAL_BYTES (64 * 1024)

/* <home>/cron/notepad.db, or `override_path` when given. Malloc'd. */
char *cn_notepad_file(const char *home, const char *override_path);

/* CREATE TABLE IF NOT EXISTS ...; returns 1 on success. */
int cn_initialize_schema(sqlite3 *db);

/* Upsert one key. Returns the stored record, or NULL with *err set to the
 * exact Python ValueError text. Caller frees both. */
json_t *cn_set_note(const char *db_path, const char *job_id, const char *key,
                    const char *value, char **err);

/* Stored value, or NULL when the key is absent. */
char *cn_get_note(const char *db_path, const char *job_id, const char *key);

/* 1 when a row was deleted. */
int cn_delete_note(const char *db_path, const char *job_id, const char *key);

/* All entries for one job, sorted by key: [{job_id,key,value,updated_at}]. */
json_t *cn_list_notes(const char *db_path, const char *job_id);

/* Delete every key for one job; returns the row count (0 without a DB file). */
int cn_clear_notepad(const char *db_path, const char *job_id);

/* The prompt section; "" when the notepad is empty (byte-identical prompt). */
char *cn_render_notepad_section(const char *db_path, const char *job_id);

#endif /* PORT_CRON_NOTEPAD_H */
