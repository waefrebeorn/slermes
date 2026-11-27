/* gate: permanent allowlist — does "always allow" remember anything?
 *
 * The old gate fed junk strings to approve_permanent and hashed the return; the
 * port printed "permanent approval added" and changed nothing, so the gate could
 * not tell. This drives a sequence: "<action>|<key>", action = approve or check.
 *
 * Ladder (matches twin.py):
 *   B1: key non-empty
 *   B2: key is in the permanent set after the action
 *   V1: the membership answer
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "watercup.h"

int  approval_approve_permanent(const char *pattern_key);
bool approval_is_permanently_approved(const char *pattern_key);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_permanent <action|key>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[256];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strchr(probe, '|');
        const char *action = probe;
        const char *key = "";
        if (bar) { *bar = '\0'; key = bar + 1; }

        WCUP_BEGIN("wc_permanent");
        WCUP_BRANCH(key[0] != '\0');                 /* B1 */
        if (strcmp(action, "approve") == 0) approval_approve_permanent(key);
        bool found = approval_is_permanently_approved(key);
        WCUP_BRANCH(found);                           /* B2 */
        WCUP_VALSTR(found ? "1" : "0");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wc_permanent");
    }
    return 0;
}
