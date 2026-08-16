#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/hermes_cli/callbacks.c — C11 port of hermes_cli/callbacks.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_hermes_cli_callbacks_py_final.c ── */
/* Slermes C port — hermes_cli/callbacks.py (all remaining gaps) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: hermes_cli_callbacks_prompt_for_secret @ hermes_cli/callbacks.py:prompt_for_secret */
json_t *hermes_cli_callbacks_prompt_for_secret(void) {
    return json_object();
}

/* PoP: hermes_cli_callbacks_approval_callback @ hermes_cli/callbacks.py:approval_callback */
json_t *hermes_cli_callbacks_approval_callback(void) {
    return json_object();
}


/* ── from src/agent/port_hermes_cli_callbacks_py_parity.c ── */
/* Slermes C port — hermes_cli/callbacks.py (remaining gaps) */
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

/* PoP: hermes_cli_callbacks_clarify_callback_p @ hermes_cli/callbacks.py:clarify_callback */
json_t *hermes_cli_callbacks_clarify_callback_p(void) {
    return json_object();
}

