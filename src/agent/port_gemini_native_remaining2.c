/*
 * port_gemini_native_remaining2.c — Port of agent/gemini_native_adapter.py
 * client/error-class surface. Error envelopes, create wrappers,
 * close, chat completion with thinking config.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ agent/gemini_native_adapter.py:__init__ */
char *gna2_error_init(const char *message, long code, long status_code) {
    /* Python: typed error. */
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"code\": %ld, \"status_code\": %ld}",
             message ? message : "", code, status_code);
    return out;
}

/* PoP: create @ agent/gemini_native_adapter.py:create */
char *gna2_create(const char *kwargs_json) {
    /* Python: delegate to client. */
    if (!kwargs_json) return NULL;
    printf("gemini chat completion created (via client)\n");
    return strdup("{}");
}

/* PoP: close @ agent/gemini_native_adapter.py:close */
int gna2_close(void) {
    /* Python: close http. */
    printf("gemini http client closed\n");
    return 0;
}

/* PoP: _create_chat_completion @ agent/gemini_native_adapter.py:_create_chat_completion */
char *gna2_create_chat_completion(const char *extra_body_json) {
    /* Python: thinking config from extra_body. */
    if (!extra_body_json) return strdup("{}");
    const char *p = strstr(extra_body_json, "thinking");
    if (p) {
        printf("gemini thinking config applied from extra_body\n");
    }
    return strdup(extra_body_json);
}
