/* gate: port__hermes_cli_web_server_py___elevenlabs_voice_label — traced copy of web_elevenlabs_voice_label (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * web_elevenlabs_voice_label(const char *voice_id, const char *voice_name,
                                  const char *category, const char *description);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s web_elevenlabs_voice_label <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("web_elevenlabs_voice_label");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = web_elevenlabs_voice_label(argv[i], argv[2 + (i - 2 + 1) % 5], argv[2 + (i - 2 + 2) % 5], argv[2 + (i - 2 + 3) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("web_elevenlabs_voice_label");
    }
    return 0;
}
