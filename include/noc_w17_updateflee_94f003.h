#ifndef NOC_W17_UPDATEFLEE_94F003_H
#define NOC_W17_UPDATEFLEE_94F003_H

#include "hermes_json.h"

/* C port of hermes_cli/update_fleet_scope.py (gate w17_updateflee_94f003).
 * Returned char* values are malloc'd, caller-owned; NULL = Python None. */

/* describe_skipped_runtime(kind, name, home, checkout=None) -> notice line.
 * home and checkout may be NULL to model Python None. */
char *w17_updateflee_94f003_describe_skipped_runtime(const char *kind, const char *name,
                                                     const char *home, const char *checkout);

/* home_in_update_scope(home, scope) with an explicitly injected scope
 * (JSON array of path strings). Mirrors Path.resolve() normalisation on
 * Windows: '/' -> '\', ASCII-lowercase, '.'/'..' collapsed, no trailing sep. */
bool w17_updateflee_94f003_home_in_update_scope(const char *home, json_t *scope);

#endif
