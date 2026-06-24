/**
 * port_msgraph_webhook.c — Port of Python: gateway/msgraph_webhook.py
 *
 * Real C implementations for MS Graph webhook helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>

/* Port of Python: normalize_path */
char *normalize_path(const char *path)
{
    if (!path) {
        hermes_log(LOG_WARNING, "port", "normalize_path: null path");
        return NULL;
    }
    char *normalized = strdup(path);
    if (!normalized) return NULL;

    /* Remove trailing slashes */
    int len = strlen(normalized);
    while (len > 1 && normalized[len - 1] == '/') {
        normalized[--len] = '\0';
    }
    /* Collapse double slashes */
    for (int i = 0; i < len - 1; i++) {
        if (normalized[i] == '/' && normalized[i + 1] == '/') {
            memmove(&normalized[i], &normalized[i + 1], len - i);
            len--;
            i--;
        }
    }
    hermes_log(LOG_DEBUG, "port", "normalize_path: '%s' -> '%s'", path, normalized);
    return normalized;
}
