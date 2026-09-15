/* gate: tools_skill_usage_py__is_protected_builtin — traced copy of su_is_protected_builtin */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool su_is_protected_builtin(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("su_is_protected_builtin");
    bool result = su_is_protected_builtin(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("su_is_protected_builtin");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s su_is_protected_builtin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
