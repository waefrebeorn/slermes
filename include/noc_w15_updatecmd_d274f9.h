#ifndef NOC_W15_UPDATECMD_D274F9_H
#define NOC_W15_UPDATECMD_D274F9_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_cmd_maint.py (gate w15_updatecmd_d274f9). */

/* _sqlite_partial_completion_lines(sqlite_version, is_windows) -> string[2]. */
json_t *w15_updatecmd_d274f9_sqlite_partial_completion_lines(const char *sqlite_version, int is_windows);
/* _update_complete_message seam: pre_version/post_version (NULL = None) -> string. */
json_t *w15_updatecmd_d274f9_update_complete_message_ex(const char *pre_version, const char *post_version);
/* _profile_skill_sync_status(r|null) -> string. */
json_t *w15_updatecmd_d274f9_profile_skill_sync_status(const json_t *r);

#endif /* NOC_W15_UPDATECMD_D274F9_H */
