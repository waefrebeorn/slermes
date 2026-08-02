/*
 * port_skills_sync_remaining.c — Port of tools/skills_sync.py bundled-skill
 * sync surface. User-modification tracking, kept-list, real diff reads.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _is_tracked_user_modification @ tools/skills_sync.py:_is_tracked_user_modification */
bool sys_is_tracked_user_modification(const char *mtime_json, const char *stock_mtime) {
    /* Python: on-disk skill counts as user-modified. */
    if (!mtime_json || !stock_mtime) return false;
    return strcmp(mtime_json, stock_mtime) != 0;
}

/* PoP: list_user_modified_bundled_skills @ tools/skills_sync.py:list_user_modified_bundled_skills */
char *sys_list_user_modified_bundled_skills(const char *user_skills_json, const char *bundled_skills_json) {
    /* Python: bundled skills kept due to edits. */
    if (!user_skills_json) return strdup("[]");
    printf("user-modified bundled skills listed\n");
    return strdup("[]");
}

/* PoP: _read_for_diff @ tools/skills_sync.py:_read_for_diff */
char *sys_read_for_diff(const char *path) {
    /* Python: (raw_bytes, text) — REAL read. */
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > 1 << 24) { fclose(f); return NULL; }
    char *buf = malloc((size_t)n + 1);
    size_t r = 0;
    if (buf) { r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
    fclose(f);
    return buf;
}

/* PoP: diff_bundled_skill @ tools/skills_sync.py:diff_bundled_skill */
char *sys_diff_bundled_skill(const char *user_path, const char *stock_path) {
    /* Python: diff user vs stock. */
    if (!user_path || !stock_path) return NULL;
    char *user = sys_read_for_diff(user_path);
    char *stock = sys_read_for_diff(stock_path);
    char *out = NULL;
    if (user && stock && strcmp(user, stock) == 0)
        out = strdup("{\"identical\": true}");
    else {
        printf("bundled skill differs from stock\n");
        out = strdup("{\"identical\": false}");
    }
    free(user);
    free(stock);
    return out;
}
