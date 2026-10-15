/* gate: port__gateway_run_py___voice_key_for_source — traced copy of _voice_key_for_source (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _voice_key_for_source(const char *source);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _voice_key_for_source <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_voice_key_for_source");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _voice_key_for_source(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_voice_key_for_source");
    }
    return 0;
}
