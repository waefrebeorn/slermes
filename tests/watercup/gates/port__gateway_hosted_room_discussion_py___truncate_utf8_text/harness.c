/* gate: port__gateway_hosted_room_discussion_py___truncate_utf8_text — traced copy of _truncate_utf8_text (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _truncate_utf8_text(const char *text, int max_bytes);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _truncate_utf8_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_truncate_utf8_text");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _truncate_utf8_text(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_truncate_utf8_text");
    }
    return 0;
}
