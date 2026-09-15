/* gate: gateway_platforms_yuanbao_py__is_table_atom — traced copy of yuanbao_md_is_table_atom */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool yuanbao_md_is_table_atom(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yuanbao_md_is_table_atom");
    bool result = yuanbao_md_is_table_atom(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yuanbao_md_is_table_atom");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yuanbao_md_is_table_atom <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
