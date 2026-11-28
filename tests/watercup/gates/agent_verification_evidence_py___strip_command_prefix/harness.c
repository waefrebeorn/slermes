/* gate: agent_verification_evidence_py___strip_command_prefix
 * Real port vev_u_strip_command_prefix(tokens_json) -> JSON array. Input is a
 * JSON array of tokens; cup the returned JSON array string. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *vev_u_strip_command_prefix(const char *tokens_json);
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <json-array>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("vev_u_strip_command_prefix");
        fflush(stdout);
        char *r = vev_u_strip_command_prefix(argv[i]);
        WCUP_VALSTR(r ? r : "[]");
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("vev_u_strip_command_prefix");
        free(r);
    }
    return 0;
}
