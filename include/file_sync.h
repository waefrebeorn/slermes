/*
 * file_sync.h — Environment file sync for remote execution backends
 *
 * Port of Python tools/environments/file_sync.py.
 * Collects credential, skills, and cache files for upload to remote execution
 * environments (Modal, SSH, Daytona), and provides a real FileSyncManager
 * that pulls remote changes back (sync_back: download tar -> SHA-256 diff ->
 * apply only changed, non-upload-only files).
 *
 * Opaque struct discipline: callers never see the manager's internal state
 * (pushed-hash map, upload-only set). Minimal includes (C11 + libc).
 */

#ifndef FILE_SYNC_H
#define FILE_SYNC_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_SYNC_PATH_MAX 1024
#define FILE_SYNC_MAX_BYTES (2L * 1024 * 1024 * 1024) /* 2 GiB tar cap */

typedef struct {
    char host_path[FILE_SYNC_PATH_MAX];   /* Absolute path on host */
    char remote_path[FILE_SYNC_PATH_MAX]; /* Absolute path in container */
} file_sync_entry_t;

typedef struct {
    file_sync_entry_t *entries;
    int count;
    int capacity;
} file_sync_list_t;

/* Callback types for remote operations */
typedef bool (*file_sync_upload_fn)(const char *host_path,
                                     const char *remote_path,
                                     void *ctx);
typedef bool (*file_sync_bulk_upload_fn)(file_sync_list_t *files, void *ctx);
typedef bool (*file_sync_delete_fn)(const char *remote_path, void *ctx);

/* Enumerate current files to sync (mirrors Python _get_files_fn).
 * Returns a malloc'd file_sync_list_t (caller frees via file_sync_list_free),
 * or NULL on failure. */
typedef file_sync_list_t *(*file_sync_get_files_fn)(void *ctx);

/* Bulk-download callback: write a tar archive of the remote .hermes/ dir
 * into local_tar_path. Returns true on success. */
typedef bool (*file_sync_bulk_download_fn)(const char *local_tar_path,
                                            void *ctx);

/* Opaque sync manager (mirrors Python FileSyncManager). */
typedef struct file_sync_manager file_sync_manager_t;

/* ── Core API ───────────────────────────────────────────────────── */

/* Enumerate all files to sync: credentials + skills + cache.
 * container_base is the remote base path (e.g. /root/.hermes).
 * Returns NULL on allocation failure. Caller must file_sync_list_free(). */
file_sync_list_t *file_sync_collect(const char *container_base);

/* Build parent directories command for remote shell execution.
 * Returns malloc'd string with "mkdir -p <dir1> <dir2> ...". */
char *file_sync_mkdir_cmd(file_sync_list_t *files);

/* Build deletion command for stale remote files.
 * files is the CURRENT state; existing_remote is a space-separated list.
 * Returns malloc'd "rm -f <stale1> <stale2> ...". */
char *file_sync_rm_stale_cmd(file_sync_list_t *files, file_sync_list_t *existing);

/* Sync all files via upload callback. Returns true on success. */
bool file_sync_upload_all(file_sync_list_t *files,
                           file_sync_upload_fn upload, void *upload_ctx);

/* Free a file_sync_list_t (including entries). */
void file_sync_list_free(file_sync_list_t *list);

/* Set HERMES_HOME path (default: ~/.hermes). Must be called before collect. */
void file_sync_set_home(const char *home);

/* ── Manager ────────────────────────────────────────────────────── */

/* Create a manager that owns a copy of `files` (caller keeps its own list).
 * download_fn/download_ctx pull the remote tar on sync_back.
 * upload_fn/bulk_upload_fn/delete_fn/get_files_fn implement the upload cycle
 * (mirrors Python FileSyncManager's injected callbacks). May be NULL. */
file_sync_manager_t *file_sync_manager_create_ex(
    const file_sync_list_t *files,
    file_sync_bulk_download_fn download_fn, void *download_ctx,
    file_sync_bulk_upload_fn bulk_upload_fn, void *bulk_upload_ctx,
    file_sync_upload_fn upload_fn, void *upload_ctx,
    file_sync_delete_fn delete_fn, void *delete_ctx,
    file_sync_get_files_fn get_files_fn, void *get_files_ctx);

#define file_sync_manager_create(files, dl, dlc) \
    file_sync_manager_create_ex((files), (dl), (dlc), NULL, NULL, \
                                NULL, NULL, NULL, NULL, NULL, NULL)

void file_sync_manager_free(file_sync_manager_t *m);

/* Set the minimum interval (seconds) between non-forced sync cycles
 * (mirrors Python _sync_interval). */
void file_sync_manager_set_interval(file_sync_manager_t *m, double seconds);

/* Record a remote path as pushed, storing its SHA-256 hex digest.
 * Mirrors Python self._pushed_hashes[remote_path] = sha256(host_path). */
void file_sync_manager_mark_pushed(file_sync_manager_t *m,
                                    const char *remote_path,
                                    const char *host_path);

/* Add a host path to the upload-only (credential) set.
 * Mirrors Python self._upload_only_host_paths. */
void file_sync_manager_add_upload_only(file_sync_manager_t *m,
                                        const char *host_path);

/* Pull remote changes back to the host (mirrors FileSyncManager.sync_back).
 * Returns 0 on success, -1 on error. */
int file_sync_manager_sync_back(file_sync_manager_t *m,
                                 const char *hermes_home);

/* Teardown pull for one sync cycle (mirrors _sync_back_transaction — thin
 * wrapper over sync_back). Returns 0 on success, -1 on error. */
int file_sync_manager_sync_back_transaction(file_sync_manager_t *m,
                                             const char *hermes_home);

/* Execute one upload sync cycle (mirrors _sync_transaction).
 * force=1 skips the rate-limit guard. Returns 0 if cycled, -1 on error.
 * Requires upload_fn (or bulk_upload_fn) + get_files_fn + delete_fn to be set. */
int file_sync_manager_sync_transaction(file_sync_manager_t *m, int force);

#ifdef __cplusplus
}
#endif

#endif /* FILE_SYNC_H */
