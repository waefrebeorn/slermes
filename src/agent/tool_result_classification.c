/*
 * tool_result_classification.c — canonical C11 port of agent/tool_result_classification.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_tool_result_classificati_tool_may_have_side_effect @ agent/tool_result_classification.py:tool_may_have_side_effect */
int agent_tool_result_classificati_tool_may_have_side_effect(const char *arg) {
    /* Python: tool_name not in NO_EFFECT_TOOL_NAMES. */
    if (!arg || !*arg) return 1;
    static const char *NO_EFFECT[] = {
        "read_file", "search_files", "session_search", "skill_view", "skills_list",
        "web_extract", "web_search", "vision_analyze", "browser_snapshot",
        "browser_get_images", "browser_console", "read_terminal", NULL
    };
    for (int i = 0; NO_EFFECT[i]; i++)
        if (strcmp(NO_EFFECT[i], arg) == 0) return 0;
    return 1;
}
