#ifndef NOC_W18_UPDATELAUN_89B41E_H
#define NOC_W18_UPDATELAUN_89B41E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C port of hermes_cli/update_launch_files.py pure decisions.
 * PoP: w18_updatelaun_89b41e_owned @ hermes_cli/update_launch_files.py:_owned_by_this_checkout
 * PoP: w18_updatelaun_89b41e_refreshers @ hermes_cli/update_launch_files.py:_refreshers
 */

/* FOLLOWUP_STEP = "launch_files" mirrored from the Python module. */
extern const char *w18_updatelaun_89b41e_FOLLOWUP_STEP;

/* -> 1 when this checkout owns the definition, else 0. `foreign` is the
 * definition_foreign_checkout() result injected as a seam (NULL = None). */
int w18_updatelaun_89b41e_owned(const char *kind, const char *foreign);

/* -> JSON array of refresher kind names for a platform/systemd decision. */
json_t *w18_updatelaun_89b41e_refreshers(const char *platform, int systemd_support);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_UPDATELAUN_89B41E_H */
