/*
 * skills_fs.c - skill filesystem paths and small file helpers.
 *
 * Split from skills.c (soul.md: self-contained modules). Path construction
 * plus the stat/read/write helpers. No file-scope state.
 */
#include "skills_fs.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

/* Port of Python: _skills_dir */

/* ================================================================
 *  Path utilities
 * ================================================================ */ void skills_dir(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(buf, sz, "%s/.hermes/skills", home);
    struct stat st;
    if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode))
        return;
    snprintf(buf, sz, "%s/skills", home);
}
 void skills_bundles_dir(char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s", sd, SKILL_BUNDLES_SUBDIR);
}


/* Port of Python gateway/platforms/weixin.py:_path(). */ void skill_path(const char *name, char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s", sd, name);
}
 void skill_sk_path(const char *name, char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s/SKILL.md", sd, name);
}
 void skill_prov_path(const char *name, char *buf, size_t sz) {
    char sd[HERMES_PATH_MAX];
    skills_dir(sd, sizeof(sd));
    snprintf(buf, sz, "%s/%s/%s", sd, name, SKILL_PROVENANCE_FILE);
}


/* Port of Python: bundle_path_for */ void bundle_path(const char *bundle, char *buf, size_t sz) {
    char bd[HERMES_PATH_MAX];
    skills_bundles_dir(bd, sizeof(bd));
    snprintf(buf, sz, "%s/%s.bundle", bd, bundle);
}
 void skills_ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return;
    mkdir(path, 0755);
}


/* ================================================================
 *  File I/O helpers
 * ================================================================ */ char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz > 512 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}


/* Port of Python tools/memory_tool.py:_write_file(). */
/* Port of Python tools/file_operations.py:write_file(). */ bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return written == len;
}
 time_t file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}
 bool skills_dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
 bool skills_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
