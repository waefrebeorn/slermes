#ifndef CLI_PORT_AGENT_DISPLAY_H
#define CLI_PORT_AGENT_DISPLAY_H

#include <stddef.h>

/* PoP ports from agent/display.py — see port_agent_display.c.
 * Caller frees returned strings / string arrays. */

char **cli_agent_display__split_shell_words(const char *segment, int *count);
char *cli_agent_display__strip_shell_pipe_tail(const char *segment);
char **cli_agent_display__split_shell_compound(const char *command, int *count);
char *cli_agent_display__clean_shell_segment(const char *segment);
int cli_agent_display__is_shell_boundary_echo(const char *segment);
char *cli_agent_display__summarize_shell_command(const char *command);

#endif
