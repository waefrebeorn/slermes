/* Slermes C port — agent/redact.py (pure helper subset)
 * Header for port_agent_redact_helpers.c
 */
#ifndef PORT_AGENT_REDACT_HELPERS_H
#define PORT_AGENT_REDACT_HELPERS_H

#include <stdbool.h>

bool agent_redact_is_env_dump_command(const char *command);
bool agent_redact_is_word_start(const char *s, size_t i);
bool agent_redact_is_word_end(const char *s, size_t j, bool allow_plural);
bool agent_redact_key_has_secret_keyword(const char *key);
bool agent_redact_command_reads_env_file(const char *command);

#endif /* PORT_AGENT_REDACT_HELPERS_H */
