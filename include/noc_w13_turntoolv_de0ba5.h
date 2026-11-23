#ifndef NOC_W13_TURNTOOLV_DE0BA5_H
#define NOC_W13_TURNTOOLV_DE0BA5_H

#include "hermes_json.h"

/* PoP port of agent/turn_tool_validation.py (gate w13_turntoolv_de0ba5). */

/* _preview_name(name) -> str */
json_t *w13_turntoolv_de0ba5_preview_name(json_t *name);

/* validate_tool_calls() decision facts with repair/deferred routing stubbed off:
 * -> {"action":str, "mixed":bool, "result":obj|null} */
json_t *w13_turntoolv_de0ba5_validate_facts(json_t *tool_calls, json_t *valid_names,
        json_t *invalid_tool_retries, json_t *invalid_json_retries, json_t *finish_reason);

#endif /* NOC_W13_TURNTOOLV_DE0BA5_H */
