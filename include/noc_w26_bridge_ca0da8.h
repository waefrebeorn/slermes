/*
 * noc_w26_bridge_ca0da8.h - C surface for upstream
 * tools/connectors/gateway/bridge.py (gate w26_bridge_ca0da8).
 *
 * The bridge's pure value->value core: which failure a raised gateway error
 * becomes, the two read legs (search and describe) with the gateway client as a
 * fixed answer, the literal-slug retry plan for confirmed misses, and the fence
 * literals.  The client, the availability probe, the file staging and the
 * execute round-trip are seams; what is ported is exactly what their answers
 * decide.
 */
#ifndef NOC_W26_BRIDGE_CA0DA8_H
#define NOC_W26_BRIDGE_CA0DA8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"kind": "auth"|"unavailable"|"other", "code": <str|null>, "status": <n|null>}
 * -> _leg_failure(): "not_set_up" | "sign_in_expired" | "unreachable" | null. */
json_t *w26_bridge_ca0da8_leg_failure(const json_t *req);

/* {"available": bool, "queries": [...], "result": <any|null>,
 *  "raise": {"kind","code","status"}|null} -> connector_search_hits():
 *  {"payload": <the client payload or {}>, "failure": <str|null>}. */
json_t *w26_bridge_ca0da8_search_leg(const json_t *req);

/* {"available": bool, "names": [...], "schemas_response": <any|null>,
 *  "raise": {...}|null} -> connector_describe(): {"payload": {"tools": {...}},
 *  "failure": <str|null>}.  Each tool carries a str description and the
 *  model-facing parameters (file_uploadable inputs become path strings). */
json_t *w26_bridge_ca0da8_describe_leg(const json_t *req);

/* {"planned": [{"connector","tool"}], "entries": [<entry>|null]} -> the
 * literal-vendor retry plan run_remote builds: [{"slot": n, "tool": <literal>}]
 * for each confirmed TOOL_NOT_FOUND miss whose primary slug differs. */
json_t *w26_bridge_ca0da8_fallback_plan(const json_t *req);

/* The fence literals, as an object. */
json_t *w26_bridge_ca0da8_constants(void);

#ifdef __cplusplus
}
#endif
#endif
