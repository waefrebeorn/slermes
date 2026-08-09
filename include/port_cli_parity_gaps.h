/*
 * port_cli_parity_gaps.h — declarations for port_cli_parity_gaps.c
 */
#ifndef PORT_CLI_PARITY_GAPS_H
#define PORT_CLI_PARITY_GAPS_H

#include <stdbool.h>
#include "libjson/json.h"
#include "context_breakdown.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _worktree_merge_cache_path @ cli.py:_worktree_merge_cache_path */
char *cli_worktree_merge_cache_path(void);
/* PoP: _load_worktree_merge_cache @ cli.py:_load_worktree_merge_cache */
json_t *cli_load_worktree_merge_cache(void);
/* PoP: _save_worktree_merge_cache @ cli.py:_save_worktree_merge_cache */
void cli_save_worktree_merge_cache(json_t *verdicts);

/* PoP: _show_context_breakdown @ cli.py:_show_context_breakdown */
char *cli_show_context_breakdown(context_breakdown_agent_t *agent,
                                 const char *cmd_original);

/* PoP: _restore_session_yolo @ cli.py:_restore_session_yolo */
bool cli_restore_session_yolo(json_t *session_meta, const char *session_key);
/* PoP: _persist_session_yolo @ cli.py:_persist_session_yolo */
bool cli_persist_session_yolo(const char *session_id, bool enabled);

/* PoP: _render_stash_panel @ cli.py:_render_stash_panel
 * Returns malloc'd string (caller frees). */
char *cli_render_stash_panel(const char **stash_items, size_t n_items,
                             int cursor, int width);

/* PoP: _should_handle_background_command_inline @ cli.py:_should_handle_background_command_inline */
bool cli_should_handle_background_command_inline(const char *text, bool has_images);

/* PoP: handle_bang_shell @ cli.py:handle_bang_shell */
bool cli_handle_bang_shell(const char *text);

/* PoP: _voice_stt_provider @ cli.py:_voice_stt_provider */
const char *cli_voice_stt_provider(json_t *config);
/* PoP: _typed_voice_stop @ cli.py:_typed_voice_stop */
bool cli_typed_voice_stop(const char *user_input, bool voice_on);
/* PoP: _maybe_start_wake_word @ cli.py:_maybe_start_wake_word */
void cli_maybe_start_wake_word(void);
/* PoP: _start_wake_word_listener @ cli.py:_start_wake_word_listener */
bool cli_start_wake_word_listener(bool announce);
/* PoP: _stop_wake_word_listener @ cli.py:_stop_wake_word_listener */
void cli_stop_wake_word_listener(bool announce);
/* PoP: _on_wake_word @ cli.py:_on_wake_word */
void cli_on_wake_word(void);
/* PoP: _start_wake_watchdog @ cli.py:_start_wake_watchdog */
void cli_start_wake_watchdog(void);
/* PoP: _show_wake_word_status @ cli.py:_show_wake_word_status */
void cli_show_wake_word_status(void);
/* Grace-period re-arm scheduled by cli_on_wake_word (no public Python
 * equivalent — C only, since the CLI has no session/voice hooks to call). */
#define WW_WAKE_REARM_GRACE_SECONDS 3
void cli_schedule_wake_rearm(int grace_seconds);
/* PoP: _voice_full_duplex_listener @ cli.py:_voice_full_duplex_listener */
void cli_voice_full_duplex_listener(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CLI_PARITY_GAPS_H */
