/* gate: port__hermes_cli_main_py___read_cgroup_memory_limit — traced copy of _read_cgroup_memory_limit (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern long _read_cgroup_memory_limit(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _read_cgroup_memory_limit <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_read_cgroup_memory_limit");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = _read_cgroup_memory_limit((int)strtol(argv[i], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_read_cgroup_memory_limit");
    }
    return 0;
}
