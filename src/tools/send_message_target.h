#ifndef SRC_TOOLS_SEND_MESSAGE_TARGET_H
#define SRC_TOOLS_SEND_MESSAGE_TARGET_H

/*
 * send_message_target.h — focused extraction from tools/send_message_tool.py
 *
 * Pure, oracle-verifiable target/display helpers (no gateway). See
 * send_message_target.c for the faithful-subset scope note on
 * parse_target_ref (telegram/feishu/discord/slack only; matrix/phone/etc.
 * are not yet in this C subset).
 */

#include <stdbool.h>
#include <stddef.h>

/* Parse a platform target into chat_id/thread_id. Returns 1 if explicit
 * (platform + split found), 0 otherwise. Mirrors _parse_target_ref for the
 * four platforms it implements. */
int send_message_target_parse_target_ref(const char *platform_name, const char *target_ref,
                                          char *chat_id_out, size_t chat_id_size,
                                          char *thread_id_out, size_t thread_id_size);

/* Log-safe chat id; redacts signal group ids to "group:***". */
char *send_message_target_display_chat_id(const char *platform_name, const char *chat_id);

/* Telegram retry backoff seconds; negative => do not retry. */
double send_message_target_telegram_retry_delay(const char *error_text, int attempt);

#endif /* SRC_TOOLS_SEND_MESSAGE_TARGET_H */
