/*
 * port_gateway_platforms_qqbot_utils.c — C port of gateway/platforms/qqbot/utils.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* QQBOT_VERSION is defined in the qqbot constants header */
#ifndef QQBOT_VERSION
#define QQBOT_VERSION "1.0.0"
#endif

/* PoP: cli_gateway_platforms_qqbot_utils_build_user_agent @ gateway/platforms/qqbot/utils.py:build_user_agent */

/* Port of Python gateway/platforms/qqbot/utils.py:build_user_agent */
/* Build a descriptive User-Agent string.
 * Format: QQBotAdapter/<qqbot_version> (Python/<py_version>; <os>; Hermes/<hermes_version>)
 * In C, we use the C version string instead of Python version. */
char *cli_gateway_platforms_qqbot_utils_build_user_agent(void)
{
    const char *hermes_version = HERMES_VERSION;
    if (!hermes_version) hermes_version = "dev";

    /* Detect OS — simplified for C */
    const char *os_name = "linux";
#if defined(_WIN32) || defined(_WIN64)
    os_name = "windows";
#elif defined(__APPLE__)
    os_name = "darwin";
#elif defined(__linux__)
    os_name = "linux";
#else
    os_name = "unknown";
#endif

    /* Build: "QQBotAdapter/1.0.0 (C/<hermes_version>; linux; Hermes/0.15.1)" */
    size_t len = 128 + strlen(QQBOT_VERSION) + strlen(os_name) + strlen(hermes_version);
    char *result = (char *)malloc(len);
    if (result) {
        snprintf(result, len,
                 "QQBotAdapter/%s (C/%s; %s; Hermes/%s)",
                 QQBOT_VERSION, hermes_version, os_name, hermes_version);
    }
    return result ? result : strdup("QQBotAdapter/1.0.0 (unknown)");
}

/* PoP: cli_gateway_platforms_qqbot_utils_get_api_headers @ gateway/platforms/qqbot/utils.py:get_api_headers */

/* Port of Python gateway/platforms/qqbot/utils.py:get_api_headers */
/* Return standard HTTP headers for QQBot API requests.
 * Returns a JSON string: {"Content-Type":"application/json","Accept":"application/json","User-Agent":"..."}
 * Caller is responsible for freeing the returned string. */
char *cli_gateway_platforms_qqbot_utils_get_api_headers(void)
{
    char *ua = cli_gateway_platforms_qqbot_utils_build_user_agent();

    size_t len = 128 + (ua ? strlen(ua) : 32);
    char *result = (char *)malloc(len);
    if (result) {
        snprintf(result, len,
                 "{\"Content-Type\":\"application/json\",\"Accept\":\"application/json\",\"User-Agent\":\"%s\"}",
                 ua ? ua : "unknown");
    }
    if (ua) free(ua);
    return result ? result : strdup("{}");
}

/* PoP: cli_gateway_platforms_qqbot_utils_coerce_list @ gateway/platforms/qqbot/utils.py:coerce_list */

/* Port of Python gateway/platforms/qqbot/utils.py:coerce_list */
/* Coerce a comma-separated string into a trimmed string array.
 * Returns a NULL-terminated array of strings. Caller must free each element and the array. */
char **cli_gateway_platforms_qqbot_utils_coerce_list(const char *value, int *count_out)
{
    if (count_out) *count_out = 0;

    if (!value || !value[0]) {
        /* Return empty array */
        char **result = (char **)malloc(sizeof(char *));
        if (result) result[0] = NULL;
        return result;
    }

    /* Count commas to estimate max items */
    int max_items = 1;
    for (const char *p = value; *p; p++) {
        if (*p == ',') max_items++;
    }

    char **result = (char **)malloc((max_items + 1) * sizeof(char *));
    if (!result) return NULL;

    int count = 0;
    const char *start = value;
    const char *p = value;

    while (*p) {
        if (*p == ',') {
            /* Extract item from start..p */
            size_t item_len = (size_t)(p - start);
            /* Trim leading/trailing whitespace */
            while (item_len > 0 && (start[0] == ' ' || start[0] == '\t')) {
                start++;
                item_len--;
            }
            while (item_len > 0 && (start[item_len - 1] == ' ' || start[item_len - 1] == '\t')) {
                item_len--;
            }
            if (item_len > 0) {
                char *item = (char *)malloc(item_len + 1);
                if (item) {
                    memcpy(item, start, item_len);
                    item[item_len] = '\0';
                    result[count++] = item;
                }
            }
            start = p + 1;
        }
        p++;
    }

    /* Last item (no trailing comma) */
    if (start < p) {
        size_t item_len = (size_t)(p - start);
        while (item_len > 0 && (start[0] == ' ' || start[0] == '\t')) {
            start++;
            item_len--;
        }
        while (item_len > 0 && (start[item_len - 1] == ' ' || start[item_len - 1] == '\t')) {
            item_len--;
        }
        if (item_len > 0) {
            char *item = (char *)malloc(item_len + 1);
            if (item) {
                memcpy(item, start, item_len);
                item[item_len] = '\0';
                result[count++] = item;
            }
        }
    }

    result[count] = NULL;
    if (count_out) *count_out = count;
    return result;
}
