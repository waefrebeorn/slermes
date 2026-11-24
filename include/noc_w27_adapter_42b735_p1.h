/*
 * noc_w27_adapter_42b735_p1.h - C surface for slice 1 of upstream
 * plugins/platforms/whatsapp/adapter.py (gate w27_adapter_42b735_p1):
 * bridge token + spawn-config identity.
 *
 * _safe_ints, _bridge_auth_headers, _bridge_auth_proof (HMAC-SHA256),
 * _bridge_config_fingerprint (SHA-256 over the consumed-env key set) and
 * _fmt_uptime.  Each symbol takes the JSON-lines request object and returns
 * the Python-equivalent JSON value of the same-named upstream function
 * applied to the keys named after its parameters.  See adapter.c for the PoP
 * map; the socket/file seams stay in the caller.
 */
#ifndef NOC_W27_ADAPTER_42B735_P1_H
#define NOC_W27_ADAPTER_42B735_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req.tokens: [any] -> [int] (int(tok) for every token that parses; the rest
 * are skipped, exactly like the ValueError-swallowing loop). */
json_t *w27_adapter_42b735_p1_safe_ints(const json_t *req);

/* req.token -> {"Authorization": "Bearer <token>"}. */
json_t *w27_adapter_42b735_p1_auth_headers(const json_t *req);

/* req.token, req.challenge -> hex HMAC-SHA256(token, challenge). */
json_t *w27_adapter_42b735_p1_auth_proof(const json_t *req);

/* req.env (the spawn env object), req.package_hash -> first 16 hex chars of
 * the SHA-256 over package.json hash + every _BRIDGE_CONSUMED_ENV key with a
 * presence-marker byte. */
json_t *w27_adapter_42b735_p1_config_fingerprint(const json_t *req);

/* req.seconds -> "2d 16h" / "16h 4m" / "4m" / "9s" / "" (unknown). */
json_t *w27_adapter_42b735_p1_fmt_uptime(const json_t *req);

/* Dispatch by request "func"; returns "UNKNOWN_FUNC" when absent. */
json_t *w27_adapter_42b735_p1_dispatch(const char *func, const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_42B735_P1_H */
