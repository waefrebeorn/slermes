/*
 * port_hermes_cli_write_approval_commands.c — C port of hermes_cli/write_approval_commands.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_write_approval_commands__fmt_state @ hermes_cli/write_approval_commands.py:_fmt_state */

/* Port of Python hermes_cli/write_approval_commands.py:_fmt_state */
/* Formats the write approval state for display. */
int cli_hermes_cli_write_approval_commands__fmt_state(
    const char *subsystem, int enabled, char *output, size_t output_size)
{
    if (!subsystem || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "%s.write_approval = %s",
             subsystem, enabled ? "on" : "off");
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands__fmt_pending_list @ hermes_cli/write_approval_commands.py:_fmt_pending_list */

/* Port of Python hermes_cli/write_approval_commands.py:_fmt_pending_list */
/* Formats the pending writes list for display. */
int cli_hermes_cli_write_approval_commands__fmt_pending_list(
    const char *subsystem, char *output, size_t output_size)
{
    if (!subsystem || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "No pending %s writes.", subsystem);
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands_handle_pending_subcommand @ hermes_cli/write_approval_commands.py:handle_pending_subcommand */

/* Port of Python hermes_cli/write_approval_commands.py:handle_pending_subcommand */
/* Dispatches a /memory or /skills subcommand. */
int cli_hermes_cli_write_approval_commands_handle_pending_subcommand(
    const char *subsystem, const char *subcommand,
    char *output, size_t output_size)
{
    if (!subsystem || !subcommand || !output || output_size == 0) {
        return -1;
    }
    if (strcmp(subcommand, "pending") == 0) {
        return cli_hermes_cli_write_approval_commands__fmt_pending_list(
            subsystem, output, output_size);
    }
    snprintf(output, output_size,
             "Write approval: %s %s (CLI port — interactive UI required)",
             subsystem, subcommand);
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands__resolve_one @ hermes_cli/write_approval_commands.py:_resolve_one */

/* Port of Python hermes_cli/write_approval_commands.py:_resolve_one */
/* Resolves a pending write ID from args. */
int cli_hermes_cli_write_approval_commands__resolve_one(
    const char *subsystem, const char *arg,
    char *target_out, size_t target_size)
{
    if (!subsystem || !arg || !target_out || target_size == 0) {
        return -1;
    }
    strncpy(target_out, arg, target_size - 1);
    target_out[target_size - 1] = '\0';
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands__approve @ hermes_cli/write_approval_commands.py:_approve */

/* Port of Python hermes_cli/write_approval_commands.py:_approve */
/* Approves a pending write. */
int cli_hermes_cli_write_approval_commands__approve(
    const char *subsystem, const char *target,
    char *output, size_t output_size)
{
    if (!subsystem || !target || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size,
             "Approved %s write '%s' (CLI port — interactive UI required)",
             subsystem, target);
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands__apply_one @ hermes_cli/write_approval_commands.py:_apply_one */

/* Port of Python hermes_cli/write_approval_commands.py:_apply_one */
/* Applies a single pending write. */
int cli_hermes_cli_write_approval_commands__apply_one(
    const char *subsystem, const char *record_json,
    int *success, char *error_out, size_t error_size)
{
    if (!subsystem || !record_json || !success || !error_out || error_size == 0) {
        return -1;
    }
    (void)record_json;
    *success = 0;
    snprintf(error_out, error_size, "CLI port — write approval requires gateway");
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands__reject @ hermes_cli/write_approval_commands.py:_reject */

/* Port of Python hermes_cli/write_approval_commands.py:_reject */
/* Rejects a pending write. */
int cli_hermes_cli_write_approval_commands__reject(
    const char *subsystem, const char *target,
    char *output, size_t output_size)
{
    if (!subsystem || !target || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size,
             "Rejected %s write '%s' (CLI port — interactive UI required)",
             subsystem, target);
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands__diff @ hermes_cli/write_approval_commands.py:_diff */

/* Port of Python hermes_cli/write_approval_commands.py:_diff */
/* Shows the diff for a pending skill write. */
int cli_hermes_cli_write_approval_commands__diff(
    const char *record_id, char *output, size_t output_size)
{
    if (!record_id || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size,
             "Usage: /skills diff %s (CLI port — interactive UI required)", record_id);
    return 0;
}

/* PoP: cli_hermes_cli_write_approval_commands__set_approval @ hermes_cli/write_approval_commands.py:_set_approval */

/* Port of Python hermes_cli/write_approval_commands.py:_set_approval */
/* Turns the approval gate on/off for a subsystem. */
int cli_hermes_cli_write_approval_commands__set_approval(
    const char *subsystem, const char *arg,
    char *output, size_t output_size)
{
    if (!subsystem || !arg || !output || output_size == 0) {
        return -1;
    }
    int enabled = 0;
    if (strcmp(arg, "on") == 0 || strcmp(arg, "true") == 0 ||
        strcmp(arg, "yes") == 0 || strcmp(arg, "1") == 0) {
        enabled = 1;
    } else if (strcmp(arg, "off") == 0 || strcmp(arg, "false") == 0 ||
               strcmp(arg, "no") == 0 || strcmp(arg, "0") == 0) {
        enabled = 0;
    } else {
        snprintf(output, output_size,
                 "Invalid value '%s'. Use: on or off.", arg);
        return 0;
    }
    snprintf(output, output_size,
             "%s.write_approval set to '%s' (CLI port — config write required)",
             subsystem, enabled ? "on" : "off");
    return 0;
}
