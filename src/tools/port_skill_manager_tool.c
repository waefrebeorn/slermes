/**
 * port_skill_manager_tool.c — Port of Python: tools/skill_manager_tool.py
 *
 * Real C implementations for skill manager tool.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Port of Python: _validate_delete_target */
char *validate_delete_target(const char *skill_dir)
{
    if (!skill_dir) {
        hermes_log(LOG_WARNING, "port", "validate_delete_target: null skill_dir");
        return strdup("{\"error\": \"null skill_dir\"}");
    }
    struct stat st;
    if (stat(skill_dir, &st) != 0) {
        hermes_log(LOG_WARNING, "port", "validate_delete_target: %s not found", skill_dir);
        return strdup("{\"error\": \"directory not found\"}");
    }
    if (!S_ISDIR(st.st_mode)) {
        hermes_log(LOG_WARNING, "port", "validate_delete_target: %s is not a directory", skill_dir);
        return strdup("{\"error\": \"not a directory\"}");
    }
    char *result = malloc(4096);
    if (!result) return NULL;
    snprintf(result, 4096, "{\"valid\": true, \"path\": \"%s\", \"size\": %ld}",
             skill_dir, (long)st.st_size);
    hermes_log(LOG_DEBUG, "port", "validate_delete_target: %s OK", skill_dir);
    return result;
}

/* Port of Python: _validate_file_path */
char *validate_file_path(const char *file_path)
{
    if (!file_path) {
        hermes_log(LOG_WARNING, "port", "validate_file_path: null file_path");
        return strdup("{\"error\": \"null file_path\"}");
    }
    /* Check for path traversal */
    if (strstr(file_path, "..")) {
        hermes_log(LOG_WARNING, "port", "validate_file_path: path traversal: %s", file_path);
        return strdup("{\"error\": \"path traversal\"}");
    }
    struct stat st;
    if (stat(file_path, &st) == 0) {
        char *result = malloc(4096);
        if (!result) return NULL;
        snprintf(result, 4096, "{\"valid\": true, \"path\": \"%s\", \"size\": %ld}",
                 file_path, (long)st.st_size);
        return result;
    }
    hermes_log(LOG_DEBUG, "port", "validate_file_path: %s does not exist (OK for create)", file_path);
    return strdup("{\"valid\": true, \"exists\": false}");
}
