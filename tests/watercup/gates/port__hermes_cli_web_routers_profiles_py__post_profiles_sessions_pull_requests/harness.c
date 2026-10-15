/* gate: port__hermes_cli_web_routers_profiles_py__post_profiles_sessions_pull_requests — traced copy of profiles_post_sessions_pull_requests (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void profiles_post_sessions_pull_requests(char *out_json, size_t out_sz);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s profiles_post_sessions_pull_requests <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("profiles_post_sessions_pull_requests");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    profiles_post_sessions_pull_requests();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("profiles_post_sessions_pull_requests");
    }
    return 0;
}
