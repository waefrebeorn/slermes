/* gate: port__hermes_cli_completion_py__generate_bash — traced copy of cpl_generate_bash (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * cpl_generate_bash(const char *parser_tree_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cpl_generate_bash <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cpl_generate_bash");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = cpl_generate_bash(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cpl_generate_bash");
    }
    return 0;
}
