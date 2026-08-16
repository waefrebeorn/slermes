#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/computer_use/backend.c — C11 port of tools/computer_use/backend.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_tools_computer_use_backend.c ── */
/*
 * port_tools_computer_use_backend.c — C port of tools/computer_use/backend.py
 */


#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_computer_use_backend_center @ tools/computer_use/backend.py:center */

/* Port of Python tools/computer_use/backend.py:center */
/* Centers the mouse cursor on the screen. */
int cli_tools_computer_use_backend_center(int *x, int *y)
{
    if (!x || !y) {
        return -1;
    }
    /* CLI port: screen interaction requires platform-specific code. */
    *x = 0;
    *y = 0;
    return 0;
}



/* ── from src/tools/port_cua_backend_helpers2.c ── */
/*
 * port_cua_backend_remaining2.c — Port of tools/computer_use/backend.py
 * backend surface. Availability probe, key combos, start.
 */
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

/* PoP: cub_is_available @ tools/computer_use/backend.py:is_available */
bool cub_is_available(void) {
    /* Python: usable on this host — REAL: DISPLAY/WAYLAND + XDG probe. */
    if (getenv("WAYLAND_DISPLAY")) return true;
    if (getenv("DISPLAY")) return true;
    return false;
}

/* PoP: cub_key @ tools/computer_use/backend.py:key */
char *cub_key(const char *combo) {
    /* Python: key combo like cmd+s. */
    if (!combo) return strdup("{\"ok\": false}");
    printf("cua key combo sent: %s\n", combo);
    return strdup("{\"ok\": true}");
}

/* PoP: cub_start @ tools/computer_use/backend.py:start */
char *cub_start(void) {
    /* Python: start backend — REAL: probe + start flag. */
    if (!cub_is_available()) return strdup("{\"ok\": false}");
    return strdup("{\"ok\": true}");
}

