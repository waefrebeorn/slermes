/* gate: port__tools_transcription_tools_py___apply_pre_transcription_hook — traced copy of _apply_pre_transcription_hook (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _apply_pre_transcription_hook(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _apply_pre_transcription_hook <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_apply_pre_transcription_hook");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _apply_pre_transcription_hook(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_apply_pre_transcription_hook");
    }
    return 0;
}
