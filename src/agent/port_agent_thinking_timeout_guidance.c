/* Slermes C port — agent/thinking_timeout_guidance.py (pure thinking-timeout detector) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* transport-kill substrings (see _THINKING_TIMEOUT_SUBSTRINGS) */
static const char *TT_SUBSTRINGS[] = {
    "broken pipe", "errno 32", "remote protocol", "connection reset",
    "connection lost", "peer closed", "server disconnected", NULL,
};

/* PoP: agent_thinking_timeout_is_thinking_timeout @ agent/thinking_timeout_guidance.py:is_thinking_timeout */
bool agent_thinking_timeout_is_thinking_timeout(const char *reason_value, const char *model, const char *err)
{
    if (!reason_value || strcmp(reason_value, "timeout") != 0) return false;
    /* Condition 3: reasoning-model allowlist (delegated by the build to the
     * reasoning_timeouts lookup; we call it via the provided helper). */
    extern double agent_reasoning_timeouts_get_floor(const char *model);
    if (agent_reasoning_timeouts_get_floor(model) < 0) return false;
    /* Condition 4: transport-kill substring. */
    if (!err) return false;
    char low[8192];
    size_t n = 0;
    for (const char *p = err; *p && n + 1 < sizeof(low); p++) low[n++] = (char)tolower((unsigned char)*p);
    low[n] = '\0';
    for (int i = 0; TT_SUBSTRINGS[i]; i++)
        if (strstr(low, TT_SUBSTRINGS[i])) return true;
    return false;
}
