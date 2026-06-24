/*
 * port_codex_runtime.c — Port of Python agent/codex_runtime.py
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Port of Python: _codex_note_to_tool_progress */
typedef struct {
    char tool_name[256];
    char preview[1024];
    char args_json[4096];
    bool valid;
} codex_tool_progress_t;

codex_tool_progress_t codex_note_to_tool_progress(const char *json_note) {
    codex_tool_progress_t result = {0};
    if (!json_note) return result;
    
    /* Parse JSON note - look for method: item/started */
    if (!strstr(json_note, "item/started")) return result;
    
    /* Extract item fields */
    const char *item_start = strstr(json_note, "\"item\"");
    if (!item_start) return result;
    
    /* Extract command field */
    const char *cmd = strstr(item_start, "\"command\"");
    if (cmd) {
        const char *val = strchr(cmd + 9, ':');
        if (val) {
            val = strchr(val, '\"');
            if (val) {
                val++;
                size_t i = 0;
                while (*val && *val != '\"' && i < sizeof(result.tool_name) - 1) {
                    result.tool_name[i++] = *val++;
                }
                result.tool_name[i] = '\0';
            }
        }
    }
    
    result.valid = (result.tool_name[0] != '\0');
    return result;
}

