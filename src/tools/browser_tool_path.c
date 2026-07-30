/*
 * browser_tool_path.c — focused concern module extracted from
 * port_browser_tool.c (refactor-first monolith split). Port of
 * tools/browser_tool.py. Self-contained, opaque struct, minimal includes.
 */

#include "browser_tool_path.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <dirent.h>

struct browser_tool_path {
    int unused;
};

browser_tool_path_t *browser_tool_path_init(void) { return calloc(1, sizeof(browser_tool_path_t)); }
void browser_tool_path_cleanup(browser_tool_path_t *s) { free(s); }

/* PoP: _discover_homebrew_node_dirs @ tools/browser_tool.py:_discover_homebrew_node_dirs */
char *browser_discover_homebrew_node_dirs(void)
{
    /* Find Homebrew versioned Node.js bin directories */
    char *result = malloc(4096);
    if (!result) return NULL;
    result[0] = '\0';

    const char *homebrew_opt = "/opt/homebrew/opt";
    DIR *dir = opendir(homebrew_opt);
    if (dir) {
        struct dirent *entry;
        bool first = true;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "node", 4) == 0 && strcmp(entry->d_name, "node") != 0) {
                char bin_dir[4096];
                snprintf(bin_dir, sizeof(bin_dir), "%s/%s/bin", homebrew_opt, entry->d_name);
                if (access(bin_dir, F_OK) == 0) {
                    if (!first) strcat(result, ":");
                    strcat(result, bin_dir);
                    first = false;
                }
            }
        }
        closedir(dir);
    }
    return result;
}

/* PoP: _browser_candidate_path_dirs @ tools/browser_tool.py:_browser_candidate_path_dirs */
char *browser_browser_candidate_path_dirs(void)
{
    /* Return ordered browser CLI PATH candidates */
    const char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home) hermes_home = "/tmp/.hermes";

    char *result = malloc(8192);
    if (!result) return NULL;
    result[0] = '\0';

    char *discover = browser_discover_homebrew_node_dirs();
    if (discover && discover[0]) {
        strcat(result, discover);
        strcat(result, ":");
    }
    free(discover);

    /* Add standard paths */
    strcat(result, "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
    return result;
}

/* PoP: _merge_browser_path @ tools/browser_tool.py:_merge_browser_path */
char *browser_merge_browser_path(const char *existing_path)
{
    /* Prepend browser-specific PATH fallbacks without reordering existing entries */
    if (!existing_path) existing_path = "";

    char *candidates = browser_browser_candidate_path_dirs();
    if (!candidates) return strdup(existing_path);

    size_t cap = strlen(candidates) + strlen(existing_path) + 256;
    char *result = malloc(cap);
    if (!result) {
        free(candidates);
        return strdup(existing_path);
    }
    result[0] = '\0';

    /* Track existing path parts */
    char *existing_parts[256];
    int existing_count = 0;
    char *existing_copy = strdup(existing_path);
    if (existing_copy) {
        char *tok = strtok(existing_copy, ":");
        while (tok && existing_count < 256) {
            existing_parts[existing_count++] = strdup(tok);
            tok = strtok(NULL, ":");
        }
    }

    /* Add candidate directories that aren't already in PATH */
    char *cand_copy = strdup(candidates);
    char *tok = strtok(cand_copy, ":");
    while (tok) {
        bool found = false;
        for (int i = 0; i < existing_count; i++) {
            if (strcmp(tok, existing_parts[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found && access(tok, F_OK) == 0) {
            if (result[0]) strcat(result, ":");
            strcat(result, tok);
        }
        tok = strtok(NULL, ":");
    }
    free(cand_copy);

    /* Append existing PATH */
    if (existing_path[0]) {
        if (result[0]) strcat(result, ":");
        strcat(result, existing_path);
    }

    /* Cleanup */
    for (int i = 0; i < existing_count; i++) free(existing_parts[i]);
    free(existing_copy);
    free(candidates);

    return result;
}

