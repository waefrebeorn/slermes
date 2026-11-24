#ifndef NOC_W17_CLISESSION_E88AF5_H
#define NOC_W17_CLISESSION_E88AF5_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/cli_session_switch.py (gate w17_clisession_e88af5).
 * Both helpers return a JSON summary of the observable side effects they
 * perform on the injected fake cli/agent state. */
json_t *w17_clisession_e88af5_end_current_session(const json_t *state);
json_t *w17_clisession_e88af5_sync_agent_to_session(const json_t *state);

#endif /* NOC_W17_CLISESSION_E88AF5_H */
