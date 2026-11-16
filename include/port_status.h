/* port_status.h -- pure helpers ported from gateway/status.py. */
#ifndef PORT_STATUS_H
#define PORT_STATUS_H

#include <stddef.h>

/* hermes_home_assignments: every token-bounded HERMES_HOME=<value> in
 * `command`, quotes stripped and trailing '/' removed. Writes up to `maxn`
 * malloc'd strings into `out`; caller frees. Returns the count. */
int st_hermes_home_assignments(const char *command, char **out, int maxn);

/* command_line_names_hermes_home: literal, token-bounded match of
 * HERMES_HOME=<home_lc> (both lowercased, '/'-separated, no trailing '/'). */
int st_command_line_names_home(const char *command_lc, const char *home_lc);

/* _profile_name_for_home: the profile id for <root>/profiles/<name>, else NULL.
 * Caller frees. */
char *st_profile_name_for_home(const char *profile_home);

#endif /* PORT_STATUS_H */
