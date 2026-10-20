/* gate: port__tools_memory_tool_py___read_entries_checked — traced copy of memory_store_read_entries_checked (scalar_outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char ** memory_store_read_entries_checked(const char *path, int *out_n, int *read_ok);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s memory_store_read_entries_checked <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("memory_store_read_entries_checked");
    int _o1 = 0;
    int _o2 = 0;
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char ** result = memory_store_read_entries_checked(argv[i], &_o1, &_o2);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o1); WCUP_VALSTR(_b); }
    { char _b[32]; snprintf(_b, sizeof(_b), "%d", (int)_o2); WCUP_VALSTR(_b); }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("memory_store_read_entries_checked");
    }
    return 0;
}
