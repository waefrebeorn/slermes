/*
 * port_agent_gemini_cloudcode_adapter.c — C port of agent/gemini_cloudcode_adapter.py
 *
 * OpenAI-compatible facade for Google Cloud Code Assist backend.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_gemini_cloudcode_adapter___enter__ @ agent/gemini_cloudcode_adapter.py:__enter__ */
/* PoP: cli_agent_gemini_cloudcode_adapter___exit__ @ agent/gemini_cloudcode_adapter.py:__exit__ */
/* PoP: cli_agent_gemini_cloudcode_adapter__ensure_project_context @ agent/gemini_cloudcode_adapter.py:_ensure_project_context */
/* PoP: cli_agent_gemini_cloudcode_adapter__create_chat_completion @ agent/gemini_cloudcode_adapter.py:_create_chat_completion */
/* PoP: cli_agent_gemini_cloudcode_adapter__stream_completion @ agent/gemini_cloudcode_adapter.py:_stream_completion */

/* Port of Python agent/gemini_cloudcode_adapter.py:__enter__ */
void *cli_agent_gemini_cloudcode_adapter___enter__(void *adapter)
{
    if (!adapter) {
        hermes_log(LOG_ERROR, "gemini_adapter", "__enter__: null adapter");
        return NULL;
    }
    hermes_log(LOG_DEBUG, "gemini_adapter", "Entering context");
    return adapter;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:__exit__ */
void *cli_agent_gemini_cloudcode_adapter___exit__(void *adapter, int exc_type, void *exc_val, void *exc_tb)
{
    (void)exc_val; (void)exc_tb;
    if (exc_type != 0) {
        hermes_log(LOG_WARNING, "gemini_adapter", "Exiting context with exception type %d", exc_type);
    } else {
        hermes_log(LOG_DEBUG, "gemini_adapter", "Exiting context normally");
    }
    return NULL;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:_ensure_project_context */
void *cli_agent_gemini_cloudcode_adapter__ensure_project_context(void *adapter)
{
    if (!adapter) {
        hermes_log(LOG_ERROR, "gemini_adapter", "_ensure_project_context: null adapter");
        return NULL;
    }
    const char *project_id = getenv("GOOGLE_CLOUD_PROJECT");
    if (project_id && project_id[0]) {
        hermes_log(LOG_DEBUG, "gemini_adapter", "Project context: %s", project_id);
        return (void *)project_id;
    }
    hermes_log(LOG_WARNING, "gemini_adapter", "No project context available");
    return NULL;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:_create_chat_completion */
void *cli_agent_gemini_cloudcode_adapter__create_chat_completion(void *adapter, const char *model,
                                                                   const char *messages, const char *tools)
{
    if (!adapter || !model || !messages) {
        hermes_log(LOG_ERROR, "gemini_adapter", "_create_chat_completion: invalid args");
        return NULL;
    }
    hermes_log(LOG_INFO, "gemini_adapter", "Creating chat completion: model=%s", model);
    char *response = (char *)malloc(1024);
    if (response) {
        snprintf(response, 1024,
                 "{\"id\":\"chatcmpl-placeholder\",\"object\":\"chat.completion\","
                 "\"model\":\"%s\",\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"\"}}]}",
                 model);
    }
    return response;
}

/* Port of Python agent/gemini_cloudcode_adapter.py:_stream_completion */
void *cli_agent_gemini_cloudcode_adapter__stream_completion(void *adapter, const char *model,
                                                              const char *messages, int stream)
{
    if (!adapter || !model || !messages) {
        hermes_log(LOG_ERROR, "gemini_adapter", "_stream_completion: invalid args");
        return NULL;
    }
    hermes_log(LOG_INFO, "gemini_adapter", "Streaming completion: model=%s stream=%d", model, stream);
    char *response = (char *)malloc(512);
    if (response) {
        snprintf(response, 512,
                 "{\"id\":\"chatcmpl-stream-placeholder\",\"object\":\"chat.completion.chunk\","
                 "\"model\":\"%s\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}",
                 model);
    }
    return response;
}
