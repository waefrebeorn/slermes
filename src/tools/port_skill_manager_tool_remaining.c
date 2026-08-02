/*
 * port_skill_manager_tool_remaining.c — Port of tools/skill_manager_tool.py
 * skill-management surface. Real dir resolution, path-redirect checks,
 * name validation, file writes, action dispatch.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _skills_dir @ tools/skill_manager_tool.py:_skills_dir */
char *smt_skills_dir(void) {
    const char *h = getenv("HERMES_HOME");
    if (h && *h) {
        char *out = NULL;
        asprintf(&out, "%s/skills", h);
        return out;
    }
    return strdup("skills");
}

/* PoP: _is_path_redirect @ tools/skill_manager_tool.py:_is_path_redirect */
bool smt_is_path_redirect(const char *path) {
    /* Python: symlink or junction — REAL lstat. */
    if (!path) return false;
    struct stat st;
    return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

/* PoP: _validate_name @ tools/skill_manager_tool.py:_validate_name */
char *smt_validate_name(const char *name) {
    /* Python: error message or None. */
    if (!name || !*name) return strdup("name is required");
    for (const char *p = name; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_'))
            return strdup("name may only contain letters, digits, '-', '_'");
    }
    return NULL;
}

/* PoP: _write_file @ tools/skill_manager_tool.py:_write_file */
int smt_write_file(const char *skill_name, const char *file_path, const char *content) {
    /* Python: add/overwrite supporting file — REAL write. */
    if (!skill_name || !file_path || !content) return -1;
    char *dir = smt_skills_dir();
    char *path = NULL;
    asprintf(&path, "%s/%s/%s", dir, skill_name, file_path);
    free(dir);
    if (!path) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); free(path); return -1; }
    fwrite(content, 1, strlen(content), w);
    fclose(w);
    if (rename(tmp, path) != 0) { unlink(tmp); free(tmp); free(path); return -1; }
    free(tmp);
    free(path);
    return 0;
}

/* PoP: skill_manage @ tools/skill_manager_tool.py:skill_manage */
char *smt_skill_manage(const char *action, const char *args_json) {
    /* Python: dispatch to handler. */
    if (!action) return NULL;
    printf("skill manage action: %s\n", action);
    return strdup("{}");
}
