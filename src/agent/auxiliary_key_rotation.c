/*
 * auxiliary_key_rotation.c — C port of upstream Hermes agent/auxiliary_key_rotation.py.
 * PoP: auxkeyrot_rotate_runtime_main_api_key @ agent/auxiliary_key_rotation.py:rotate_runtime_main_api_key
 *
 * Faithful port of the pure in-place-swap decision. The ContextVar holding the
 * published runtime is a seam (runtime_is_dict + current api_key supplied by
 * the caller); the comparison and _normalize_api_key assignment are real.
 */
#include "noc_auxkeyrot.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* _normalize_api_key: non-str -> "" ; str -> str.strip() (ASCII whitespace). */
static char *normalize_api_key(const char *new_key, int new_is_null) {
    if (new_is_null || !new_key) return dup_str("");
    size_t n = strlen(new_key), a = 0, b = n;
    while (a < b && isspace((unsigned char)new_key[a])) a++;
    while (b > a && isspace((unsigned char)new_key[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (p) {
        memcpy(p, new_key + a, b - a);
        p[b - a] = '\0';
    }
    return p;
}

/* Python == between two api_key values that are each str-or-None. */
static int keys_equal(const char *a, int a_null, const char *b, int b_null) {
    if (a_null || b_null) return a_null && b_null;
    return a && b && strcmp(a, b) == 0;
}

char *auxkeyrot_rotate_runtime_main_api_key(const char *old, int old_is_null,
        const char *new_key, int new_is_null,
        const char *current, int current_is_null,
        int runtime_is_dict, int *rotated) {
    if (rotated) *rotated = 0;
    /* No published dict => there is no api_key to report (Python None). */
    if (!runtime_is_dict) return NULL;
    int do_rotate = keys_equal(old, old_is_null, current, current_is_null);
    if (!do_rotate) {
        return current_is_null ? NULL : dup_str(current);
    }
    if (rotated) *rotated = 1;
    return normalize_api_key(new_key, new_is_null);
}
