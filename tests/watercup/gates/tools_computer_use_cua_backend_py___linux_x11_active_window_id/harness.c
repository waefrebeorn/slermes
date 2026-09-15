/* gate: tools_computer_use_cua_backend_py___linux_x11_active_window_id — traced copy of cua_u_linux_x11_active_window_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cua_u_linux_x11_active_window_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cua_u_linux_x11_active_window_id");
    int result = cua_u_linux_x11_active_window_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cua_u_linux_x11_active_window_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cua_u_linux_x11_active_window_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
