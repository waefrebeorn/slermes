/*
 * port_terminal_tool.c
 *
 * Pure helpers ported from tools/terminal_tool.py (63 defs, previously 0
 * ported).
 *
 *   tt_volume_uses_host_path <- _docker_volume_uses_host_path
 *   tt_has_host_access       <- _docker_has_host_access
 *   tt_is_isolation_key      <- _is_isolation_override_key
 *   tt_qualify_task_key      <- _qualify_task_key
 *   tt_resolve_container_alias <- _resolve_container_alias
 *
 * The threading / callback / registry half stays in Python. The registry-backed
 * predicates take their state as parameters rather than owning globals.
 */

#include "port_terminal_tool.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 *  _docker_volume_uses_host_path  (terminal_tool.py:_docker_volume_uses_host_path)
 *
 *      if not isinstance(volume_spec, str): return False
 *      vol = volume_spec.strip()
 *      return bool(vol) and (
 *          vol.startswith(("/", "~", "./", "../")) or
 *          (len(vol) >= 3 and vol[1] == ":" and vol[2] in ("/", "\\")))
 *
 *  Two forms count as a host path: a POSIX/relative prefix, or a Windows drive
 *  specifier (`C:\...` / `C:/...`). A bare named volume (`mydata:/x`) does not.
 * =========================================================================== */

/* PoP: _docker_volume_uses_host_path @ tools/terminal_tool.py:_docker_volume_uses_host_path */
int tt_volume_uses_host_path(const char *volume_spec)
{
    if (!volume_spec) return 0;
    const char *vol = volume_spec;
    while (*vol && isspace((unsigned char)*vol)) vol++;
    size_t len = strlen(vol);
    while (len > 0 && isspace((unsigned char)vol[len - 1])) len--;
    if (len == 0) return 0;

    if (vol[0] == '/' || vol[0] == '~') return 1;
    if (len >= 2 && vol[0] == '.' && (vol[1] == '/' || (vol[1] == '.' && len >= 3 && vol[2] == '/'))) return 1;
    if (len >= 3 && vol[1] == ':' && (vol[2] == '/' || vol[2] == '\\')) return 1;
    return 0;
}

/* ===========================================================================
 *  _docker_has_host_access  (terminal_tool.py:_docker_has_host_access)
 *
 *      if config.get("env_type") != "docker": return False
 *      if config.get("host_cwd") and config.get("docker_mount_cwd_to_workspace"):
 *          return True
 *      return any(_docker_volume_uses_host_path(vol)
 *                 for vol in config.get("docker_volumes", []))
 *
 *  `config` is a flat [key, value, ...] array. Values are the raw strings;
 *  a missing key reads as None (falsy).
 * =========================================================================== */

static const char *tt_cfg_get(const char *const *config, const char *key)
{
    for (int i = 0; config && config[i]; i += 2)
        if (strcmp(config[i], key) == 0) return config[i + 1];
    return NULL;
}

/* PoP: _docker_has_host_access @ tools/terminal_tool.py:_docker_has_host_access */
int tt_has_host_access(const char *const *config)
{
    const char *env_type = tt_cfg_get(config, "env_type");
    if (!env_type || strcmp(env_type, "docker") != 0) return 0;

    const char *host_cwd = tt_cfg_get(config, "host_cwd");
    const char *mount_cwd = tt_cfg_get(config, "docker_mount_cwd_to_workspace");
    if (host_cwd && host_cwd[0] && mount_cwd && mount_cwd[0]) return 1;

    const char *vols = tt_cfg_get(config, "docker_volumes");
    /* docker_volumes is a list; the harness supplies it as a
     * NUL-terminated array of volume specs in *out_vols. */
    if (vols) {
        const char *const *list = (const char *const *)vols;
        for (int i = 0; list[i]; i++)
            if (tt_volume_uses_host_path(list[i])) return 1;
    }
    return 0;
}

/* ===========================================================================
 *  _is_isolation_override_key  (terminal_tool.py:_is_isolation_override_key)
 *
 *      return key == "env_type" or key.endswith("_image")
 * =========================================================================== */

/* PoP: _is_isolation_override_key @ tools/terminal_tool.py:_is_isolation_override_key */
int tt_is_isolation_key(const char *key)
{
    if (!key) return 0;
    if (strcmp(key, "env_type") == 0) return 1;
    size_t n = strlen(key);
    if (n < 6) return 0;
    return strcmp(key + n - 6, "_image") == 0;
}

/* ===========================================================================
 *  _qualify_task_key  (terminal_tool.py:_qualify_task_key)
 *
 *      scope = _routed_home_task_key(profile_scoped=True)
 *      return f"{scope}:{key}" if scope else key
 *
 *  `scope` stands in for the routed-home prefix; NULL/empty means the CLI or a
 *  single-profile gateway, which keep the raw key. Caller frees.
 * =========================================================================== */

/* PoP: _qualify_task_key @ tools/terminal_tool.py:_qualify_task_key */
char *tt_qualify_task_key(const char *key, const char *scope)
{
    if (!scope || !scope[0]) return strdup(key ? key : "");
    size_t n = strlen(scope) + 1 + strlen(key ? key : "") + 1;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "%s:%s", scope, key ? key : "");
    return out;
}

/* ===========================================================================
 *  _resolve_container_alias  (terminal_tool.py:_resolve_container_alias)
 *
 *      seen = set()
 *      key = task_id
 *      while key in _container_aliases and key not in seen:
 *          seen.add(key)
 *          key = _container_aliases[key]
 *      return key
 *
 *  Cycle-safe: a loop terminates on the key that was already visited, so
 *  `a -> b -> a` resolves to `a` rather than hanging. `aliases` is a flat
 *  [child, parent, ...] array. Caller frees.
 * =========================================================================== */

/* PoP: _resolve_container_alias @ tools/terminal_tool.py:_resolve_container_alias */
char *tt_resolve_container_alias(const char *task_id, const char *const *aliases)
{
    const char *key = task_id;
    if (!key) return NULL;

    const char **seen = (const char **)calloc(64, sizeof(char *));
    if (!seen) return strdup(key);
    int nseen = 0;

    for (;;) {
        const char *parent = NULL;
        for (int i = 0; aliases && aliases[i]; i += 2) {
            if (strcmp(aliases[i], key) == 0) { parent = aliases[i + 1]; break; }
        }
        if (!parent) break;
        int already = 0;
        for (int i = 0; i < nseen; i++) if (strcmp(seen[i], key) == 0) { already = 1; break; }
        if (already) break;
        if (nseen < 64) seen[nseen++] = key;
        key = parent;
    }
    free(seen);
    return strdup(key);
}
