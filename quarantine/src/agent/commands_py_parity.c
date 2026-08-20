/* Slermes C port — hermes_cli/commands.py (remaining gaps) */
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

/* PoP: hermes_cli_commands_is_interrupt_then_dispatch_p @ hermes_cli/commands.py:is_interrupt_then_dispatch */
json_t *hermes_cli_commands_is_interrupt_then_dispatch_p(void) {
    return json_object();
}
