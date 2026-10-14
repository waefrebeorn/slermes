/* gate: port__hermes_cli_doctor_live_py___launch_browser_probe — traced copy of doctor_launch_browser_probe (outparam) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int doctor_launch_browser_probe(int timeout_sec, char *out_detail, size_t out_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s doctor_launch_browser_probe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("doctor_launch_browser_probe");
    char _ob[4096]; _ob[0] = '\0';
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = doctor_launch_browser_probe(argv[i], _ob, sizeof(_ob));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(_ob);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("doctor_launch_browser_probe");
    }
    return 0;
}
