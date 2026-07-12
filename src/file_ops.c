/*
 * file_ops.c — File read/write/browse operations for C11 desktop app
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

#include "file_ops.h"
#include "hermes_core_types.h"
#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <pwd.h>

/* ── Internal helpers ────────────────────────────────────────────────────── */

static void file_entry_init(file_entry_t *e, const char *name, const char *path,
                             struct stat *st) {
    strncpy(e->name, name, FILE_OPS_MAX_NAME - 1);
    strncpy(e->path, path, FILE_OPS_MAX_PATH - 1);
    e->size     = (int64_t)st->st_size;
    e->modified = st->st_mtime;
    e->created  = st->st_ctime;
    e->is_dir   = S_ISDIR(st->st_mode);
    e->is_file  = S_ISREG(st->st_mode);
    e->is_symlink = S_ISLNK(st->st_mode);
    e->is_hidden  = (name[0] == '.');
}

/* ── File read ──────────────────────────────────────────────────────────── */

/* PoP: file_read_text @ electron/main.cjs:readFileText */
char *file_read_text(const char *path, size_t *out_len) {
    if (!path) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        hermes_log("file_read_text: cannot open '%s': %s", path, strerror(errno));
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize < 0 || fsize > (long)FILE_OPS_MAX_TEXT) {
        fclose(fp);
        fprintf(stderr, "file_read_text: file too large or error: %ld", fsize);
        return NULL;
    }

    char *buf = malloc((size_t)fsize + 1);
    if (!buf) { fclose(fp); return NULL; }

    size_t nread = fread(buf, 1, (size_t)fsize, fp);
    buf[nread] = '\0';
    fclose(fp);

    if (out_len) *out_len = nread;
    return buf;
}

/* PoP: file_read_data_url @ electron/main.cjs:readFileDataUrl */
char *file_read_data_url(const char *path) {
    if (!path) return NULL;

    /* Determine MIME type from extension */
    const char *ext = strrchr(path, '.');
    const char *mime = "application/octet-stream";
    if (ext) {
        if (strcasecmp(ext, ".png") == 0) mime = "image/png";
        else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) mime = "image/jpeg";
        else if (strcasecmp(ext, ".gif") == 0) mime = "image/gif";
        else if (strcasecmp(ext, ".svg") == 0) mime = "image/svg+xml";
        else if (strcasecmp(ext, ".webp") == 0) mime = "image/webp";
        else if (strcasecmp(ext, ".pdf") == 0) mime = "application/pdf";
        else if (strcasecmp(ext, ".txt") == 0) mime = "text/plain";
        else if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) mime = "text/html";
        else if (strcasecmp(ext, ".json") == 0) mime = "application/json";
        else if (strcasecmp(ext, ".md") == 0) mime = "text/markdown";
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > (long)FILE_OPS_MAX_TEXT) { fclose(fp); return NULL; }

    unsigned char *raw = malloc((size_t)fsize);
    if (!raw) { fclose(fp); return NULL; }
    size_t nread = fread(raw, 1, (size_t)fsize, fp);
    fclose(fp);

    /* Base64 encode */
    char *b64 = base64_encode(raw, nread);
    free(raw);

    if (!b64) return NULL;

    /* Build data URL */
    size_t url_len = strlen(mime) + 20 + strlen(b64);
    char *url = malloc(url_len);
    if (url) snprintf(url, url_len, "data:%s;base64,%s", mime, b64);
    free(b64);

    return url;
}

int file_read_bytes(const char *path, void *buf, size_t bufsize) {
    if (!path || !buf) return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    size_t nread = fread(buf, 1, bufsize, fp);
    fclose(fp);
    return (int)nread;
}

/* ── File write ─────────────────────────────────────────────────────────── */

/* PoP: file_write @ electron/main.cjs:writeFile */
bool file_write_text(const char *path, const char *text) {
    if (!path || !text) return false;

    /* Create parent directories */
    char *dir = strdup(path);
    char *dname = dirname(dir);
    if (dname && strcmp(dname, ".") != 0 && strcmp(dname, "/") != 0) {
        dir_create(dname);
    }
    free(dir);

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        hermes_log("file_write_text: cannot open '%s': %s", path, strerror(errno));
        return false;
    }

    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, fp);
    fclose(fp);

    return written == len;
}

bool file_write_bytes(const char *path, const void *data, size_t len) {
    if (!path || !data) return false;

    FILE *fp = fopen(path, "wb");
    if (!fp) return false;

    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);
    return written == len;
}

/* ── File operations ────────────────────────────────────────────────────── */

/* PoP: file_delete @ electron/main.cjs:deleteFile */
bool file_delete(const char *path) {
    if (!path) return false;
    return remove(path) == 0;
}

bool file_exists(const char *path) {
    if (!path) return false;
    return access(path, F_OK) == 0;
}

int64_t file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

/* ── Directory operations ───────────────────────────────────────────────── */

/* PoP: dir_create @ electron/main.cjs:createDir */
bool dir_create(const char *path) {
    if (!path) return false;

    /* Create parent directories recursively */
    char *p = strdup(path);
    if (!p) return false;

    size_t len = strlen(p);
    for (size_t i = 1; i < len; i++) {
        if (p[i] == '/') {
            p[i] = '\0';
            mkdir(p, 0755);
            p[i] = '/';
        }
    }
    int ret = mkdir(p, 0755);
    free(p);

    /* EEXIST is OK (directory already exists) */
    return ret == 0 || errno == EEXIST;
}

/* PoP: file_read_dir @ electron/main.cjs:fs:readDir */
dir_listing_t *dir_list(const char *path) {
    if (!path) return NULL;

    dir_listing_t *listing = calloc(1, sizeof(dir_listing_t));
    if (!listing) return NULL;

    strncpy(listing->path, path, FILE_OPS_MAX_PATH - 1);

    DIR *dir = opendir(path);
    if (!dir) {
        hermes_log("dir_list: cannot open '%s': %s", path, strerror(errno));
        free(listing);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && listing->count < FILE_OPS_MAX_DIR) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[FILE_OPS_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) != 0) continue;

        file_entry_init(&listing->entries[listing->count],
                        entry->d_name, full_path, &st);
        listing->count++;
    }

    closedir(dir);
    return listing;
}

void dir_free(dir_listing_t *listing) {
    free(listing);
}

bool dir_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* ── Path utilities ─────────────────────────────────────────────────────── */

const char *file_basename(const char *path) {
    if (!path) return "";
    const char *last = strrchr(path, '/');
    return last ? last + 1 : path;
}

const char *file_dirname(const char *path) {
    if (!path) return ".";
    static char buf[FILE_OPS_MAX_PATH];
    strncpy(buf, path, FILE_OPS_MAX_PATH - 1);
    buf[FILE_OPS_MAX_PATH - 1] = '\0';
    char *d = dirname(buf);
    return d ? d : ".";
}

char *file_join(const char *dir, const char *name) {
    if (!dir || !name) return NULL;
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    size_t total = dlen + nlen + 2;
    char *result = malloc(total);
    if (!result) return NULL;

    if (dir[dlen - 1] == '/')
        snprintf(result, total, "%s%s", dir, name);
    else
        snprintf(result, total, "%s/%s", dir, name);
    return result;
}

char *file_home_dir(void) {
    const char *home = getenv("HOME");
    if (home) return strdup(home);

    struct passwd *pw = getpwuid(getuid());
    if (pw) return strdup(pw->pw_dir);

    return strdup("/tmp");
}

char *file_cwd(void) {
    char buf[FILE_OPS_MAX_PATH];
    if (getcwd(buf, sizeof(buf)))
        return strdup(buf);
    return strdup(".");
}
