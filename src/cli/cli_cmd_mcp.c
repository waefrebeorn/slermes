/*
 * cli_cmd_mcp.c — MCP slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "cli_cmd_mcp.h"
#include "commands_shared.h"
#include "hermes_core_types.h"

#include "mcp_tool.h"

/* /mcp: MCP server status and management */
/* PoP: cmd_mcp @ hermes_cli/main.py:cmd_mcp */
void cmd_mcp(const char *args, agent_state_t *state) {
    (void)state;
    /* Parse subcommand */
    char subcmd[64] = "";
    if (args && args[0]) sscanf(args, "%63s", subcmd);

    if (strcmp(subcmd, "status") == 0 || strcmp(subcmd, "list") == 0 || !subcmd[0]) {
        /* Show all MCP servers and their tools */
        printf("MCP Status: %d server(s) configured\n", g_server_count);
        for (int i = 0; i < g_server_count; i++) {
            mcp_server_t *srv = g_servers[i];
            if (!srv) continue;
            bool connected = mcp_server_is_connected(srv);
            const char *name = mcp_server_name(srv);
            printf("\n  %s — %s\n", name ? name : "(unnamed)",
                   connected ? "\033[32mconnected\033[0m" : "\033[31mdisconnected\033[0m");
            if (!connected) {
                const char *err = mcp_server_last_error(srv);
                if (err) printf("    Error: %s\n", err);
                continue;
            }
            /* List tools */
            mcp_tool_t *tools = NULL;
            int tool_count = mcp_server_list_tools(srv, &tools);
            if (tool_count > 0 && tools) {
                printf("    Tools (%d):\n", tool_count);
                for (int j = 0; j < tool_count && j < 20; j++) {
                    printf("      %s", tools[j].name);
                    if (tools[j].description[0]) {
                        char desc[80];
                        strncpy(desc, tools[j].description, sizeof(desc) - 1);
                        desc[sizeof(desc) - 1] = '\0';
                        char *nl = strchr(desc, '\n');
                        if (nl) *nl = '\0';
                        printf(" — %s", desc);
                    }
                    printf("\n");
                }
                if (tool_count > 20) printf("      ... and %d more\n", tool_count - 20);
                mcp_tool_list_free(tools, tool_count);
            } else {
                printf("    Tools: (none or not yet queried)\n");
            }
        }
    } else if (strcmp(subcmd, "test") == 0) {
        /* Extract server name from the rest of args */
        char srv_name[128] = "";
        if (args) {
            /* Skip past "test" */
            const char *p = strstr(args, "test");
            if (p) {
                p += 4;
                while (*p == ' ') p++;
                strncpy(srv_name, p, sizeof(srv_name) - 1);
                srv_name[sizeof(srv_name) - 1] = '\0';
                /* Trim trailing whitespace */
                char *end = srv_name + strlen(srv_name) - 1;
                while (end >= srv_name && (*end == ' ' || *end == '\n')) *end-- = '\0';
            }
        }
        if (!srv_name[0]) {
            printf("Usage: /mcp test <server_name>\n");
            printf("  Test connection to a configured MCP server.\n");
            return;
        }

        /* Find server by name */
        int idx = -1;
        for (int i = 0; i < g_server_count; i++) {
            mcp_server_t *srv = g_servers[i];
            if (!srv) continue;
            const char *name = mcp_server_name(srv);
            if (name && strcmp(name, srv_name) == 0) {
                idx = i;
                break;
            }
        }

        if (idx < 0) {
            printf("Error: MCP server '%s' not found.\n", srv_name);
            printf("Run '/mcp list' to see configured servers.\n");
            return;
        }

        mcp_server_t *srv = g_servers[idx];
        printf("Testing MCP server '%s'...\n", mcp_server_name(srv));

        if (mcp_server_is_connected(srv)) {
            /* Already connected — ping test */
            printf("  Status: connected\n");
            printf("  Ping:   ");
            if (mcp_server_ping(srv)) {
                printf("ok\n");
            } else {
                printf("failed: %s\n", mcp_server_last_error(srv) ? mcp_server_last_error(srv) : "unknown error");
            }
        } else {
            /* Not connected — try connecting */
            printf("  Status: disconnected\n");
            printf("  Connecting... ");
            if (mcp_server_connect(srv)) {
                printf("connected\n");
                /* List tools */
                mcp_tool_t *tools = NULL;
                int tool_count = mcp_server_list_tools(srv, &tools);
                if (tool_count > 0 && tools) {
                    printf("  Tools (%d):\n", tool_count);
                    for (int j = 0; j < tool_count && j < 10; j++) {
                        printf("    - %s\n", tools[j].name);
                    }
                    if (tool_count > 10) printf("    ... and %d more\n", tool_count - 10);
                    mcp_tool_list_free(tools, tool_count);
                }
            } else {
                printf("failed\n");
                const char *err = mcp_server_last_error(srv);
                if (err) printf("  Error: %s\n", err);
            }
        }
    } else if (strcmp(subcmd, "add") == 0) {
        /* /mcp add <name> <command>  — add a stdio MCP server at runtime */
        char srv_name[128] = "", srv_cmd[1024] = "";
        if (sscanf(args, "add %127s %1023[^\n]", srv_name, srv_cmd) < 2) {
            printf("Usage: /mcp add <server_name> <command>\n");
            printf("  Add and connect an MCP server via stdio at runtime.\n");
            printf("  Example: /mcp add my-server npx -y @modelcontextprotocol/server\n");
            return;
        }
        /* Split command into args by spaces (simple split — no quoting) */
        char *cmd_copy = strdup(srv_cmd);
        if (!cmd_copy) { printf("Error: memory allocation failed\n"); return; }
        char *args_arr[64];
        int arg_count = 0;
        args_arr[arg_count++] = strtok(cmd_copy, " ");
        while (arg_count < 64 && (args_arr[arg_count] = strtok(NULL, " ")))
            arg_count++;
        bool ok = mcp_add_stdio_server(srv_name, args_arr[0],
                                        arg_count > 1 ? args_arr + 1 : NULL,
                                        arg_count > 1 ? arg_count - 1 : 0);
        free(cmd_copy);
        if (ok) {
            printf("MCP server '%s' connected successfully.\n", srv_name);
            /* Relist servers */
            for (int i = 0; i < g_server_count; i++) {
                mcp_server_t *s = g_servers[i];
                if (!s) continue;
                const char *n = mcp_server_name(s);
                if (n && strcmp(n, srv_name) == 0) {
                    mcp_tool_t *tools = NULL;
                    int tc = mcp_server_list_tools(s, &tools);
                    if (tc > 0) {
                        printf("  Registered %d tool(s):\n", tc);
                        for (int j = 0; j < tc && j < 10; j++)
                            printf("    mcp_%s_%s\n", srv_name, tools[j].name);
                        if (tc > 10) printf("    ... and %d more\n", tc - 10);
                        mcp_tool_list_free(tools, tc);
                    }
                    break;
                }
            }
        } else {
            printf("Error: failed to connect MCP server '%s'\n", srv_name);
            const char *last_err = NULL;
            for (int i = 0; i < g_server_count; i++) {
                if (g_servers[i]) {
                    last_err = mcp_server_last_error(g_servers[i]);
                    if (last_err) break;
                }
            }
            if (last_err) printf("  Reason: %s\n", last_err);
        }
    } else if (strcmp(subcmd, "remove") == 0 || strcmp(subcmd, "rm") == 0) {
        /* /mcp remove <name> — disconnect and remove an MCP server */
        char srv_name[128] = "";
        if (sscanf(args, "%*s %127s", srv_name) < 1 || !srv_name[0]) {
            printf("Usage: /mcp remove <server_name>\n");
            printf("  Disconnect and remove an MCP server.\n");
            return;
        }
        if (mcp_remove_server(srv_name)) {
            printf("MCP server '%s' removed.\n", srv_name);
        } else {
            printf("Error: MCP server '%s' not found or could not be removed.\n", srv_name);
        }
    } else if (strcmp(subcmd, "reload") == 0) {
        /* /mcp reload — reload MCP servers from config.yaml */
        printf("Reloading MCP servers from config...\n");
        cmd_reload_mcp("", state);
    } else {
        printf("Usage: /mcp [status|list|test <name>|add <name> <cmd>|remove <name>|reload]\n");
        printf("  status              Show MCP server connection status and tools (default)\n");
        printf("  list                Alias for status\n");
        printf("  test <name>         Test connection to a specific MCP server\n");
        printf("  add <name> <cmd>    Add and connect a new MCP server via stdio\n");
        printf("  remove <name>       Disconnect and remove an MCP server\n");
        printf("  reload              Re-read MCP servers from config.yaml\n");
    }
}

/* PoP: _reload_mcp @ cli.py:_reload_mcp */
/* Port of Python cli.py:_reload_mcp(). */
void cmd_reload_mcp(const char *args, agent_state_t *state) {
    (void)args; (void)state;
    printf("MCP server reload: restart hermes to pick up config.yaml mcp_servers changes.\n");
    printf("Currently loaded: %d MCP server(s).\n", g_server_count);
    printf("To add new servers, edit ~/.slermes/config.yaml under mcp_servers: and restart.\n");
}

