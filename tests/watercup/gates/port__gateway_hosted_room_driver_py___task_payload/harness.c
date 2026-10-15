/* gate: port__gateway_hosted_room_driver_py___task_payload — traced copy of _task_payload (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _task_payload(const char *payload_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _task_payload <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_task_payload");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _task_payload(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_task_payload");
    }
    return 0;
}
