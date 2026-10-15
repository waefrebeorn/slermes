/* gate: port__hermes_cli_session_export_html_py___format_timestamp — traced copy of hcli_session_export_html_ts (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * hcli_session_export_html_ts(double ts);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hcli_session_export_html_ts <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("hcli_session_export_html_ts");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = hcli_session_export_html_ts(strtod(argv[i], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("hcli_session_export_html_ts");
    }
    return 0;
}
