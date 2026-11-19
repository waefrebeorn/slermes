/*
 * port_gateway_command_errors.c — Port of Python: hermes_cli/gateway_command_errors.py
 *
 * Turns a failed `hermes gateway start|stop|restart` into the same guidance
 * lines the Python CLI prints. Pure formatting: the subprocess has already
 * run (and failed) by the time these helpers are called, so nothing here
 * spawns or shells out.
 */

#include "port_gateway_command_errors.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define GCE_JOURNAL_HINT \
    "journalctl --user -u hermes-gateway --since \"5 min ago\""

/* PoP: gce_journal_hint @ hermes_cli/gateway_command_errors.py:_JOURNAL_HINT */
const char *gce_journal_hint(void)
{
    return GCE_JOURNAL_HINT;
}

/* PoP: gce_verb_for @ hermes_cli/gateway_command_errors.py:_verb_for */
const char *gce_verb_for(const char *cmd)
{
    if (!cmd) return "start";

    /* Python: exc.cmd if isinstance(list|tuple) else str(exc.cmd).split().
     * A C caller always holds the command as text, so tokenize on
     * whitespace; only a whole-token match counts. */
    const char *p = cmd;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        const char *start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = (size_t)(p - start);
        if (len == 5 && strncmp(start, "start", 5) == 0) return "start";
        if (len == 4 && strncmp(start, "stop", 4) == 0) return "stop";
        if (len == 7 && strncmp(start, "restart", 7) == 0) return "restart";
    }
    return "start";
}

static void append_str_line(json_t *arr, const char *line)
{
    if (!arr || !line) return;
    json_append(arr, json_string(line));
}

/* Python repr() of a str token, as it appears inside a list repr. */
static void append_repr_token(char *dst, size_t cap, size_t *o, const char *tok)
{
    int has_sq = strchr(tok, '\'') != NULL;
    int has_dq = strchr(tok, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    if (*o + 2 < cap) dst[(*o)++] = quote;
    for (const char *p = tok; *p && *o + 4 < cap; p++) {
        if (*p == '\\' || *p == quote) dst[(*o)++] = '\\';
        dst[(*o)++] = *p;
    }
    if (*o + 1 < cap) dst[(*o)++] = quote;
    dst[*o] = '\0';
}

/* Helper (not a Python module function): the text a C caller passes as
 * `details`, reproducing str(subprocess.CalledProcessError). */
char *gce_called_process_error_text(const char *const *argv, int argc,
                                    int returncode)
{
    size_t cap = 256;
    for (int i = 0; i < argc; i++) cap += (argv[i] ? strlen(argv[i]) : 0) * 2 + 8;
    char *tok = (char *)malloc(cap);
    char *out = (char *)malloc(cap + 96);
    if (!tok || !out) { free(tok); free(out); return NULL; }

    size_t o = 0;
    tok[0] = '\0';
    if (o + 1 < cap) tok[o++] = '[';
    for (int i = 0; i < argc; i++) {
        if (i) { tok[o++] = ','; tok[o++] = ' '; }
        append_repr_token(tok, cap, &o, argv[i] ? argv[i] : "");
    }
    if (o + 1 < cap) tok[o++] = ']';
    tok[o] = '\0';

    /* Python: "Command '%s' returned non-zero exit status %d." % (cmd, rc) */
    snprintf(out, cap + 96,
             "Command '%s' returned non-zero exit status %d.", tok, returncode);
    free(tok);
    return out;
}

/* PoP: gce_explain_service_failure @ hermes_cli/gateway_command_errors.py:explain_service_failure */
json_t *gce_explain_service_failure(int kind, const char *verb,
                                    const char *details)
{
    if (kind == GCE_FAULT_NO_SYSTEMCTL) {
        json_t *arr = json_array();
        if (!arr) return NULL;
        append_str_line(arr,
            "This system has no systemd, so Hermes cannot install a background service here.");
        append_str_line(arr,
            "Run the gateway directly with `hermes gateway run` (keep it alive with tmux or screen).");
        return arr;
    }

    if (kind == GCE_FAULT_SYSTEMCTL_FAILED) {
        const char *v = (verb && *verb) ? verb : "start";
        char line[512];
        json_t *arr = json_array();
        if (!arr) return NULL;

        snprintf(line, sizeof(line),
            "Could not %s the gateway service; systemd reported an error.", v);
        append_str_line(arr, line);

        snprintf(line, sizeof(line),
            "See why with `hermes gateway status --deep` or `%s`.",
            GCE_JOURNAL_HINT);
        append_str_line(arr, line);

        append_str_line(arr,
            "To reinstall the service run `hermes gateway install --force`.");

        snprintf(line, sizeof(line), "Details: %s", details ? details : "");
        append_str_line(arr, line);
        return arr;
    }

    /* Not a fault this module explains — the caller re-raises. */
    return NULL;
}
