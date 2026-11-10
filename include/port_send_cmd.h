/*
 * port_send_cmd.h -- pure helpers ported from hermes_cli/send_cmd.py.
 *
 * The send path itself (channel directory, gateway config, the actual send)
 * stays in Python; the result interpretation is here.
 */
#ifndef PORT_SEND_CMD_H
#define PORT_SEND_CMD_H

#include <stddef.h>

/* send_cmd.py:_emit_result's exit-code decision.
 * Returns 0 when the payload has no truthy "error" and a truthy "skipped" or
 * "success"; 1 otherwise. Unknown shapes are failures so scripts notice. */
int sc_exit_code(const char *result_json);

/* send_cmd.py:_emit_result's output routing.
 *
 * Returns one of:
 *   SC_DISPLAY_SUPPRESSED (0)  quiet, not -j
 *   SC_DISPLAY_ERROR      (1)  payload["error"] -> stderr
 *   SC_DISPLAY_NOTE       (2)  payload["success"] -> stdout: note or "sent"
 *   SC_DISPLAY_UNKNOWN    (3)  neither -> stdout: the whole payload
 *   SC_DISPLAY_JSON_MODE  (4)  -j: the whole payload on stdout
 *
 * out_text (malloc'd, caller frees) receives the note text for
 * SC_DISPLAY_NOTE; NULL otherwise. out_to_stderr is set only for
 * SC_DISPLAY_ERROR. */
int sc_display(const char *result_json, int json_mode, int quiet,
               char **out_text, int *out_to_stderr);

#define SC_DISPLAY_SUPPRESSED 0
#define SC_DISPLAY_ERROR      1
#define SC_DISPLAY_NOTE       2
#define SC_DISPLAY_UNKNOWN    3
#define SC_DISPLAY_JSON_MODE  4

#endif /* PORT_SEND_CMD_H */
