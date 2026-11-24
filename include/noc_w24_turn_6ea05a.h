/*
 * noc_w24_turn_6ea05a.h - C surface for upstream tools/connectors/turn.py
 * (gate w24_turn_6ea05a).
 *
 * The pure part of the connector "surface" tag: which surface an agent is
 * (a card with a connection callback, a side agent, or a plain link), which
 * tools a side agent loses, and the context-variable walk that scopes a
 * surface around a block of work.  The agent objects are values here
 * ({"connection_callback": <any>, "side_agent": <any>}), the ContextVar is a
 * plain value carried through the walk, and nothing is stored between calls.
 */
#ifndef W24_TURN_6EA05A_H
#define W24_TURN_6EA05A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Surface tags, as upstream spells them. */
#define W24_TURN_6EA05A_CARD "card"
#define W24_TURN_6EA05A_SIDE "side"
#define W24_TURN_6EA05A_LINK "link"

/* agent: {"connection_callback": <any>, "side_agent": <any>} ->
 * "card" when a connection_callback attribute is present and is not None,
 * else "side" when side_agent is truthy, else "link". */
json_t *w24_turn_6ea05a_agent_surface(const json_t *agent);

/* agent -> ["manage_connections"] for a side agent, else []. */
json_t *w24_turn_6ea05a_tool_drops(const json_t *agent);

/* ops: [{"op":"get"} | {"op":"scope","surface":<any>,"ops":[...]}, ...] ->
 * one result per op: "get" yields the current surface value, "scope" yields
 * {"inside": [inner results], "after": <restored value>}.  Unknown ops yield
 * "UNKNOWN_OP". */
json_t *w24_turn_6ea05a_surface_scope(const json_t *ops);

/* The surface tags, the default surface and the side-agent drop set. */
json_t *w24_turn_6ea05a_constants(void);

#ifdef __cplusplus
}
#endif
#endif
