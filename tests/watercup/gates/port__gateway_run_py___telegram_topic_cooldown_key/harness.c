/* gate: port__gateway_run_py___telegram_topic_cooldown_key — traced copy of _telegram_topic_cooldown_key (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _telegram_topic_cooldown_key(const char *profile_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _telegram_topic_cooldown_key <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_telegram_topic_cooldown_key");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _telegram_topic_cooldown_key(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_telegram_topic_cooldown_key");
    }
    return 0;
}
