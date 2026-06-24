/*
 * port_tools_mixture_of_agents_tool.c — C port of tools/mixture_of_agents_tool.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_mixture_of_agents_tool__construct_aggregator_prompt @ tools/mixture_of_agents_tool.py:_construct_aggregator_prompt */

/* Port of Python tools/mixture_of_agents_tool.py:_construct_aggregator_prompt */
/* Constructs the final system prompt for the aggregator including all model responses. */
int cli_tools_mixture_of_agents_tool__construct_aggregator_prompt(
    const char *system_prompt, const char **responses, int response_count,
    char *output, size_t output_size)
{
    if (!system_prompt || !responses || !output || output_size == 0) {
        return -1;
    }
    size_t offset = 0;
    int n = snprintf(output + offset, output_size - offset, "%s\n\n", system_prompt);
    if (n < 0 || (size_t)n >= output_size - offset) return -1;
    offset += (size_t)n;
    for (int i = 0; i < response_count; i++) {
        if (responses[i]) {
            n = snprintf(output + offset, output_size - offset, "%d. %s\n", i + 1, responses[i]);
            if (n < 0 || (size_t)n >= output_size - offset) return -1;
            offset += (size_t)n;
        }
    }
    return 0;
}

/* PoP: cli_tools_mixture_of_agents_tool__run_reference_model_safe @ tools/mixture_of_agents_tool.py:_run_reference_model_safe */

/* Port of Python tools/mixture_of_agents_tool.py:_run_reference_model_safe */
/* Runs a single reference model with retry logic and graceful failure handling. */
/* Returns 0 on success, -1 on failure. */
int cli_tools_mixture_of_agents_tool__run_reference_model_safe(
    const char *model, const char *user_prompt,
    float temperature, int max_tokens, int max_retries,
    char *response_out, size_t response_size)
{
    if (!model || !user_prompt || !response_out || response_size == 0) {
        return -1;
    }
    (void)temperature;
    (void)max_tokens;
    (void)max_retries;
    /* CLI port: API calls are handled by the gateway. Return failure. */
    hermes_log(LOG_WARNING, "moa",
               "MoA: reference model calls require gateway API; CLI port unavailable");
    response_out[0] = '\0';
    return -1;
}

/* PoP: cli_tools_mixture_of_agents_tool__run_aggregator_model @ tools/mixture_of_agents_tool.py:_run_aggregator_model */

/* Port of Python tools/mixture_of_agents_tool.py:_run_aggregator_model */
/* Runs the aggregator model to synthesize the final response. */
int cli_tools_mixture_of_agents_tool__run_aggregator_model(
    const char *system_prompt, const char *user_prompt,
    float temperature, int max_tokens,
    char *response_out, size_t response_size)
{
    if (!system_prompt || !user_prompt || !response_out || response_size == 0) {
        return -1;
    }
    (void)temperature;
    (void)max_tokens;
    hermes_log(LOG_WARNING, "moa",
               "MoA: aggregator model call requires gateway API; CLI port unavailable");
    response_out[0] = '\0';
    return -1;
}

/* PoP: cli_tools_mixture_of_agents_tool_mixture_of_agents_tool @ tools/mixture_of_agents_tool.py:mixture_of_agents_tool */

/* Port of Python tools/mixture_of_agents_tool.py:mixture_of_agents_tool */
/* Main MoA tool entry point. Processes a complex query using multiple models. */
int cli_tools_mixture_of_agents_tool_mixture_of_agents_tool(
    const char *user_prompt, const char **reference_models, int ref_count,
    const char *aggregator_model, char *result_out, size_t result_size)
{
    if (!user_prompt || !result_out || result_size == 0) {
        return -1;
    }
    (void)reference_models;
    (void)ref_count;
    (void)aggregator_model;
    hermes_log(LOG_WARNING, "moa",
               "MoA: full MoA processing requires gateway API; CLI port unavailable");
    snprintf(result_out, result_size,
             "{\"success\":false,\"response\":\"MoA processing requires gateway API\"}");
    return -1;
}

/* PoP: cli_tools_mixture_of_agents_tool_check_moa_requirements @ tools/mixture_of_agents_tool.py:check_moa_requirements */

/* Port of Python tools/mixture_of_agents_tool.py:check_moa_requirements */
/* Checks if all requirements for MoA tools are met. */
int cli_tools_mixture_of_agents_tool_check_moa_requirements(void)
{
    const char *api_key = getenv("OPENROUTER_API_KEY");
    if (!api_key || !api_key[0]) {
        return 0;
    }
    return 1;
}

/* PoP: cli_tools_mixture_of_agents_tool_get_moa_configuration @ tools/mixture_of_agents_tool.py:get_moa_configuration */

/* Port of Python tools/mixture_of_agents_tool.py:get_moa_configuration */
/* Gets the current MoA configuration settings. */
int cli_tools_mixture_of_agents_tool_get_moa_configuration(
    char *config_out, size_t config_size)
{
    if (!config_out || config_size == 0) {
        return -1;
    }
    snprintf(config_out, config_size,
             "{\"reference_models\":[\"anthropic/claude-opus-4.6\"],"
             "\"aggregator_model\":\"anthropic/claude-opus-4.6\","
             "\"reference_temperature\":0.6,"
             "\"aggregator_temperature\":0.4,"
             "\"min_successful_references\":1}");
    return 0;
}
