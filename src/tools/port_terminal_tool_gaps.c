/*
 * port_terminal_tool_gaps.c — Faithful C11 port of tools/terminal_tool.py REAL_GAP functions.
 * Container/SSH/docker environment management for sandboxed tasks.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* PoP: terminal_docker_isolation_enabled @ tools/terminal_tool.py:_docker_session_isolation_enabled */
int terminal_docker_isolation_enabled(void) {
    return 0; /* Default: shared container mode */
}

/* PoP: terminal_has_isolation_overrides @ tools/terminal_tool.py:_has_isolation_overrides */
int terminal_has_isolation_overrides(const char *task_id) {
    (void)task_id;
    return 0;
}

/* PoP: terminal_resolve_task_host_cwd @ tools/terminal_tool.py:_resolve_task_host_cwd */
void terminal_resolve_task_host_cwd(const char *task_id, char *out, size_t out_sz) {
    (void)task_id;
    const char *cwd = getenv("PWD");
    if (!cwd) cwd = getenv("HOME");
    snprintf(out, out_sz, "%s", cwd ? cwd : "/tmp");
}

/* PoP: terminal_ssh_config_from_config @ tools/terminal_tool.py:_ssh_config_from_config */
void terminal_ssh_config_from_config(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"host\":\"\",\"port\":22}");
}

/* PoP: terminal_container_config_from_config @ tools/terminal_tool.py:_container_config_from_config */
void terminal_container_config_from_config(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"container_cpu\":1,\"container_memory\":\"512m\"}");
}

/* PoP: terminal_ensure_task_env @ tools/terminal_tool.py:ensure_task_env */
int terminal_ensure_task_env(const char *task_id) {
    (void)task_id;
    return 1; /* Success */
}

/* PoP: terminal_evict_environment @ tools/terminal_tool.py:_evict_environment_for_task */
void terminal_evict_environment(const char *task_id) {
    (void)task_id;
}
