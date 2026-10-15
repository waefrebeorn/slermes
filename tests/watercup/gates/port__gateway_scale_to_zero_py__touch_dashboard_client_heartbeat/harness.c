/* gate: port__gateway_scale_to_zero_py__touch_dashboard_client_heartbeat — traced copy of touch_dashboard_client_heartbeat (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * touch_dashboard_client_heartbeat(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s touch_dashboard_client_heartbeat <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("touch_dashboard_client_heartbeat");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = touch_dashboard_client_heartbeat();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("touch_dashboard_client_heartbeat");
    }
    return 0;
}
