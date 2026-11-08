/*
 * file_batch_fs.c - file-batch filesystem operation wrappers.
 *
 * Split from file_batch.c (soul.md: self-contained modules). Thin
 * wrappers over access / stat / copy / move / delete / chmod. No
 * file-scope state.
 */
#include "file_batch_fs.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include "hash.h"



/* Check if we have the required permissions for a file operation.
   Returns NULL if OK, or error string if permission denied. */ const char *check_file_access(const char *path, bool need_write) {
    if (!path) return NULL;
    int amode = need_write ? R_OK | W_OK : R_OK;
    if (access(path, amode) != 0) {
        if (errno == EACCES) return "Permission denied";
        if (errno == ENOENT) return NULL;  /* non-existent path -> handled by caller */
        return NULL;
    }
    return NULL;  /* OK */
}


/* Check parent directory write permission for a given path.
   Returns NULL if OK, or error string if no write access. */ const char *check_parent_writable(const char *path) {
    if (!path) return NULL;
    char parent[4096];
    snprintf(parent, sizeof(parent), "%s", path);
    char *slash = strrchr(parent, '/');
    if (!slash) return NULL;  /* no parent dir to check */
    *slash = '\0';
    if (*parent == '\0') { snprintf(parent, sizeof(parent), "/"); }
    struct stat st;
    if (stat(parent, &st) != 0) return NULL;  /* parent might not exist yet */
    if (access(parent, W_OK) != 0) return "Parent directory not writable";
    return NULL;
}
 bool copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;

    /* Ensure parent dir exists */
    char dstdir[4096];
    snprintf(dstdir, sizeof(dstdir), "%s", dst);
    char *slash = strrchr(dstdir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dstdir, 0755);
    }

    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return false; }

    char buf[COPY_BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in); fclose(out); unlink(dst);
            return false;
        }
    }

    fclose(in);
    fclose(out);
    return true;
}


/* Port of Python tools/file_operations.py:move_file(). */ bool move_file(const char *src, const char *dst) {
    if (rename(src, dst) == 0) return true;
    /* Cross-filesystem: copy + unlink */
    if (!copy_file(src, dst)) return false;
    unlink(src);
    return true;
}


/* Port of Python tools/file_operations.py:delete_file(). */ bool delete_file(const char *path) {
    return unlink(path) == 0;
}


/* Parse octal mode string (e.g. "755", "0644") to mode_t.
 * Returns (mode_t)-1 on parse failure. */ mode_t parse_mode_string(const char *mode_str) {
    if (!mode_str || !*mode_str) return (mode_t)-1;
    char *end = NULL;
    long val = strtol(mode_str, &end, 8);
    if (*end != '\0' || val < 0 || val > 07777) return (mode_t)-1;
    return (mode_t)val;
}
 bool chmod_file(const char *path, mode_t mode) {
    return chmod(path, mode) == 0;
}
 bool touch_file(const char *path) {
    /* Update timestamps. If file doesn't exist, create it. */
    if (utimensat(AT_FDCWD, path, NULL, 0) == 0)
        return true;
    /* Create empty file */
    FILE *f = fopen(path, "a");
    if (!f) return false;
    fclose(f);
    return true;
}


/* Stat a file and return JSON object with size, mode, mtime. */ json_t *stat_file_json(const char *path) {

    struct stat st;
    if (stat(path, &st) != 0) return NULL;

    json_t *j = json_object();
    json_set(j, "size", json_number((double)st.st_size));
    char mode_str[16];
    snprintf(mode_str, sizeof(mode_str), "%o", (unsigned)(st.st_mode & 07777));
    json_set(j, "mode", json_string(mode_str));
    json_set(j, "mtime", json_number((double)st.st_mtime));
    json_set(j, "is_dir", json_bool(S_ISDIR(st.st_mode) ? 1 : 0));
    return j;
}


/* Hash a file and return JSON with sha256 hex. */ json_t *hash_file_json(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    /* Hash via libhash */
    size_t n;
    /* Read whole file into memory. 100MB cap for safety. */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize < 0 || fsize > 100 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    unsigned char *data = (unsigned char *)malloc((size_t)fsize);
    if (!data) { fclose(f); return NULL; }
    n = fread(data, 1, (size_t)fsize, f);
    fclose(f);
    if (n != (size_t)fsize) { free(data); return NULL; }

    char *hex = hash_sha256_hex(data, n);
    free(data);
    if (!hex) return NULL;

    json_t *j = json_object();
    json_set(j, "sha256", json_string(hex));
    free(hex);
    return j;
}
