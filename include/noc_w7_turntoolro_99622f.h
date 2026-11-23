#ifndef NOC_W7_TURNTORO_99622F_H
#define NOC_W7_TURNTORO_99622F_H
#include "hermes_json.h"
#include <stdbool.h>

/* C home for upstream Hermes agent/turn_tool_round.py (gate w7_turntoolro_99622f).
 * Faithful port of the pure classification decisions inside stage_tool_call_message. */

/* PoP: w7_turntoolro_tool_round_facts @ agent/turn_tool_round.py:stage_tool_call_message
 *   all_housekeeping = all(n in _HOUSEKEEPING_TOOLS for n in tool_names)
 *   line_is_reply    = all(n in _LINE_IS_REPLY_TOOLS for n in tool_names)
 *   drop_marker      = bool(tool_names and _STALE_MARKER_RE.fullmatch(content.strip()))
 * tool_names is a JSON array of strings; content is the (possibly NULL) turn content. */
void w7_turntoolro_tool_round_facts(const json_t *tool_names, const char *content,
                                    bool *all_housekeeping, bool *line_is_reply,
                                    bool *drop_marker);

#endif /* NOC_W7_TURNTORO_99622F_H */
