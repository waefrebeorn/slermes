/* gate: port__tools_tool_search_py__estimate_tokens_from_schemas — traced copy of ts_estimate_tokens_from_schemas (long_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern long ts_estimate_tokens_from_schemas(const char *tool_defs_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ts_estimate_tokens_from_schemas <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ts_estimate_tokens_from_schemas");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    long result = ts_estimate_tokens_from_schemas(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ts_estimate_tokens_from_schemas");
    }
    return 0;
}
