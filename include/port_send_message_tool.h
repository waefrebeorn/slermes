#ifndef SLERMES_PORT_SEND_MESSAGE_TOOL_H
#define SLERMES_PORT_SEND_MESSAGE_TOOL_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;
typedef struct port_send_message_tool_state port_send_message_tool_state_t;

/* Lifecycle */
port_send_message_tool_state_t *port_send_message_tool_state_init(void);
void port_send_message_tool_state_cleanup(port_send_message_tool_state_t *state);

/* Public API */
char *display_chat_id(const char *platform_name, const char *chat_id);
char *registry_standalone_send(const char *platform_name, json_t *pconfig, const char *chat_id, const char *message, const char *thread_id);
char *send_message_display_chat_id(const char *platform_name, const char *chat_id);
double send_message_telegram_retry_delay(const char *error_text, int attempt);
int send_message_parse_target_ref(const char *platform_name, const char *target_ref, char *chat_id_out, size_t chat_id_size, char *thread_id_out, size_t thread_id_size);

#endif /* SLERMES_PORT_SEND_MESSAGE_TOOL_H */
