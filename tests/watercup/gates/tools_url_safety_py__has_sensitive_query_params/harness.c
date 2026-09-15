/* gate: tools_url_safety_py__has_sensitive_query_params — traced copy of has_sensitive_query_params */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool has_sensitive_query_params(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("has_sensitive_query_params");
    bool result = has_sensitive_query_params(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("has_sensitive_query_params");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s has_sensitive_query_params <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
