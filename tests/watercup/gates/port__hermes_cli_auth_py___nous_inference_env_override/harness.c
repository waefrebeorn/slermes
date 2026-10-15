/* gate: port__hermes_cli_auth_py___nous_inference_env_override — traced copy of auth_nous_inference_env_override (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * auth_nous_inference_env_override(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_nous_inference_env_override <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_nous_inference_env_override");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = auth_nous_inference_env_override();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_nous_inference_env_override");
    }
    return 0;
}
