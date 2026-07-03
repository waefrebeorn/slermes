/**
 * port_skills_sync.c — Port of Python: tools/skills_sync.py
 *
 * Real C implementations for skills sync functions.
 */

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
