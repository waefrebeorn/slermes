/* gate: tools_write_approval_py___prompt_inline_memory_approval — traced copy of cli_tools_write_approval__prompt_inline_memory_approval */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_tools_write_approval__prompt_inline_memory_approval(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_tools_write_approval__prompt_inline_memory_approval");
    int result = cli_tools_write_approval__prompt_inline_memory_approval(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_tools_write_approval__prompt_inline_memory_approval");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_write_approval__prompt_inline_memory_approval <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
