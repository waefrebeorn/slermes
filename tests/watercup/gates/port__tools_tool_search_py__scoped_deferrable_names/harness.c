/* gate: port__tools_tool_search_py__scoped_deferrable_names — traced copy of ts_scoped_deferrable_names (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * ts_scoped_deferrable_names(const char *tool_defs_json, const char *deferrable_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ts_scoped_deferrable_names <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ts_scoped_deferrable_names");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = ts_scoped_deferrable_names(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ts_scoped_deferrable_names");
    }
    return 0;
}
