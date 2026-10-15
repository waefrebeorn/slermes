/* gate: port__tools_memory_tool_py___scan_memory_content — traced copy of memory_tool_scan_memory_content (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * memory_tool_scan_memory_content(const char *content);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s memory_tool_scan_memory_content <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("memory_tool_scan_memory_content");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = memory_tool_scan_memory_content(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("memory_tool_scan_memory_content");
    }
    return 0;
}
