/* gate: hermes_cli_session_export_md_py___frontmatter_line — traced copy of sexmd_u_frontmatter_line */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sexmd_u_frontmatter_line(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sexmd_u_frontmatter_line");
    int result = sexmd_u_frontmatter_line(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sexmd_u_frontmatter_line");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sexmd_u_frontmatter_line <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
