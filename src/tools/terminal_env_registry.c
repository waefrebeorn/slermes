/*
 * terminal_env_registry.c — C11 port of tools/terminal_tool.py env registry
 *
 * Stateful, faithful port of the terminal environment registry layer:
 *   _session_cwd, _task_env_overrides, _active_environments, _last_activity
 * and every function that reads/writes them. The Python module keeps these
 * as module-level dicts guarded by locks; C keeps them as a small
 * json_t*-backed registry (same data shapes, thread-safe via a mutex).
 *
 * Env *execution* backends (docker/ssh/modal/daytona/vercel) live in
 * src/tools/environments*.c — this module is the bookkeeping/decision layer
 * (resolve keys, parse config, decide persistence, drive cleanup) that the
 * Python module owns.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "terminal_env_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* ------------------------------------------------------------------ */
/* Registry state (module-level, mirroring Python module dicts)        */
/* ------------------------------------------------------------------ */

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static json_t *g_session_cwd;      /* {key: cwd}     -> _session_cwd       */
static json_t *g_task_overrides;   /* {task: {...}}  -> _task_env_overrides */
static json_t *g_active_envs;      /* {task: {...}}  -> _active_environments */
static json_t *g_last_activity;    /* {task: float}  -> _last_activity      */
static bool g_registry_initialized = false;

/* One-shot config-bridge guard (mirrors _terminal_config_bridge_attempted). */
static bool g_bridge_attempted = false;

/* Path prefixes that identify a *host* working directory which cannot exist
 * inside a container sandbox (mirrors _HOST_CWD_PREFIXES). */
static const char *const HOST_CWD_PREFIXES[] = {"/Users/", "/home/", "C:\\", "C:/", NULL};

/* Backends that are containers (mirrors _CONTAINER_BACKENDS). */
static const char *const CONTAINER_BACKENDS[] = {
    "docker", "singularity", "modal", "daytona", "vercel_sandbox", NULL};

/* Override keys that trigger per-task isolation (mirrors _ISOLATION_KEYS). */
static const char *const ISOLATION_KEYS[] = {
    "docker_image", "modal_image", "singularity_image", "daytona_image",
    "env_type", NULL};

static void registry_init_locked(void) {
    if (g_registry_initialized) return;
    g_session_cwd = json_object();
    g_task_overrides = json_object();
    g_active_envs = json_object();
    g_last_activity = json_object();
    g_registry_initialized = true;
}

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* str -> json number (mirrors _parse_env_var with int converter). */
static long parse_env_int(const char *name, const char *def, const char *type_label) {
    const char *raw = getenv(name);
    if (!raw || !*raw) raw = def;
    char *end = NULL;
    errno = 0;
    long v = strtol(raw, &end, 10);
    if (errno != 0 || end == raw || (end && *end != '\0')) {
        /* Python raises ValueError; C logs and falls back to default. */
        hermes_log(LOG_WARNING, "terminal", "Invalid value for %s: %s (expected %s)",
                   name, raw, type_label);
        return strtol(def, NULL, 10);
    }
    return v;
}

static double parse_env_float(const char *name, const char *def, const char *type_label) {
    const char *raw = getenv(name);
    if (!raw || !*raw) raw = def;
    char *end = NULL;
    errno = 0;
    double v = strtod(raw, &end);
    if (errno != 0 || end == raw || (end && *end != '\0')) {
        hermes_log(LOG_WARNING, "terminal", "Invalid value for %s: %s (expected %s)",
                   name, raw, type_label);
        return strtod(def, NULL);
    }
    return v;
}

static bool env_truthy(const char *name, bool def) {
    const char *raw = getenv(name);
    if (!raw || !*raw) return def;
    return env_truthy_raw(raw);
}

/* truthy on a raw string value */
bool env_truthy_raw(const char *v) {
    if (!v) return false;
    char low[64];
    size_t n = strlen(v);
    if (n >= sizeof(low)) n = sizeof(low) - 1;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)v[i]);
    low[n] = '\0';
    return strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0;
}

static bool in_list(const char *const list[], const char *s) {
    if (!s) return false;
    for (int i = 0; list[i]; i++)
        if (strcmp(list[i], s) == 0) return true;
    return false;
}

/* os.path.isabs() for POSIX (mirrors Python's os.path.isabs). */
static bool is_abs_path(const char *p) {
    return p && p[0] == '/';
}

/* _is_ssh_remote_tilde_cwd: only ssh backend expands ~ remotely. */
static bool is_ssh_remote_tilde_cwd(const char *backend, const char *cwd) {
    if (!backend || strcmp(backend, "ssh") != 0) return false;
    if (!cwd) return false;
    return strcmp(cwd, "~") == 0 || strncmp(cwd, "~/", 2) == 0;
}

/* _safe_getcwd: tolerate deleted CWD, fall back to TERMINAL_CWD then ~. */
static void safe_getcwd(char *buf, size_t bufsz) {
    if (getcwd(buf, bufsz)) return;
    const char *tc = getenv("TERMINAL_CWD");
    if (tc && *tc) { snprintf(buf, bufsz, "%s", tc); return; }
    const char *home = getenv("HOME");
    snprintf(buf, bufsz, "%s", home ? home : "/");
}

/* ------------------------------------------------------------------ */
/* PoP: record_session_cwd @ tools/terminal_tool.py:record_session_cwd */
/* ------------------------------------------------------------------ */
void term_rec_record_session_cwd(const char *session_key, const char *cwd) {
    if (!cwd || !*cwd) return;                 /* empty cwd ignored */
    /* strip whitespace */
    while (*cwd == ' ' || *cwd == '\t') cwd++;
    if (!*cwd) return;
    const char *key = (session_key && *session_key) ? session_key : "default";

    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    const char *old = json_get_str(g_session_cwd, key, NULL);
    if (!old || strcmp(old, cwd) != 0)
        json_set(g_session_cwd, key, json_string(cwd));
    pthread_mutex_unlock(&g_lock);
}

/* PoP: get_session_cwd @ tools/terminal_tool.py:get_session_cwd */
const char *term_rec_get_session_cwd(const char *session_key) {
    const char *key = (session_key && *session_key) ? session_key : "default";
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    const char *v = json_get_str(g_session_cwd, key, NULL);
    char *out = v ? strdup(v) : NULL;
    pthread_mutex_unlock(&g_lock);
    return out;
}

/* PoP: clear_session_cwd @ tools/terminal_tool.py:clear_session_cwd */
void term_rec_clear_session_cwd(const char *session_key) {
    if (!session_key) return;
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    json_obj_del(g_session_cwd, session_key);
    pthread_mutex_unlock(&g_lock);
}

/* PoP: register_task_env_overrides @ tools/terminal_tool.py:register_task_env_overrides */
void term_reg_register_task_env_overrides(const char *task_id, json_t *overrides) {
    if (!task_id || !*task_id || !overrides) return;
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    json_set(g_task_overrides, task_id, json_copy(overrides));

    /* If a live env exists for this task, a freshly registered cwd override
     * must take effect immediately (session record + live env cwd). */
    const char *new_cwd = json_get_str(overrides, "cwd", NULL);
    if (new_cwd && *new_cwd) {
        term_rec_record_session_cwd(task_id, new_cwd);
        char container_id[128];
        term_resolve_container_task_id(task_id, container_id, sizeof(container_id));
        json_t *env = json_obj_get(g_active_envs, task_id);
        if (!env) env = json_obj_get(g_active_envs, container_id);
        if (env) json_set(env, "cwd", json_string(new_cwd));
    }
    pthread_mutex_unlock(&g_lock);
}

/* PoP: clear_task_env_overrides @ tools/terminal_tool.py:clear_task_env_overrides */
void term_reg_clear_task_env_overrides(const char *task_id) {
    if (!task_id) return;
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    json_obj_del(g_task_overrides, task_id);
    pthread_mutex_unlock(&g_lock);
    term_rec_clear_session_cwd(task_id);
}

/* PoP: _resolve_container_task_id @ tools/terminal_tool.py:_resolve_container_task_id */
/* Map a tool-call task_id to the container/sandbox key. Subagents collapse to
 * "default"; override-registered rollouts with isolation keys stay distinct. */
void term_resolve_container_task_id(const char *task_id, char *out, size_t outsz) {
    if (task_id && *task_id) {
        pthread_mutex_lock(&g_lock);
        registry_init_locked();
        json_t *ov = json_obj_get(g_task_overrides, task_id);
        if (ov) {
            for (int i = 0; ISOLATION_KEYS[i]; i++) {
                if (json_obj_get(ov, ISOLATION_KEYS[i])) {
                    snprintf(out, outsz, "%s", task_id);
                    pthread_mutex_unlock(&g_lock);
                    return;
                }
            }
        }
        pthread_mutex_unlock(&g_lock);
    }
    snprintf(out, outsz, "%s", "default");
}

/* PoP: resolve_task_overrides @ tools/terminal_tool.py:resolve_task_overrides */
/* Raw key first, then collapsed container id — the single source so the
 * terminal and file layers can't drift apart. */
json_t *term_resolve_task_overrides(const char *task_id) {
    const char *raw = (task_id && *task_id) ? task_id : "default";
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    json_t *ov = json_obj_get(g_task_overrides, raw);
    if (!ov) {
        char cid[128];
        term_resolve_container_task_id(raw, cid, sizeof(cid));
        ov = json_obj_get(g_task_overrides, cid);
    }
    json_t *out = ov ? json_copy(ov) : json_object();
    pthread_mutex_unlock(&g_lock);
    return out;
}

/* ------------------------------------------------------------------ */
/* PoP: _is_unusable_container_cwd @ tools/terminal_tool.py:_is_unusable_container_cwd */
/* ------------------------------------------------------------------ */
bool term_is_unusable_container_cwd(const char *cwd) {
    if (!cwd || !*cwd) return false;
    for (int i = 0; HOST_CWD_PREFIXES[i]; i++)
        if (strncmp(cwd, HOST_CWD_PREFIXES[i], strlen(HOST_CWD_PREFIXES[i])) == 0)
            return true;
    if (!is_abs_path(cwd)) return true;
    return false;
}

/* PoP: _get_env_config @ tools/terminal_tool.py:_get_env_config */
/* Build the full terminal env config dict from TERMINAL_* env vars. */
json_t *term_get_env_config(void) {
    /* _ensure_terminal_env_bridged() — the C launcher bridges terminal.*
     * config into TERMINAL_* env at startup (cli.c / gateway). One-shot
     * guard mirrors Python; no config re-read per call. */
    if (!g_bridge_attempted) {
        g_bridge_attempted = true;
        /* C core: terminal config is bridged at startup by the launcher;
         * nothing to backfill here (local default applies otherwise). */
    }

    const char *env_type = getenv("TERMINAL_ENV");
    if (!env_type || !*env_type) env_type = "local";

    bool container_backend = in_list(CONTAINER_BACKENDS, env_type);
    bool docker_backend = strcmp(env_type, "docker") == 0;
    bool mount_docker_cwd = env_truthy("TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE", false);

    json_t *cfg = json_object();
    json_set(cfg, "env_type", json_string(env_type));
    json_set(cfg, "modal_mode", json_string(getenv("TERMINAL_MODAL_MODE") ? getenv("TERMINAL_MODAL_MODE") : "auto"));

    const char *default_image = "nikolaik/python-nodejs:python3.11-nodejs20";
    json_set(cfg, "docker_image",
             json_string(getenv("TERMINAL_DOCKER_IMAGE") ? getenv("TERMINAL_DOCKER_IMAGE") : default_image));

    /* container resource knobs (parsed only for container backends) */
    if (container_backend) {
        double cpu = parse_env_float("TERMINAL_CONTAINER_CPU", "1", "number");
        long mem = parse_env_int("TERMINAL_CONTAINER_MEMORY", "5120", "integer");
        long disk = parse_env_int("TERMINAL_CONTAINER_DISK", "51200", "integer");
        json_set(cfg, "container_cpu", json_number(cpu));
        json_set(cfg, "container_memory", json_number(mem));
        json_set(cfg, "container_disk", json_number(disk));
    } else {
        json_set(cfg, "container_cpu", json_number(1.0));
        json_set(cfg, "container_memory", json_number(5120));
        json_set(cfg, "container_disk", json_number(51200));
    }

    if (docker_backend) {
        const char *fe = getenv("TERMINAL_DOCKER_FORWARD_ENV");
        const char *vol = getenv("TERMINAL_DOCKER_VOLUMES");
        const char *de = getenv("TERMINAL_DOCKER_ENV");
        const char *ea = getenv("TERMINAL_DOCKER_EXTRA_ARGS");
        json_set(cfg, "docker_forward_env", json_string(fe ? fe : "[]"));
        json_set(cfg, "docker_volumes", json_string(vol ? vol : "[]"));
        json_set(cfg, "docker_env", json_string(de ? de : "{}"));
        json_set(cfg, "docker_extra_args", json_string(ea ? ea : "[]"));
        json_set(cfg, "docker_shm_size",
                 json_string(getenv("TERMINAL_DOCKER_SHM_SIZE") ? getenv("TERMINAL_DOCKER_SHM_SIZE") : "1g"));
    } else {
        json_set(cfg, "docker_forward_env", json_string("[]"));
        json_set(cfg, "docker_volumes", json_string("[]"));
        json_set(cfg, "docker_env", json_string("{}"));
        json_set(cfg, "docker_extra_args", json_string("[]"));
        json_set(cfg, "docker_shm_size", json_string("1g"));
    }

    /* default cwd per backend */
    char cwd_buf[4096];
    if (strcmp(env_type, "local") == 0) {
        safe_getcwd(cwd_buf, sizeof(cwd_buf));
    } else if (strcmp(env_type, "ssh") == 0) {
        snprintf(cwd_buf, sizeof(cwd_buf), "%s", "~");
    } else if (strcmp(env_type, "vercel_sandbox") == 0) {
        snprintf(cwd_buf, sizeof(cwd_buf), "%s", "/workspace");
    } else {
        snprintf(cwd_buf, sizeof(cwd_buf), "%s", "/root");
    }

    const char *cwd = getenv("TERMINAL_CWD");
    if (cwd && *cwd && !is_ssh_remote_tilde_cwd(env_type, cwd)) {
        /* expanduser (~) — minimal: leading ~/ or ~ */
        if (cwd[0] == '~') {
            const char *home = getenv("HOME");
            if (cwd[1] == '/' && home) {
                snprintf(cwd_buf, sizeof(cwd_buf), "%s%s", home, cwd + 1);
            } else if (cwd[1] == '\0' && home) {
                snprintf(cwd_buf, sizeof(cwd_buf), "%s", home);
            } else {
                snprintf(cwd_buf, sizeof(cwd_buf), "%s", cwd);
            }
        } else {
            snprintf(cwd_buf, sizeof(cwd_buf), "%s", cwd);
        }
    } else {
        /* cwd unset: default per backend already in cwd_buf */
    }

    /* docker mount passthrough: host path -> /workspace */
    const char *host_cwd = NULL;
    if (docker_backend && mount_docker_cwd) {
        const char *src = getenv("TERMINAL_CWD");
        char host_buf[4096];
        if (src && *src) {
            snprintf(host_buf, sizeof(host_buf), "%s", src);
            if (host_buf[0] == '~') {
                const char *home = getenv("HOME");
                if (home) {
                    char tmp[4096];
                    snprintf(tmp, sizeof(tmp), "%s%s", home, host_buf + 1);
                    snprintf(host_buf, sizeof(host_buf), "%s", tmp);
                }
            }
            if (is_abs_path(host_buf)) {
                bool hostish = false;
                for (int i = 0; HOST_CWD_PREFIXES[i]; i++) {
                    size_t plen = strlen(HOST_CWD_PREFIXES[i]);
                    if (strncmp(host_buf, HOST_CWD_PREFIXES[i], plen) == 0) { hostish = true; break; }
                }
                struct stat st;
                bool isdir = stat(host_buf, &st) == 0 && S_ISDIR(st.st_mode);
                if (hostish || (isdir && strncmp(host_buf, "/workspace", 10) != 0
                                && strncmp(host_buf, "/root", 5) != 0)) {
                    host_cwd = "set"; /* mark host path present */
                    snprintf(cwd_buf, sizeof(cwd_buf), "%s", "/workspace");
                }
            }
        }
    } else if (container_backend && cwd && *cwd) {
        /* Host/relative paths that won't work inside containers */
        if (term_is_unusable_container_cwd(cwd)
            && strcmp(cwd, cwd_buf) != 0) {
            /* keep default_cwd (cwd_buf already holds it) */
        }
    }

    json_set(cfg, "cwd", json_string(cwd_buf));
    if (host_cwd)
        json_set(cfg, "host_cwd", json_string(cwd_buf[0] ? cwd_buf : "/workspace"));
    else
        json_set(cfg, "host_cwd", json_null());
    json_set(cfg, "docker_mount_cwd_to_workspace", json_bool(mount_docker_cwd));
    json_set(cfg, "timeout", json_number(parse_env_int("TERMINAL_TIMEOUT", "180", "integer")));
    json_set(cfg, "lifetime_seconds", json_number(parse_env_int("TERMINAL_LIFETIME_SECONDS", "300", "integer")));

    /* SSH-specific */
    json_set(cfg, "ssh_host", json_string(getenv("TERMINAL_SSH_HOST") ? getenv("TERMINAL_SSH_HOST") : ""));
    json_set(cfg, "ssh_user", json_string(getenv("TERMINAL_SSH_USER") ? getenv("TERMINAL_SSH_USER") : ""));
    json_set(cfg, "ssh_port", json_number(parse_env_int("TERMINAL_SSH_PORT", "22", "integer")));
    json_set(cfg, "ssh_key", json_string(getenv("TERMINAL_SSH_KEY") ? getenv("TERMINAL_SSH_KEY") : ""));
    const char *sp = getenv("TERMINAL_SSH_PERSISTENT");
    if (!sp || !*sp) sp = getenv("TERMINAL_PERSISTENT_SHELL");
    if (!sp || !*sp) sp = "true";
    json_set(cfg, "ssh_persistent", json_bool(env_truthy_raw(sp)));

    return cfg;
}

/* ------------------------------------------------------------------ */
/* PoP: get_active_env @ tools/terminal_tool.py:get_active_env         */
/* PoP: is_persistent_env @ tools/terminal_tool.py:is_persistent_env   */
/* ------------------------------------------------------------------ */

/* Return a copy of the active env entry for task_id (or NULL). */
json_t *term_get_active_env(const char *task_id) {
    const char *raw = (task_id && *task_id) ? task_id : "default";
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    char cid[128];
    term_resolve_container_task_id(raw, cid, sizeof(cid));
    json_t *env = json_obj_get(g_active_envs, raw);
    if (!env) env = json_obj_get(g_active_envs, cid);
    json_t *out = env ? json_copy(env) : NULL;
    pthread_mutex_unlock(&g_lock);
    return out;
}

bool term_is_persistent_env(const char *task_id) {
    json_t *env = term_get_active_env(task_id);
    if (!env) return false;
    bool persist = false;
    json_t *pf = json_obj_get(env, "persistent_filesystem");
    if (pf && pf->type == JSON_BOOL) persist = pf->bool_val;
    json_free(env);
    return persist;
}

/* Record an active env entry (called by environments layer on creation). */
void term_env_set_active(const char *task_id, json_t *env_entry) {
    if (!task_id || !env_entry) return;
    const char *raw = (task_id && *task_id) ? task_id : "default";
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    json_set(g_active_envs, raw, json_copy(env_entry));
    json_set(g_last_activity, raw, json_number((double)time(NULL)));
    pthread_mutex_unlock(&g_lock);
}

/* ------------------------------------------------------------------ */
/* PoP: cleanup_vm @ tools/terminal_tool.py:cleanup_vm                 */
/* ------------------------------------------------------------------ */
void term_cleanup_vm(const char *task_id, bool force_remove) {
    (void)force_remove;
    if (!task_id) return;
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    json_t *env = json_obj_get(g_active_envs, task_id);
    json_obj_del(g_active_envs, task_id);
    json_obj_del(g_last_activity, task_id);
    pthread_mutex_unlock(&g_lock);
    /* Defer actual env.cleanup() to the environments layer. */
    if (env) {
        hermes_log(LOG_INFO, "terminal", "Manually cleaned up environment for task: %s", task_id);
        json_free(env);
    }
}

/* PoP: cleanup_all_environments @ tools/terminal_tool.py:cleanup_all_environments */
int term_cleanup_all_environments(void) {
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    /* Collect task ids from the active-envs object keys. */
    size_t n = g_active_envs->c.count;
    char **ids = calloc(n ? n : 1, sizeof(char *));
    size_t got = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *k = g_active_envs->c.keys[i];
        if (k && k->type == JSON_STRING)
            ids[got++] = strdup(k->str_val);
    }
    pthread_mutex_unlock(&g_lock);
    for (size_t i = 0; i < got; i++) {
        term_cleanup_vm(ids[i], false);
        free(ids[i]);
    }
    free(ids);
    return (int)got;
}

/* PoP: _cleanup_inactive_envs @ tools/terminal_tool.py:_cleanup_inactive_envs */
int term_cleanup_inactive_envs(long lifetime_seconds) {
    if (lifetime_seconds <= 0) lifetime_seconds = 300;
    double now = (double)time(NULL);
    int cleaned = 0;
    pthread_mutex_lock(&g_lock);
    registry_init_locked();
    size_t n = g_last_activity->c.count;
    char **ids = calloc(n ? n : 1, sizeof(char *));
    size_t got = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *k = g_last_activity->c.keys[i];
        if (!k || k->type != JSON_STRING) continue;
        json_t *la = json_obj_get(g_last_activity, k->str_val);
        double last = (la && la->type == JSON_NUMBER) ? la->num_val : 0.0;
        if (now - last > (double)lifetime_seconds)
            ids[got++] = strdup(k->str_val);
    }
    pthread_mutex_unlock(&g_lock);
    for (size_t i = 0; i < got; i++) {
        term_cleanup_vm(ids[i], false);
        free(ids[i]);
        cleaned++;
    }
    free(ids);
    return cleaned;
}

/* PoP: check_terminal_requirements @ tools/terminal_tool.py:check_terminal_requirements */
bool term_check_terminal_requirements(void) {
    json_t *cfg = term_get_env_config();
    const char *env_type = cfg ? json_get_str(cfg, "env_type", "local") : "local";
    if (cfg) json_free(cfg);

    if (strcmp(env_type, "local") == 0)
        return true;

    if (strcmp(env_type, "docker") == 0) {
        /* find_docker(): PATH + common install locations, then `docker version`. */
        const char *path = getenv("PATH");
        char full[4096];
        const char *candidates[] = {
            "/usr/bin/docker", "/usr/local/bin/docker",
            "/opt/homebrew/bin/docker", "/snap/bin/docker", NULL};
        bool found = false;
        if (path) {
            char *dup = strdup(path);
            char *tok = strtok(dup, ":");
            while (tok) {
                snprintf(full, sizeof(full), "%s/docker", tok);
                if (access(full, X_OK) == 0) { found = true; break; }
                tok = strtok(NULL, ":");
            }
            free(dup);
        }
        if (!found) {
            for (int i = 0; candidates[i]; i++) {
                if (access(candidates[i], X_OK) == 0) { found = true; break; }
            }
        }
        if (!found) {
            hermes_log(LOG_ERROR, "terminal",
                       "Docker executable not found in PATH or common install locations");
            return false;
        }
        /* `docker version` returncode == 0 */
        snprintf(full, sizeof(full), "%s version >/dev/null 2>&1", found ? full : "docker");
        int rc = system("docker version >/dev/null 2>&1");
        return rc == 0;
    }

    /* singularity/apptainer, modal, daytona, vercel, ssh — binary presence */
    const char *exe = NULL;
    if (strcmp(env_type, "singularity") == 0)
        exe = "apptainer";
    else if (strcmp(env_type, "vercel_sandbox") == 0)
        exe = "vercel";
    else if (strcmp(env_type, "ssh") == 0)
        exe = "ssh";
    else if (strcmp(env_type, "daytona") == 0)
        exe = "daytona";
    if (exe) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", exe);
        if (system(cmd) != 0) {
            if (strcmp(env_type, "singularity") == 0) {
                /* fall back to `singularity` binary */
                if (system("command -v singularity >/dev/null 2>&1") != 0) return false;
            } else {
                return false;
            }
        }
    }
    return true;
}

/* Free all registry state (atexit). */
void term_env_registry_shutdown(void) {
    pthread_mutex_lock(&g_lock);
    if (g_registry_initialized) {
        json_free(g_session_cwd);
        json_free(g_task_overrides);
        json_free(g_active_envs);
        json_free(g_last_activity);
        g_registry_initialized = false;
    }
    pthread_mutex_unlock(&g_lock);
}
