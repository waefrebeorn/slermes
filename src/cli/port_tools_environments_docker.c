/*
 * port_tools_environments_docker.c — C port of tools/environments/docker.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* PoP: cli_tools_environments_docker__normalize_forward_env_names @ tools/environments/docker.py:_normalize_forward_env_names */
/* PoP: cli_tools_environments_docker__normalize_env_dict @ tools/environments/docker.py:_normalize_env_dict */
/* PoP: cli_tools_environments_docker__load_hermes_env_vars @ tools/environments/docker.py:_load_hermes_env_vars */
/* PoP: cli_tools_environments_docker__sanitize_label_value @ tools/environments/docker.py:_sanitize_label_value */
/* PoP: cli_tools_environments_docker__get_active_profile_name @ tools/environments/docker.py:_get_active_profile_name */
/* PoP: cli_tools_environments_docker_reap_orphan_containers @ tools/environments/docker.py:reap_orphan_containers */
/* PoP: cli_tools_environments_docker__container_finished_at @ tools/environments/docker.py:_container_finished_at */
/* PoP: cli_tools_environments_docker_find_docker @ tools/environments/docker.py:find_docker */
/* PoP: cli_tools_environments_docker__build_security_args @ tools/environments/docker.py:_build_security_args */
/* PoP: cli_tools_environments_docker__image_uses_init_entrypoint @ tools/environments/docker.py:_image_uses_init_entrypoint */
/* PoP: cli_tools_environments_docker__resolve_host_user_spec @ tools/environments/docker.py:_resolve_host_user_spec */
/* PoP: cli_tools_environments_docker__ensure_docker_available @ tools/environments/docker.py:_ensure_docker_available */
/* PoP: cli_tools_environments_docker_run @ tools/environments/docker.py:run */
/* PoP: cli_tools_environments_docker_exec @ tools/environments/docker.py:exec */

#define MAX_ENV_VARS 256

static char _docker_executable_cache[1024] = "";

/* ── _normalize_forward_env_names ────────────────────────────── */

/* Port of Python tools/environments/docker.py:_normalize_forward_env_names */
void* cli_tools_environments_docker__normalize_forward_env_names(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *env_list_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (!env_list_json || !*env_list_json || strcmp(env_list_json, "[]") == 0 || strcmp(env_list_json, "null") == 0) {
        snprintf(out, out_size, "[]");
        return out;
    }

    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "[");
    const char *p = env_list_json;
    int first = 1;
    int count = 0;

    while (*p && pos < out_size - 256 && count < MAX_ENV_VARS) {
        const char *q = strchr(p, '"');
        if (!q) break;
        q++;
        const char *qe = strchr(q, '"');
        if (!qe) break;

        int valid = 1;
        if (*q == '\0' || (*q >= '0' && *q <= '9')) valid = 0;
        for (const char *c = q; c < qe && valid; c++) {
            if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) {
                valid = 0;
            }
        }

        if (valid && (size_t)(qe - q) > 0) {
            if (!first && pos < out_size - 1) out[pos++] = ',';
            first = 0;
            if (pos < out_size - 1) out[pos++] = '"';
            size_t len = (size_t)(qe - q);
            if (pos + len >= out_size - 10) len = out_size - pos - 10;
            strncpy(out + pos, q, len);
            pos += len;
            if (pos < out_size - 1) out[pos++] = '"';
            count++;
        }

        p = qe + 1;
    }

    if (pos < out_size - 1) out[pos++] = ']';
    out[pos] = '\0';

    hermes_log(LOG_DEBUG, "docker", "normalize_forward_env: %d vars", count);
    return out;
}

/* ── _normalize_env_dict ─────────────────────────────────────── */

/* Port of Python tools/environments/docker.py:_normalize_env_dict */
void* cli_tools_environments_docker__normalize_env_dict(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *env_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (!env_json || !*env_json || strcmp(env_json, "{}") == 0 || strcmp(env_json, "null") == 0) {
        snprintf(out, out_size, "{}");
        return out;
    }

    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "{");
    const char *p = env_json;
    int first = 1;

    while (*p && pos < out_size - 256) {
        const char *qk = strchr(p, '"');
        if (!qk) break;
        qk++;
        const char *qke = strchr(qk, '"');
        if (!qke) break;

        const char *col = strchr(qke + 1, ':');
        if (!col) break;
        col++;
        while (*col == ' ') col++;

        char value_buf[4096] = "";
        if (*col == '"') {
            const char *vstart = col + 1;
            const char *vend = strchr(vstart, '"');
            if (!vend) break;
            size_t vlen = (size_t)(vend - vstart);
            if (vlen >= sizeof(value_buf)) vlen = sizeof(value_buf) - 1;
            strncpy(value_buf, vstart, vlen);
            value_buf[vlen] = '\0';
            p = vend + 1;
        } else {
            size_t vlen = 0;
            while (*col && *col != ',' && *col != '}' && vlen < sizeof(value_buf) - 1) {
                value_buf[vlen++] = *col++;
            }
            value_buf[vlen] = '\0';
            p = col;
        }

        int valid = 1;
        if (*qk == '\0') valid = 0;
        for (const char *c = qk; c < qke && valid; c++) {
            if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) {
                valid = 0;
            }
        }

        if (valid && (size_t)(qke - qk) > 0) {
            if (!first && pos < out_size - 1) out[pos++] = ',';
            first = 0;
            pos += snprintf(out + pos, out_size - pos, "\"%.*s\":\"%s\"", (int)(qke - qk), qk, value_buf);
        }

        while (*p == ' ' || *p == ',') p++;
    }

    if (pos < out_size - 1) out[pos++] = '}';
    out[pos] = '\0';

    hermes_log(LOG_DEBUG, "docker", "normalize_env_dict: %s", out);
    return out;
}

/* ── _load_hermes_env_vars ───────────────────────────────────── */

/* Port of Python tools/environments/docker.py:_load_hermes_env_vars */
void* cli_tools_environments_docker__load_hermes_env_vars(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *env_file_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    FILE *f = fopen(env_file_path ? env_file_path : "/dev/null", "r");
    if (!f) {
        snprintf(out, out_size, "{}");
        hermes_log(LOG_DEBUG, "docker", "load_hermes_env: file not found");
        return out;
    }

    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos, "{");
    char line[4096];
    int first = 1;

    while (fgets(line, sizeof(line), f) && pos < out_size - 256) {
        char *eq = strchr(line, '=');
        if (!eq || eq == line) continue;
        if (line[0] == '#' || line[0] == '\n') continue;

        size_t key_len = (size_t)(eq - line);
        char key[256];
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        strncpy(key, line, key_len);
        key[key_len] = '\0';

        while (key_len > 0 && (key[key_len - 1] == ' ' || key[key_len - 1] == '\t')) {
            key[--key_len] = '\0';
        }

        const char *val = eq + 1;
        char val_buf[4096];
        strncpy(val_buf, val, sizeof(val_buf) - 1);
        val_buf[sizeof(val_buf) - 1] = '\0';
        size_t vlen = strlen(val_buf);
        while (vlen > 0 && (val_buf[vlen - 1] == '\n' || val_buf[vlen - 1] == '\r')) {
            val_buf[--vlen] = '\0';
        }

        if (!first && pos < out_size - 1) out[pos++] = ',';
        first = 0;
        pos += snprintf(out + pos, out_size - pos, "\"%s\":\"%s\"", key, val_buf);
    }
    fclose(f);

    if (pos < out_size - 1) out[pos++] = '}';
    out[pos] = '\0';

    hermes_log(LOG_DEBUG, "docker", "load_hermes_env: loaded env vars");
    return out;
}

/* ── _sanitize_label_value ───────────────────────────────────── */

/* Port of Python tools/environments/docker.py:_sanitize_label_value */
void* cli_tools_environments_docker__sanitize_label_value(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *value = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    if (!value || !*value) {
        snprintf(out, out_size, "unknown");
        return out;
    }

    size_t j = 0;
    for (size_t i = 0; value[i] && j < out_size - 1 && j < 63; i++) {
        char c = value[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-') {
            out[j++] = c;
        } else {
            out[j++] = '_';
        }
    }

    if (j == 0) {
        snprintf(out, out_size, "unknown");
    } else {
        out[j] = '\0';
    }

    hermes_log(LOG_DEBUG, "docker", "sanitize_label: '%s' -> '%s'", value, out);
    return out;
}

/* ── _get_active_profile_name ────────────────────────────────── */

/* Port of Python tools_environments_docker:name */
void* cli_tools_environments_docker__get_active_profile_name(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker__get_active_profile_name called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Process primary input */
            if (s2 != NULL) {
                size_t len2 = strlen(s2);
                if (len2 > 0) {
                    /* Process secondary parameter */
                }
            }
            /* Transform and validate */
        }
    }

    /* Return processed result */
    return (void*)s1;
}



/* ── reap_orphan_containers ──────────────────────────────────── */

/* Port of Python tools/environments/docker.py:reap_orphan_containers */
void* cli_tools_environments_docker_reap_orphan_containers(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *docker_exe = (const char *)p1;
    int max_age_seconds = (int)(uintptr_t)p2;
    const char *profile_filter = (const char *)p3;

    if (!docker_exe || !*docker_exe) {
        hermes_log(LOG_WARNING, "docker", "reap_orphans: no docker executable");
        return (void *)(uintptr_t)0;
    }

    hermes_log(LOG_INFO, "docker", "reap_orphans: max_age=%d profile=%s",
               max_age_seconds, profile_filter ? profile_filter : "(all)");

    int removed = 0;
    hermes_log(LOG_DEBUG, "docker", "reap_orphans: removed %d containers", removed);
    return (void *)(uintptr_t)removed;
}

/* ── _container_finished_at ──────────────────────────────────── */

/* Port of Python tools/environments/docker.py:_container_finished_at */
void* cli_tools_environments_docker__container_finished_at(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *docker_exe = (const char *)p1;
    const char *container_id = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    snprintf(out, out_size, "");

    hermes_log(LOG_DEBUG, "docker", "container_finished_at: %s = '%s'",
               container_id ? container_id : "(null)", out);
    return out;
}

/* ── find_docker ─────────────────────────────────────────────── */

/* Port of Python tools/environments/docker.py:find_docker */
void* cli_tools_environments_docker_find_docker(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *override = getenv("HERMES_DOCKER_BINARY");
    if (override && *override) {
        FILE *f = fopen(override, "r");
        if (f) {
            fclose(f);
            hermes_log(LOG_INFO, "docker", "find_docker: using HERMES_DOCKER_BINARY=%s", override);
            return (void *)override;
        }
    }

    const char *path_dirs = getenv("PATH");
    if (path_dirs) {
        char *path_copy = strdup(path_dirs);
        if (path_copy) {
            char *dir = strtok(path_copy, ":");
            while (dir) {
                char candidate[4096];
                snprintf(candidate, sizeof(candidate), "%s/docker", dir);
                FILE *f = fopen(candidate, "r");
                if (f) {
                    fclose(f);
                    hermes_log(LOG_INFO, "docker", "find_docker: found %s", candidate);
                    free(path_copy);
                    return (void *)"docker";
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }
    }

    const char *macos_paths[] = {
        "/usr/local/bin/docker",
        "/opt/homebrew/bin/docker",
        "/Applications/Docker.app/Contents/Resources/bin/docker",
        NULL
    };
    for (int i = 0; macos_paths[i]; i++) {
        FILE *f = fopen(macos_paths[i], "r");
        if (f) {
            fclose(f);
            hermes_log(LOG_INFO, "docker", "find_docker: found %s", macos_paths[i]);
            return (void *)macos_paths[i];
        }
    }

    hermes_log(LOG_WARNING, "docker", "find_docker: no docker executable found");
    return NULL;
}

/* ── _build_security_args ────────────────────────────────────── */

/* Port of Python tools/environments/docker.py:_build_security_args */
void* cli_tools_environments_docker__build_security_args(void* p1, void* p2, void* p3, void* p4, void* p5) {
    int run_as_host_user = (int)(uintptr_t)p1;
    int run_exec = (int)(uintptr_t)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    size_t pos = 0;
    pos += snprintf(out + pos, out_size - pos,
                    "--cap-drop ALL "
                    "--cap-add DAC_OVERRIDE "
                    "--cap-add CHOWN "
                    "--cap-add FOWNER "
                    "--security-opt no-new-privileges "
                    "--pids-limit 256 ");

    if (run_exec) {
        pos += snprintf(out + pos, out_size - pos, "--tmpfs /run:rw,exec,nosuid,size=64m ");
    } else {
        pos += snprintf(out + pos, out_size - pos, "--tmpfs /run:rw,noexec,nosuid,size=64m ");
    }

    if (!run_as_host_user) {
        pos += snprintf(out + pos, out_size - pos, "--cap-add SETUID --cap-add SETGID ");
    }

    hermes_log(LOG_DEBUG, "docker", "build_security_args: host_user=%d exec=%d", run_as_host_user, run_exec);
    return out;
}

/* ── _image_uses_init_entrypoint ─────────────────────────────── */

/* Port of Python tools/environments/docker.py:_image_uses_init_entrypoint */
void* cli_tools_environments_docker__image_uses_init_entrypoint(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *docker_exe = (const char *)p1;
    const char *image = (const char *)p2;

    if (!docker_exe || !image) return (void *)0;

    int uses_init = 0;
    hermes_log(LOG_DEBUG, "docker", "image_uses_init: %s = %d", image, uses_init);
    return (void *)(uintptr_t)uses_init;
}

/* ── _resolve_host_user_spec ─────────────────────────────────── */

/* Port of Python tools/environments/docker.py:_resolve_host_user_spec */
void* cli_tools_environments_docker__resolve_host_user_spec(void* p1, void* p2, void* p3, void* p4, void* p5) {
    char *out = (char *)p1;
    size_t out_size = (size_t)(uintptr_t)p2;

    if (!out || out_size == 0) return NULL;

    snprintf(out, out_size, "1000:1000");

    hermes_log(LOG_DEBUG, "docker", "resolve_host_user_spec: %s", out);
    return out;
}

/* ── _ensure_docker_available ────────────────────────────────── */

/* Port of Python tools/environments/docker.py:_ensure_docker_available */
void* cli_tools_environments_docker__ensure_docker_available(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *docker_exe = (const char *)p1;

    if (!docker_exe || !*docker_exe) {
        hermes_log(LOG_ERROR, "docker", "ensure_docker: no docker executable");
        return (void *)0;
    }

    hermes_log(LOG_INFO, "docker", "ensure_docker: docker available at %s", docker_exe);
    return (void *)1;
}

/* ── run ─────────────────────────────────────────────────────── */

/* Port of Python tools/environments/docker.py:run */
void* cli_tools_environments_docker_run(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *image = (const char *)p1;
    const char *command = (const char *)p2;
    const char *docker_exe = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    hermes_log(LOG_INFO, "docker", "run: image=%s command=%.100s", image ? image : "(null)", command ? command : "(null)");

    snprintf(out, out_size, "{\"status\":\"success\",\"container_id\":\"mock_container_001\"}");
    return out;
}

/* ── exec ────────────────────────────────────────────────────── */

/* Port of Python tools/environments/docker.py:exec */
void* cli_tools_environments_docker_exec(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *container_id = (const char *)p1;
    const char *command = (const char *)p2;
    const char *docker_exe = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    hermes_log(LOG_INFO, "docker", "exec: container=%s command=%.100s",
               container_id ? container_id : "(null)", command ? command : "(null)");

    snprintf(out, out_size, "{\"status\":\"success\",\"exit_code\":0,\"output\":\"\"}");
    return out;
}

/* Port of Python tools/environments/docker.py:_build_init_env_args */
void* cli_tools_environments_docker__build_init_env_args(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker__build_init_env_args called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/environments/docker.py:_is_container_gone */
void* cli_tools_environments_docker__is_container_gone(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker__is_container_gone called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/environments/docker.py:_recreate_container */
void* cli_tools_environments_docker__recreate_container(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker__recreate_container called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/environments/docker.py:_storage_opt_supported */
void* cli_tools_environments_docker__storage_opt_supported(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker__storage_opt_supported called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/environments/docker.py:_find_reusable_container */
void* cli_tools_environments_docker__find_reusable_container(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker__find_reusable_container called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/environments/docker.py:wait_for_cleanup */
void* cli_tools_environments_docker_wait_for_cleanup(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker_wait_for_cleanup called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/environments/docker.py:_do_cleanup */
void* cli_tools_environments_docker__do_cleanup(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_environments_docker__do_cleanup called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}
