/* gate: gateway_relay_ws_transport_py__send_interrupt — traced copy of wst_send_interrupt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int wst_send_interrupt(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("wst_send_interrupt");
    int result = wst_send_interrupt(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("wst_send_interrupt");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wst_send_interrupt <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
