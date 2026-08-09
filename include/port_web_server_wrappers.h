/*
 * port_web_server_wrappers.h — Public API for the web-server helper ports.
 *
 * Exposes the topology cache, gateway-platform collection, timezone
 * options, plugins-hub cache invalidation, and check-fn probe scheduling
 * that port_web_server_wrappers.c owns.
 *
 * C11 — callers free returned json_t * with json_free.
 */
#ifndef PORT_WEB_SERVER_WRAPPERS_H
#define PORT_WEB_SERVER_WRAPPERS_H

#include <stdbool.h>
#include "hermes_json.h"

/* Shared reference; NULL on cache miss. */
json_t *ws_topology_cache_get(void *fn);

/* port of _collect_profile_gateway_topology */
json_t *ws_u_collect_profile_gateway_topology(void);

/* port of _collect_profile_gateway_topology_cached */
json_t *ws_collect_profile_gateway_topology_cached(void);

/* port of _load_configured_gateway_platforms */
json_t *ws_load_configured_gateway_platforms(void);

/* port of _timezone_options — ["/usr/share/zoneinfo"-derived names] or ["UTC"] */
json_t *ws_timezone_options(void);

/* port of _invalidate_plugins_hub_cache */
void ws_invalidate_plugins_hub_cache(void);

/* port of _schedule_check_fn_probe */
bool ws_schedule_check_fn_probe(bool (*fn)(void));

/* ── cron dashboard port (web_server.py:_*_cron_job* + _*_cron_*sync) ── */
/* port of _cron_default_profile — malloc'd profile name, caller frees. */
char *ws_u_cron_default_profile(void);
/* port of _cron_profile_home — malloc'd "name\thome" or NULL on 400/404. */
char *ws_u_cron_profile_home(const char *profile_query);
/* port of _cron_profile_dicts — json_t array of {name,home,is_default}. */
json_t *ws_u_cron_profile_dicts(void);
/* port of _annotate_cron_job — annotate a job dict (caller frees result). */
json_t *ws_u_annotate_cron_job(const json_t *job, const char *profile, const char *home);
/* port of _call_cron_for_profile — run a cron method against a profile store. */
json_t *ws_u_call_cron_for_profile(const char *profile, const char *func_name, const char *arg);
/* port of _find_cron_job_profile — malloc'd profile name or NULL. */
char *ws_u_find_cron_job_profile(const char *job_id);
/* port of _list_cron_jobs_sync — json_t array across profile(s). */
json_t *ws_u_list_cron_jobs_sync(const char *profile);
/* port of _list_cron_job_runs_sync — {runs: [], limit: n}. */
json_t *ws_u_list_cron_job_runs_sync(const char *job_id, const char *profile, int limit);
/* port of _get_cron_job_sync — NULL when 404. */
json_t *ws_u_get_cron_job_sync(const char *job_id, const char *profile);
/* port of _create_cron_job_sync — build opts from body JSON. */
json_t *ws_u_create_cron_job_sync(const char *profile, const json_t *body);
/* port of _update_cron_job_sync — NULL when 404. */
json_t *ws_u_update_cron_job_sync(const char *job_id, const char *profile, const json_t *updates);
/* port of pause/resume/trigger/delete cron job sync workers. */
json_t *ws_u_pause_cron_job_sync(const char *job_id, const char *profile);
json_t *ws_u_resume_cron_job_sync(const char *job_id, const char *profile);
json_t *ws_u_trigger_cron_job_sync(const char *job_id, const char *profile);
json_t *ws_u_delete_cron_job_sync(const char *job_id, const char *profile);
/* port of _run_cron_dashboard_io (threadpool seam). */
json_t *ws_u_run_cron_dashboard_io(void);
/* ── Route handlers (web_routers/cron.py) ───────────────────────────── */
json_t *ws_list_cron_jobs(const char *profile);
json_t *ws_get_cron_job(const char *job_id, const char *profile);
json_t *ws_list_cron_job_runs(const char *job_id, const char *profile, int limit);
json_t *ws_create_cron_job(const char *profile, const json_t *body);
json_t *ws_update_cron_job(const char *job_id, const char *profile, const json_t *body);
json_t *ws_pause_cron_job(const char *job_id, const char *profile);
json_t *ws_resume_cron_job(const char *job_id, const char *profile);
json_t *ws_trigger_cron_job(const char *job_id, const char *profile);
json_t *ws_delete_cron_job(const char *job_id, const char *profile);
json_t *ws_get_cron_delivery_targets(void);
/* port of _fire_cron_job_for_profile — resolve + mark run + annotate. */
json_t *ws_u_fire_cron_job_for_profile(const char *profile, const char *job_id);
/* port of list_cron_blueprints — {blueprints:[entry,...]}. */
json_t *ws_list_cron_blueprints(void);
/* port of instantiate_blueprint — fill + create. */
json_t *ws_instantiate_blueprint(const char *blueprint_key, const char *values_json, const char *profile);
/* port of cron_fire_webhook — NAS Chronos fire (401 in native build). */
json_t *ws_cron_fire_webhook(const char *auth_header, const char *body_json);
bool cli_persist_wake_word_enabled(bool enabled);
/* port of _handle_wake_command (cli_commands_mixin.py) */
int cli_handle_wake_command(const char *command);

#endif /* PORT_WEB_SERVER_WRAPPERS_H */
