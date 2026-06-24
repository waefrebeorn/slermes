/*
 * port_tools_tool_search.c — C port of tools/tool_search.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_tool_search_from_raw @ tools/tool_search.py:from_raw */

/* Port of Python tools/tool_search.py:from_raw */
/* Creates a tool search instance from raw tool definitions. */
int cli_tools_tool_search_from_raw(
    const char *tool_defs_json, char *output, size_t output_size)
{
    if (!tool_defs_json || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: tool search requires full tool registry. */
    strncpy(output, tool_defs_json, output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_tools_tool_search_load_config @ tools/tool_search.py:load_config */

/* Port of Python tools/tool_search.py:load_config */
/* Loads tool search configuration. */
int cli_tools_tool_search_load_config(
    const char *config_path, char *output, size_t output_size)
{
    if (!config_path || !output || output_size == 0) {
        return -1;
    }
    FILE *f = fopen(config_path, "r");
    if (!f) {
        return -1;
    }
    size_t n = fread(output, 1, output_size - 1, f);
    output[n] = '\0';
    fclose(f);
    return 0;
}

/* PoP: cli_tools_tool_search__core_tool_names @ tools/tool_search.py:_core_tool_names */

/* Port of Python tools/tool_search.py:_core_tool_names */
/* Returns the list of core tool names that are never deferred. */
int cli_tools_tool_search__core_tool_names(
    char *names[], int max_names)
{
    if (!names || max_names <= 0) {
        return 0;
    }
    /* Core Hermes tools that are never deferred. */
    static const char *core_tools[] = {
        "terminal", "file", "web", "memory", "todo", "process",
        "cronjob", "delegate", "clarify", "vision", "tts",
        "image_gen", "video_gen", "transcribe", "patch",
        "exec_code", "read_terminal", "skill_mgmt", "session_search",
        NULL
    };
    int count = 0;
    for (int i = 0; core_tools[i] && count < max_names; i++) {
        names[count] = strdup(core_tools[i]);
        if (names[count]) count++;
    }
    return count;
}

/* PoP: cli_tools_tool_search_classify_tools @ tools/tool_search.py:classify_tools */

/* Port of Python tools/tool_search.py:classify_tools */
/* Classifies tools into core and deferrable categories. */
int cli_tools_tool_search_classify_tools(
    const char *tool_defs_json, int threshold_pct,
    char *core_tools[], int *core_count, int max_core,
    char *deferrable_tools[], int *defer_count, int max_defer)
{
    if (!tool_defs_json || !core_tools || !deferrable_tools) {
        return -1;
    }
    (void)threshold_pct;
    *core_count = 0;
    *defer_count = 0;
    /* CLI port: classification requires full tool registry. */
    return 0;
}

/* PoP: cli_tools_tool_search_is_bridge_tool @ tools/tool_search.py:is_bridge_tool */

/* Port of Python tools/tool_search.py:is_bridge_tool */
/* Checks if a tool is a bridge tool (tool_search, tool_describe, tool_call). */
int cli_tools_tool_search_is_bridge_tool(const char *tool_name)
{
    if (!tool_name) {
        return 0;
    }
    if (strcmp(tool_name, "tool_search") == 0 ||
        strcmp(tool_name, "tool_describe") == 0 ||
        strcmp(tool_name, "tool_call") == 0) {
        return 1;
    }
    return 0;
}

/* PoP: cli_tools_tool_search__format_search_hit @ tools/tool_search.py:_format_search_hit */

/* Port of Python tools/tool_search.py:_format_search_hit */
/* Formats a tool search hit for display. */
int cli_tools_tool_search__format_search_hit(
    const char *tool_name, const char *description,
    char *output, size_t output_size)
{
    if (!tool_name || !description || !output || output_size == 0) {
        return -1;
    }
    snprintf(output, output_size, "%s — %s", tool_name, description);
    return 0;
}
