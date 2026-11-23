#ifndef NOC_W15_UPDATECMD_05D181_H
#define NOC_W15_UPDATECMD_05D181_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_cmd_stash.py (gate w15_updatecmd_05d181). */

/* _intent_to_add_paths(porcelain_z) -> string[]. z is NUL-separated, zlen its byte length. */
json_t *w15_updatecmd_05d181_intent_to_add_paths(const char *z, size_t zlen);
/* _porcelain_records(porcelain_z) -> string[]. */
json_t *w15_updatecmd_05d181_porcelain_records(const char *z, size_t zlen);
/* _untracked_collides(entry, files[], dirs[]) -> bool. */
json_t *w15_updatecmd_05d181_untracked_collides(const char *entry,
                                               const json_t *files,
                                               const json_t *dirs);
/* _stash_apply_failed_only_on_existing_untracked(stderr) -> bool. */
json_t *w15_updatecmd_05d181_stash_apply_failed_only_on_existing_untracked(const char *stderr_text);

#endif /* NOC_W15_UPDATECMD_05D181_H */
