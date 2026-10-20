/* gate: port__hermes_cli_models_py__fetch_ai_gateway_models — traced copy of fetch_ai_gateway_models (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * fetch_ai_gateway_models(double timeout_sec, bool force_refresh);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s fetch_ai_gateway_models <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("fetch_ai_gateway_models");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = fetch_ai_gateway_models(strtod(argv[i], NULL), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("fetch_ai_gateway_models");
    }
    return 0;
}
