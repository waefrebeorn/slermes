#ifndef NOC_W12_SKILLMANAG_069E36_H
#define NOC_W12_SKILLMANAG_069E36_H

#include "hermes_json.h"

/* PoP port of tools/skill_manager_batch.py (gate w12_skillmanag_069e36). */

/* _misplaced_text_hint(action, args) -> string (possibly empty). */
json_t *w12_skillmanag_069e36_misplaced_text_hint(json_t *action, json_t *args);

/* _op_shape_error(action, args) -> error string or null. */
json_t *w12_skillmanag_069e36_op_shape_error(json_t *action, json_t *args);

#endif /* NOC_W12_SKILLMANAG_069E36_H */
