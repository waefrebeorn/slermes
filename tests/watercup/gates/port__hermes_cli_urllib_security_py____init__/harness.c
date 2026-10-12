/* gate: port__hermes_cli_urllib_security_py____init__ — traced copy of hcli_cors_sanitizer_init (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int hcli_cors_sanitizer_init(const char *original_url);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_cors_sanitizer_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hcli_cors_sanitizer_init");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = hcli_cors_sanitizer_init(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hcli_cors_sanitizer_init");
    }
    return 0;
}
