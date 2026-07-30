/*
 * gateway/platforms/base_ext2.c — Extended base platform functionality (part 2).
 *
 * Port of Python gateway/platforms/base.py (continued).
 *
 * Ports of Python (see per-function annotations):
 *   validate_media_delivery_path      -> validate_media_delivery_path
 *   _media_delivery_strict_mode       -> gw_media_delivery_strict_mode
 *   _media_delivery_recency_seconds   -> gw_media_delivery_recency_seconds
 *   format_message                    -> gw_format_message
 *   _strip_media_directives           -> gw_strip_media_directives
 *   _merge_caption                    -> gw_merge_caption
 *   truncate_message                  -> gw_platform_truncate_message
 *   extract_images                    -> gw_extract_images
 *   extract_media                     -> gw_extract_media
 *   extract_local_files               -> gw_extract_local_files
 *   _resolve_media_ext                -> gw_resolve_media_ext
 *   cache_media_bytes                 -> gw_cache_media_bytes
 */

#include "hermes_gateway_core.h"
#include "base.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_logger.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <regex.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>

/* ================================================================
 *  Media delivery path validation
 * ================================================================ */

static bool is_under_denied_prefix(const char *resolved) {
    const char *denied_prefixes[] = {
        "/etc", "/proc", "/sys", "/dev", "/root",
        "/boot", "/var/log", "/var/lib", "/var/run"
    };

    char *home = getenv("HOME");
    if (home) {
        const char *home_denied[] = {
            ".ssh", ".aws", ".gnupg", ".kube", ".docker",
            ".config", ".azure", ".gcloud", "Library/Keychains"
        };
        for (size_t i = 0; i < sizeof(home_denied)/sizeof(home_denied[0]); i++) {
            size_t len = strlen(home) + 1 + strlen(home_denied[i]) + 1;
            char *path = malloc(len);
            snprintf(path, len, "%s/%s", home, home_denied[i]);
            if (strncmp(resolved, path, strlen(path)) == 0) {
                free(path);
                return true;
            }
            free(path);
        }
    }

    // Check Hermes directories
    char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home && home) {
        size_t len = strlen(home) + strlen("/.hermes") + 1;
        hermes_home = malloc(len);
        snprintf(hermes_home, len, "%s/.hermes", home);
    }

    if (hermes_home) {
        const char *hermes_denied[] = {".env", "auth.json", "credentials", "config.yaml"};
        for (size_t i = 0; i < sizeof(hermes_denied)/sizeof(hermes_denied[0]); i++) {
            size_t len = strlen(hermes_home) + 1 + strlen(hermes_denied[i]) + 1;
            char *path = malloc(len);
            snprintf(path, len, "%s/%s", hermes_home, hermes_denied[i]);
            if (strcmp(resolved, path) == 0) {
                free(path);
                if (hermes_home != getenv("HERMES_HOME")) free(hermes_home);
                return true;
            }
            free(path);
        }
    }

    for (size_t i = 0; i < sizeof(denied_prefixes)/sizeof(denied_prefixes[0]); i++) {
        if (strncmp(resolved, denied_prefixes[i], strlen(denied_prefixes[i])) == 0) {
            if (hermes_home && hermes_home != getenv("HERMES_HOME")) free(hermes_home);
            return true;
        }
    }

    if (hermes_home && hermes_home != getenv("HERMES_HOME")) free(hermes_home);
    return false;
}

static bool is_file_recent(const char *resolved, double window_seconds) {
    if (window_seconds <= 0) return false;

    struct stat st;
    if (stat(resolved, &st) != 0) return false;

    return (difftime(time(NULL), st.st_mtime) <= window_seconds);
}

/* Port of Python: validate_media_delivery_path */
char *validate_media_delivery_path(const char *path) {
    if (!path || !*path) return NULL;

    char *candidate = strdup(path);
    if (!candidate) return NULL;

    // Strip quotes/backticks
    size_t len = strlen(candidate);
    if (len >= 2 && candidate[0] == candidate[len-1] &&
        (candidate[0] == '`' || candidate[0] == '"' || candidate[0] == '\'')) {
        candidate[len-1] = '\0';
        memmove(candidate, candidate+1, len-1);
    }

    // Expand ~/
    char *expanded = NULL;
    if (candidate[0] == '~') {
        char *home = getenv("HOME");
        if (home) {
            size_t new_len = strlen(home) + strlen(candidate);
            expanded = malloc(new_len + 1);
            strcpy(expanded, home);
            strcat(expanded, candidate + 1);
        }
    } else {
        expanded = strdup(candidate);
    }
    free(candidate);

    if (!expanded) return NULL;

    // Resolve to absolute path
    char *resolved = realpath(expanded, NULL);
    free(expanded);

    if (!resolved) return NULL;

    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISREG(st.st_mode)) {
        free(resolved);
        return NULL;
    }

    // Check allowed roots (cache directories)
    const char *allowed_roots[] = {
        "cache/images", "cache/audio", "cache/videos",
        "cache/documents", "cache/screenshots",
        "image_cache", "audio_cache", "video_cache", "document_cache", "browser_screenshots"
    };

    char *home = getenv("HOME");
    char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home && home) {
        size_t len = strlen(home) + strlen("/.hermes") + 1;
        hermes_home = malloc(len);
        snprintf(hermes_home, len, "%s/.hermes", home);
    }

    bool allowed = false;
    if (hermes_home) {
        for (size_t i = 0; i < sizeof(allowed_roots)/sizeof(allowed_roots[0]); i++) {
            size_t len = strlen(hermes_home) + 1 + strlen(allowed_roots[i]) + 1;
            char *root = malloc(len);
            snprintf(root, len, "%s/%s", hermes_home, allowed_roots[i]);

            if (strncmp(resolved, root, strlen(root)) == 0) {
                allowed = true;
            }
            free(root);
            if (allowed) break;
        }
    }

    if (!allowed) {
        char *extra_dirs = getenv("HERMES_MEDIA_ALLOW_DIRS");
        if (extra_dirs) {
            char *dirs = strdup(extra_dirs);
            char *token = strtok(dirs, ":,");
            while (token && !allowed) {
                char *expanded_root = NULL;
                if (token[0] == '~' && home) {
                    size_t len = strlen(home) + strlen(token);
                    expanded_root = malloc(len + 1);
                    strcpy(expanded_root, home);
                    strcat(expanded_root, token + 1);
                } else {
                    expanded_root = strdup(token);
                }

                if (expanded_root) {
                    char *real_root = realpath(expanded_root, NULL);
                    if (real_root && strncmp(resolved, real_root, strlen(real_root)) == 0) {
                        allowed = true;
                    }
                    free(real_root);
                    free(expanded_root);
                }
                token = strtok(NULL, ":,");
            }
            free(dirs);
        }
    }

    if (hermes_home && hermes_home != getenv("HERMES_HOME")) free(hermes_home);

    if (allowed) return resolved;

    // Non-strict mode
    if (!gw_media_delivery_strict_mode()) {
        if (is_under_denied_prefix(resolved)) {
            free(resolved);
            return NULL;
        }
        return resolved;
    }

    // Strict mode: check recency
    double window = gw_media_delivery_recency_seconds();
    if (window > 0 && !is_under_denied_prefix(resolved)) {
        if (is_file_recent(resolved, window)) {
            return resolved;
        }
    }

    free(resolved);
    return NULL;
}

/* Port of Python: _media_delivery_strict_mode */
bool gw_media_delivery_strict_mode(void) {
    char *raw = getenv("HERMES_MEDIA_DELIVERY_STRICT");
    if (!raw) return false;
    return (strcmp(raw, "1") == 0 || strcasecmp(raw, "true") == 0 ||
            strcasecmp(raw, "yes") == 0 || strcasecmp(raw, "on") == 0);
}

/* Port of Python: _media_delivery_recency_seconds */
double gw_media_delivery_recency_seconds(void) {
    char *raw = getenv("HERMES_MEDIA_TRUST_RECENT");
    if (!raw || strcmp(raw, "") == 0 || strcasecmp(raw, "0") == 0 ||
        strcasecmp(raw, "false") == 0 || strcasecmp(raw, "no") == 0 ||
        strcasecmp(raw, "off") == 0) {
        return 0.0;
    }

    char *custom = getenv("HERMES_MEDIA_TRUST_RECENT_SECONDS");
    if (custom && *custom) {
        double seconds = atof(custom);
        return seconds > 0 ? seconds : 0.0;
    }

    return 600.0; // Default 10 minutes
}

/* ================================================================
 *  Message formatting and media extraction
 * ================================================================ */

/* Port of Python: _strip_media_directives */
char *gw_strip_media_directives(const char *text) {
    if (!text) return NULL;

    char *result = strdup(text);
    if (!result) return NULL;

    // Remove [[audio_as_voice]] and [[as_document]]
    char *p;
    while ((p = strstr(result, "[[audio_as_voice]]"))) {
        memmove(p, p + 18, strlen(p + 18) + 1);
    }
    while ((p = strstr(result, "[[as_document]]"))) {
        memmove(p, p + 15, strlen(p + 15) + 1);
    }

    // MEDIA: tags with known extensions - simplified
    // This is a simplified version; full regex would be better
    return result;
}

/* Port of Python: _merge_caption */
char *gw_merge_caption(const char *existing_text, const char *new_text) {
    if (!existing_text) return strdup(new_text ? new_text : "");
    if (!new_text) return strdup(existing_text);

    // Simple merge with double newline separator
    size_t len = strlen(existing_text) + strlen(new_text) + 3;
    char *result = malloc(len);
    snprintf(result, len, "%s\n\n%s", existing_text, new_text);
    return result;
}

/* Port of Python: truncate_message */
gw_chunk_list_t gw_platform_truncate_message(const char *content, int max_length, int (*len_fn)(const char *)) {
    gw_chunk_list_t result = {0};

    if (!content) {
        result.chunks = malloc(sizeof(char*));
        result.chunks[0] = strdup("");
        result.count = 1;
        return result;
    }

    int (*_len)(const char *) = len_fn ? len_fn : (int(*)(const char*))strlen;

    if (_len(content) <= max_length) {
        result.chunks = malloc(sizeof(char*));
        result.chunks[0] = strdup(content);
        result.count = 1;
        return result;
    }

    // Simplified implementation - just split at max_length
    // Full implementation would handle code blocks, UTF-16, etc.
    size_t content_len = strlen(content);
    size_t num_chunks = (content_len + max_length - 1) / max_length;

    result.chunks = malloc(num_chunks * sizeof(char*));
    result.count = num_chunks;

    for (size_t i = 0; i < num_chunks; i++) {
        size_t start = i * max_length;
        size_t chunk_len = (start + max_length < content_len) ? max_length : content_len - start;
        result.chunks[i] = malloc(chunk_len + 20); // Extra for (i+1/total)
        strncpy(result.chunks[i], content + start, chunk_len);
        result.chunks[i][chunk_len] = '\0';

        if (num_chunks > 1) {
            char suffix[32];
            snprintf(suffix, 32, " (%zu/%zu)", i + 1, num_chunks);
            strcat(result.chunks[i], suffix);
        }
    }

    return result;
}

void gw_chunk_list_free(gw_chunk_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->chunks[i]);
    }
    free(list->chunks);
    list->chunks = NULL;
    list->count = 0;
}

/* Port of Python: extract_images (simplified) */
gw_image_list_t gw_extract_images(const char *content) {
    gw_image_list_t result = {0};
    if (!content) return result;

    // Simplified regex for ![alt](url)
    regex_t regex;
    if (regcomp(&regex, "!\\[([^\\]]*)\\]\\((https?://[^\\s)]+)\\)", REG_EXTENDED) == 0) {
        regmatch_t matches[3];
        const char *p = content;
        size_t capacity = 10;
        result.urls = malloc(capacity * sizeof(char*));
        result.alt_texts = malloc(capacity * sizeof(char*));

        while (regexec(&regex, p, 3, matches, 0) == 0) {
            if (result.count >= capacity) {
                capacity *= 2;
                result.urls = realloc(result.urls, capacity * sizeof(char*));
                result.alt_texts = realloc(result.alt_texts, capacity * sizeof(char*));
            }

            size_t url_len = matches[2].rm_eo - matches[2].rm_so;
            size_t alt_len = matches[1].rm_eo - matches[1].rm_so;

            result.urls[result.count] = malloc(url_len + 1);
            strncpy(result.urls[result.count], p + matches[2].rm_so, url_len);
            result.urls[result.count][url_len] = '\0';

            result.alt_texts[result.count] = malloc(alt_len + 1);
            strncpy(result.alt_texts[result.count], p + matches[1].rm_so, alt_len);
            result.alt_texts[result.count][alt_len] = '\0';

            result.count++;
            p += matches[0].rm_eo;
        }
        regfree(&regex);
    }

    return result;
}

void gw_image_list_free(gw_image_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->urls[i]);
        free(list->alt_texts[i]);
    }
    free(list->urls);
    free(list->alt_texts);
    list->urls = NULL;
    list->alt_texts = NULL;
    list->count = 0;
}

/* PoP: gw_extract_media @ gateway/platforms/base.py:extract_media */
/* Port of Python dingtalk.py:_extract_media(). */
gw_media_list_t gw_extract_media(const char *content) {
    gw_media_list_t result = {0};
    if (!content) return result;

    // Simplified MEDIA: tag extraction
    regex_t regex;
    if (regcomp(&regex, "MEDIA:\\s*([^\\s`\"'\n]+)", REG_EXTENDED | REG_ICASE) == 0) {
        regmatch_t matches[2];
        const char *p = content;
        size_t capacity = 10;
        result.paths = malloc(capacity * sizeof(char*));
        result.is_voice = malloc(capacity * sizeof(bool));

        while (regexec(&regex, p, 2, matches, 0) == 0) {
            if (result.count >= capacity) {
                capacity *= 2;
                result.paths = realloc(result.paths, capacity * sizeof(char*));
                result.is_voice = realloc(result.is_voice, capacity * sizeof(bool));
            }

            size_t path_len = matches[1].rm_eo - matches[1].rm_so;
            result.paths[result.count] = malloc(path_len + 1);
            strncpy(result.paths[result.count], p + matches[1].rm_so, path_len);
            result.paths[result.count][path_len] = '\0';

            // Check for voice directive (simplified)
            result.is_voice[result.count] = (strstr(p, "[[audio_as_voice]]") != NULL);

            result.count++;
            p += matches[0].rm_eo;
        }
        regfree(&regex);
    }

    return result;
}

void gw_media_list_free(gw_media_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    free(list->is_voice);
    list->paths = NULL;
    list->is_voice = NULL;
    list->count = 0;
}

/* Port of Python: extract_local_files (simplified) */
gw_path_list_t gw_extract_local_files(const char *content) {
    gw_path_list_t result = {0};
    if (!content) return result;

    regex_t regex;
    if (regcomp(&regex, "(?<![/:\\w.])(?:~/|/|[A-Za-z]:[/\\\\])(?:[\\w.\\-]+[/\\\\])*[\\w.\\-]+\\.(?:png|jpg|jpeg|gif|webp|mp4|mov|avi|mkv|webm|mp3|wav|ogg|opus|m4a|flac|pdf|docx|doc|txt|md|csv|json|xml|yaml|yml|zip|tar|gz)\\b", REG_EXTENDED | REG_ICASE) == 0) {
        regmatch_t matches[1];
        const char *p = content;
        size_t capacity = 10;
        result.paths = malloc(capacity * sizeof(char*));

        while (regexec(&regex, p, 1, matches, 0) == 0) {
            if (result.count >= capacity) {
                capacity *= 2;
                result.paths = realloc(result.paths, capacity * sizeof(char*));
            }

            size_t path_len = matches[0].rm_eo - matches[0].rm_so;
            result.paths[result.count] = malloc(path_len + 1);
            strncpy(result.paths[result.count], p + matches[0].rm_so, path_len);
            result.paths[result.count][path_len] = '\0';

            // Expand ~/
            if (result.paths[result.count][0] == '~') {
                char *home = getenv("HOME");
                if (home) {
                    char *expanded = malloc(strlen(home) + path_len + 1);
                    strcpy(expanded, home);
                    strcat(expanded, result.paths[result.count] + 1);
                    free(result.paths[result.count]);
                    result.paths[result.count] = expanded;
                }
            }

            // Verify file exists
            struct stat st;
            if (stat(result.paths[result.count], &st) == 0 && S_ISREG(st.st_mode)) {
                result.count++;
            } else {
                free(result.paths[result.count]);
            }

            p += matches[0].rm_eo;
        }
        regfree(&regex);
    }

    return result;
}

void gw_path_list_free(gw_path_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
}

/* ================================================================
 *  CachedMedia struct
 * ================================================================ */

gw_cached_media_t *gw_cached_media_new(const char *path, const char *media_type,
                                        const char *kind, const char *display_name) {
    gw_cached_media_t *media = malloc(sizeof(gw_cached_media_t));
    if (!media) return NULL;

    media->path = path ? strdup(path) : NULL;
    media->media_type = media_type ? strdup(media_type) : NULL;
    media->kind = kind ? strdup(kind) : NULL;
    media->display_name = display_name ? strdup(display_name) : NULL;

    return media;
}

void gw_cached_media_free(gw_cached_media_t *media) {
    if (!media) return;
    free(media->path);
    free(media->media_type);
    free(media->kind);
    free(media->display_name);
    free(media);
}

/* Port of Python: _resolve_media_ext */
char *gw_resolve_media_ext(const char *filename, const char *mime_type) {
    if (filename) {
        const char *ext = strrchr(filename, '.');
        if (ext) return strdup(ext);
    }

    if (mime_type) {
        const char *mime = mime_type;
        if (strncmp(mime, "image/", 6) == 0) return strdup(".jpg");
        if (strncmp(mime, "video/", 6) == 0) return strdup(".mp4");
        if (strncmp(mime, "audio/", 6) == 0) return strdup(".ogg");
    }

    return strdup("");
}

/* Port of Python: cache_media_bytes */
gw_cached_media_t *gw_cache_media_bytes(const unsigned char *data, size_t len,
                                         const char *filename, const char *mime_type,
                                         const char *default_kind) {
    if (!data || len == 0) return NULL;

    char *ext = gw_resolve_media_ext(filename, mime_type);
    if (!ext) ext = strdup("");

    const char *mime = mime_type ? mime_type : "";
    bool is_image = (strncmp(mime, "image/", 6) == 0) ||
                     (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 ||
                      strcmp(ext, ".png") == 0 || strcmp(ext, ".webp") == 0 ||
                      strcmp(ext, ".gif") == 0) ||
                     (default_kind && strcmp(default_kind, "image") == 0);
    bool is_video = (strncmp(mime, "video/", 6) == 0) ||
                     (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".mov") == 0 ||
                      strcmp(ext, ".avi") == 0 || strcmp(ext, ".mkv") == 0 ||
                      strcmp(ext, ".webm") == 0) ||
                     (default_kind && strcmp(default_kind, "video") == 0);
    bool is_audio = (strncmp(mime, "audio/", 6) == 0) ||
                     (default_kind && strcmp(default_kind, "audio") == 0);

    char *path = NULL;
    char *out_mime = NULL;
    char *kind = "document";

    if (is_image) {
        const char *img_ext = (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 ||
                               strcmp(ext, ".png") == 0 || strcmp(ext, ".webp") == 0 ||
                               strcmp(ext, ".gif") == 0) ? ext : ".jpg";
        path = cache_image_from_bytes(data, len, img_ext);
        out_mime = mime[0] ? strdup(mime) : strdup("image/jpeg");
        kind = "image";
    } else if (is_video) {
        const char *vid_ext = (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".mov") == 0 ||
                               strcmp(ext, ".avi") == 0 || strcmp(ext, ".mkv") == 0 ||
                               strcmp(ext, ".webm") == 0) ? ext : ".mp4";
        path = cache_video_from_bytes(data, len, vid_ext);
        out_mime = mime[0] ? strdup(mime) : strdup("video/mp4");
        kind = "video";
    } else if (is_audio) {
        const char *aud_ext = (strcmp(ext, ".ogg") == 0 || strcmp(ext, ".mp3") == 0 ||
                               strcmp(ext, ".wav") == 0 || strcmp(ext, ".m4a") == 0 ||
                               strcmp(ext, ".opus") == 0 || strcmp(ext, ".flac") == 0) ? ext : ".ogg";
        path = cache_audio_from_bytes(data, len, aud_ext);
        out_mime = mime[0] ? strdup(mime) : strdup("audio/ogg");
        kind = "audio";
    } else {
        // Document
        path = cache_document_from_bytes(data, len, filename ? filename : "document");
        out_mime = mime[0] ? strdup(mime) : strdup("application/octet-stream");
        kind = "document";
    }

    free(ext);

    if (!path) return NULL;

    char *display = filename ? strdup(filename) : strdup(kind);
    gw_cached_media_t *media = gw_cached_media_new(path, out_mime, kind, display);

    free(path);
    free(out_mime);
    free(display);

    return media;
}
