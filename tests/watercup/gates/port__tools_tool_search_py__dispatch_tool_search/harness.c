/* gate: port__tools_tool_search_py__dispatch_tool_search — traced copy of ts_dispatch_tool_search (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * ts_dispatch_tool_search(const char *args_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ts_dispatch_tool_search <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ts_dispatch_tool_search");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = ts_dispatch_tool_search(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ts_dispatch_tool_search");
    }
    return 0;
}
