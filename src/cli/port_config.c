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
#include <ctype.h>
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

/* Port of Python: redact_config_value
 * Recursively walk dicts/lists; any key (case-insensitive) in the secret set
 * with a non-empty string value is masked via mask_secret(head=4,tail=4,floor=12).
 * Mirrors hermes_cli/config.py:_SECRET_CONFIG_KEYS + agent.redact.mask_secret. */
static int redact_is_secret_key(const char *key) {
    static const char *SECRET[] = {
        "api_key","apikey","key","token","access_token","refresh_token","id_token",
        "secret","client_secret","password","passwd","auth","authorization",
        "private_key","bearer","jwt", NULL
    };
    if (!key) return 0;
    size_t n = strlen(key);
    char *low = malloc(n + 1);
    if (!low) return 0;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)key[i]);
    low[n] = '\0';
    int hit = 0;
    for (int i = 0; SECRET[i]; i++) if (strcmp(low, SECRET[i]) == 0) { hit = 1; break; }
    free(low);
    return hit;
}

/* Mask a secret: empty -> ""; len<12 -> "***"; else head4...tail4. */
static char *redact_mask_secret(const char *v) {
    size_t n = strlen(v);
    if (n == 0) return strdup("");
    if (n < 12) return strdup("***");
    size_t need = 4 + 3 + 4 + 1;
    char *out = malloc(need);
    if (!out) return NULL;
    memcpy(out, v, 4);
    out[4] = '.'; out[5] = '.'; out[6] = '.';
    memcpy(out + 7, v + n - 4, 4);
    out[need - 1] = '\0';
    return out;
}

static json_t *redact_config_value_impl(json_t *value, int depth) {
    if (!value) return json_new_object();
    if (depth > 20) return json_copy(value);
    if (value->type == JSON_OBJECT) {
        json_t *out = json_new_object();
        if (!out) return NULL;
        size_t n = json_object_size(value);
        for (size_t idx = 0; idx < n; idx++) {
            const char *k = json_object_get_key_at(value, idx);
            json_t *v = json_object_get_at(value, idx);
            if (!k || !v) continue;
            if (redact_is_secret_key(k) && v->type == JSON_STRING && json_string_value(v)[0]) {
                char *m = redact_mask_secret(json_string_value(v));
                json_object_set(out, k, json_new_string(m ? m : "***"));
                free(m);
            } else {
                json_object_set(out, k, redact_config_value_impl(v, depth + 1));
            }
        }
        return out;
    }
    if (value->type == JSON_ARRAY) {
        json_t *out = json_new_array();
        if (!out) return NULL;
        size_t n = json_array_size(value);
        for (size_t i = 0; i < n; i++) {
            json_t *v = json_array_get(value, i);
            json_array_append(out, redact_config_value_impl(v, depth + 1));
        }
        return out;
    }
    return json_copy(value);
}

json_t* redact_config_value(json_t* value, int depth)
{
    return redact_config_value_impl(value, depth);
}
