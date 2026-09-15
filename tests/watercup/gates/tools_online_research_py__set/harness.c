/* gate: tools_online_research_py__set — traced copy of onr_set */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int onr_set(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("onr_set");
    int result = onr_set(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("onr_set");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s onr_set <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
