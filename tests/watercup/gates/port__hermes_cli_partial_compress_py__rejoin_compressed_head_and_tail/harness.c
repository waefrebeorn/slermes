/* gate: port__hermes_cli_partial_compress_py__rejoin_compressed_head_and_tail — traced copy of rejoin_compressed_head_and_tail (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * rejoin_compressed_head_and_tail(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rejoin_compressed_head_and_tail <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("rejoin_compressed_head_and_tail");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = rejoin_compressed_head_and_tail();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("rejoin_compressed_head_and_tail");
    }
    return 0;
}
