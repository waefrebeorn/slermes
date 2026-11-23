#ifndef NOC_W13_TOOLHISTOR_2D13FE_H
#define NOC_W13_TOOLHISTOR_2D13FE_H

#include "hermes_json.h"

/* PoP port of agent/tool_history_as_text.py (gate w13_toolhistor_2d13fe). */

/* _text_of(content) -> str */
json_t *w13_toolhistor_2d13fe_text_of(json_t *content);
/* tool_history_for_request(messages, tools, tool_choice) -> list[dict] */
json_t *w13_toolhistor_2d13fe_history(json_t *messages, json_t *tools, json_t *tool_choice);

#endif /* NOC_W13_TOOLHISTOR_2D13FE_H */
