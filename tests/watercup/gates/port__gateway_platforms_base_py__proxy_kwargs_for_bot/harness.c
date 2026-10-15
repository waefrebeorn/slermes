/* gate: port__gateway_platforms_base_py__proxy_kwargs_for_bot — traced copy of proxy_kwargs_for_bot (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * proxy_kwargs_for_bot(const char *proxy_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s proxy_kwargs_for_bot <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("proxy_kwargs_for_bot");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = proxy_kwargs_for_bot(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("proxy_kwargs_for_bot");
    }
    return 0;
}
