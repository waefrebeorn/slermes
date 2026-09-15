/* gate: hermes_cli_mcp_startup_py_get_mcp_server_filter */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern char *get_mcp_server_filter(void);

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    WCUP_BEGIN("get_mcp_server_filter");
    WCUP_BRANCH(1);
    char *result = get_mcp_server_filter();
    WCUP_VALSTR(result ? result : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("get_mcp_server_filter");
    return 0;
}
