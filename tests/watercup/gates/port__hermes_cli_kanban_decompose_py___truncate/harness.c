/* gate: port__hermes_cli_kanban_decompose_py___truncate — traced copy of kbd_truncate (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * kbd_truncate(const char *text, long limit);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kbd_truncate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kbd_truncate");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = kbd_truncate((int)strtol(argv[i], NULL, 10), (long)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kbd_truncate");
    }
    return 0;
}
