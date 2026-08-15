/* Slermes C port — utils.py (remaining gaps) */
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

/* PoP: utils_atomic_roundtrip_yaml_save @ utils.py:atomic_roundtrip_yaml_save */
json_t *utils_atomic_roundtrip_yaml_save_p(void) {
    return json_object();
}
