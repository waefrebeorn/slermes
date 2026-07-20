/*
 * fuzzy_match_helpers.h — minimal declaration surface for the deterministic
 * string helpers ported from tools/fuzzy_match.py in
 * src/cli/port_fuzzy_match_helpers.c. Opaque / minimal: no god-header.
 */

#ifndef SLERMES_FUZZY_MATCH_HELPERS_H
#define SLERMES_FUZZY_MATCH_HELPERS_H

#include <stddef.h>

/* Port of tools/fuzzy_match.py:_unicode_normalize (ASCII-fold the UNICODE_MAP). */
char *fuzzy_match_unicode_normalize(const char *text);

/* Port of tools/fuzzy_match.py:_leading_whitespace (count of leading WS). */
size_t fuzzy_match_leading_whitespace(const char *line);

/* Port of tools/fuzzy_match.py:_first_meaningful_line (first non-blank line,
 * or NULL). Caller frees. */
char *fuzzy_match_first_meaningful_line(const char *text);

#endif /* SLERMES_FUZZY_MATCH_HELPERS_H */
