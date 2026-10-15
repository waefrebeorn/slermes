/* gate: port__tools_tts_tool_py___repair_ogg_container — traced copy of tts_repair_ogg_container (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * tts_repair_ogg_container(const char *file_str);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_repair_ogg_container <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_repair_ogg_container");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = tts_repair_ogg_container(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_repair_ogg_container");
    }
    return 0;
}
