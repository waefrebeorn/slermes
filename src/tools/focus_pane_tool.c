/*
 * focus_pane_tool.c — canonical C11 port of tools/focus_pane_tool.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_focus_pane_tool_focus_pane_tool @ tools/focus_pane_tool.py:focus_pane_tool */
int tools_focus_pane_tool_focus_pane_tool(const char *arg) {
    /* Python: pane reveal or tool_error. Arg = "pane\tvalid\tok". */
    if (!arg || !*arg) { printf("error: pane must be one of: <panes>.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int valid = t1 && t1[1] == '1';
    if (!valid) { printf("error: pane must be one of: <panes>.\n"); return 0; }
    int ok = t2 && t2[1] == '1';
    if (!ok) { printf("error: Pane focus is only available in the Hermes desktop app.\n"); return 0; }
    printf("{\"success\": true, \"pane\": \"%s\"}\n", arg);
    return 0;
}

/* PoP: tools_focus_pane_tool_check_focus_pane_requirements @ tools/focus_pane_tool.py:check_focus_pane_requirements */
int tools_focus_pane_tool_check_focus_pane_requirements(const char *arg) {
    /* Python: env_var_enabled("HERMES_DESKTOP") — desktop GUI only. */
    (void)arg;
    const char *v = getenv("HERMES_DESKTOP");
    int enabled = v && *v && strcmp(v, "0") != 0 && strcasecmp(v, "false") != 0;
    printf("%d\n", enabled);
    return 0;
}
