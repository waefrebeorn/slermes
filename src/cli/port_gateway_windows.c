/*
 * port_gateway_windows.c — Port of Python
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Port of Python: _wait_for_gateway_absent */
bool _wait_for_gateway_absent(void* ctx, void* timeout_s, void* interval_s)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_wait_for_gateway_absent: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "_wait_for_gateway_absent called");
    if (timeout_s) {
        hermes_log(LOG_DEBUG, "port", "_wait_for_gateway_absent: timeout_s is set");
    }
    if (interval_s) {
        hermes_log(LOG_DEBUG, "port", "_wait_for_gateway_absent: interval_s is set");
    }
    /* TODO: implement _wait_for_gateway_absent logic */
    return false;
}
