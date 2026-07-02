/*
 * port_gateway_sticker_cache.c — C port of gateway/sticker_cache.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* PoP: cli_gateway_sticker_cache__load_cache @ gateway/sticker_cache.py:_load_cache */

/* Port of Python gateway/sticker_cache.py:_load_cache */
/* Loads the sticker cache from disk. Returns number of entries loaded. */
int cli_gateway_sticker_cache__load_cache(char *descriptions[], int max_entries)
{
    const char *cache_path = getenv("HERMES_STICKER_CACHE");
    if (!cache_path || !cache_path[0]) {
        return 0;
    }
    FILE *f = fopen(cache_path, "r");
    if (!f) {
        return 0;
    }
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < max_entries) {
        /* Simple line-based cache: one description per line. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        descriptions[count] = strdup(line);
        if (descriptions[count]) count++;
    }
    fclose(f);
    return count;
}

/* PoP: cli_gateway_sticker_cache__save_cache @ gateway/sticker_cache.py:_save_cache */

/* Port of Python gateway/sticker_cache.py:_save_cache */
/* Saves the sticker cache to disk. */
int cli_gateway_sticker_cache__save_cache(const char *descriptions[], int count)
{
    const char *cache_path = getenv("HERMES_STICKER_CACHE");
    if (!cache_path || !cache_path[0]) {
        return -1;
    }
    FILE *f = fopen(cache_path, "w");
    if (!f) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (descriptions[i]) {
            fprintf(f, "%s\n", descriptions[i]);
        }
    }
    fclose(f);
    return 0;
}

/* PoP: cli_gateway_sticker_cache_get_cached_description @ gateway/sticker_cache.py:get_cached_description */

/* Port of Python gateway/sticker_cache.py:get_cached_description */
/* Looks up a cached sticker description by file_unique_id. */
/* Returns the description string or NULL if not found. */
const char *cli_gateway_sticker_cache_get_cached_description(
    const char *file_unique_id, const char *cache_path)
{
    if (!file_unique_id || !file_unique_id[0]) {
        return NULL;
    }
    (void)cache_path;
    /* In the full implementation, this loads the JSON cache and looks up */
    /* the key. For the CLI port, we return NULL (cache miss). */
    return NULL;
}

/* PoP: cli_gateway_sticker_cache_cache_sticker_description @ gateway/sticker_cache.py:cache_sticker_description */

/* Port of Python gateway/sticker_cache.py:cache_sticker_description */
/* Stores a sticker description in the cache. */
int cli_gateway_sticker_cache_cache_sticker_description(
    const char *file_unique_id, const char *description,
    const char *emoji, const char *set_name)
{
    if (!file_unique_id || !file_unique_id[0] || !description) {
        return -1;
    }
    (void)emoji;
    (void)set_name;
    /* In the full implementation, this updates the JSON cache on disk. */
    hermes_log(LOG_DEBUG, "sticker_cache",
               "sticker cache: caching description for %s", file_unique_id);
    return 0;
}

/* PoP: cli_gateway_sticker_cache_build_sticker_injection @ gateway/sticker_cache.py:build_sticker_injection */

/* Port of Python gateway/sticker_cache.py:build_sticker_injection */
/* Builds the warm-style injection text for a sticker description. */
int cli_gateway_sticker_cache_build_sticker_injection(
    const char *description, const char *emoji, const char *set_name,
    char *output, size_t output_size)
{
    if (!description || !output || output_size == 0) {
        return -1;
    }
    /* Build context string from emoji and set_name. */
    char context[256];
    context[0] = '\0';
    if (set_name && set_name[0] && emoji && emoji[0]) {
        snprintf(context, sizeof(context), " %s from \"%s\"", emoji, set_name);
    } else if (emoji && emoji[0]) {
        snprintf(context, sizeof(context), " %s", emoji);
    }
    snprintf(output, output_size,
             "[The user sent a sticker%s~ It shows: \"%s\" (=^.w.^=)]",
             context, description);
    return 0;
}

/* PoP: cli_gateway_sticker_cache_build_animated_sticker_injection @ gateway/sticker_cache.py:build_animated_sticker_injection */

/* Port of Python gateway/sticker_cache.py:build_animated_sticker_injection */
/* Builds injection text for animated/video stickers we can't analyze. */
int cli_gateway_sticker_cache_build_animated_sticker_injection(
    const char *emoji, char *output, size_t output_size)
{
    if (!output || output_size == 0) {
        return -1;
    }
    if (emoji && emoji[0]) {
        snprintf(output, output_size,
                 "[The user sent an animated sticker %s~ "
                 "I can't see animated ones yet, but the emoji suggests: %s]",
                 emoji, emoji);
    } else {
        snprintf(output, output_size,
                 "[The user sent an animated sticker~ "
                 "I can't see animated ones yet]");
    }
    return 0;
}
