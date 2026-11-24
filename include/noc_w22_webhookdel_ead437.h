#ifndef NOC_W22_WEBHOOKDEL_EAD437_H
#define NOC_W22_WEBHOOKDEL_EAD437_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/webhook_delivery.py (gate w22_webhookdel_ead437).
 *
 * `validate_destination` is the module's entry point (it is called on every
 * delivery and on every readback) and is ported whole, refusals included.
 * `snapshot_destination` adds one adapter lookup -- the home channel of an
 * unaddressed target -- which is the seam, so the caller passes the chat id.
 * `route_digest` hashes a route; the adapter's `_reload_dynamic_routes()` /
 * `_routes.get()` / `route_from_chat_id` lookups around it are the seam, so the
 * route object comes in already resolved.
 *
 * Two platform-table lookups are seams too: the builtin delivery platforms are
 * ported, `platform_registry`-registered plugin names arrive as
 * `extra_platforms`.
 *
 * A refusal is `json_string("invalid_params")` / `json_string("not_found")`;
 * success is the validated destination object.
 */

/* PoP: w22_webhookdel_ead437_validate_destination @ gateway/platforms/webhook_delivery.py:validate_destination
 * The destination row persisted for a delivery: the required keys, the closed
 * key set, the per-field type checks, the github_comment repo/number rule, the
 * known-platform rule, and the per-target extra-field whitelist (credentials
 * and rendered payloads are dropped). */
json_t *w22_webhookdel_ead437_validate_destination(const json_t *delivery,
                                                   const json_t *extra_platforms);

/* PoP: w22_webhookdel_ead437_snapshot_destination @ gateway/platforms/webhook_delivery.py:snapshot_destination
 * `validate_destination` then, for a platform target with no chat id, the home
 * channel's chat id (`home_chat_id`); NULL means there is no home channel and
 * the delivery is refused with "not_found". */
json_t *w22_webhookdel_ead437_snapshot_destination(const json_t *delivery,
                                                   const json_t *extra_platforms,
                                                   const char *home_chat_id);

/* PoP: w22_webhookdel_ead437_route_digest @ gateway/platforms/webhook_delivery.py:route_digest
 * `sha256(json.dumps(route, sort_keys=True, separators=(",", ":")))` hex.
 * NULL when the route was not found (the resolver refused with "not_found").
 * Caller frees the 64-char hex string. */
char *w22_webhookdel_ead437_route_digest(const json_t *route);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_WEBHOOKDEL_EAD437_H */
