/* Slermes C port — agent/replay_cleanup.py (pure interrupted-tool-result detector) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* PoP: agent_replay_cleanup_is_interrupted_tool_result @ agent/replay_cleanup.py:is_interrupted_tool_result */
bool agent_replay_cleanup_is_interrupted_tool_result(const char *content)
{
    if (!content) return false;
    char low[8192];
    size_t n = 0;
    for (const char *p = content; *p && n + 1 < sizeof(low); p++)
        low[n++] = (char)tolower((unsigned char)*p);
    low[n] = '\0';
    if (strstr(low, "[command interrupted]")) return true;
    if (strstr(low, "exit_code") && (strstr(low, "130") || strstr(low, "-1"))) {
        if (strstr(low, "interrupt")) return true;
    }
    return false;
}
