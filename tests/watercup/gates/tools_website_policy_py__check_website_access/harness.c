/* gate: tools_website_policy_py__check_website_access — traced copy of tools_website_policy_check_website_access */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_website_policy_check_website_access(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_website_policy_check_website_access");
    int result = tools_website_policy_check_website_access(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_website_policy_check_website_access");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_website_policy_check_website_access <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
