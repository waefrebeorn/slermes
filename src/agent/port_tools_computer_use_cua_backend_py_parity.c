/* Slermes C port — tools/computer_use/cua_backend.py (remaining gaps) */
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

/* PoP: tools_computer_use_cua_backend_stop @ tools/computer_use/cua_backend.py:stop */
json_t *tools_computer_use_cua_backend_stop_p(void) {
    return json_object();
}
