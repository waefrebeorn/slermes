/**
 * port_web_tools.c — Port of Python: tools/web_tools.py
 *
 * Real C implementations for web tool helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: _has_env */
bool has_env(const char *name)
{
    if (!name) {
        hermes_log(LOG_WARNING, "port", "has_env: null name");
        return false;
    }
    const char *val = getenv(name);
    bool exists = (val != NULL && val[0] != '\0');
    hermes_log(LOG_DEBUG, "port", "has_env: %s=%s", name, exists ? "true" : "false");
    return exists;
}
