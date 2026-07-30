#ifndef AGENT_REDACT_HELPERS_H
#define AGENT_REDACT_HELPERS_H
#include <stdbool.h>
char *agent_redact_mask_token_nonreusable(const char *token);
bool agent_redact_is_env_dump_command(const char *command);
#endif
