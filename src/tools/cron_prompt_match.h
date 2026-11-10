/*
 * cron_prompt_match.h - prompt-sanitiser matching / utf8 helpers.
 *
 * Module-internal header (cron_prompt_sanitize.c <-> cron_prompt_match.c).
 */
#ifndef CRON_PROMPT_MATCH_H
#define CRON_PROMPT_MATCH_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

bool contains_ci(const char *haystack, const char *needle);
bool contains_secret_var_token(const char *haystack);
int decode_utf8(const char *s, size_t len, size_t i, unsigned int *out_cp);
unsigned int decode_utf8_before(const char *s, size_t len, size_t end, size_t *start_out);
int encode_utf8(unsigned int cp, char *out4, size_t cap);
bool match_deception_hide(const char *s);
bool match_disregard_rules(const char *s);
bool match_injection_directive(const char *s);
bool match_sys_prompt_override(const char *s);

#endif /* CRON_PROMPT_MATCH_H */
