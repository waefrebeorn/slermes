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
/* PoP: agent_display_u_display_url @ agent/display.py:_display_url */
/* Faithful: (value.get("url") or value.get("href")) when dict; return
 * value.strip() if str else "". Was: printed and returned 0; stripped only
 * space/nl/cr (python .strip() = \t\v\f too). Returns owned string. */
char *agent_display_u_display_url(const char *arg) {
    json_t *j = arg ? json_parse(arg, NULL) : NULL;
    char *out = NULL;
    if (j && json_is_object(j)) {
        json_t *u = json_obj_get(j, "url");
        if (!u || (json_is_string(u) && !u->str_val[0]) || !json_is_string(u))
            u = json_obj_get(j, "href");
        if (u && json_is_string(u)) out = strdup(u->str_val);
    } else if (j && json_is_string(j)) {
        out = strdup(j->str_val);
    }
    if (j) json_free(j);
    if (!out) out = strdup("");
    char *q = out;
    while (*q==' '||*q=='\t'||*q=='\n'||*q=='\r'||*q=='\v'||*q=='\f') q++;
    if (q != out) memmove(out, q, strlen(q)+1);
    size_t n = strlen(out);
    while (n>0){char c=out[n-1]; if(c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f') out[--n]='\0'; else break;}
    return out;
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
