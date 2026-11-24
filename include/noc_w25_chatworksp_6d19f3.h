/*
 * noc_w25_chatworksp_6d19f3.h - C surface for upstream
 * hermes_cli/web_routers/chat_workspaces.py (gate w25_chatworksp_6d19f3).
 *
 * The pure decision core of the dashboard's workspace picker: how a ``?cwd=``
 * turns into an absolute host directory (or a 400), and the exact payload the
 * picker answers with.  The seams -- the filesystem, Path.home(), projects.db,
 * repo discovery, the in-process gateway and FastAPI's exception machinery --
 * stay in the caller; this file owns only what the values decide.
 */
#ifndef NOC_W25_CHATWORKSP_6D19F3_H
#define NOC_W25_CHATWORKSP_6D19F3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The status the picker fails a dead path closed with. */
#define W25_CHATWORKSP_6D19F3_BAD_CWD_STATUS 400

/* {"raw": <string|null>, "home": <string|null>, "cwd": <string|null>,
 *  "isdir": <any>} -> null when unset, the absolute directory when it exists,
 * or {"status": 400, "detail": "Working directory does not exist: <text>"}. */
json_t *w25_chatworksp_6d19f3_resolve_chat_cwd(const json_t *req);

/* {"projects": [...], "repos": [...], "default_cwd": <any>, "home": <string>,
 *  "enabled": <any>} -> the dict _collect_workspaces returns. */
json_t *w25_chatworksp_6d19f3_payload(const json_t *req);

/* The picker's literal tables (payload keys, error template, status). */
json_t *w25_chatworksp_6d19f3_constants(void);

#ifdef __cplusplus
}
#endif
#endif
