/* gate: gateway_relay_transport_py__send_interrupt — traced copy of rtr_send_interrupt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int rtr_send_interrupt(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("rtr_send_interrupt");
    int result = rtr_send_interrupt(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("rtr_send_interrupt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rtr_send_interrupt <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
