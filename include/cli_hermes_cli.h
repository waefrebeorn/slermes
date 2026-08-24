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
/* PoP: hcli_persist_session_yolo @ cli.py:_persist_session_yolo */
void hcli_persist_session_yolo(hcli_t *self, const char *session_key, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_CLI_HERMES_CLI_H */
