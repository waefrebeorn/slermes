/*
 * worktree_sweep.c — C home for upstream Hermes hermes_cli/worktree_sweep.py.
 *
 * PoP: w12_worktreesw_d88fbf_inside      @ hermes_cli/worktree_sweep.py:_inside
 * PoP: w12_worktreesw_d88fbf_classic_mode @ hermes_cli/worktree_sweep.py:classic_mode
 *
 * The two pure, dependency-free decisions of the sweep module.  _inside uses
 * os.sep ('\\' on the Windows host the oracle runs on); classic_mode is the
 * exact `not settings["archive_dirty"] and not settings["max_total_gb"]`.
 */
#include "noc_w12_worktreesw_d88fbf.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

const char *w12_worktreesw_d88fbf_inside(const char *target, const struct json_t *roots) {
    if (!target || !roots || roots->type != JSON_ARRAY)
        return NULL;
    for (size_t i = 0; i < json_len(roots); ++i) {
        json_t *rv = json_get(roots, i);
        if (!rv || !json_is_string(rv))
            continue;
        const char *root = rv->str_val;
        if (!root)
            continue;
        size_t n = strlen(root);
        if (!strcmp(target, root) ||
            (strncmp(target, root, n) == 0 && target[n] == '\\'))
            return strdup(root);
    }
    return NULL;
}

int w12_worktreesw_d88fbf_classic_mode(int archive_dirty, double max_total_gb) {
    return (!archive_dirty && max_total_gb == 0.0) ? 1 : 0;
}
