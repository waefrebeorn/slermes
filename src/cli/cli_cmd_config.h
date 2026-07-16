#ifndef SLERMES_CLI_CMD_CONFIG_H
#define SLERMES_CLI_CMD_CONFIG_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes.h"

void cmd_backup(const char *args, agent_state_t *state);
void cmd_config(const char *args, agent_state_t *state);
void cmd_fast(const char *args, agent_state_t *state);
void cmd_footer(const char *args, agent_state_t *state);
void cmd_model(const char *args, agent_state_t *state);
void cmd_personality(const char *args, agent_state_t *state);
void cmd_reasoning(const char *args, agent_state_t *state);
void cmd_setup(const char *args, agent_state_t *state);
void cmd_topic(const char *args, agent_state_t *state);
void cmd_uninstall(const char *args, agent_state_t *state);
void cmd_voice(const char *args, agent_state_t *state);
void cmd_yolo(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_CONFIG_H */

/* Forward declaration (defined in cli_cmd_config.c). */
void list_config_groups(void);
