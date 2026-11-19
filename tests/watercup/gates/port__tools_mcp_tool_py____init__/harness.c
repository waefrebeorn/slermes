/* gate: port__tools_mcp_tool_py____init__ — traced copy of mct_server_task_init (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int mct_server_task_init(const char *server_name, long max_rpm, double timeout);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_server_task_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("mct_server_task_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = mct_server_task_init(argv[i], (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), strtod(argv[2 + (i - 2 + 2) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("mct_server_task_init");
    }
    return 0;
}
