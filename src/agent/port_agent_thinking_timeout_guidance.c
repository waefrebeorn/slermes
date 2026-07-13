/* Slermes C port — agent/thinking_timeout_guidance.py (pure thinking-timeout detector) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

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

/* PoP: agent_thinking_timeout_build_thinking_timeout_guidance @ agent/thinking_timeout_guidance.py:build_thinking_timeout_guidance */
char *agent_thinking_timeout_build_thinking_timeout_guidance(const char *provider, const char *model, const char *model_label)
{
    const char *label = model_label && model_label[0] ? model_label : (model ? model : "");
    const char *provider_s = provider ? provider : "";
    const char *model_s = model ? model : "";
    /* Faithful to the Python f-string template (3 workaround lines). */
    size_t need = 1024 + strlen(label) + strlen(provider_s) + strlen(model_s) * 2;
    char *out = (char *)malloc(need);
    if (!out) return strdup("");
    snprintf(out, need,
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
        "require deep thinking.",
        label, provider_s, model_s);
    return out;
}
