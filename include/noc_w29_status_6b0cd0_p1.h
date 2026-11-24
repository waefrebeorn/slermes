/*
 * noc_w29_status_6b0cd0_p1.h - C surface for upstream
 * hermes_cli/web_routers/status.py, gate p1 (gateway/platform projection and
 * the health rollup).
 *
 * Every HTTP/DB/process probe in the module is a fixture seam here: no socket
 * is opened, no state.db is read and no thread pool is started.  What this
 * gate owns is the value arithmetic the /api/status handler runs on the
 * results of those probes --
 *   * the ``<profile>:<platform>`` key grammar and the configured-set gate,
 *   * writer-identity stripping from a public platform entry,
 *   * the per-profile gateway platform merge (multiplexed profiles),
 *   * the public platform projection (stale/startup_failed clearing),
 *   * the gateway liveness/state/staleness ladder,
 *   * the component health rollup and its platform counts,
 *   * the all-components ``overall`` verdict, and
 *   * deferred FTS rebuild progress.
 */
#ifndef NOC_W29_STATUS_6B0CD0_P1_H
#define NOC_W29_STATUS_6B0CD0_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w29_status_6b0cd0_is_key @ hermes_cli/web_routers/status.py:_is_profile_platform_status_key
 * key: any JSON value -> the runner's public ``<profile>:<platform>`` grammar
 * (``^[a-z0-9][a-z0-9_-]{0,63}:[a-z0-9][a-z0-9_-]{0,63}$``). */
json_t *w29_status_6b0cd0_is_key(const json_t *key);

/* PoP: w29_status_6b0cd0_key_allowed @ hermes_cli/web_routers/status.py:_status_platform_key_allowed
 * Namespaced keys are validated against the grammar unconditionally; plain
 * keys pass when ``configured`` did not load (null/absent) and are members of
 * it otherwise.  Non-string keys never pass. */
json_t *w29_status_6b0cd0_key_allowed(const json_t *key, const json_t *configured);

/* PoP: w29_status_6b0cd0_public_entry @ hermes_cli/web_routers/status.py:_public_platform_entry
 * Drops the writer_pid / writer_start_time recon stamps from a platform entry
 * object; a non-object value projects unchanged. */
json_t *w29_status_6b0cd0_public_entry(const json_t *value);

/* PoP: w29_status_6b0cd0_merge_platforms @ hermes_cli/web_routers/status.py:_merge_profile_gateway_platforms
 * gateway_platforms: the machine-level map; profile_platforms: each profile's
 * own map; active: the active profile NAME (the live ``get_active_profile_name``
 * seam).  New ``<profile>:<platform>`` keys are added only when absent. */
json_t *w29_status_6b0cd0_merge_platforms(const json_t *gateway_platforms,
                                          const json_t *profile_platforms,
                                          const json_t *active);

/* PoP: w29_status_6b0cd0_project_platforms @ hermes_cli/web_routers/status.py:_project_gateway_platforms
 * Public projection of a runtime's platform map: allowed keys only; cleared
 * when the gateway is stopped, fatal-only when it startup_failed. */
json_t *w29_status_6b0cd0_project_platforms(const json_t *gateway_platforms,
                                            const json_t *configured,
                                            const json_t *gateway_running,
                                            const json_t *gateway_state);

/* PoP: w29_status_6b0cd0_gateway_view @ hermes_cli/web_routers/status.py:_resolve_gateway_status
 * req: {local_runtime, remote_body, gateway_running, pid, configured,
 *       retained_state, stale, heartbeat_age}
 * The state ladder after the liveness ladder: retained/durable state when the
 * PID is gone, remote 'running' override, heartbeat staleness, platform
 * projection, stopped-cleared exit reason, multiplexer served_profiles.
 * ``gateway_updated_at`` stays on the normalize_updated_at seam. */
json_t *w29_status_6b0cd0_gateway_view(const json_t *req);

/* PoP: w29_status_6b0cd0_component_health @ hermes_cli/web_routers/status.py:_component_health
 * req: {gateway_running, gateway_state, gateway_platforms, dashboard, storage,
 *       storage_error} -> components {gateway, dashboard, storage, platforms}
 * with status enums and platform counts only. */
json_t *w29_status_6b0cd0_component_health(const json_t *req);

/* PoP: w29_status_6b0cd0_overall @ hermes_cli/web_routers/status.py:get_status
 * The inline verdict: "ok" only when every component's status is "ok". */
json_t *w29_status_6b0cd0_overall(const json_t *components);

/* PoP: w29_status_6b0cd0_fts_rebuild @ hermes_cli/web_routers/status.py:_fts_rebuild_status
 * req: {store_exists, rebuild: {total, indexed} | null} -> {pending, total,
 * indexed, percent} with percent clamped at 100, or null. */
json_t *w29_status_6b0cd0_fts_rebuild(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W29_STATUS_6B0CD0_P1_H */
