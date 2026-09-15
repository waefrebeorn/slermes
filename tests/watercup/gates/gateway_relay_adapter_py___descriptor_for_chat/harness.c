/* gate: gateway_relay_adapter_py___descriptor_for_chat — traced copy of descriptor_max_len_for_chat */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int descriptor_max_len_for_chat(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("descriptor_max_len_for_chat");
    int result = descriptor_max_len_for_chat(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("descriptor_max_len_for_chat");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s descriptor_max_len_for_chat <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
