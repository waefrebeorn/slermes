/* gate: port__gateway_stream_consumer_py__ensure_closed_code_fences — traced copy of sc_ensure_closed_code_fences (bool_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool sc_ensure_closed_code_fences(const char *text, const char *sent_text);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sc_ensure_closed_code_fences <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("sc_ensure_closed_code_fences");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = sc_ensure_closed_code_fences(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("sc_ensure_closed_code_fences");
    }
    return 0;
}
