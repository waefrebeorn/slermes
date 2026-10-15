/* gate: port__hermes_cli_nous_billing_py___absolutize_portal_url — traced copy of nb2_absolutize_portal_url (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * nb2_absolutize_portal_url(const char *portal_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s nb2_absolutize_portal_url <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("nb2_absolutize_portal_url");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = nb2_absolutize_portal_url(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("nb2_absolutize_portal_url");
    }
    return 0;
}
