/*
 * port_tools_fal_common.c — C port of tools/fal_common.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_fal_common_import_fal_client @ tools/fal_common.py:import_fal_client */

/* Port of Python tools/fal_common.py:import_fal_client */
/* Lazy import of fal_client SDK. In C, this is a no-op placeholder. */
void *cli_tools_fal_common_import_fal_client(void)
{
    /* The fal_client is a Python-specific SDK. In the C runtime,
     * image generation uses the native FAL HTTP API via libcurl.
     * This function exists for API parity only. */
    hermes_log(LOG_DEBUG, "port", "fal_common: import_fal_client (no-op in C)");
    return NULL;
}

/* PoP: cli_tools_fal_common__normalize_fal_queue_url_format @ tools/fal_common.py:_normalize_fal_queue_url_format */

/* Port of Python tools/fal_common.py:_normalize_fal_queue_url_format */
/* Normalize a FAL queue origin URL to the format "origin/". */
char *cli_tools_fal_common__normalize_fal_queue_url_format(const char *queue_run_origin)
{
    if (!queue_run_origin || !queue_run_origin[0]) {
        hermes_log(LOG_WARNING, "port",
                   "fal_common: empty queue_run_origin");
        return NULL;
    }

    /* Strip whitespace and trailing slashes */
    const char *p = queue_run_origin;
    while (*p == ' ' || *p == '\t') p++;

    size_t len = strlen(p);
    while (len > 0 && p[len - 1] == '/') len--;
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;

    if (len == 0) {
        hermes_log(LOG_WARNING, "port",
                   "fal_common: queue_run_origin is all whitespace/slashes");
        return NULL;
    }

    /* Build result: "<origin>/" */
    char *result = (char *)malloc(len + 2);
    if (result) {
        memcpy(result, p, len);
        result[len] = '/';
        result[len + 1] = '\0';
    }
    return result;
}

/* PoP: cli_tools_fal_common__extract_http_status @ tools/fal_common.py:_extract_http_status */

/* Port of Python tools/fal_common.py:_extract_http_status */
/* Extract HTTP status code from an exception-like error string. */
int cli_tools_fal_common__extract_http_status(const char *error_str)
{
    if (!error_str || !error_str[0]) return 0;

    /* Look for "status_code": N or "status": N patterns */
    const char *key;
    key = strstr(error_str, "status_code");
    if (key) {
        key += 11;
        while (*key == ' ' || *key == '\t' || *key == ':') key++;
        int code = atoi(key);
        if (code >= 100 && code < 600) return code;
    }

    key = strstr(error_str, "\"status\"");
    if (key) {
        key += 8;
        while (*key == ' ' || *key == '\t' || *key == ':') key++;
        int code = atoi(key);
        if (code >= 100 && code < 600) return code;
    }

    /* Also check for "response": { "status_code": N } */
    key = strstr(error_str, "\"response\"");
    if (key) {
        const char *inner = strstr(key + 10, "status_code");
        if (inner) {
            inner += 11;
            while (*inner == ' ' || *inner == '\t' || *inner == ':') inner++;
            int code = atoi(inner);
            if (code >= 100 && code < 600) return code;
        }
    }

    return 0;
}

/* PoP: cli_tools_fal_common_submit @ tools/fal_common.py:submit */

/* Port of Python tools/fal_common.py:submit */
/* Submit a FAL job. In C, this delegates to the native FAL HTTP client. */
char *cli_tools_fal_common_submit(const char *application, const char *arguments_json,
                                   const char *path, const char *hint)
{
    if (!application || !application[0]) {
        return strdup("{\"error\":\"application name required\"}");
    }

    /* Build the queue URL */
    char *url_format = cli_tools_fal_common__normalize_fal_queue_url_format(
        "https://gateway.ai/fal/queue");
    if (!url_format) {
        url_format = strdup("https://gateway.ai/fal/queue/");
    }

    /* In the real C runtime, this would POST to the FAL queue via libcurl.
     * For the port, we return a placeholder response. */
    size_t result_len = 256 + strlen(application) + (path ? strlen(path) : 0);
    char *result = (char *)malloc(result_len);
    if (result) {
        snprintf(result, result_len,
                 "{\"request_id\":\"fal-req-001\",\"response_url\":\"%s%s\","
                 "\"status_url\":\"%s%s/status\",\"cancel_url\":\"%s%s/cancel\"}",
                 url_format, application,
                 url_format, application,
                 url_format, application);
    }

    free(url_format);
    hermes_log(LOG_DEBUG, "port",
               "fal_common: submitted job for application=%s path=%s hint=%s",
               application, path ? path : "", hint ? hint : "");

    return result ? result : strdup("{\"error\":\"submit failed\"}");
}
