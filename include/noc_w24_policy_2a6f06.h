/*
 * noc_w24_policy_2a6f06.h - C surface for upstream
 * tools/connectors/portal/policy.py (gate w24_policy_2a6f06).
 *
 * How a member-level connector/tool policy write is composed: which layer key
 * the member body resolves to, the write that flips one connector, and the
 * write that sets one connector's disabled tool slugs.  Pure dict -> dict; the
 * portal client, the HTTP write and the TUI change contracts stay upstream.
 *
 * Errors are returned as strings, matching what upstream raises:
 *   "ERROR:policy:<message>"  InvalidMemberPolicy
 *   "ERROR:keyerror"          _LAYERS[mode] KeyError
 *   "ERROR:type"              TypeError (non-str connector/tool)
 *   "ERROR:attribute"         AttributeError (missing body field)
 */
#ifndef W24_POLICY_2A6F06_H
#define W24_POLICY_2A6F06_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* member: {"mode": str, "connectors": [str], "disabled_connectors": [str]} or
 * null -> {"key": "connectors"|"disabledConnectors", "values": [...],
 * "allow": bool}. */
json_t *w24_policy_2a6f06_layer(const json_t *member);

/* change: {"connector": str, "enabled": bool} ->
 * {"scope": "member", key: [...]}. */
json_t *w24_policy_2a6f06_connector_write(const json_t *member, const json_t *change);

/* change: {"connector": str, "disabled_tools": [str]} ->
 * {"scope": "member", key: [...], "tools": {connector: [...]}}. */
json_t *w24_policy_2a6f06_tools_write(const json_t *member, const json_t *change);

/* The literal layer table, slug pattern and tool-slug cap. */
json_t *w24_policy_2a6f06_constants(void);

#ifdef __cplusplus
}
#endif
#endif
