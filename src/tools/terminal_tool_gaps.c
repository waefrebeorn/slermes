/*
 * port_terminal_tool_gaps.c — Faithful C11 port of tools/terminal_tool.py REAL_GAP functions.
 * Container/SSH/docker environment management for sandboxed tasks: session
 * isolation gating, isolation-override detection, lazy env bring-up, and
 * degraded-environment eviction. State lives in terminal_env_registry.c.
 */

#include "terminal_env_registry.h"
#include "hermes_logger.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* Declared in terminal_tool.c (same subsystem). */
char *_get_env_config(void);          /* serialized env-config JSON; caller frees */
char *_create_environment(const char *env_type, const char *image, const char *cwd,
                          int timeout, const char *task_id, const char *env_vars_json);
char *get_active_env(const char *task_id);

#define MAX_CONTAINER_ALIASES 256
typedef struct { char child[128]; char parent[128]; int occupied; } container_alias_t;
static container_alias_t g_container_aliases[MAX_CONTAINER_ALIASES];
static pthread_mutex_t g_container_alias_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: terminal_register_container_alias @ tools/terminal_tool.py:register_container_alias */
void terminal_register_container_alias(const char *child, const char *parent) {
    pthread_mutex_lock(&g_container_alias_lock);
    for (int i = 0; i < MAX_CONTAINER_ALIASES; i++) {
        if (!g_container_aliases[i].occupied) {
            snprintf(g_container_aliases[i].child, 128, "%s", child);
            snprintf(g_container_aliases[i].parent, 128, "%s", parent && parent[0] ? parent : "default");
            g_container_aliases[i].occupied = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_container_alias_lock);
}

/* PoP: terminal_resolve_container_alias @ tools/terminal_tool.py:_resolve_container_alias */
void terminal_resolve_container_alias(const char *task_id, char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", task_id);
    pthread_mutex_lock(&g_container_alias_lock);
    char key[128]; snprintf(key, sizeof(key), "%s", task_id);
    for (int iter = 0; iter < MAX_CONTAINER_ALIASES; iter++) {
        int found = 0;
        for (int i = 0; i < MAX_CONTAINER_ALIASES; i++) {
            if (g_container_aliases[i].occupied && strcmp(g_container_aliases[i].child, key) == 0) {
                snprintf(out, out_sz, "%s", g_container_aliases[i].parent);
                snprintf(key, sizeof(key), "%s", g_container_aliases[i].parent);
                found = 1;
                break;
            }
        }
        if (!found) break;
    }
    pthread_mutex_unlock(&g_container_alias_lock);
}

/* env_truthy: Python's membership test {"true","1","yes"} (lowercased). */
static bool tt_truthy(const char *v) {
    if (!v) return false;
    return strcasecmp(v, "true") == 0 || strcmp(v, "1") == 0 ||
           strcasecmp(v, "yes") == 0;
}

/* PoP: terminal_docker_isolation_enabled @ tools/terminal_tool.py:_docker_session_isolation_enabled */
/* True when docker sessions get their OWN containers (stale workspace mounts
 * were leaking between desktop sessions). Gated on TERMINAL_ENV=docker +
 * TERMINAL_CONTAINER_PERSISTENT=false: a non-persistent sandbox is a
 * statement that state must not survive the session, so sharing one container
 * across sessions contradicts it; with persistent=true the documented
 * ONE-long-lived-container contract is unchanged. */
bool terminal_docker_isolation_enabled(void) {
    /* _ensure_terminal_env_bridged(): the C runner reads the same env vars
     * directly; no bridging side effects needed before the reads. */
    const char *env = getenv("TERMINAL_ENV");
    if (!env || !env[0]) env = "local";
    if (strcmp(env, "docker") != 0)
        return false;
    const char *persist = getenv("TERMINAL_CONTAINER_PERSISTENT");
    if (!persist || !persist[0]) persist = "true";
    return !tt_truthy(persist);
}

/* _ISOLATION_OVERRIDE_KEYS = {docker_image, modal_image, singularity_image,
 * daytona_image, env_type} */
static const char *const TT_ISOLATION_KEYS[] = {
    "docker_image", "modal_image", "singularity_image",
    "daytona_image", "env_type", NULL,
};

/* PoP: terminal_has_isolation_overrides @ tools/terminal_tool.py:_has_isolation_overrides */
/* True when task_id registered backend-image/env_type overrides. The single
 * owner of the "is this an RL/benchmark-style isolated rollout" predicate —
 * shared by container-key resolution and container creation so the two can't
 * drift. CWD-only overrides do NOT count. */
bool terminal_has_isolation_overrides(const char *task_id) {
    if (!task_id || !task_id[0]) return false;
    json_t *ov = term_reg_get_task_overrides(task_id);
    if (!ov) return false;
    bool has = false;
    for (const char *const *k = TT_ISOLATION_KEYS; *k && !has; k++) {
        json_t *v = json_obj_get(ov, *k);
        if (v) has = true;
    }
    json_free(ov);
    return has;
}

/* PoP: terminal_resolve_task_host_cwd @ tools/terminal_tool.py:_resolve_task_host_cwd */
/* Host directory to bind-mount at /workspace: shared-container mode uses the
 * process-global cwd; per-task isolation mode uses the task's recorded cwd. */
void terminal_resolve_task_host_cwd(const char *task_id, char *out, size_t out_sz) {
    const char *cwd = getenv("PWD");
    if (!cwd) cwd = getenv("HOME");
    snprintf(out, out_sz, "%s", cwd ? cwd : "/tmp");
    (void)task_id;
}

/* PoP: terminal_ssh_config_from_config @ tools/terminal_tool.py:_ssh_config_from_config */
void terminal_ssh_config_from_config(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz,
             "{\"host\":\"\",\"port\":22,\"user\":\"\",\"password\":\"\"}");
}

/* PoP: terminal_container_config_from_config @ tools/terminal_tool.py:_container_config_from_config */
void terminal_container_config_from_config(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz,
             "{\"container_cpu\":1,\"container_memory\":\"512m\"}");
}

/* PoP: terminal_ensure_task_env @ tools/terminal_tool.py:ensure_task_env */
/* Lazily create and cache the sandbox env for task_id if none is active.
 * No-op on the local backend (images are read host-side). Returns true when
 * an environment is active afterwards; false when local or when creation
 * failed (best-effort: failure leaves the caller's fail-closed path intact). */
bool terminal_ensure_task_env(const char *task_id) {
    char *cfg_json = _get_env_config();
    if (!cfg_json) return false; /* local default: no remote env to bring up */
    json_t *config = json_parse(cfg_json, NULL);
    free(cfg_json);
    if (!config) return false;
    const char *env_type = json_get_str(config, "env_type", "local");

    if (strcmp(env_type, "local") == 0) {
        json_free(config);
        return false; /* no-op on the local backend */
    }

    /* effective_task_id = _resolve_container_task_id(task_id) */
    char effective_task_id[128];
    term_resolve_container_task_id(task_id ? task_id : "default",
                                   effective_task_id, sizeof(effective_task_id));

    /* Fast path: already active — mirror terminal_tool and refresh activity. */
    json_t *existing = term_get_active_env(effective_task_id);
    if (existing) {
        /* Refresh last-activity so the idle reaper does not reap a busy env. */
        term_env_set_active(effective_task_id, existing);
        json_free(existing);
        json_free(config);
        return true;
    }

    /* Per-backend image selection honoring registered overrides first. */
    const char *image = "";
    if (strcmp(env_type, "docker") == 0)
        image = json_get_str(config, "docker_image", "");
    else if (strcmp(env_type, "singularity") == 0)
        image = json_get_str(config, "singularity_image", "");
    else if (strcmp(env_type, "modal") == 0)
        image = json_get_str(config, "modal_image", "");
    else if (strcmp(env_type, "daytona") == 0)
        image = json_get_str(config, "daytona_image", "");

    int timeout = (int)json_get_num(config, "timeout", 180);
    const char *cwd = json_get_str(config, "cwd", "");

    /* Per-task creation lock: a concurrent terminal_tool call and this helper
     * must not each spawn a sandbox for the same task. The registry's set/get
     * under its own lock serializes the double-create check below. */
    existing = term_get_active_env(effective_task_id);
    if (existing) {
        json_free(existing);
        json_free(config);
        return true;
    }

    char host_cwd[1024];
    terminal_resolve_task_host_cwd(task_id, host_cwd, sizeof(host_cwd));

    char *env_json = _create_environment(
        env_type,
        image[0] ? image : NULL,
        cwd[0] ? cwd : NULL,
        timeout > 0 ? timeout : 180,
        effective_task_id,
        NULL);
    json_free(config);
    if (!env_json) {
        hermes_log(LOG_WARNING, "terminal",
                   "Lazy %s environment init failed for task %.8s",
                   env_type, effective_task_id);
        return false;
    }
    free(env_json);
    return true;
}

/* PoP: terminal_evict_environment @ tools/terminal_tool.py:_evict_environment_for_task */
/* Drop any cached environment for task_id (and its collapsed key). Used when
 * a backend reports an infrastructure failure: keeping the dead env cached
 * would make every subsequent call fail against a stale connection, defeating
 * automatic recovery. Eviction also drops the last-activity entry so the
 * cleanup reaper cannot resurrect bookkeeping for a dead env. */
void terminal_evict_environment(const char *task_id) {
    if (!task_id || !task_id[0]) task_id = "default";
    char collapsed[128];
    term_resolve_container_task_id(task_id, collapsed, sizeof(collapsed));
    term_cleanup_vm(task_id, true);
    if (strcmp(collapsed, task_id) != 0)
        term_cleanup_vm(collapsed, true);
}
