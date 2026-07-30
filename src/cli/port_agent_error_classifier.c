/*
 * port_agent_error_classifier.c — C port of agent/error_classifier.c
 */

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

/* PoP: cli_agent_error_classifier__is_openrouter_upstream_error @ agent/error_classifier.py:_is_openrouter_upstream_error */

/* Port of Python agent/error_classifier.py:_is_openrouter_upstream_error */
/* Detect OpenRouter's aggregator-wrapped upstream provider errors.
 * body is a JSON object string; provider is the configured provider slug. */
int cli_agent_error_classifier__is_openrouter_upstream_error(
    const char *body_json, const char *provider)
{
    if (!body_json) return 0;

    /* Cheap prefix check: body must contain an "error" object. */
    const char *err = strstr(body_json, "\"error\"");
    if (!err) return 0;
    /* The "error" we want must be an object (followed by '{'). A bare
     * "error_code"/"error_message" key would be a false positive, but the
     * Python check requires err to be a dict, so we require a '{'. */
    const char *p = err + 7; /* skip '"error"' */
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    if (*p != '{') return 0;

    /* Walk to the error object's "message" field. */
    const char *msg = strstr(p, "\"message\"");
    if (!msg) return 0;
    msg += 9; /* skip '"message"' */
    while (*msg == ' ' || *msg == '\t' || *msg == ':') msg++;
    if (*msg != '"') return 0;
    msg++; /* skip opening quote */

    /* Extract the outer message, lower-cased for comparison. */
    char buf[512];
    size_t i = 0;
    while (*msg && *msg != '"' && i + 1 < sizeof(buf)) {
        buf[i++] = (char)tolower((unsigned char)*msg);
        msg++;
    }
    buf[i] = '\0';
    if (strcmp(buf, "provider returned error") != 0) {
        return 0;
    }

    /* Require either the explicit OpenRouter provider OR the metadata shape. */
    const char *provider_lower = provider ? provider : "";
    /* lower-case the provider slug */
    char pl[128];
    size_t j = 0;
    for (; provider_lower[j] && j + 1 < sizeof(pl); j++) {
        pl[j] = (char)tolower((unsigned char)provider_lower[j]);
    }
    pl[j] = '\0';
    if (strcmp(pl, "openrouter") == 0) {
        return 1;
    }

    /* Look for metadata with "raw" or "provider_name". */
    const char *metadata = strstr(p, "\"metadata\"");
    if (metadata) {
        metadata += 10; /* skip '"metadata"' */
        while (*metadata == ' ' || *metadata == '\t' || *metadata == ':') metadata++;
        if (*metadata == '{') {
            const char *end = metadata;
            int depth = 0;
            for (; *end; end++) {
                if (*end == '{') depth++;
                else if (*end == '}') {
                    depth--;
                    if (depth == 0) { end++; break; }
                }
            }
            size_t meta_len = (size_t)(end - metadata);
            /* Check for "raw" or "provider_name" inside the metadata object. */
            if (meta_len > 0) {
                char *meta = (char *)malloc(meta_len + 1);
                if (meta) {
                    memcpy(meta, metadata, meta_len);
                    meta[meta_len] = '\0';
                    int found = (strstr(meta, "\"raw\"") != NULL)
                                || (strstr(meta, "\"provider_name\"") != NULL);
                    free(meta);
                    if (found) return 1;
                }
            }
        }
    }
    return 0;
}

/* PoP: cli_agent_error_classifier__extract_upstream_provider_name @ agent/error_classifier.py:_extract_upstream_provider_name */

/* Port of Python agent/error_classifier.py:_extract_upstream_provider_name */
/* Pull the upstream provider name out of OpenRouter's error metadata.
 * Returns a malloc'd string (caller frees) or NULL. */
char *cli_agent_error_classifier__extract_upstream_provider_name(
    const char *body_json)
{
    if (!body_json) return NULL;
    const char *err = strstr(body_json, "\"error\"");
    if (!err) return NULL;
    err += 7;
    while (*err == ' ' || *err == '\t' || *err == ':') err++;
    if (*err != '{') return NULL;

    /* Find metadata object. */
    const char *metadata = strstr(err, "\"metadata\"");
    if (!metadata) return NULL;
    metadata += 10;
    while (*metadata == ' ' || *metadata == '\t' || *metadata == ':') metadata++;
    if (*metadata != '{') return NULL;

    /* Find provider_name inside metadata. */
    const char *pn = strstr(metadata, "\"provider_name\"");
    if (!pn) return NULL;
    pn += 15;
    while (*pn == ' ' || *pn == '\t' || *pn == ':') pn++;
    if (*pn != '"') return NULL;
    pn++; /* skip opening quote */
    char buf[256];
    size_t i = 0;
    while (*pn && *pn != '"' && i + 1 < sizeof(buf)) {
        buf[i++] = *pn;
        pn++;
    }
    buf[i] = '\0';
    if (buf[0] == '\0') return NULL;
    return strdup(buf);
}
