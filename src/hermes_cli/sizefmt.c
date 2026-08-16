/*
 * port_hermes_cli_sizefmt.c — C11 port of hermes_cli/sizefmt.py
 *
 * Small shared byte-formatting helper. Accepts a numeric string (anything
 * float() would accept); returns "?" for NULL/unparseable input so display
 * call sites never raise.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "port_hermes_cli_sizefmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: szf_format_bytes @ hermes_cli/sizefmt.py:format_bytes */
char *szf_format_bytes(const char *input) {
    if (!input) return strdup("?");
    char *end = NULL;
    double size = strtod(input, &end);
    if (end == input || (end && *end != '\0')) {
        /* not a clean number -> "?" (mirrors float() raising) */
        return strdup("?");
    }
    if (size < 1024.0) {
        char buf[64];
        snprintf(buf, sizeof buf, "%d B", (int)size);
        return strdup(buf);
    }
    static const char *units[] = {"KB", "MB", "GB"};
    for (int i = 0; i < 3; i++) {
        size /= 1024.0;
        if (size < 1024.0) {
            char buf[64];
            snprintf(buf, sizeof buf, "%.1f %s", size, units[i]);
            return strdup(buf);
        }
    }
    char buf[64];
    snprintf(buf, sizeof buf, "%.1f TB", size / 1024.0);
    return strdup(buf);
}
