/**
 * port_clarify_tool.c — Port of Python: tools/clarify_tool.py
 *
 * Real C implementations for clarify tool helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: _flatten_choice */
char *flatten_choice(const char *c)
{
    if (!c) {
        hermes_log(LOG_WARNING, "port", "flatten_choice: null choice");
        return strdup("(none)");
    }
    /* Strip extra whitespace and normalize */
    while (*c == ' ') c++;
    int len = strlen(c);
    while (len > 0 && c[len-1] == ' ') len--;
    char *result = malloc(len + 1);
    if (!result) return NULL;
    strncpy(result, c, len);
    result[len] = '\0';
    hermes_log(LOG_DEBUG, "port", "flatten_choice: '%s'", result);
    return result;
}
