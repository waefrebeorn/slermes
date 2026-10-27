/* gate: port__cron_scheduler_py__drain_delivery_queue — traced copy of drain_delivery_queue (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void drain_delivery_queue(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s drain_delivery_queue <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("drain_delivery_queue");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    drain_delivery_queue();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("drain_delivery_queue");
    }
    return 0;
}
