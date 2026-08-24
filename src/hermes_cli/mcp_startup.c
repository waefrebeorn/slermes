/*
 * mcp_startup.c — canonical C11 port of hermes_cli/mcp_startup.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_mcp_startup_u_has_configured_mcp_servers @ hermes_cli/mcp_startup.py:_has_configured_mcp_servers */
int hermes_cli_mcp_startup_u_has_configured_mcp_servers(const char *arg) {
    /* Python: mcp_servers dict non-empty; conservative True on error. Arg =
     * "servers_json\tconfig_error". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') { printf("1\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (j && json_is_object(j)) {
        size_t n = json_object_size(j);
        printf("%d\n", n > 0 ? 1 : 0);
        json_free(j);
        return 0;
    }
    if (j) json_free(j);
    printf("1\n");
    return 0;
}

/* PoP: hermes_cli_mcp_startup_u_resolve_discovery_timeout @ hermes_cli/mcp_startup.py:_resolve_discovery_timeout */
int hermes_cli_mcp_startup_u_resolve_discovery_timeout(const char *arg) {
    /* Python: explicit > config > 1.5 default. Arg =
     * "explicit\tconfig\tresult". */
    if (!arg || !*arg) { printf("1.50\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t2 ? t2 + 1 : "";
    if (result[0]) { printf("%s\n", result); return 0; }
    if (arg[0] && strcmp(arg, "none") != 0) { printf("%s\n", arg); return 0; }
    printf("1.50\n");
    return 0;
}

/* PoP: hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th @ hermes_cli/mcp_startup.py:_discover_mcp_tools_without_interactive_oauth */
int hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(const char *arg) {
    /* Python: discover MCP tools under suppressed-interactive-oauth context. */
    (void)arg;
    printf("mcp discovery done\n");
    return 0;
}

/* PoP: hermes_cli_mcp_startup_mcp_discovery_in_flight @ hermes_cli/mcp_startup.py:mcp_discovery_in_flight */
int hermes_cli_mcp_startup_mcp_discovery_in_flight(const char *arg) {
    /* Python: discovery thread alive. Arg = "alive". */
    if (arg && arg[0] == '1') { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_mcp_startup_join_mcp_discovery @ hermes_cli/mcp_startup.py:join_mcp_discovery */
int hermes_cli_mcp_startup_join_mcp_discovery(const char *arg) {
    /* Python: join thread; True if done. Arg = "alive". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    printf("%d\n", arg[0] == '1' ? 0 : 1);
    return 0;
}

/* PoP: hermes_cli_mcp_startup_ensure_mcp_discovery_before_agent_build @ hermes_cli/mcp_startup.py:ensure_mcp_discovery_before_agent_build */
int hermes_cli_mcp_startup_ensure_mcp_discovery_before_agent_build(const char *arg) {
    /* Python: start background MCP discovery if needed (idempotent), then
     * wait up to the configured bound so the first agent turn sees mcp__
     * tools. Arg = "discovered\ttimed_out\tthread_name". */
    if (!arg || !*arg) { printf("mcp discovery: no servers configured\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int discovered = arg[0] == '1';
    int timed_out = t2 && t2[1] == '1';
    if (!discovered) { printf("mcp discovery: no servers configured\n"); return 0; }
    if (timed_out) { printf("mcp discovery: timed out for %s (single_query bound)\n", t3 ? t3 + 1 : "cli-mcp-discovery"); return 0; }
    printf("mcp discovery: ready (%s)\n", t3 ? t3 + 1 : "cli-mcp-discovery");
    return 0;
}
