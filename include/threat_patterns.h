/*
 * threat_patterns.h — prompt-injection / exfiltration pattern detection.
 *
 * Port of tools/threat_patterns.py. The Python module compiles regexes; this
 * C port uses case-insensitive substring matchers with the same pattern ids
 * and the same three scopes, which is what call sites actually need.
 *
 * Scopes are a mask: pass (1 << THREAT_SCOPE_ALL) etc., or OR several.
 *   ALL      always-on checks (prompt injection, instruction override,
 *            curl/wget exfiltration, translate-and-execute)
 *   CONTEXT  only suspicious in untrusted context (role hijack, identity
 *            override, C2 registration / task pull / network connect)
 *   STRICT   high false-positive cost; secrets and send-to-URL exfiltration
 */

#ifndef SLERMES_THREAT_PATTERNS_H
#define SLERMES_THREAT_PATTERNS_H

#include <stddef.h>
#include <stdbool.h>

#define THREAT_SCOPE_ALL      0
#define THREAT_SCOPE_CONTEXT  1
#define THREAT_SCOPE_STRICT   2

typedef struct {
    char pattern_id[64];
    int  scope;
    int  match_start;   /* byte offset of the match, -1 if unknown */
    int  match_len;
} threat_match_t;

/* Compile the pattern table. Idempotent: a second call returns the same count.
 * Returns the number of patterns compiled. */
int threat_patterns_init(void);

/* Number of patterns currently compiled (0 before init / after cleanup). */
int threat_patterns_count(void);

/* Release the table. Safe to call when already clean. */
void threat_patterns_cleanup(void);

/* First pattern matching `text` whose scope is in `scope_mask`.
 * Returns true and fills *out on a match; false otherwise. `out` may be NULL. */
bool threat_patterns_check(const char *text, unsigned scope_mask,
                           threat_match_t *out);

/* Convenience: check against all three scopes. */
bool threat_patterns_check_all(const char *text, threat_match_t *out);

/* Port of tools/threat_patterns.py:scan_for_threats. Returns a malloc'd JSON
 * array of matched pattern-id strings (and "invisible_unicode_U+XXXX" hits).
 * Caller frees. */
char *cli_tools_threat_patterns_scan_for_threats(const char *content, const char *scope);

/* Port of tools/threat_patterns.py:first_threat_message. Returns a malloc'd
 * human-readable message, or NULL if safe. Caller frees. */
char *cli_tools_threat_patterns_first_threat_message(const char *content, const char *scope);

#endif /* SLERMES_THREAT_PATTERNS_H */
