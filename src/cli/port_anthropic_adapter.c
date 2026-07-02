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
        /* is_streaming_access_denied_error check would go here */
        result = true; /* Simplified: treat as stream unavailable */
    }
    free(lower);
    return result;
}

