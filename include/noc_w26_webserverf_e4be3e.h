/*
 * noc_w26_webserverf_e4be3e.h - C surface for upstream
 * hermes_cli/web_server_file_tickets.py (gate w26_webserverf_e4be3e).
 *
 * The path-bound ticket store as a value machine: a ticket's scope is the route
 * plus the sorted query pairs, minting stamps an expiry and a download use
 * budget, and redeeming spends one use only for the exact scope.  The clock,
 * the token generator, the credential generation and the in-memory store all
 * arrive as inputs; nothing here touches a lock, a request or a file.
 */
#ifndef NOC_W26_WEBSERVERF_E4BE3E_H
#define NOC_W26_WEBSERVERF_E4BE3E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* (route_path, [[k,v],...]) -> {"route": str, "query": [[k,v],...]} sorted. */
json_t *w26_webserverf_e4be3e_scope(const char *route_path, const json_t *query);

/* {route, query, store, now, token, session_token, ttl_download, ttl_stream,
 *  max_tickets} -> {"ticket": str, "store": {...}} */
json_t *w26_webserverf_e4be3e_mint(const json_t *req);

/* {store, method, path, query, now, current} ->
 * {"ok": bool, "store": {...}, "authority": bool} */
json_t *w26_webserverf_e4be3e_redeem(const json_t *req);

/* TTLs, redemption cap, store cap and the route table. */
json_t *w26_webserverf_e4be3e_constants(void);

#ifdef __cplusplus
}
#endif
#endif
