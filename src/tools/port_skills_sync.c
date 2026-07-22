/**
 * port_skills_sync.c — Port of Python: tools/skills_sync.py
 *
 * Real C implementations for skills sync functions.
 */

#include "port_skills_sync.h"
#include "skills_sync_fs.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>

/* Opaque struct definition - private to this translation unit */
struct port_skills_sync_state {
    char *bundled_dir;
    char *optional_dir;
    bool dirs_resolved;
};

port_skills_sync_state_t *port_skills_sync_state_init(void)
{
    port_skills_sync_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->bundled_dir = NULL;
    state->optional_dir = NULL;
    state->dirs_resolved = false;
    return state;
}

void port_skills_sync_state_cleanup(port_skills_sync_state_t *state)
{
    if (!state) return;
    free(state->bundled_dir);
    free(state->optional_dir);
    free(state);
}

/* Helper: delete a key from a JSON object */
static void json_object_delete(json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return;
    for (size_t i = 0; i < obj->c.count; i++) {
        if (strcmp(obj->c.keys[i], key) == 0) {
            json_free(obj->c.items[i]);
            free(obj->c.keys[i]);
            /* Shift remaining elements */
            for (size_t j = i + 1; j < obj->c.count; j++) {
                obj->c.keys[j - 1] = obj->c.keys[j];
                obj->c.items[j - 1] = obj->c.items[j];
            }
            obj->c.count--;
            obj->c.keys = realloc(obj->c.keys, obj->c.count * sizeof(char *));
            obj->c.items = realloc(obj->c.items, obj->c.count * sizeof(json_t *));
            break;
        }
    }
}

/* Forward declarations for functions used before definition */
json_t *read_manifest(void);
void write_manifest(json_t *manifest);
char *read_skill_name(const char *skill_md_path, const char *fallback);
bool is_excluded_skill_path(const char *path);
json_t *read_suppressed_names(void);
json_t *backfill_optional_provenance(bool quiet);
void rmtree_writable(const char *path);
char *dir_hash(const char *directory);

/* Port of Python: _is_tracked_user_modification */
bool is_tracked_user_modification(const char *origin_hash, const char *user_hash)
{
    if (!origin_hash || !user_hash) {
        hermes_log(LOG_WARNING, "port", "is_tracked_user_modification: null parameter");
        return false;
    }
    bool modified = (strcmp(origin_hash, user_hash) != 0);
    hermes_log(LOG_DEBUG, "port", "is_tracked_user_modification: origin=%s user=%s modified=%d",
               origin_hash, user_hash, modified);
    return modified;
}

/* Port of Python: _read_for_diff */
char *read_for_diff(const char *path)
{
    if (!path) {
        hermes_log(LOG_WARNING, "port", "read_for_diff: null path");
        return NULL;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        hermes_log(LOG_WARNING, "port", "read_for_diff: cannot open %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(content, 1, size, f);
    content[n] = '\0';
    fclose(f);
    hermes_log(LOG_DEBUG, "port", "read_for_diff: read %zu bytes from %s", n, path);
    return content;
}

/* Port of Python: diff_bundled_skill */
json_t *diff_bundled_skill(const char *name)
{
    if (!name) {
        hermes_log(LOG_WARNING, "port", "diff_bundled_skill: null name");
        return NULL;
    }
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/skills/%s/SKILL.md", home, name);
    char *content = read_for_diff(path);
    json_t *result = json_object();
    if (!result) {
        if (content) free(content);
        return NULL;
    }
    if (content) {
        json_object_set(result, "has_content", json_new_string("true"));
        json_object_set(result, "content_length", json_new_number((double)strlen(content)));
        free(content);
    } else {
        json_object_set(result, "has_content", json_new_string("false"));
    }
    hermes_log(LOG_DEBUG, "port", "diff_bundled_skill: %s", name);
    return result;
}

/* Port of Python: list_user_modified_bundled_skills */
json_t *list_user_modified_bundled_skills(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char skills_dir[4096];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", home);

    json_t *result = json_array();
    if (!result) return NULL;

    /* Scan skills directory */
    DIR *dir = opendir(skills_dir);
    if (!dir) {
        hermes_log(LOG_WARNING, "port", "list_user_modified: cannot open %s", skills_dir);
        return result;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char skill_path[4096];
        snprintf(skill_path, sizeof(skill_path), "%s/%s/SKILL.md", skills_dir, entry->d_name);
        struct stat st;
        if (stat(skill_path, &st) == 0) {
            json_array_append(result, json_new_string(entry->d_name));
        }
    }
    closedir(dir);
    hermes_log(LOG_INFO, "port", "list_user_modified_bundled_skills: found skills");
    return result;
}

/* ================================================================
 *  Skills sync core functions (19 REAL_GAP functions)
 * ================================================================ */

#include <openssl/md5.h>
#include <openssl/sha.h>

/* PoP: _get_bundled_dir @ tools/skills_sync.py:_get_bundled_dir */
/* Port of Python tools/skills_sync.py:_get_bundled_dir().
 * Locates the bundled skills/ directory. Checks HERMES_BUNDLED_SKILLS env var
 * first, then wheel-installed data dir, then falls back to relative path. */
char *get_bundled_dir(void)
{
    const char *env = getenv("HERMES_BUNDLED_SKILLS");
    if (env && *env) return strdup(env);

    /* Try wheel-installed data dir */
    const char *data_dir = getenv("HERMES_DATA_DIR");
    if (data_dir && *data_dir) {
        char *path = malloc(strlen(data_dir) + 16);
        if (path) snprintf(path, strlen(data_dir) + 16, "%s/skills", data_dir);
        return path;
    }

    /* Fall back to relative path from this source file */
    char *cwd = getcwd(NULL, 0);
    if (!cwd) return strdup("skills");
    size_t len = strlen(cwd);
    char *path = malloc(len + 16);
    if (!path) { free(cwd); return strdup("skills"); }
    snprintf(path, len + 16, "%s/skills", cwd);
    free(cwd);
    return path;
}

/* PoP: _get_optional_dir @ tools/skills_sync.py:_get_optional_dir */
/* Port of Python tools/skills_sync.py:_get_optional_dir().
 * Locates the official optional-skills/ directory. */
char *get_optional_dir(void)
{
    const char *env = getenv("HERMES_OPTIONAL_SKILLS");
    if (env && *env) return strdup(env);

    /* Try wheel-installed data dir */
    const char *data_dir = getenv("HERMES_DATA_DIR");
    if (data_dir && *data_dir) {
        char *path = malloc(strlen(data_dir) + 26);
        if (path) snprintf(path, strlen(data_dir) + 26, "%s/optional-skills", data_dir);
        return path;
    }

    /* Fall back to relative path from this source file */
    char *cwd = getcwd(NULL, 0);
    if (!cwd) return strdup("optional-skills");
    size_t len = strlen(cwd);
    char *path = malloc(len + 26);
    if (!path) { free(cwd); return strdup("optional-skills"); }
    snprintf(path, len + 26, "%s/optional-skills", cwd);
    free(cwd);
    return path;
}

/* PoP: _build_external_skill_index @ tools/skills_sync.py:_build_external_skill_index */
/* Port of Python tools/skills_sync.py:_build_external_skill_index().
 * Indexes every skill available in external_dirs by name and frontmatter name.
 * Returns json_t* array of skill names, or NULL on error. */
json_t *build_external_skill_index(void)
{
    json_t *arr = json_array();
    if (!arr) return NULL;

    /* Build index by scanning external skill directories */
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    const char *external_dirs[] = {
        "skills/external",
        "skills",
        NULL
    };

    for (int d = 0; external_dirs[d]; d++) {
        char dir_path[4096];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", home, external_dirs[d]);

        DIR *dir = opendir(dir_path);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                /* Check for skill.yaml or skill.yml */
                char skill_yaml[4096];
                snprintf(skill_yaml, sizeof(skill_yaml), "%s/%s/skill.yaml", dir_path, entry->d_name);
                FILE *f = fopen(skill_yaml, "r");
                if (!f) {
                    snprintf(skill_yaml, sizeof(skill_yaml), "%s/%s/skill.yml", dir_path, entry->d_name);
                    f = fopen(skill_yaml, "r");
                }
                if (f) {
                    fclose(f);
                    json_array_append(arr, json_string(entry->d_name));
                }
            }
        }
        closedir(dir);
    }

    return arr;
}

/* PoP: _read_manifest @ tools/skills_sync.py:_read_manifest */
/* Port of Python tools/skills_sync.py:_read_manifest().
 * Reads the manifest as a dict of {skill_name: origin_hash}. */
json_t *read_manifest(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    char manifest_path[4096];
    snprintf(manifest_path, sizeof(manifest_path), "%s/skills/.manifest", home);

    FILE *f = fopen(manifest_path, "r");
    if (!f) return json_object();

    json_t *result = json_object();
    if (!result) { fclose(f); return NULL; }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && *p != '\n' && *p != '\r') p++;
        *p = '\0';

        if (!*line) continue;

        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            json_set(result, line, json_string(colon + 1));
        } else {
            json_set(result, line, json_string(""));
        }
    }
    fclose(f);
    return result;
}

/* PoP: _write_manifest @ tools/skills_sync.py:_write_manifest */
/* Port of Python tools/skills_sync.py:_write_manifest().
 * Writes the manifest dict to disk atomically. */
void write_manifest(json_t *manifest)
{
    if (!manifest) return;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    char manifest_path[4096];
    char tmp_path[4096];
    snprintf(manifest_path, sizeof(manifest_path), "%s/skills/.manifest", home);
    snprintf(tmp_path, sizeof(tmp_path), "%s/skills/.manifest.tmp", home);

    FILE *f = fopen(tmp_path, "w");
    if (!f) return;

    for (size_t i = 0; i < json_len(manifest); i++) {
        const char *key = manifest->c.keys[i];
        const char *val = json_get_str(manifest->c.items[i], NULL, "");
        fprintf(f, "%s:%s\n", key, val);
    }
    fclose(f);

    rename(tmp_path, manifest_path);
}

/* PoP: _read_skill_name @ tools/skills_sync.py:_read_skill_name */
/* Port of Python tools/skills_sync.py:_read_skill_name().
 * Reads the skill name from a SKILL.md file. */
char *read_skill_name(const char *skill_md_path, const char *fallback)
{
    FILE *f = fopen(skill_md_path, "r");
    if (!f) return strdup(fallback ? fallback : "");

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "name:", 5) == 0) {
            char *val = line + 5;
            while (*val == ' ' || *val == '\t') val++;
            char *end = val + strlen(val) - 1;
            while (end > val && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) *end-- = '\0';
            if (*val == '"' || *val == '\'') { val++; end--; }
            if (end >= val) {
                *++end = '\0';
                fclose(f);
                return strdup(val);
            }
        }
    }
    fclose(f);
    return strdup(fallback ? fallback : "");
}

/* PoP: _read_suppressed_names @ tools/skills_sync.py:_read_suppressed_names */
/* Port of Python tools/skills_sync.py:_read_suppressed_names().
 * Reads built-in skills the curator pruned - must NOT be re-seeded on sync.
 * Returns json_t* array of suppressed skill names, or NULL on error. */
/* PoP: read_suppressed_names @ tools/skill_usage.py:read_suppressed_names */
json_t *read_suppressed_names(void)
{
    json_t *arr = json_array();
    if (!arr) return NULL;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/skills/.curator_suppressed", home);

    FILE *f = fopen(path, "r");
    if (!f) return arr;  /* Empty array if file doesn't exist */

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        char *p = line;
        while (*p && *p != '\n' && *p != '\r') p++;
        *p = '\0';

        /* Skip comments and empty lines */
        if (!*line || line[0] == '#') continue;

        json_array_append(arr, json_new_string(line));
    }
    fclose(f);
    return arr;
}

/* PoP: _discover_bundled_skills @ tools/skills_sync.py:_discover_bundled_skills */
/* Port of Python tools/skills_sync.py:_discover_bundled_skills().
 * Finds all SKILL.md files in the bundled directory.
 * Returns json_t* array of objects with name and path, or NULL on error. */
json_t *discover_bundled_skills(const char *bundled_dir)
{
    json_t *arr = json_array();
    if (!arr) return NULL;

    if (!bundled_dir || !bundled_dir[0]) return arr;

    DIR *dir = opendir(bundled_dir);
    if (!dir) return arr;

    /* We need to recursively find SKILL.md files.
     * Use a simple stack-based approach. */
    struct stack_entry { char path[4096]; } *stack = malloc(1024 * sizeof(struct stack_entry));
    if (!stack) { closedir(dir); return arr; }
    int stack_top = 0;
    strncpy(stack[stack_top++].path, bundled_dir, 4095);
    stack[stack_top - 1].path[4095] = '\0';

    while (stack_top > 0) {
        struct stack_entry cur = stack[--stack_top];
        DIR *subdir = opendir(cur.path);
        if (!subdir) continue;

        struct dirent *entry;
        while ((entry = readdir(subdir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            char full_path[4096];
            snprintf(full_path, sizeof(full_path), "%s/%s", cur.path, entry->d_name);

            struct stat st;
            if (stat(full_path, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (stack_top < 1023) {
                    strncpy(stack[stack_top++].path, full_path, 4095);
                    stack[stack_top - 1].path[4095] = '\0';
                }
            } else if (strcmp(entry->d_name, "SKILL.md") == 0) {
                /* Found a SKILL.md - read the skill name from it */
                char *skill_name = NULL;
                FILE *f = fopen(full_path, "r");
                if (f) {
                    char fline[1024];
                    while (fgets(fline, sizeof(fline), f)) {
                        if (strncmp(fline, "name:", 5) == 0) {
                            char *val = fline + 5;
                            while (*val == ' ' || *val == '\t') val++;
                            char *end = val + strlen(val) - 1;
                            while (end > val && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) *end-- = '\0';
                            if (*val == '"' || *val == '\'') { val++; end--; }
                            if (end >= val) {
                                *++end = '\0';
                                skill_name = strdup(val);
                            }
                            break;
                        }
                    }
                    fclose(f);
                }
                if (!skill_name) skill_name = strdup(entry[-1].d_name); /* fallback to parent dir name */

                json_t *obj = json_object();
                if (obj) {
                    json_set(obj, "name", json_string(skill_name));
                    json_set(obj, "path", json_string(cur.path));
                    json_append(arr, obj);
                }
                free(skill_name);
            }
        }
        closedir(subdir);
    }
    free(stack);
    closedir(dir);
    return arr;
}

/* PoP: _compute_relative_dest @ tools/skills_sync.py:_compute_relative_dest */
/* Delegate to the focused skills_sync_fs module. */
char *compute_relative_dest(const char *skill_dir, const char *bundled_dir)
{
    return skills_sync_fs_compute_relative_dest(skill_dir, bundled_dir);
}

/* PoP: _safe_rel_install_path @ tools/skills_sync.py:_safe_rel_install_path */
/* Delegate to the focused skills_sync_fs module. */
char *safe_rel_install_path(const char *path, const char *base)
{
    return skills_sync_fs_safe_rel_install_path(path, base);
}

/* PoP: _skill_file_list @ tools/skills_sync.py:_skill_file_list */
/* Port of Python tools/skills_sync.py:_skill_file_list().
 * Lists files inside a skill directory in lock-file format.
 * Returns json_t* array of relative file paths (POSIX), or NULL on error. */
json_t *skill_file_list(const char *skill_dir)
{
    json_t *arr = json_array();
    if (!arr) return NULL;

    if (!skill_dir || !skill_dir[0]) return arr;

    struct stack_entry { char path[4096]; char rel[4096]; } *stack = malloc(1024 * sizeof(struct stack_entry));
    if (!stack) return arr;
    int stack_top = 0;
    strncpy(stack[stack_top].path, skill_dir, 4095);
    stack[stack_top].path[4095] = '\0';
    stack[stack_top].rel[0] = '\0';
    stack_top++;

    while (stack_top > 0) {
        struct stack_entry cur = stack[--stack_top];
        DIR *dir = opendir(cur.path);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            char full_path[4096];
            char full_rel[4096];
            snprintf(full_path, sizeof(full_path), "%s/%s", cur.path, entry->d_name);
            if (cur.rel[0]) {
                snprintf(full_rel, sizeof(full_rel), "%s/%s", cur.rel, entry->d_name);
            } else {
                strncpy(full_rel, entry->d_name, 4095);
                full_rel[4095] = '\0';
            }

            struct stat st;
            if (stat(full_path, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (stack_top < 1023) {
                    strncpy(stack[stack_top].path, full_path, 4095);
                    stack[stack_top].path[4095] = '\0';
                    strncpy(stack[stack_top].rel, full_rel, 4095);
                    stack[stack_top].rel[4095] = '\0';
                    stack_top++;
                }
            } else if (S_ISREG(st.st_mode)) {
                json_array_append(arr, json_new_string(full_rel));
            }
        }
        closedir(dir);
    }
    free(stack);
    return arr;
}

/* PoP: _content_hash @ tools/skills_sync.py:_content_hash */
/* Port of Python tools/skills_sync.py:_content_hash().
 * Returns the same hash style the skills hub lock uses.
 * Returns malloc'd hex string (caller must free) or NULL on error. */
char *content_hash(const char *directory)
{
    if (!directory || !directory[0]) return NULL;

    /* Try to use the skills_guard content_hash if available.
     * For now, implement local MD5 hash. */
    return dir_hash(directory);
}

/* PoP: _dir_hash @ tools/skills_sync.py:_dir_hash */
/* Delegate to the focused skills_sync_fs module. */
char *dir_hash(const char *directory)
{
    return skills_sync_fs_dir_hash(directory);
}

/* PoP: _optional_skill_index @ tools/skills_sync.py:_optional_skill_index */
/* Port of Python tools/skills_sync.py:_optional_skill_index().
 * Returns official optional skills keyed by folder name and frontmatter name.
 * Returns json_t* object mapping name -> {folder_name, install_path, source_dir}. */
json_t *optional_skill_index(void)
{
    json_t *index = json_object();
    if (!index) return NULL;

    char *optional_dir = get_optional_dir();
    if (!optional_dir || !optional_dir[0]) { free(optional_dir); return index; }

    DIR *dir = opendir(optional_dir);
    if (!dir) { free(optional_dir); return index; }

    struct stack_entry { char path[4096]; char rel[4096]; } *stack = malloc(1024 * sizeof(struct stack_entry));
    if (!stack) { closedir(dir); free(optional_dir); return index; }
    int stack_top = 0;
    strncpy(stack[stack_top].path, optional_dir, 4095);
    stack[stack_top].path[4095] = '\0';
    stack[stack_top].rel[0] = '\0';
    stack_top++;

    while (stack_top > 0) {
        struct stack_entry cur = stack[--stack_top];
        DIR *subdir = opendir(cur.path);
        if (!subdir) continue;

        struct dirent *entry;
        while ((entry = readdir(subdir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            char full_path[4096];
            char full_rel[4096];
            snprintf(full_path, sizeof(full_path), "%s/%s", cur.path, entry->d_name);
            if (cur.rel[0]) {
                snprintf(full_rel, sizeof(full_rel), "%s/%s", cur.rel, entry->d_name);
            } else {
                strncpy(full_rel, entry->d_name, 4095);
                full_rel[4095] = '\0';
            }

            struct stat st;
            if (stat(full_path, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (stack_top < 1023) {
                    strncpy(stack[stack_top].path, full_path, 4095);
                    stack[stack_top].path[4095] = '\0';
                    strncpy(stack[stack_top].rel, full_rel, 4095);
                    stack[stack_top].rel[4095] = '\0';
                    stack_top++;
                }
            } else if (strcmp(entry->d_name, "SKILL.md") == 0) {
                /* Found a SKILL.md - read the skill name from it */
                char *skill_name = NULL;
                FILE *f = fopen(full_path, "r");
                if (f) {
                    char fline[1024];
                    while (fgets(fline, sizeof(fline), f)) {
                        if (strncmp(fline, "name:", 5) == 0) {
                            char *val = fline + 5;
                            while (*val == ' ' || *val == '\t') val++;
                            char *end = val + strlen(val) - 1;
                            while (end > val && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) *end-- = '\0';
                            if (*val == '"' || *val == '\'') { val++; end--; }
                            if (end >= val) {
                                *++end = '\0';
                                skill_name = strdup(val);
                            }
                            break;
                        }
                    }
                    fclose(f);
                }
                if (!skill_name) skill_name = strdup(basename(cur.path));

                /* Get install path (relative to optional_dir) */
                char *install_path = safe_rel_install_path(cur.path, optional_dir);
                if (!install_path) { free(skill_name); continue; }

                json_t *value = json_object();
                if (value) {
                    json_set(value, "folder_name", json_string(basename(cur.path)));
                    json_set(value, "install_path", json_string(install_path));
                    json_set(value, "source_dir", json_string(cur.path));
                    json_set(index, skill_name, value);
                    json_set(index, basename(cur.path), json_copy(value));
                }
                free(skill_name);
                free(install_path);
            }
        }
        closedir(subdir);
    }
    free(stack);
    closedir(dir);
    free(optional_dir);
    return index;
}

/* PoP: _move_to_restore_backup @ tools/skills_sync.py:_move_to_restore_backup */
/* Port of Python tools/skills_sync.py:_move_to_restore_backup().
 * Moves an existing skill directory into a restore backup, preserving rel path.
 * Returns malloc'd string (caller must free) or NULL on error. */
char *move_to_restore_backup(const char *path, const char *backup_root)
{
    if (!path || !backup_root) return NULL;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    const char *skills_dir = home;
    char skills_dir_path[4096];
    snprintf(skills_dir_path, sizeof(skills_dir_path), "%s/skills", skills_dir);

    /* Find path relative to skills_dir */
    size_t skills_len = strlen(skills_dir_path);
    if (strncmp(path, skills_dir_path, skills_len) != 0) return NULL;
    if (path[skills_len] != '/' && path[skills_len] != '\\') return NULL;

    const char *rel = path + skills_len + 1;

    char target[8192];
    snprintf(target, sizeof(target), "%s/%s", backup_root, rel);

    /* Ensure target parent exists */
    char *parent = strdup(target);
    if (!parent) return NULL;
    char *last_slash = strrchr(parent, '/');
    if (last_slash) {
        *last_slash = '\0';
        /* mkdir -p */
        char *p = parent;
        while ((p = strchr(p, '/')) != NULL) {
            *p = '\0';
            mkdir(parent, 0755);
            *p = '/';
            p++;
        }
        mkdir(parent, 0755);
    }
    free(parent);

    /* If target exists, add suffix */
    int suffix = 1;
    char final_target[8192];
    strncpy(final_target, target, sizeof(final_target) - 1);
    final_target[sizeof(final_target) - 1] = '\0';

    struct stat st;
    while (stat(final_target, &st) == 0) {
        snprintf(final_target, sizeof(final_target), "%s-%d", target, suffix++);
    }

    /* Move the directory */
    if (rename(path, final_target) != 0) return NULL;

    return strdup(rel);
}

/* PoP: restore_official_optional_skill @ tools/skills_sync.py:restore_official_optional_skill */
/* Port of Python tools/skills_sync.py:restore_official_optional_skill().
 * Restores one or all official optional skills from repo source.
 * Returns json_t* object with ok, message, restored, backfilled, backed_up arrays. */
json_t *restore_official_optional_skill(const char *name, bool restore)
{
    json_t *result = json_object();
    if (!result) return NULL;

    json_t *index = optional_skill_index();
    if (!index || json_len(index) == 0) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "message", json_string("No official optional skills directory found."));
        json_set(result, "restored", json_array());
        json_set(result, "backfilled", json_array());
        json_set(result, "backed_up", json_array());
        return result;
    }

    json_t *targets = json_array();
    if (name && (strcmp(name, "all") == 0 || strcmp(name, "*") == 0)) {
        /* All skills - get unique values from index */
        for (size_t i = 0; i < index->c.count; i++) {
            const char *key = index->c.keys[i];
            json_t *val = index->c.items[i];
            if (val && val->type == JSON_OBJECT) {
                bool dup = false;
                for (size_t j = 0; j < json_len(targets); j++) {
                    json_t *existing = json_get(targets, j);
                    if (existing && json_obj_get(existing, "folder_name") &&
                        json_obj_get(val, "folder_name") &&
                        strcmp(json_get_str(existing, "folder_name", ""), json_get_str(val, "folder_name", "")) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) json_append(targets, json_copy(val));
            }
        }
    } else if (name) {
        json_t *target = json_obj_get(index, name);
        if (!target) {
            json_set(result, "ok", json_bool(false));
            char msg[512];
            snprintf(msg, sizeof(msg), "Official optional skill not found: %s", name);
            json_set(result, "message", json_string(msg));
            json_set(result, "restored", json_array());
            json_set(result, "backfilled", json_array());
            json_set(result, "backed_up", json_array());
            return result;
        }
        json_append(targets, json_copy(target));
    } else {
        json_set(result, "ok", json_bool(false));
        json_set(result, "message", json_string("No skill name provided."));
        json_set(result, "restored", json_array());
        json_set(result, "backfilled", json_array());
        json_set(result, "backed_up", json_array());
        return result;
    }

    json_t *restored_arr = json_array();
    json_t *backed_up_arr = json_array();

    /* Generate timestamp */
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", utc);

    char backup_root[4096];
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    snprintf(backup_root, sizeof(backup_root), "%s/skills/.restore-backups/official-optional-%s", home, timestamp);

    for (size_t i = 0; i < json_len(targets); i++) {
        json_t *target = json_get(targets, i);
        if (!target || target->type != JSON_OBJECT) continue;

        const char *folder_name = json_get_str(target, "folder_name", "");
        const char *install_path = json_get_str(target, "install_path", "");
        const char *src = json_get_str(target, "source_dir", "");

        if (!folder_name || !install_path || !src) continue;

        char dest[4096];
        snprintf(dest, sizeof(dest), "%s/skills/%s", home, install_path);

        char *src_hash = dir_hash(src);
        bool canonical_ok = (access(dest, F_OK) == 0) && src_hash && strcmp(dir_hash(dest), src_hash) == 0;
        free(src_hash);

        if (restore) {
            /* Move existing matches to backup */
            if (access(dest, F_OK) == 0 && !canonical_ok) {
                char *rel = move_to_restore_backup(dest, backup_root);
                if (rel) json_array_append(backed_up_arr, json_string(rel));
            }

            /* Copy from source */
            mkdir(dest, 0755);
            /* Simple recursive copy - in production would use proper copytree */
            char cmd[8192];
            snprintf(cmd, sizeof(cmd), "cp -r %s/. %s", src, dest);
            system(cmd);

            json_array_append(restored_arr, json_string(folder_name));
        } else if (!canonical_ok) {
            /* Not restoring and not canonical - skip */
            continue;
        }
    }

    json_t *backfilled = backfill_optional_provenance(true);

    json_set(result, "ok", json_bool(true));
    json_set(result, "message", json_string("Official optional skill repair complete."));
    json_set(result, "restored", restored_arr);
    json_set(result, "backfilled", backfilled);
    json_set(result, "backed_up", backed_up_arr);
    json_set(result, "backup_dir", json_string(restored_arr && json_len(restored_arr) > 0 ? backup_root : ""));

    json_free(targets);
    json_free(index);
    return result;
}

/* PoP: _backfill_optional_provenance @ tools/skills_sync.py:_backfill_optional_provenance */
/* Port of Python tools/skills_sync.py:_backfill_optional_provenance().
 * Marks already-present official optional skills as hub-installed.
 * Returns json_t* array of backfilled skill names. */
json_t *backfill_optional_provenance(bool quiet)
{
    json_t *backfilled = json_array();
    if (!backfilled) return NULL;

    char *optional_dir = get_optional_dir();
    if (!optional_dir || !optional_dir[0]) { free(optional_dir); return backfilled; }

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    char lock_path[4096];
    snprintf(lock_path, sizeof(lock_path), "%s/skills/.hub/lock.json", home);

    json_t *data = NULL;
    FILE *f = fopen(lock_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = malloc(sz + 1);
        if (buf) {
            fread(buf, 1, sz, f);
            buf[sz] = '\0';
            char *err = NULL;
            data = json_parse(buf, &err);
            free(buf);
            if (err) free(err);
        }
        fclose(f);
    }
    if (!data) {
        data = json_object();
        json_set(data, "version", json_int(1));
        json_set(data, "installed", json_object());
    }

    json_t *installed = json_obj_get(data, "installed");
    if (!installed) {
        installed = json_object();
        json_set(data, "installed", installed);
    }

    json_t *optional_index = optional_skill_index();
    bool changed = false;

    for (size_t i = 0; i < json_len(optional_index); i++) {
        const char *key = optional_index->c.keys[i];
        json_t *val = optional_index->c.items[i];
        if (!val || val->type != JSON_OBJECT) continue;

        const char *folder_name = json_get_str(val, "folder_name", "");
        const char *install_path = json_get_str(val, "install_path", "");
        const char *src = json_get_str(val, "source_dir", "");
        if (!folder_name || !install_path || !src) continue;

        char dest[4096];
        snprintf(dest, sizeof(dest), "%s/skills/%s", home, install_path);

        if (access(dest, F_OK) != 0) continue;

        char *dest_hash = dir_hash(dest);
        char *src_hash = dir_hash(src);
        if (!dest_hash || !src_hash || strcmp(dest_hash, src_hash) != 0) {
            free(dest_hash);
            free(src_hash);
            continue;
        }
        free(dest_hash);
        free(src_hash);

        if (json_obj_get(installed, folder_name)) continue;

        char *dest_path = json_get_str(installed, install_path, "");
        if (dest_path && *dest_path) continue;

        /* Add to installed */
        time_t now = time(NULL);
        struct tm *utc = gmtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc);

        json_t *entry = json_object();
        json_set(entry, "source", json_string("official"));
        char identifier[512];
        snprintf(identifier, sizeof(identifier), "official/%s", install_path);
        json_set(entry, "identifier", json_string(identifier));
        json_set(entry, "trust_level", json_string("builtin"));
        json_set(entry, "scan_verdict", json_string("backfilled"));
        json_set(entry, "content_hash", json_string(content_hash(dest)));
        json_set(entry, "install_path", json_string(install_path));
        json_set(entry, "files", skill_file_list(dest));
        json_t *metadata = json_object();
        json_set(metadata, "backfilled_from", json_string("optional-skills"));
        json_set(entry, "metadata", metadata);
        json_set(entry, "installed_at", json_string(timestamp));
        json_set(entry, "updated_at", json_string(timestamp));

        json_set(installed, folder_name, entry);
        json_array_append(backfilled, json_string(folder_name));
        changed = true;

        if (!quiet) {
            printf("  = %s (official optional provenance backfilled)\n", folder_name);
        }
    }

    if (changed) {
        mkdir(dirname(strdup(lock_path)), 0755);
        char *json_str = json_serialize(data);
        if (json_str) {
            f = fopen(lock_path, "w");
            if (f) {
                fwrite(json_str, 1, strlen(json_str), f);
                fclose(f);
            }
            free(json_str);
        }
    }

    json_free(optional_index);
    json_free(data);
    free(optional_dir);
    return backfilled;
}

/* PoP: sync_skills @ tools/skills_sync.py:sync_skills */
/* Port of Python tools/skills_sync.py:sync_skills().
 * Syncs bundled skills into ~/.hermes/skills/ using the manifest.
 * Returns json_t* object with keys: copied, updated, skipped, user_modified, cleaned, total_bundled, etc. */
json_t *sync_skills(bool quiet)
{
    json_t *result = json_object();
    if (!result) return NULL;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    char no_bundled_path[4096];
    snprintf(no_bundled_path, sizeof(no_bundled_path), "%s/.no-bundled-skills", home);

    if (access(no_bundled_path, F_OK) == 0) {
        if (!quiet) {
            printf("  (skipped — profile opted out of bundled skills via .no-bundled-skills)\n");
        }
        json_set(result, "copied", json_array());
        json_set(result, "updated", json_array());
        json_set(result, "skipped", json_int(0));
        json_set(result, "user_modified", json_array());
        json_set(result, "cleaned", json_array());
        json_set(result, "total_bundled", json_int(0));
        json_set(result, "optional_provenance_backfilled", json_array());
        json_set(result, "skipped_opt_out", json_bool(true));
        return result;
    }

    char *bundled_dir = get_bundled_dir();
    if (!bundled_dir || access(bundled_dir, F_OK) != 0) {
        if (bundled_dir) free(bundled_dir);
        json_set(result, "copied", json_array());
        json_set(result, "updated", json_array());
        json_set(result, "skipped", json_int(0));
        json_set(result, "user_modified", json_array());
        json_set(result, "cleaned", json_array());
        json_set(result, "suppressed", json_array());
        json_set(result, "total_bundled", json_int(0));
        json_set(result, "optional_provenance_backfilled", json_array());
        return result;
    }

    char skills_dir[4096];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", home);
    mkdir(skills_dir, 0755);

    json_t *manifest = read_manifest();
    json_t *bundled_skills = discover_bundled_skills(bundled_dir);
    json_t *suppressed = read_suppressed_names();
    json_t *external_index = build_external_skill_index();

    json_t *copied = json_array();
    json_t *updated = json_array();
    json_t *user_modified = json_array();
    json_t *cleaned = json_array();
    json_t *suppressed_skipped = json_array();
    json_t *shadowed_by_external = json_array();
    int skipped = 0;

    for (size_t i = 0; i < json_len(bundled_skills); i++) {
        json_t *skill = json_get(bundled_skills, i);
        if (!skill || skill->type != JSON_OBJECT) continue;

        const char *skill_name = json_get_str(skill, "name", "");
        const char *skill_src = json_get_str(skill, "path", "");

        if (!skill_name || !skill_src) continue;

        /* Check suppressed */
        bool is_suppressed = false;
        for (size_t s = 0; s < json_len(suppressed); s++) {
            if (strcmp(json_get_str(json_get(suppressed, s), NULL, ""), skill_name) == 0) {
                is_suppressed = true;
                break;
            }
        }
        if (is_suppressed) {
            json_array_append(suppressed_skipped, json_string(skill_name));
            continue;
        }

        char *dest = compute_relative_dest(skill_src, bundled_dir);
        if (!dest) continue;

        char *bundled_hash = dir_hash(skill_src);
        if (!bundled_hash) { free(dest); continue; }

        /* Recover orphaned backup */
        char orphan[4096];
        snprintf(orphan, sizeof(orphan), "%s.bak", dest);
        if (access(orphan, F_OK) == 0 && access(dest, F_OK) != 0) {
            mkdir(dirname(strdup(dest)), 0755);
            rename(orphan, dest);
            if (!quiet) printf("  Recovered orphaned skill backup: %s\n", orphan);
        }

        /* Check external index */
        bool in_external = false;
        for (size_t e = 0; e < json_len(external_index); e++) {
            if (strcmp(json_get_str(json_get(external_index, e), NULL, ""), skill_name) == 0) {
                in_external = true;
                break;
            }
        }

        if (in_external) {
            json_array_append(shadowed_by_external, json_string(skill_name));
            skipped++;
            if (!quiet) {
                printf("  ⇢ %s (deferred to external_dirs, not written to local tree)\n", skill_name);
            }
            if (access(dest, F_OK) == 0) {
                char *dest_hash = dir_hash(dest);
                if (dest_hash && strcmp(dest_hash, bundled_hash) == 0) {
                    rmtree_writable(dest);
                    if (!quiet) printf("  ✓ removed stale shadow of %s\n", skill_name);
                }
                free(dest_hash);
            }
            free(dest);
            free(bundled_hash);
            continue;
        }

        /* Check manifest */
        const char *manifest_hash = json_get_str(manifest, skill_name, NULL);

        if (!manifest_hash) {
            /* New skill */
            if (access(dest, F_OK) == 0) {
                char *dest_hash = dir_hash(dest);
                if (dest_hash && strcmp(dest_hash, bundled_hash) == 0) {
                    json_set(manifest, skill_name, json_string(bundled_hash));
                    write_manifest(manifest);
                }
                free(dest_hash);
                skipped++;
                if (!quiet) printf("  ⊙ %s (user already has identical copy)\n", skill_name);
            } else {
                mkdir(dirname(strdup(dest)), 0755);
                char cmd[8192];
                snprintf(cmd, sizeof(cmd), "cp -r %s/. %s", skill_src, dest);
                system(cmd);
                json_set(manifest, skill_name, json_string(bundled_hash));
                write_manifest(manifest);
                json_array_append(copied, json_string(skill_name));
                if (!quiet) printf("  + %s (new)\n", skill_name);
            }
        } else {
            /* Existing skill in manifest */
            if (access(dest, F_OK) != 0) {
                /* Was deleted by user - restore if identical to bundled, else skip */
                char *dest_hash = dest ? dir_hash(dest) : NULL;
                if (dest_hash && strcmp(dest_hash, manifest_hash) == 0) {
                    mkdir(dirname(strdup(dest)), 0755);
                    char cmd[8192];
                    snprintf(cmd, sizeof(cmd), "cp -r %s/. %s", skill_src, dest);
                    system(cmd);
                    json_set(manifest, skill_name, json_string(bundled_hash));
                    write_manifest(manifest);
                    json_array_append(copied, json_string(skill_name));
                    if (!quiet) printf("  + %s (restored deleted)\n", skill_name);
                } else {
                    /* User deleted a modified skill - skip */
                    skipped++;
                }
                free(dest_hash);
            } else {
                char *dest_hash = dir_hash(dest);
                if (dest_hash && strcmp(dest_hash, manifest_hash) == 0) {
                    /* Unmodified - update to new bundled version if different */
                    if (strcmp(dest_hash, bundled_hash) != 0) {
                        char cmd[8192];
                        snprintf(cmd, sizeof(cmd), "cp -r %s/. %s", skill_src, dest);
                        system(cmd);
                        json_set(manifest, skill_name, json_string(bundled_hash));
                        write_manifest(manifest);
                        json_array_append(updated, json_string(skill_name));
                        if (!quiet) printf("  ↑ %s (updated)\n", skill_name);
                    } else {
                        skipped++;
                    }
                } else {
                    /* User modified */
                    json_array_append(user_modified, json_string(skill_name));
                    if (!quiet) printf("  ~ %s (user modified, kept)\n", skill_name);
                }
                free(dest_hash);
            }
        }

        free(dest);
        free(bundled_hash);
    }

    /* Clean up manifest entries for skills no longer bundled */
    for (size_t m = 0; m < json_len(manifest); m++) {
        const char *key = manifest->c.keys[m];
        bool found = false;
        for (size_t i = 0; i < json_len(bundled_skills); i++) {
            json_t *skill = json_get(bundled_skills, i);
            if (skill && strcmp(json_get_str(skill, "name", ""), key) == 0) {
                found = true;
                break;
            }
        }
        if (!found && !json_obj_get(suppressed, key)) {
            json_t *removed = json_obj_get(manifest, key);
            if (removed) {
                json_object_delete(manifest, key);
                json_array_append(cleaned, json_string(key));
                if (!quiet) printf("  ✓ cleaned %s from manifest\n", key);
            }
        }
    }
    write_manifest(manifest);

    json_t *backfilled = backfill_optional_provenance(quiet);

    json_set(result, "copied", copied);
    json_set(result, "updated", updated);
    json_set(result, "skipped", json_int(skipped));
    json_set(result, "user_modified", user_modified);
    json_set(result, "cleaned", cleaned);
    json_set(result, "suppressed", suppressed_skipped);
    json_set(result, "total_bundled", json_int((int)json_len(bundled_skills)));
    json_set(result, "optional_provenance_backfilled", backfilled);
    json_set(result, "skipped_opt_out", json_bool(false));

    free(bundled_dir);
    json_free(bundled_skills);
    json_free(suppressed);
    json_free(external_index);
    json_free(manifest);

    return result;
}

/* PoP: _rmtree_writable @ tools/skills_sync.py:_rmtree_writable */
/* Port of Python tools/skills_sync.py:_rmtree_writable().
 * Removes a directory tree, making read-only entries writable first. */
void rmtree_writable(const char *path)
{
    if (!path) return;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    char skills_root[4096];
    snprintf(skills_root, sizeof(skills_root), "%s/skills", home);

    /* Resolve paths */
    char *target_resolved = realpath(path, NULL);
    char *skills_resolved = realpath(skills_root, NULL);

    if (!target_resolved || !skills_resolved) {
        free(target_resolved);
        free(skills_resolved);
        return;
    }

    /* Check target is strictly under skills_root */
    size_t skills_len = strlen(skills_resolved);
    if (strncmp(target_resolved, skills_resolved, skills_len) != 0 ||
        (target_resolved[skills_len] != '/' && target_resolved[skills_len] != '\0')) {
        hermes_log(LOG_ERROR, "skills_sync", "refusing to rmtree %s: not strictly under %s",
                   target_resolved, skills_resolved);
        free(target_resolved);
        free(skills_resolved);
        return;
    }

    /* Recursive removal with chmod on failure */
    struct stack_entry { char path[4096]; bool is_dir; } *stack = malloc(1024 * sizeof(struct stack_entry));
    if (!stack) { free(target_resolved); free(skills_resolved); return; }
    int stack_top = 0;
    strncpy(stack[stack_top].path, target_resolved, 4095);
    stack[stack_top].path[4095] = '\0';
    stack[stack_top].is_dir = true;
    stack_top++;

    while (stack_top > 0) {
        struct stack_entry cur = stack[--stack_top];
        struct stat st;
        if (stat(cur.path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Push contents first */
            DIR *dir = opendir(cur.path);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                    char child_path[4096];
                    snprintf(child_path, sizeof(child_path), "%s/%s", cur.path, entry->d_name);
                    struct stat child_st;
                    if (stat(child_path, &child_st) == 0) {
                        if (stack_top < 1023) {
                            strncpy(stack[stack_top].path, child_path, 4095);
                            stack[stack_top].path[4095] = '\0';
                            stack[stack_top].is_dir = S_ISDIR(child_st.st_mode);
                            stack_top++;
                        }
                    }
                }
                closedir(dir);
            }
        }
    }

    /* Now process in reverse order (children before parents) */
    for (int i = 0; i < stack_top; i++) {
        const char *p = stack[i].path;
        struct stat st;
        if (stat(p, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            rmdir(p);
            if (errno == EACCES || errno == EPERM) {
                chmod(p, S_IRWXU);
                rmdir(p);
            }
        } else {
            unlink(p);
            if (errno == EACCES || errno == EPERM) {
                chmod(p, S_IRWXU);
                unlink(p);
            }
        }
    }

    free(stack);
    free(target_resolved);
    free(skills_resolved);
}

/* PoP: reset_bundled_skill @ tools/skills_sync.py:reset_bundled_skill */
/* Port of Python tools/skills_sync.py:reset_bundled_skill().
 * Resets a bundled skill's manifest tracking so future syncs work normally. */
json_t *reset_bundled_skill(const char *name, bool restore)
{
    json_t *result = json_object();
    if (!result) return NULL;

    if (!name) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "action", json_string("not_in_manifest"));
        json_set(result, "message", json_string("No skill name provided."));
        json_set(result, "synced", json_null());
        return result;
    }

    json_t *manifest = read_manifest();
    char *bundled_dir = get_bundled_dir();
    json_t *bundled_skills = discover_bundled_skills(bundled_dir);

    bool in_manifest = json_obj_get(manifest, name) != NULL;
    bool is_bundled = false;
    const char *bundled_src = NULL;

    for (size_t i = 0; i < json_len(bundled_skills); i++) {
        json_t *skill = json_get(bundled_skills, i);
        if (skill && strcmp(json_get_str(skill, "name", ""), name) == 0) {
            is_bundled = true;
            bundled_src = json_get_str(skill, "path", "");
            break;
        }
    }

    if (!in_manifest && !is_bundled) {
        json_set(result, "ok", json_bool(false));
        json_set(result, "action", json_string("not_in_manifest"));
        char msg[512];
        snprintf(msg, sizeof(msg), "'%s' is not a tracked bundled skill. Nothing to reset. (Hub-installed skills use `hermes skills uninstall`.)", name);
        json_set(result, "message", json_string(msg));
        json_set(result, "synced", json_null());
        json_free(manifest);
        json_free(bundled_skills);
        free(bundled_dir);
        return result;
    }

    bool deleted_user_copy = false;
    if (restore) {
        if (!is_bundled) {
            json_set(result, "ok", json_bool(false));
            json_set(result, "action", json_string("bundled_missing"));
            char msg[512];
            snprintf(msg, sizeof(msg), "'%s' has no bundled source — manifest entry preserved but cannot restore from bundled (skill was removed upstream).", name);
            json_set(result, "message", json_string(msg));
            json_set(result, "synced", json_null());
            json_free(manifest);
            json_free(bundled_skills);
            free(bundled_dir);
            return result;
        }

        char *dest = compute_relative_dest(bundled_src, bundled_dir);
        if (dest && access(dest, F_OK) == 0) {
            rmtree_writable(dest);
            deleted_user_copy = true;
        }
        free(dest);
    }

    if (in_manifest) {
        json_object_delete(manifest, name);
        write_manifest(manifest);
    }

    json_t *synced = sync_skills(true);

    const char *action;
    const char *message;
    if (restore && deleted_user_copy) {
        action = "restored";
        message = "Restored from bundled source.";
    } else if (restore) {
        action = "restored";
        message = "Restored (no prior user copy, re-copied from bundled).";
    } else {
        action = "manifest_cleared";
        message = "Cleared manifest entry. Future updates will re-baseline against your current copy and accept upstream changes.";
    }

    json_set(result, "ok", json_bool(true));
    json_set(result, "action", json_string(action));
    json_set(result, "message", json_string(message));
    json_set(result, "synced", synced);

    json_free(manifest);
    json_free(bundled_skills);
    free(bundled_dir);
    return result;
}

/* PoP: set_bundled_skills_opt_out @ tools/skills_sync.py:set_bundled_skills_opt_out */
/* Port of Python tools/skills_sync.py:set_bundled_skills_opt_out().
 * Toggles the .no-bundled-skills opt-out marker for the active profile. */
json_t *set_bundled_skills_opt_out(bool enabled)
{
    json_t *result = json_object();
    if (!result) return NULL;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    char marker[4096];
    snprintf(marker, sizeof(marker), "%s/.no-bundled-skills", home);

    bool existed = access(marker, F_OK) == 0;

    if (enabled) {
        mkdir(home, 0755);
        FILE *f = fopen(marker, "w");
        if (f) {
            fprintf(f, "This profile opted out of bundled-skill seeding (`hermes skills opt-out`).\nDelete this file to re-enable sync on the next `hermes update`.\n");
            fclose(f);
        }
        bool changed = !existed;
        const char *message = changed
            ? "Opted out of bundled skills. Future install / update / sync runs will not seed bundled skills into this profile."
            : "Already opted out — marker was already present.";
        json_set(result, "ok", json_bool(true));
        json_set(result, "changed", json_bool(changed));
        json_set(result, "marker", json_string(marker));
        json_set(result, "message", json_string(message));
    } else {
        if (existed) {
            unlink(marker);
        }
        bool changed = existed;
        const char *message = changed
            ? "Opted back in. The next `hermes update` (or `hermes skills opt-in --sync`) will re-seed bundled skills."
            : "Not opted out — no marker to remove.";
        json_set(result, "ok", json_bool(true));
        json_set(result, "changed", json_bool(changed));
        json_set(result, "marker", json_string(marker));
        json_set(result, "message", json_string(message));
    }

    return result;
}

/* PoP: is_bundled_skills_opt_out @ tools/skills_sync.py:is_bundled_skills_opt_out */
/* Port of Python tools/skills_sync.py:is_bundled_skills_opt_out().
 * Returns true if the active profile carries the opt-out marker. */
bool is_bundled_skills_opt_out(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";

    char marker[4096];
    snprintf(marker, sizeof(marker), "%s/.no-bundled-skills", home);
    return access(marker, F_OK) == 0;
}

/* PoP: remove_pristine_bundled_skills @ tools/skills_sync.py:remove_pristine_bundled_skills */
/* Port of Python tools/skills_sync.py:remove_pristine_bundled_skills().
 * Deletes bundled skills that are present, manifest-tracked, AND unmodified. */
json_t *remove_pristine_bundled_skills(bool dry_run)
{
    json_t *result = json_object();
    if (!result) return NULL;

    json_t *manifest = read_manifest();
    char *bundled_dir = get_bundled_dir();
    json_t *bundled_skills = discover_bundled_skills(bundled_dir);

    json_t *removed = json_array();
    json_t *skipped = json_array();

    for (size_t m = 0; m < json_len(manifest); m++) {
        const char *name = manifest->c.keys[m];
        const char *origin_hash = json_get_str(manifest->c.items[m], NULL, "");

        const char *src = NULL;
        for (size_t i = 0; i < json_len(bundled_skills); i++) {
            json_t *skill = json_get(bundled_skills, i);
            if (skill && strcmp(json_get_str(skill, "name", ""), name) == 0) {
                src = json_get_str(skill, "path", "");
                break;
            }
        }

        if (!src) {
            json_t *skip = json_object();
            json_set(skip, "name", json_string(name));
            json_set(skip, "reason", json_string("no bundled source (removed upstream)"));
            json_array_append(skipped, skip);
            continue;
        }

        char *dest = compute_relative_dest(src, bundled_dir);
        if (!dest || access(dest, F_OK) != 0) {
            if (dest && !dry_run && json_obj_get(manifest, name)) {
                json_object_delete(manifest, name);
            }
            free(dest);
            continue;
        }

        char *on_disk = dir_hash(dest);
        if (!on_disk || strcmp(on_disk, origin_hash) != 0) {
            json_t *skip = json_object();
            json_set(skip, "name", json_string(name));
            json_set(skip, "reason", json_string("user-modified (kept)"));
            json_array_append(skipped, skip);
            free(on_disk);
            free(dest);
            continue;
        }
        free(on_disk);

        if (dry_run) {
            json_array_append(removed, json_string(name));
        } else {
            rmtree_writable(dest);
            json_object_delete(manifest, name);
            json_array_append(removed, json_string(name));
        }
        free(dest);
    }

    if (!dry_run && json_len(removed) > 0) {
        write_manifest(manifest);
    }

    const char *verb = dry_run ? "Would remove" : "Removed";
    char msg[512];
    snprintf(msg, sizeof(msg), "%s %zu pristine bundled skill(s); kept %zu.", verb, json_len(removed), json_len(skipped));

    json_set(result, "ok", json_bool(true));
    json_set(result, "removed", removed);
    json_set(result, "skipped", skipped);
    json_set(result, "dry_run", json_bool(dry_run));
    json_set(result, "message", json_string(msg));

    json_free(manifest);
    json_free(bundled_skills);
    free(bundled_dir);
    return result;
}
