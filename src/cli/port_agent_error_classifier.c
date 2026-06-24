/*
 * port_agent_error_classifier.c — C port of agent/error_classifier.c
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_agent_error_classifier_is_auth @ agent/error_classifier.py:is_auth */

/* Port of Python agent/error_classifier.py:is_auth */
/* Check if the error reason is an authentication error.
 * Note: The PoP annotation maps to error_is_auth in the header. */
int cli_agent_error_classifier_is_auth(const char *reason)
{
    if (!reason) return 0;
    /* Check for auth-related failover reasons */
    if (strcmp(reason, "auth") == 0 || strcmp(reason, "auth_permanent") == 0) {
        return 1;
    }
    return 0;
}

/* PoP: cli_agent_error_classifier__extract_status_code @ agent/error_classifier.py:_extract_status_code */

/* Port of Python agent/error_classifier.py:_extract_status_code */
/* Walk the error and its cause chain to find an HTTP status code. */
/* In C, we extract from a JSON error representation. */
int cli_agent_error_classifier__extract_status_code(const char *error_json)
{
    if (!error_json) return 0;

    /* Look for "status_code" or "status" in the JSON error */
    const char *key;

    key = strstr(error_json, "\"status_code\"");
    if (key) {
        key += 13; /* skip '"status_code"' */
        while (*key == ' ' || *key == '\t' || *key == ':') key++;
        int code = atoi(key);
        if (code >= 100 && code < 600) return code;
    }

    key = strstr(error_json, "\"status\"");
    if (key) {
        key += 8; /* skip '"status"' */
        while (*key == ' ' || *key == '\t' || *key == ':') key++;
        int code = atoi(key);
        if (code >= 100 && code < 600) return code;
    }

    return 0; /* not found */
}

/* PoP: cli_agent_error_classifier__extract_error_body @ agent/error_classifier.py:_extract_error_body */

/* Port of Python agent/error_classifier.py:_extract_error_body */
/* Extract the structured error body from an SDK exception. */
char *cli_agent_error_classifier__extract_error_body(const char *error_json)
{
    if (!error_json) {
        return strdup("{}");
    }

    /* Try to extract the "body" field from JSON */
    const char *body = strstr(error_json, "\"body\"");
    if (body) {
        body += 6; /* skip '"body"' */
        while (*body == ' ' || *body == '\t' || *body == ':') body++;
        if (*body == '{') {
            /* Find matching closing brace */
            int depth = 0;
            const char *end = body;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') {
                    depth--;
                    if (depth == 0) { end++; break; }
                }
            }
            size_t len = (size_t)(end - body);
            char *result = (char *)malloc(len + 1);
            if (result) {
                memcpy(result, body, len);
                result[len] = '\0';
                return result;
            }
        }
    }

    /* Try response.json() equivalent: look for "error" field */
    const char *err = strstr(error_json, "\"error\"");
    if (err) {
        err += 7; /* skip '"error"' */
        while (*err == ' ' || *err == '\t' || *err == ':') err++;
        if (*err == '{') {
            int depth = 0;
            const char *end = err;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') {
                    depth--;
                    if (depth == 0) { end++; break; }
                }
            }
            size_t len = (size_t)(end - err);
            char *result = (char *)malloc(len + 3);
            if (result) {
                result[0] = '{';
                memcpy(result + 1, err, len);
                result[len + 1] = '}';
                result[len + 2] = '\0';
                return result;
            }
        }
    }

    return strdup("{}");
}

/* PoP: cli_agent_error_classifier__extract_message @ agent/error_classifier.py:_extract_message */

/* Port of Python agent/error_classifier.py:_extract_message */
/* Extract the most informative error message. */
char *cli_agent_error_classifier__extract_message(const char *error_json)
{
    if (!error_json) {
        return strdup("Unknown error");
    }

    /* Try structured body->error->message first */
    char *body = cli_agent_error_classifier__extract_error_body(error_json);
    if (body) {
        const char *err = strstr(body, "\"error\"");
        if (err) {
            err += 7;
            while (*err == ' ' || *err == '\t' || *err == ':') err++;
            if (*err == '{') {
                const char *msg = strstr(err, "\"message\"");
                if (msg) {
                    msg += 9;
                    while (*msg == ' ' || *msg == '\t' || *msg == ':') msg++;
                    if (*msg == '"') {
                        msg++;
                        /* Find closing quote */
                        const char *end = msg;
                        while (*end && *end != '"') end++;
                        size_t len = (size_t)(end - msg);
                        if (len > 500) len = 500;
                        char *result = (char *)malloc(len + 1);
                        if (result) {
                            memcpy(result, msg, len);
                            result[len] = '\0';
                            free(body);
                            return result;
                        }
                    }
                }
            }
        }

        /* Try body->message */
        const char *msg = strstr(body, "\"message\"");
        if (msg) {
            msg += 9;
            while (*msg == ' ' || *msg == '\t' || *msg == ':') msg++;
            if (*msg == '"') {
                msg++;
                const char *end = msg;
                while (*end && *end != '"') end++;
                size_t len = (size_t)(end - msg);
                if (len > 500) len = 500;
                char *result = (char *)malloc(len + 1);
                if (result) {
                    memcpy(result, msg, len);
                    result[len] = '\0';
                    free(body);
                    return result;
                }
            }
        }

        free(body);
    }

    /* Fallback: return first 500 chars of error_json */
    size_t len = strlen(error_json);
    if (len > 500) len = 500;
    char *result = (char *)malloc(len + 1);
    if (result) {
        memcpy(result, error_json, len);
        result[len] = '\0';
        return result;
    }

    return strdup("Unknown error");
}
