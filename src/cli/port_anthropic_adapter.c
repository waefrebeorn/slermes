/*
 * port_anthropic_adapter.c — Port of Python agent/anthropic_adapter.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Port of Python: _is_stream_unavailable_error */
bool anthropic_is_stream_unavailable_error(const char *error_msg) {
    if (!error_msg) return false;
    char *lower = strdup(error_msg);
    if (!lower) return false;
    for (char *p = lower; *p; p++) *p = tolower(*p);
    bool result = false;
    if (strstr(lower, "stream") && strstr(lower, "not supported")) {
        result = true;
    } else if (strstr(lower, "invokemodelwithresponsestream")) {
        /* Reuse the real Bedrock detector (message-based, same as Python). */
        extern int cli_agent_bedrock_adapter_is_streaming_access_denied_error(const char *error_msg);
        result = cli_agent_bedrock_adapter_is_streaming_access_denied_error(error_msg) != 0;
    }
    free(lower);
    return result;
}

