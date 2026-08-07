/**
 * @file hermes_gateway_session_context.h
 * @brief Session context API (port of Python gateway/session_context.py).
 */
#ifndef HERMES_GATEWAY_SESSION_CONTEXT_H
#define HERMES_GATEWAY_SESSION_CONTEXT_H

/* ================================================================
 *  Session Context — Port of Python gateway/session_context.py
 * ================================================================ */

/* Set the async delivery capability for the current session.
 * Called by session init code before each turn.
 * 0 = not supported (stateless adapters), 1 = supported (gateway/cli/cron). */
void gw_session_set_async_delivery(int supported);

/* Reset async delivery to unset state (used in cleanup / session clear). */
void gw_session_reset_async_delivery(void);

/* True if any session has been bound via set_session_vars in this process. */
int gw_session_context_engaged(void);

/* Whether the current session can deliver a background completion later.
 * Returns 1 unless the active session was bound by a stateless adapter. */
int gw_session_async_delivery_supported(void);

/* Whether this turn is delivered over a human messaging channel.
 * Resolves HERMES_PLATFORM env, then HERMES_SESSION_PLATFORM env/context,
 * reporting True when any names a surface outside the non-messaging set. */
bool gw_session_is_messaging_surface(void);

#endif /* HERMES_GATEWAY_SESSION_CONTEXT_H */