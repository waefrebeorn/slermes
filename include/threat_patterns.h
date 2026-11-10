/*
 * include/threat_patterns.h — declarations for the threat-pattern table.
 *
 * The implementation lives in lib/libthreatpatterns/threat_patterns.c. A
 * second copy briefly existed in src/agent/threat_patterns.c and broke the
 * link with multiple-definition errors; this header declares the real API
 * rather than redeclaring the module.
 */

#ifndef SLERMES_THREAT_PATTERNS_H
#define SLERMES_THREAT_PATTERNS_H

#include <stdbool.h>
#include <stddef.h>

#define THREAT_SCOPE_ALL      0
#define THREAT_SCOPE_CONTEXT  1
#define THREAT_SCOPE_STRICT   2

typedef struct {
    char pattern_id[64];
    int  scope;
    int  match_start;
    int  match_len;
} threat_match_t;

int  threat_patterns_init(void);
int  threat_patterns_count(void);
void threat_patterns_cleanup(void);
bool threat_patterns_check(const char *content, int scope_mask, threat_match_t *match_out);
bool threat_patterns_check_all(const char *content, threat_match_t *match_out);

char *cli_tools_threat_patterns_scan_for_threats(const char *content, const char *scope);
char *cli_tools_threat_patterns_first_threat_message(const char *content, const char *scope);

#endif /* SLERMES_THREAT_PATTERNS_H */
