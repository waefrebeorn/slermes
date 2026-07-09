/*
 * port_tools_xai_http.c — C port of tools/xai_http.c
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_xai_http_hermes_xai_user_agent @ tools/xai_http.py:hermes_xai_user_agent */

/* Port of Python tools/xai_http.py:hermes_xai_user_agent */
/* Return a stable Hermes-specific User-Agent for xAI HTTP calls. */
char *cli_tools_xai_http_hermes_xai_user_agent(void)
{
    const char *ver = HERMES_VERSION;
    if (!ver) ver = "unknown";

    /* Allocate result: "Hermes-Agent/<version>" */
    size_t len = strlen("Hermes-Agent/") + strlen(ver) + 1;
    char *result = (char *)malloc(len);
    if (result) {
        snprintf(result, len, "Hermes-Agent/%s", ver);
    }
    return result ? result : strdup("Hermes-Agent/unknown");
}

/* PoP: cli_tools_xai_http_resolve_xai_http_credentials @ tools/xai_http.py:resolve_xai_http_credentials */

/* Port of Python tools/xai_http.py:resolve_xai_http_credentials */
/* Resolve bearer credentials for direct xAI HTTP endpoints.
 * In the C port, we return a JSON string with provider/api_key/base_url.
 * Caller is responsible for freeing the returned string. */
char *cli_tools_xai_http_resolve_xai_http_credentials(int force_refresh)
{
    (void)force_refresh; /* OAuth refresh path is Python-specific */

    /* Try XAI_API_KEY from environment first */
    const char *api_key = getenv("XAI_API_KEY");
    const char *base_url = getenv("XAI_BASE_URL");

    if (!api_key || !api_key[0]) api_key = "";
    if (!base_url || !base_url[0]) base_url = "https://api.x.ai/v1";
    else {
        /* rstrip("/") */
        size_t len = strlen(base_url);
        char *trimmed = (char *)malloc(len + 1);
        if (trimmed) {
            strcpy(trimmed, base_url);
            while (len > 0 && trimmed[len - 1] == '/') {
                trimmed[--len] = '\0';
            }
            base_url = trimmed;
        }
    }

    /* Build result JSON: {"provider":"xai","api_key":"...","base_url":"..."} */
    size_t result_len = 64 + strlen(api_key) + strlen(base_url);
    char *result = (char *)malloc(result_len);
    if (result) {
        snprintf(result, result_len,
                 "{\"provider\":\"xai\",\"api_key\":\"%s\",\"base_url\":\"%s\"}",
                 api_key, base_url);
    }

    if (api_key && api_key[0]) {
        hermes_log(LOG_DEBUG, "port", "xai_http: resolved credentials from XAI_API_KEY env var");
    } else {
        hermes_log(LOG_DEBUG, "port", "xai_http: no credentials found, returning empty key");
    }

    return result ? result : strdup("{\"provider\":\"xai\",\"api_key\":\"\",\"base_url\":\"https://api.x.ai/v1\"}");
}

/* PoP: cli_tools_xai_http__coerce_expires_after @ tools/xai_http.py:_coerce_expires_after */

/* Port of Python tools/xai_http.py:_coerce_expires_after.
 * Normalize an xAI storage TTL: int seconds, or None for permanent.
 * Returns malloc'd string: the decimal seconds, or "null" for permanent. */
char *cli_tools_xai_http__coerce_expires_after(const char *value)
{
    /* MAX_XAI_STORAGE_EXPIRES_AFTER_SECONDS = 30*24*60*60 = 2592000
     * SAFE_XAI_STORAGE_EXPIRES_AFTER_SECONDS = 2*24*60*60 = 172800 */
    const long MAX_EXP = 30L * 24 * 60 * 60;   /* 2592000 */
    const long SAFE_EXP = 2L * 24 * 60 * 60;    /* 172800 */

    /* None -> None (permanent) */
    if (!value) {
        return strdup("null");
    }
    /* str(value) normalization; if value already numeric string, use it. */
    char buf[256];
    /* Convert the input to a normalized lowercase string for the keyword checks. */
    size_t j = 0;
    for (const char *s = value; *s && j + 1 < sizeof(buf); s++) {
        unsigned char c = (unsigned char)*s;
        buf[j++] = (char)tolower(c);
    }
    buf[j] = '\0';
    /* strip surrounding whitespace */
    char *b = buf;
    while (*b == ' ' || *b == '\t') b++;
    size_t L = strlen(b);
    while (L > 0 && (b[L - 1] == ' ' || b[L - 1] == '\t')) b[--L] = '\0';

    if (b[0] == '\0' || strcmp(b, "default") == 0
        || strcmp(b, "none") == 0 || strcmp(b, "null") == 0
        || strcmp(b, "never") == 0 || strcmp(b, "permanent") == 0
        || strcmp(b, "forever") == 0 || strcmp(b, "0") == 0) {
        return strdup("null");
    }

    /* Try int parse */
    char *endp = NULL;
    long seconds = strtol(b, &endp, 10);
    if (endp != b && (*endp == '\0' || *endp == ' ' || *endp == '\t')) {
        if (seconds <= 0) {
            return strdup("null");
        }
        if (seconds > MAX_EXP) seconds = MAX_EXP;
        char out[32];
        snprintf(out, sizeof(out), "%ld", seconds);
        return strdup(out);
    }

    /* Unparseable -> SAFE default */
    char out[32];
    snprintf(out, sizeof(out), "%ld", SAFE_EXP);
    return strdup(out);
}
