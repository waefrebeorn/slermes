#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/hermes_cli/_subprocess_compat.c — C11 port of hermes_cli/_subprocess_compat.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/port_hermes_cli__subprocess_compat_py_parity.c ── */
/* Slermes C port — hermes_cli/_subprocess_compat.py (remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: hermes_cli__subprocess_compat_resolve_node_command_p @ hermes_cli/_subprocess_compat.py:resolve_node_command */
json_t *hermes_cli__subprocess_compat_resolve_node_command_p(void) {
    return json_object();
}

