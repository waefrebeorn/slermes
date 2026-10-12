/* gate: real__agent_monitoring_otlp_exporter_py___span_attrs — GAP sentinel for _span_attrs */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_span_attrs";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _span_attrs <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_span_attrs");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_span_attrs");
    }
    return 0;
}
