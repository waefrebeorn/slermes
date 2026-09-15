/* gate: gateway_platforms_yuanbao_py__query_session_members — traced copy of yb_query_session_members */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_query_session_members(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_query_session_members");
    int result = yb_query_session_members(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_query_session_members");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_query_session_members <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
