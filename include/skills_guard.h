/*
 * skills_guard.h — minimal declaration surface for the deterministic
 * skill-trust helpers ported from tools/skills_guard.py in
 * src/cli/port_tools_skills_guard.c. Opaque / minimal: no god-header.
 */

#ifndef SLERMES_SKILLS_GUARD_H
#define SLERMES_SKILLS_GUARD_H

#include <stddef.h>

/* Port of tools/skills_guard.py:_resolve_trust_level (source -> trust level).
 * The repo argument is accepted for API compatibility but ignored (Python
 * derives trust from source alone). */
const char *cli_tools_skills_guard__resolve_trust_level(const char *source, const char *repo);

/* Port of tools/skills_guard.py:_determine_verdict. Severity levels:
 * 3=critical, 2=high, 1=medium, 0=low. critical->dangerous, high->caution,
 * else safe (trust-independent, matching the Python findings logic). */
const char *cli_tools_skills_guard__determine_verdict(int max_severity, int total_findings);

#endif /* SLERMES_SKILLS_GUARD_H */
