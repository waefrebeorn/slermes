/*
 * sticker_cache.c — Telegram sticker description cache.
 *
 * Port of Python gateway/sticker_cache.py.
 *
 * When users send stickers, we describe them via the vision tool and cache
 * the descriptions keyed by file_unique_id so we don't re-analyze the same
 * sticker image on every send. Descriptions are concise (1-2 sentences).
 *
 * Cache location: ~/.hermes/sticker_cache.json
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

/* ================================================================
 *  Internal: get cache file path
 *  Port of Python CACHE_PATH
 * ================================================================ */

static const char *sticker_cache_path(void) {
    static char path[1024] = {0};
    if (path[0] == '\0') {
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            snprintf(path, sizeof(path), "%s/sticker_cache.json", home);
        }
    }
    return path[0] ? path : NULL;
}

/* ================================================================
 *  Internal: load sticker cache from disk
 *  Port of Python _load_cache()
 * ================================================================ */

static json_node_t *sticker_cache_load(void) {
    const char *path = sticker_cache_path();
    if (!path) return json_new_object();

    char *err = NULL;
    json_node_t *root = json_parse_file(path, &err);
    if (!root) {
        free(err);
        return json_new_object();
    }
    return root;
}

/* ================================================================
 *  Internal: save sticker cache to disk atomically
 *  Port of Python _save_cache()
 * ================================================================ */

static bool sticker_cache_save(json_node_t *cache) {
    const char *path = sticker_cache_path();
    if (!path || !cache) return false;

    /* Build the JSON string */
    char *json_str = json_serialize_pretty(cache, 2);
    if (!json_str) return false;

    /* Write to temp file atomically */
    char tmp_path[1060];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.XXXXXX", path);

    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(json_str);
        return false;
    }

    size_t len = strlen(json_str);
    ssize_t written = write(fd, json_str, len);
    if (written < 0 || (size_t)written != len) {
        close(fd);
        unlink(tmp_path);
        free(json_str);
        return false;
    }
    fsync(fd);
    close(fd);

    /* Atomic rename */
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        free(json_str);
        return false;
    }

    free(json_str);
    return true;
}

/* ================================================================
 *  Look up a cached sticker description
 *  Port of Python get_cached_description()
 *
 *  Returns a json_node_t with keys {description, emoji, set_name,
 *  cached_at} or NULL. Caller must json_free() the result.
 * ================================================================ */

json_node_t *sticker_cache_get(const char *file_unique_id) {
    if (!file_unique_id || !*file_unique_id) return NULL;

    json_node_t *cache = sticker_cache_load();
    if (!cache) return NULL;

    json_node_t *entry = json_object_get(cache, file_unique_id);
    json_node_t *result = NULL;
    if (entry) {
        /* Return a copy */
        result = json_copy(entry);
    }

    json_free(cache);
    return result;
}

/* ================================================================
 *  Store a sticker description in the cache
 *  Port of Python cache_sticker_description()
 * ================================================================ */

bool sticker_cache_set(const char *file_unique_id,
                        const char *description,
                        const char *emoji,
                        const char *set_name) {
    if (!file_unique_id || !*file_unique_id || !description) return false;

    json_node_t *cache = sticker_cache_load();
    if (!cache) return false;

    json_node_t *entry = json_new_object();
    json_object_set(entry, "description", json_new_string(description));
    json_object_set(entry, "emoji", json_new_string(emoji ? emoji : ""));
    json_object_set(entry, "set_name", json_new_string(set_name ? set_name : ""));
    json_object_set(entry, "cached_at", json_new_number((double)time(NULL)));

    json_object_set(cache, file_unique_id, entry);

    bool ok = sticker_cache_save(cache);
    json_free(cache);
    return ok;
}

/* ================================================================
 *  Build warm-style injection text for a sticker description
 *  Port of Python build_sticker_injection()
 *
 *  Returns a malloc'd string like:
 *    [The user sent a sticker 😀 from "MyPack"~ It shows: "A cat waving" (=^.w.^=)]
 *  Caller must free().
 * ================================================================ */

char *sticker_build_injection(const char *description,
                               const char *emoji,
                               const char *set_name) {
    if (!description) return strdup("");

    char buf[2048];
    if (set_name && *set_name && emoji && *emoji) {
        snprintf(buf, sizeof(buf),
                 "[The user sent a sticker %s from \"%s\"~ It shows: \"%s\" (=^.w.^=)]",
                 emoji, set_name, description);
    } else if (emoji && *emoji) {
        snprintf(buf, sizeof(buf),
                 "[The user sent a sticker %s~ It shows: \"%s\" (=^.w.^=)]",
                 emoji, description);
    } else {
        snprintf(buf, sizeof(buf),
                 "[The user sent a sticker~ It shows: \"%s\" (=^.w.^=)]",
                 description);
    }

    return strdup(buf);
}

/* ================================================================
 *  Build injection text for animated/video stickers we can't analyze
 *  Port of Python build_animated_sticker_injection()
 *
 *  Returns a malloc'd string, caller must free().
 * ================================================================ */

char *sticker_build_animated_injection(const char *emoji) {
    char buf[512];
    if (emoji && *emoji) {
        snprintf(buf, sizeof(buf),
                 "[The user sent an animated sticker %s~ "
                 "I can't see animated ones yet, but the emoji suggests: %s]",
                 emoji, emoji);
    } else {
        snprintf(buf, sizeof(buf),
                 "[The user sent an animated sticker~ I can't see animated ones yet]");
    }
    return strdup(buf);
}
