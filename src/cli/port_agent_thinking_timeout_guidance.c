/*
 * port_agent_thinking_timeout_guidance.c
 *   C port of agent/thinking_timeout_guidance.py
 *
 * Pure detection + message builder for reasoning-model thinking-timeout
 * guidance. No network, no async. is_thinking_timeout reuses the
 * reasoning-timeout floor resolver (port_agent_reasoning_timeouts.c).
 *
 * The Python classifier passes a duck-typed "classified" object exposing
 * .reason.value; here we pass the reason string directly (the only field
 * read). Returns malloc'd guidance string (caller frees) or NULL on mismatch.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "port_agent_reasoning_timeouts.h"

static const char *THINKING_TIMEOUT_SUBSTRINGS[] = {
    "broken pipe", "errno 32", "remote protocol", "connection reset",
    "connection lost", "peer closed", "server disconnected", NULL
};

/* PoP: thinking_timeout_is @ agent/thinking_timeout_guidance.py:is_thinking_timeout */
/* True when a reasoning model's thinking phase hit a transport kill.
 * reason_value: the classifier reason ("timeout" etc.); model: bare/aggregator
 * slug; error_msg: lowercased error string. */
bool thinking_timeout_is(const char *reason_value, const char *model,
                         const char *error_msg) {
    if (!reason_value || strcasecmp(reason_value, "timeout") != 0)
        return false;
    /* no HTTP status code gate here — caller pre-gates on that. */
    /* condition 3: reasoning model allowlist */
    if (reasoning_timeouts_get_floor(model) < 0)
        return false;
    /* condition 4: transport-kill substring */
    char low[8192];
    size_t i = 0;
    for (; error_msg && error_msg[i] && i + 1 < sizeof(low); i++)
        low[i] = (char)tolower((unsigned char)error_msg[i]);
    low[i] = '\0';
    for (int k = 0; THINKING_TIMEOUT_SUBSTRINGS[k]; k++)
        if (strstr(low, THINKING_TIMEOUT_SUBSTRINGS[k]))
            return true;
    return false;
}

/* PoP: thinking_timeout_guidance @ agent/thinking_timeout_guidance.py:build_thinking_timeout_guidance */
/* Returns malloc'd guidance string (caller frees). */
char *thinking_timeout_guidance(const char *provider, const char *model,
                                 const char *model_label) {
    const char *label = model_label && *model_label ? model_label : (model ? model : "");
    const char *fmt =
        "\n\nThe model's thinking phase exceeded the upstream proxy's "
        "idle timeout before the first content token arrived. This is a "
        "known issue with reasoning models (like %s) behind cloud "
        "gateways (NVIDIA NIM, OpenAI, Anthropic, DeepSeek). Workarounds "
        "in priority order:\n"
        "1. Set `providers.%s.models.%s.stale_timeout_seconds: 900` "
        "in `~/.hermes/config.yaml` to extend the per-call timeout. "
        "(Hermes's built-in floor is 600s for known reasoning models — "
        "if you still see this after raising, the upstream cap is even "
        "shorter.)\n"
        "2. Lower `reasoning_budget` or set `reasoning_effort: medium` on this "
        "model if the provider supports it.\n"
        "3. Use a smaller / faster reasoning model if the task doesn't "
        "require deep thinking.";
    size_t need = strlen(fmt) + strlen(label) + strlen(provider ? provider : "") +
                  strlen(model ? model : "") + 64;
    char *buf = malloc(need);
    if (!buf) return NULL;
    snprintf(buf, need, fmt, label, provider ? provider : "", model ? model : "");
    return buf;
}
