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

/* Dangerous-command approval modal state (_approval_state). choices are the
 * pending verdict tokens ("once"/"session"/"always"/"deny"/"view"); all
 * strings owned by the hcli_t. response_queue/queue_put is the
 * queue.put() seam consumed by _handle_approval_selection. */
typedef struct {
    char **choices;
    size_t choice_count;
    int    selected;
    bool   show_full;
    char  *command;                 /* display command string */
    char  *description;             /* panel description */
    void  *response_queue;
    void  (*queue_put)(void *queue, const char *value);
} hcli_approval_state_t;

/* Secret capture modal state (_secret_state): just the answer channel. */
typedef struct {
    void *response_queue;
    void (*queue_put)(void *queue, const char *value);
} hcli_secret_state_t;

/* Styled fragment line for panel renderers (confirm/approval/stash/prompt).
 * Declared here (ahead of struct hcli) because batch-4 seams reference it. */
typedef struct {
    char *style;   /* e.g. "class:approval-border" */
    char *text;    /* fragment text including trailing newline where set */
} hcli_fragment_t;

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

    /* ── Batch 3: user-message preview options (getattr defaults in py) ─── */
    bool   show_timestamps;                     /* _show_timestamps */
    char  *timestamp_format;                    /* _timestamp_format (NULL → "%H:%M") */
    int    user_message_preview_first_lines;    /* 0 → default 2 */
    int    user_message_preview_last_lines;     /* 0 → default 2 */
    void (*invalidate_fn)(hcli_t *self, double min_interval); /* _invalidate seam */

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

    /* ── Batch 4: dangerous-command approval modal (_approval_state) ────── */
    hcli_approval_state_t *approval_state;      /* NULL = no pending approval */
    /* _approval_callback seam: prompt for command/description, return the
     * CLI verdict ("once"/"session"/"always"/"deny"/"timeout") — malloc'd,
     * caller frees. Bound by the interactive shell; NULL on stand-ins. */
    char *(*approval_prompt_fn)(hcli_t *self, const char *command,
                                const char *description);

    /* ── Batch 4: secret capture modal (_secret_state / _secret_deadline) ─ */
    hcli_secret_state_t   *secret_state;        /* NULL = not capturing */
    double                 secret_deadline;     /* monotonic; 0 = disarmed */

    /* ── Batch 4: exit summary (_print_exit_summary) ────────────────────── */
    double session_start_epoch;                 /* self.session_start (wall s) */
    bool   has_session_start;
    void (*clear_terminal_fn)(hcli_t *self);    /* _clear_terminal_on_exit seam */
    void (*goodbye_fn)(const char *fallback);   /* skin get_active_goodbye seam */

    /* ── Batch 4: prompt text (_get_tui_prompt_text) seam ───────────────── */
    hcli_fragment_t *(*tui_prompt_fragments_fn)(hcli_t *self, size_t *out_len);

    /* ── Batch 4: TUI layout extras (_build_tui_layout_children) ────────── */
    bool has_pet_widget;                        /* getattr(self,"_pet_widget") */
    bool has_stash_panel_widget;                /* getattr(self,"_stash_panel_widget") */

    /* ── Batch 5: approval modal runtime + resize recovery + spinner +
     *           stream delta state (hermes_cli_agent_b5.c) ───────────────── */
    void   *approval_lock;                      /* _approval_lock (pthread_mutex_t, lazy) */
    double  approval_deadline;                  /* _approval_deadline (monotonic); 0 = disarmed */
    int     last_resize_width;                  /* _last_resize_width; -1 = unset */
    bool    status_bar_suppressed_after_resize;
    int     status_bar_unsuppress_gen;          /* debounce generation for the
                                                 * post-resize unsuppress timer */
    char   *spinner_text;                       /* _spinner_text */
    bool    spinner_token_flow_enabled;         /* _spinner_token_flow_enabled */
    bool    agent_running_flag;                 /* mirror of _agent_running for spinner flow */
    double  tool_start_time;                    /* _tool_start_time (monotonic s); 0 = none */
    long    turn_token_baseline;                /* _turn_token_baseline */
    char  *(*spinner_token_flow_fn)(hcli_t *self); /* _spinner_token_flow seam */
    void  (*clear_prompt_toolkit_screen_fn)(hcli_t *self, void *app, bool rebuild);
    void  (*replay_output_history_fn)(void);    /* module-level _replay_output_history */
    void  (*invalidate_simple_fn)(hcli_t *self);/* _invalidate() without throttle arg */
    /* Stream delta state (_stream_*). stream_last_was_newline starts true. */
    bool    stream_started;
    char   *stream_prefilt;
    bool    in_reasoning_block;
    bool    stream_last_was_newline;
    bool    show_reasoning;                     /* self.show_reasoning */
    void  (*stream_emit_text_fn)(hcli_t *self, const char *text);  /* _emit_stream_text */
    void  (*stream_reasoning_delta_fn)(hcli_t *self, const char *text);
    void  (*stream_flush_fn)(hcli_t *self);     /* _flush_stream */
    void  (*persist_prompt_summary_fn)(hcli_t *self, const char *icon,
                                       const char *title, const char *command,
                                       const char *outcome); /* _persist_prompt_summary */
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

/* Styled fragment line for the confirm panel renderer.
 * (hcli_fragment_t itself is declared above, before struct hcli.) */

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

/* ══════════ Batch 4 (approval + secret capture + exit summary) ═══════════ */

/* PoP: hcli_computer_use_approval_callback @ cli.py:_computer_use_approval_callback
 * Adapt the generic approval UI for the computer_use tool: prompts via
 * self->approval_prompt_fn(command="computer_use: <summary>",
 * description="Allow computer_use to perform `<action>`?") and translates the
 * CLI verdict ("once"/"session"/"always"/"deny"/"timeout") to the handler's
 * verdict ("approve_once"/"approve_session"/"always_approve"/"deny").
 * Unknown/NULL verdicts → "deny". args_json is accepted for signature parity
 * with the Python callback contract (unused, like Python ignoring `args`).
 * Returns malloc'd verdict string, caller frees. */
char *hcli_computer_use_approval_callback(hcli_t *self, const char *action,
                                          const char *args_json,
                                          const char *summary);

/* PoP: hcli_handle_approval_selection @ cli.py:_handle_approval_selection
 * Process the currently selected dangerous-command approval choice. "view"
 * flips show_full and removes itself from the choices (re-clamping selected);
 * anything else is delivered to response_queue and tears the modal down.
 * No state → no-op. */
void hcli_handle_approval_selection(hcli_t *self);

/* PoP: hcli_submit_secret_response @ cli.py:_submit_secret_response
 * Deliver the captured secret value to response_queue, clear _secret_state
 * and _secret_deadline, then paint immediately (modal teardown, #41098).
 * No active capture → no-op. */
void hcli_submit_secret_response(hcli_t *self, const char *value);

/* PoP: hcli_cancel_secret_capture @ cli.py:_cancel_secret_capture
 * Cancel = submit an empty value. */
void hcli_cancel_secret_capture(hcli_t *self);

/* PoP: hcli_print_exit_summary @ cli.py:_print_exit_summary
 * Print session resume info on exit. clear_screen=true runs the
 * _clear_terminal_on_exit seam first (interactive TUI teardown); quiet/-q
 * mode passes false to preserve printed output. Empty history → goodbye. */
void hcli_print_exit_summary(hcli_t *self, bool clear_screen);

/* PoP: hcli_get_tui_prompt_text @ cli.py:_get_tui_prompt_text
 * Join the text of the TUI prompt fragments (via the tui_prompt_fragments_fn
 * seam) for width calculations. Malloc'd (possibly empty), caller frees. */
char *hcli_get_tui_prompt_text(hcli_t *self);

/* PoP: hcli_build_tui_layout_children @ cli.py:_build_tui_layout_children
 * Assemble the ordered root HSplit children list, dropping NULL widgets
 * (the Python `[item for item in [...] if item is not None]` filter). Widget
 * identity travels as opaque token strings; extra_widgets are the
 * _get_extra_tui_widgets() override slots inserted after spacer; the pet and
 * stash-panel getattr slots follow them when present. Returns a malloc'd
 * array of *out_len malloc'd tokens, caller frees each + the array. */
char **hcli_build_tui_layout_children(
    hcli_t *self,
    const char *sudo_widget,
    const char *secret_widget,
    const char *approval_widget,
    const char *slash_confirm_widget,   /* nullable keyword default */
    const char *clarify_widget,
    const char *model_picker_widget,    /* nullable keyword default */
    const char *command_palette_widget, /* nullable keyword default */
    const char *spinner_widget,         /* nullable keyword default */
    const char *spacer,
    const char *status_bar,
    const char *input_rule_top,
    const char *image_bar,
    const char *input_area,
    const char *input_rule_bot,
    const char *voice_status_bar,
    const char *completions_menu,
    size_t extra_count, char *const *extra_widgets,
    size_t *out_len);

/* One stash row for hcli_render_stash_panel ({stashed_at, preview}). */
typedef struct {
    double stashed_at;      /* monotonic timestamp, as in Python */
    const char *preview;    /* borrowed */
} hcli_stash_row_t;

/* PoP: hcli_render_stash_panel @ cli.py:_render_stash_panel
 * Render the stash panel box as styled fragments. All horizontal measurement
 * goes through cli_status_bar_display_width (get_cwidth port — 📌 is two
 * terminal cells). Narrow-terminal compact affordances included. Returns a
 * malloc'd fragment array (+*out_len), caller frees via hcli_fragments_free;
 * empty stash still renders header/footer rows. */
hcli_fragment_t *hcli_render_stash_panel(hcli_t *self,
                                         const hcli_stash_row_t *rows,
                                         size_t row_count,
                                         int cursor, int width,
                                         size_t *out_len);

/* ══════════ Batch 5 (approval callback + resize recovery + snapshot) ═══ */

/* PoP: hcli_approval_choices @ cli.py:_approval_choices
 * Verdict tokens for the dangerous-command prompt. smart_denied or
 * !allow_session → [once,deny]; else once/session[/always]/deny.
 * len(command) > 70 appends "view". Malloc'd array of malloc'd strings;
 * caller frees each and the array. */
char **hcli_approval_choices(const char *command, bool allow_permanent,
                             bool allow_session, bool smart_denied,
                             size_t *out_count);

/* PoP: hcli_approval_callback @ cli.py:_approval_callback
 * Serialize concurrent dangerous-command approvals under _approval_lock,
 * install _approval_state, paint, then poll the response queue at 1s
 * granularity until answered or approvals.timeout expires ("timeout").
 * Malloc'd verdict; caller frees. */
char *hcli_approval_callback(hcli_t *self, const char *command,
                             const char *description, bool allow_permanent,
                             bool allow_session, bool smart_denied);

/* PoP: hcli_recover_after_resize @ cli.py:_recover_after_resize
 * SIGWINCH recovery that preserves scrollback: suppress the status bar,
 * wipe the visible viewport + replay history only on an OBSERVED width
 * change, record the new width, run the wrapped on_resize, then schedule
 * the debounced unsuppress. original_on_resize(self, app). */
void hcli_recover_after_resize(hcli_t *self, void *app,
                               void (*original_on_resize)(hcli_t *, void *));

/* PoP: hcli_get_status_bar_snapshot @ cli.py:_get_status_bar_snapshot
 * Build the status-bar field map (model name/short via reverse alias +
 * display formatting, duration, session title, prompt elapsed / idle
 * since, session token counters from agent_state_t, zeros for live
 * background counters). Returns a JSON object (caller frees). */
json_t *hcli_get_status_bar_snapshot(hcli_t *self);

/* PoP: hcli_spinner_widget_height @ cli.py:_spinner_widget_height
 * Visible rows for the spinner/status line above the status bar:
 * 0 when no text or minimal chrome; ceil(display_width/width) otherwise. */
int hcli_spinner_widget_height(hcli_t *self, int width);

/* PoP: hcli_stream_delta @ cli.py:_stream_delta
 * Line-buffered streaming callback: NULL flushes+resets, "" no-op,
 * otherwise tag-boundary reasoning suppression against the pre-filter
 * buffer with partial-tag holdback, routing inner content to the
 * reasoning display when show_reasoning is set. */
void hcli_stream_delta(hcli_t *self, const char *text);

#endif /* SLERMES_CLI_HERMES_CLI_H */
