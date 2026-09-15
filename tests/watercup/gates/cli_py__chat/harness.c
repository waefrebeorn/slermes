/* gate: cli_py__chat — traced copy of cl2_chat */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cl2_chat(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cl2_chat");
    int result = cl2_chat(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cl2_chat");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cl2_chat <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
