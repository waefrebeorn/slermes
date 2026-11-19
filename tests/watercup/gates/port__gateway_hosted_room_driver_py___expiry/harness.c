/* gate: port__gateway_hosted_room_driver_py___expiry — traced copy of _expiry (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern float _expiry(float now, float ttl);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _expiry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_expiry");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    float result = _expiry(strtod(argv[i], NULL), strtod(argv[2 + (i - 2 + 1) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_expiry");
    }
    return 0;
}
