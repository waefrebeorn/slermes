/* gate: port__hermes_cli_setup_py___setup_tts_provider — traced copy of _setup_tts_provider (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int _setup_tts_provider(const char *config_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _setup_tts_provider <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_setup_tts_provider");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = _setup_tts_provider(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_setup_tts_provider");
    }
    return 0;
}
