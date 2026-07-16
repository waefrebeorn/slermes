#ifndef SLERMES_CLI_CMD_SYSTEM_H
#define SLERMES_CLI_CMD_SYSTEM_H

#include <stdbool.h>
#include <stdio.h>
#include "hermes.h"
#include "commands_shared.h"

void cmd_approve(const char *args, agent_state_t *state);
void cmd_browser(const char *args, agent_state_t *state);
void cmd_busy(const char *args, agent_state_t *state);
void cmd_clear(const char *args, agent_state_t *state);
void cmd_commands(const char *args, agent_state_t *state);
void cmd_completions(const char *args, agent_state_t *state);
void cmd_copy(const char *args, agent_state_t *state);
void cmd_cron(const char *args, agent_state_t *state);
void cmd_dashboard(const char *args, agent_state_t *state);
void cmd_debug(const char *args, agent_state_t *state);
void cmd_deny(const char *args, agent_state_t *state);
void cmd_deps(const char *args, agent_state_t *state);
void cmd_doctor(const char *args, agent_state_t *state);
void cmd_dump(const char *args, agent_state_t *state);
void cmd_exit(const char *args, agent_state_t *state);
void cmd_handoff(const char *args, agent_state_t *state);
void cmd_indicator(const char *args, agent_state_t *state);
void cmd_logs(const char *args, agent_state_t *state);
void cmd_paste(const char *args, agent_state_t *state);
void cmd_platforms(const char *args, agent_state_t *state);
void cmd_profile(const char *args, agent_state_t *state);
void cmd_redraw(const char *args, agent_state_t *state);
void cmd_reload(const char *args, agent_state_t *state);
void cmd_send(const char *args, agent_state_t *state);
void cmd_sethome(const char *args, agent_state_t *state);
void cmd_skin(const char *args, agent_state_t *state);
void cmd_statusbar(const char *args, agent_state_t *state);
void cmd_stop(const char *args, agent_state_t *state);
void cmd_tools(const char *args, agent_state_t *state);
void cmd_tools_verify(const char *args, agent_state_t *state);
void cmd_toolsets(const char *args, agent_state_t *state);
void cmd_update(const char *args, agent_state_t *state);
void cmd_verbose(const char *args, agent_state_t *state);
void cmd_whoami(const char *args, agent_state_t *state);

#endif /* SLERMES_CLI_CMD_SYSTEM_H */

/* Forward declaration (defined in cli_cmd_system.c). */
void handoff_read_dir(list_t *entries);
