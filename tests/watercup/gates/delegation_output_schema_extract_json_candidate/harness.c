/* gate: delegation_output_schema_extract_json_candidate — traced copy of
 * src/tools/delegate_tool.c::delegation_extract_json_candidate
 * (PoP: tools/delegation_output_schema.py:extract_json_candidate).
 *
 * Rungs: fence-start? -> newline-found -> tail-fence -> json-prefix ->
 *        per-opener ({,[): startswith? / find-span? — exact candidate string.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static void extract_json_candidate(const char *text) {
    WCUP_BEGIN("extract_json_candidate");
    char *working;
    {
        /* strip leading/trailing whitespace */
        const char *raw = text;
        while (*raw == ' ' || *raw == '\t' || *raw == '\n' || *raw == '\r') raw++;
        size_t len = strlen(raw);
        while (len > 0 && (raw[len-1] == ' ' || raw[len-1] == '\t' || raw[len-1] == '\n' || raw[len-1] == '\r')) len--;
        working = strndup(raw, len ? len : 0);
        /* markdown fences */
        if (strncmp(working, "```", 3) == 0) {
            WCUP_BRANCH(1);
            char *nl = strchr(working + 3, '\n');
            if (nl) { WCUP_BRANCH(1); char *rest = strdup(nl + 1); free(working); working = rest; }
            else WCUP_BRANCH(0);
            size_t wlen = strlen(working);
            if (wlen >= 3 && strcmp(working + wlen - 3, "```") == 0) {
                WCUP_BRANCH(1);
                working[wlen - 3] = '\0';
            } else WCUP_BRANCH(0);
            if (strncasecmp(working, "json\n", 5) == 0) {
                WCUP_BRANCH(1);
                char *rest = strdup(working + 5); free(working); working = rest;
            } else WCUP_BRANCH(0);
        } else {
            WCUP_BRANCH(0);
        }
        /* outermost { } or [ ] span */
        int done = 0;
        for (const char *opener = "{"; !done; opener = "[") {
            const char *closer = (opener[0] == '{') ? "}" : "]";
            WCUP_LOOP_TICK();
            if (working[0] == opener[0]) {
                WCUP_BRANCH(1);
                break;
            }
            WCUP_BRANCH(0);
            char *start = strchr(working, opener[0]);
            char *end = strrchr(working, closer[0]);
            if (start && end && end > start) {
                WCUP_BRANCH(1);
                size_t slen = (size_t)(end - start) + 1;
                char *result = strndup(start, slen);
                free(working);
                working = result;
                done = 1;
            } else {
                WCUP_BRANCH(0);
                if (opener[0] == '[') done = 1;
            }
        }
    }
    WCUP_VALSTR(working);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("extract_json_candidate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s extract_json_candidate <text>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) extract_json_candidate(argv[i]);
    return 0;
}
