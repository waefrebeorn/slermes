/* gate: port__hermes_cli_web_routers_tools_py__select_terminal_backend — traced copy of select_terminal_backend (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void select_terminal_backend(char *backend, size_t sz,
                                    char *cpu, size_t cpu_sz,
                                    char *mem, size_t mem_sz,
                                    char *disk, size_t disk_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s select_terminal_backend <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("select_terminal_backend");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    select_terminal_backend(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5], argv[2 + (i - 2 + 4) % 5], argv[2 + (i - 2 + 5) % 5], argv[2 + (i - 2 + 6) % 5], argv[2 + (i - 2 + 7) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("select_terminal_backend");
    }
    return 0;
}
