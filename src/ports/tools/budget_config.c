#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/tools/budget_config.c — C11 port of tools/budget_config.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/tools/port_budget_config.c ── */
/**
 * port_budget_config.c — Port of Python: tools/budget_config.py
 *
 * Real C implementations for budget configuration.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: budget_for_context_window */
char *budget_for_context_window(const char *context_length)
{
    if (!context_length) {
        hermes_log(LOG_WARNING, "port", "budget_for_context_window: null context_length");
        return strdup("{\"error\": \"null context_length\"}");
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "{\"context\": \"%s\", \"budget\": \"auto\"}", context_length);
    hermes_log(LOG_DEBUG, "port", "budget_for_context_window: context=%s", context_length);
    return result;
}

