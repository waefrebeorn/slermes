/*
 * port_turn_summary_cli.h — C port of cli.py's turn_summary wrapper methods.
 */
#ifndef PORT_TURN_SUMMARY_CLI_H
#define PORT_TURN_SUMMARY_CLI_H

#include <stdbool.h>
#include "port_turn_summary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cli_turn_summary_mgr cli_turn_summary_mgr_t;

/* PoP: _spinner_token_flow @ cli.py:_spinner_token_flow */
char *cli_turn_spinner_token_flow(cli_turn_summary_mgr_t *mgr, void *agent);
/* PoP: _turn_summary_is_active @ cli.py:_turn_summary_is_active */
bool cli_turn_summary_is_active(cli_turn_summary_mgr_t *mgr);
/* PoP: _turn_summary_begin @ cli.py:_turn_summary_begin */
void cli_turn_summary_begin(cli_turn_summary_mgr_t *mgr, void *agent);
/* PoP: _turn_summary_record @ cli.py:_turn_summary_record */
void cli_turn_summary_record(cli_turn_summary_mgr_t *mgr,
                              const char *function_name,
                              const json_t *result, bool is_error);
/* PoP: _turn_summary_emit @ cli.py:_turn_summary_emit */
char *cli_turn_summary_emit(cli_turn_summary_mgr_t *mgr);

/* Lifecycle */
cli_turn_summary_mgr_t *cli_turn_summary_mgr_new(void);
void cli_turn_summary_mgr_free(cli_turn_summary_mgr_t *m);

/* Setters — driven by the CLI agent loop. */
void cli_turn_set_enabled(cli_turn_summary_mgr_t *m, bool v);
void cli_turn_set_interactive(cli_turn_summary_mgr_t *m, bool v);
void cli_turn_set_spinner_enabled(cli_turn_summary_mgr_t *m, bool v);
void cli_turn_set_agent_running(cli_turn_summary_mgr_t *m, bool v);
void cli_turn_set_quiet_mode(cli_turn_summary_mgr_t *m, bool v);
void cli_turn_set_tool_progress_mode(cli_turn_summary_mgr_t *m, const char *mode);
void cli_turn_set_agent_output_tokens(cli_turn_summary_mgr_t *m, long tokens);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TURN_SUMMARY_CLI_H */
