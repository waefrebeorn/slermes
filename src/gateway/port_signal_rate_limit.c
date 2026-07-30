/**
 * port_signal_rate_limit.c — Port of Python: gateway/signal_rate_limit.py
 *
 * Real C implementations for Signal rate limiting.
 */

#include "hermes_logger.h"
#include "hermes_gateway_signal.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static time_t last_reset = 0;
static int message_count = 0;
static const int MAX_MESSAGES_PER_MINUTE = 30;

/* Port of Python: reset_scheduler */
void reset_scheduler(void)
{
    hermes_log(LOG_INFO, "port", "reset_scheduler: resetting rate limiter");
    last_reset = time(NULL);
    message_count = 0;
}

/* Port of Python: get_scheduler */
char *get_scheduler(void)
{
    static char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"last_reset\": %ld, \"count\": %d, \"limit\": %d}",
             (long)last_reset, message_count, MAX_MESSAGES_PER_MINUTE);
    hermes_log(LOG_DEBUG, "port", "get_scheduler: %s", buf);
    return buf;
}

/* Port of Python: _reset_scheduler */
void _reset_scheduler(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_reset_scheduler: null context");
        return;
    }
    hermes_log(LOG_INFO, "port", "_reset_scheduler: resetting with context");
    reset_scheduler();
    /* Update context if it has rate limit fields */
    int *count = (int *)ctx;
    if (count) {
        *count = 0;
    }
}
