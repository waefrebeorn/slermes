#ifndef AGENT_TOOL_DISPATCH_HELPERS_H
#define AGENT_TOOL_DISPATCH_HELPERS_H
#include <stdbool.h>
char *agent_tool_dispatch_neutralize_delimiters(const char *content);
char **agent_tool_dispatch_extract_landed_file_mutation_paths(const char *tool_name,
    const char *args_json, const char *result, int *out_n);
#endif
