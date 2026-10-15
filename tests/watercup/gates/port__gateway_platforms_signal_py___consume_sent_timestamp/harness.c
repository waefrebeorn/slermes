/* gate: port__gateway_platforms_signal_py___consume_sent_timestamp — traced copy of sig_consume_sent_timestamp (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool sig_consume_sent_timestamp(const char *cache_json, const char *ts);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sig_consume_sent_timestamp <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sig_consume_sent_timestamp");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = sig_consume_sent_timestamp(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sig_consume_sent_timestamp");
    }
    return 0;
}
