/*
 * cli_cmd_parity.h — Command-surface parity handlers
 *
 * Closes the name-parity gap between the C CLI command table and the Python
 * COMMAND_REGISTRY (hermes_cli/commands.py). Each handler here ports the
 * Python command of the SAME NAME so `slermes /<cmd>` matches `hermes /<cmd>`
 * 1:1. Commands: battery, blueprint, codex-runtime, egress, hatch, journey,
 * learn, moa, prompt, start, subscription, suggestions, timestamps, topup.
 */

#ifndef SLERMES_CLI_CMD_PARITY_H
#define SLERMES_CLI_CMD_PARITY_H

#include "hermes_core_types.h"

void cmd_battery(const char *args, agent_state_t *state);
void cmd_blueprint(const char *args, agent_state_t *state);
void cmd_codex_runtime(const char *args, agent_state_t *state);
void cmd_egress(const char *args, agent_state_t *state);
void cmd_hatch(const char *args, agent_state_t *state);
void cmd_journey(const char *args, agent_state_t *state);
void cmd_learn(const char *args, agent_state_t *state);
void cmd_moa(const char *args, agent_state_t *state);
void cmd_prompt(const char *args, agent_state_t *state);
void cmd_start(const char *args, agent_state_t *state);
void cmd_subscription(const char *args, agent_state_t *state);
void cmd_suggestions(const char *args, agent_state_t *state);
void cmd_timestamps(const char *args, agent_state_t *state);
void cmd_topup(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_PARITY_H */
