#ifndef TOOLS_FILE_OPERATIONS_SEARCH_H
#define TOOLS_FILE_OPERATIONS_SEARCH_H
#include <stdbool.h>
typedef struct {
    int exit_code;
    char *stdout;
    char *stderr;
} file_ops_execute_result_t;
char *file_ops_search_search_stdout_and_limit(const file_ops_execute_result_t *result, char **out_reason);
void file_ops_search_split_tool_diagnostics(const char *output, char **out_diagnostics, char **out_payload);
#endif
