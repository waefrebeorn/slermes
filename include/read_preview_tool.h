/*
 * read_preview_tool.h — C11 port of tools/read_preview_tool.py
 *
 * Python name -> C symbol mapping:
 *   read_preview_tool(start,count,callback) -> read_preview_tool
 *   READ_PREVIEW_SCHEMA                     -> read_preview_schema
 *   registry.register(...)                  -> read_preview_tool_register
 *
 * The preview's content lives in the desktop renderer, so the tool
 * round-trips through the gateway's blocking-prompt bridge via a
 * platform-injected callback:
 *
 *   typedef int (*read_preview_cb)(void *ctx, int has_start, long start,
 *                                  int has_count, long count,
 *                                  char **out, char **err);
 *     - returns 0: *out = raw answer (malloc'd, caller frees)
 *     - returns -1: bridge raised; *err = exception text (malloc'd)
 *   A NULL callback means "not running inside the Hermes desktop app".
 */

#ifndef HERMES_READ_PREVIEW_TOOL_H
#define HERMES_READ_PREVIEW_TOOL_H

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*read_preview_cb)(void *ctx, int has_start, long start,
                               int has_count, long count,
                               char **out, char **err);

/* PoP: read_preview_tool @ tools/read_preview_tool.py:read_preview_tool.
 * `start_node`/`count_node` mirror Python's Optional[int] arguments (any JSON
 * value node; NULL = omitted). Returns a malloc'd JSON string — the preview
 * contents or a {"error": ...} string; never NULL. */
char *read_preview_tool(const json_t *start_node, const json_t *count_node,
                        read_preview_cb callback, void *ctx);

/* PoP: READ_PREVIEW_SCHEMA @ tools/read_preview_tool.py.
 * Returns a malloc'd JSON schema string; caller frees. */
char *read_preview_schema(void);

/* Platform callback injection seam (Python passes the callback via handler
 * kwargs from the gateway; the C registry handler signature has no kwargs
 * channel, so the gateway sets it here before dispatch). */
void read_preview_tool_set_callback(read_preview_cb callback, void *ctx);

/* PoP: module-level registry.register @ tools/read_preview_tool.py.
 * Registers `read_preview` into the "desktop_ui" toolset. */
bool read_preview_tool_register(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_READ_PREVIEW_TOOL_H */
