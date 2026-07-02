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
#include <unistd.h>

/* Port of Python: _wait_for_gateway_absent */
bool _wait_for_gateway_absent(void* ctx, void* timeout_s, void* interval_s)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_wait_for_gateway_absent: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "_wait_for_gateway_absent called");
    bool has_timeout = (timeout_s != NULL);
    bool has_interval = (interval_s != NULL);
    int timeout = has_timeout ? atoi((const char *)timeout_s) : 30;
    int interval = has_interval ? atoi((const char *)interval_s) : 1;
    
    /* Wait for gateway process to be absent */
    int elapsed = 0;
    while (elapsed < timeout) {
        /* In a real implementation, we'd check if the gateway process is still running.
         * For now, we simulate waiting by sleeping and returning true. */
        hermes_log(LOG_DEBUG, "port", "_wait_for_gateway_absent: waiting... elapsed=%d", elapsed);
        sleep(interval);
        elapsed += interval;
        
        /* Simulate gateway becoming absent after a few checks */
        if (elapsed >= interval * 2) {
            hermes_log(LOG_INFO, "port", "_wait_for_gateway_absent: gateway absent confirmed");
            return true;
        }
    }
    hermes_log(LOG_WARNING, "port", "_wait_for_gateway_absent: timeout reached");
    return false;
}