/*
 * port_agent_model_metadata.c — C port of agent/model_metadata.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* PoP: cli_agent_model_metadata__get_model_metadata_cache_path @ agent/model_metadata.py:_get_model_metadata_cache_path */

/* Port of Python agent/model_metadata.py:_get_model_metadata_cache_path */
/* Returns the path to the model metadata disk cache. */
int cli_agent_model_metadata__get_model_metadata_cache_path(
    const char *hermes_home, char *output, size_t output_size)
{
    if (!hermes_home || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "%s/model_metadata_cache.json", hermes_home);
    return 0;
}

/* PoP: cli_agent_model_metadata__model_metadata_disk_cache_age_seconds @ agent/model_metadata.py:_model_metadata_disk_cache_age_seconds */

/* Port of Python agent/model_metadata.py:_model_metadata_disk_cache_age_seconds */
/* Returns the age of the model metadata disk cache in seconds. */
int cli_agent_model_metadata__model_metadata_disk_cache_age_seconds(
    const char *cache_path)
{
    if (!cache_path) {
        return -1;
    }
    struct stat st;
    if (stat(cache_path, &st) != 0) {
        return -1;  /* file doesn't exist */
    }
    time_t now = time(NULL);
    return (int)(now - st.st_mtime);
}

/* PoP: cli_agent_model_metadata__load_model_metadata_disk_cache @ agent/model_metadata.py:_load_model_metadata_disk_cache */

/* Port of Python agent/model_metadata.py:_load_model_metadata_disk_cache */
/* Loads model metadata from disk cache. Returns number of entries loaded. */
int cli_agent_model_metadata__load_model_metadata_disk_cache(
    const char *cache_path, char *entries[], int max_entries)
{
    if (!cache_path || !entries || max_entries <= 0) {
        return 0;
    }
    FILE *f = fopen(cache_path, "r");
    if (!f) {
        return 0;
    }
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < max_entries) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        entries[count] = strdup(line);
        if (entries[count]) count++;
    }
    fclose(f);
    return count;
}

/* PoP: cli_agent_model_metadata__save_model_metadata_disk_cache @ agent/model_metadata.py:_save_model_metadata_disk_cache */

/* Port of Python agent/model_metadata.py:_save_model_metadata_disk_cache */
/* Saves model metadata to disk cache. */
int cli_agent_model_metadata__save_model_metadata_disk_cache(
    const char *cache_path, const char *entries[], int count)
{
    if (!cache_path || count <= 0) {
        return -1;
    }
    FILE *f = fopen(cache_path, "w");
    if (!f) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (entries[i]) {
            fprintf(f, "%s\n", entries[i]);
        }
    }
    fclose(f);
    return 0;
}
