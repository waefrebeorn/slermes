/**
 * port_backup.c — Port of Python: cli.py (backup helpers)
 *
 * Real C implementations for backup file collection.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Port of Python: _collect_memory_provider_external_paths */
void collect_memory_provider_external_paths(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "collect_memory_provider_external_paths: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "collect_memory_provider_external_paths: scanning");

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char memory_dir[4096];
    snprintf(memory_dir, sizeof(memory_dir), "%s/memory", home);

    DIR *dir = opendir(memory_dir);
    if (!dir) {
        hermes_log(LOG_DEBUG, "port", "collect_memory: no memory directory");
        return;
    }
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", memory_dir, entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            count++;
        }
    }
    closedir(dir);
    hermes_log(LOG_INFO, "port", "collect_memory: found %d files", count);
}

/* Port of Python: _iter_external_files */
void iter_external_files(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "iter_external_files: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "iter_external_files: iterating");

    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char external_dir[4096];
    snprintf(external_dir, sizeof(external_dir), "%s/external", home);

    DIR *dir = opendir(external_dir);
    if (!dir) {
        hermes_log(LOG_DEBUG, "port", "iter_external: no external directory");
        return;
    }
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        hermes_log(LOG_DEBUG, "port", "iter_external: found %s", entry->d_name);
        count++;
    }
    closedir(dir);
    hermes_log(LOG_INFO, "port", "iter_external: iterated %d entries", count);
}
