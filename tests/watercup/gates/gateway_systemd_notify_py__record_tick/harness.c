/* gate: gateway_systemd_notify_py__record_tick — traced copy of gateway_systemd_notify_record_tick */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_systemd_notify_record_tick(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_systemd_notify_record_tick");
    int result = gateway_systemd_notify_record_tick(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_systemd_notify_record_tick");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_systemd_notify_record_tick <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
