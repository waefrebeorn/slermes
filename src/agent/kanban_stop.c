/*
 * kanban_stop.c — canonical C11 port of agent/kanban_stop.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_kanban_stop_kanban_stop_nudge_enabled @ agent/kanban_stop.py:kanban_stop_nudge_enabled */
int agent_kanban_stop_kanban_stop_nudge_enabled(const char *arg) {
    /* Python: nudge off unless task set; explicit 0/false/no/off disables.
     * Arg = "nudge_env\ttask". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *nudge = arg;
    const char *task = tab ? tab + 1 : "";
    if (nudge[0] && (strcmp(nudge, "0") == 0 || strcasecmp(nudge, "false") == 0 ||
        strcasecmp(nudge, "no") == 0 || strcasecmp(nudge, "off") == 0)) { printf("0\n"); return 0; }
    printf("%d\n", task[0] ? 1 : 0);
    return 0;
}

/* PoP: agent_kanban_stop_u_tool_call_name @ agent/kanban_stop.py:_tool_call_name */
int agent_kanban_stop_u_tool_call_name(const char *arg) {
    /* Python: tool_call dict/attr -> function.name then name. Arg = JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *tc = json_parse(arg, NULL);
    if (!tc || !json_is_object(tc)) {
        if (tc) json_free(tc);
        printf("\n");
        return 0;
    }
    json_t *fn = json_obj_get(tc, "function");
    const char *name = "";
    if (fn && json_is_object(fn)) name = json_get_str(fn, "name", "");
    if (!*name) name = json_get_str(tc, "name", "");
    printf("%s\n", name);
    json_free(tc);
    return 0;
}

/* PoP: agent_kanban_stop_session_called_kanban_terminal @ agent/kanban_stop.py:session_called_kanban_terminal */
int agent_kanban_stop_session_called_kanban_terminal(const char *arg) {
    /* Python: scan messages for terminal kanban tool calls. Arg =
     * "messages_json\tterminal_tools". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    json_t *msgs = json_parse(arg, NULL);
    if (!msgs || !json_is_array(msgs)) {
        if (msgs) json_free(msgs);
        printf("0\n");
        return 0;
    }
    const char *tools = t1 ? t1 + 1 : "";
    size_t n = json_array_size(msgs);
    for (size_t i = 0; i < n; i++) {
        json_t *m = json_array_get(msgs, i);
        if (!m || !json_is_object(m)) continue;
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "assistant") == 0) {
            json_t *tcs = json_obj_get(m, "tool_calls");
            if (tcs && json_is_array(tcs)) {
                size_t tn = json_array_size(tcs);
                for (size_t j = 0; j < tn; j++) {
                    json_t *tc = json_array_get(tcs, j);
                    if (!tc) continue;
                    json_t *fn = json_obj_get(tc, "function");
                    const char *name = fn ? json_get_str(fn, "name", "") : "";
                    if (!name[0]) name = json_get_str(tc, "name", "");
                    const char *tp = tools;
                    while (*tp) {
                        const char *tt = strchr(tp, '\t');
                        size_t tl = tt ? (size_t)(tt - tp) : strlen(tp);
                        if (tl == strlen(name) && strncmp(tp, name, tl) == 0) { printf("1\n"); json_free(msgs); return 0; }
                        tp = tt ? tt + 1 : tp + tl;
                    }
                }
            }
        } else if (strcmp(role, "tool") == 0) {
            const char *name = json_get_str(m, "name", "");
            const char *tp = tools;
            while (*tp) {
                const char *tt = strchr(tp, '\t');
                size_t tl = tt ? (size_t)(tt - tp) : strlen(tp);
                if (tl == strlen(name) && strncmp(tp, name, tl) == 0) { printf("1\n"); json_free(msgs); return 0; }
                tp = tt ? tt + 1 : tp + tl;
            }
        }
    }
    json_free(msgs);
    printf("0\n");
    return 0;
}

/* PoP: agent_kanban_stop_build_kanban_stop_nudge @ agent/kanban_stop.py:build_kanban_stop_nudge */
int agent_kanban_stop_build_kanban_stop_nudge(const char *arg) {
    /* Python: protocol-violation nudge. Arg =
     * "enabled\tattempts\tmax\tterminal_called\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    int attempts = t1 && t1[1] == '1';
    int max_reached = t2 && t2[1] == '1';
    int terminal_called = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (!enabled || attempts || max_reached || terminal_called || !state) { printf("\n"); return 0; }
    printf("[System: You are a Hermes kanban worker. A plain-text reply is NOT a terminal state for the board.\nCall `kanban_complete` / `kanban_block` immediately.]\n");
    return 0;
}
