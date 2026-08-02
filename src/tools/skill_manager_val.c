/*
 * skill_manager_val.c — Skill Manager validation/security core (faithful C11
 * port of tools/skill_manager_tool.py). See skill_manager_val.h.
 */

#include "skill_manager_val.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

const char *SKILL_VAL_ALLOWED_SUBDIRS[SKILL_VAL_NUM_ALLOWED_SUBDIRS] = {
    "references", "templates", "scripts", "assets"
};

/* VALID_NAME_RE = ^[a-z0-9][a-z0-9._-]*$ */
static bool valid_name_chars(const char *s) {
    if (!s || !*s) return false;
    for (size_t i = 0; s[i]; i++) {
        char c = s[i];
        if (!islower((unsigned char)c) && !isdigit((unsigned char)c) &&
            c != '.' && c != '_' && c != '-') return false;
    }
    return true;
}

char *skill_val_validate_name(const char *name) {
    if (!name || !*name) return strdup("Skill name is required.");
    if (strlen(name) > SKILL_VAL_MAX_NAME_LENGTH) {
        char *r = (char*)malloc(64);
        snprintf(r, 64, "Skill name exceeds %d characters.", SKILL_VAL_MAX_NAME_LENGTH);
        return r;
    }
    if (!islower((unsigned char)name[0]) && !isdigit((unsigned char)name[0]))
        return strdup("Invalid skill name. Use lowercase letters, numbers, hyphens, dots, and underscores. Must start with a letter or digit.");
    if (!valid_name_chars(name))
        return strdup("Invalid skill name. Use lowercase letters, numbers, hyphens, dots, and underscores. Must start with a letter or digit.");
    return NULL;
}

/* PoP: skill_val_validate_category @ tools/skill_manager_tool.py:_validate_category */
char *skill_val_validate_category(const char *category) {
    if (!category) return NULL;
    if (strchr(category, '/') || strchr(category, '\\'))
        return strdup("Invalid category. Use lowercase letters, numbers, hyphens, dots, and underscores. Categories must be a single directory name.");
    if (strlen(category) > SKILL_VAL_MAX_NAME_LENGTH) {
        char *r = (char*)malloc(64);
        snprintf(r, 64, "Category exceeds %d characters.", SKILL_VAL_MAX_NAME_LENGTH);
        return r;
    }
    if (!valid_name_chars(category))
        return strdup("Invalid category. Use lowercase letters, numbers, hyphens, dots, and underscores. Categories must be a single directory name.");
    return NULL;
}

/* PoP: _validate_content_size @ tools/skill_manager_tool.py:_validate_content_size */
char *skill_val_validate_content_size(const char *content, const char *label) {
    const char *lab = label && *label ? label : "SKILL.md";
    size_t len = content ? strlen(content) : 0;
    if (len > SKILL_VAL_MAX_SKILL_CONTENT_CHARS) {
        size_t need = 256;
        char *r = (char*)malloc(need);
        snprintf(r, need,
            "%s content is %zu characters (limit: %d). Consider splitting into a smaller SKILL.md with supporting files in references/ or templates/.",
            lab, len, SKILL_VAL_MAX_SKILL_CONTENT_CHARS);
        return r;
    }
    return NULL;
}

/* Minimal frontmatter check (faithful to the Python contract without a YAML
 * parser): must start with '---', have a closing '\n---\n', contain 'name:' and
 * 'description:' keys, description within length, and non-empty body. */
/* PoP: skill_val_validate_frontmatter @ tools/skill_manager_tool.py:_validate_frontmatter */
char *skill_val_validate_frontmatter(const char *content) {
    if (!content || !*content) return strdup("Content cannot be empty.");
    while (*content == ' ' || *content == '\t') content++;
    if (strncmp(content, "---", 3) != 0)
        return strdup("SKILL.md must start with YAML frontmatter (---). See existing skills for format.");

    /* find closing "\n---\n" in content[3:] */
    const char *p = content + 3;
    const char *close = NULL;
    while (*p) {
        if (*p == '\n' && strncmp(p+1, "---", 3) == 0 &&
            (p[4] == '\n' || p[4] == '\r' || p[4] == '\0')) { close = p; break; }
        p++;
    }
    if (!close) return strdup("SKILL.md frontmatter is not closed. Ensure you have a closing '---' line.");

    /* frontmatter block = content[3 .. close] */
    size_t fm_len = (size_t)(close - (content + 3));
    char *fm = (char*)malloc(fm_len + 1);
    memcpy(fm, content + 3, fm_len); fm[fm_len] = '\0';

    bool has_name = false, has_desc = false;
    char *line = fm;
    char *body_start = (char*)(close + 4); /* after "\n---\n" */
    while (*line) {
        char *nl = strchr(line, '\n');
        size_t llen = nl ? (size_t)(nl - line) : strlen(line);
        /* key: value */
        char *colon = (char*)memchr(line, ':', llen);
        if (colon && colon > line) {
            size_t klen = (size_t)(colon - line);
            if (klen == 4 && strncmp(line, "name", 4) == 0) has_name = true;
            if (klen == 11 && strncmp(line, "description", 11) == 0) {
                has_desc = true;
                /* description value length */
                const char *v = colon + 1;
                while (*v == ' ' || *v == '\t') v++;
                size_t vlen = strlen(v);
                if (vlen > SKILL_VAL_MAX_DESCRIPTION_LENGTH) {
                    char *r = (char*)malloc(64);
                    snprintf(r, 64, "Description exceeds %d characters.", SKILL_VAL_MAX_DESCRIPTION_LENGTH);
                    free(fm); return r;
                }
            }
        }
        if (!nl) break;
        line = nl + 1;
    }
    free(fm);
    if (!has_name) return strdup("Frontmatter must include 'name' field.");
    if (!has_desc) return strdup("Frontmatter must include 'description' field.");

    /* body must be non-empty */
    while (*body_start == ' ' || *body_start == '\t' || *body_start == '\n' || *body_start == '\r') body_start++;
    if (!*body_start) return strdup("SKILL.md must have content after the frontmatter (instructions, procedures, etc.).");
    return NULL;
}

bool skill_val_is_path_redirect(const char *path) {
    if (!path) return false;
    struct stat st;
    /* lstat follows no symlinks; S_ISLNK true => redirect */
    if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) return true;
    return false;
}

/* path has a '..' traversal component */
static bool has_traversal(const char *p) {
    if (!p) return false;
    const char *s = p;
    while (*s) {
        if (s[0] == '.' && s[1] == '.' && (s[2] == '/' || s[2] == '\\' || s[2] == '\0')) return true;
        if (s[0] == '/' || s[0] == '\\') s++;
        else s++;
    }
    return false;
}

static int count_parts(const char *p) {
    int n = 0; bool sep = true;
    for (size_t i = 0; p[i]; i++) {
        if (p[i] == '/' || p[i] == '\\') sep = true;
        else { if (sep) { n++; sep = false; } }
    }
    return n;
}

/* first path component (copy, caller frees) */
static char *first_component(const char *p) {
    while (*p == '/' || *p == '\\') p++;
    const char *end = p;
    while (*end && *end != '/' && *end != '\\') end++;
    size_t n = (size_t)(end - p);
    char *c = (char*)malloc(n + 1);
    memcpy(c, p, n); c[n] = '\0';
    return c;
}

char *skill_val_validate_file_path(const char *file_path) {
    if (!file_path || !*file_path) return strdup("file_path is required.");
    if (has_traversal(file_path)) return strdup("Path traversal ('..') is not allowed.");

    /* name (last component) */
    const char *slash = strrchr(file_path, '/');
    const char *name = slash ? slash + 1 : file_path;
    int parts = count_parts(file_path);
    if (strcmp(name, "SKILL.md") == 0) {
        if (parts == 1 || parts == 2) return NULL;
    }
    char *first = first_component(file_path);
    bool ok_sub = false;
    for (int i = 0; i < SKILL_VAL_NUM_ALLOWED_SUBDIRS; i++)
        if (strcmp(first, SKILL_VAL_ALLOWED_SUBDIRS[i]) == 0) { ok_sub = true; break; }
    free(first);
    if (!ok_sub) {
        char *r = (char*)malloc(160);
        snprintf(r, 160, "File must be under one of: assets, references, scripts, templates. Got: '%s'", file_path);
        return r;
    }
    if (parts < 2) {
        char *r = (char*)malloc(160);
        snprintf(r, 160, "Provide a file path, not just a directory. Example: '%s/myfile.md'", SKILL_VAL_ALLOWED_SUBDIRS[0]);
        return r;
    }
    return NULL;
}

/* PoP: skill_val_resolve_skill_target @ tools/skill_manager_tool.py:_resolve_skill_target */
char *skill_val_resolve_skill_target(const char *skill_dir, const char *file_path,
                                     char **out_error) {
    if (out_error) *out_error = NULL;
    if (has_traversal(file_path)) {
        char *e = strdup("Path traversal ('..') is not allowed.");
        if (out_error) *out_error = e; else free(e);
        return NULL;
    }
    /* Join skill_dir + file_path, then verify the result stays within skill_dir. */
    size_t need = strlen(skill_dir) + 1 + strlen(file_path) + 1;
    char *joined = (char*)malloc(need);
    snprintf(joined, need, "%s/%s", skill_dir, file_path);
    /* normalize: reject if resolved path does not start with skill_dir/ */
    size_t sd = strlen(skill_dir);
    if (strncmp(joined, skill_dir, sd) != 0 ||
        (joined[sd] != '/' && joined[sd] != '\\')) {
        char *e = strdup("File path escapes the skill directory.");
        free(joined);
        if (out_error) *out_error = e; else free(e);
        return NULL;
    }
    return joined;
}

/* PoP: skill_val_validate_delete_target @ tools/skill_manager_tool.py:_validate_delete_target */
char *skill_val_validate_delete_target(const char *skill_dir,
                                       char **roots, int nroots) {
    if (skill_val_is_path_redirect(skill_dir)) {
        size_t n = strlen(skill_dir) + 96;
        char *r = (char*)malloc(n);
        snprintf(r, n, "Refusing to delete '%s': the skill directory is a symlink/junction. Remove the link target manually if intended.", skill_dir);
        return r;
    }
    /* resolve skill_dir (best-effort: use as-is; real impl would realpath) */
    char resolved[4096];
    snprintf(resolved, sizeof(resolved), "%s", skill_dir);

    for (int i = 0; i < nroots; i++) {
        size_t rl = strlen(roots[i]);
        if (strncmp(resolved, roots[i], rl) == 0 &&
            (resolved[rl] == '\0' || resolved[rl] == '/' || resolved[rl] == '\\')) {
            if (resolved[rl] == '\0')
                return strdup("Refusing to delete: resolves to the skills root itself, which would remove every installed skill.");
            /* strictly inside: at least one component below root */
            const char *rest = resolved + rl + 1;
            if (*rest) return NULL;
        }
    }
    size_t n = strlen(skill_dir) + 96;
    char *r = (char*)malloc(n);
    snprintf(r, n, "Refusing to delete '%s': path does not resolve inside any known skills root.", skill_dir);
    return r;
}

/* PoP: skill_val_containing_skills_root @ tools/skill_manager_tool.py:_containing_skills_root */
char *skill_val_containing_skills_root(const char *skill_path,
                                       char **roots, int nroots) {
    for (int i = 0; i < nroots; i++) {
        size_t rl = strlen(roots[i]);
        if (strncmp(skill_path, roots[i], rl) == 0 &&
            (skill_path[rl] == '\0' || skill_path[rl] == '/' || skill_path[rl] == '\\')) {
            if (skill_path[rl] != '\0') return strdup(roots[i]);
        }
    }
    return NULL;
}
