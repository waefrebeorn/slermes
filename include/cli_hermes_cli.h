/* cli_hermes_cli.h — C11 port of the HermesCLI class (cli.py) session /
 * conversation I/O surface. Batch 1: /save, resume selection, empty-session
 * pruning, model-runtime snapshots, command palette ranking, slash-confirm
 * normalization, YOLO transfer, process-notification ownership, and friends.
 *
 * Convention: every function takes the opaque `hcli_t *self` handle, mirroring
 * `self` on class HermesCLI. Persistence goes through the real hermes_state_db
 * (SessionDB) API — no stubs. Shared with the parallel HermesCLI batch agents:
 * extend this header rather than recreating it; re-read before editing.
 */

#ifndef SLERMES_CLI_HERMES_CLI_H
#define SLERMES_CLI_HERMES_CLI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hermes_state_db hermes_state_db_t;
typedef struct json_t json_t;

/* Pending local-edit snapshot entry (_pending_edit_snapshots[tool_call_id]).
 * snap is a display_local_edit_snapshot_t* owned by the hcli_t. */
typedef struct {
    char *id;
    void *snap;
} hcli_pending_snap_t;

/* Opaque HermesCLI instance (cli.py class HermesCLI). */
typedef struct hcli hcli_t;

/* One in-memory conversation message (role/content pair, like the dicts on
 * self.conversation_history). Both strings are owned by the hcli_t. */
typedef struct {
    char *role;
    char *content;
} hcli_msg_t;

/* Slash-confirm choice triple: (value, label, description). Pointers are
 * borrowed (caller-owned storage), matching the Python tuples. */
typedef struct {
    const char *value;
    const char *label;
    const char *desc;
} hcli_confirm_choice_t;

/* Model runtime snapshot (_snapshot_model_runtime). Strings owned by the
 * struct; hcli_model_runtime_free() releases them. */
typedef struct {
    char *model;
    char *provider;
    char *requested_provider;
    char *explicit_api_key;
    char *explicit_base_url;
    char *api_key;
    char *base_url;
    char *api_mode;
    /* Deep copy of agent._primary_runtime JSON (NULL when no agent). */
    char *agent_primary_runtime_json;
} hcli_model_runtime_t;

/* Command palette entry row: (command, category, description). Owned. */
typedef struct {
    char *cmd;
    char *category;
    char *desc;
} hcli_palette_entry_t;

struct hcli {
    /* Session persistence */
    hermes_state_db_t *session_db;
    char *session_id;

    /* Live conversation transcript (in-memory authoritative view). */
    hcli_msg_t *history;
    size_t history_len;

    /* Enabled toolset names as a JSON array string (self.enabled_toolsets);
     * NULL/empty means "all" (no explicit enable list). */
    char *enabled_toolsets_json;

    /* Model/runtime identity */
    char *model;
    char *provider;
    char *requested_provider;
    char *base_url;
    char *api_key;
    char *api_mode;
    bool  explicit_model_override;
    bool  verbose;

    /* Agent back-reference (opaque; may be NULL — TUI slash-worker case). */
    void *agent;
    bool  agent_has_memory_manager;   /* getattr(agent, "_memory_manager") */
    bool  yolo_mode_frozen;           /* tools.approval._YOLO_MODE_FROZEN */

    /* Pending bare-/resume prompt state (#34584): armed list of session ids. */
    char **pending_resume_sessions;
    size_t pending_resume_count;

    /* Slash confirm modal state (_slash_confirm_state). */
    hcli_confirm_choice_t *confirm_choices;
    size_t confirm_choice_count;
    char  *confirm_title;
    char  *confirm_detail;
    int    confirm_selected;

    /* Command palette modal state (_command_palette_state). */
    hcli_palette_entry_t *palette_entries;
    size_t palette_entry_count;
    char  *palette_filter;
    int    palette_selected;

    /* Composer prefill buffer (_prefill_input_buffer target). */
    char *input_prefill;

    /* Optional hook: persist YOLO flag onto a session row (bound to
     * _persist_session_yolo); NULL when unavailable (unbound test stand-ins). */
    void (*persist_yolo)(hcli_t *self, const char *session_key, bool enabled);

    /* ── Status-bar / session-lease state (_release_active_session & co) ── */
    void *active_session_lease;                 /* _active_session_lease */
    void (*lease_release)(void *lease);         /* lease.release() */

    char *pending_title;                        /* _pending_title */
    char *status_bar_title_session_id;          /* _status_bar_title_session_id */
    char *status_bar_title_cache;               /* _status_bar_title_cache */
    double status_bar_title_checked_at;         /* _status_bar_title_checked_at (monotonic) */

    double prompt_start_time;                   /* _prompt_start_time */
    bool   has_prompt_start_time;
    double prompt_duration;                     /* _prompt_duration */
    double last_turn_finished_at;               /* _last_turn_finished_at */
    bool   has_last_turn_finished_at;

    /* ── Batch 2: config watch (_check_config_mcp_changes) ──────────────── */
    double last_config_check;                   /* _last_config_check (monotonic s) */
    double config_mtime;                        /* _config_mtime */
    char  *config_mcp_servers_json;             /* serialized mcp_servers snapshot
                                                 * (_config_mcp_servers, env-expanded
                                                 * because the loader expands). */
    void (*reload_mcp)(hcli_t *self);           /* self._reload_mcp seam */

    /* ── Batch 2: pending edit snapshots (_pending_edit_snapshots) ──────── */
    hcli_pending_snap_t *pending_edit_snaps;
    size_t               pending_edit_snap_count;

    /* ── Batch 2: voice mode + run-state flags ──────────────────────────── */
    bool should_exit;                           /* _should_exit */
    bool agent_running;                         /* _agent_running */
    bool voice_mode;                            /* _voice_mode */
    bool voice_recording;                       /* _voice_recording */
    bool voice_processing;                      /* _voice_processing */
    bool voice_continuous;                      /* _voice_continuous */
    bool voice_tts;                             /* _voice_tts */
    char *voice_last_tts_text;                  /* _voice_last_tts_text */
    void *voice_lock;                           /* lazily created pthread_mutex_t */
    void *voice_tts_done_ev;                    /* lazily created threading.Event */
    void *voice_recorder;                       /* opaque recorder handle */
    void (*recorder_cancel)(void *rec);         /* recorder.cancel() */
    void (*recorder_shutdown)(void *rec);       /* recorder.shutdown() */
    void  *voice_tts_stop;                      /* stop event for TTS pipeline */
    void  (*tts_stop_set)(void *ev);            /* stop_event.set() */
    void  (*stop_playback_fn)(void);            /* tools.voice_mode.stop_playback */
    char *(*tts_generate_fn)(const char *text, const char *output_path);
                                /* tools.tts_tool.text_to_speech_tool seam —
                                 * returns the tool's raw JSON result string. */
    void (*voice_fd_listener_fn)(hcli_t *self); /* _voice_full_duplex_listener */
    bool (*pending_input_nonempty_fn)(hcli_t *self); /* not self._pending_input.empty() */

    /* ── Batch 2: wake word ─────────────────────────────────────────────── */
    bool wake_word_active;                      /* _wake_word_active */
    bool wake_suspended;                        /* _wake_suspended */
    bool wake_watchdog_started;                 /* _wake_watchdog_started */

    /* ── Batch 3: model identity / streaming state ──────────────────────── */
    bool   model_is_default;                    /* _model_is_default */
    char  *reasoning_preview_buf;               /* _reasoning_preview_buf */
    char  *reasoning_buf;                       /* _reasoning_buf */
    bool   reasoning_box_opened;                /* _reasoning_box_opened */
    bool   reasoning_shown_this_turn;           /* _reasoning_shown_this_turn */
    bool   stream_box_opened;                   /* _stream_box_opened */
    char  *deferred_content;                    /* _deferred_content */

    /* ── Batch 3: busy slash-command state (_busy_command) ──────────────── */
    bool   command_running;                     /* _command_running */
    bool   command_blocks_input;                /* _command_blocks_input */
    char  *command_status;                      /* _command_status */
    bool   skip_paste_collapse;                 /* _skip_paste_collapse */

    /* ── Batch 3: termios drift watchdog (_check_termios_drift) ─────────── */
    double last_termios_drift_check;            /* _last_termios_drift_check (monotonic) */
    bool   termios_drift_notice_shown;          /* _termios_drift_notice_shown */

    /* Injectable probe for model_tools.check_tool_availability() — returns a
     * malloc'd JSON array of {name, tools[], missing_vars[]} rows for
     * UNAVAILABLE tools, or NULL when the tool registry is not linked in.
     * Bound by the CLI shell at startup; NULL on test stand-ins (the Python
     * path equally no-ops when the import fails). */
    char *(*tool_availability_probe)(hcli_t *self);
};

/* ── Lifecycle ─────────────────────────────────────────────────────────── */
hcli_t *hcli_new(void);
void    hcli_free(hcli_t *self);

/* History mutation helpers (own/dup semantics like the Python list). */
void hcli_history_clear(hcli_t *self);
void hcli_history_append(hcli_t *self, const char *role, const char *content);

/* ── PoP: hcli_show_toolsets @ cli.py:HermesCLI.show_toolsets ────────────── */
void hcli_show_toolsets(hcli_t *self);

/* ── PoP: hcli_handle_whoami_command @ cli.py:HermesCLI._handle_whoami_command ─*/
void hcli_handle_whoami_command(hcli_t *self);

/* ── PoP: hcli_list_recent_sessions @ cli.py:HermesCLI._list_recent_sessions ──
 * Recent CLI sessions for in-chat browsing/resume. Returns a malloc'd JSON
 * array of {id,title,preview,last_active} rows (source=cli prefix, kanban/
 * tool excluded, current session excluded, unnamed included) or NULL. */
char *hcli_list_recent_sessions(hcli_t *self, int limit);

/* ── PoP: hcli_discard_session_if_empty @ cli.py:HermesCLI._discard_session_if_empty ─*/
bool hcli_discard_session_if_empty(hcli_t *self, const char *session_id);

/* ── PoP: hcli_launch_session_boundary_memory_flush @ cli.py:_launch_session_boundary_memory_flush ─*/
bool hcli_launch_session_boundary_memory_flush(hcli_t *self,
                                               const hcli_msg_t *history_snapshot,
                                               size_t history_len,
                                               const char *session_id);

/* Result codes for hcli_consume_pending_resume_selection:
 *   0 = not consumed (no pending prompt / not a bare number → normal handling)
 *   1 = consumed: resumed via "/resume <index>"
 *   2 = consumed but out of range (message printed)
 * The pending state is always disarmed on the first submitted input. */
#define HCLI_RESUME_NOT_CONSUMED   0
#define HCLI_RESUME_DISPATCHED     1
#define HCLI_RESUME_OUT_OF_RANGE   2

/* ── PoP: hcli_consume_pending_resume_selection @ cli.py:_consume_pending_resume_selection ─*/
int hcli_consume_pending_resume_selection(hcli_t *self, const char *text);

/* ── PoP: hcli_save_conversation @ cli.py:HermesCLI.save_conversation ──────
 * Handle `/save [json|md|html] [filename] [redact]`. Writes under
 * <hermes_home>/sessions/saved when no filename given. Prefers the durable
 * DB row, falls back to the in-memory history. Returns malloc'd message or
 * NULL (output already printed). */
char *hcli_save_conversation(hcli_t *self, const char *cmd);

/* ── PoP: hcli_undo_content_to_text @ cli.py:HermesCLI._undo_content_to_text ──
 * Flatten content (plain string or JSON array of {type:text,text:...} parts)
 * to plain text. Malloc'd; caller frees. */
char *hcli_undo_content_to_text(const char *content_json);

/* ── PoP: hcli_prefill_input_buffer @ cli.py:HermesCLI._prefill_input_buffer ──*/
void hcli_prefill_input_buffer(hcli_t *self, const char *text);

/* ── PoP: hcli_normalize_slash_confirm_choice @ cli.py:_normalize_slash_confirm_choice ─
 * Map raw input ("1", "yes", "always", "cancel", ...) to one of the offered
 * choice values. Malloc'd value (caller frees) or NULL. */
char *hcli_normalize_slash_confirm_choice(const char *raw,
                                          const hcli_confirm_choice_t *choices,
                                          size_t choice_count);

/* Styled fragment line for the confirm panel renderer. */
typedef struct {
    char *style;   /* e.g. "class:approval-border" */
    char *text;    /* fragment text including trailing newline where set */
} hcli_fragment_t;

/* ── PoP: hcli_get_slash_confirm_display_fragments @ cli.py:_get_slash_confirm_display_fragments ─
 * Render the /new-/clear-style confirmation panel as styled fragments.
 * Returns malloc'd array (caller frees each style/text + the array via
 * hcli_fragments_free); *out_len receives the count. Empty state → NULL/0. */
hcli_fragment_t *hcli_get_slash_confirm_display_fragments(hcli_t *self, size_t *out_len);
void hcli_fragments_free(hcli_fragment_t *frags, size_t len);

/* ── PoP: hcli_command_palette_visible_entries @ cli.py:_command_palette_visible_entries ─
 * Ranked rows matching the active filter (exact < startswith < substring <
 * subsequence-in-command < substring-in-description; ties keep registry
 * order). Returns malloc'd array of owned entries + count. */
hcli_palette_entry_t *hcli_command_palette_visible_entries(hcli_t *self, size_t *out_len);

/* ── PoP: hcli_snapshot_model_runtime @ cli.py:_snapshot_model_runtime ──────*/
hcli_model_runtime_t *hcli_snapshot_model_runtime(hcli_t *self);
/* ── PoP: hcli_restore_model_runtime_snapshot @ cli.py:_restore_model_runtime_snapshot ─*/
void hcli_restore_model_runtime_snapshot(hcli_t *self, const hcli_model_runtime_t *snap);
void hcli_model_runtime_free(hcli_model_runtime_t *snap);

/* ── PoP: hcli_filter_model_picker_entries @ cli.py:_filter_model_picker_entries ─
 * Subsequence fuzzy filter; returns ORIGINAL indices of matching entries
 * (order preserved). Malloc'd index array + count; empty query keeps all. */
size_t *hcli_filter_model_picker_entries(char **entries, size_t n,
                                         const char *query, size_t *out_len);

/* ── PoP: hcli_compute_model_picker_viewport @ cli.py:_compute_model_picker_viewport ─*/
void hcli_compute_model_picker_viewport(int selected, int scroll_offset, int n,
                                        int term_rows, int reserved_below,
                                        int panel_chrome, int min_visible,
                                        int *out_scroll_offset, int *out_visible);

/* ── PoP: hcli_should_handle_model_command_inline @ cli.py:_should_handle_model_command_inline ─*/
bool hcli_should_handle_model_command_inline(const char *text);

/* ── PoP: hcli_resolve_personality_prompt @ cli.py:_resolve_personality_prompt ─
 * Accept string-or-dict (JSON-encoded) personality value; returns malloc'd
 * system prompt string or NULL. */
char *hcli_resolve_personality_prompt(const char *value_json);

/* ── PoP: hcli_owns_process_notification @ cli.py:_owns_process_notification ─
 * event_json: {"session_key": "..."} delegation event. Resolves compression
 * continuations before comparing; missing/foreign keys fail closed. */
bool hcli_owns_process_notification(hcli_t *self, const char *event_json);

/* ── PoP: hcli_write_terminal_breadcrumb @ cli.py:_write_terminal_breadcrumb ─
 * Best-effort record of this terminal's live session id for bare `hermes -c`.
 * No-op without HERMES_TERMINAL_ID / terminal_continue disabled. */
void hcli_write_terminal_breadcrumb(hcli_t *self);

/* ── YOLO bypass quartet (tools/approval session-keyed state) ───────────── */
/* PoP: hcli_transfer_session_yolo @ cli.py:_transfer_session_yolo */
void hcli_transfer_session_yolo(hcli_t *self, const char *old_id, const char *new_id);
/* PoP: hcli_is_session_yolo_active @ cli.py:_is_session_yolo_active */
bool hcli_is_session_yolo_active(hcli_t *self);
/* PoP: hcli_toggle_yolo @ cli.py:_toggle_yolo */
void hcli_toggle_yolo(hcli_t *self);
/* ── PoP: hcli_persist_session_yolo @ cli.py:_persist_session_yolo */
void hcli_persist_session_yolo(hcli_t *self, const char *session_key, bool enabled);

/* ── Batch 0 continuation: status-bar / formatting surface ─────────────── */

/* PoP: hcli_release_active_session @ cli.py:_release_active_session
 * Release the active-session lease (best effort) and clear it. */
void hcli_release_active_session(hcli_t *self);

/* PoP: cli_redraw_rebuilds_scrollback @ cli.py:_redraw_rebuilds_scrollback
 * display.cli_rebuild_scrollback_on_redraw config gate; string truthy set
 * {"1","true","yes","on","always"}. */
bool cli_redraw_rebuilds_scrollback(void);

/* PoP: cli_status_bar_context_style @ cli.py:_status_bar_context_style
 * percent_used < 0 means Python None. Returns a shared style-class string. */
const char *cli_status_bar_context_style(int percent_used);

/* PoP: cli_battery_status_style @ cli.py:_battery_status_style */
const char *cli_battery_status_style(const char *category);

/* PoP: cli_compression_count_style @ cli.py:_compression_count_style */
const char *cli_compression_count_style(int count);

/* PoP: cli_build_context_bar @ cli.py:_build_context_bar
 * "[████░░░░]" style bar; percent_used < 0 means None → 0%. Malloc'd. */
char *cli_build_context_bar(int percent_used, int width);

/* PoP: cli_format_prompt_elapsed @ cli.py:_format_prompt_elapsed
 * has_start=false + duration==0 → "⏲ 0s". When has_start, elapsed is
 * now() - start_time, else duration. Malloc'd. */
char *cli_format_prompt_elapsed(bool has_start, double start_time,
                                double duration, bool live);

/* PoP: cli_format_idle_since @ cli.py:_format_idle_since
 * "" while live / never finished; else "✓ <compact>". Malloc'd. */
char *cli_format_idle_since(bool has_last_finished_at, double last_finished_at,
                            bool turn_live);

/* PoP: hcli_get_status_bar_session_title @ cli.py:_get_status_bar_session_title
 * pending_title short-circuit, 1.5s monotonic cache, then SessionDB lookup.
 * Malloc'd (possibly empty). */
char *hcli_get_status_bar_session_title(hcli_t *self);

/* PoP: cli_status_bar_display_width @ cli.py:_status_bar_display_width
 * Terminal cell width (prompt_toolkit get_cwidth port): wide East-Asian /
 * emoji = 2, combining/variation selectors = 0, ambiguous = 1. */
int cli_status_bar_display_width(const char *text);

/* PoP: cli_trim_status_bar_text @ cli.py:_trim_status_bar_text
 * Trim to one terminal row with a "..." suffix. Malloc'd. */
char *cli_trim_status_bar_text(const char *text, int max_width);

/* PoP: cli_right_align_status_title @ cli.py:_right_align_status_title
 * Pin a bounded session-title badge to the far-right edge. Malloc'd. */
char *cli_right_align_status_title(const char *text, const char *title, int width);

/* PoP: hcli_right_align_status_title_fragments @ cli.py:_right_align_status_title_fragments
 * Styled counterpart. Input frags are parallel arrays of style/value strings;
 * returns a malloc'd hcli_fragment_t array (+*out_len) or NULL when the badge
 * cannot be placed (no title / narrow width) — caller frees via
 * hcli_fragments_free(). */
hcli_fragment_t *hcli_right_align_status_title_fragments(char **styles,
                                                         char **values,
                                                         size_t n,
                                                         const char *title,
                                                         int width,
                                                         size_t *out_len);

/* PoP: cli_get_tui_terminal_width @ cli.py:_get_tui_terminal_width
 * Live TTY width via TIOCGWINSZ (the C stand-in for prompt_toolkit's app
 * output), falling back to the shutil default of 80 columns. */
int cli_get_tui_terminal_width(void);

/* PoP: cli_scrollback_box_width @ cli.py:_scrollback_box_width
 * width <= 0 means auto-detect; floor of 32 columns. */
int cli_scrollback_box_width(int width);


#ifdef __cplusplus
}
#endif

/* ══════════ Batch 2 (cli_hermes_cli_skeleton.h wave) — refined API ═════════ */

/* PoP: hcli_check_config_mcp_changes @ cli.py:_check_config_mcp_changes
 * Detect mcp_servers changes in config.yaml and react. Throttled to one
 * stat() per 5 s; auto-reloads via self->reload_mcp when enabled. */
void hcli_check_config_mcp_changes(hcli_t *self);

/* PoP: hcli_split_destructive_skip @ cli.py:_split_destructive_skip
 * Classmethod: strip "/cmd" word + inline-skip tokens ("now","--yes","-y").
 * Sets *out_remainder (malloc'd, caller frees; "" when nothing remains).
 * Returns true iff at least one skip token was found. */
bool hcli_split_destructive_skip(const char *cmd_text, char **out_remainder);

/* PoP: hcli_on_tool_start @ cli.py:_on_tool_start
 * Capture local before-state snapshot for write-capable tools. */
void hcli_on_tool_start(hcli_t *self, const char *tool_call_id,
                        const char *function_name, const char *function_args_json);

/* PoP: hcli_on_tool_complete @ cli.py:_on_tool_complete
 * Pop the pending snapshot and render the inline edit diff. */
void hcli_on_tool_complete(hcli_t *self, const char *tool_call_id,
                           const char *function_name, const char *function_args_json,
                           const char *function_result);

/* PoP: hcli_voice_stt_model @ cli.py:_voice_stt_model
 * STT model override from config ("stt.model", or "stt.local.model" with a
 * "base" default for the local provider). malloc'd or NULL on error. */
char *hcli_voice_stt_model(hcli_t *self);

/* PoP: hcli_voice_stt_provider @ cli.py:_voice_stt_provider
 * Configured STT provider name, lowercased/stripped. malloc'd, never NULL. */
char *hcli_voice_stt_provider(hcli_t *self);

/* PoP: hcli_voice_speak_response_async @ cli.py:_voice_speak_response_async */
void hcli_voice_speak_response_async(hcli_t *self, const char *text);

/* PoP: hcli_voice_speak_response @ cli.py:_voice_speak_response
 * Blocking TTS speak path (Python runs it in a background thread). */
void hcli_voice_speak_response(hcli_t *self, const char *text);

/* PoP: hcli_voice_beeps_enabled @ cli.py:_voice_beeps_enabled */
bool hcli_voice_beeps_enabled(hcli_t *self);

/* PoP: hcli_disable_voice_mode @ cli.py:_disable_voice_mode */
void hcli_disable_voice_mode(hcli_t *self);

/* PoP: hcli_stop_wake_word_listener @ cli.py:_stop_wake_word_listener */
void hcli_stop_wake_word_listener(hcli_t *self, bool announce);

/* PoP: hcli_start_wake_watchdog @ cli.py:_start_wake_watchdog */
void hcli_start_wake_watchdog(hcli_t *self);

#endif /* SLERMES_CLI_HERMES_CLI_H */
