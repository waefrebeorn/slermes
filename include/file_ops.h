/*
 * file_ops.h — File read/write/browse operations for C11 desktop app
 *
 * Provides file system operations needed by the desktop app.
 *
 * PoP: file_read_text    @ electron/main.cjs:readFileText
 * PoP: file_read_dir     @ electron/main.cjs:fs:readDir
 * PoP: file_write        @ electron/main.cjs:writeFile
 * PoP: file_delete       @ electron/main.cjs:deleteFile
 * PoP: dir_create        @ electron/main.cjs:createDir
 * PoP: file_read_data_url @ electron/main.cjs:readFileDataUrl
 */

#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define FILE_OPS_MAX_PATH   4096
#define FILE_OPS_MAX_TEXT   1048576  /* 1MB max file read */
#define FILE_OPS_MAX_DIR    1024     /* max directory entries */
#define FILE_OPS_MAX_NAME   256

/* ── File info ──────────────────────────────────────────────────────────── */
typedef struct {
    char   name[FILE_OPS_MAX_NAME];
    char   path[FILE_OPS_MAX_PATH];
    int64_t size;
    time_t  modified;
    time_t  created;
    bool    is_dir;
    bool    is_file;
    bool    is_symlink;
    bool    is_hidden;
} file_entry_t;

/* ── Directory listing ─────────────────────────────────────────────────── */
typedef struct {
    file_entry_t entries[FILE_OPS_MAX_DIR];
    int          count;
    char         path[FILE_OPS_MAX_PATH];
} dir_listing_t;

/* ── File read ──────────────────────────────────────────────────────────── */

/* PoP: file_read_text @ electron/main.cjs:readFileText */
/* Read entire file as text (UTF-8).
 * Returns allocated string (caller must free), or NULL on error.
 * Sets *out_len to the byte length if out_len is non-NULL. */
char *file_read_text(const char *path, size_t *out_len);

/* PoP: file_read_data_url @ electron/main.cjs:readFileDataUrl */
/* Read file and encode as base64 data URL.
 * Returns allocated string like "data:image/png;base64,...", or NULL on error. */
char *file_read_data_url(const char *path);

/* Read file into a buffer. Returns bytes read, -1 on error. */
int file_read_bytes(const char *path, void *buf, size_t bufsize);

/* ── File write ─────────────────────────────────────────────────────────── */

/* PoP: file_write @ electron/main.cjs:writeFile */
/* Write text to file (UTF-8). Creates parent directories if needed.
 * Returns true on success. */
bool file_write_text(const char *path, const char *text);

/* Write bytes to file. Returns true on success. */
bool file_write_bytes(const char *path, const void *data, size_t len);

/* ── File operations ────────────────────────────────────────────────────── */

/* PoP: file_delete @ electron/main.cjs:deleteFile */
/* Delete a file. Returns true on success. */
bool file_delete(const char *path);

/* Check if a file exists. */
bool file_exists(const char *path);

/* Get file size. Returns -1 on error. */
int64_t file_size(const char *path);

/* ── Directory operations ───────────────────────────────────────────────── */

/* PoP: dir_create @ electron/main.cjs:createDir */
/* Create a directory (and parents if needed). Returns true on success. */
bool dir_create(const char *path);

/* PoP: file_read_dir @ electron/main.cjs:fs:readDir */
/* List directory contents. Returns listing (caller must free with dir_free()). */
dir_listing_t *dir_list(const char *path);

/* Free a directory listing. */
void dir_free(dir_listing_t *listing);

/* Check if a directory exists. */
bool dir_exists(const char *path);

/* ── Path utilities ─────────────────────────────────────────────────────── */

/* Get the file name from a path. Returns pointer into path (not allocated). */
const char *file_basename(const char *path);

/* Get the directory part of a path. Returns pointer into path (not allocated). */
const char *file_dirname(const char *path);

/* Join two path components. Returns allocated string. */
char *file_join(const char *dir, const char *name);

/* Get the home directory. Returns allocated string. */
char *file_home_dir(void);

/* Get the current working directory. Returns allocated string. */
char *file_cwd(void);

#ifdef __cplusplus
}
#endif

#endif /* FILE_OPS_H */
