/* gate: gateway_platforms_feishu_meeting_invite_py___int_field — traced copy of cli_gateway_platforms_feishu_meeting_invite__int_field */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_gateway_platforms_feishu_meeting_invite__int_field(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_gateway_platforms_feishu_meeting_invite__int_field");
    int result = cli_gateway_platforms_feishu_meeting_invite__int_field(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_gateway_platforms_feishu_meeting_invite__int_field");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_gateway_platforms_feishu_meeting_invite__int_field <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
