/*
 * noc_w25_webhostter_aa608d.h - C surface for upstream
 * hermes_cli/web_host_terminal_ssh.py (gate w25_webhostter_aa608d).
 *
 * A Webapp SSH terminal belongs to the saved connection, not to a remote
 * ``serve`` HTTP endpoint: the shell rides the same SSH master as native
 * Desktop's terminal.  The PTY, the SSH tunnel, the registry file and the
 * websocket are seams; this header owns the value decisions -- which saved
 * connection a terminal is bound to, when that binding still admits more
 * input, and the profile a query string selects.
 */
#ifndef NOC_W25_WEBHOSTTER_AA608D_H
#define NOC_W25_WEBHOSTTER_AA608D_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of connection fields whose change makes a live dial stale. */
#define W25_WEBHOSTTER_AA608D_DIAL_FIELDS 9
/* _profile's fallback when the query string names none. */
#define W25_WEBHOSTTER_AA608D_DEFAULT_PROFILE "current"

/* _profile(query_params) -> (query_params["profile"] or "current").strip() */
json_t *w25_webhostter_aa608d_profile(const json_t *query_params);

/* _entry(registry, connection_id) ->
 * the first registry["connections"] entry whose "id" == connection_id, or
 * null when the registry names no such connection. */
json_t *w25_webhostter_aa608d_entry(const json_t *registry, const json_t *connection_id);

/* connection_dial_fields_changed(before, after) -> true when the two records
 * disagree on the kind, any dial field, the token or the headers. */
json_t *w25_webhostter_aa608d_dial_fields_changed(const json_t *before, const json_t *after);

/* SshOwner.matches(owner, selected, query_params) -> true when the websocket's
 * selected (connection, store path) is this owner's and the profile matches. */
json_t *w25_webhostter_aa608d_matches(const json_t *owner, const json_t *selected,
                                      const json_t *query_params);

/* The dial-field table and profile fallback, as an object. */
json_t *w25_webhostter_aa608d_constants(void);

#ifdef __cplusplus
}
#endif
#endif
