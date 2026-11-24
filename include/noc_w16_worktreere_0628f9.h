#ifndef NOC_W16_WORKTREERE_0628F9_H
#define NOC_W16_WORKTREERE_0628F9_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_worktreere_0628f9_read_repos @ hermes_cli/worktree_registry.py:_read
 * data is the JSON value read_json_or_empty() would return. */
struct json_t *w16_worktreere_0628f9_read_repos(const struct json_t *data);

/* PoP: w16_worktreere_0628f9_registered @ hermes_cli/worktree_registry.py:registered_repos
 * repos is the fixture's registry list; existing is the set of repos whose .git exists. */
struct json_t *w16_worktreere_0628f9_registered(const struct json_t *repos,
                                               const struct json_t *existing);

/* PoP: w16_worktreere_0628f9_merge_kept @ hermes_cli/worktree_registry.py:record_worktree_repo
 * Returns the deduped/existence-filtered list that record_worktree_repo would persist. */
struct json_t *w16_worktreere_0628f9_merge_kept(const struct json_t *current,
                                               const char *repo,
                                               const struct json_t *existing);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_WORKTREERE_0628F9_H */
