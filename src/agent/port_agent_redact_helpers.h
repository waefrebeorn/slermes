#ifndef AGENT_REDACT_HELPERS_H
#define AGENT_REDACT_HELPERS_H
#include <stdbool.h>
#include <stddef.h>
char *agent_redact_mask_token_nonreusable(const char *token);
bool agent_redact_is_env_dump_command(const char *command);
bool agent_redact_is_word_start(const char *s, size_t i);
bool agent_redact_is_word_end(const char *s, size_t j, bool allow_plural);
bool agent_redact_key_has_secret_keyword(const char *key);
#endif
