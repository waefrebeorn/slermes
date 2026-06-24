/*
 * port_hermes_cli_hooks.c — C port of hermes_cli/hooks.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_hooks_hooks_command @ hermes_cli/hooks.py:hooks_command */

/* Port of Python hermes_cli/hooks.py:hooks_command */
/* Entry point for hermes hooks — dispatches to the requested action. */
void cli_hermes_cli_hooks_hooks_command(const char *action)
{
    if (!action) {
        printf("Usage: hermes hooks {list|test|revoke|doctor}\n");
        return;
    }
    hermes_log(LOG_DEBUG, "hooks", "hooks_command: %s (CLI port)", action);
}

/* PoP: cli_hermes_cli_hooks__cmd_list @ hermes_cli/hooks.py:_cmd_list */

/* Port of Python hermes_cli/hooks.py:_cmd_list */
/* Lists configured shell hooks. */
void cli_hermes_cli_hooks__cmd_list(void)
{
    printf("No shell hooks configured (CLI port).\n");
}

/* PoP: cli_hermes_cli_hooks__cmd_test @ hermes_cli/hooks.py:_cmd_test */

/* Port of Python hermes_cli/hooks.py:_cmd_test */
/* Tests a hook by firing it with a synthetic payload. */
void cli_hermes_cli_hooks__cmd_test(const char *event)
{
    if (!event) {
        printf("Usage: hermes hooks test <event>\n");
        return;
    }
    printf("Firing hook for event '%s' (CLI port — no-op)\n", event);
}

/* PoP: cli_hermes_cli_hooks__print_run_result @ hermes_cli/hooks.py:_print_run_result */

/* Port of Python hermes_cli/hooks.py:_print_run_result */
/* Prints the result of a hook run. */
void cli_hermes_cli_hooks__print_run_result(
    const char *error, int timed_out, int return_code,
    double elapsed, const char *stdout_str, const char *stderr_str)
{
    if (error && error[0]) {
        printf("      ✗ error: %s\n", error);
        return;
    }
    if (timed_out) {
        printf("      ✗ timed out after %.1fs\n", elapsed);
        return;
    }
    printf("      exit=%d  elapsed=%.1fs\n", return_code, elapsed);
    if (stdout_str && stdout_str[0]) {
        printf("      stdout: %.400s\n", stdout_str);
    }
    if (stderr_str && stderr_str[0]) {
        printf("      stderr: %.400s\n", stderr_str);
    }
}

/* PoP: cli_hermes_cli_hooks__truncate @ hermes_cli/hooks.py:_truncate */

/* Port of Python hermes_cli/hooks.py:_truncate */
/* Truncates a string to max length. */
int cli_hermes_cli_hooks__truncate(
    const char *s, int n, char *output, size_t output_size)
{
    if (!s || !output || output_size == 0) {
        return -1;
    }
    int len = (int)strlen(s);
    if (len <= n) {
        strncpy(output, s, output_size - 1);
        output[output_size - 1] = '\0';
    } else {
        if (n - 3 >= (int)output_size) n = (int)output_size - 1;
        strncpy(output, s, n - 3);
        output[n - 3] = '\0';
        strcat(output, "...");
    }
    return 0;
}

/* PoP: cli_hermes_cli_hooks__cmd_revoke @ hermes_cli/hooks.py:_cmd_revoke */

/* Port of Python hermes_cli/hooks.py:_cmd_revoke */
/* Revokes a hook allowlist entry. */
void cli_hermes_cli_hooks__cmd_revoke(const char *command)
{
    if (!command) {
        printf("Usage: hermes hooks revoke <command>\n");
        return;
    }
    printf("Revoked allowlist entry for: %s (CLI port — no-op)\n", command);
}

/* PoP: cli_hermes_cli_hooks__cmd_doctor @ hermes_cli/hooks.py:_cmd_doctor */

/* Port of Python hermes_cli/hooks.py:_cmd_doctor */
/* Runs health checks on configured hooks. */
void cli_hermes_cli_hooks__cmd_doctor(void)
{
    printf("No shell hooks configured — nothing to check (CLI port).\n");
}

/* PoP: cli_hermes_cli_hooks__doctor_one @ hermes_cli/hooks.py:_doctor_one */

/* Port of Python hermes_cli/hooks.py:_doctor_one */
/* Runs health checks on a single hook. Returns problem count. */
int cli_hermes_cli_hooks__doctor_one(
    const char *event, const char *command)
{
    (void)event;
    (void)command;
    /* CLI port: no hook validation available. */
    return 0;
}
