/*
 * gateway/platforms/base_ext.c — Extended base platform functionality.
 *
 * Port of Python gateway/platforms/base.py (continued).
 *
 * Ports of Python (see per-function annotations):
 *   _looks_like_image                  -> looks_like_image
 *   _looks_like_image (bytes variant)  -> looks_like_image_bytes
 *   cache_image_from_bytes             -> cache_image_from_bytes
 *   cache_audio_from_bytes             -> cache_audio_from_bytes
 *   cache_video_from_bytes             -> cache_video_from_bytes
 *   cache_document_from_bytes          -> cache_document_from_bytes
 *   cleanup_image_cache                -> cleanup_image_cache
 *   cleanup_audio_cache                -> cleanup_audio_cache
 *   cleanup_video_cache                -> cleanup_video_cache
 *   cleanup_document_cache             -> cleanup_document_cache
 *   build_source                       -> gw_build_source
 *   _detect_macos_system_proxy         -> detect_macos_system_proxy
 *   _split_host_port                   -> gw_split_host_port
 *   resolve_proxy_url                  -> resolve_proxy_url
 *   should_bypass_proxy                -> should_bypass_proxy
 *   proxy_kwargs_for_aiohttp           -> proxy_kwargs_for_aiohttp
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

/* ================================================================
 *  Media cache helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: _looks_like_image (bytes variant) */
bool looks_like_image_bytes(const unsigned char *data, size_t len) {
    if (!data || len < 4) return false;

    if (len >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G' &&
        data[4] == '\r' && data[5] == '\n' && data[6] == 0x1a && data[7] == '\n') {
        return true;  // PNG
    }
    if (len >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff) {
        return true;  // JPEG
    }
    if (len >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) {
        return true;  // GIF
    }
    if (len >= 2 && data[0] == 'B' && data[1] == 'M') {
        return true;  // BMP
    }
    if (len >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
        return true;  // WebP
    }

    return false;
}

/* Helper: get cache directory */
static char *get_cache_dir(const char *subdir, const char *legacy_subdir) {
    char *home = getenv("HOME");
    if (!home) home = ".";

    char *hermes_home = getenv("HERMES_HOME");
    if (!hermes_home) {
        size_t len = strlen(home) + strlen("/.hermes") + 1;
        hermes_home = malloc(len);
        snprintf(hermes_home, len, "%s/.hermes", home);
    }

    size_t len = strlen(hermes_home) + strlen("/cache/") + strlen(subdir) + 1;
    char *path = malloc(len);
    snprintf(path, len, "%s/cache/%s", hermes_home, subdir);

    mkdir(path, 0755);
    return path;
}

/* Port of Python: gateway.platforms.base.cache_image_from_bytes */
char *cache_image_from_bytes(const unsigned char *data, size_t len, const char *ext) {
    if (!looks_like_image_bytes(data, len)) {
        return NULL;
    }

    char *cache_dir = get_cache_dir("images", "image_cache");
    if (!ext) ext = ".jpg";

    // Generate UUID-like name
    char uuid_str[13];
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        unsigned char bytes[6];
        fread(bytes, 1, 6, urandom);
        fclose(urandom);
        for (int i = 0; i < 6; i++) {
            snprintf(uuid_str + i * 2, 3, "%02x", bytes[i]);
        }
    } else {
        snprintf(uuid_str, 13, "%lx", (unsigned long)time(NULL));
    }

    size_t path_len = strlen(cache_dir) + strlen("/img_") + 12 + strlen(ext) + 1;
    char *path = malloc(path_len);
    snprintf(path, path_len, "%s/img_%s%s", cache_dir, uuid_str, ext);

    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    } else {
        free(path);
        path = NULL;
    }

    free(cache_dir);
    return path;
}

/* Port of Python: gateway/platforms.base.cache_audio_from_bytes */
char *cache_audio_from_bytes(const unsigned char *data, size_t len, const char *ext) {
    char *cache_dir = get_cache_dir("audio", "audio_cache");
    if (!ext) ext = ".ogg";

    char uuid_str[13];
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        unsigned char bytes[6];
        fread(bytes, 1, 6, urandom);
        fclose(urandom);
        for (int i = 0; i < 6; i++) {
            snprintf(uuid_str + i * 2, 3, "%02x", bytes[i]);
        }
    } else {
        snprintf(uuid_str, 13, "%lx", (unsigned long)time(NULL));
    }

    size_t path_len = strlen(cache_dir) + strlen("/audio_") + 12 + strlen(ext) + 1;
    char *path = malloc(path_len);
    snprintf(path, path_len, "%s/audio_%s%s", cache_dir, uuid_str, ext);

    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    } else {
        free(path);
        path = NULL;
    }

    free(cache_dir);
    return path;
}

/* Port of Python: gateway.platforms.base.cache_video_from_bytes */
char *cache_video_from_bytes(const unsigned char *data, size_t len, const char *ext) {
    char *cache_dir = get_cache_dir("videos", "video_cache");
    if (!ext) ext = ".mp4";

    char uuid_str[13];
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        unsigned char bytes[6];
        fread(bytes, 1, 6, urandom);
        fclose(urandom);
        for (int i = 0; i < 6; i++) {
            snprintf(uuid_str + i * 2, 3, "%02x", bytes[i]);
        }
    } else {
        snprintf(uuid_str, 13, "%lx", (unsigned long)time(NULL));
    }

    size_t path_len = strlen(cache_dir) + strlen("/video_") + 12 + strlen(ext) + 1;
    char *path = malloc(path_len);
    snprintf(path, path_len, "%s/video_%s%s", cache_dir, uuid_str, ext);

    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    } else {
        free(path);
        path = NULL;
    }

    free(cache_dir);
    return path;
}

/* Port of Python: gateway.platforms.base.cache_document_from_bytes */
char *cache_document_from_bytes(const unsigned char *data, size_t len, const char *filename) {
    char *cache_dir = get_cache_dir("documents", "document_cache");
    if (!filename) filename = "document";

    // Extract filename only (no path)
    const char *base = strrchr(filename, '/');
    if (base) base++; else base = filename;

    char uuid_str[13];
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        unsigned char bytes[6];
        fread(bytes, 1, 6, urandom);
        fclose(urandom);
        for (int i = 0; i < 6; i++) {
            snprintf(uuid_str + i * 2, 3, "%02x", bytes[i]);
        }
    } else {
        snprintf(uuid_str, 13, "%lx", (unsigned long)time(NULL));
    }

    size_t path_len = strlen(cache_dir) + strlen("/doc_") + 12 + strlen("_") + strlen(base) + 1;
    char *path = malloc(path_len);
    snprintf(path, path_len, "%s/doc_%s_%s", cache_dir, uuid_str, base);

    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, len, f);
        fclose(f);
    } else {
        free(path);
        path = NULL;
    }

    free(cache_dir);
    return path;
}

/* Port of Python: gateway.platforms.base.cleanup_image_cache */
int cleanup_image_cache(int max_age_hours) {
    char *cache_dir = get_cache_dir("images", "image_cache");
    int removed = 0;

    DIR *d = opendir(cache_dir);
    if (d) {
        struct dirent *entry;
        time_t cutoff = time(NULL) - (max_age_hours * 3600);
        while ((entry = readdir(d))) {
            if (entry->d_type == DT_REG) {
                size_t path_len = strlen(cache_dir) + 1 + strlen(entry->d_name) + 1;
                char *path = malloc(path_len);
                snprintf(path, path_len, "%s/%s", cache_dir, entry->d_name);
                struct stat st;
                if (stat(path, &st) == 0 && st.st_mtime < cutoff) {
                    if (unlink(path) == 0) removed++;
                }
                free(path);
            }
        }
        closedir(d);
    }

    free(cache_dir);
    return removed;
}

/* PoP: cleanup_audio_cache @ gateway/platforms/base.py:cleanup_audio_cache */
int cleanup_audio_cache(int max_age_hours) {
    char *cache_dir = get_cache_dir("audio", "audio_cache");
    int removed = 0;

    DIR *d = opendir(cache_dir);
    if (d) {
        struct dirent *entry;
        time_t cutoff = time(NULL) - (max_age_hours * 3600);
        while ((entry = readdir(d))) {
            if (entry->d_type == DT_REG) {
                size_t path_len = strlen(cache_dir) + 1 + strlen(entry->d_name) + 1;
                char *path = malloc(path_len);
                snprintf(path, path_len, "%s/%s", cache_dir, entry->d_name);
                struct stat st;
                if (stat(path, &st) == 0 && st.st_mtime < cutoff) {
                    if (unlink(path) == 0) removed++;
                }
                free(path);
            }
        }
        closedir(d);
    }

    free(cache_dir);
    return removed;
}

/* PoP: cleanup_video_cache @ gateway/platforms/base.py:cleanup_video_cache */
int cleanup_video_cache(int max_age_hours) {
    char *cache_dir = get_cache_dir("videos", "video_cache");
    int removed = 0;

    DIR *d = opendir(cache_dir);
    if (d) {
        struct dirent *entry;
        time_t cutoff = time(NULL) - (max_age_hours * 3600);
        while ((entry = readdir(d))) {
            if (entry->d_type == DT_REG) {
                size_t path_len = strlen(cache_dir) + 1 + strlen(entry->d_name) + 1;
                char *path = malloc(path_len);
                snprintf(path, path_len, "%s/%s", cache_dir, entry->d_name);
                struct stat st;
                if (stat(path, &st) == 0 && st.st_mtime < cutoff) {
                    if (unlink(path) == 0) removed++;
                }
                free(path);
            }
        }
        closedir(d);
    }

    free(cache_dir);
    return removed;
}

/* Port of Python: gateway.platforms.base.cleanup_document_cache */
int cleanup_document_cache(int max_age_hours) {
    char *cache_dir = get_cache_dir("documents", "document_cache");
    int removed = 0;

    DIR *d = opendir(cache_dir);
    if (d) {
        struct dirent *entry;
        time_t cutoff = time(NULL) - (max_age_hours * 3600);
        while ((entry = readdir(d))) {
            if (entry->d_type == DT_REG) {
                size_t path_len = strlen(cache_dir) + 1 + strlen(entry->d_name) + 1;
                char *path = malloc(path_len);
                snprintf(path, path_len, "%s/%s", cache_dir, entry->d_name);
                struct stat st;
                if (stat(path, &st) == 0 && st.st_mtime < cutoff) {
                    if (unlink(path) == 0) removed++;
                }
                free(path);
            }
        }
        closedir(d);
    }

    free(cache_dir);
    return removed;
}

/* PoP: get_screenshot_cache_dir @ gateway/platforms/base.py:get_screenshot_cache_dir */
char *get_screenshot_cache_dir(void) {
    return get_cache_dir("screenshots", "browser_screenshots");
}

/* PoP: cleanup_screenshot_cache @ gateway/platforms/base.py:cleanup_screenshot_cache */
int cleanup_screenshot_cache(int max_age_hours) {
    char *cache_dir = get_screenshot_cache_dir();
    int removed = 0;

    DIR *d = opendir(cache_dir);
    if (d) {
        struct dirent *entry;
        time_t cutoff = time(NULL) - (max_age_hours * 3600);
        while ((entry = readdir(d))) {
            if (entry->d_type == DT_REG) {
                size_t path_len = strlen(cache_dir) + 1 + strlen(entry->d_name) + 1;
                char *path = malloc(path_len);
                snprintf(path, path_len, "%s/%s", cache_dir, entry->d_name);
                struct stat st;
                if (stat(path, &st) == 0 && st.st_mtime < cutoff) {
                    if (unlink(path) == 0) removed++;
                }
                free(path);
            }
        }
        closedir(d);
    }

    free(cache_dir);
    return removed;
}

/* ================================================================
 *  Session/source helpers
 * ================================================================ */

/* ================================================================
 *  Proxy helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: gateway.platforms.base._detect_macos_system_proxy */
char *detect_macos_system_proxy(void) {
#if defined(__APPLE__) || defined(__MACH__)
    FILE *fp = popen("scutil --proxy 2>/dev/null", "r");
    if (!fp) return NULL;

    char line[256];
    char *props = malloc(4096);
    props[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        strncat(props, line, 4095 - strlen(props));
    }
    pclose(fp);

    // Parse props to find HTTPS/HTTP proxy
    char *https_enable = strstr(props, "HTTPSEnable");
    char *https_proxy = strstr(props, "HTTPSProxy");
    char *https_port = strstr(props, "HTTPSPort");
    char *http_enable = strstr(props, "HTTPEnable");
    char *http_proxy = strstr(props, "HTTPProxy");
    char *http_port = strstr(props, "HTTPPort");

    char *result = NULL;

    if (https_enable && strstr(https_enable, " : 1") && https_proxy && https_port) {
        // Parse host and port
        sscanf(https_proxy, "HTTPSProxy : %[^\n]", line);
        char port_str[16];
        sscanf(https_port, "HTTPSPort : %s", port_str);
        size_t len = strlen("http://") + strlen(line) + strlen(port_str) + 2;
        result = malloc(len);
        snprintf(result, len, "http://%s:%s", line, port_str);
    } else if (http_enable && strstr(http_enable, " : 1") && http_proxy && http_port) {
        sscanf(http_proxy, "HTTPProxy : %[^\n]", line);
        char port_str[16];
        sscanf(http_port, "HTTPPort : %s", port_str);
        size_t len = strlen("http://") + strlen(line) + strlen(port_str) + 2;
        result = malloc(len);
        snprintf(result, len, "http://%s:%s", line, port_str);
    }

    free(props);
    return result;
#else
    return NULL;
#endif
}

/* Port of Python: gateway.platforms.base._split_host_port */
gw_host_port_t gw_split_host_port(const char *value) {
    gw_host_port_t result = {0};
    if (!value) return result;

    char *raw = strdup(value);
    if (!raw) return result;

    if (strstr(raw, "://")) {
        // URL parsing - simplified
        char *scheme_end = strstr(raw, "://");
        char *host_start = scheme_end + 3;
        char *path_start = strchr(host_start, '/');
        if (path_start) *path_start = '\0';

        char *port_start = strchr(host_start, ':');
        if (port_start) {
            *port_start = '\0';
            result.port = atoi(port_start + 1);
            result.has_port = true;
        }
        result.host = strdup(host_start);
    } else if (raw[0] == '[') {
        // IPv6
        char *bracket_end = strchr(raw, ']');
        if (bracket_end) {
            *bracket_end = '\0';
            result.host = strdup(raw + 1);
            if (*(bracket_end + 1) == ':') {
                result.port = atoi(bracket_end + 2);
                result.has_port = true;
            }
        }
    } else {
        // Simple host:port
        char *colon = strrchr(raw, ':');
        if (colon && !strchr(colon + 1, ':')) {
            *colon = '\0';
            result.port = atoi(colon + 1);
            result.has_port = true;
            result.host = strdup(raw);
        } else {
            result.host = strdup(raw);
        }
    }

    free(raw);
    return result;
}

/* Port of Python: gateway.platforms.base.should_bypass_proxy */
bool should_bypass_proxy(const char *target_hosts) {
    if (!target_hosts) return false;

    // Get NO_PROXY entries
    char *no_proxy = getenv("NO_PROXY");
    if (!no_proxy) no_proxy = getenv("no_proxy");
    if (!no_proxy) return false;

    // Simple implementation - check if any target matches
    // Full implementation would parse CIDR, wildcards, etc.
    return false; // Placeholder
}

/* PoP: resolve_proxy_url @ gateway/platforms/base.py:resolve_proxy_url */
/* Port of Python telegram_network.py:_resolve_proxy_url(). */
/* PoP: resolve_proxy_url @ gateway/platforms/helpers:resolve_proxy_url */
/* PoP: cli_gateway_platforms_base_resolve_proxy_url @ gateway/platforms/base.py:resolve_proxy_url */
char *resolve_proxy_url(const char *platform_env_var, const char *target_hosts) {
    if (platform_env_var) {
        char *value = getenv(platform_env_var);
        if (value && *value) {
            if (!should_bypass_proxy(target_hosts)) {
                return strdup(value);
            }
        }
    }

    const char *keys[] = {"HTTPS_PROXY", "HTTP_PROXY", "ALL_PROXY",
                          "https_proxy", "http_proxy", "all_proxy"};
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        char *value = getenv(keys[i]);
        if (value && *value) {
            if (!should_bypass_proxy(target_hosts)) {
                return strdup(value);
            }
        }
    }

    char *detected = detect_macos_system_proxy();
    if (detected && !should_bypass_proxy(target_hosts)) {
        return detected;
    }

    if (detected) free(detected);
    return NULL;
}

/* Port of Python: gateway.platforms.base.proxy_kwargs_for_aiohttp */
gw_proxy_kwargs_t proxy_kwargs_for_aiohttp(const char *proxy_url) {
    gw_proxy_kwargs_t result = {0};
    if (!proxy_url) return result;

    if (strncasecmp(proxy_url, "socks", 5) == 0) {
        result.connector = strdup(proxy_url);
        result.has_connector = true;
    } else {
        result.proxy = strdup(proxy_url);
        result.has_proxy = true;
    }
    return result;
}
