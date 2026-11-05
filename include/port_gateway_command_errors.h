#ifndef PORT_GATEWAY_COMMAND_ERRORS_H
#define PORT_GATEWAY_COMMAND_ERRORS_H

#include "hermes_json.h"

/* Port of Python: hermes_cli/gateway_command_errors.py
 *
 * User-facing copy for `hermes gateway start/stop/restart` failures on
 * systemd hosts. Pure formatting; no subprocess calls.
 */

/* Fault kinds the Python module knows how to explain. */
enum {
    GCE_FAULT_NONE = 0,          /* not ours — caller re-raises          */
    GCE_FAULT_NO_SYSTEMCTL = 1,  /* SystemctlUnavailableError            */
    GCE_FAULT_SYSTEMCTL_FAILED = 2 /* subprocess.CalledProcessError      */
};

/* Return the verb named inside a command string ("start"/"stop"/"restart"),
 * else "start" — mirrors _verb_for's token scan. Never NULL. */
const char *gce_verb_for(const char *cmd);

/* The journalctl hint embedded in the second guidance line. */
const char *gce_journal_hint(void);

/* Python's str(subprocess.CalledProcessError) for an argv-style command:
 *   Command '['systemctl', '--user', 'restart', 'hermes-gateway']' returned
 *   non-zero exit status N.
 * Returns a malloc'd string (caller frees) or NULL. */
char *gce_called_process_error_text(const char *const *argv, int argc,
                                    int returncode);

/* Lines to print for a fault, or NULL when kind is GCE_FAULT_NONE.
 * Returns a JSON array of strings; caller owns it (json_free). */
json_t *gce_explain_service_failure(int kind, const char *verb,
                                    const char *details);

#endif /* PORT_GATEWAY_COMMAND_ERRORS_H */
