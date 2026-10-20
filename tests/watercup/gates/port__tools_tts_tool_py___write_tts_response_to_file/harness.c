/* gate: port__tools_tts_tool_py___write_tts_response_to_file — traced copy of tts_write_tts_response_to_file (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int tts_write_tts_response_to_file(const void *response, const char *output_path,
                                   const char *label, size_t limit);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tts_write_tts_response_to_file <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("tts_write_tts_response_to_file");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = tts_write_tts_response_to_file(argv[i], argv[2 + (i - 2 + 1) % 5], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("tts_write_tts_response_to_file");
    }
    return 0;
}
