/* gate: agent_title_generator_py___is_real_user_turn — traced copy of title_is_real_user_turn */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int title_is_real_user_turn(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("title_is_real_user_turn");
    int result = title_is_real_user_turn(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("title_is_real_user_turn");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s title_is_real_user_turn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
