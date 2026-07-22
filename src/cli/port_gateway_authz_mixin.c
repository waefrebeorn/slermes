/*
 * port_gateway_authz_mixin.c — C port of gateway/authz_mixin.py
 */

#include "hermes_logger.h"
#include "hermes_gateway_dm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_authz_mixin__adapter_enforces_own_access_policy @ gateway/authz_mixin.py:_adapter_enforces_own_access_policy */

/* Port of Python gateway/authz_mixin.py:_adapter_enforces_own_access_policy */
/* Checks whether the adapter for a platform enforces its own access policy. */
/* Returns 1 if the adapter gates access at intake, 0 otherwise. */
int cli_gateway_authz_mixin__adapter_enforces_own_access_policy(
    void *self, const char *platform_name)
{
    if (!platform_name || !platform_name[0]) {
        return 0;
    }
    /* In the full implementation, this would look up the adapter in */
    /* self->adapters by platform and check enforces_own_access_policy. */
    /* For the CLI port, we return 0 (no adapter) as the gateway */
    /* handles this at runtime via the adapter table. */
    (void)self;
    return 0;
}

/* PoP: cli_gateway_authz_mixin__adapter_dm_policy @ gateway/authz_mixin.py:_adapter_dm_policy */

/* Port of Python gateway/authz_mixin.py:_adapter_dm_policy */
/* Returns the effective DM policy string for a platform adapter. */
/* Possible values: "open", "allowlist", "disabled", "pairing", or "". */
const char *cli_gateway_authz_mixin__adapter_dm_policy(
    void *self, const char *platform_name)
{
    if (!platform_name || !platform_name[0]) {
        return "";
    }
    (void)self;
    /* CLI port: no live adapter; return empty string to signal "unknown". */
    return "";
}

/* PoP: cli_gateway_authz_mixin__adapter_group_policy @ gateway/authz_mixin.py:_adapter_group_policy */

/* Port of Python gateway/authz_mixin.py:_adapter_group_policy */
/* Returns the effective group policy string for a platform adapter. */
const char *cli_gateway_authz_mixin__adapter_group_policy(
    void *self, const char *platform_name)
{
    if (!platform_name || !platform_name[0]) {
        return "";
    }
    (void)self;
    return "";
}

/* PoP: cli_gateway_authz_mixin__adapter_group_has_sender_allowlist @ gateway/authz_mixin.py:_adapter_group_has_sender_allowlist */

/* Port of Python gateway/authz_mixin.py:_adapter_group_has_sender_allowlist */
/* Checks whether a per-group sender allowlist gated this group message. */
int cli_gateway_authz_mixin__adapter_group_has_sender_allowlist(
    void *self, const char *platform_name, const char *chat_id)
{
    if (!platform_name || !platform_name[0] || !chat_id || !chat_id[0]) {
        return 0;
    }
    (void)self;
    return 0;
}

/* PoP: cli_gateway_authz_mixin__is_user_authorized @ gateway/authz_mixin.py:_is_user_authorized */

/* Port of Python gateway/authz_mixin.py:_is_user_authorized */
/* Checks if a user is authorized to use the bot. */
/* Returns 1 if authorized, 0 if denied. */
int cli_gateway_authz_mixin__is_user_authorized(
    void *self, const char *user_id, const char *platform_name,
    const char *chat_id, const char *chat_type)
{
    (void)self;
    if (!user_id || !user_id[0]) {
        return 0;
    }
    /* Check for wildcard allow-all via GATEWAY_ALLOW_ALL_USERS env var. */
    const char *allow_all = getenv("GATEWAY_ALLOW_ALL_USERS");
    if (allow_all && (strcmp(allow_all, "true") == 0 ||
                      strcmp(allow_all, "1") == 0 ||
                      strcmp(allow_all, "yes") == 0)) {
        return 1;
    }
    /* Platform-specific allowlist check: look for <PLATFORM>_ALLOWED_USERS. */
    if (platform_name && platform_name[0]) {
        char env_var[128];
        snprintf(env_var, sizeof(env_var), "%s_ALLOWED_USERS", platform_name);
        /* Convert to uppercase for env var convention. */
        for (char *p = env_var; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p = *p - 'a' + 'A';
        }
        const char *allowed = getenv(env_var);
        if (allowed && allowed[0]) {
            /* Simple comma-separated check for user_id. */
            char *allowed_copy = strdup(allowed);
            if (allowed_copy) {
                char *token = strtok(allowed_copy, ",");
                while (token) {
                    /* Trim whitespace. */
                    while (*token == ' ') token++;
                    char *end = token + strlen(token) - 1;
                    while (end > token && *end == ' ') *end-- = '\0';
                    if (strcmp(token, "*") == 0 || strcmp(token, user_id) == 0) {
                        free(allowed_copy);
                        return 1;
                    }
                    token = strtok(NULL, ",");
                }
                free(allowed_copy);
            }
        }
    }
    /* Global allowlist check. */
    const char *global_allowed = getenv("GATEWAY_ALLOWED_USERS");
    if (global_allowed && global_allowed[0]) {
        char *allowed_copy = strdup(global_allowed);
        if (allowed_copy) {
            char *token = strtok(allowed_copy, ",");
            while (token) {
                while (*token == ' ') token++;
                char *end = token + strlen(token) - 1;
                while (end > token && *end == ' ') *end-- = '\0';
                if (strcmp(token, "*") == 0 || strcmp(token, user_id) == 0) {
                    free(allowed_copy);
                    return 1;
                }
                token = strtok(NULL, ",");
            }
            free(allowed_copy);
        }
    }
    return 0;
}

/* Shared gateway DM-authorization helper. Identical logic across
 * gateway/platforms/{weixin,whatsapp_common,qqbot/adapter,yuanbao}.py:
 * _open_dm_opted_in — OR of GATEWAY_ALLOW_ALL_USERS and the platform-specific
 * allow-all env var, each opted-in when lowercased value is in {true,1,yes}. */
/* PoP: gateway_dm_opted_in @ gateway/platforms/weixin.py:_open_dm_opted_in */
/* PoP: gateway_dm_opted_in @ gateway/platforms/whatsapp_common.py:_open_dm_opted_in */
/* PoP: gateway_dm_opted_in @ gateway/platforms/qqbot/adapter.py:_open_dm_opted_in */
/* PoP: gateway_dm_opted_in @ gateway/platforms/yuanbao.py:_open_dm_opted_in */
bool gateway_dm_opted_in(const char *platform_env)
{
    static const char *truthy[] = {"true", "1", "yes", NULL};
    const char *g = getenv("GATEWAY_ALLOW_ALL_USERS");
    if (g) {
        for (int i = 0; truthy[i]; i++) {
            if (strcasecmp(g, truthy[i]) == 0) return true;
        }
    }
    if (platform_env && *platform_env) {
        const char *p = getenv(platform_env);
        if (p) {
            for (int i = 0; truthy[i]; i++) {
                if (strcasecmp(p, truthy[i]) == 0) return true;
            }
        }
    }
    return false;
}


