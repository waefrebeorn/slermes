/* gate: port__gateway_hosted_room_driver_py___lease_from_row — traced copy of _lease_from_row (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _lease_from_row(const char *room_id, const char *gateway_id, int authority_epoch,
        const char *process_generation, int lease_generation, float expires_at);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _lease_from_row <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_lease_from_row");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _lease_from_row((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 4) % 5], NULL, 10), strtod(argv[2 + (i - 2 + 5) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_lease_from_row");
    }
    return 0;
}
