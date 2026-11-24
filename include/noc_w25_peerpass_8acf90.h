/*
 * noc_w25_peerpass_8acf90.h - C surface for upstream
 * hermes_cli/dashboard_auth/peer_pass.py (gate w25_peerpass_8acf90).
 *
 * Bearer check for ``/api/peer/...``: only a pass minted for this agent opens
 * it.  The provider registry, the threadpool hop into ``verify_peer_pass``, the
 * audit log and the response objects are seams; what the values decide is:
 * whether a path belongs to the route, which contract ``/api/status`` reports,
 * and which provider (if any) accepted the token or was unreachable.
 */
#ifndef NOC_W25_PEERPASS_8ACF90_H
#define NOC_W25_PEERPASS_8ACF90_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_PEERPASS_8ACF90_PEER_PREFIX "/api/peer/"
#define W25_PEERPASS_8ACF90_PEER_ACCESS_CONTRACT 1

/* path -> bool: ``path.startswith("/api/peer/")`` (a non-string path raises). */
json_t *w25_peerpass_8acf90_is_peer_path(const json_t *path);

/* providers: [{"name":..., "supports_peer_pass": <any>}] -> PEER_ACCESS_CONTRACT
 * when any provider takes peer passes, else 0. */
json_t *w25_peerpass_8acf90_peer_access_contract(const json_t *providers);

/* providers: [{"name":..., "verify": "accept"|"reject"|"unreachable"}] (the
 * verify_peer_pass outcome, fixed as input), token ->
 * {"accepted": name|null, "unreachable": name|null}: the first accepting
 * provider wins; otherwise the first unreachable provider is reported. */
json_t *w25_peerpass_8acf90_verify(const json_t *providers, const json_t *token);

/* PEER_PREFIX and PEER_ACCESS_CONTRACT. */
json_t *w25_peerpass_8acf90_constants(void);

#ifdef __cplusplus
}
#endif
#endif
