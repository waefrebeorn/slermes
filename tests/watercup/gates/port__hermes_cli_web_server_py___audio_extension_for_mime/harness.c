/* gate: port__hermes_cli_web_server_py___audio_extension_for_mime — traced copy of audio_extension_for_mime (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * audio_extension_for_mime(const char *mime_type);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s audio_extension_for_mime <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("audio_extension_for_mime");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = audio_extension_for_mime(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("audio_extension_for_mime");
    }
    return 0;
}
