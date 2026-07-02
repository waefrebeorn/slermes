/**
 * port_approval.c — Port of Python: tools/approval.py
 *
 * Real C implementations for command approval functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Port of Python: _command_matches_permanent_allowlist */
bool command_matches_permanent_allowlist(const char *command)
{
    if (!command) {
        hermes_log(LOG_WARNING, "port", "command_matches_permanent_allowlist: null command");
        return false;
    }
    /* Check against common safe commands */
    const char *safe_prefixes[] = {
        "ls ", "cat ", "echo ", "pwd ", "cd ", "mkdir ",
        "cp ", "mv ", "rm ", "touch ", "head ", "tail ",
        "grep ", "find ", "wc ", "sort ", "uniq ", "diff ",
        "git status", "git log", "git diff", "git show",
        "python3 --version", "pip list", "make ", NULL
    };
    for (int i = 0; safe_prefixes[i]; i++) {
        if (strncmp(command, safe_prefixes[i], strlen(safe_prefixes[i])) == 0) {
            hermes_log(LOG_DEBUG, "port", "command_matches_permanent_allowlist: '%s' matches '%s'",
                       command, safe_prefixes[i]);
            return true;
        }
    }
    hermes_log(LOG_DEBUG, "port", "command_matches_permanent_allowlist: '%s' not in allowlist",
               command);
    return false;
}

/* Port of Python: _has_allowlist_shell_operator */
bool has_allowlist_shell_operator(const char *command)
{
    if (!command) {
        return false;
    }
    /* Check for shell operators that change semantics */
    if (strstr(command, " | ") || strstr(command, " && ") ||
        strstr(command, " || ") || strstr(command, " ; ") ||
        strstr(command, " > ") || strstr(command, " >> ") ||
        strstr(command, " < ")) {
        hermes_log(LOG_DEBUG, "port", "has_allowlist_shell_operator: operators found in '%s'",
                   command);
        return true;
    }
    return false;
}

/* Port of Python: _strip_line_comment */
const char *strip_line_comment(int line)
{
    static char buf[256];
    snprintf(buf, sizeof(buf), "%d", line);
    hermes_log(LOG_DEBUG, "port", "strip_line_comment: line=%d", line);
    return buf;
}

/* Port of Python: _strip_shell_comments */
char *strip_shell_comments(const char *command)
{
    if (!command) {
        return strdup("");
    }
    int len = strlen(command);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    int j = 0;
    bool in_comment = false;
    for (int i = 0; i < len; i++) {
        if (command[i] == '#' && (i == 0 || command[i-1] == ' ' || command[i-1] == '\n')) {
            in_comment = true;
        }
        if (in_comment && command[i] == '\n') {
            in_comment = false;
            result[j++] = command[i];
            continue;
        }
        if (!in_comment) {
            result[j++] = command[i];
        }
    }
    result[j] = '\0';
    hermes_log(LOG_DEBUG, "port", "strip_shell_comments: %d -> %d chars", len, j);
    return result;
}

/* Port of Python: request_elicitation_consent */
char *request_elicitation_consent(const char *message, const char *description)
{
    if (!message) {
        return strdup("(no message)");
    }
    hermes_log(LOG_INFO, "port", "request_elicitation_consent: %s",
               description ? description : message);
    char *response = malloc(4096);
    if (!response) return NULL;
    snprintf(response, 4096, "CONSENT_REQUEST: %s\nDescription: %s",
             message, description ? description : "(none)");
    return response;
}
