/* gate: browser_tool_format_timeout_error
 * Faithful traced copy of src/tools/browser_tool_env.c::browser_format_timeout_error
 * (LIVE twin: tools/browser_tool.py::_format_browser_timeout_error).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   V1: header line "Command timed out after N seconds"
 *   B1: detail non-empty? -> V2 detail
 *   V3: lowercased combined stderr+stdout
 *   B2: "sandbox" in combined?      -> V4 sandbox hint
 *   B3: command == "open"?          (env-dependent local-mode/docker
 *       sub-branches are EXCLUDED from this walk — see gate.json notes;
 *       harness stubs browser_is_local_mode() = false)
 *   B4: any hint appended?
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static bool browser_is_local_mode(void) { return false; } /* env-stubbed; walk avoids this branch */

static void format_timeout_error(const char *command, int timeout,
                                 const char *stdout_text, const char *stderr_text) {
    WCUP_BEGIN("browser_format_timeout_error");
    size_t cap = 512;
    char *result = malloc(cap);
    int len = snprintf(result, cap, "Command timed out after %d seconds", timeout);
    char vbuf[2200];

    snprintf(vbuf, sizeof vbuf, "%s", result);
    WCUP_VALSTR(vbuf);   /* V1: header */

    const char *detail = stderr_text && strlen(stderr_text) > 0 ? stderr_text :
                         (stdout_text && strlen(stdout_text) > 0 ? stdout_text : "");
    if (detail && strlen(detail) > 0) {
        WCUP_BRANCH(1);
        len += snprintf(result + len, cap - len, "\n%s", detail);
        snprintf(vbuf, sizeof vbuf, "%s", result);
        WCUP_VALSTR(vbuf);   /* V2: with detail */
    } else {
        WCUP_BRANCH(0);
    }

    char combined[2048];
    snprintf(combined, sizeof(combined), "%s\n%s",
             stderr_text ? stderr_text : "", stdout_text ? stdout_text : "");
    for (char *p = combined; *p; p++) *p = (char)tolower((unsigned char)*p);
    snprintf(vbuf, sizeof vbuf, "%s", combined);
    WCUP_VALSTR(vbuf);   /* V3 */

    if (strstr(combined, "sandbox")) {
        WCUP_BRANCH(1);
        const char *hint = "\nChromium sandbox launch failed. Set AGENT_BROWSER_ARGS='--no-sandbox,--disable-dev-shm-usage' in your environment, or run: npx agent-browser install --with-deps";
        strcat(result, hint);
        len += strlen(hint);
        snprintf(vbuf, sizeof vbuf, "%s", result);
        WCUP_VALSTR(vbuf);   /* V4 */
    } else {
        WCUP_BRANCH(0);
        if (strcmp(command, "open") == 0 && browser_is_local_mode()) {
            WCUP_BRANCH(1);
            /* excluded from the input walk; unreachable here */
        } else {
            WCUP_BRANCH(0);
        }
    }

    snprintf(vbuf, sizeof vbuf, "%s", result);
    WCUP_VALSTR(vbuf);   /* final string */
    if (strstr(vbuf, "hint") || len < 0) { /* keep len used */ }
    free(result);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser_format_timeout_error");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_format_timeout_error \"<cmd>\\t<timeout>\\t<stdout>\\t<stderr>\"...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char tmp[4096];
        snprintf(tmp, sizeof tmp, "%s", argv[i]);
        char *f1 = strchr(tmp, '\t');
        if (!f1) { fprintf(stderr, "bad input: %s\n", argv[i]); return 2; }
        *f1 = '\0';
        char *f2 = strchr(f1 + 1, '\t'); if (!f2) return 2;
        *f2 = '\0';
        char *f3 = strchr(f2 + 1, '\t'); if (!f3) return 2;
        *f3 = '\0';
        format_timeout_error(tmp, atoi(f1 + 1), f2 + 1, f3 + 1);
    }
    return 0;
}
