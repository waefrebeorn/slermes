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

#endif /* PORT_WEB_SERVER_WRAPPERS_H */
