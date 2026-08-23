/*
 * preview_desktop_ui_tools.h — C11 ports of the desktop_ui preview tools:
 *   tools/close_preview_tool.py, tools/tour_tool.py,
 *   tools/annotate_preview_tool.py, tools/drive_preview_tool.py
 *
 * All four round-trip through the desktop renderer via a platform-injected
 * callback / emit hook (the C seam for Python's `desktop_ui.emit` and the
 * gateway's blocking-prompt bridge callback).
 */
#ifndef SLERMES_PREVIEW_DESKTOP_UI_TOOLS_H
#define SLERMES_PREVIEW_DESKTOP_UI_TOOLS_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── shared bridge seams ────────────────────────────────────────────────── */

/* desktop_ui.emit(event, data) -> bool, with a C-side failure channel:
 * returns 1 on success, 0 when no GUI window answered (Python False), or -1
 * on exception with *err set to the exception text (caller frees). */
typedef int (*desktop_ui_emit_cb)(void *ctx, const char *event,
                                  const json_t *data, char **err);
void desktop_ui_set_emitter(desktop_ui_emit_cb cb, void *ctx);

/* Blocking-prompt-bridge callback: payload -> raw answer string (malloc'd)
 * or NULL on timeout / no responder. */
typedef char *(*preview_bridge_cb)(void *ctx, const json_t *payload);
void preview_bridge_set_callback(preview_bridge_cb cb, void *ctx);
preview_bridge_cb preview_bridge_get(void **out_ctx);

/* ── _normalize_target @ tools/open_preview_tool.py:_normalize_target ───── */
char *preview_normalize_target(const char *raw); /* malloc'd, caller frees */

/* ── close_preview_tool @ tools/close_preview_tool.py:close_preview_tool ── */
/* Returns malloc'd JSON result/error string (caller frees). */
char *close_preview_tool(const char *url);

/* ── tour_tool @ tools/tour_tool.py:tour_tool ────────────────────────────── */
extern const char *const TOUR_ACTIONS[];   /* ACTIONS */
extern const size_t TOUR_ACTIONS_N;
extern const char *const TOUR_SURFACES[];  /* SURFACES */
extern const size_t TOUR_SURFACES_N;
extern const char *const TOUR_SIDES[];     /* SIDES */
extern const size_t TOUR_SIDES_N;
char *tour_tool(const json_t *args /* action,surface,selector,title,text,
                                        side,steps,step_index */);

/* ── annotate_preview_tool @ tools/annotate_preview_tool.py ─────────────── */
extern const char *const ANNOTATE_PREVIEW_ACTIONS[]; /* ACTIONS ("add","hold","remove","clear") */
extern const size_t ANNOTATE_PREVIEW_ACTIONS_N;
char *annotate_preview_tool(const json_t *args /* action,ref,selector,label */);

/* ── drive_preview_tool @ tools/drive_preview_tool.py ───────────────────── */
extern const char *const DRIVE_PREVIEW_ACTIONS[];
extern const size_t DRIVE_PREVIEW_ACTIONS_N;
extern const char *const DRIVE_PREVIEW_SCROLL_TO[]; /* SCROLL_TO */
extern const size_t DRIVE_PREVIEW_SCROLL_TO_N;
extern const char *const DRIVE_PREVIEW_NEEDS_TARGET[]; /* NEEDS_TARGET */
extern const size_t DRIVE_PREVIEW_NEEDS_TARGET_N;
char *drive_preview_tool(const json_t *args /* action,ref,selector,text,key,
                                               submit,amount,to,limit,full */);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PREVIEW_DESKTOP_UI_TOOLS_H */
