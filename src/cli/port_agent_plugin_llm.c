/*
 * port_agent_plugin_llm.c — C port of agent/plugin_llm.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_plugin_llm_acomplete @ agent/plugin_llm.py:acomplete */

/* Port of Python agent/plugin_llm.py:acomplete */
/* Async completion via the plugin LLM. Delegates to llm_chat_completion. */
char *cli_agent_plugin_llm_acomplete(
    const char **messages, int message_count,
    const char *provider, const char *model,
    float temperature, int max_tokens, float timeout)
{
    if (!messages || message_count <= 0) {
        return strdup("{\"error\":\"No messages provided\"}");
    }

    hermes_log(LOG_DEBUG, "plugin_llm", "acomplete: provider=%s model=%s",
        provider ? provider : "(default)", model ? model : "(default)");

    /* The plugin LLM requires the async client integration that is not present
     * in the C port. Return an honest not_implemented status rather than fake
     * completions. */
    size_t buf_size = 512;
    char *result = (char *)malloc(buf_size);
    if (!result) return NULL;

    snprintf(result, buf_size,
        "{\"status\":\"not_implemented\",\"message\":\"Plugin LLM acomplete requires async client integration\"}");
    return result;
}

/* PoP: cli_agent_plugin_llm_acomplete_structured @ agent/plugin_llm.py:acomplete_structured */

/* Port of Python agent/plugin_llm.py:acomplete_structured */
/* Async structured completion via the plugin LLM. */
char *cli_agent_plugin_llm_acomplete_structured(
    const char *instructions, const char **inputs, int input_count,
    const char *json_schema, int json_mode,
    const char *provider, const char *model)
{
    if (!instructions || !*instructions) {
        return strdup("{\"error\":\"acomplete_structured requires non-empty instructions\"}");
    }
    if (!inputs || input_count <= 0) {
        return strdup("{\"error\":\"acomplete_structured requires at least one input block\"}");
    }

    hermes_log(LOG_DEBUG, "plugin_llm", "acomplete_structured: provider=%s json_mode=%d",
        provider ? provider : "(default)", json_mode);

    size_t buf_size = 512;
    char *result = (char *)malloc(buf_size);
    if (!result) return NULL;

    snprintf(result, buf_size,
        "{\"status\":\"not_implemented\",\"message\":\"Plugin LLM acomplete_structured requires async client integration\"}");
    return result;
}

/* PoP: cli_agent_plugin_llm__invoke_async @ agent/plugin_llm.py:_invoke_async */

/* Port of Python agent/plugin_llm.py:_invoke_async */
/* Internal async invocation — delegates to the async caller or auxiliary_client. */
char *cli_agent_plugin_llm__invoke_async(
    const char **messages, int message_count,
    const char *provider_override, const char *model_override,
    float temperature, int max_tokens, float timeout)
{
    if (!messages || message_count <= 0) {
        return strdup("{\"error\":\"No messages for async invocation\"}");
    }

    hermes_log(LOG_DEBUG, "plugin_llm",
        "_invoke_async: provider_override=%s model_override=%s",
        provider_override ? provider_override : "(none)",
        model_override ? model_override : "(none)");

    /* The plugin LLM's async invocation requires auxiliary_client, which is
     * not ported to C. Return an honest not_implemented status. */
    size_t buf_size = 512;
    char *result = (char *)malloc(buf_size);
    if (!result) return NULL;

    snprintf(result, buf_size,
        "{\"status\":\"not_implemented\",\"message\":\"Plugin LLM async invocation requires auxiliary_client\"}");
    return result;
}
