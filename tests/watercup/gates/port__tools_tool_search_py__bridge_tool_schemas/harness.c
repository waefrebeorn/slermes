/* gate: port__tools_tool_search_py__bridge_tool_schemas — traced copy of ts_bridge_tool_schemas (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * ts_bridge_tool_schemas(const char *deferred_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ts_bridge_tool_schemas <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ts_bridge_tool_schemas");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = ts_bridge_tool_schemas(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ts_bridge_tool_schemas");
    }
    return 0;
}
