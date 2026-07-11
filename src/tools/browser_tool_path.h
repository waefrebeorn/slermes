#ifndef SLERMES_BROWSER_TOOL_PATH_H
#define SLERMES_BROWSER_TOOL_PATH_H

#include <stdbool.h>
#include <stdio.h>
#include <json.h>

typedef struct browser_tool_path browser_tool_path_t;

browser_tool_path_t *browser_tool_path_init(void);
void browser_tool_path_cleanup(browser_tool_path_t *s);

char *browser_discover_homebrew_node_dirs(void);
char *browser_browser_candidate_path_dirs(void);
char *browser_merge_browser_path(const char *existing_path);

#endif /* SLERMES_BROWSER_TOOL_PATH_H */
