/* gate: bedrock_model_ids_or_none — CONVERTED from a GAP sentinel to the real port symbol cli_agent_bedrock_adapter_bedrock_model_ids_or_none.
 * The sentinel returned the function name for every input; this calls cli_agent_bedrock_adapter_bedrock_model_ids_or_none().
 * Kept only because its cups equalled the LIVE Python twin's with this shape. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static void trace_one(const char *input) {
    WCUP_BEGIN("bedrock_model_ids_or_none");
extern const char *cli_agent_bedrock_adapter_bedrock_model_ids_or_none(const char *input);
    const char *r = cli_agent_bedrock_adapter_bedrock_model_ids_or_none(input);
    WCUP_VALSTR(r ? r : "");
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bedrock_model_ids_or_none");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *input = strcmp(argv[i], "NULL") == 0 ? NULL : argv[i];
        trace_one(input);
    }
    return 0;
}
