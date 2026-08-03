/*
 * port_tools_todo_tool.c — C port of tools/todo_tool.py
 *
 * Todo Tool Module - Planning & Task Management
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_TODO_CONTENT_CHARS 4000
#define MAX_TODO_ITEMS 256
#define MAX_ITEMS 256
#define MAX_ID_LEN 64
#define MAX_CONTENT_LEN 4096

typedef struct {
    char id[MAX_ID_LEN];
    char content[MAX_CONTENT_LEN];
    char status[16];
} TodoItem;

static TodoItem *_items = NULL;   /* heap-backed: no 1MB landlocked static */
static int _item_count = 0;

/* PoP: todo_tool_check_requirements @ tools/todo_tool.py:check_todo_requirements */
/* Todo tool has no external requirements — always available. */
bool todo_tool_check_requirements(void) {
    return true;
}

/* PoP: todo_tool_has_items @ tools/todo_tool.py:has_items */

/* Port of Python tools/todo_tool.py:has_items */
/* Check if there are any items in the list. */
int todo_tool_has_items(void)
{
    return _item_count > 0;
}

/* PoP: todo_tool_format_for_injection @ tools/todo_tool.py:format_for_injection */

/* Port of Python tools/todo_tool.py:format_for_injection */
/* Render the todo list for post-compression injection. Returns NULL if empty. */
char *todo_tool_format_for_injection(void)
{
    if (_item_count == 0 || !_items) return NULL;

    /* Count active items */
    int active = 0;
    for (int i = 0; i < _item_count; i++) {
        if (strcmp(_items[i].status, "pending") == 0 ||
            strcmp(_items[i].status, "in_progress") == 0) {
            active++;
        }
    }
    if (active == 0) return NULL;

    /* Build output string */
    char *result = (char *)malloc(active * MAX_CONTENT_LEN + 256);
    if (!result) return NULL;

    strcpy(result, "[Your active task list was preserved across context compression]\n");
    for (int i = 0; i < _item_count; i++) {
        if (strcmp(_items[i].status, "pending") != 0 &&
            strcmp(_items[i].status, "in_progress") != 0) continue;

        const char *marker = "[?]";
        if (strcmp(_items[i].status, "completed") == 0) marker = "[x]";
        else if (strcmp(_items[i].status, "in_progress") == 0) marker = "[>]";
        else if (strcmp(_items[i].status, "pending") == 0) marker = "[ ]";
        else if (strcmp(_items[i].status, "cancelled") == 0) marker = "[~]";

        char line[MAX_CONTENT_LEN + 128];
        snprintf(line, sizeof(line), "- %s %s. %s (%s)\n",
                 marker, _items[i].id, _items[i].content, _items[i].status);
        strcat(result, line);
    }

    hermes_log(LOG_DEBUG, "todo_tool", "Formatted %d active items for injection", active);
    return result;
}

/* PoP: todo_tool__cap_content @ tools/todo_tool.py:_cap_content */

/* Port of Python tools/todo_tool.py:_cap_content */
/* Truncate oversized todo content to MAX_TODO_CONTENT_CHARS. */
char *todo_tool__cap_content(const char *content)
{
    if (!content) return strdup("");

    size_t len = strlen(content);
    if (len <= MAX_TODO_CONTENT_CHARS) return strdup(content);

    const char *marker = "… [truncated]";
    size_t keep = MAX_TODO_CONTENT_CHARS - strlen(marker);
    char *result = (char *)malloc(MAX_TODO_CONTENT_CHARS + 1);
    if (!result) return strdup("");

    memcpy(result, content, keep);
    result[keep] = '\0';
    strcat(result, marker);
    hermes_log(LOG_DEBUG, "todo_tool", "Truncated content from %zu to %d chars", len, MAX_TODO_CONTENT_CHARS);
    return result;
}

/* PoP: todo_tool__dedupe_by_id @ tools/todo_tool.py:_dedupe_by_id */

/* Port of Python tools/todo_tool.py:_dedupe_by_id */
/* Remove duplicate items by ID, keeping the first occurrence. Returns new count. */
int todo_tool__dedupe_by_id(TodoItem *items, int count)
{
    if (count <= 0) return 0;

    int write = 0;
    for (int i = 0; i < count; i++) {
        int found = 0;
        for (int j = 0; j < write; j++) {
            if (strcmp(items[i].id, items[j].id) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            if (write != i) items[write] = items[i];
            write++;
        }
    }
    hermes_log(LOG_DEBUG, "todo_tool", "Deduped %d items to %d", count, write);
    return write;
}


