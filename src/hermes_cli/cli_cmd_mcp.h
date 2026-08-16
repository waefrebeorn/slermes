#ifndef SLERMES_CLI_CMD_MCP_H
#define SLERMES_CLI_CMD_MCP_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes_core_types.h"

void cmd_mcp(const char *args, agent_state_t *state);
void cmd_reload_mcp(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_MCP_H */
