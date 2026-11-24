/*
 * noc_w23_clisubagen_f7c046.h - C surface for upstream
 * hermes_cli/cli_subagent_monitor.py (gate w23_clisubagen_f7c046).
 *
 * The monitor's live sources are seams: the subagent backend, the process
 * registry, the goal/loop/queue session rows and the wall clock all arrive as
 * the fixture's snapshot.  This surface owns only the presentation arithmetic
 * the monitor performs on that snapshot -- the cell-width clip, the count /
 * title / activity fragments and the whole dock layout (collapsed and
 * expanded), plus the transcript tail read and the modal-composer guard.
 */
#ifndef NOC_W23_CLISUBAGEN_F7C046_H
#define NOC_W23_CLISUBAGEN_F7C046_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* session_rows.QUEUE_ROWS: queued prompts painted on a tall dock. */
#define W23_CLISUBAGEN_F7C046_QUEUE_ROWS 3

/* _clip(value, width): str(value or '') -> " ".join(text.split()) -> drop
 * non-printable characters -> cell-width clip with a trailing ellipsis.
 * Caller frees. */
char *w23_clisubagen_f7c046_clip(const json_t *value, int width);

/* _counts(session=True): "2 live", "1 proc", "2 live / 3 procs / goal active
 * / 2 queued" (the separators are U+00B7 in the painted text).  req keys:
 * entries, processes, goal, loop_badge, queued,
 * session (bool, default true).  Caller frees. */
char *w23_clisubagen_f7c046_counts(const json_t *req);

/* _title(): "Live work" | "Subagents" | "Processes".  Caller frees. */
char *w23_clisubagen_f7c046_title(const json_t *req);

/* _agent_activity(row): "last: <tool>" while a tool runs, else the raw status
 * id, else "starting".  Caller frees. */
char *w23_clisubagen_f7c046_agent_activity(const json_t *row);

/* _collapsed_activity(): first agent's activity, else the first process's,
 * else the goal, the loop prompt or "next: <queued[0]>".  Caller frees. */
char *w23_clisubagen_f7c046_collapsed_activity(const json_t *req);

/* process_activity(row) from cli_process_dock (the monitor's pseudo-method
 * calls it for the dock's process rows).  Caller frees. */
char *w23_clisubagen_f7c046_process_activity(const json_t *row);

/* dock_text(columns, rows): the collapsed one-liner or the expanded block.
 * req keys: columns, rows, collapsed, entries, processes, goal, loop,
 * loop_prompt, loop_badge, queued, selected_id.  Empty when there are no rows.
 * Caller frees. */
char *w23_clisubagen_f7c046_dock_text(const json_t *req);

/* select(delta): the roster key after moving delta rows (wrapping), or JSON
 * null when the roster is empty.  req keys: entries, processes, selected_id,
 * delta.  Owned node. */
json_t *w23_clisubagen_f7c046_select(const json_t *req);

/* has_rows / roster / selected / selected_process in one answer:
 * {"has_rows": bool, "roster": [keys], "selected": key|null,
 *  "selected_is_process": bool}.  Owned object. */
json_t *w23_clisubagen_f7c046_view(const json_t *req);

/* modal_prompt_active(cli): true when any composer prompt owns the composer.
 * attrs maps the eight prompt-state attribute names to their values. */
int w23_clisubagen_f7c046_modal_prompt_active(const json_t *attrs);

/* read_tail(path): last 32 KiB of the transcript, undecodable bytes replaced
 * and everything but printable text / newline / tab dropped.  An empty path or
 * an unreadable file answers "Live transcript not available yet." */
char *w23_clisubagen_f7c046_read_tail(const char *path);

/* {queue_rows: 3} */
json_t *w23_clisubagen_f7c046_consts(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_CLISUBAGEN_F7C046_H */
