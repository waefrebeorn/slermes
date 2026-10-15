/* gate: port__gateway_platforms_signal_rate_limit_py____init__ — traced copy of srl_init (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * srl_init(const char *message, double retry_after);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s srl_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("srl_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = srl_init((int)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("srl_init");
    }
    return 0;
}
