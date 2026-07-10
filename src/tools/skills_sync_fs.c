/*
 * skills_sync_fs.c — focused extraction from tools/skills_sync.py
 *
 * Pure filesystem helpers (see skills_sync_fs.h). These are the
 * deterministic, oracle-verifiable core of skills_sync; kept here so the
 * MD5 + path-safety logic is defined ONCE and reused by the
 * port_skills_sync.c sync routines (no duplicated hashing/sanitizing).
 */

#ifndef SRC_TOOLS_SKILLS_SYNC_FS_C
#define SRC_TOOLS_SKILLS_SYNC_FS_C

#include "skills_sync_fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/md5.h>

/* Shared recursive-dir walker (mirrors Python sorted(directory.rglob('*')):
 * visits every entry; calls `on_file`(full_path, rel_path) for regular
 * files. `rel` is the path relative to `root` ("" at root). */

typedef void (*walk_file_t)(const char *full, const char *rel, void *ctx);

static void walk_dir(const char *root, const char *rel, walk_file_t on_file, void *ctx)
{
    char path[8192];
    snprintf(path, sizeof(path), "%s%s%s", root,
             rel[0] ? "/" : "", rel);

    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    /* Sort entries for deterministic (sorted) iteration like Python. */
    char names[1024][256];
    int n = 0;
    while ((entry = readdir(dir)) != NULL && n < 1024) {
        if (entry->d_name[0] == '.') continue;
        strncpy(names[n], entry->d_name, 255);
        names[n][255] = '\0';
        n++;
    }
    closedir(dir);
    /* insertion sort by name */
    for (int i = 1; i < n; i++) {
        char key[256]; strncpy(key, names[i], 255); key[255] = '\0';
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], key) > 0) {
            strncpy(names[j + 1], names[j], 255); names[j + 1][255] = '\0';
            j--;
        }
        strncpy(names[j + 1], key, 255); names[j + 1][255] = '\0';
    }

    for (int i = 0; i < n; i++) {
        char full[8192], rel2[8192];
        snprintf(full, sizeof(full), "%s/%s", path, names[i]);
        snprintf(rel2, sizeof(rel2), "%s%s%s", rel,
                 rel[0] ? "/" : "", names[i]);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk_dir(root, rel2, on_file, ctx);
        } else if (S_ISREG(st.st_mode)) {
            on_file(full, rel2, ctx);
        }
    }
}

/* ---- dir_hash (MD5 over sorted rel-path + bytes) ----------------- */

struct hash_ctx { MD5_CTX md; };

static void hash_on_file(const char *full, const char *rel, void *ctx)
{
    struct hash_ctx *h = (struct hash_ctx *)ctx;
    MD5_Update(&h->md, rel, strlen(rel));
    FILE *f = fopen(full, "rb");
    if (f) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            MD5_Update(&h->md, buf, n);
        fclose(f);
    }
}

char *skills_sync_fs_dir_hash(const char *directory)
{
    if (!directory || !directory[0]) return NULL;

    struct hash_ctx h;
    MD5_Init(&h.md);

    /* Python updates str(rel).encode first even before files exist,
     * but the effect is identical: each file's rel then bytes. */
    walk_dir(directory, "", hash_on_file, &h);

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &h.md);

    char *hex = malloc(2 * MD5_DIGEST_LENGTH + 1);
    if (!hex) return NULL;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    return hex;
}

/* ---- safe_rel_install_path (traversal rejection) ------------------ */

char *skills_sync_fs_safe_rel_install_path(const char *path, const char *base)
{
    if (!path || !base) return NULL;

    size_t base_len = strlen(base);
    if (strncmp(path, base, base_len) != 0) return NULL;
    if (path[base_len] != '/' && path[base_len] != '\\') return NULL;

    const char *rel = path + base_len + 1;

    /* Split on / or \, reject empty, ".", and ".." segments. */
    char *rel_copy = strdup(rel);
    if (!rel_copy) return NULL;

    char *parts[256];
    int part_count = 0;
    char *saveptr = NULL;
    char *tok = strtok_r(rel_copy, "/\\", &saveptr);
    while (tok && part_count < 256) {
        if (strcmp(tok, "") != 0 && strcmp(tok, ".") != 0) {
            if (strcmp(tok, "..") == 0) {
                free(rel_copy);
                return NULL; /* traversal detected */
            }
            parts[part_count++] = tok;
        }
        tok = strtok_r(NULL, "/\\", &saveptr);
    }

    size_t total = 1;
    for (int i = 0; i < part_count; i++) total += strlen(parts[i]) + 1;
    char *result = malloc(total);
    if (!result) { free(rel_copy); return NULL; }

    result[0] = '\0';
    for (int i = 0; i < part_count; i++) {
        if (i > 0) strcat(result, "/");
        strcat(result, parts[i]);
    }
    free(rel_copy);
    return result;
}

/* ---- compute_relative_dest -------------------------------------- */

char *skills_sync_fs_compute_relative_dest(const char *skill_dir,
                                           const char *bundled_dir)
{
    if (!skill_dir || !bundled_dir) return NULL;

    size_t bundled_len = strlen(bundled_dir);
    if (strncmp(skill_dir, bundled_dir, bundled_len) != 0) return NULL;
    if (skill_dir[bundled_len] != '/' && skill_dir[bundled_len] != '\\')
        return NULL;

    const char *rel = skill_dir + bundled_len + 1;
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    size_t needed = strlen(home) + 8 + strlen(rel) + 1;
    char *dest = malloc(needed);
    if (!dest) return NULL;

    snprintf(dest, needed, "%s/skills/%s", home, rel);
    return dest;
}

#endif /* SRC_TOOLS_SKILLS_SYNC_FS_C */
