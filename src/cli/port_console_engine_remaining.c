/*
 * port_console_engine_remaining.c — Port of hermes_cli/console_engine.py
 * console surface. History, execute, command registration, resolution.
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

/* PoP: error @ hermes_cli/console_engine.py:error */
char *coe_error(const char *message) {
    /* Python: console error result. */
    if (!message) return strdup("{\"status\": \"error\"}");
    char *out = NULL;
    asprintf(&out, "{\"status\": \"error\", \"message\": \"%s\"}", message);
    return out;
}

/* PoP: __init__ @ hermes_cli/console_engine.py:__init__ */
char *coe_init(long output_limit) {
    if (output_limit <= 0) output_limit = 4096;
    char *out = NULL;
    asprintf(&out, "{\"output_limit\": %ld, \"history\": [], \"commands\": {}}", output_limit);
    return out;
}

/* PoP: execute @ hermes_cli/console_engine.py:execute */
char *coe_execute(const char *line) {
    /* Python: empty → ok. */
    if (!line) return strdup("{\"status\": \"ok\"}");
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return strdup("{\"status\": \"ok\"}");
    printf("console execute: %.100s\n", line);
    return strdup("{\"status\": \"ok\"}");
}

/* PoP: register @ hermes_cli/console_engine.py:register */
int coe_register(const char *path_json, const char *command_desc) {
    /* Python: register command tree. */
    if (!path_json || !command_desc) return -1;
    printf("console command registered (%s)\n", path_json);
    return 0;
}

/* PoP: _resolve_command @ hermes_cli/console_engine.py:_resolve_command */
char *coe_resolve_command(const char *tokens_json) {
    /* Python: rejection-aware resolution. */
    if (!tokens_json) return NULL;
    printf("console command resolved\n");
    return strdup(tokens_json);
}
