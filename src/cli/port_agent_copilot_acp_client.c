/*
 * port_agent_copilot_acp_client.c — C port of agent/copilot_acp_client.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_copilot_acp_client__create_chat_completion @ agent/copilot_acp_client.py:_create_chat_completion */

/* Port of Python agent/copilot_acp_client.py:_create_chat_completion */
/* Format messages as a prompt string for the Copilot ACP subprocess. */
char *cli_agent_copilot_acp_client__create_chat_completion(
    const char **messages, int message_count,
    const char *model, const char *tools_json)
{
    if (!messages || message_count <= 0) {
        return strdup("");
    }

    /* Build prompt text from messages */
    size_t buf_size = 4096;
    for (int i = 0; i < message_count; i++) {
        if (messages[i]) buf_size += strlen(messages[i]) + 32;
    }

    char *prompt = (char *)malloc(buf_size);
    if (!prompt) return NULL;

    int pos = 0;
    for (int i = 0; i < message_count; i++) {
        if (!messages[i]) continue;
        pos += snprintf(prompt + pos, buf_size - pos, "%s\n\n", messages[i]);
    }

    if (model && *model) {
        hermes_log(LOG_DEBUG, "copilot_acp", "create_chat_completion: model=%s", model);
    }
    if (tools_json && *tools_json) {
        hermes_log(LOG_DEBUG, "copilot_acp", "create_chat_completion: tools provided");
    }

    return prompt;
}

/* PoP: cli_agent_copilot_acp_client__run_prompt @ agent/copilot_acp_client.py:_run_prompt */

/* Port of Python agent/copilot_acp_client.py:_run_prompt */
/* Run a prompt through the Copilot ACP subprocess. */
int cli_agent_copilot_acp_client__run_prompt(
    const char *prompt_text, float timeout_seconds,
    char *stdout_out, size_t stdout_size,
    char *stderr_out, size_t stderr_size)
{
    if (!prompt_text || !*prompt_text) return -1;

    /* In a full implementation, this would:
     * 1. Start the ACP subprocess (codex CLI)
     * 2. Send the prompt via stdin
     * 3. Read stdout/stderr with timeout
     * 4. Return the response
     *
     * For now, return a placeholder indicating the subprocess
     * integration is needed.
     */
    hermes_log(LOG_DEBUG, "copilot_acp", "run_prompt: timeout=%.1f", timeout_seconds);

    if (stdout_out && stdout_size > 0) {
        snprintf(stdout_out, stdout_size,
            "{\"error\":\"Copilot ACP subprocess integration not available in C\"}");
    }
    if (stderr_out && stderr_size > 0) {
        stderr_out[0] = '\0';
    }

    return 0;
}

/* PoP: cli_agent_copilot_acp_client__handle_server_message @ agent/copilot_acp_client.py:_handle_server_message */

/* Port of Python agent/copilot_acp_client.py:_handle_server_message */
/* Handle a server message from the Copilot ACP session. */
int cli_agent_copilot_acp_client__handle_server_message(
    const char *method, const char *params_json,
    char *text_out, size_t text_size,
    char *reasoning_out, size_t reasoning_size)
{
    if (!method || !*method) return 0;

    if (strcmp(method, "session/update") == 0) {
        /* Parse session update: extract text/reasoning chunks */
        if (params_json && strstr(params_json, "agent_message_chunk")) {
            /* Extract text content from update */
            if (text_out && text_size > 0) {
                /* Simplified: copy a portion of params as text */
                snprintf(text_out, text_size, "%s", params_json);
            }
        } else if (params_json && strstr(params_json, "agent_thought_chunk")) {
            /* Extract reasoning content from update */
            if (reasoning_out && reasoning_size > 0) {
                snprintf(reasoning_out, reasoning_size, "%s", params_json);
            }
        }
        return 1; /* handled */
    }

    return 0; /* not handled */
}
