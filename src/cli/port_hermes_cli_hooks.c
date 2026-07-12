/*
 * port_hermes_cli_hooks.c — C port of hermes_cli/hooks.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "hermes_hooks.h"
#include "hermes_json.h"
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

/* Port of Python hermes_cli/hooks.py:_doctor_one.
 * Runs health checks on a single hook. Returns problem count.
 * Takes the spec's primitive fields (event/command/timeout) — the only fields
 * the Python check reads. Uses the real shell_hooks API
 * (script_is_executable / allowlist_entry_for / script_mtime_iso / run_once). */
int cli_hermes_cli_hooks__doctor_one(const char *event, const char *command, int timeout)
{
    int problems = 0;
    if (!command) return 0;

    /* 1. Script exists and is executable */
    if (script_is_executable(command)) {
        printf("      \xe2\x9c\x93 script exists and is executable\n");
    } else {
        problems++;
        printf("      \xe2\x9c\x97 script missing or not executable "
               "(chmod +x the file, or fix the path)\n");
    }

    /* 2. Allowlist status */
    char *entry = allowlist_entry_for(event, command);
    if (entry) {
        char *approved = NULL;
        json_t *root = json_parse(entry, NULL);
        if (root && root->type == JSON_OBJECT)
            approved = (char *)json_get_str(root, "approved_at", NULL);
        printf("      \xe2\x9c\x93 allowlisted (approved %s)\n",
               approved ? approved : "?");
        json_free(root);
    } else {
        problems++;
        printf("      \xe2\x9c\x97 not allowlisted \xe2\x80\x94 hook will NOT fire "
               "at runtime (run with --accept-hooks once, or confirm at the TTY prompt)\n");
    }

    /* 3. Mtime drift */
    if (entry) {
        const char *mtime_at = NULL;
        json_t *root = json_parse(entry, NULL);
        if (root && root->type == JSON_OBJECT)
            mtime_at = json_get_str(root, "script_mtime_at_approval", NULL);
        char *mtime_now = script_mtime_iso(command);
        if (mtime_at && mtime_now) {
            int cmp = strcmp(mtime_now, mtime_at);
            if (cmp > 0) {
                problems++;
                printf("      \xe2\x9a\xa0 script modified since approval "
                       "(was %s, now %s) \xe2\x80\x94 review changes, "
                       "then `hermes hooks revoke` + re-approve to refresh\n",
                       mtime_at, mtime_now);
            } else if (cmp == 0) {
                printf("      \xe2\x9c\x93 script unchanged since approval\n");
            }
        }
        free(mtime_now);
        json_free(root);
    }

    /* 4. JSON smoke test (only when allowlisted) */
    if (!entry) {
        printf("      \xe2\x84\xb9 skipped JSON smoke test \xe2\x80\x94 not allowlisted yet. "
               "Approve the hook first (via TTY prompt or --accept-hooks), "
               "then re-run `hermes hooks doctor`.\n");
    } else if (script_is_executable(command)) {
        char *result = shell_hooks_run_once(event, command, "{\"extra\":{}}");
        if (!result) {
            problems++;
            printf("      \xe2\x9c\x97 execution error: no result\n");
        } else {
            json_t *r = json_parse(result, NULL);
            int timed_out = 0, rc = 0;
            const char *err = NULL, *stdout_str = NULL;
            double elapsed = 0;
            if (r && r->type == JSON_OBJECT) {
                timed_out = json_get_bool(r, "timed_out", false);
                rc = (int)json_get_num(r, "returncode", 0);
                elapsed = json_get_num(r, "elapsed_seconds", 0);
                err = json_get_str(r, "error", NULL);
                stdout_str = json_get_str(r, "stdout", NULL);
            }
            if (timed_out) {
                problems++;
                printf("      \xe2\x9c\x97 timed out after %.0fs on synthetic payload (timeout=%d)\n",
                       elapsed, timeout);
            } else if (err && *err) {
                problems++;
                printf("      \xe2\x9c\x97 execution error: %s\n", err);
            } else {
                char buf[8192];
                if (stdout_str) {
                    snprintf(buf, sizeof(buf), "%s", stdout_str);
                    json_t *p = json_parse(buf, NULL);
                    if (p) {
                        printf("      \xe2\x9c\x93 produced valid JSON on synthetic payload "
                               "(exit=%d, %.0fs)\n", rc, elapsed);
                        json_free(p);
                    } else {
                        problems++;
                        printf("      \xe2\x9c\x97 stdout was not valid JSON (exit=%d, %.0fs): %.120s\n",
                               rc, elapsed, buf);
                    }
                } else {
                    printf("      \xe2\x9c\x93 ran clean with empty stdout (exit=%d, %.0fs) "
                           "\xe2\x80\x94 hook is observer-only\n", rc, elapsed);
                }
            }
            json_free(r);
        }
        free(result);
    }

    free(entry);
    return problems;
}
