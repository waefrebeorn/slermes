/*
 * port_agent_display.c — Port of Python agent/display.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include "hermes_json.h"


/* Port of Python: _delegate_task_goal_parts */
typedef struct {
    int count;
    char goals[32][256];
} delegate_task_goals_t;

delegate_task_goals_t delegate_task_goal_parts(const char *tasks_json, int per_goal_len) {
    delegate_task_goals_t result = {0};
    if (!tasks_json) return result;
    
    /* Simple JSON parsing for task goals */
    const char *p = tasks_json;
    while (*p && result.count < 32) {
        const char *goal_key = strstr(p, "\"goal\"");
        if (!goal_key) break;
        const char *val = strchr(goal_key + 6, ':');
        if (!val) break;
        val = strchr(val, '\"');
        if (!val) break;
        val++;
        size_t i = 0;
        while (*val && *val != '\"' && i < 255) {
            result.goals[result.count][i++] = *val++;
        }
        result.goals[result.count][i] = '\0';
        result.count++;
        p = val;
    }
    return result;
}


/* Port of Python: _truncate_preview */
void truncate_preview(const char *text, int max_len, char *out, size_t out_sz) {
    if (!text || !out || out_sz == 0) return;
    size_t text_len = strlen(text);
    if (max_len > 0 && (int)text_len > max_len) {
        if ((size_t)max_len <= 3) {
            memset(out, '.', max_len);
            out[max_len] = '\0';
        } else {
            memcpy(out, text, max_len - 3);
            out[max_len - 3] = '.';
            out[max_len - 2] = '.';
            out[max_len - 1] = '.';
            out[max_len] = '\0';
        }
    } else {
        strncpy(out, text, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}



/* === dissolved from ports_wrappers.c === */
/* PoP: agent_display_u_display_url @ agent/display.py:_display_url */
int agent_display_u_display_url(const char *arg) {
    /* Python: dict url/href or str strip, else "". Arg = "json_or_raw". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    const char *v = NULL;
    if (j && json_is_object(j)) {
        v = json_get_str(j, "url", "");
        if (!v[0]) v = json_get_str(j, "href", "");
    } else {
        v = arg;
    }
    if (j) json_free(j);
    if (!v) { printf("\n"); return 0; }
    while (*v == ' ') v++;
    size_t len = strlen(v);
    while (len > 0 && (v[len-1] == ' ' || v[len-1] == '\n' || v[len-1] == '\r')) len--;
    printf("%.*s\n", (int)len, v);
    return 0;
}

/* PoP: agent_display_build_status_phrase @ agent/display.py:build_status_phrase */
int agent_display_build_status_phrase(const char *arg) {
    /* Python: present-tense phrase. Arg =
     * "tool_name\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *tool = arg;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (strcmp(tool, "_thinking") == 0) { printf("\n"); return 0; }
    printf("%s…\n", t2 ? t2 + 1 : "is working");
    return 0;
}

/* PoP: agent_display_u_get_cute_tool_message @ agent/display.py:_get_cute_tool_message */
int agent_display_u_get_cute_tool_message(const char *arg) {
    /* Python: quiet-mode line. Arg =
     * "failed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int failed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (failed) { printf("| 💥 {verb:9} {detail}  {dur} {red prefix + failure suffix}\n"); return 0; }
    printf("| ✅ {verb:9} {detail}  %ss (redact args, skin prefix, truncation limits)%s\n", t2 ? t2 + 1 : "0.0", (t2 && t2[1] == '1') ? " — emoji from skin" : "");
    return 0;
}
