/* gate: port__tools_voice_mode_py___import_audio — traced copy of vmd_import_audio (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * vmd_import_audio(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s vmd_import_audio <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("vmd_import_audio");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = vmd_import_audio();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("vmd_import_audio");
    }
    return 0;
}
