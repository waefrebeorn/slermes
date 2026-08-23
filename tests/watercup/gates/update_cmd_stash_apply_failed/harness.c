/* gate: update_cmd_stash_apply_failed_only_on_existing_untracked — traced copy of
 * src/hermes_cli/update_cmd.c::uc_stash_apply_failed_only_on_existing_untracked
 * (LIVE twin: hermes_cli/update_cmd.py::_stash_apply_failed_only_on_existing_untracked).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: empty stderr            -> false
 *   per line: LOOP tick
 *     B2: blank line            -> skip
 *     B3: warning:/hint: prefix -> skip
 *     B4: untracked substring   -> saw=true
 *     B5: anything else         -> return false
 *   B6: final -> saw_untracked
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static bool wc_stash_apply_failed(const char *stderr_text) {
    WCUP_BEGIN("uc_stash_apply_failed");
    bool result = false;
    if (!stderr_text || !*stderr_text) {
        WCUP_BRANCH(1);           /* B1 */
        goto out;
    }
    WCUP_BRANCH(0);
    {
        const char *p = stderr_text;
        bool saw_untracked = false;
        bool aborted = false;
        while (*p) {
            WCUP_LOOP_TICK();
            const char *eol = strchr(p, '\n');
            size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
            while (linelen > 0 && p[linelen-1] == '\r') linelen--;
            if (linelen == 0) {          /* B2 blank */
                WCUP_BRANCH(1);
                p = eol ? eol + 1 : p + strlen(p);
                continue;
            }
            WCUP_BRANCH(0);
            char line[1024];
            size_t copylen = linelen < sizeof(line) - 1 ? linelen : sizeof(line) - 1;
            memcpy(line, p, copylen);
            line[copylen] = '\0';
            if (strncmp(line, "warning:", 8) == 0 || strncmp(line, "hint:", 5) == 0) {
                WCUP_BRANCH(1);          /* B3 skip informational */
            } else {
                WCUP_BRANCH(0);
                if (strstr(line, "already exists, no checkout") != NULL ||
                    strstr(line, "could not restore untracked files from stash") != NULL) {
                    WCUP_BRANCH(1);      /* B4 untracked flag */
                    saw_untracked = true;
                } else {
                    WCUP_BRANCH(0);      /* B5 tracked apply failed */
                    aborted = true;
                    result = false;
                    break;
                }
            }
            p = eol ? eol + 1 : p + strlen(p);
        }
        if (!aborted) {
            result = saw_untracked;      /* B6 */
            WCUP_BRANCH(result);
        }
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_stash_apply_failed");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_stash_apply_failed <stderr>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_stash_apply_failed(argv[i]);
    return 0;
}
