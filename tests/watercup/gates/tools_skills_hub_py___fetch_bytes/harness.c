/* gate: tools_skills_hub_py___fetch_bytes — traced copy of tools_skills_hub_u_fetch_bytes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_skills_hub_u_fetch_bytes(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_skills_hub_u_fetch_bytes");
    int result = tools_skills_hub_u_fetch_bytes(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_skills_hub_u_fetch_bytes");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_skills_hub_u_fetch_bytes <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
