/*
 * port_cli_process_dock.h — pure helpers from hermes_cli/cli_process_dock.py.
 * The process registry IO (process_rows/process_tail) and locale-rendered
 * process_activity stay unported.
 */
#ifndef PORT_CLI_PROCESS_DOCK_H
#define PORT_CLI_PROCESS_DOCK_H

/* dock._last_output_line(buffer): last non-empty line, whitespace-collapsed.
 * Returns malloc'd text ("" when none). Caller frees. */
char *cpa_last_output_line(const char *buffer);

/* dock.process_status: "running" unless exited, then the completion-reason
 * mapping ("killed"/"lost"/"failed"), else "done"/"failed" by exit_code.
 * Returns a static literal. exited=1 when status=="exited"; has_exit_code is
 * truthy when the entry has a non-None exit_code. */
const char *cpa_process_status(int exited, const char *completion_reason,
                               int has_exit_code);

/* dock.process_glyph: the dock glyph for a status, "?" for unknown. */
const char *cpa_process_glyph(const char *status);

#endif /* PORT_CLI_PROCESS_DOCK_H */
