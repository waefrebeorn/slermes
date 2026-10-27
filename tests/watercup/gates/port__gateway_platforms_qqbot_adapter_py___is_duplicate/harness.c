/* gate: port__gateway_platforms_qqbot_adapter_py___is_duplicate — traced copy of helpers_is_duplicate (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int helpers_is_duplicate(const char *msg_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s helpers_is_duplicate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("helpers_is_duplicate");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = helpers_is_duplicate(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("helpers_is_duplicate");
    }
    return 0;
}
