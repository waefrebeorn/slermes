/* session_recovery_internal.h — shared internals for the session_recovery
 * port units (paths/copy/meta/verify/main). Faithful C11 port of
 * hermes_cli/session_recovery.py; each unit maps a cohesive slice of the
 * Python module. NOT a public header — include/session_recovery.h is.
 */
#ifndef SESSION_RECOVERY_INTERNAL_H
#define SESSION_RECOVERY_INTERNAL_H

#include "session_recovery.h"
#include "sqlite3.h"

/* Python module constants */
#define SR_MIN_SPACE_HEADROOM (256LL * 1024 * 1024)
#define SR_MAX_SALVAGE_RANGE_QUERIES 10000
#define SR_SCHEMA_VERSION 23
#define SR_FTS_STORAGE_VERSION 1

extern const char *SR_CANONICAL_TABLES[6];
extern const char *SR_TOPIC_TABLES[2];
extern const char *SR_GENERATED_META_KEYS[8]; /* sorted */
extern const char *SR_SIDECAR_SUFFIXES[4];    /* "", -wal, -shm, -journal */

/* paths unit */
char *sr_sidecar_path(const char *db_path, const char *suffix);   /* malloc */
char *sr_resolved_output_path(const char *path, char *err, size_t elen);
int   sr_validate_paths(const char *source_path, const char *output_path,
                        const char *work_dir, char **source_out,
                        char **output_out, char **work_root_out,
                        char *err, size_t elen); /* 0 ok, else safety error */
json_t *sr_source_fingerprint(const char *source);
bool  sr_same_filesystem(const char *left, const char *right);
json_t *sr_disk_space_preflight(const char *source, const char *work_root,
                                const char *output_parent,
                                char *err, size_t elen); /* NULL on error */
int   sr_copy_source_bundle(const char *source, const char *snapshot_dir,
                            char **snapshot_source_out, json_t **copied_out,
                            char *err, size_t elen);

/* copy unit */
json_t *sr_table_columns(sqlite3 *conn, const char *table); /* json array */
json_t *sr_table_inventory(sqlite3 *conn, const char *table);
json_t *sr_inspect_connection(sqlite3 *conn);
json_t *sr_copy_table(sqlite3 *src, sqlite3 *dst, const char *table,
                      int chunk_size, session_recovery_progress_cb cb,
                      void *ud, const json_t *source_rows);
json_t *sr_copy_table_salvage(sqlite3 *src, sqlite3 *dst, const char *table,
                              int chunk_size, session_recovery_progress_cb cb,
                              void *ud, const json_t *source_rows,
                              const char *insert_prefix,
                              bool filter_user_meta);

/* meta unit */
json_t *sr_copy_state_meta(sqlite3 *src, sqlite3 *dst, int chunk_size,
                           session_recovery_progress_cb cb, void *ud,
                           const json_t *source_rows);
json_t *sr_copy_state_meta_salvage(sqlite3 *src, sqlite3 *dst, int chunk_size,
                                   session_recovery_progress_cb cb, void *ud,
                                   const json_t *source_rows);
json_t *sr_reconstruct_missing_sessions(sqlite3 *dst);
json_t *sr_cleanup_partial_orphans(sqlite3 *dst);

/* verify unit */
json_t *sr_verify_recovered_database(const char *output,
                                     const json_t *expected_counts,
                                     const json_t *copy_report,
                                     bool allow_partial,
                                     const json_t *orphan_cleanup);
json_t *sr_finalize_derived_metadata(sqlite3 *dst);

/* helpers */
bool sr_is_generated_meta_key(const char *key);
bool sr_columns_contains(const json_t *cols, const char *name);
void sr_set_err(char *err, size_t elen, const char *fmt, ...);
long long sr_file_size(const char *path);
bool sr_path_lexists(const char *path);

#endif /* SESSION_RECOVERY_INTERNAL_H */
