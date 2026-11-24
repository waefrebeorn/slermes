#ifndef NOC_W17_UPDATEWIND_E677B5_H
#define NOC_W17_UPDATEWIND_E677B5_H

#include "hermes_json.h"

/* C port of hermes_cli/update_windows_acl.py (gate w17_updatewind_e677b5).
 *
 * repair_windows_home_acl() is a side-effecting Windows ACL walker; its pure
 * decision core is exposed here as a seam. `elevated` uses -1 for Python None,
 * 0 for False and 1 for True. `probe_elevated` is the injected result of
 * process_is_elevated() used when elevated == -1. granted/skipped are JSON
 * arrays of strings; failed is a JSON array of [path, reason] string pairs.
 *
 * Return value: malloc'd RuntimeError message string when the live function
 * would raise, or NULL when it returns None (success / no-op). Caller frees.
 */
char *w17_updatewind_e677b5_repair(int elevated, bool probe_elevated, const char *home,
                                   json_t *granted, json_t *skipped, json_t *failed);

#endif
