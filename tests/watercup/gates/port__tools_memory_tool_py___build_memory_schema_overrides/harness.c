/* gate: port__tools_memory_tool_py___build_memory_schema_overrides — traced copy of memory_tool_build_memory_schema_overrides (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * memory_tool_build_memory_schema_overrides(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s memory_tool_build_memory_schema_overrides <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("memory_tool_build_memory_schema_overrides");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = memory_tool_build_memory_schema_overrides();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("memory_tool_build_memory_schema_overrides");
    }
    return 0;
}
