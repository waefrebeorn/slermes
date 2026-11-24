#ifndef NOC_W17_WORKTREERE_5152ED_H
#define NOC_W17_WORKTREERE_5152ED_H

#include "hermes_json.h"

/* C home for upstream Hermes hermes_cli/worktree_retention.py.
 * Pure reason-formatting + module constants of the worktree removal guards. */

/* _named(paths, more=False): first five path names joined by ", " with the
 * live module's overflow suffix. */
json_t *w17_worktreere_5152ed_named(const json_t *paths, bool more);

/* Module-level removal-action constants and thresholds. */
json_t *w17_worktreere_5152ed_constants(void);

#endif /* NOC_W17_WORKTREERE_5152ED_H */
