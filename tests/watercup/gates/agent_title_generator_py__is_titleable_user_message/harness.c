/* gate: agent_title_generator_py__is_titleable_user_message — traced copy of title_is_titleable_user_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool title_is_titleable_user_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("title_is_titleable_user_message");
    bool result = title_is_titleable_user_message(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("title_is_titleable_user_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s title_is_titleable_user_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
