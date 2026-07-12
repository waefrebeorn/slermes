#ifndef SLERMES_CLI_CMD_SKILLS_H
#define SLERMES_CLI_CMD_SKILLS_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes.h"

void cmd_bundles(const char *args, agent_state_t *state);
void cmd_curator(const char *args, agent_state_t *state);
void cmd_reload_skills(const char *args, agent_state_t *state);
void cmd_skills(const char *args, agent_state_t *state);
void cmd_skills_hub(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_SKILLS_H */
