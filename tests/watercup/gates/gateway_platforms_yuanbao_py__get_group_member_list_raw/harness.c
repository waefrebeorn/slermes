/* gate: gateway_platforms_yuanbao_py__get_group_member_list_raw — traced copy of yb_get_group_member_list_raw */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int yb_get_group_member_list_raw(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("yb_get_group_member_list_raw");
    int result = yb_get_group_member_list_raw(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("yb_get_group_member_list_raw");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s yb_get_group_member_list_raw <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
