/* Slermes C port — tools/skills_hub.py (DA-4 gaps)
 * Path resolution, validation, skill name normalization */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _validate_skill_name (L226) ── */
/* PoP: skills_hub_validate_skill_name @ tools/skills_hub.py:_validate_skill_name */
char *skills_hub_validate_skill_name(const char *name) {
    if (!name || !*name) return NULL;
    /* Must be a single path component (no slashes) */
    if (strchr(name, '/') || strchr(name, '\\')) return NULL;
    /* Must match ^[a-zA-Z0-9_-]+$ */
    for (const char *p = name; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_') return NULL;
    }
    return xstrdup(name);
}

/* ── _normalize_bundle_path (L195) ── */
/* PoP: skills_hub_normalize_bundle_path @ tools/skills_hub.py:_normalize_bundle_path */
char *skills_hub_normalize_bundle_path(const char *path_value) {
    if (!path_value || !*path_value) return NULL;
    /* Strip whitespace */
    while (*path_value == ' ' || *path_value == '\t') path_value++;
    if (!*path_value) return NULL;
    /* Replace backslashes with forward slashes */
    char *result = xstrdup(path_value);
    if (!result) return NULL;
    for (char *p = result; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    /* Reject absolute paths */
    if (result[0] == '/') {
        free(result);
        return NULL;
    }
    /* Reject path traversal */
    if (strstr(result, "../") || strstr(result, "/..")) {
        free(result);
        return NULL;
    }
    return result;
}

/* ── _is_path_redirect (L261) ── */
/* PoP: skills_hub_is_path_redirect @ tools/skills_hub.py:_is_path_redirect */
bool skills_hub_is_path_redirect(const char *path) {
    if (!path) return false;
    /* Check if path is a symlink — use lstat */
    struct stat st;
    if (lstat(path, &st) != 0) return false;
    return S_ISLNK(st.st_mode);
}

/* ── _hermes_home (L63) ── */
/* PoP: skills_hub_hermes_home @ tools/skills_hub.py:_hermes_home */
const char *skills_hub_hermes_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (home && *home) return home;
    return "~/.hermes";
}

/* ── _skills_dir (L67) ── */
/* PoP: skills_hub_skills_dir @ tools/skills_hub.py:_skills_dir */
void skills_hub_skills_dir(char *out, size_t out_sz) {
    const char *home = skills_hub_hermes_home();
    snprintf(out, out_sz, "%s/skills", home);
}

/* ── _hub_dir (L72) ── */
/* PoP: skills_hub_hub_dir @ tools/skills_hub.py:_hub_dir */
void skills_hub_hub_dir(char *out, size_t out_sz) {
    char skills[512];
    skills_hub_skills_dir(skills, sizeof(skills));
    snprintf(out, out_sz, "%s/.hub", skills);
}

/* ── _lock_file (L77) ── */
/* PoP: skills_hub_lock_file @ tools/skills_hub.py:_lock_file */
void skills_hub_lock_file(char *out, size_t out_sz) {
    char hub[512];
    skills_hub_hub_dir(hub, sizeof(hub));
    snprintf(out, out_sz, "%s/lock.json", hub);
}

/* ── _quarantine_dir (L82) ── */
/* PoP: skills_hub_quarantine_dir @ tools/skills_hub.py:_quarantine_dir */
void skills_hub_quarantine_dir(char *out, size_t out_sz) {
    char hub[512];
    skills_hub_hub_dir(hub, sizeof(hub));
    snprintf(out, out_sz, "%s/quarantine", hub);
}

/* ── _audit_log (L87) ── */
/* PoP: skills_hub_audit_log @ tools/skills_hub.py:_audit_log */
void skills_hub_audit_log(char *out, size_t out_sz) {
    char hub[512];
    skills_hub_hub_dir(hub, sizeof(hub));
    snprintf(out, out_sz, "%s/audit.log", hub);
}
