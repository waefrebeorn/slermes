/**
 * port_whatsapp_cloud.c — Port of Python: gateway/whatsapp_cloud.py
 *
 * Real C implementations for WhatsApp Cloud API helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    hermes_log(LOG_DEBUG, "port", "normalize_path: '%s' -> '%s'", path, normalized);
    return normalized;
}
