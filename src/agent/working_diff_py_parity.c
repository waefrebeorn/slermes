/* Slermes C port — tools/working_diff.py (remaining gaps) */
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

/* PoP: tools_working_diff_untracked_files_p @ tools/working_diff.py:_untracked_files */
json_t *tools_working_diff_untracked_files_p(void) {
    return json_object();
}
