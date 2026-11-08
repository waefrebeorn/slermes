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

/* True when s ends with suf. (Byte-identical copy folded from
 * port_cli_profiles.c / port_provider_registry.c.) */
static inline bool str_ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* One of: true when value equals any entry of set[0..n). Pure membership -
 * folded byte-identical from port_error_surface.c / port_side_question_request.c. */
static inline int str_member_of(const char *value, const char *const *set, size_t n) {
    if (!value) return 0;
    for (size_t i = 0; i < n; i++)
        if (strcmp(value, set[i]) == 0) return 1;
    return 0;
}

/* Lowercase s in place. (Folded byte-identical from three port_*_helpers.c.) */
static inline void str_lower_inplace(char *s) {
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

#ifdef __cplusplus
}
#endif

#endif /* HERMES_STRUTIL_H */
