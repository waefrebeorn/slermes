/*
 * kanban_db.h — Slermes C11 engine for hermes_cli/kanban_db.py
 *
 * This is the SOLE shared header for the kanban_db port. It is deliberately
 * NOT a god header: it forward-declares the opaque model structs and exposes
 * only the engine API + sqlite3 handle type. Each concern lives in its own
 * translation unit (kanban_schema, kanban_model, kanban_tasks,
 * kanban_lifecycle, kanban_notify) and includes ONLY what it needs.
 *
 * Faithful port of the Python module's database engine. Read/write parity is
 * oracle-verified against the live Python module (tests/oracle/runners).
 *
 * Reuses (no duplication):
 *   - path/config helpers already in src/cli/port_kanban_db.c
 *     (kanban_db_path, kanban_home, board_dir, normalize_board_slug,
 *      board_exists, kanban_board_exists, to_epoch, relative_age,
 *      error_fingerprint, looks_like_path, is_windows_batch_shim, ...)
 *   - src/hermes_cli/sqlite_util.{c,h} (add_column_if_missing, write_txn)
 *   - lib/libdb/sqlite3.c (bundled SQLite)
 *   - lib/libjson via hermes_json.h (json_t shim)
 *
 * PoP: exact port. Semantic source of truth = hermes_cli/kanban_db.py.
 */

#ifndef SLERMES_KANBAN_DB_H
#define SLERMES_KANBAN_DB_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants (mirror Python module-level constants)
 * ========================================================================= */

#define KANBAN_DEFAULT_CLAIM_TTL_SECONDS      (15 * 60)
#define KANBAN_DEFAULT_CRASH_GRACE_SECONDS    30
#define KANBAN_DEFAULT_RATE_LIMIT_COOLDOWN    300
#define KANBAN_DEFAULT_BUSY_TIMEOUT_MS        120000
#define KANBAN_DEFAULT_FAILURE_LIMIT          2
#define KANBAN_DEFAULT_BOARD                  "default"
#define KANBAN_BLOCK_RECURRENCE_LIMIT         2
#define KANBAN_RATE_LIMIT_EXIT_CODE           75

/* =========================================================================
 * Opaque model structs
 *
 * The Python module uses @dataclass view objects (Task, Run, Comment,
 * Attachment, Event). We model them as opaque handles: callers never reach
 * into the fields directly; they go through *_get / *_set accessors. This
 * keeps each struct's layout private to kanban_model.c (the only TU that
 * includes the backing definitions), so the other concerns stay decoupled
 * and the module is genuinely self-contained per concern.
 * ========================================================================= */

typedef struct kanban_task     kanban_task_t;
typedef struct kanban_run      kanban_run_t;
typedef struct kanban_comment  kanban_comment_t;
typedef struct kanban_attach   kanban_attach_t;
typedef struct kanban_event    kanban_event_t;

/* ---- internal constructors (used across concerns, not for callers) ---- */
kanban_task_t     *kdb_task_from_row(sqlite3_stmt *row);
kanban_run_t      *kdb_run_from_row(sqlite3_stmt *row);
kanban_comment_t  *kdb_comment_from_row(sqlite3_stmt *row);
kanban_attach_t   *kdb_attach_from_row(sqlite3_stmt *row);
kanban_event_t    *kdb_event_from_row(sqlite3_stmt *row);

/* Canonicalise an assignee/profile name (lowercased slug). */
void kdb_canon_assignee(const char *in, char *out, size_t sz);
void kdb_task_free(kanban_task_t *t);
void kdb_run_free(kanban_run_t *r);
void kdb_comment_free(kanban_comment_t *c);
void kdb_attach_free(kanban_attach_t *a);
void kdb_event_free(kanban_event_t *e);

/* =========================================================================
 * Connection / schema  (kanban_schema.c)
 * ========================================================================= */

/* Deterministic wall-clock helper. Returns epoch seconds. When the env var
 * HERMES_KANBAN_NOW is set to an integer, that value is returned for the life
 * of the process (used by the contract oracle so time-based stats compare
 * byte-for-byte). Otherwise returns the real time(NULL). */
long kdb_now(void);

/* Open (and lazily init) the kanban DB for `board`, or the resolved default.
 * Returns NULL on failure. Caller closes with kdb_close(). */
sqlite3 *kdb_connect(const char *board);

/* Open a connection at an explicit db path. */
sqlite3 *kdb_connect_path(const char *db_path);

/* Guarantee the connection is closed. */
void kdb_close(sqlite3 *conn);

/* Idempotent schema + additive migration pass. Returns 0 on success. */
int kdb_init_db(sqlite3 *conn);

/* Begin/commit a write transaction (wraps sqlite_util). */
int  kdb_write_begin(sqlite3 *conn);
int  kdb_write_end(sqlite3 *conn, int committed);

/* =========================================================================
 * Model accessors  (kanban_model.c)
 * ========================================================================= */

/* Task */
kanban_task_t *kdb_task_from_row(sqlite3_stmt *row);  /* binds by column name */
kanban_task_t *kdb_task_get(sqlite3 *conn, const char *task_id);
const char *kdb_task_id(const kanban_task_t *t);
const char *kdb_task_title(const kanban_task_t *t);
const char *kdb_task_status(const kanban_task_t *t);
int         kdb_task_priority(const kanban_task_t *t);
const char *kdb_task_assignee(const kanban_task_t *t);
const char *kdb_task_body(const kanban_task_t *t);
const char *kdb_task_workspace_kind(const kanban_task_t *t);
const char *kdb_task_workspace_path(const kanban_task_t *t);
const char *kdb_task_tenant(const kanban_task_t *t);
const char *kdb_task_created_by(const kanban_task_t *t);
long        kdb_task_created_at(const kanban_task_t *t);
long        kdb_task_started_at(const kanban_task_t *t);
long        kdb_task_completed_at(const kanban_task_t *t);
long        kdb_task_claim_expires(const kanban_task_t *t);
const char *kdb_task_branch_name(const kanban_task_t *t);
const char *kdb_task_result(const kanban_task_t *t);
int         kdb_task_consecutive_failures(const kanban_task_t *t);
const char *kdb_task_last_failure_error(const kanban_task_t *t);
long        kdb_task_current_run_id(const kanban_task_t *t);
const char *kdb_task_block_kind(const kanban_task_t *t);
int         kdb_task_block_recurrences(const kanban_task_t *t);

/* Run */
const char *kdb_run_outcome(const kanban_run_t *r);
const char *kdb_run_summary(const kanban_run_t *r);
const char *kdb_run_error(const kanban_run_t *r);
long        kdb_run_id(const kanban_run_t *r);
const char *kdb_run_status(const kanban_run_t *r);

/* Comment */
const char *kdb_comment_author(const kanban_comment_t *c);
const char *kdb_comment_body(const kanban_comment_t *c);
long        kdb_comment_created_at(const kanban_comment_t *c);

/* Attachment */
const char *kdb_attach_filename(const kanban_attach_t *a);
const char *kdb_attach_stored_path(const kanban_attach_t *a);
const char *kdb_attach_content_type(const kanban_attach_t *a);
long        kdb_attach_size(const kanban_attach_t *a);

/* Event */
const char *kdb_event_kind(const kanban_event_t *e);
const char *kdb_event_payload_json(const kanban_event_t *e);
long        kdb_event_id(const kanban_event_t *e);
long        kdb_event_created_at(const kanban_event_t *e);

/* =========================================================================
 * Task CRUD / links / comments / attachments / events / runs  (kanban_tasks.c)
 * ========================================================================= */

typedef struct {
    const char *title;
    const char *body;
    const char *assignee;
    const char *created_by;
    const char *workspace_kind;   /* NULL -> "scratch" */
    const char *workspace_path;
    const char *branch_name;
    const char *tenant;
    const char *idempotency_key;  /* NULL ok */
    const char *skills_json;      /* JSON array string, or NULL */
    const char *max_retries;      /* stringified int or NULL */
    const char *goal_max_turns;   /* stringified int or NULL */
    const char *session_id;
    const char *project_id;
    int         priority;
    int         triage;           /* bool */
    int         goal_mode;        /* bool */
    int         has_max_runtime;  /* bool */
    long        max_runtime_seconds;
} kdb_create_spec_t;

/* Create a task. Returns a malloc'd id (caller frees) or NULL on validation
 * failure. `parents` is a NULL-terminated array of parent ids (or NULL). */
char *kdb_create_task(sqlite3 *conn, const kdb_create_spec_t *spec,
                         char **parents);

/* List tasks. Returns a malloc'd NULL-terminated array of kanban_task_t*.
 * `status_filter` / `assignee_filter` / `tenant_filter` / `session_filter`
 * may be NULL. `limit` <= 0 means unlimited. Caller frees with
 * kdb_task_list_free(). */
kanban_task_t **kdb_list_tasks(sqlite3 *conn,
                                  const char *status_filter,
                                  const char *assignee_filter,
                                  const char *tenant_filter,
                                  const char *session_filter,
                                  int include_archived,
                                  int limit,
                                  int *out_count);
void kdb_task_list_free(kanban_task_t **list);

int  kdb_assign_task(sqlite3 *conn, const char *task_id, const char *profile);

/* Links (dependency edges). */
int  kdb_link_tasks(sqlite3 *conn, const char *parent_id, const char *child_id);
int  kdb_unlink_tasks(sqlite3 *conn, const char *parent_id, const char *child_id);
char **kdb_parent_ids(sqlite3 *conn, const char *task_id, int *out_n);
void    kdb_parent_ids_free(char **list);
char **kdb_child_ids(sqlite3 *conn, const char *task_id, int *out_n);
void    kdb_child_ids_free(char **list);

/* Comments. */
int  kdb_add_comment(sqlite3 *conn, const char *task_id,
                        const char *author, const char *body);
kanban_comment_t **kdb_list_comments(sqlite3 *conn, const char *task_id, int *out_n);
void kdb_comment_list_free(kanban_comment_t **list);

/* Attachments. */
int  kdb_add_attachment(sqlite3 *conn, const char *task_id,
                           const char *filename, const char *stored_path,
                           const char *content_type, long size,
                           const char *uploaded_by);
kanban_attach_t **kdb_list_attachments(sqlite3 *conn, const char *task_id, int *out_n);
kanban_attach_t *kdb_get_attachment(sqlite3 *conn, long attach_id);
void kdb_attachment_list_free(kanban_attach_t **list);
int  kdb_delete_attachment(sqlite3 *conn, long attach_id);

/* Events (append + query). */
int  kdb_append_event(sqlite3 *conn, const char *task_id, long run_id,
                         const char *kind, const char *payload_json);
kanban_event_t **kdb_list_events(sqlite3 *conn, const char *task_id, int *out_n);
void kdb_event_list_free(kanban_event_t **list);

/* Runs. */
kanban_run_t **kdb_list_runs(sqlite3 *conn, const char *task_id,
                                int include_active, int *out_n);
void kdb_run_list_free(kanban_run_t **list);
kanban_run_t *kdb_get_run(sqlite3 *conn, long run_id);
char *kdb_latest_summary(sqlite3 *conn, const char *task_id);  /* malloc'd */
kanban_run_t *kdb_latest_run(sqlite3 *conn, const char *task_id);

/* Run lifecycle (kanban_runs.c). */
int  kdb_current_run_id(sqlite3 *conn, const char *task_id);
int  kdb_end_run(sqlite3 *conn, const char *task_id, const char *outcome,
                 const char *summary, const char *error, const char *metadata,
                 const char *status);
int  kdb_synthesize_ended_run(sqlite3 *conn, const char *task_id,
                              const char *outcome, const char *summary,
                              const char *error, const char *metadata);
int  kdb_has_sticky_block(sqlite3 *conn, const char *task_id);
int  kdb_would_cycle(sqlite3 *conn, const char *parent_id, const char *child_id);
int  kdb_parent_results(sqlite3 *conn, const char *task_id,
                         char ***out_parents, char ***out_results);
void kdb_parent_results_free(char **parents, char **results);
int  kdb_reclaim_task(sqlite3 *conn, const char *task_id, const char *reason);

/* =========================================================================
 * Notify  (kanban_notify.c)
 * ========================================================================= */

/* Subscribe a (task, platform, chat_id, thread_id) to task events. */
int  kdb_add_notify_sub(sqlite3 *conn, const char *task_id,
                         const char *platform, const char *chat_id,
                         const char *thread_id, const char *user_id,
                         const char *notifier_profile);
int  kdb_remove_notify_sub(sqlite3 *conn, const char *task_id,
                           const char *platform, const char *chat_id,
                           const char *thread_id);
/* Advance / rewind a subscription's event cursor (after / on failed delivery). */
int  kdb_advance_notify_cursor(sqlite3 *conn, const char *task_id,
                                const char *platform, const char *chat_id,
                                const char *thread_id, int new_cursor);
int  kdb_rewind_notify_cursor(sqlite3 *conn, const char *task_id,
                              const char *platform, const char *chat_id,
                              const char *thread_id, int claimed_cursor, int old_cursor);

/* Listing / claiming unseen events. */
char **kdb_list_notify_subs(sqlite3 *conn, const char *task_id, int *out_n); /* NULL-term rows as JSON */
int  kdb_unseen_events_for_sub(sqlite3 *conn, const char *task_id,
                               const char *platform, const char *chat_id,
                               const char *thread_id, char **kinds, int n_kinds,
                               int *out_new_cursor, kanban_event_t ***out_events, int *out_n);
int  kdb_claim_unseen_events_for_sub(sqlite3 *conn, const char *task_id,
                                     const char *platform, const char *chat_id,
                                     const char *thread_id, char **kinds, int n_kinds,
                                     int *out_old_cursor, int *out_new_cursor,
                                     kanban_event_t ***out_events, int *out_n);
void kdb_event_list_free(kanban_event_t **list);

/* =========================================================================
 * Query / analytics  (kanban_query.c)
 * ========================================================================= */

/* Garbage-collect events for terminal-state tasks older than the cutoff. */
int  kdb_gc_events(sqlite3 *conn, int older_than_seconds);
/* Discover profile names on disk (mirrors list_profiles_on_disk). Returns a
 * NULL-terminated malloc'd array (caller frees with kdb_strv_free). */
char **kdb_list_profiles_on_disk(void);
void   kdb_strv_free(char **v);
/* True when an sqlite error message denotes a lock/busy contention. */
int  kdb_is_busy_error(const char *msg);
/* Canonical absolute path for a resolved Hermes shim (expands ~, cwd). */
char *kdb_absolute_hermes_path(const char *path);
/* Candidate executable names for an unqualified command (NULL-term, free w/ kdb_strv_free). */
char **kdb_path_search_names(const char *command);
/* Resolve a bare command from PATH, skipping empty/"." entries. Returns malloc'd path or NULL. */
char *kdb_safe_which_no_cwd(const char *command);
/* True when `table` still carries the legacy (pre-AUTOINCREMENT) shape. */
int  kdb_table_has_drifted(sqlite3 *conn, const char *table);
/* Read SQLite header page_count vs actual file size; returns 1 if the file is
 * shorter than the header claims (torn-extent corruption), else 0. */
int  kdb_check_file_length_invariant(sqlite3 *conn);
/* Copy a corrupt DB (+WAL/-SHM sidecars) to a content-addressed backup
 * inside its own parent dir; returns malloc'd backup path (caller frees) or NULL. */
char *kdb_backup_corrupt_db(const char *path);
/* Run PRAGMA integrity_check on an existing DB. Returns 0=healthy/no-op,
 * 1=CORRUPT (backup written, *reason_out malloc'd, caller frees),
 * -1=transient lock/busy (propagate; no backup). */
int  kdb_guard_existing_db_is_healthy(const char *path, char **reason_out);
/* Per-install scratch-workspace tip sentinel (malloc'd, caller frees). */
char *kdb_scratch_tip_sentinel_path(void);
/* True iff the scratch-tip has already been emitted this install. */
int  kdb_scratch_tip_shown(void);
/* Touch the sentinel so future scratch workspaces stay silent. */
void kdb_mark_scratch_tip_shown(void);
/* Emit the first-use scratch-workspace tip exactly once per install. */
void kdb_maybe_emit_scratch_tip(sqlite3 *conn, const char *task_id,
                                const char *workspace_kind);
/* Enumerate assignees: returns malloc'd JSON array of
 * {"name","on_disk","counts":{status:n}} objects (caller frees). */
char *kdb_known_assignees(sqlite3 *conn);
/* Board-level run summary for a set of task ids (NULL-term). Returns malloc'd
 * JSON dict {task_id: summary}. Caller frees. */
char *kdb_latest_summaries_json(sqlite3 *conn, char **task_ids, int n_ids);

/* ---- kanban_decompose concern: pure triage/roster helpers ---- */

/* Extract the first {...} JSON object blob from an LLM reply, stripping
 * ```fence``` markers. Returns malloc'd JSON string or NULL on no/!dict. */
char *kdb_extract_json_blob(const char *raw);

/* Resolve which profile owns the orchestration task after fan-out. Mirrors
 * _resolve_orchestrator_profile: honors kanban.orchestrator_profile when the
 * profile exists, else the active/default profile ("default"). */
char *kdb_resolve_orchestrator_profile(const char *kanban_cfg_json);
/* Resolve the default assignee for fan-out children (kanban.default_assignee
 * if it exists, else active/default). */
char *kdb_resolve_default_assignee(const char *kanban_cfg_json);

/* Return a malloc'd JSON roster of installed profiles:
 * [{"name","description","has_description"}]. Caller frees. */
char *kdb_build_roster(void);
/* Format a roster (from kdb_build_roster output) into the prompt string.
 * Returns malloc'd string; "" -> "(no profiles installed ...)". Caller frees. */
char *kdb_format_roster(const char *roster_json);

/* Normalize an assignee choice to a valid name, falling back to
 * default_assignee. valid_names is a NULL-terminated list. Returns malloc'd
 * chosen name (never NULL). */
char *kdb_normalize_assignee_choice(const char *assignee,
                                    const char *default_assignee,
                                    char **valid_names);

/* =========================================================================
 * Lifecycle  (kanban_lifecycle.c)
 * ========================================================================= */

/* Recompute ready: promote todo/blocked tasks with all parents done.
 * Returns number promoted. failure_limit < 0 -> module default. */
int  kdb_recompute_ready(sqlite3 *conn, int failure_limit);

/* Claim. Returns a malloc'd claimed task-id (caller frees) on success, or
 * NULL if not claimable / already claimed. `claimer` may be NULL. */
char *kdb_claim_task(sqlite3 *conn, const char *task_id,
                        int ttl_seconds, const char *claimer);
char *kdb_claim_review_task(sqlite3 *conn, const char *task_id,
                               int ttl_seconds, const char *claimer);
int  kdb_heartbeat_claim(sqlite3 *conn, const char *task_id,
                            int ttl_seconds, const char *claimer);
int  kdb_release_stale_claims(sqlite3 *conn);

/* Complete. Returns 1 on success, 0 if the task wasn't in a completable
 * state. `created_cards` is a NULL-terminated array of verified ids or NULL. */
int  kdb_complete_task(sqlite3 *conn, const char *task_id,
                          const char *result, const char *summary,
                          const char *metadata_json,
                          char **created_cards, long expected_run_id);

/* Block. `kind` may be NULL (legacy). Returns 1 on success, 0 otherwise. */
int  kdb_block_task(sqlite3 *conn, const char *task_id,
                       const char *reason, const char *kind,
                       long expected_run_id);

/* Promote (manual). Returns 1 on success, 0 refused (and sets *refuse_reason,
 * caller frees, when non-NULL). */
int  kdb_promote_task(sqlite3 *conn, const char *task_id,
                         const char *actor, const char *reason,
                         int force, int dry_run, char **refuse_reason);

/* Unblock. Returns 1 on success, 0 otherwise. */
int  kdb_unblock_task(sqlite3 *conn, const char *task_id);

/* Reassign to a (possibly different) profile. Returns 1 on success. */
int  kdb_reassign_task(sqlite3 *conn, const char *task_id,
                          const char *profile);

/* Schedule (park in `scheduled`). Returns 1 on success. */
int  kdb_schedule_task(sqlite3 *conn, const char *task_id,
                          const char *reason, long expected_run_id);

/* Archive / delete. */
int  kdb_archive_task(sqlite3 *conn, const char *task_id);
int  kdb_delete_archived_task(sqlite3 *conn, const char *task_id);
int  kdb_delete_task(sqlite3 *conn, const char *task_id);

/* Specify a triage task and promote to todo. Returns 1 on success. */
int  kdb_specify_triage_task(sqlite3 *conn, const char *task_id,
                                const char *title, const char *body,
                                const char *assignee, const char *author);

/* Edit a completed task's result/summary. Returns 1 on success. */
int  kdb_edit_completed_task_result(sqlite3 *conn, const char *task_id,
                                       const char *result,
                                       const char *summary,
                                       const char *metadata_json);

/* =========================================================================
 * Notifications  (kanban_notify.c)
 * ========================================================================= */

/* (subscriptions declared in the Notification section below) */

/* =========================================================================
 * Notify  (kanban_notify.c)
 * ========================================================================= */

/* Board statistics. Returns a malloc'd JSON string (caller frees) shaped
 * like the Python dict: {"by_status":{...},"by_assignee":{...},
 * "oldest_ready_age_seconds":N|null,"now":N}. */
char *kdb_board_stats(sqlite3 *conn);

/* Age metrics for a single task. Returns malloc'd JSON like Python's dict
 * {"created_age_seconds":N|null,"started_age_seconds":N|null,
 * "time_to_complete_seconds":N|null}. */
char *kdb_task_age(sqlite3 *conn, const char *task_id);

/* =========================================================================
 * Boards  (kanban_boards.c)
 * ========================================================================= */

/* Path helpers (implemented in port_kanban_db.c). Caller frees the result. */
char *kanban_home(void);
char *kanban_db_path(const char *board);
char *board_dir(const char *board);          /* <home>/kanban/boards/<slug>/  */
char *kanban_boards_root(void);              /* <home>/kanban/boards          */
char *board_metadata_path(const char *board);/* board_dir(slug)/board.json    */
char *normalize_board_slug(const char *slug);
char *get_current_board(void);
#define KB_DEFAULT_BOARD "default"

/* True if the board has persisted metadata or a DB on disk.
 * "default" always exists. */
int  kdb_board_exists(const char *board);

/* Create a board dir + board.json + touch the DB. Idempotent.
 * Returns a malloc'd metadata JSON string (caller frees); NULL on bad slug. */
char *kdb_create_board(const char *slug,
                       const char *name, const char *description,
                       const char *icon, const char *color,
                       const char *default_workdir);

/* Enumerate boards on disk. Returns a malloc'd JSON array of metadata
 * objects (default first, rest alpha). Caller frees. */
char *kdb_list_boards(int include_archived);

/* Read board.json (or synthesized defaults). Returns malloc'd JSON; caller frees. */
char *kdb_read_board_metadata(const char *board);

/* Write board.json, preserving existing fields. Returns malloc'd JSON; caller frees. */
char *kdb_write_board_metadata(const char *board,
                               const char *name, const char *description,
                               const char *icon, const char *color,
                               int archived_set, int archived,
                               const char *default_workdir);

/* Remove or archive a board. action = "archive" (default) or "delete".
 * Returns malloc'd JSON summary {slug,action,new_path}; caller frees.
 * Returns NULL for the default board or a non-existent board (matches Python
 * raising ValueError — caller should treat NULL as error). */
char *kdb_remove_board(const char *slug, int archive);

/* Returns malloc'd workspaces dir for board. Caller frees.
 * (port_kanban_db.c) */
char *workspaces_root(const char *board);

/* True iff a profile directory named `name` exists on disk.
 * (port_kanban_db.c — shared by decompose/util concern modules.) */
int  profile_exists(const char *name);

/* =========================================================================
 * Utilities  (kanban_util.c)  — portable DB/logic helpers
 * ========================================================================= */

/* Generate a short URL-safe task id ("t_" + 4 hex bytes). Caller frees. */
char *kdb_new_task_id(void);

/* Return a "host:pid" claimer string. Caller frees. */
char *kdb_claimer_id(void);

/* Of `parents` (NULL-terminated), return the subset that do NOT exist in the
 * tasks table. Caller frees the returned NULL-terminated array. */
char **kdb_find_missing_parents(sqlite3 *conn, char **parents, int n);

/* Partition `claimed_ids` (NULL-terminated) into (verified, phantom) JSON
 * arrays describing which exist+trusted vs don't. Writes malloc'd JSON to
 * *out_verified / *out_phantom (caller frees). Returns 1 on success. */
int  kdb_verify_created_cards(sqlite3 *conn, const char *completing_task_id,
                               char **claimed_ids, int n,
                               char **out_verified, char **out_phantom);

/* Free the arrays returned by kdb_find_missing_parents / kdb_verify_created_cards. */
void kdb_strv_free(char **list);

/* Regex-scan free-form `text` for t_<hex> references that do NOT exist in
 * tasks. Returns a malloc'd JSON array of phantom ids; caller frees. */
char *kdb_scan_prose_for_phantom_ids(sqlite3 *conn, const char *text);

/* True iff `path` is a STRICT descendant of a kanban-managed scratch root
 * (workspaces/, per-board workspaces/, or the HERMES_KANBAN_WORKSPACES_ROOT
 * override). Refuses paths equal to a root or outside managed storage. */
int  kdb_is_managed_scratch_path(const char *path);

/* Resolve (and create if needed) the workspace for a task, returning the
 * absolute path as a malloc'd string. `kind` is "scratch"|"dir"|"worktree";
 * `board` optional. Caller frees. Returns NULL on a bad/non-absolute path. */
char *kdb_resolve_workspace(const char *kind, const char *workspace_path,
                             const char *task_id, const char *board);

/* Persist a workspace path back onto a task row. Returns 1 on success. */
int  kdb_set_workspace_path(sqlite3 *conn, const char *task_id, const char *path);

/* True iff there is at least one ready+assigned+unclaimed task whose assignee
 * maps to a real profile. */
int  kdb_has_spawnable_ready(sqlite3 *conn);

/* Same, for the `review` status column. */
int  kdb_has_spawnable_review(sqlite3 *conn);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_KANBAN_DB_H */
