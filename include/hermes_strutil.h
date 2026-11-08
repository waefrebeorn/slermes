/*
 * hermes_strutil.h — shared small string helpers.
 *
 * str_strip_dup (trim + malloc'd copy) was copy-pasted byte-identical into
 * agent/port_error_surface.c (as strip_dup) and
 * gateway/port_hosted_room_execution_policy.c (as str_strip_dup). Single
 * source of truth. Minimal includes (<string.h>, <stdlib.h>, <ctype.h>);
 * callers that already pull them pay nothing.
 */
#ifndef HERMES_STRUTIL_H
#define HERMES_STRUTIL_H

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Copy s trimmed of leading/trailing whitespace (isspace). Caller free()s.
 * Returns NULL only on malloc failure (NULL input -> NULL). */
static inline char *str_strip_dup(const char *s) {
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

#ifdef __cplusplus
}
#endif

#endif /* HERMES_STRUTIL_H */
