/*
 * port_left_core_migration.h -- pure helpers ported from
 * hermes_cli/left_core_migration.py (the "left core" migration notices:
 * features that moved out of core into plugins).
 *
 * Only the network-free, filesystem-free slice is here; the notice rendering
 * that needs the active home and the plugin registry stays in Python.
 */
#ifndef PORT_LEFT_CORE_MIGRATION_H
#define PORT_LEFT_CORE_MIGRATION_H

#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _toolset_listed -- a toolset in `names` is selected in platform_toolsets
 * (any platform row except `own_platform`) or the top-level toolsets list.
 * `names` carries the already-lowercased toolset names. */
int lcm_toolset_listed(const json_t *config, const char *const *names, size_t n_names,
                       const char *own_platform);

/* _delegation_backend_routes -- every dict entry of each backend's `options`
 * list, across delegation.backends.<name>. Caller owns the array. */
json_t *lcm_delegation_backend_routes(const json_t *delegation);

/* _at_host -- url is at host or a subdomain. A URL the parser rejects (an
 * unclosed IPv6 bracket) is at no host, so one bad slot cannot abort the rest. */
int lcm_at_host(const char *url, const char *host);

#ifdef __cplusplus
}
#endif

#endif /* PORT_LEFT_CORE_MIGRATION_H */
