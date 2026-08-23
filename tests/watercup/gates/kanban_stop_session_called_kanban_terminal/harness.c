/* gate: kanban_stop_session_called_kanban_terminal — traced faithful copy of
 * src/agent/ports_wrappers.c::agent_kanban_stop_session_called_kanban_terminal
 * over lib/libjson (same json_t API the port uses).
 * LIVE twin: agent/kanban_stop.py::session_called_kanban_terminal.
 * Input encoding: "<messages_json>\t<tools tab-separated>".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_json.h"

static bool tools_has(const char *tools, const char *name) {
    const char *tp = tools;
    while (*tp) {
        const char *tt = strchr(tp, '\t');
        size_t tl = tt ? (size_t)(tt - tp) : strlen(tp);
        if (tl == strlen(name) && strncmp(tp, name, tl) == 0) return true;
        tp = tt ? tt + 1 : tp + tl;
    }
    return false;
}

static void trace_one(const char *arg) {
    WCUP_BEGIN("session_called_kanban_terminal");
    bool found = false;

    const char *t1 = arg ? strchr(arg, '\t') : NULL;
    char buf[4096];
    snprintf(buf, sizeof buf, "%.*s", t1 ? (int)(t1 - arg) : (int)(arg ? strlen(arg) : 0), arg ? arg : "");
    json_t *msgs = json_parse(buf, NULL);
    bool is_list = msgs && json_is_array(msgs);
    WCUP_BRANCH(!is_list);

    if (is_list) {
        const char *tools = t1 ? t1 + 1 : "";
        size_t n = json_array_size(msgs);
        for (size_t i = 0; i < n && !found; i++) {
            WCUP_LOOP_TICK();
            json_t *m = json_array_get(msgs, i);
            if (!m || !json_is_object(m)) { WCUP_BRANCH(false); continue; }
            WCUP_BRANCH(true);
            const char *role = json_get_str(m, "role", "");
            if (strcmp(role, "assistant") == 0) {
                WCUP_BRANCH(true);
                json_t *tcs = json_obj_get(m, "tool_calls");
                size_t tn = (tcs && json_is_array(tcs)) ? json_array_size(tcs) : 0;
                WCUP_BRANCH(tn > 0);
                for (size_t j = 0; j < tn; j++) {
                    WCUP_LOOP_TICK();
                    json_t *tc = json_array_get(tcs, j);
                    if (!tc) { WCUP_BRANCH(false); continue; }
                    WCUP_BRANCH(true);
                    json_t *fn = json_obj_get(tc, "function");
                    const char *name = (fn && json_is_object(fn)) ? json_get_str(fn, "name", "") : "";
                    if (!name[0] && json_is_object(tc)) name = json_get_str(tc, "name", "");
                    bool hit = tools_has(tools, name);
                    WCUP_BRANCH(hit);
                    if (hit) { found = true; break; }
                }
            } else if (strcmp(role, "tool") == 0) {
                WCUP_BRANCH(false);
                WCUP_BRANCH(true);
                const char *name = json_get_str(m, "name", "");
                bool hit = tools_has(tools, name);
                WCUP_BRANCH(hit);
                if (hit) found = true;
            } else {
                WCUP_BRANCH(false);
                WCUP_BRANCH(false);
            }
        }
    }
    if (msgs) json_free(msgs);

    WCUP_BRANCH(found);
    WCUP_VALSTR(found ? "True" : "False");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("session_called_kanban_terminal");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_called_kanban_terminal '<json>\\t<tools>'...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
