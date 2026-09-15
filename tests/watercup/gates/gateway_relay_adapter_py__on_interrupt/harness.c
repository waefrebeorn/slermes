/* gate: gateway_relay_adapter_py__on_interrupt — traced copy of rla_on_interrupt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int rla_on_interrupt(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("rla_on_interrupt");
    int result = rla_on_interrupt(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("rla_on_interrupt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rla_on_interrupt <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
