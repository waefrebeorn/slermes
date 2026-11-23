#ifndef NOC_W11_WORKTREEX_93E054_H
#define NOC_W11_WORKTREEX_93E054_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w11_worktreeex_93e054_worktree_state @ hermes_cli/worktree_exit.py:worktree_state
 * is_dir / is_dirty / has_unpushed are injected seams for os.path.isdir and
 * hermes_cli.worktree_ops probes. Returns a static "dirty"/"unpushed" string
 * or NULL for None (do not free). */
const char *w11_worktreeex_93e054_worktree_state(const struct json_t *info,
                                                 int is_dir, int is_dirty,
                                                 int has_unpushed);
/* PoP: w11_worktreeex_93e054_ask_exit_choice @ hermes_cli/worktree_exit.py:ask_exit_choice
 * has_tty / state / still_in_use / curses_idx / curses_error are injected seams
 * (state: null or "dirty"/"unpushed"; curses_idx: JSON number or null).
 * Returns a static "keep"/"archive"/"discard" string (do not free). */
const char *w11_worktreeex_93e054_ask_exit_choice(const struct json_t *info,
                                                  int has_tty, const struct json_t *state,
                                                  int still_in_use,
                                                  const struct json_t *curses_idx,
                                                  int curses_error);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_WORKTREEX_93E054_H */
