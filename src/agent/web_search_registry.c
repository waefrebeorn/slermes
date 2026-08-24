/*
 * web_search_registry.c — canonical C11 port of agent/web_search_registry.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_web_search_registry_u_disabled_web_plugin_for @ agent/web_search_registry.py:_disabled_web_plugin_for */
int agent_web_search_registry_u_disabled_web_plugin_for(const char *arg) {
    /* Python: disabled-provider hint. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !found) { printf("\n"); return 0; }
    printf("disabled plugin: %s\n", t2 ? t2 + 1 : "");
    return 0;
}
