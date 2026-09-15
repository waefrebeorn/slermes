/* gate: hermes_cli_dingtalk_auth_py___ensure_qrcode_installed — traced copy of hermes_cli_dingtalk_auth_u_ensure_qrcode_installed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_dingtalk_auth_u_ensure_qrcode_installed");
    int result = hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_dingtalk_auth_u_ensure_qrcode_installed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_dingtalk_auth_u_ensure_qrcode_installed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
