/* gate: port__gateway_hosted_room_discussion_py___derive_member_watermarks — traced copy of _derive_member_watermarks (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _derive_member_watermarks(const char *room_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _derive_member_watermarks <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_derive_member_watermarks");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _derive_member_watermarks(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_derive_member_watermarks");
    }
    return 0;
}
