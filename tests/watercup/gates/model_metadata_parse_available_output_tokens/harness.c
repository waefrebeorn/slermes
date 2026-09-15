/* gate: model_metadata_parse_available_output_tokens_from_error — traced copy
 * of src/agent/provider_metadata.c::parse_available_output_tokens_from_error
 * (LIVE twin: agent/model_metadata.py::parse_available_output_tokens_from_error).
 *
 * Branch ladder per input (must match twin.py EXACTLY):
 *   B1: empty message
 *   B2: contains "max_tokens"
 *   B3: contains an available-tokens marker
 *   B4: numeric figure directly after the marker keyword
 *   B5: '=' fallback present
 *   B6: numeric figure after the last '='
 *   VSTR(result or "-1")
 *
 * Documented narrowing (inputs avoided): LIVE also classifies
 * DashScope range / OpenRouter parts / LM-Studio char-count / vLLM phrasings
 * that the C port does not implement; those phrasings are excluded from this
 * walk. Also excluded: "available_output_tokens" (C extracts from it, LIVE's
 * regexes do not know that spelling).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static int wc_parse(const char *error_msg) {
    WCUP_BEGIN("parse_available_output_tokens_from_error");
    int result = -1;

    WCUP_BRANCH(!error_msg || !*error_msg);                  /* B1 */
    if (error_msg && *error_msg) {
        char buf[2048];
        size_t len = strlen(error_msg);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        for (size_t i = 0; i < len; i++)
            buf[i] = (char)tolower((unsigned char)error_msg[i]);
        buf[len] = '\0';

        WCUP_BRANCH(strstr(buf, "max_tokens") != NULL);       /* B2 */
        WCUP_BRANCH(strstr(buf, "available_tokens") != NULL ||
                    strstr(buf, "available tokens") != NULL ||
                    strstr(buf, "available_output_tokens") != NULL); /* B3 */
        if (strstr(buf, "max_tokens") &&
            (strstr(buf, "available_tokens") || strstr(buf, "available tokens") ||
             strstr(buf, "available_output_tokens"))) {
            const char *at = strstr(buf, "available_output_tokens");
            if (!at) at = strstr(buf, "available_tokens");
            if (!at) at = strstr(buf, "available tokens");
            if (at) {
                const char *num = at;
                while (*num && *num != ':' && *num != '=' && *num != ' ') num++;
                while (*num && (*num == ':' || *num == ' ' || *num == '=')) num++;
                WCUP_BRANCH(*num >= '0' && *num <= '9');      /* B4 */
                if (*num >= '0' && *num <= '9') {
                    long val = strtol(num, NULL, 10);
                    if (val >= 1) result = (int)val;
                }
            }
            if (result < 0) {
                const char *eq = strrchr(buf, '=');
                WCUP_BRANCH(eq != NULL);                      /* B5 */
                if (eq) {
                    const char *num = eq + 1;
                    while (*num && *num == ' ') num++;
                    WCUP_BRANCH(*num >= '0' && *num <= '9');  /* B6 */
                    if (*num >= '0' && *num <= '9') {
                        long val = strtol(num, NULL, 10);
                        if (val >= 1) result = (int)val;
                    }
                }
            }
        }
    }

    char vbuf[32];
    snprintf(vbuf, sizeof vbuf, "%d", result);
    WCUP_VALSTR(vbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_available_output_tokens_from_error");
    return result;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_available_output_tokens_from_error <msgs...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_parse(argv[i]);
    return 0;
}
