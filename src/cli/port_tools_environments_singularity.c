/*
 * port_tools_environments_singularity.c — C port of tools/environments/singularity.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_environments_singularity__find_singularity_executable @ tools/environments/singularity.py:_find_singularity_executable */
const char* cli_tools_environments_singularity__find_singularity_executable(void) {
    const char *path;
    path = getenv("PATH");
    if (!path) {
        hermes_log(LOG_WARNING, "singularity", "PATH not set");
        return NULL;
    }
    /* Check for apptainer first, then singularity */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v apptainer 2>/dev/null");
    FILE *fp = popen(cmd, "r");
    if (fp) {
        static char result[256];
        if (fgets(result, sizeof(result), fp) != NULL) {
            size_t len = strlen(result);
            while (len > 0 && (result[len - 1] == '\n' || result[len - 1] == '\r')) result[--len] = '\0';
            if (len > 0) {
                pclose(fp);
                hermes_log(LOG_DEBUG, "singularity", "found apptainer: %s", result);
                return "apptainer";
            }
        }
        pclose(fp);
    }
    snprintf(cmd, sizeof(cmd), "command -v singularity 2>/dev/null");
    fp = popen(cmd, "r");
    if (fp) {
        static char result2[256];
        if (fgets(result2, sizeof(result2), fp) != NULL) {
            size_t len = strlen(result2);
            while (len > 0 && (result2[len - 1] == '\n' || result2[len - 1] == '\r')) result2[--len] = '\0';
            if (len > 0) {
                pclose(fp);
                hermes_log(LOG_DEBUG, "singularity", "found singularity: %s", result2);
                return "singularity";
            }
        }
        pclose(fp);
    }
    hermes_log(LOG_WARNING, "singularity", "Neither apptainer nor singularity found in PATH");
    return NULL;
}

/* PoP: cli_tools_environments_singularity__ensure_singularity_available @ tools/environments/singularity.py:_ensure_singularity_available */
const char* cli_tools_environments_singularity__ensure_singularity_available(void) {
    const char *exe = cli_tools_environments_singularity__find_singularity_executable();
    if (!exe) {
        hermes_log(LOG_WARNING, "singularity", "ensure_available: no executable found");
        return NULL;
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s version 2>&1", exe);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        hermes_log(LOG_WARNING, "singularity", "ensure_available: popen failed for %s", exe);
        return NULL;
    }
    char output[1024];
    size_t n = fread(output, 1, sizeof(output) - 1, fp);
    output[n] = '\0';
    int status = pclose(fp);
    if (status != 0) {
        hermes_log(LOG_WARNING, "singularity", "ensure_available: %s version failed (status=%d)", exe, status);
        return NULL;
    }
    hermes_log(LOG_DEBUG, "singularity", "ensure_available: %s version OK", exe);
    return exe;
}

/* PoP: cli_tools_environments_singularity__load_snapshots @ tools/environments/modal.py:_load_snapshots */
/* PoP: cli_tools_environments_singularity__load_snapshots @ tools/environments/singularity.py:_load_snapshots */
int cli_tools_environments_singularity__load_snapshots(const char *store_path, char *buf, size_t bufsize) {
    if (!store_path || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "singularity", "_load_snapshots: invalid args");
        return -1;
    }
    FILE *f = fopen(store_path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "singularity", "_load_snapshots: no file at %s", store_path);
        buf[0] = '\0';
        return 0;
    }
    size_t n = fread(buf, 1, bufsize - 1, f);
    buf[n] = '\0';
    fclose(f);
    hermes_log(LOG_DEBUG, "singularity", "_load_snapshots: read %zu bytes from %s", n, store_path);
    return 0;
}

/* PoP: cli_tools_environments_singularity__save_snapshots @ tools/environments/modal.py:_save_snapshots */
/* PoP: cli_tools_environments_singularity__save_snapshots @ tools/environments/singularity.py:_save_snapshots */
int cli_tools_environments_singularity__save_snapshots(const char *store_path, const char *data) {
    if (!store_path || !data) {
        hermes_log(LOG_WARNING, "singularity", "_save_snapshots: invalid args");
        return -1;
    }
    FILE *f = fopen(store_path, "w");
    if (!f) {
        hermes_log(LOG_WARNING, "singularity", "_save_snapshots: cannot open %s for writing", store_path);
        return -1;
    }
    size_t len = strlen(data);
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    if (written != len) {
        hermes_log(LOG_WARNING, "singularity", "_save_snapshots: partial write (%zu/%zu)", written, len);
        return -1;
    }
    hermes_log(LOG_DEBUG, "singularity", "_save_snapshots: wrote %zu bytes to %s", written, store_path);
    return 0;
}

/* PoP: cli_tools_environments_singularity__get_scratch_dir @ tools/environments/singularity.py:_get_scratch_dir */
int cli_tools_environments_singularity__get_scratch_dir(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) {
        return -1;
    }
    const char *custom = getenv("TERMINAL_SCRATCH_DIR");
    if (custom && *custom) {
        strncpy(buf, custom, bufsize - 1);
        buf[bufsize - 1] = '\0';
        hermes_log(LOG_DEBUG, "singularity", "_get_scratch_dir: custom=%s", buf);
        return 0;
    }
    const char *sandbox = getenv("HERMES_SANDBOX_DIR");
    if (sandbox && *custom) {
        int n = snprintf(buf, bufsize, "%s/singularity", sandbox);
        if (n > 0 && (size_t)n < bufsize) {
            hermes_log(LOG_DEBUG, "singularity", "_get_scratch_dir: sandbox=%s", buf);
            return 0;
        }
    }
    strncpy(buf, "/tmp/hermes-singularity", bufsize - 1);
    buf[bufsize - 1] = '\0';
    hermes_log(LOG_DEBUG, "singularity", "_get_scratch_dir: default=%s", buf);
    return 0;
}

/* PoP: cli_tools_environments_singularity__get_apptainer_cache_dir @ tools/environments/singularity.py:_get_apptainer_cache_dir */
int cli_tools_environments_singularity__get_apptainer_cache_dir(char *buf, size_t bufsize) {
    if (!buf || bufsize == 0) {
        return -1;
    }
    const char *cachedir = getenv("APPTAINER_CACHEDIR");
    if (cachedir && *cachedir) {
        strncpy(buf, cachedir, bufsize - 1);
        buf[bufsize - 1] = '\0';
        hermes_log(LOG_DEBUG, "singularity", "_get_apptainer_cache_dir: env=%s", buf);
        return 0;
    }
    char scratch[2048];
    if (cli_tools_environments_singularity__get_scratch_dir(scratch, sizeof(scratch)) != 0) {
        strncpy(buf, "/tmp/hermes-singularity/.apptainer", bufsize - 1);
    } else {
        snprintf(buf, bufsize, "%s/.apptainer", scratch);
    }
    buf[bufsize - 1] = '\0';
    hermes_log(LOG_DEBUG, "singularity", "_get_apptainer_cache_dir: %s", buf);
    return 0;
}

/* PoP: cli_tools_environments_singularity__get_or_build_sif @ tools/environments/singularity.py:_get_or_build_sif */
int cli_tools_environments_singularity__get_or_build_sif(const char *image, const char *executable, char *buf, size_t bufsize) {
    if (!image || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "singularity", "_get_or_build_sif: invalid args");
        return -1;
    }
    /* If it's already a .sif file, return as-is */
    size_t ilen = strlen(image);
    if (ilen > 4 && strcmp(image + ilen - 4, ".sif") == 0) {
        strncpy(buf, image, bufsize - 1);
        buf[bufsize - 1] = '\0';
        hermes_log(LOG_DEBUG, "singularity", "_get_or_build_sif: already .sif: %s", buf);
        return 0;
    }
    /* If not a docker:// URL, return as-is */
    if (strncmp(image, "docker://", 9) != 0) {
        strncpy(buf, image, bufsize - 1);
        buf[bufsize - 1] = '\0';
        hermes_log(LOG_DEBUG, "singularity", "_get_or_build_sif: not docker: %s", buf);
        return 0;
    }
    /* Build SIF path from docker image name */
    char cache_dir[2048];
    if (cli_tools_environments_singularity__get_apptainer_cache_dir(cache_dir, sizeof(cache_dir)) != 0) {
        strncpy(buf, image, bufsize - 1);
        buf[bufsize - 1] = '\0';
        return 0;
    }
    /* Convert docker://name:tag to name-tag.sif */
    const char *img = image + 9; /* skip docker:// */
    char name_buf[1024];
    strncpy(name_buf, img, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    for (char *p = name_buf; *p; p++) {
        if (*p == '/' || *p == ':') *p = '-';
    }
    snprintf(buf, bufsize, "%s/%s.sif", cache_dir, name_buf);
    hermes_log(LOG_DEBUG, "singularity", "_get_or_build_sif: sif_path=%s", buf);
    return 0;
}

/* PoP: cli_tools_environments_singularity__start_instance @ tools/environments/singularity.py:_start_instance */
int cli_tools_environments_singularity__start_instance(const char *executable, const char *image, const char *instance_id, int persistent, const char *overlay_dir, int memory, int cpus) {
    if (!executable || !image || !instance_id) {
        hermes_log(LOG_WARNING, "singularity", "_start_instance: invalid args");
        return -1;
    }
    char cmd[4096];
    int n = snprintf(cmd, sizeof(cmd), "%s instance start --containall --no-home", executable);
    if (persistent && overlay_dir) {
        n += snprintf(cmd + n, sizeof(cmd) - n, " --overlay %s", overlay_dir);
    } else {
        n += snprintf(cmd + n, sizeof(cmd) - n, " --writable-tmpfs");
    }
    if (memory > 0) {
        n += snprintf(cmd + n, sizeof(cmd) - n, " --memory %dM", memory);
    }
    if (cpus > 0) {
        n += snprintf(cmd + n, sizeof(cmd) - n, " --cpus %d", cpus);
    }
    n += snprintf(cmd + n, sizeof(cmd) - n, " %s %s", image, instance_id);
    hermes_log(LOG_DEBUG, "singularity", "_start_instance: %s", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "singularity", "_start_instance: failed (status=%d)", ret);
        return -1;
    }
    hermes_log(LOG_DEBUG, "singularity", "_start_instance: instance %s started", instance_id);
    return 0;
}

/* PoP: cli_tools_environments_singularity__run_bash @ tools/environments/singularity.py:_run_bash */
int cli_tools_environments_singularity__run_bash(const char *executable, const char *instance_id, const char *cmd_string, int login_mode) {
    if (!executable || !instance_id || !cmd_string) {
        hermes_log(LOG_WARNING, "singularity", "_run_bash: invalid args");
        return -1;
    }
    char cmd[4096];
    if (login_mode) {
        snprintf(cmd, sizeof(cmd), "%s exec instance://%s bash -l -c \"%s\"", executable, instance_id, cmd_string);
    } else {
        snprintf(cmd, sizeof(cmd), "%s exec instance://%s bash -c \"%s\"", executable, instance_id, cmd_string);
    }
    hermes_log(LOG_DEBUG, "singularity", "_run_bash: %s", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "singularity", "_run_bash: failed (status=%d)", ret);
        return -1;
    }
    return 0;
}
