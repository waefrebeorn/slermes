/*
 * web_server_fs.h — /api/fs endpoint cluster of the Hermes dashboard
 * (faithful C11 port of hermes_cli/web_server.py fs_list / fs_read_text /
 * fs_write_text / fs_read_data_url / fs_git_root / fs_default_cwd).
 *
 * Reuses hermes_web_server_pure.h (ws_fs_path, ws_fs_regular_file,
 * ws_fs_mime_type, ws_fs_looks_binary, ws_fs_find_git_root).
 */

#ifndef WEB_SERVER_FS_H
#define WEB_SERVER_FS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Size caps (mirror web_server.py constants). */
#define WS_FS_DATA_URL_MAX_BYTES     (16u * 1024u * 1024u)
#define WS_FS_TEXT_SOURCE_MAX_BYTES  (64u * 1024u * 1024u)
#define WS_FS_TEXT_PREVIEW_MAX_BYTES (512u * 1024u)
#define WS_FS_TEXT_WRITE_MAX_BYTES   (8u * 1024u * 1024u)

/* Python _FS_READDIR_HIDDEN: true if a directory entry name must be hidden
 * from /api/fs/list (VCS dirs, caches, build outputs). */
bool ws_fs_readdir_hidden(const char *name);

/* Python _FS_PREVIEW_LANGUAGE_BY_EXT lookup: returns the editor language for
 * a file path's extension, or "text" when unknown. Static string, no free. */
const char *ws_fs_preview_language(const char *path);

/* One /api/fs/list entry. */
typedef struct {
    char name[256];
    char path[2048];
    bool is_directory;
} ws_fs_entry_t;

/* Python fs_list: scandir, filter hidden, sort (dirs first, name lower, name).
 * Returns malloc'd array (caller frees) with *out_n entries; on OS error
 * returns NULL and sets *out_err to the errno-style token Python returns
 * ("ENOENT" / "ENOTDIR" / "EACCES" / strerror). *out_err is a static or
 * malloc'd string — check *out_err_malloced. */
ws_fs_entry_t *ws_fs_list(const char *path, size_t *out_n,
                          const char **out_err, bool *out_err_malloced);

/* Python fs_read_text result. */
typedef struct {
    int status;          /* 0 ok, else HTTP-style error (403/404/400/413) */
    bool binary;         /* _fs_looks_binary(data[:4096]) */
    long long byte_size; /* full file size */
    char language[16];   /* preview language */
    const char *mime;    /* static string */
    bool truncated;      /* size > preview cap */
    char *text;          /* malloc'd UTF-8 (errors replaced); caller frees */
    size_t text_len;
} ws_fs_read_text_t;

/* Python fs_read_text: stat+validate regular file, cap checks, read up to
 * preview cap, binary sniff on first 4096, UTF-8 with U+FFFD replacement. */
void ws_fs_read_text(const char *path, ws_fs_read_text_t *out);

/* Python fs_write_text: size cap, only regular files replaced, parent must
 * exist, staged tmp sibling + rename (atomic). Returns 0 ok else HTTP-style
 * error code (413/400/403/500). */
int ws_fs_write_text(const char *path, const char *content, size_t content_len);

/* Python fs_read_data_url: cap 16MB, base64 the bytes, "data:<mime>;base64,…".
 * Returns malloc'd data URL or NULL with *out_status set (403/404/400/413). */
char *ws_fs_read_data_url(const char *path, int *out_status);

/* Python _fs_default_cwd: config terminal.cwd (or TERMINAL_CWD env), must be
 * an existing dir and not "."/"auto"/"cwd"; else getcwd. Fills out. */
void ws_fs_default_cwd(char *out, size_t cap);

/* Python _fs_git_branch: `git -C cwd branch --show-current` with 2s timeout;
 * empty string on any failure. Fills out. */
void ws_fs_git_branch(const char *cwd, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_FS_H */
