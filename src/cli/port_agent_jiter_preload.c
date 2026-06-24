/*
 * port_agent_jiter_preload.c — C port of agent/jiter_preload.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_jiter_preload_preload_jiter_native_extension @ agent/jiter_preload.py:preload_jiter_native_extension */

/*
 * preload_jiter_native_extension: Preload jiter native extension early.
 *
 * In C, we simulate the preload state. The jiter library is a Python-specific
 * JSON parser; in the C runtime, JSON parsing uses the built-in libjson instead.
 * This function records that the "preload" was attempted and returns success
 * since the C runtime always has JSON parsing available.
 *
 * Returns: (void*)1 on success, (void*)0 on failure.
 */
void* cli_agent_jiter_preload_preload_jiter_native_extension(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    static int preloaded = 0;
    static int preload_error = 0;

    if (preloaded) {
        hermes_log(LOG_DEBUG, "port",
                   "preload_jiter_native_extension: already preloaded, returning true");
        return (void *)1;
    }

    /* In the C runtime, JSON parsing is always available via libjson.
     * Simulate a successful preload. */
    hermes_log(LOG_DEBUG, "port",
               "preload_jiter_native_extension: C runtime JSON parser available");

    preloaded = 1;
    preload_error = 0;

    return (void *)1;
}
