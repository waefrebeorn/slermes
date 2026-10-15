/* gate: port__gateway_platforms_signal_py___quote_references_own_message — traced copy of sig_quote_references_own_message (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool sig_quote_references_own_message(const char *reply_to_id, const char *sent_timestamps_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sig_quote_references_own_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sig_quote_references_own_message");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = sig_quote_references_own_message(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sig_quote_references_own_message");
    }
    return 0;
}
