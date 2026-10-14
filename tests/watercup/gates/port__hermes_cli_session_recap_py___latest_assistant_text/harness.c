/* gate: port__hermes_cli_session_recap_py___latest_assistant_text — traced copy of session_latest_assistant_text (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int session_latest_assistant_text(const json_t *messages, char *out, size_t out_cap);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_latest_assistant_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("session_latest_assistant_text");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = session_latest_assistant_text(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("session_latest_assistant_text");
    }
    return 0;
}
