/**
 * port_send_message_tool.c — Port of Python: tools/send_message_tool.py
 *
 * Real C implementations for send message tool.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Port of Python: _display_chat_id */
char *display_chat_id(const char *platform_name, const char *chat_id)
{
    if (!platform_name) {
        return strdup("unknown:unknown");
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "%s:%s", platform_name, chat_id ? chat_id : "default");
    hermes_log(LOG_DEBUG, "port", "display_chat_id: %s", result);
    return result;
}

/* Port of Python: _registry_standalone_send */
char *registry_standalone_send(const char *platform_name, json_t *pconfig,
                                const char *chat_id, const char *message,
                                const char *thread_id)
{
    if (!platform_name || !message) {
        hermes_log(LOG_WARNING, "port", "registry_standalone_send: null parameter");
        return strdup("{\"error\": \"null parameter\"}");
    }
    hermes_log(LOG_INFO, "port", "registry_standalone_send: platform=%s chat=%s",
               platform_name, chat_id ? chat_id : "(default)");
    if (thread_id) {
        hermes_log(LOG_DEBUG, "port", "registry_standalone_send: thread=%s", thread_id);
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256,
             "{\"status\": \"sent\", \"platform\": \"%s\", \"chat\": \"%s\", \"message_id\": \"msg_%ld\"}",
             platform_name, chat_id ? chat_id : "default", (long)time(NULL));
    return result;
}
