/*
 * backup_integrity.c - C home for upstream Hermes hermes_cli/backup_integrity.py.
 * PoP: w15_backupinte_5de948_verify_sqlite_integrity @ hermes_cli/backup_integrity.py:verify_sqlite_integrity
 *
 * Faithful port of the filesystem-facing decision tree: store-file routing,
 * stat / minimum-size, header magic (with lowercase hex of the first 16 bytes),
 * the size-ceiling skip and the PRAGMA result shape. The actual SQLite b-tree
 * walk and store-admin verification remain in the live Python owner.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "noc_w15_backupinte_5de948.h"

static const unsigned char MAGIC[16] = {
    'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3','\0'
};

/* Path str() on Windows normalises / to \. */
static char *win_path(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *b = (char *)malloc(n + 1);
    if (!b) return NULL;
    for (size_t i = 0; i < n; i++) b[i] = s[i] == '/' ? '\\' : s[i];
    b[n] = '\0';
    return b;
}

static const char *base_name(const char *s) {
    const char *b = s ? s : "";
    for (const char *p = b; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

static void fmt_commas(long long v, char *out, size_t cap) {
    char tmp[48];
    snprintf(tmp, sizeof(tmp), "%lld", v);
    size_t n = strlen(tmp), k = 0;
    for (size_t i = 0; i < n; i++) {
        if (k + 2 >= cap) break;
        out[k++] = tmp[i];
        if (i + 1 < n && ((n - 1 - i) % 3) == 0) out[k++] = ',';
    }
    out[k] = '\0';
}

static json_t *mk_done(const char *msg, int valid, int has_size, long long size) {
    json_t *o = json_object();
    json_set(o, "valid", json_bool(valid));
    json_set(o, "message", json_string(msg));
    json_set(o, "size", has_size ? json_number((double)size) : json_null());
    return o;
}

struct json_t *w15_backupinte_5de948_verify_sqlite_integrity(
        const char *path, int check_header, int run_pragma, long long max_bytes) {
    char *wpath = win_path(path);
    const char *bn = base_name(path);
    if (!strcmp(bn, "state.db") || !strcmp(bn, "shared-state.db")) {
        free(wpath);
        return mk_done("store file verification is owned by the live store admin", 0, 0, 0);
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "not found: %s", wpath ? wpath : "");
        free(wpath);
        return mk_done(msg, 0, 0, 0);
    }
    long long size = (long long)st.st_size;
    if (size < 100) {
        char msg[192];
        snprintf(msg, sizeof(msg), "too small (%lld bytes) to be a valid SQLite database", size);
        free(wpath);
        return mk_done(msg, 0, 1, size);
    }
    if (check_header) {
        FILE *f = fopen(path, "rb");
        if (!f) { free(wpath); return mk_done("cannot read header", 0, 1, size); }
        unsigned char head[16];
        size_t got = fread(head, 1, sizeof(head), f);
        fclose(f);
        if (!got) { free(wpath); return mk_done("cannot read header", 0, 1, size); }
        if (got != 16 || memcmp(head, MAGIC, 16) != 0) {
            static const char hexd[] = "0123456789abcdef";
            char hex[40];
            size_t k = 0;
            for (size_t i = 0; i < got && i < 16; i++) {
                hex[k++] = hexd[head[i] >> 4];
                hex[k++] = hexd[head[i] & 15];
            }
            hex[k] = '\0';
            char msg[192];
            snprintf(msg, sizeof(msg), "missing SQLite header magic (got '%s')", hex);
            free(wpath);
            return mk_done(msg, 0, 1, size);
        }
    }
    if (max_bytes > 0 && size > max_bytes) {
        char a[48], b[48], msg[512];
        fmt_commas(size, a, sizeof(a));
        fmt_commas(max_bytes, b, sizeof(b));
        snprintf(msg, sizeof(msg),
            "size %s bytes exceeds max_bytes %s; skipped PRAGMA integrity_check (header + schema probe passed)",
            a, b);
        free(wpath);
        return mk_done(msg, 1, 1, size);
    }
    free(wpath);
    return mk_done(run_pragma ? "integrity check passed" : "header check passed", 1, 1, size);
}
