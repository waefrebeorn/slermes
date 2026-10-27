/* gate: port__gateway_scale_to_zero_py__is_idle — traced copy of scale_to_zero_is_idle (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int scale_to_zero_is_idle(int running_agent_count, double seconds_since_last_inbound,
                          double idle_timeout_seconds, int has_live_background_work);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s scale_to_zero_is_idle <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("scale_to_zero_is_idle");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = scale_to_zero_is_idle((int)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL), strtod(argv[2 + (i - 2 + 2) % 5], NULL), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("scale_to_zero_is_idle");
    }
    return 0;
}
