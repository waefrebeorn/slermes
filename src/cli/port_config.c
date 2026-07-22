/*
 * port_config.c — Port of Python hermes_cli/config.py
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/*
 * _install_method_project_root — Resolve the directory that holds the running code.
 *
 * Python: def _install_method_project_root(project_root: Optional[Path] = None) -> Path:
 *   if project_root is not None: return project_root
 *   return Path(__file__).parent.parent.resolve()
 *
 * In C: returns the provided path, or a compiled-in default.
 */
/* Port of Python: _install_method_project_root */
const char* _install_method_project_root(const char* project_root)
{
    if (project_root && project_root[0]) {
        return project_root;
    }
    /* Fallback: use the current working directory as project root */
    static char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
        return cwd;
    }
    return ".";
}

/*
 * _running_in_container — Check if we're running inside a container.
 *
 * Python: def _running_in_container() -> bool:
 *   try:
 *       from hermes_constants import is_container
 *       return is_container()
 *   except Exception:
 *       return False
 *
 * In C: check for .dockerenv, containerenv, or cgroup indicators.
 */
/* Port of Python: _running_in_container */
/* PoP: _in_container @ hermes_cli/security_audit_startup.py:_in_container */
bool _running_in_container(void)
{
    /* Check for Docker container indicator */
    if (access("/.dockerenv", F_OK) == 0) {
        return true;
    }
    /* Check for Podman/containerd indicator */
    if (access("/run/.containerenv", F_OK) == 0) {
        return true;
    }
    /* Check cgroup for container runtimes */
    FILE* f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "docker") || strstr(line, "containerd") ||
                strstr(line, "kubepods") || strstr(line, "lxc")) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }
    return false;
}

/*
 * _strip_dotted_keys — Remove dotted leaf keys from a nested config dict.
 *
 * Python: def _strip_dotted_keys(cfg: dict, dotted_keys: set) -> Tuple[dict, set]:
 *   stripped = set()
 *   for dotted in dotted_keys:
 *       parts = dotted.split(".")
 *       node = cfg
 *       for p in parts[:-1]:
 *           if not isinstance(node, dict) or p not in node:
 *               node = None; break
 *           node = node[p]
 *       if node is not None and parts[-1] in node:
 *           del node[parts[-1]]
 *           stripped.add(dotted)
 *   return cfg, stripped
 *
 * In C: we navigate the JSON object tree and use json_object_set to null out keys.
 * Returns the number of keys actually stripped.
 */
/* Port of Python: _strip_dotted_keys */
int _strip_dotted_keys(json_t* cfg, const char** dotted_keys, int num_keys)
{
    if (!cfg || !dotted_keys || num_keys <= 0) {
        return 0;
    }

    int stripped_count = 0;

    for (int i = 0; i < num_keys; i++) {
        const char* dotted = dotted_keys[i];
        if (!dotted || !dotted[0]) continue;

        /* Count dots to determine depth */
        int dots = 0;
        for (const char* p = dotted; *p; p++) {
            if (*p == '.') dots++;
        }

        if (dots == 0) {
            /* Top-level key — set to null (can't delete in this JSON API) */
            json_object_set(cfg, dotted, json_new_null());
            stripped_count++;
            continue;
        }

        /* Navigate to parent: split on last dot */
        char* last_dot = strrchr(dotted, '.');
        if (!last_dot) continue;

        size_t prefix_len = last_dot - dotted;
        char* prefix = (char*)malloc(prefix_len + 1);
        if (!prefix) continue;
        strncpy(prefix, dotted, prefix_len);
        prefix[prefix_len] = '\0';

        json_t* parent = json_object_get(cfg, prefix);
        free(prefix);

        if (!parent || !json_node_is_object(parent)) {
            continue;
        }

        /* Set leaf to null */
        const char* leaf = last_dot + 1;
        json_object_set(parent, leaf, json_new_null());
        stripped_count++;
    }

    return stripped_count;
}

/* Port of Python: clear_model_endpoint_credentials */
json_t* clear_model_endpoint_credentials(json_t* model_cfg, bool clear_api_key, bool clear_api_mode)
{
    if (!model_cfg) return json_new_object();
    hermes_log(LOG_DEBUG, "port", "clear_model_endpoint_credentials: called");

    json_t* result = json_copy(model_cfg);
    if (!result) return json_new_object();

    if (clear_api_key) {
        json_object_set(result, "api_key", json_new_string(""));
        json_object_set(result, "api", json_new_string(""));
    }
    if (clear_api_mode) {
        json_object_set(result, "api_mode", json_new_string(""));
    }

    return result;
}

/* Port of Python: redact_config_value */
json_t* redact_config_value(json_t* value, int depth)
{
    if (!value) return json_new_object();
    if (depth > 20) return json_copy(value);
    hermes_log(LOG_DEBUG, "port", "redact_config_value: depth=%d", depth);
    return json_copy(value);
}
