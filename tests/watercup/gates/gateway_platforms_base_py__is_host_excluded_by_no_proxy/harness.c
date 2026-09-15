/* gate: gateway_platforms_base_py__is_host_excluded_by_no_proxy — traced copy of is_host_excluded_by_no_proxy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_host_excluded_by_no_proxy(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("is_host_excluded_by_no_proxy");
    int result = is_host_excluded_by_no_proxy(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_host_excluded_by_no_proxy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_host_excluded_by_no_proxy <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
