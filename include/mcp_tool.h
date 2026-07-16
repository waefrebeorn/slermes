/*
 * mcp_tool.h — Public API for the MCP (Model Context Protocol) tool bridge.
 *
 * Faithful extraction from the monolithic hermes.h god header (the
 * god-header-elimination pass). Defined in src/tools/mcp_tool.c. CLI
 * handler modules (cli_cmd_mcp.c, commands.c) consume the server
 * registration API without pulling in the entire master header.
 */

#ifndef MCP_TOOL_H
#define MCP_TOOL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register an MCP server launched via stdio (command + args). */
bool mcp_add_stdio_server(const char *name, const char *command,
                          const char *args_csv, const char *env_csv);
/* Remove a registered MCP server by name. */
bool mcp_remove_server(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* MCP_TOOL_H */
