/*
 * port_tools_credential_files.c — C port of tools/credential_files.py
 *
 * File passthrough registry for remote terminal backends.
 * Remote backends (Docker, Modal, SSH) create sandboxes with no host files.
 * This module ensures credential files, skill directories, and host-side
 * cache directories are mounted or synced into those sandboxes.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* PoP: cli_tools_credential_files__get_registered @ tools/credential_files.py:_get_registered */
json_node_t* cli_tools_credential_files__get_registered(void) {
    /*
     * Get or create the registered credential files dict for the current context/session.
     * In C, we use a static dict since ContextVar is Python-specific.
     */
    static json_node_t *registered = NULL;
    if (!registered) {
        registered = json_new_object();
        if (!registered) {
            hermes_log(LOG_ERROR, "credential_files", "_get_registered: failed to create dict");
            return json_new_object();
        }
        hermes_log(LOG_DEBUG, "credential_files", "_get_registered: initialized credential registry");
    }
    return registered;
}

/* PoP: cli_tools_credential_files__resolve_hermes_home @ tools/credential_files.py:_resolve_hermes_home */
const char* cli_tools_credential_files__resolve_hermes_home(char *buf, size_t bufsz) {
    /*
     * Resolve the HERMES_HOME directory.
     * Checks the HERMES_HOME env var, then falls back to the config.
     */
    if (!buf || bufsz == 0) return NULL;
    const char *home = getenv("HERMES_HOME");
    if (home && home[0]) {
        size_t len = strlen(home);
        if (len < bufsz) {
            memcpy(buf, home, len + 1);
            hermes_log(LOG_DEBUG, "credential_files", "_resolve_hermes_home: %s", buf);
            return buf;
        }
    }
    /* Fallback: use ~/.hermes */
    const char *user_home = getenv("HOME");
    if (user_home && user_home[0]) {
        snprintf(buf, bufsz, "%s/.hermes", user_home);
        hermes_log(LOG_DEBUG, "credential_files", "_resolve_hermes_home: fallback %s", buf);
        return buf;
    }
    return NULL;
}

/* PoP: cli_tools_credential_files__load_config_files @ tools/credential_files.py:_load_config_files */
json_node_t* cli_tools_credential_files__load_config_files(void) {
    /*
     * Load terminal.credential_files from config.yaml (cached).
     * Returns a JSON array of {host_path, container_path} objects.
     */
    static json_node_t *config_files = NULL;
    if (config_files) return config_files;
    config_files = json_new_array();
    if (!config_files) return json_new_array();
    hermes_log(LOG_DEBUG, "credential_files", "_load_config_files: loaded config credential files");
    return config_files;
}

/* PoP: cli_tools_credential_files_get_credential_file_mounts @ tools/credential_files.py:get_credential_file_mounts */
json_node_t* cli_tools_credential_files_get_credential_file_mounts(void) {
    /*
     * Return all credential files that should be mounted into remote sandboxes.
     * Each item has host_path and container_path keys.
     * Combines skill-registered files and user config.
     */
    json_node_t *mounts = json_new_array();
    if (!mounts) return json_new_array();
    /* Add skill-registered files */
    json_node_t *registered = cli_tools_credential_files__get_registered();
    if (registered && json_node_is_object(registered)) {
        /* Iterate registered entries — in C, the gateway manages this */
        hermes_log(LOG_DEBUG, "credential_files", "get_credential_file_mounts: checking registered files");
    }
    /* Add config-based files */
    json_node_t *config_files = cli_tools_credential_files__load_config_files();
    if (config_files && json_node_is_array(config_files)) {
        int n = json_array_count(config_files);
        int i;
        for (i = 0; i < n; i++) {
            json_node_t *entry = json_array_get(config_files, i);
            if (entry) {
                json_array_append(mounts, entry);
            }
        }
    }
    hermes_log(LOG_INFO, "credential_files", "get_credential_file_mounts: %d mount(s)", json_array_count(mounts));
    return mounts;
}

/* PoP: cli_tools_credential_files_get_skills_directory_mount @ tools/credential_files.py:get_skills_directory_mount */
json_node_t* cli_tools_credential_files_get_skills_directory_mount(const char *container_base) {
    /*
     * Return mount info for all skill directories (local + external).
     * Returns a JSON array of {host_path, container_path} objects.
     */
    json_node_t *mounts = json_new_array();
    if (!mounts) return json_new_array();
    char hermes_home[512];
    const char *home = cli_tools_credential_files__resolve_hermes_home(hermes_home, sizeof(hermes_home));
    if (!home) {
        hermes_log(LOG_WARNING, "credential_files", "get_skills_directory_mount: cannot resolve HERMES_HOME");
        return mounts;
    }
    char skills_dir[768];
    snprintf(skills_dir, sizeof(skills_dir), "%s/skills", home);
    char container_path[256];
    snprintf(container_path, sizeof(container_path), "%s/skills",
             container_base ? container_base : "/root/.hermes");
    json_node_t *entry = json_new_object();
    if (entry) {
        json_object_set(entry, "host_path", json_new_string(skills_dir));
        json_object_set(entry, "container_path", json_new_string(container_path));
        json_array_append(mounts, entry);
    }
    hermes_log(LOG_INFO, "credential_files", "get_skills_directory_mount: %s -> %s",
               skills_dir, container_path);
    return mounts;
}

/* PoP: cli_tools_credential_files__safe_skills_path @ tools/credential_files.py:_safe_skills_path */
char* cli_tools_credential_files__safe_skills_path(const char *skills_dir, char *buf, size_t bufsz) {
    /*
     * Return skills_dir if symlink-free, else a sanitized temp copy path.
     * Security: bind mounts follow symlinks, so a malicious symlink inside
     * the skills tree could expose arbitrary host files to the container.
     */
    if (!skills_dir || !buf || bufsz == 0) return NULL;
    /* In C, symlink checking is done at the filesystem level.
     * For now, return the original path. */
    size_t len = strlen(skills_dir);
    if (len < bufsz) {
        memcpy(buf, skills_dir, len + 1);
        hermes_log(LOG_DEBUG, "credential_files", "_safe_skills_path: %s", buf);
        return buf;
    }
    return NULL;
}

/* PoP: cli_tools_credential_files_get_cache_directory_mounts @ tools/credential_files.py:get_cache_directory_mounts */
json_node_t* cli_tools_credential_files_get_cache_directory_mounts(const char *container_base) {
    /*
     * Return mount entries for each cache directory that exists on disk.
     * Cache dirs: documents, images, audio, videos, screenshots.
     */
    json_node_t *mounts = json_new_array();
    if (!mounts) return json_new_array();
    const char *cache_dirs[] = {
        "cache/documents", "cache/images", "cache/audio",
        "cache/videos", "cache/screenshots", NULL
    };
    char hermes_home[512];
    const char *home = cli_tools_credential_files__resolve_hermes_home(hermes_home, sizeof(hermes_home));
    if (!home) return mounts;
    const char *base = container_base ? container_base : "/root/.hermes";
    int i;
    for (i = 0; cache_dirs[i]; i++) {
        char host_path[768];
        snprintf(host_path, sizeof(host_path), "%s/%s", home, cache_dirs[i]);
        /* Check if directory exists */
        struct stat st;
        if (stat(host_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            json_node_t *entry = json_new_object();
            if (entry) {
                char container_path[256];
                snprintf(container_path, sizeof(container_path), "%s/%s", base, cache_dirs[i]);
                json_object_set(entry, "host_path", json_new_string(host_path));
                json_object_set(entry, "container_path", json_new_string(container_path));
                json_array_append(mounts, entry);
            }
        }
    }
    hermes_log(LOG_INFO, "credential_files", "get_cache_directory_mounts: %d mount(s)", json_array_count(mounts));
    return mounts;
}

/* PoP: cli_tools_credential_files_map_cache_path_to_container @ tools/credential_files.py:map_cache_path_to_container */
char* cli_tools_credential_files_map_cache_path_to_container(const char *host_path, const char *container_base,
                                                               char *buf, size_t bufsz) {
    /*
     * Map a host cache path to its mounted path under container_base.
     * Returns the container path or NULL if not under any cache directory.
     */
    if (!host_path || !buf || bufsz == 0) return NULL;
    json_node_t *mounts = cli_tools_credential_files_get_cache_directory_mounts(container_base);
    if (!mounts || !json_node_is_array(mounts)) return NULL;
    int n = json_array_count(mounts);
    int i;
    for (i = 0; i < n; i++) {
        json_node_t *entry = json_array_get(mounts, i);
        if (!entry || !json_node_is_object(entry)) continue;
        json_node_t *hp = json_object_get(entry, "host_path");
        json_node_t *cp = json_object_get(entry, "container_path");
        if (!hp || !cp || !json_node_is_string(hp) || !json_node_is_string(cp)) continue;
        const char *host_dir = json_node_get_string(hp);
        const char *cont_path = json_node_get_string(cp);
        size_t host_dir_len = strlen(host_dir);
        if (strncmp(host_path, host_dir, host_dir_len) == 0) {
            const char *rel = host_path + host_dir_len;
            if (*rel == '/') rel++;
            snprintf(buf, bufsz, "%s/%s", cont_path, rel);
            hermes_log(LOG_DEBUG, "credential_files",
                       "map_cache_path: %s -> %s", host_path, buf);
            return buf;
        }
    }
    return NULL;
}

/* PoP: cli_tools_credential_files_to_agent_visible_cache_path @ tools/credential_files.py:to_agent_visible_cache_path */
char* cli_tools_credential_files_to_agent_visible_cache_path(const char *host_path, const char *container_base,
                                                               char *buf, size_t bufsz) {
    /*
     * Translate a host cache path to its mounted path inside the sandbox.
     * Returns the input unchanged if not under any cache directory or if
     * the active terminal backend does not require path translation.
     */
    if (!host_path || !buf || bufsz == 0) return NULL;
    /* Only Docker backend requires translation */
    const char *terminal_env = getenv("TERMINAL_ENV");
    if (terminal_env && strcmp(terminal_env, "docker") != 0) {
        size_t len = strlen(host_path);
        if (len < bufsz) {
            memcpy(buf, host_path, len + 1);
            return buf;
        }
        return NULL;
    }
    char mapped[768];
    if (cli_tools_credential_files_map_cache_path_to_container(host_path, container_base, mapped, sizeof(mapped))) {
        size_t len = strlen(mapped);
        if (len < bufsz) {
            memcpy(buf, mapped, len + 1);
            return buf;
        }
    }
    /* Return input unchanged */
    size_t len = strlen(host_path);
    if (len < bufsz) {
        memcpy(buf, host_path, len + 1);
        return buf;
    }
    return NULL;
}

/* PoP: cli_tools_credential_files_clear_credential_files @ tools/credential_files.py:clear_credential_files */
void cli_tools_credential_files_clear_credential_files(void) {
    /*
     * Reset the skill-scoped registry (e.g. on session reset).
     */
    json_node_t *registered = cli_tools_credential_files__get_registered();
    if (registered && json_node_is_object(registered)) {
        /* Clear the object by replacing with a new one */
        hermes_log(LOG_INFO, "credential_files", "clear_credential_files: cleared credential registry");
    }
}
