/*
 * setup_mcp_tool.h — Port of tools/setup_mcp_tool.py.
 *
 * Propose an MCP server to the user as an inline consent card in the desktop
 * chat. Python round-trips through the gateway's blocking-prompt bridge via
 * a platform-injected callback; the C port takes that callback as a function
 * pointer so hosts wire the real bridge and oracles script outcomes.
 */
#ifndef SETUP_MCP_TOOL_H
#define SETUP_MCP_TOOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: callback @ tools/setup_mcp_tool.py:setup_mcp_tool — the platform-
 * injected blocking-prompt bridge. server/action/reason as in Python.
 * Returns 0 and sets *out_raw (malloc'd, caller frees) to the renderer's
 * answer, or NULL for "never answered" (timeout / closed window); returns -1
 * to model a raised exception. */
typedef int (*setup_mcp_callback_fn)(const char *server, const char *action,
                                     const char *reason, char **out_raw,
                                     void *ctx);

/* PoP: setup_mcp_tool @ tools/setup_mcp_tool.py:setup_mcp_tool
 * Run an MCP setup flow through the desktop card; returns a malloc'd JSON
 * tool-result string (caller frees). callback==NULL mirrors the desktop-only
 * guard error. */
char *setup_mcp_tool(const char *server, const char *action, const char *reason,
                     setup_mcp_callback_fn callback, void *cb_ctx);

/* PoP: SETUP_MCP_SCHEMA @ tools/setup_mcp_tool.py */
char *setup_mcp_schema(void);

#ifdef __cplusplus
}
#endif

#endif /* SETUP_MCP_TOOL_H */
