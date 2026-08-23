/* gate: gemini_native_adapter_is_standard_key_auth_error — traced copy of
 * src/agent/gemini_native_adapter_ports.c::gna_is_standard_key_auth_error.
 * LIVE twin: agent/gemini_native_adapter.py::is_standard_key_auth_error.
 *
 * argv input format: "status|reason|message".
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: status != 401 -> False
 *   B2: reason == ACCESS_TOKEN_TYPE_UNSUPPORTED -> True
 *   B3: "expected oauth 2 access token" in lowered message
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static char *wc_lowerdup(const char *s) {
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static bool wc_is_standard_key_auth_error(int status, const char *error_message,
                                          const char *reason) {
    WCUP_BEGIN("is_standard_key_auth_error");
    bool result = false;
    if (status != 401) {                        /* B1 */
        WCUP_BRANCH(1);
        result = false;
        goto out;
    }
    WCUP_BRANCH(0);
    if (reason && strcmp(reason, "ACCESS_TOKEN_TYPE_UNSUPPORTED") == 0) {  /* B2 */
        WCUP_BRANCH(1);
        result = true;
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char *lower = error_message ? wc_lowerdup(error_message) : NULL;
        bool hit = lower && strstr(lower, "expected oauth 2 access token") != NULL; /* B3 */
        WCUP_BRANCH(hit);
        result = hit;
        free(lower);
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("is_standard_key_auth_error");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_standard_key_auth_error <status|reason|message>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *b1 = strchr(arg, '|');
        const char *reason = "", *msg = "";
        if (b1) {
            *b1 = '\0';
            reason = b1 + 1;
            char *b2 = strchr(reason, '|');
            if (b2) { *b2 = '\0'; msg = b2 + 1; }
        }
        wc_is_standard_key_auth_error(atoi(arg), msg, reason);
        free(arg);
    }
    return 0;
}
