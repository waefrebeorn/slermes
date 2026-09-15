/* gate: agent_proxy_sources_iron_proxy_py__write_mappings — traced copy of ipx_write_mappings */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ipx_write_mappings(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("ipx_write_mappings");
    int result = ipx_write_mappings(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ipx_write_mappings");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ipx_write_mappings <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
