#ifndef NOC_W15_UPDATECMD_78C8C3_H
#define NOC_W15_UPDATECMD_78C8C3_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_cmd_check.py (gate w15_updatecmd_78c8c3). */

/* tracking_refspec(remote, branch) -> string. */
json_t *w15_updatecmd_78c8c3_tracking_refspec(const char *remote, const char *branch);
/* is_shallow_repository: seam taking the git rev-parse stdout. */
json_t *w15_updatecmd_78c8c3_is_shallow_repository(const char *git_stdout);
/* compare_ref_exists: seam taking the git rev-parse return code. */
json_t *w15_updatecmd_78c8c3_compare_ref_exists(int returncode);

#endif /* NOC_W15_UPDATECMD_78C8C3_H */
