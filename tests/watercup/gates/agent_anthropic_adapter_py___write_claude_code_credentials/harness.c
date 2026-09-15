/* gate: agent_anthropic_adapter_py___write_claude_code_credentials — traced copy of antr_write_claude_code_credentials */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int antr_write_claude_code_credentials(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("antr_write_claude_code_credentials");
    int result = antr_write_claude_code_credentials(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("antr_write_claude_code_credentials");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s antr_write_claude_code_credentials <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
