#ifndef NOC_W22_GATEWAYCON_D39075_H
#define NOC_W22_GATEWAYCON_D39075_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_connections_watch.py (gate w22_gatewaycon_d39075).
 *
 * The module polls gateway-connections.json for edits made by another writer
 * (native Desktop). Everything it *decides* is pure and ported here; the
 * filesystem stat, the store read and the teardown calls stay seams:
 *
 *   - fingerprints: per-entry dial fingerprint -- the JSON dump of
 *     [entry.get(k) for k in _DIAL_FIELDS] with sorted keys and default=str.
 *   - check: one poll step. `(changed, removed)` since the last check, both
 *     empty when the stat stamp did not move or nothing that matters changed.
 *   - teardown_plan: the ordered steps _teardown() performs for those ids --
 *     close sockets for changed+removed, forget removed, disconnect/cancel
 *     sign-in for changed. Each step is one (function, connection id) pair.
 *
 * Stamps are the (st_mtime_ns, st_size, st_ino) tuple _stamp() returns, or
 * null when os.stat raised: {"mtime_ns": <int>, "size": <int>, "ino": <int>}.
 * A registry is {"revision": <int>, "connections": [{"id": ..., ...}]}.
 */

/* PoP: w22_gatewaycon_d39075_poll_interval @ hermes_cli/gateway_connections_watch.py:POLL_INTERVAL_S */
double w22_gatewaycon_d39075_poll_interval(void);

/* PoP: w22_gatewaycon_d39075_dial_fields @ hermes_cli/gateway_connections_watch.py:_DIAL_FIELDS
 * The fields that decide where and how an entry dials, in source order. */
json_t *w22_gatewaycon_d39075_dial_fields(void);

/* PoP: w22_gatewaycon_d39075_fingerprints @ hermes_cli/gateway_connections_watch.py:fingerprints
 * {"id": "<json dump of the dial fields>"} in registry order.
 * -> {"ok": true, "value": {...}} or {"ok": false, "error": "KeyError"|"TypeError"}. */
json_t *w22_gatewaycon_d39075_fingerprints(const json_t *registry);

/* PoP: w22_gatewaycon_d39075_check @ hermes_cli/gateway_connections_watch.py:StoreWatch.check
 * One poll: prev_registry/prev_stamp are what the watch held, new_stamp the
 * fresh _stamp() and new_registry the fresh read.
 * -> {"changed": [...], "removed": [...]} in the previous registry's order,
 *    or {"status": "raised", "type": ...} when the read itself raises. */
json_t *w22_gatewaycon_d39075_check(const json_t *prev_registry, const json_t *prev_stamp,
                                    const json_t *new_stamp, const json_t *new_registry);

/* PoP: w22_gatewaycon_d39075_teardown_plan @ hermes_cli/gateway_connections_watch.py:_teardown
 * [["close_gateway_sockets", id], ["forget", id], ["disconnect", id],
 *  ["cancel_sign_in", id]] in call order. */
json_t *w22_gatewaycon_d39075_teardown_plan(const json_t *changed, const json_t *removed);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_GATEWAYCON_D39075_H */
