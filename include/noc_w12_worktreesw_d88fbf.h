#ifndef NOC_W12_WORKTREESW_D88FBF_H
#define NOC_W12_WORKTREESW_D88FBF_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_worktreesw_d88fbf_inside @ hermes_cli/worktree_sweep.py:_inside
 * First root equal to target or a parent of it (os.sep == '\\' on Windows),
 * else NULL.  Caller frees the returned string. */
const char *w12_worktreesw_d88fbf_inside(const char *target, const struct json_t *roots);

/* PoP: w12_worktreesw_d88fbf_classic_mode @ hermes_cli/worktree_sweep.py:classic_mode
 * not archive_dirty and not max_total_gb. */
int w12_worktreesw_d88fbf_classic_mode(int archive_dirty, double max_total_gb);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_WORKTREESW_D88FBF_H */
