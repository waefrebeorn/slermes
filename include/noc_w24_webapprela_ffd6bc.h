/*
 * noc_w24_webapprela_ffd6bc.h - C surface for upstream
 * hermes_cli/webapp_relay_target.py (gate w24_webapprela_ffd6bc).
 *
 * The relay's value layer: the Target bearer parse, what a saved connection
 * names today (url, else host, else id), the resolve decision (none / target /
 * unreachable reason), the own-ticket socket table and its constants.  The
 * registry read, the reach resolver, the SSH tunnel and the ticket POST are
 * seams: they arrive as inputs.
 */
#ifndef W24_WEBAPPRELA_FFD6BC_H
#define W24_WEBAPPRELA_FFD6BC_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* headers: obj -> the bearer behind "Authorization" ("" when absent). */
json_t *w24_webapprela_ffd6bc_bearer(const json_t *headers);

/* entry: obj|null -> entry["url"] or entry["host"] or entry["id"], or null. */
json_t *w24_webapprela_ffd6bc_entry_url(const json_t *entry);

/* req: {"connection_id": str, "entry": obj|null,
 *       "reach": {"kind": "ok"|"nosuch"|"error", "base_url": str,
 *                 "headers": obj, "message": str}}
 * -> {"kind": "none"} | {"kind": "target", "target": {...}} |
 *    {"kind": "error", "message": <Unreachable text>} */
json_t *w24_webapprela_ffd6bc_resolve(const json_t *req);

/* path: str -> the socket's own ticket name, or null. */
json_t *w24_webapprela_ffd6bc_own_ticket(const json_t *path);

/* {"own_ticket_paths": {...}, "session_token_header": str} */
json_t *w24_webapprela_ffd6bc_constants(void);

#ifdef __cplusplus
}
#endif
#endif
