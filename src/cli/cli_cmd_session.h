#ifndef SLERMES_CLI_CMD_SESSION_H
#define SLERMES_CLI_CMD_SESSION_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes_core_types.h"

void cmd_agents(const char *args, agent_state_t *state);
void cmd_background(const char *args, agent_state_t *state);
void cmd_branch(const char *args, agent_state_t *state);
void cmd_compress(const char *args, agent_state_t *state);
void cmd_conv(const char *args, agent_state_t *state);
void cmd_goal(const char *args, agent_state_t *state);
void cmd_history(const char *args, agent_state_t *state);
void cmd_insights(const char *args, agent_state_t *state);
void cmd_load(const char *args, agent_state_t *state);
void cmd_new(const char *args, agent_state_t *state);
void cmd_queue(const char *args, agent_state_t *state);
void cmd_recap(const char *args, agent_state_t *state);
void cmd_reset(const char *args, agent_state_t *state);
void cmd_resume(const char *args, agent_state_t *state);
void cmd_retry(const char *args, agent_state_t *state);
void cmd_rollback(const char *args, agent_state_t *state);
void cmd_save(const char *args, agent_state_t *state);
void cmd_session_export(const char *args, agent_state_t *state);
void cmd_session_import(const char *args, agent_state_t *state);
void cmd_session_search(const char *args, agent_state_t *state);
void cmd_sessions(const char *args, agent_state_t *state);
void cmd_snapshot(const char *args, agent_state_t *state);
void cmd_stats(const char *args, agent_state_t *state);
/* PoP: cmd_status @ hermes_cli/console_engine.py:_status */
void cmd_status(const char *args, agent_state_t *state);
void cmd_steer(const char *args, agent_state_t *state);
void cmd_subgoal(const char *args, agent_state_t *state);
void cmd_title(const char *args, agent_state_t *state);
void cmd_undo(const char *args, agent_state_t *state);
/* PoP: cmd_usage @ hermes_cli/curator.py:_cmd_usage */
void cmd_usage(const char *args, agent_state_t *state);

/* PoP: cli_pt_input_extras_install_ignored_terminal_sequences @ hermes_cli/pt_input_extras.py:install_ignored_terminal_sequences */
int cli_pt_input_extras_install_ignored_terminal_sequences(void);

#endif /* SLERMES_CLI_CMD_SESSION_H */
