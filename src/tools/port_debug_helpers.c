/*
 * port_debug_helpers.c — Port of Python
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Port of Python: log_call */
void log_call(const char * call_name, json_t * call_data) {
    if (!call_name) return;
    /* log_call */
    hermes_log(LOG_DEBUG, "port", "[log_call] called");
    (void)call_name;
    (void)call_data;
    return;
}

