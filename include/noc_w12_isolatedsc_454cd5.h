#ifndef NOC_W12_ISOLATEDSC_454CD5_H
#define NOC_W12_ISOLATEDSC_454CD5_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/isolated_scope.py (gate w12_isolatedsc_454cd5). */

/* scope_to_launch_profile(items, name_of): keep items whose name equals the
 * launch scope; a NULL scope lists everything. `key` is the injected name_of
 * seam for dict items (NULL/empty => identity, i.e. string items). */
json_t *w12_isolatedsc_454cd5_filter(const json_t *scope, const json_t *items,
                                     const char *key);

/* record_launch_scope(isolated, desktop_owned) with the three
 * hermes_constants lookups injected as explicit seams (home/default_root are
 * already-resolved paths, profile = profile_name_for_home(home)). Returns the
 * pinned profile string, or null when the process stays unscoped. */
json_t *w12_isolatedsc_454cd5_record(int isolated, int desktop_owned,
                                     const char *home, const char *default_root,
                                     const char *profile);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_ISOLATEDSC_454CD5_H */
