/*
 * port_gateway_slash_access.c — C port of gateway/slash_access.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_slash_access_policy_from_extra @ gateway/slash_access.py:policy_from_extra */

/* Port of Python gateway/slash_access.py:policy_from_extra */
/* Build a policy from a platform's extra dict for one scope. */
/* DM scope falls back to group scope keys for user_allowed_commands. */
int cli_gateway_slash_access_policy_from_extra(
    const char *admin_key, const char *cmd_key,
    const char **extra_keys, const char **extra_values, int extra_count,
    const char *scope,
    char **admin_ids_out, int *admin_count_out,
    char **allowed_cmds_out, int *cmd_count_out)
{
    if (!admin_key || !cmd_key || !scope) return -1;

    *admin_count_out = 0;
    *cmd_count_out = 0;

    /* Look up admin IDs from extra dict */
    for (int i = 0; i < extra_count; i++) {
        if (extra_keys[i] && strcmp(extra_keys[i], admin_key) == 0 && extra_values[i]) {
            admin_ids_out[(*admin_count_out)++] = strdup(extra_values[i]);
            break;
        }
    }

    /* Look up allowed commands from extra dict */
    for (int i = 0; i < extra_count; i++) {
        if (extra_keys[i] && strcmp(extra_keys[i], cmd_key) == 0 && extra_values[i]) {
            allowed_cmds_out[(*cmd_count_out)++] = strdup(extra_values[i]);
            break;
        }
    }

    /* DM scope fallback: if no commands found, try group_user_allowed_commands */
    if (*cmd_count_out == 0 && strcmp(scope, "dm") == 0) {
        const char *fallback_key = "group_user_allowed_commands";
        for (int i = 0; i < extra_count; i++) {
            if (extra_keys[i] && strcmp(extra_keys[i], fallback_key) == 0 && extra_values[i]) {
                allowed_cmds_out[(*cmd_count_out)++] = strdup(extra_values[i]);
                break;
            }
        }
    }

    return 0; /* success: enabled if admin_count > 0 */
}

/* PoP: cli_gateway_slash_access_policy_for_source @ gateway/slash_access.py:policy_for_source */

/* Port of Python gateway/slash_access.py:policy_for_source */
/* Resolve the access policy for a SessionSource. */
/* Returns a disabled policy when gateway_config or source is NULL. */
int cli_gateway_slash_access_policy_for_source(
    const char *platform_name, const char *chat_type,
    /* gateway_config: platform extra dict */
    const char **extra_keys, const char **extra_values, int extra_count,
    char **admin_ids_out, int *admin_count_out,
    char **allowed_cmds_out, int *cmd_count_out)
{
    *admin_count_out = 0;
    *cmd_count_out = 0;

    if (!platform_name || !chat_type) {
        /* Return disabled policy */
        return 0;
    }

    /* Determine scope from chat_type */
    const char *scope = "group";
    if (strcmp(chat_type, "dm") == 0 || strcmp(chat_type, "direct") == 0) {
        scope = "dm";
    }

    /* Build scope-specific keys */
    char admin_key[128], cmd_key[128];
    snprintf(admin_key, sizeof(admin_key), "%s_admin_user_ids", scope);
    snprintf(cmd_key, sizeof(cmd_key), "%s_user_allowed_commands", scope);

    return cli_gateway_slash_access_policy_from_extra(
        admin_key, cmd_key, extra_keys, extra_values, extra_count, scope,
        admin_ids_out, admin_count_out, allowed_cmds_out, cmd_count_out);
}
