/* gate: port__agent_auxiliary_client_py__resolve_compression_fast_lane — traced copy of resolve_compression_fast_lane (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int resolve_compression_fast_lane(const char *config);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_compression_fast_lane <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("resolve_compression_fast_lane");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = resolve_compression_fast_lane(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_compression_fast_lane");
    }
    return 0;
}
