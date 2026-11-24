#ifndef NOC_W17_UPDATECMD_736DA6_H
#define NOC_W17_UPDATECMD_736DA6_H

#include "hermes_json.h"

/* C port of selected pure helpers in hermes_cli/update_cmd_windows.py
 * (gate w17_updatecmd_736da6). Returned char* values are malloc'd and
 * caller-owned; NULL = Python None. */

/* _planned_stop_marker_path(profile_path) -> str: Path join on Windows uses
 * backslashes and preserves case. */
char *w17_updatecmd_736da6_planned_stop_marker_path(const char *profile_path);

/* _lower_dir_prefix(path) -> str: Path.resolve(), lowercase, one trailing
 * os.sep (a backslash on Windows). */
char *w17_updatecmd_736da6_lower_dir_prefix(const char *path);

/* _pending_relaunch_pids(profiles, unmapped, alive) -> sorted unique ints.
 * profiles: JSON object name->pid(decimal string); unmapped: JSON array of
 * {pid: decimal string, argv: array}; alive: JSON array of pid decimal
 * strings (the injected pid_exists predicate). Returns a JSON_ARRAY. */
json_t *w17_updatecmd_736da6_pending_relaunch_pids(json_t *profiles, json_t *unmapped,
                                                   json_t *alive);

#endif
