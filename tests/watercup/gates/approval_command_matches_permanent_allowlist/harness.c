/* gate: approval_command_matches_permanent_allowlist — traced copy of
 * src/tools/approval_ports.c::apr_command_matches_permanent_allowlist.
 * LIVE twin: tools/approval.py::_command_matches_permanent_allowlist.
 * Input format: "<command>|<json array of allowlist patterns>".
 * Trace contract (coarse rungs, same order both sides):
 *   B1 command non-empty;
 *   per candidate pattern: B(pattern exists?) then B(pattern matched?);
 *   V "True"/"False".
 * Sub-contract: commands without shell operators and patterns that are exact
 * or contain a single '*' (the LIVE fnmatchcase and the C port's star-glob
 * agree on that subset); no blank/whitespace-only patterns.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

static bool star_glob_match(const char *pat, const char *cmd) {
    const char *star = strchr(pat, '*');
    size_t plen = (size_t)(star - pat);
    const char *suffix = star + 1;
    size_t slen = strlen(suffix);
    size_t clen = strlen(cmd);
    return clen >= plen + slen &&
           strncmp(cmd, pat, plen) == 0 &&
           strcmp(cmd + clen - slen, suffix) == 0;
}

static void trace_one(const char *input) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", input);
    char *pipe = strrchr(tmp, '|');
    const char *json_txt = "";
    if (pipe) { *pipe = '\0'; json_txt = pipe + 1; }
    const char *command = tmp;

    WCUP_BEGIN("command_matches_permanent_allowlist");
    bool result = false;
    if (!command || !*command) {
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
        const char *p = json_txt;
        for (;;) {
            p = strstr(p, "\"");
            if (!p) { WCUP_BRANCH(0); break; }
            WCUP_BRANCH(1);
            const char *e = p + 1;
            while (*e && *e != '"') e++;
            size_t len = (size_t)(e - p - 1);
            char entry[512];
            if (len >= sizeof entry) len = sizeof entry - 1;
            memcpy(entry, p + 1, len);
            entry[len] = '\0';
            if (len == 0) { p = e + 1; continue; }  /* skip blank scan artifact, no rung */
            bool match = strchr(entry, '*')
                             ? star_glob_match(entry, command)
                             : (strcmp(entry, command) == 0);
            WCUP_BRANCH(match);
            if (match) { result = true; break; }
            p = e + 1;
        }
    }
    WCUP_VALSTR(result ? "True" : "False");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("command_matches_permanent_allowlist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s command_matches_permanent_allowlist <cmd|json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
