#ifndef SLERMES_PORT_VERSION_INFO_H
#define SLERMES_PORT_VERSION_INFO_H

#include <stddef.h>

/* hermes_cli/version_info.py — the canonical runtime identity helpers.
 *
 * The git/subprocess/fs half of the module stays in Python; what the C tree
 * needs is the pure derivation layer every version surface consumes:
 * display-version formatting, non-negative parsing, and the outbound
 * User-Agent. Used by 61 reference modules.
 *
 * `has_distance` distinguishes a measured distance of 0 from "no tag found"
 * (None) — the two take different branches in _derived_version. */

/* PoP: _derived_version @ hermes_cli/version_info.py:_derived_version */
void version_derived(const char *base_version, long distance, int has_distance,
                     int dirty, const char *short_commit, char *out,
                     size_t outsz);

/* PoP: _parse_nonnegative @ hermes_cli/version_info.py:_parse_nonnegative */
long version_parse_nonnegative(const char *value, int *ok);

/* PoP: VersionInfo.display_version @ hermes_cli/version_info.py:display_version */
void version_display(const char *base_version, const char *derived_version,
                     long distance, int has_distance, char *out, size_t outsz);

/* PoP: hermes_user_agent @ hermes_cli/version_info.py:hermes_user_agent */
void version_user_agent(const char *product, const char *base_version,
                        char *out, size_t outsz);

#endif /* SLERMES_PORT_VERSION_INFO_H */
