/* gate: tools_url_safety_py___allows_private_ip_resolution — traced copy of cli_tools_url_safety__allows_private_ip_resolution */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cli_tools_url_safety__allows_private_ip_resolution(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("cli_tools_url_safety__allows_private_ip_resolution");
    int result = cli_tools_url_safety__allows_private_ip_resolution(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_tools_url_safety__allows_private_ip_resolution");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_tools_url_safety__allows_private_ip_resolution <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
