/*
 * port_sizefmt.c — pure helpers from hermes_cli/sizefmt.py.
 */

#include "port_sizefmt.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: format_bytes @ hermes_cli/sizefmt.py:format_bytes */
char *format_bytes(const char *n)
{
    if (!n)
        return strdup("?");

    /* float(n): leading/trailing whitespace allowed, no internal junk. */
    const char *s = n;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s)
        return strdup("?");
    char *end = NULL;
    double size = strtod(s, &end);
    if (end == s)
        return strdup("?");
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end)
        return strdup("?");

    char buf[64];
    if (size < 1024) {
        snprintf(buf, sizeof(buf), "%d B", (int)size);
        return strdup(buf);
    }
    static const char *const units[] = { "KB", "MB", "GB" };
    for (int i = 0; i < 3; i++) {
        size /= 1024.0;
        if (size < 1024) {
            snprintf(buf, sizeof(buf), "%.1f %s", size, units[i]);
            return strdup(buf);
        }
    }
    snprintf(buf, sizeof(buf), "%.1f TB", size / 1024.0);
    return strdup(buf);
}
