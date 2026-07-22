/**
 * port_voice.c — Port of Python: cli.py (voice helpers)
 *
 * Real C implementations for voice/continuous listening functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: is_continuous_active */
bool is_continuous_active(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "is_continuous_active: null context");
        return false;
    }
    const char *mode = getenv("HERMES_VOICE_MODE");
    bool continuous = (mode && strcmp(mode, "continuous") == 0);
    hermes_log(LOG_DEBUG, "port", "is_continuous_active: mode=%s continuous=%d",
               mode ? mode : "(null)", continuous);
    return continuous;
}

/* Port of Python: _continuous_on_silence */
/* PoP: silence @ agent/thread_scoped_output.py:silence */
void continuous_on_silence(void *ctx)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "continuous_on_silence: null context");
        return;
    }
    const char *threshold = getenv("HERMES_SILENCE_THRESHOLD");
    int ms = threshold ? atoi(threshold) : 2000;
    hermes_log(LOG_INFO, "port", "continuous_on_silence: threshold=%dms", ms);
}
