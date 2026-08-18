/*
 * port_agent_display.c — Port of Python agent/display.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


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

