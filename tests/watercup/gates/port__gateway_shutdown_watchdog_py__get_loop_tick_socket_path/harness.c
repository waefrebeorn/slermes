/* gate: port__gateway_shutdown_watchdog_py__get_loop_tick_socket_path — traced copy of get_loop_tick_socket_path (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * get_loop_tick_socket_path(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s get_loop_tick_socket_path <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("get_loop_tick_socket_path");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = get_loop_tick_socket_path();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("get_loop_tick_socket_path");
    }
    return 0;
}
