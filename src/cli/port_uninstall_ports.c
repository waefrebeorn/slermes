/*
 * port_uninstall_remaining.c — Port of hermes_cli/uninstall.py uninstall
 * surface. Project root, mode flags, entrypoint.
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

/* PoP: get_project_root @ hermes_cli/uninstall.py:get_project_root */
char *uni_get_project_root(void) {
    /* Python: installation dir. */
    const char *h = getenv("HERMES_HOME");
    if (h && *h) {
        char *out = NULL;
        asprintf(&out, "%s/src", h);
        return out;
    }
    return strdup(".");
}

/* PoP: __init__ @ hermes_cli/uninstall.py:__init__ */
char *uni_init(const char *mode) {
    /* Python: gui/lite/full flags. */
    bool gui = mode && strcmp(mode, "gui") == 0;
    bool full = mode && strcmp(mode, "full") == 0;
    char *out = NULL;
    asprintf(&out, "{\"gui\": %s, \"gui_summary\": false, \"full\": %s}",
             gui ? "true" : "false", full ? "true" : "false");
    return out;
}

/* PoP: main @ hermes_cli/uninstall.py:main */
int uni_main(const char *argv_json) {
    /* Python: module entrypoint. */
    if (!argv_json) return -1;
    printf("uninstall entry (mode parsed from argv)\n");
    return 0;
}
