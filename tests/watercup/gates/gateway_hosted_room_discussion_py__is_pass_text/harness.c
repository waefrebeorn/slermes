/* gate: gateway_hosted_room_discussion_py__is_pass_text — traced copy of is_pass_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_pass_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_pass_text");
    int result = is_pass_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_pass_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_pass_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
