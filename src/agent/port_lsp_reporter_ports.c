/*
 * port_lsp_reporter_remaining.c — Port of agent/lsp/reporter.py diagnostics
 * surface. Field sanitization, one-line formatting, file blocks,
 * truncation.
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

/* PoP: _sanitize_field @ agent/lsp/reporter.py:_sanitize_field */
char *lsr_sanitize_field(const char *value) {
    /* Python: strip control chars for tool-result blocks. */
    if (!value) return strdup("");
    size_t n = strlen(value);
    char *out = malloc(n + 1);
    if (!out) return strdup("");
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '\n') { out[o++] = ' '; }
        else if (c < 32 && c != '\t') { /* drop */ }
        else out[o++] = (char)c;
    }
    out[o] = '\0';
    return out;
}

/* PoP: format_diagnostic @ agent/lsp/reporter.py:format_diagnostic */
char *lsr_format_diagnostic(const char *message, const char *code, const char *severity, long line, long col) {
    /* Python: one-line diagnostic. */
    if (!message) return strdup("");
    char *san = lsr_sanitize_field(message);
    char *out = NULL;
    if (severity)
        asprintf(&out, "%s %ld:%ld %s%s%s", severity, line, col, san,
                 code && *code ? " (" : "", code && *code ? code : "");
    else
        asprintf(&out, "%ld:%ld %s", line, col, san);
    free(san);
    return out;
}

/* PoP: report_for_file @ agent/lsp/reporter.py:report_for_file */
char *lsr_report_for_file(const char *file_path, const char *diagnostics_json) {
    /* Python: <diagnostics file=...> block. */
    if (!file_path) return strdup("");
    if (!diagnostics_json || strcmp(diagnostics_json, "[]") == 0) return strdup("");
    char *out = NULL;
    asprintf(&out, "<diagnostics file=\"%s\">\n%s\n</diagnostics>", file_path, diagnostics_json);
    return out;
}

/* PoP: truncate @ agent/lsp/reporter.py:truncate */
char *lsr_truncate(const char *summary, long limit) {
    /* Python: hard-cap formatted summary. */
    if (!summary) return strdup("");
    size_t n = strlen(summary);
    if ((long)n <= limit) return strdup(summary);
    char *out = malloc((size_t)limit + 8);
    if (!out) return strdup(summary);
    memcpy(out, summary, (size_t)limit);
    strcpy(out + limit, "…");
    return out;
}
