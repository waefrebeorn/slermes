/*
 * noc_w28_webserverd_919ecc_p2.h - C surface for the PLUGIN-HUB + PLUGIN-API
 * TRUST partition of upstream hermes_cli/web_server_dashboard.py
 * (gate w28_webserverd_919ecc_p2).
 *
 * Which dashboard plugins exist, how each manifest is shaped for the wire,
 * and every gate a plugin's Python backend must pass before the web server
 * imports it: the enable/disable consent lists, the safe-relative `api` path
 * (GHSA-5qr3-c538-wm9j), the project-source refusal, the mounted/hot-mount
 * decision and the isolation: host forwarding branch.
 *
 * Every seam (the filesystem walk, the plugin registry's cached check_fn
 * verdicts, LEFT_CORE, the app route table, plugin_isolation) arrives as a
 * fixture value.
 */
#ifndef NOC_W28_WEBSERVERD_919ECC_P2_H
#define NOC_W28_WEBSERVERD_919ECC_P2_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _PLUGINS_HUB_CACHE_TTL_SECONDS */
#define W28_WEBSERVERD_919ECC_P2_HUB_CACHE_TTL_SECONDS 5.0

/* A dashboard manifest entry -> the same entry without its "_"-prefixed
 * private keys (_dir, _api_file). */
json_t *w28_webserverd_919ecc_p2_strip_manifest(const json_t *p);

/* {aliases:[str], enabled:[str], disabled:[str]} ->
 * "disabled" | "enabled" | "inactive" (disabled wins). */
json_t *w28_webserverd_919ecc_p2_runtime_status(const json_t *req);

/* {plugin, enabled:[str], disabled:[str]} ->
 * null | "explicitly disabled" | "not in plugins.enabled". */
json_t *w28_webserverd_919ecc_p2_mount_skip_reason(const json_t *req);

/* {data, name, dashboard_dir, source} -> the discovery-time manifest entry:
 * {name, label, description, icon, version, tab{path,position[,override][,hidden]},
 *  slots[], entry, css, has_api, source, _dir, _api_file}. */
json_t *w28_webserverd_919ecc_p2_plugin_entry(const json_t *req);

/* {api_field, dashboard_dir} -> api_field when the resolved path stays inside
 * the plugin's dashboard dir, else null. */
json_t *w28_webserverd_919ecc_p2_safe_api_relpath(const json_t *req);

/* {path, root_path} -> the <name> of an /api/plugins/<name>/... route, else "". */
json_t *w28_webserverd_919ecc_p2_api_name(const json_t *req);

/* {routes:[{path, hot}], name} -> whether the app already serves
 * /api/plugins/<name>/ from anything but the hot-mount fallback. */
json_t *w28_webserverd_919ecc_p2_api_mounted(const json_t *req);

/* {name} -> whether _try_hot_mount_plugin_api would accept the name at all
 * (non-empty, no / \ { }, not ".."). */
json_t *w28_webserverd_919ecc_p2_hot_name_ok(const json_t *req);

/* {name, left_core:[{plugin, cli}]} -> "hermes <cli> login" for a plugin that
 * left core, else "hermes auth <name>". */
json_t *w28_webserverd_919ecc_p2_login_command(const json_t *req);

/* {name, provides_tools:[str], registry:{tool:{check_fn}|null},
 *  cached:{check_fn: true|false|null}} ->
 * {auth_required, auth_command, probes:[check_fn]}: only a cached False
 * requires auth; an uncached verdict is probed off the request path. */
json_t *w28_webserverd_919ecc_p2_auth_hint(const json_t *req);

/* {plugin, enabled, disabled, routes, isolation, api_exists, api_inside,
 *  import_mode} ->
 * {ok, mode: none|in-process|hosted, imported, forwarded}: the
 * _mount_one_plugin_api ladder in order. */
json_t *w28_webserverd_919ecc_p2_mount_decision(const json_t *req);

/* {value} -> the shared truthy env semantics (1/true/yes/on, stripped). */
json_t *w28_webserverd_919ecc_p2_env_truthy(const json_t *req);

/* {config} -> {enabled:[str], disabled:[str]} from plugins.enabled/disabled;
 * a non-list value grants no consent (empty). */
json_t *w28_webserverd_919ecc_p2_enable_sets(const json_t *req);

/* {rows:[{name}], dashboard:[entry]} -> the dashboard entries with no agent
 * plugin of the same name, stripped of private keys. */
json_t *w28_webserverd_919ecc_p2_orphan_dashboard(const json_t *req);

/* {force, has_entry, expiry, now} -> whether the hub read is served from the
 * memo instead of rebuilding. */
json_t *w28_webserverd_919ecc_p2_cache_hit(const json_t *req);

/* {routes:[{hot}]} -> the index new plugin routes are spliced in at (just
 * before the hot-mount fallback, else the end). */
json_t *w28_webserverd_919ecc_p2_splice_index(const json_t *req);

/* The plugin-hub constants of the module, as one object. */
json_t *w28_webserverd_919ecc_p2_constants(void);

#ifdef __cplusplus
}
#endif
#endif
