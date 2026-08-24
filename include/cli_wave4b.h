/*
 * cli_wave4b.h — C11 port of dependency-ordered wave 4, batch B leaf
 * functions from cli.py (zero-dependency module-level defs).
 *
 * Each export carries a one-line PoP annotation naming its cli.py origin.
 */
#ifndef CLI_WAVE4B_H
#define CLI_WAVE4B_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"
#include "hermes_state_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Module-level session bookkeeping sets (cli.py:992/999) ──────────── */
/* _single_query_finalize_attempted_session_ids / _handed_off_session_ids.
 * NULL models Python None. Bounded sets; marking is idempotent. */
void cli_w4b_mark_single_query_finalize_attempted(const char *session_id);
void cli_w4b_mark_handed_off(const char *session_id);
bool cli_w4b_is_single_query_finalize_attempted(const char *session_id);
bool cli_w4b_is_handed_off(const char *session_id);

/* PoP: cli_query_osc11_background @ cli.py:_query_osc11_background
 * Ask the terminal for its background color via OSC 11 fenced with a DA1
 * sentinel. Returns malloc'd "#RRGGBB" (caller frees) or NULL on
 * timeout / non-tty / SSH. Real termios cbreak + select read loop,
 * TCSAFLUSH restore and 50ms post-flush drain. */
char *cli_query_osc11_background(void);

/* PoP: cli_terminal_supports_extended_enter_keys @ cli.py:_terminal_supports_extended_enter_keys
 * Allowlist of terminals safe to request modified Enter key reporting on. */
bool cli_terminal_supports_extended_enter_keys(void);

/* PoP: cli_normalize_git_bash_path @ cli.py:_normalize_git_bash_path
 * Translate /c/Users/... or /cygdrive/c/... to C:\Users\... (Windows only;
 * native paths pass through). Returns malloc'd string, caller frees.
 * NULL maps to NULL. */
char *cli_normalize_git_bash_path(const char *p);

/* PoP: cli_flush_one_shot_session_store @ cli.py:_flush_one_shot_session_store
 * Durably flush + finalize a one-shot session row before process exit.
 * db/session_id model the cli.agent surfaces; persist_disabled models
 * agent._persist_disabled. Idempotent + best-effort; sessions handed off
 * to the gateway are left strictly alone. */
void cli_flush_one_shot_session_store(hermes_state_db_t *db,
                                      const char *session_id,
                                      bool persist_disabled);

/* PoP: cli_get_toolset_for_tool @ cli.py:get_toolset_for_tool
 * Delegation to model_tools.get_toolset_for_tool (C: registry). */
const char *cli_get_toolset_for_tool(const char *tool_name);

/* Callback used by the kanban goal loop to run one agent turn.
 * Implementations must return a malloc'd final response (or an empty
 * malloc'd string); "" is fine. May return NULL to signal failure. */
typedef char *(*cli_w4b_run_turn_fn)(void *user, const char *prompt);

/* PoP: cli_run_kanban_goal_loop_q @ cli.py:_run_kanban_goal_loop_q
 * Drive a dispatcher-spawned goal_mode worker through the Ralph-style goal
 * loop (HERMES_KANBAN_TASK / HERMES_KANBAN_RUN_ID env). All errors are
 * swallowed — a broken goal loop must never wedge a worker. */
void cli_run_kanban_goal_loop_q(cli_w4b_run_turn_fn run_turn, void *user,
                                const char *first_response);

/* PoP: cli_run_state_db_auto_maintenance @ cli.py:_run_state_db_auto_maintenance
 * Config-driven state.db auto-maintenance. Never raises. */
void cli_run_state_db_auto_maintenance(hermes_state_db_t *db);

/* PoP: cli_run_checkpoint_auto_maintenance @ cli.py:_run_checkpoint_auto_maintenance
 * Config-driven checkpoint auto-prune. delete_orphans intentionally never
 * honoured (unattended sweep). Never raises. */
void cli_run_checkpoint_auto_maintenance(void);

/* PoP: cli_save_config_value @ cli.py:save_config_value
 * Persist one dotted config key into $HERMES_HOME/config.yaml atomically,
 * chmod 0600, then run the cron drift warning. Returns true on success. */
bool cli_save_config_value(const char *key_path, const char *value);

/* PoP: cli_repo_is_shallow @ cli.py:_repo_is_shallow
 * `git rev-parse --is-shallow-repository` with timeout; fails toward false. */
bool cli_repo_is_shallow(const char *repo_path, int timeout_seconds);

/* PoP: cli_emit_interrupted_session_end @ cli.py:_emit_interrupted_session_end
 * Best-effort on_session_end hook for interrupted non-interactive runs. */
void cli_emit_interrupted_session_end(const char *cli_session_id,
                                      const char *agent_session_id,
                                      const char *task_id,
                                      const char *turn_id,
                                      const char *api_request_id,
                                      const char *model,
                                      const char *platform,
                                      const char *reason);

/* PoP: cli_get_plugin_cmd_handler_names @ cli.py:_get_plugin_cmd_handler_names
 * Plugin command names (without slash prefix) as a JSON array of strings
 * (Python returns a set). Empty array on any failure. Caller frees. */
json_t *cli_get_plugin_cmd_handler_names(void);

/* PoP: cli_is_backslash_line_continuation @ cli.py:_is_backslash_line_continuation
 * True when Enter should turn a trailing backslash into a newline
 * (_BACKSLASH_LINE_CONTINUATION_RE = r"\\[ \t]*$"). */
bool cli_is_backslash_line_continuation(const char *text);

/* PoP: cli_apply_backslash_line_continuation @ cli.py:_apply_backslash_line_continuation
 * Replace a trailing "\\" marker with an actual newline. Malloc'd, free. */
char *cli_apply_backslash_line_continuation(const char *text);

/* PoP: cli_post_stream_transform_output @ cli.py:_post_stream_transform_output
 * Text still needing display after a streamed response transform.
 * Malloc'd (may be ""), caller frees. */
char *cli_post_stream_transform_output(const char *response, json_t *result);

/* PoP: cli_luminance_from_hex @ cli.py:_luminance_from_hex
 * Rec.709 luma of #RGB/#RRGGBB in 0..1. Returns false when unparsable. */
bool cli_luminance_from_hex(const char *hex_str, double *out_luma);

/* PoP: cli_cleanup_failed_worktree_add @ cli.py:_cleanup_failed_worktree_add
 * Make a failed/timed-out `git worktree add` atomic after the fact:
 * unlock, remove --force, rmtree, prune, drop branch. Fail-soft. */
void cli_cleanup_failed_worktree_add(const char *repo_root,
                                     const char *wt_path,
                                     const char *branch_name);

/* prompt_toolkit screen-diff shim types (cli.py:_hermes_call_output_screen_diff).
 * The classic CLI only ever touches `.height` on the paint buffers, so the
 * C shim carries exactly that plus width for size. A negative rc from the
 * wrapped callback models Python's AttributeError/TypeError. */
typedef struct cli_w4b_screen {
    int width;
    int height;
} cli_w4b_screen_t;

typedef int (*cli_w4b_output_screen_diff_fn)(
    void *app, void *output, cli_w4b_screen_t *screen, int current_pos,
    int color_depth, cli_w4b_screen_t *previous_screen, const char *last_style,
    bool is_done, bool full_screen, void *attrs_for_style_string,
    void *style_string_has_style, cli_w4b_screen_t size, int previous_width,
    void *user);

/* PoP: cli_hermes_call_output_screen_diff @ cli.py:_hermes_call_output_screen_diff
 * Call the wrapped _output_screen_diff with Hermes resize guards: inflate
 * previous height when the new screen is taller, and retry once with
 * previous_screen=NULL / previous_width=0 on a corrupt paint buffer. */
int cli_hermes_call_output_screen_diff(
    cli_w4b_output_screen_diff_fn orig_osd, void *orig_osd_user,
    void *app, void *output, cli_w4b_screen_t *screen, int current_pos,
    int color_depth, cli_w4b_screen_t *previous_screen, const char *last_style,
    bool is_done, bool full_screen, void *attrs_for_style_string,
    void *style_string_has_style, cli_w4b_screen_t size, int previous_width);

/* PoP: cli_status_bar_visible_from_display_config @ cli.py:_status_bar_visible_from_display_config
 * display.tui_statusbar/statusbar → visibility; "0"/"false"/"hidden"/"no"/
 * "off" all disable regardless of type. */
bool cli_status_bar_visible_from_display_config(json_t *display_config);

/* PoP: cli_notify_session_finalize @ cli.py:_notify_session_finalize
 * Best-effort lifecycle.finalize_session; swallows every error. */
void cli_notify_session_finalize(const char *session_id,
                                 const char *platform, const char *reason);

/* PoP: cli_should_emit_cleanup_session_finalize @ cli.py:_should_emit_cleanup_session_finalize
 * Gateway-owned sessions are never finalized by the CLI (#88234). */
bool cli_should_emit_cleanup_session_finalize(const char *session_id);

#ifdef __cplusplus
}
#endif
#endif /* CLI_WAVE4B_H */
