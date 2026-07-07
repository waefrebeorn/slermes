/*
 * port_tools_fal_common.c — C port of tools/fal_common.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "libhttp/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_fal_common_import_fal_client @ tools/fal_common.py:import_fal_client */

/* Port of Python tools/fal_common.py:import_fal_client */
/* Lazy import of the fal_client SDK. In C there is no SDK to import — the
 * native FAL HTTP client (see cli_tools_fal_common_submit) replaces it. This
 * exists for API parity only and correctly does nothing. */
void *cli_tools_fal_common_import_fal_client(void)
{
    hermes_log(LOG_DEBUG, "port", "fal_common: import_fal_client (no Python SDK in C runtime)");
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

/* PoP: cli_tools_fal_common_submit @ tools/fal_common.py:submit */
/* Submit a FAL job: POST the arguments JSON to the managed FAL queue URL and
 * return the real response JSON (request_id / response_url / status_url /
 * cancel_url) exactly as the Python client does. */
char *cli_tools_fal_common_submit(const char *application, const char *arguments_json,
                                   const char *path, const char *hint)
{
    if (!application || !application[0]) {
        return strdup("{\"error\":\"application name required\"}");
    }
    if (!arguments_json || !arguments_json[0]) {
        return strdup("{\"error\":\"arguments json required\"}");
    }

    /* Build the queue URL: <origin>/<application>[/<path>] */
    char *url_format = cli_tools_fal_common__normalize_fal_queue_url_format(
        "https://gateway.ai/fal/queue");
    if (!url_format) {
        url_format = strdup("https://gateway.ai/fal/queue/");
    }

    size_t url_len = strlen(url_format) + strlen(application) + (path ? strlen(path) + 1 : 0) + 16;
    char *url = (char *)malloc(url_len);
    if (!url) { free(url_format); return strdup("{\"error\":\"submit failed\"}"); }
    snprintf(url, url_len, "%s%s", url_format, application);
    if (path && path[0]) {
        size_t l = strlen(url);
        snprintf(url + l, url_len - l, "/%s", path);
    }
    free(url_format);

    hermes_log(LOG_DEBUG, "port",
               "fal_common: POST job application=%s path=%s hint=%s",
               application, path ? path : "", hint ? hint : "");

    http_t *http = http_new(120);
    if (!http) { free(url); return strdup("{\"error\":\"http init failed\"}"); }

    /* http_post_json sets the JSON content-type and POSTs the body. */
    http_resp_t *resp = http_post_json(http, url, arguments_json);
    char *result = NULL;
    if (!resp) {
        result = strdup("{\"error\":\"submit request failed\"}");
    } else if (resp->status < 200 || resp->status >= 300) {
        size_t el = 64 + (resp->body ? strlen(resp->body) : 0);
        result = (char *)malloc(el);
        snprintf(result, el, "{\"error\":\"fal submit http %d\",\"detail\":%s}",
                 resp->status, resp->body ? resp->body : "null");
    } else {
        /* Return the server's JSON response verbatim (request_id, urls, etc.). */
        result = strdup(resp->body ? resp->body : "{\"error\":\"empty response\"}");
    }
    http_resp_free(resp);
    http_free(http);
    free(url);
    return result ? result : strdup("{\"error\":\"submit failed\"}");
}
