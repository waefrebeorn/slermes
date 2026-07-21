/**
 * @file hermes_gateway_slash_access.h
 * @brief Slash command access policy API (port of Python gateway/slash_access.py).
 */
#ifndef HERMES_GATEWAY_SLASH_ACCESS_H
#define HERMES_GATEWAY_SLASH_ACCESS_H

#include "hermes_gateway_types.h"
#include "hermes_json.h"

/* ================================================================
 *  Slash Access Policy
 * ================================================================ */

/* Check if user is admin in the policy context.
 * True when gating is disabled (everyone is admin). */
bool slash_policy_is_admin(const slash_policy_t *policy, const char *user_id);

/* Check if user can run a specific slash command.
 * True when gating disabled, user is admin, or command is always-allowed. */
bool slash_policy_can_run(const slash_policy_t *policy,
                           const char *user_id,
                           const char *canonical_cmd);

/* Build a policy from extra dict for one scope.
 * DM scope falls back to group's user_allowed_commands when DM didn't specify.
 * Caller must free() the returned policy. */
slash_policy_t *slash_policy_from_extra(json_node_t *extra, const char *scope);

/* Resolve access policy for a SessionSource.
 * Returns "disabled" policy (gating off) when config/platform is missing.
 * Caller must free() the returned policy. */
slash_policy_t *slash_policy_for_source(json_node_t *gateway_config,
                                         const gw_session_source_t *source);

#endif /* HERMES_GATEWAY_SLASH_ACCESS_H */