/**
 * @file hermes_gateway_platform.h
 * @brief Gateway platform registry API.
 */
#ifndef HERMES_GATEWAY_PLATFORM_H
#define HERMES_GATEWAY_PLATFORM_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P103: Platform interface API
 * ================================================================ */

/* Register a platform implementation. Called at startup per platform type. */
void gw_platform_register(const gw_platform_t *plat);
int gw_platform_get_count(void);

/* Send a message on a registered platform. Returns platform's send result. */
bool gw_platform_send(const char *platform_name, const char *chat_id, const char *text);

/* Send typing indicator on a registered platform (no-op if not supported). */
void gw_platform_send_typing(const char *platform_name, const char *chat_id);

/* Send emoji reaction on a registered platform (returns false if not supported). */
bool gw_platform_send_reaction(const char *platform_name, const char *chat_id,
                                const char *message_id, const char *emoji);

/* Shutdown all registered platforms — calls each platform's shutdown callback. */
void gw_platform_shutdown_all(void);

#endif /* HERMES_GATEWAY_PLATFORM_H */