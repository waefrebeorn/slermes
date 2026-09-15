/* gate: tools_skill_usage_py__mark_agent_created — traced copy of su_mark_agent_created */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int su_mark_agent_created(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("su_mark_agent_created");
    int result = su_mark_agent_created(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("su_mark_agent_created");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_mark_agent_created <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
