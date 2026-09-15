/* gate: port__gateway_control_socket_py__start — traced copy of control_socket_server_start */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern int control_socket_server_start(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("control_socket_server_start");
    int result = control_socket_server_start(input);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("control_socket_server_start");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s control_socket_server_start <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
