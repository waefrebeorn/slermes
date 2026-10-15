/* gate: port__hermes_cli__parser_py__top_level_value_flag_sets — traced copy of top_level_value_flag_sets (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * top_level_value_flag_sets(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s top_level_value_flag_sets <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("top_level_value_flag_sets");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = top_level_value_flag_sets();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("top_level_value_flag_sets");
    }
    return 0;
}
