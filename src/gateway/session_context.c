#include "hermes_logger.h"
#include "hermes.h"
#include <string.h>
#include <stdlib.h>

/*
 * session_context.c — Port of Python gateway/session_context.py
 *
 * Async delivery support — determines whether the current session's
 * delivery channel can route an async completion back to the agent
 * after the current turn ends.
 *
 * PoP annotations referencing this module: 6
 */

/* Per-session async-delivery capability flag.
 *
 * Mirrors Python's _SESSION_ASYNC_DELIVERY ContextVar.
 * - UNSET sentinel (-1) => treated as supported (CLI fallback / unaware path)
 * - 0 => NOT supported (stateless adapters like API server)
 * - 1 => supported (real gateway platforms with persistent outbound channels)
 *
 * In Python this is contextvar per-task; for our C port we use a
 * simple global that the session init code sets before each turn.
 */
static int _session_async_delivery = -1; /* -1 = UNSENTINEL */

/* Mark the session context machinery as engaged.
 * Once any host binds a session, the process stays engaged for life.
 */
static int _session_context_engaged = 0;

/* Port of Python gateway/session_context.py:session_context_engaged
 *
 * Return true if any session has been bound via set_session_vars
 * in this process. Used by the subprocess-env bridge to choose
 * its leak policy.
 *
 * Returns int* (malloc'd) to match the void* return convention,
 * or returns NULL on allocation failure.
 */
void* cli_gateway_session_context_session_context_engaged(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    (void)p1;
    int *result = (int*)malloc(sizeof(int));
    if (!result) return NULL;
    *result = _session_context_engaged;
    return result;
}

/* Port of Python gateway/session_context.py:async_delivery_supported
 *
 * Whether the current session can deliver a background completion later.
 * Returns false only when the active session was explicitly bound by
 * a stateless adapter (API server) that cannot route a notification
 * back to the agent after the turn ends.
 *
 * Returns int* (malloc'd) where *int is 1 (supported) or 0 (not supported).
 */
void* cli_gateway_session_context_async_delivery_supported(void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;
    (void)p1;
    int *result = (int*)malloc(sizeof(int));
    if (!result) return NULL;
    if (_session_async_delivery == -1) {
        *result = 1; /* UNSET => treated as supported */
    } else {
        *result = _session_async_delivery;
    }
    return result;
}

/* Set the async delivery capability for the current session.
 * Called by session init code before each turn.
 * 0 = not supported (stateless adapters), 1 = supported (gateway/cli/cron).
 */
void gw_session_set_async_delivery(int supported)
{
    _session_async_delivery = supported ? 1 : 0;
    _session_context_engaged = 1;
}

/* Reset async delivery to unset state (used in cleanup / session clear). */
void gw_session_reset_async_delivery(void)
{
    _session_async_delivery = -1;
}