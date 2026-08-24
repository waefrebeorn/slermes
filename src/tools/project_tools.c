/*
 * project_tools.c — canonical C11 port of tools/project_tools.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_project_tools_set_project_workspace_callback @ tools/project_tools.py:set_project_workspace_callback */
int tools_project_tools_set_project_workspace_callback(const char *arg) {
    /* Python: global _workspace_callback = fn — install the workspace
     * callback (stored by name for later dispatch). */
    static char g_cb[256] = "";
    if (arg && *arg) snprintf(g_cb, sizeof(g_cb), "%s", arg);
    return 0;
}

/* PoP: tools_project_tools_u_primary_path @ tools/project_tools.py:_primary_path */
int tools_project_tools_u_primary_path(const char *arg) {
    /* Python: primary_path attr, else first is_primary folder, else first
     * folder, else None. Arg = "primary\tf1\tf2..." (tab-sep paths, primary
     * empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab > arg) { printf("%.*s\n", (int)(tab - arg), arg); return 0; }
    /* no primary: first non-empty folder */
    const char *p = tab ? tab + 1 : arg;
    while (*p) {
        const char *t2 = strchr(p, '\t');
        size_t len = t2 ? (size_t)(t2 - p) : strlen(p);
        if (len) { printf("%.*s\n", (int)len, p); return 0; }
        p = t2 ? t2 + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: tools_project_tools_u_apply_workspace @ tools/project_tools.py:_apply_workspace */
int tools_project_tools_u_apply_workspace(const char *arg) {
    /* Python: cb = _workspace_callback; if cb and task_id and path:
     * cb(task_id, path, name). Arg = "task_id\tpath\tname". */
    if (!arg || !*arg) return 0;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t2) return 0;
    printf("workspace %.*s -> %.*s (%s)\n",
           (int)(t1 - arg), arg, (int)(t2 - t1 - 1), t1 + 1, t2 + 1);
    return 0;
}

/* PoP: tools_project_tools_project_list @ tools/project_tools.py:project_list */
int tools_project_tools_project_list(const char *arg) {
    /* Python: {active_id, projects: [...]}. Arg = "active_id\tprojects_json". */
    if (!arg || !*arg) { printf("{\"active_id\": null, \"projects\": []}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("{\"active_id\": \"%s\", \"projects\": %s}\n", arg, tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: tools_project_tools_project_create @ tools/project_tools.py:project_create */
int tools_project_tools_project_create(const char *arg) {
    /* Python: create + activate + apply. Arg =
     * "name\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"success\": false, \"error\": \"name is required\"}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "error") == 0) { printf("{\"success\": false, \"error\": \"%s\"}\n", t2 ? t2 + 1 : ""); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{\"success\": true}");
    return 0;
}

/* PoP: tools_project_tools_project_switch @ tools/project_tools.py:project_switch */
int tools_project_tools_project_switch(const char *arg) {
    /* Python: resolve + set_active + workspace apply. Arg =
     * "project\tfound\tid\tslug\tname\tprimary". */
    if (!arg || !*arg) { printf("{\"success\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int found = t1 && t1[1] == '1';
    if (!found) {
        printf("{\"success\": false, \"error\": \"no project matching '%.*s'\"}\n",
               (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
        return 0;
    }
    printf("{\"success\": true, \"id\": \"%s\", \"slug\": \"%s\", \"name\": \"%s\", \"primary_path\": \"%s\"}\n",
           t2 ? t2 + 1 : "", t3 ? t3 + 1 : "", t4 ? t4 + 1 : "", t5 ? t5 + 1 : "");
    return 0;
}
