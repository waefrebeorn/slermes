#ifndef NOC_W22_GATEWAYCOM_7A0702_H
#define NOC_W22_GATEWAYCOM_7A0702_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_command_errors.py (gate w22_gatewaycom_7a0702).
 *
 * The module is the exception -> guidance table `hermes gateway start/stop/
 * restart` prints when systemd refuses the job. Everything it decides is pure:
 *
 *   - verb_for: which of start/stop/restart the failing `systemctl` command
 *     carried (first such token, else "start").
 *   - explain: the exact line list printed for (a) a system with no
 *     `systemctl` at all and (b) a non-zero `systemctl` exit, plus the null
 *     answer for any other exception (callers re-raise those).
 *
 * The two cases travel as data instead of exceptions: `kind` is
 * "unavailable" (SystemctlUnavailableError) or "cpe" (CalledProcessError) or
 * anything else (unexplained -> null). `cmd` is the exception's `.cmd`: an
 * array (a list, rendered with Python's repr) or a string, and `returncode`
 * is an int (a decimal string is accepted too, int64).
 */

/* PoP: w22_gatewaycom_7a0702_verb_for @ hermes_cli/gateway_command_errors.py:_verb_for
 * "start" | "stop" | "restart" -- the first token of `cmd` that names one,
 * "start" when the command carries none. A string cmd is split on whitespace;
 * an array is scanned element by element (an array element is compared as the
 * string it renders to). */
const char *w22_gatewaycom_7a0702_verb_for(const json_t *cmd);

/* PoP: w22_gatewaycom_7a0702_explain @ hermes_cli/gateway_command_errors.py:explain_service_failure
 * The lines to print, in order, or JSON null when the failure is not one this
 * table explains. For "cpe" the last line is "Details: " + str(exc), i.e.
 * "Command '<cmd repr>' returned non-zero exit status <rc>." */
json_t *w22_gatewaycom_7a0702_explain(const char *kind, const json_t *cmd,
                                      const json_t *returncode);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_GATEWAYCOM_7A0702_H */
