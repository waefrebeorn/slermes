/* gate: port__hermes_cli_observability_shared_metrics_py__record_client_active — traced copy of metrics_record_client_active (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int metrics_record_client_active(const char *resource_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s metrics_record_client_active <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("metrics_record_client_active");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = metrics_record_client_active(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("metrics_record_client_active");
    }
    return 0;
}
