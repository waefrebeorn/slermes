#ifndef SLERMES_CLI_CMD_GATEWAY_H
#define SLERMES_CLI_CMD_GATEWAY_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes_core_types.h"

void cmd_gateway(const char *args, agent_state_t *state);
void cmd_platform(const char *args, agent_state_t *state);
/* PoP: cmd_restart @ hermes_cli/proxy_cli.py:cmd_restart */
void cmd_restart(const char *args, agent_state_t *state);
void cmd_webhook(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_GATEWAY_H */
