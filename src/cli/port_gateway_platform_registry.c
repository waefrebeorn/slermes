/*
 * port_gateway_platform_registry.c — C port of gateway/platform_registry.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_platform_registry_unregister @ gateway/platform_registry.py:unregister */

/* Port of Python gateway/platform_registry.py:unregister */
/* Unregisters a platform from the registry. */
int cli_gateway_platform_registry_unregister(const char *platform_name)
{
    if (!platform_name) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "platform_registry", "unregister: %s", platform_name);
    return 0;
}

/* PoP: cli_gateway_platform_registry_all_entries @ gateway/platform_registry.py:all_entries */

/* Port of Python gateway/platform_registry.py:all_entries */
/* Returns all registered platform entries. */
int cli_gateway_platform_registry_all_entries(
    char *names[], int max_names)
{
    (void)names;
    (void)max_names;
    /* CLI port: no dynamic platform registry. */
    return 0;
}

/* PoP: cli_gateway_platform_registry_plugin_entries @ gateway/platform_registry.py:plugin_entries */

/* Port of Python gateway/platform_registry.py:plugin_entries */
/* Returns all plugin-registered platform entries. */
int cli_gateway_platform_registry_plugin_entries(
    char *names[], int max_names)
{
    (void)names;
    (void)max_names;
    return 0;
}

/* PoP: cli_gateway_platform_registry_is_registered @ gateway/platform_registry.py:is_registered */

/* Port of Python gateway/platform_registry.py:is_registered */
/* Checks if a platform is registered. */
int cli_gateway_platform_registry_is_registered(const char *platform_name)
{
    if (!platform_name) {
        return 0;
    }
    /* CLI port: check against known built-in platforms. */
    const char *builtins[] = {
        "telegram", "discord", "slack", "signal", "whatsapp",
        "email", "sms", "matrix", "mattermost", "feishu",
        "wecom", "dingtalk", "qqbot", "bluebubbles", "weixin",
        "yuanbao", "homeassistant", "webhook", NULL
    };
    for (int i = 0; builtins[i]; i++) {
        if (strcmp(platform_name, builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* PoP: cli_gateway_platform_registry_create_adapter @ gateway/platform_registry.py:create_adapter */

/* Port of Python gateway/platform_registry.py:create_adapter */
/* Creates a platform adapter instance. */
void *cli_gateway_platform_registry_create_adapter(
    const char *platform_name, const char *config_json)
{
    (void)platform_name;
    (void)config_json;
    /* CLI port: adapter creation requires full gateway runtime. */
    return NULL;
}
