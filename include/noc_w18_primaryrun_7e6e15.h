#ifndef NOC_W18_PRIMARYRUN_7E6E15_H
#define NOC_W18_PRIMARYRUN_7E6E15_H

#include "hermes_json.h"

/* C home for upstream Hermes agent/primary_runtime_snapshot.py.
 * runtime_entry_owner() is a controlled seam: its result is supplied as `owner`.
 * SNAPSHOT_OWNER_KEY == "custom_entry". */

/* -> the _primary_runtime dict written by _snapshot_primary_runtime */
json_t *w18_primaryrun_7e6e15_take(const json_t *agent, const json_t *cc, const json_t *owner);
/* -> the dict returned by _build_primary_runtime_snapshot */
json_t *w18_primaryrun_7e6e15_build(const json_t *agent, const json_t *cc, const json_t *owner,
                                    const char *api_mode);

#endif /* NOC_W18_PRIMARYRUN_7E6E15_H */
