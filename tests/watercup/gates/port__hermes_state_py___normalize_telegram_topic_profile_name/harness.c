/* gate: port__hermes_state_py___normalize_telegram_topic_profile_name — traced copy of _normalize_telegram_topic_profile_name (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _normalize_telegram_topic_profile_name(const char *profile_name);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _normalize_telegram_topic_profile_name <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_normalize_telegram_topic_profile_name");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _normalize_telegram_topic_profile_name(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_normalize_telegram_topic_profile_name");
    }
    return 0;
}
