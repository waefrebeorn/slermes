/*
 * read_window_tool.h — C11 port of tools/read_window_tool.py
 *
 * Python name -> C symbol mapping:
 *   read_window_below_tool(callback) -> read_window_below_tool
 *   READ_WINDOW_BELOW_SCHEMA         -> read_window_below_schema
 *   registry.register(...)           -> read_window_tool_register
 *
 * The window list lives with the OS, so the tool round-trips through the
 * gateway's blocking-prompt bridge via a platform-injected callback. The C
 * port models that callback as a function pointer:
 *
 *   typedef int (*read_window_below_cb)(void *ctx, char **out, char **err);
 *     - returns 0: *out = raw answer (malloc'd, caller frees)
 *     - returns -1: bridge raised; *err = exception text (malloc'd)
 *   A NULL callback means "not running inside the Hermes desktop app",
 *   exactly like Python's callback=None.
 */

#ifndef HERMES_READ_WINDOW_TOOL_H
#define HERMES_READ_WINDOW_TOOL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*read_window_below_cb)(void *ctx, char **out, char **err);

/* PoP: tool_error @ tools/registry.py (shared helper used by the shim).
 * Returns a malloc'd {"error": "<message>"} JSON string. */
char *read_window_tool_error(const char *message);

/* PoP: read_window_below_tool @ tools/read_window_tool.py:read_window_below_tool
 * Returns a malloc'd JSON string (the window underneath the Hermes window),
 * or a {"error": ...} JSON string on every failure path — never NULL. */
char *read_window_below_tool(read_window_below_cb callback, void *ctx);

/* PoP: READ_WINDOW_BELOW_SCHEMA @ tools/read_window_tool.py.
 * Returns a malloc'd JSON schema string; caller frees. */
char *read_window_below_schema(void);

/* Platform callback injection seam (Python passes the callback via handler
 * kwargs from the gateway; the C registry handler signature has no kwargs
 * channel, so the gateway sets it here before dispatch). */
void read_window_tool_set_callback(read_window_below_cb callback, void *ctx);

/* PoP: module-level registry.register @ tools/read_window_tool.py.
 * Registers `read_window_below` into the "desktop_ui" toolset. */
bool read_window_tool_register(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_READ_WINDOW_TOOL_H */
