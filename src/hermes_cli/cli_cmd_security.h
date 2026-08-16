#ifndef SLERMES_CLI_CMD_SECURITY_H
#define SLERMES_CLI_CMD_SECURITY_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes_core_types.h"

void cmd_auth(const char *args, agent_state_t *state);
void cmd_key(const char *args, agent_state_t *state);
void cmd_secrets(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_SECURITY_H */
