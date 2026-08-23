/*
 * worktree_gc.h — C11 port of hermes_cli/worktree_gc.py (+ the cli.py
 * classification primitives it imports: _repo_is_shallow,
 * _deepen_shallow_repo, _worktree_has_unpushed_commits,
 * _worktree_commits_all_merged_upstream, _worktree_lock_is_live,
 * _git_repo_root).
 *
 * Attended worktree + branch reclaim: explicit, loud, dry-run-first.
 * Invariants: tracked modifications are NEVER deleted; unique unpushed
 * commits are NEVER deleted (git cherry patch-equivalence decides);
 * live-locked trees are never touched; a branch is deleted only after its
 * worktree removal succeeded; untracked-only dirt is archived first.
 */
#ifndef SLERMES_WORKTREE_GC_H
#define SLERMES_WORKTREE_GC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WGC_MAX_CHERRY_AHEAD 50
/* Branches never considered for deletion, in any mode. */
extern const char *const WGC_PROTECTED_BRANCHES[];
extern const size_t WGC_PROTECTED_BRANCHES_N;

/* TreeRecord / BranchRecord dataclasses. */
typedef struct {
    char *name;
    char *path;
    char *branch;
    double age_days;
    int    size_mb;      /* valid when has_size */
    int    has_size;
    char *verdict;       /* reap | reap-archive | keep */
    char *reason;
    char **untracked;
    size_t untracked_n;
} wgc_tree_record_t;

typedef struct {
    char *name;
    char *verdict;       /* delete | keep */
    char *reason;
} wgc_branch_record_t;

void wgc_free_tree_records(wgc_tree_record_t *recs, size_t n);
void wgc_free_branch_records(wgc_branch_record_t *recs, size_t n);
void wgc_free_actions(char **actions, size_t n);

/* Run git with argv[0]="git"; returns rc (124 on timeout). stdout/stderr are
 * malloc'd when non-NULL out params given. Every verdict in this module
 * fails safe toward keep on nonzero rc. */
int wgc_run_git(char *const args[], const char *cwd, int timeout,
                char **out_stdout, char **out_stderr);

/* PoP of cli._git_repo_root: malloc'd repo root or NULL. */
char *wgc_git_repo_root(void);

/* PoP of cli._repo_is_shallow — fails toward False. */
int wgc_repo_is_shallow(const char *repo_path);

/* PoP of cli._deepen_shallow_repo — one-time blobless unshallow. */
int wgc_deepen_shallow_repo(const char *repo_root);

/* PoP of cli._worktree_has_unpushed_commits. */
int wgc_worktree_has_unpushed_commits(const char *worktree_path, int timeout);

/* PoP of cli._worktree_commits_all_merged_upstream. cache is a JSON object
 * (from cli_load_worktree_merge_cache) or NULL; memoized in place. */
int wgc_worktree_commits_all_merged_upstream(const char *worktree_path,
                                             int timeout, long max_ahead,
                                             void *cache /* json_t* or NULL */);

/* PoP of cli._worktree_lock_is_live: returns malloc'd "live"/"dead", or
 * NULL when not locked at all. Fails SAFE toward "live". */
char *wgc_worktree_lock_is_live(const char *repo_root, const char *worktree_path,
                                int timeout);

/* audit_worktrees @ hermes_cli/worktree_gc.py:audit_worktrees
 * Classify every tree under .worktrees/ without mutating anything. */
wgc_tree_record_t *wgc_audit_worktrees(const char *repo_root, int with_sizes,
                                       size_t *out_n);

/* reclaim_worktrees @ hermes_cli/worktree_gc.py:reclaim_worktrees
 * Remove every reap-verdict tree from a frozen audit list. Returns a
 * malloc'd NULL-terminated action-line array (*out_n set). records may be
 * NULL (fresh audit computed internally, without sizes). */
char **wgc_reclaim_worktrees(const char *repo_root, int dry_run,
                             wgc_tree_record_t *records, size_t records_n,
                             size_t *out_n);

/* audit_branches @ hermes_cli/worktree_gc.py:audit_branches */
wgc_branch_record_t *wgc_audit_branches(const char *repo_root, size_t *out_n);

/* reclaim_branches @ hermes_cli/worktree_gc.py:reclaim_branches */
char **wgc_reclaim_branches(const char *repo_root, int dry_run,
                            wgc_branch_record_t *records, size_t records_n,
                            size_t *out_n);

/* worktrees_summary @ hermes_cli/worktree_gc.py:worktrees_summary
 * (tree_count, total_size_mb). Size best-effort: *has_size=0 when unknown. */
void wgc_worktrees_summary(const char *repo_root, size_t *out_count,
                           int *out_size_mb, int *has_size);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_WORKTREE_GC_H */
