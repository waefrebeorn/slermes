#ifndef SLERMES_CLI_CMD_KANBAN_H
#define SLERMES_CLI_CMD_KANBAN_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes.h"

void cmd_kanban(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_KANBAN_H */

/* Forward declaration (defined in cli_cmd_kanban.c). */
void json_escape_arg(const char *src, char *dst, size_t dst_sz);
