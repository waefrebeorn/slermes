/* gate: port__hermes_cli_web_server_py___notify_cron_provider_for_profile — traced copy of _notify_cron_provider_for_profile (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _notify_cron_provider_for_profile(const char *profile, const char *event);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _notify_cron_provider_for_profile <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_notify_cron_provider_for_profile");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _notify_cron_provider_for_profile(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_notify_cron_provider_for_profile");
    }
    return 0;
}
