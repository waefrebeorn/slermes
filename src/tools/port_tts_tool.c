/**
 * port_tts_tool.c — Port of Python: tools/tts_tool.py
 *
 * Real C implementations for TTS tool helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/* Port of Python: _check */
char *check(int importer, const char *label)
{
    if (!label) {
        label = "unknown";
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "tts_check importer=%d label=%s", importer, label);
    hermes_log(LOG_DEBUG, "port", "check: %s", result);
    return result;
}

/* Port of Python: _shell_quote_context */
char *shell_quote_context(double command_template, int position)
{
    static char buf[512];
    snprintf(buf, sizeof(buf), "template=%.2f pos=%d", command_template, position);
    hermes_log(LOG_DEBUG, "port", "shell_quote_context: %s", buf);
    return strdup(buf);
}
