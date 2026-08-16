/*
 * ports/hermes_cli/_scan_venv_blockers.c — C11 port of hermes_cli/_scan_venv_blockers.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/cli/port_scan_venv_blockers.c ── */
/*
 * port_scan_venv_blockers.c — C port of hermes_cli/_scan_venv_blockers.py
 *
 * Standalone venv-process scan for JSON consumption (invoked by the
 * Desktop Electron app). Exits 0 for valid clear/blocked results, non-zero
 * on probe failure. Exactly one JSON document on stdout.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_redact.h"
#include "gateway_status.h"
#include "hermes_json.h"

/* Long CLI flags whose argument value must be redacted from the cmdline. */
static const char *SENSITIVE_LONG_FLAGS[] = {
    "--token", "--api-key", "--password", "--secret",
    "--authorization", "--access-key", "--private-key", "--session-key",
    NULL
};

/* PoP: scanvb_probe_fail_json @ hermes_cli/_scan_venv_blockers.py:_probe_fail_json */
char *scanvb_probe_fail_json(void) {
    /* Python: json.dumps({"ok": False, "blocked": False, "processes": []}) */
    return strdup("{\"ok\": false, \"blocked\": false, \"processes\": []}");
}

/* PoP: scanvb_emit_probe_fail @ hermes_cli/_scan_venv_blockers.py:_emit_probe_fail */
int scanvb_emit_probe_fail(const char *diagnostic) {
    /* Python: print one JSON to stdout, diagnostic to stderr, exit non-zero. */
    char *probe = scanvb_probe_fail_json();
    printf("%s\n", probe ? probe : "{}");
    free(probe);
    fprintf(stderr, "%s\n", diagnostic ? diagnostic : "probe failed");
    return 1;
}

/* PoP: scanvb_find_flag @ hermes_cli/_scan_venv_blockers.py:_find_flag */
long scanvb_find_flag(const char *text, const char *flag) {
    /* Python: index of *flag* when it starts the string or follows a space.
     * Returns -1 when not found. Avoids matching --token inside an
     * embedded token or path like /some--token-thing. */
    if (!text || !flag) return -1;
    size_t text_len = strlen(text);
    size_t flag_len = strlen(flag);
    for (size_t idx = 0; idx + flag_len <= text_len; idx++) {
        if (strncasecmp(text + idx, flag, flag_len) != 0) continue;
        if (idx == 0 || text[idx - 1] == ' ') return (long)idx;
    }
    return -1;
}

/* PoP: scanvb_redact_sensitive_cmdline @ hermes_cli/_scan_venv_blockers.py:_redact_sensitive_cmdline */
char *scanvb_redact_sensitive_cmdline(const char *cmdline) {
    /* Python: generic secret redaction then long-flag redaction.
     * If the generic redactor fails, return "<redacted>". */
    if (!cmdline) return strdup("");
    char *redacted = hermes_redact_force(cmdline);
    if (!redacted) return strdup("<redacted>");

    /* Conservative long-flag pass: preserve flag name, replace value and
     * everything after it with <redacted>. */
    size_t earliest = strlen(redacted);
    for (int i = 0; SENSITIVE_LONG_FLAGS[i]; i++) {
        char eq[64], sp[64];
        snprintf(eq, sizeof(eq), "%s=", SENSITIVE_LONG_FLAGS[i]);
        snprintf(sp, sizeof(sp), "%s ", SENSITIVE_LONG_FLAGS[i]);
        long idx = scanvb_find_flag(redacted, eq);
        if (idx != -1 && (size_t)(idx + strlen(eq)) < earliest)
            earliest = (size_t)(idx + strlen(eq));
        idx = scanvb_find_flag(redacted, sp);
        if (idx != -1 && (size_t)(idx + strlen(sp)) < earliest)
            earliest = (size_t)(idx + strlen(sp));
    }
    if (earliest < strlen(redacted)) {
        char *out = malloc(earliest + 11);
        if (out) {
            memcpy(out, redacted, earliest);
            strcpy(out + earliest, "<redacted>");
            free(redacted);
            return out;
        }
    }
    return redacted;
}

/* PoP: scanvb_is_pausable_gateway @ hermes_cli/_scan_venv_blockers.py:_is_pausable_gateway */
bool scanvb_is_pausable_gateway(const char *cmdline) {
    /* Python: True when cmdline is a gateway process the updater can pause.
     * Delegates to gateway.status.looks_like_gateway_command_line. */
    if (!cmdline || !cmdline[0]) return false;
    return gwstatus_looks_like_gateway_command_line(cmdline);
}

/* PoP: scanvb_main @ hermes_cli/_scan_venv_blockers.py:main */
int scanvb_main(const char *processes_json, const char *exempted_count) {
    /* Python entry point: prints one JSON doc, exits 0 for valid scan.
     * Arg = "processes_json\texempted_count" where processes_json is the
     * JSON array of {pid, name, cmdline} blocker records (already filtered
     * and redacted by the caller) and exempted_count the number of
     * pausable gateway processes found. */
    if (!processes_json || !*processes_json) {
        printf("{\"ok\": true, \"blocked\": false, \"processes\": [], \"pausable_gateways\": 0}\n");
        return 0;
    }
    const char *tab = strchr(processes_json, '\t');
    const char *procs = processes_json;
    long exempted = 0;
    if (tab) {
        *((char *)tab) = '\0';
        exempted = atol(tab + 1);
    }
    json_t *j = json_parse(procs, NULL);
    bool blocked = j && j->type == JSON_ARRAY && j->c.count > 0;
    if (j) json_free(j);
    printf("{\"ok\": true, \"blocked\": %s, \"processes\": %s, \"pausable_gateways\": %ld}\n",
           blocked ? "true" : "false", procs, exempted);
    return 0;
}

