/* cli_helpers2.h — wave 4 batch A leaf ports from cli.py. */
#ifndef CLI_HELPERS2_H
#define CLI_HELPERS2_H

#include "hermes_json.h"
#include "port_config_py_helpers.h"

/* PoP: _split_model_config_default @ cli.py:_split_model_config_default */
void cli_split_model_config_default(const json_t *raw_default, char **out_model, char **out_provider);

/* PoP: _worktree_branch_pr_merged @ cli.py:_worktree_branch_pr_merged */
int cli_worktree_branch_pr_merged(const char *worktree_path, int timeout, json_t *cache);

/* PoP: _cli_multiline_shortcuts_enabled @ cli.py:_cli_multiline_shortcuts_enabled */
int cli_multiline_shortcuts_enabled(const json_t *config);

/* PoP: _parse_skills_argument @ cli.py:_parse_skills_argument */
int cli_parse_skills_argument(const json_t *skills, char ***out_items);

/* PoP: _is_ghostty_terminal @ cli.py:_is_ghostty_terminal */
int cli_is_ghostty_terminal(void);

/* PoP: _cleanup_all_terminals @ cli.py:_cleanup_all_terminals */
int cli_cleanup_all_terminals(void);

/* PoP: _maintain_pack_health @ cli.py:_maintain_pack_health */
void cli_maintain_pack_health(const char *repo_root);

/* PoP: _select_classic_cli_pt_output @ cli.py:_select_classic_cli_pt_output */
int cli_select_classic_cli_pt_output(void);

/* PoP: _heal_cooked_mode_drift @ cli.py:_heal_cooked_mode_drift */
int cli_heal_cooked_mode_drift(int fd);

/* PoP: _wait_for_oneshot_background_completions @ cli.py:_wait_for_oneshot_background_completions */
void cli_wait_for_oneshot_background_completions(const char *task_id);

/* PoP: _normalize_moa_model @ cli.py:_normalize_moa_model */
int cli_normalize_moa_model(const char *model, char **out_provider, char **out_rest);

/* PoP: _worktree_commits_all_merged_upstream @ cli.py:_worktree_commits_all_merged_upstream */
int cli_worktree_commits_all_merged_upstream(const char *worktree_path, int timeout,
                                             int max_ahead, json_t *cache);

/* PoP: _worktree_has_unpushed_commits @ cli.py:_worktree_has_unpushed_commits */
int cli_worktree_has_unpushed_commits(const char *worktree_path, int timeout);

/* PoP: _prepare_deferred_agent_startup @ cli.py:_prepare_deferred_agent_startup */
void cli_prepare_deferred_agent_startup(void);

/* PoP: _prune_orphaned_branches @ cli.py:_prune_orphaned_branches */
void cli_prune_orphaned_branches(const char *repo_root);

/* PoP: _cleanup_all_browsers @ cli.py:_cleanup_all_browsers */
void cli_cleanup_all_browsers(void);

/* PoP: _reverse_alias_for_display @ cli.py:_reverse_alias_for_display */
char *cli_reverse_alias_for_display(const char *model_name);

/* PoP: _mark_tui_input_modes_active @ cli.py:_mark_tui_input_modes_active */
void cli_mark_tui_input_modes_active(void);
int cli_tui_input_modes_active(void);

/* PoP: _path_is_within_root @ cli.py:_path_is_within_root */
int cli_path_is_within_root(const char *path, const char *root);

#endif /* CLI_HELPERS2_H */
