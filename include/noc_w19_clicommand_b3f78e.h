#ifndef NOC_W19_CLICOMMAND_B3F78E_H
#define NOC_W19_CLICOMMAND_B3F78E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_clicommand_b3f78e_keep @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_keep
 * Print the lines and keep the CLI session: {"result":true,"lines":[...]}. */
json_t *w19_clicommand_b3f78e_keep(json_t *lines);

/* PoP: w19_clicommand_b3f78e_prepare_session @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_prepare_session
 * Refuse mid-turn, refuse with no store at all, offer a row-less session the handoff
 * title, and return the display title (NULL after printing why the handoff cannot start). */
json_t *w19_clicommand_b3f78e_prepare_session(int agent_running, int store_present,
                                             int session_db_present, int session_exists,
                                             int set_title_raises, int get_title_raises,
                                             const char *session_id, const char *title_value);

/* PoP: w19_clicommand_b3f78e_validate_target @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_validate_target
 * {"result":<home name|NULL>,"lines":[...]}: the destination home, or NULL after printing
 * why (unknown/disabled platform, no home channel, config load failure). */
json_t *w19_clicommand_b3f78e_validate_target(const char *platform_name, const char *outcome,
                                              const char *home_name);

/* PoP: w19_clicommand_b3f78e_handoff_wait @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_wait
 * The two-phase 0.5s poll: PENDING 60s then CAS-fail and retry, RUNNING 15 min with a
 * heartbeat and no row touch. Drives an injected state script and clock (the poll's
 * seams) and reports result/lines/should_exit/reads/fail_calls/sleeps. */
json_t *w19_clicommand_b3f78e_handoff_wait(json_t *states, int fail_pending_raises,
                                           const char *platform_name, const char *session_title);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CLICOMMAND_B3F78E_H */
