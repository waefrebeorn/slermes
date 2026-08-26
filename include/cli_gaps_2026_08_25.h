/* cli_gaps_2026_08_25.h — exports of src/cli/cli_gaps_2026_08_25.c
 * (REAL_GAP closure batch for cli.py, 2026-08-25). */
#ifndef SLERMES_CLI_GAPS_2026_08_25_H
#define SLERMES_CLI_GAPS_2026_08_25_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"
#include "hermes_state_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module state mirrors (cli.py globals). */
void cli_gaps_set_active_agent_ref(void *agent, const char *session_id,
                                   const char *platform);
void cli_gaps_set_wake_owner(const char *owner);
const char *cli_gaps_active_worktree_json(void);
void cli_gaps_set_active_worktree_json(const char *info_json);
void cli_mark_tui_input_modes_active_set(int v);

/* Exit watchdog + cleanup pipeline. */
void _arm_exit_watchdog(double timeout_s, int from_signal);
void _arm_exit_watchdog_on_shutdown_signal(void);
void _run_cleanup(int notify_session_finalize);
bool _should_emit_cleanup_session_finalize(const char *session_id);
void cli_should_emit_cleanup_session_finalize_noop(void); /* reserved */

/* Session finalize / one-shot store flush. */
void _notify_session_finalize(const char *session_id, const char *platform,
                              const char *reason);
void cli_notify_session_finalize(const char *session_id,
                                 const char *platform, const char *reason);
void _emit_interrupted_session_end(const char *cli_session_id,
                                   const char *agent_session_id,
                                   const char *task_id, const char *turn_id,
                                   const char *api_request_id,
                                   const char *model, const char *platform,
                                   const char *reason);
void cli_emit_interrupted_session_end(const char *cli_session_id,
                                      const char *agent_session_id,
                                      const char *task_id, const char *turn_id,
                                      const char *api_request_id,
                                      const char *model, const char *platform,
                                      const char *reason);
void _notify_single_query_session_finalize(const char *cli_session_id,
                                           const char *agent_session_id,
                                           const char *agent_platform,
                                           const char *reason);
void _flush_one_shot_session_store(hermes_state_db_t *db,
                                   const char *session_id,
                                   int persist_disabled);
void cli_flush_one_shot_session_store(hermes_state_db_t *db,
                                      const char *session_id,
                                      bool persist_disabled);
void _finalize_single_query(hermes_state_db_t *db, const char *cli_session_id,
                            const char *agent_session_id,
                            const char *agent_platform, int persist_disabled,
                            void (*release_active_session)(void));
void _reset_terminal_input_modes_on_exit(void);

/* Git worktree isolation suite. */
char *_normalize_git_bash_path(const char *p);
char *cli_normalize_git_bash_path(const char *p);
char *_git_repo_root(void);
bool _path_is_within_root(const char *path, const char *root);
void _cleanup_failed_worktree_add(const char *repo_root, const char *wt_path,
                                  const char *branch_name);
void cli_cleanup_failed_worktree_add(const char *repo_root,
                                     const char *wt_path,
                                     const char *branch_name);
void _maintain_pack_health(const char *repo_root);
void _resolve_worktree_base(const char *repo_root, double fetch_timeout,
                            double freshness_window, char **out_base_ref,
                            char **out_base_label);
json_t *_setup_worktree(const char *repo_root_in, int sync_base,
                        const char *name);
int _worktree_has_unpushed_commits(const char *worktree_path, int timeout);
int _worktree_is_dirty(const char *worktree_path, int timeout);
int _repo_is_shallow(const char *repo_path, int timeout);
bool cli_repo_is_shallow(const char *repo_path, int timeout_seconds);
int _deepen_shallow_repo(const char *repo_root, int timeout);
char *_worktree_merge_cache_path(void);
json_t *_load_worktree_merge_cache(void);
void _save_worktree_merge_cache(json_t *verdicts);
int _worktree_commits_all_merged_upstream(const char *worktree_path,
                                          int timeout, int max_ahead,
                                          json_t *cache);
int _worktree_branch_pr_merged(const char *worktree_path, int timeout,
                               json_t *cache);
char *_worktree_lock_is_live(const char *repo_root, const char *worktree_path,
                             int timeout);
void _cleanup_worktree(json_t *info);
void _prune_stale_worktrees(const char *repo_root, int max_age_hours);

/* Config-driven maintenance. */
void _run_state_db_auto_maintenance(hermes_state_db_t *session_db);
void cli_run_state_db_auto_maintenance(hermes_state_db_t *db);
void _run_checkpoint_auto_maintenance(void);
void cli_run_checkpoint_auto_maintenance(void);

/* Light/dark terminal mode + skin-aware ANSI. */
bool _luminance_from_hex(const char *hex_str, double *out_luma);
bool cg_luminance_from_hex(const char *hex_str, double *out_luma);
bool cli_luminance_from_hex(const char *hex_str, double *out_luma);
char *_query_osc11_background(void);
char *cli_query_osc11_background(void);
bool _detect_light_mode(void);
char *_maybe_remap_for_light_mode(const char *hex_color);
void _install_skin_light_mode_hook(void);
char *_hex_to_ansi(const char *hex_color, bool bold);

typedef struct _SkinAwareAnsi _SkinAwareAnsi;
_SkinAwareAnsi *_SkinAwareAnsi___init__(const char *skin_key,
                                        const char *fallback_hex, bool bold);
char *_SkinAwareAnsi___str__(_SkinAwareAnsi *self);
char *_SkinAwareAnsi___add__(_SkinAwareAnsi *self, const char *other);
char *_SkinAwareAnsi___radd__(const char *other, _SkinAwareAnsi *self);
void _SkinAwareAnsi_reset(_SkinAwareAnsi *self);
char *_accent_hex(void);

/* Stream transform output. */
char *_post_stream_transform_output(const char *response, json_t *result);
char *cli_post_stream_transform_output(const char *response, json_t *result);

#ifdef __cplusplus
}
#endif
#endif /* SLERMES_CLI_GAPS_2026_08_25_H */
