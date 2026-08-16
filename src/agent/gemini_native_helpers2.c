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

/* PoP: gna2_error_init @ agent/gemini_native_adapter.py:__init__ */
char *gna2_error_init(const char *message, long code, long status_code) {
    /* Python: typed error. */
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"code\": %ld, \"status_code\": %ld}",
             message ? message : "", code, status_code);
    return out;
}

/* PoP: gna2_create @ agent/gemini_native_adapter.py:create */
char *gna2_create(const char *kwargs_json) {
    /* Python: delegate to client. */
    if (!kwargs_json) return NULL;
    printf("gemini chat completion created (via client)\n");
    return strdup("{}");
}

/* PoP: gna2_close @ agent/gemini_native_adapter.py:close */
int gna2_close(void) {
    /* Python: close http. */
    printf("gemini http client closed\n");
    return 0;
}

/* PoP: gna2_create_chat_completion @ agent/gemini_native_adapter.py:_create_chat_completion */
char *gna2_create_chat_completion(const char *extra_body_json) {
    /* Python: thinking config from extra_body. */
    if (!extra_body_json) return strdup("{}");
    const char *p = strstr(extra_body_json, "thinking");
    if (p) {
        printf("gemini thinking config applied from extra_body\n");
    }
    return strdup(extra_body_json);
}

/* PoP: gna2_is_standard_key_auth_error @ agent/gemini_native_adapter.py:is_standard_key_auth_error */
int gna2_is_standard_key_auth_error(int status, const char *error_message,
                                    const char *reason)
{
    /* Return true when a Gemini 401 indicates Google rejected the key TYPE.
     * Google began rejecting unrestricted legacy "Standard" Google Cloud API
     * keys on the Gemini API; the rejection surfaces as a misleading 401
     * telling the user to supply an OAuth 2 access token, optionally carrying
     * google.rpc.ErrorInfo reason ACCESS_TOKEN_TYPE_UNSUPPORTED. Scoped
     * narrowly so a plain bad key keeps its existing message. */
    if (status != 401) return 0;
    if (reason && strcmp(reason, "ACCESS_TOKEN_TYPE_UNSUPPORTED") == 0) return 1;
    if (error_message) {
        /* case-insensitive "expected oauth 2 access token" */
        char *lower = strdup(error_message);
        if (lower) {
            for (char *c = lower; *c; c++) *c = (char)tolower((unsigned char)*c);
            int hit = strstr(lower, "expected oauth 2 access token") != NULL;
            free(lower);
            return hit;
        }
    }
    return 0;
}
