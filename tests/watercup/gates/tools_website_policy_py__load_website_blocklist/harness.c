/* gate: tools_website_policy_py__load_website_blocklist — traced copy of cli_tools_website_policy_load_website_blocklist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_tools_website_policy_load_website_blocklist(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_tools_website_policy_load_website_blocklist");
    int result = cli_tools_website_policy_load_website_blocklist(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_tools_website_policy_load_website_blocklist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_website_policy_load_website_blocklist <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
