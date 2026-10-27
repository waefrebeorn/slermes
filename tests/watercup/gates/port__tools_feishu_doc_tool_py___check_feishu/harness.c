/* gate: port__tools_feishu_doc_tool_py___check_feishu — traced copy of fdt2_check_feishu (bool_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool fdt2_check_feishu(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fdt2_check_feishu <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fdt2_check_feishu");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = fdt2_check_feishu();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fdt2_check_feishu");
    }
    return 0;
}
