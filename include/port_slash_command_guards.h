/*
 * port_slash_command_guards.h — Faithful C11 ports of the pure-logic
 * cross-origin / resume security guards from Python
 * gateway/slash_commands.py (GatewaySlashCommandsMixin).
 *
 * These are the participant-scoping IDOR guards that decide whether a slash
 * command caller may resume / enumerate a target session. They operate purely
 * on SessionSource fields plus two config flags, so they port faithfully as
 * pure functions with no runtime coupling.
 */

#ifndef PORT_SLASH_COMMAND_GUARDS_H
#define PORT_SLASH_COMMAND_GUARDS_H

#include <stdbool.h>
#include "hermes_gateway_types.h"

/* _typed_command_prefix_for(platform) -> str
 * Return the prefix users can always type to reach commands. Reads the
 * adapter's typed_command_prefix capability (default "/"). Pass the adapter's
 * configured prefix (or NULL when no adapter is resolved). */
const char *slash_typed_command_prefix_for(const char *adapter_prefix);

/* _same_matrix_room(current, origin) -> bool
 * True when both sides are Matrix, same chat_id, same thread_id. */
bool slash_same_matrix_room(const gw_session_source_t *current,
                            const gw_session_source_t *origin);

/* _same_origin_chat(current, origin, group_per_user, thread_per_user) -> bool
 * Platform-agnostic counterpart to _same_matrix_room. Requires same platform,
 * chat, thread; then participant-scopes per the session-key contract. */
bool slash_same_origin_chat(const gw_session_source_t *current,
                            const gw_session_source_t *origin,
                            bool group_sessions_per_user,
                            bool thread_sessions_per_user);

#endif /* PORT_SLASH_COMMAND_GUARDS_H */
