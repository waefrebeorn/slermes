/*
 * port_tools_clarify_tool.c — C port of tools/clarify_tool.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CHOICES 4

/* PoP: cli_tools_clarify_tool_clarify_tool @ tools/clarify_tool.py:clarify_tool */

/* Port of Python tools/clarify_tool.py:clarify_tool */
/* Ask the user a question, optionally with multiple-choice options. */
char *cli_tools_clarify_tool_clarify_tool(
    const char *question,
    const char **choices, int choice_count)
{
    if (!question || !*question) {
        return strdup("{\"error\":\"Question text is required.\"}");
    }

    /* Validate and trim choices */
    if (choices && choice_count > MAX_CHOICES) {
        return strdup("{\"error\":\"Too many choices (max 4).\"}");
    }

    /* Build JSON response with question and choices */
    size_t buf_size = 4096;
    char *json = (char *)malloc(buf_size);
    if (!json) return NULL;

    int pos = 0;
    pos += snprintf(json + pos, buf_size - pos,
        "{\"question\":\"%s\"", question);

    if (choices && choice_count > 0) {
        pos += snprintf(json + pos, buf_size - pos, ",\"choices\":[");
        for (int i = 0; i < choice_count; i++) {
            if (i > 0) pos += snprintf(json + pos, buf_size - pos, ",");
            pos += snprintf(json + pos, buf_size - pos, "\"%s\"",
                choices[i] ? choices[i] : "");
        }
        pos += snprintf(json + pos, buf_size - pos, "]");
    }

    pos += snprintf(json + pos, buf_size - pos, "}");

    return json;
}


