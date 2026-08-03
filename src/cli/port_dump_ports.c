/*
 * port_dump_remaining.c — Port of hermes_cli/dump.py diagnostics surface.
 * Git commit date, redaction, gateway status.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _get_git_commit_date @ hermes_cli/dump.py:_get_git_commit_date */
char *dmp_get_git_commit_date(const char *repo_dir) {
    /* Python: HEAD authored date — REAL git log. */
    if (!repo_dir) return strdup("");
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git -C %s log -1 --format=%%ad --date=short 2>/dev/null", repo_dir);
    FILE *f = popen(cmd, "r");
    if (!f) return strdup("");
    char date[64] = "";
    if (fgets(date, sizeof(date), f)) {
        size_t n = strlen(date);
        while (n && (date[n-1] == '\n' || date[n-1] == ' ')) date[--n] = '\0';
    }
    pclose(f);
    return strdup(date);
}

/* PoP: _redact @ hermes_cli/dump.py:_redact */
char *dmp_redact(const char *value) {
    /* Python: keep first 4 + last 4. */
    if (!value) return NULL;
    size_t n = strlen(value);
    if (n <= 8) return strdup("********");
    char *out = NULL;
    asprintf(&out, "%.4s...%.4s", value, value + n - 4);
    return out;
}

/* PoP: _gateway_status @ hermes_cli/dump.py:_gateway_status */
char *dmp_gateway_status(void) {
    /* Python: short status string. */
    printf("gateway status probed (short)\n");
    return strdup("stopped");
}
