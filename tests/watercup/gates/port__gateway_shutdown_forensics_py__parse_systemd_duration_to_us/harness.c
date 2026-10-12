/* gate: port__gateway_shutdown_forensics_py__parse_systemd_duration_to_us — traced copy of parse_systemd_duration_to_us (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern long parse_systemd_duration_to_us(const char *raw);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_systemd_duration_to_us <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("parse_systemd_duration_to_us");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = parse_systemd_duration_to_us((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_systemd_duration_to_us");
    }
    return 0;
}
