/* gate: port__gateway_shutdown_forensics_py___signal_name — traced copy of forensics_signal_name (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * forensics_signal_name(int sig, char *buf, size_t buf_size);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s forensics_signal_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("forensics_signal_name");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = forensics_signal_name((int)strtol(argv[i], NULL, 10), argv[2 + (i - 2 + 1) % 5], (long)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("forensics_signal_name");
    }
    return 0;
}
