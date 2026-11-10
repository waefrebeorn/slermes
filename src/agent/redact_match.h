/*
 * redact_match.h - redaction pattern / value matchers.
 *
 * Module-internal header (redact.c <-> redact_match.c).
 */
#ifndef REDACT_MATCH_H
#define REDACT_MATCH_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

const char * find_free_text_key(const char *text, const char *prefix, size_t prefix_len, size_t *val_len, size_t min_len);
const char * find_key_value(const char *text, const char *key, size_t key_len, size_t *val_len);
bool looks_like_jwt(const char *start, size_t len);
size_t redact_value(char *text, const char *val_start, size_t val_len, size_t max_show);

#endif /* REDACT_MATCH_H */
