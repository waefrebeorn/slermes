/* gate: port__hermes_cli_banner_py___is_official_ssh_remote — traced copy of banner_is_official_ssh_remote (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool banner_is_official_ssh_remote(const char *url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s banner_is_official_ssh_remote <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("banner_is_official_ssh_remote");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = banner_is_official_ssh_remote(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("banner_is_official_ssh_remote");
    }
    return 0;
}
