/*
 * threat_patterns.h — minimal declaration surface for the deterministic
 * threat-scanning helpers ported from tools/threat_patterns.py in
 * src/cli/port_tools_threat_patterns.c. Opaque / minimal: no god-header.
 */

#ifndef SLERMES_THREAT_PATTERNS_H
#define SLERMES_THREAT_PATTERNS_H

#include <stddef.h>

/* Port of tools/threat_patterns.py:scan_for_threats. Returns a malloc'd JSON
 * array of matched pattern-id strings (and "invisible_unicode_U+XXXX" hits).
 * Caller frees. */
char *cli_tools_threat_patterns_scan_for_threats(const char *content, const char *scope);

/* Port of tools/threat_patterns.py:first_threat_message. Returns a malloc'd
 * human-readable message, or NULL if safe. Caller frees. */
char *cli_tools_threat_patterns_first_threat_message(const char *content, const char *scope);

#endif /* SLERMES_THREAT_PATTERNS_H */
