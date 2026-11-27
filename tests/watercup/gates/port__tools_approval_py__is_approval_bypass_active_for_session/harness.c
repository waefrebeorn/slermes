/* gate: the session YOLO registry — does /yolo actually change the answer?
 *
 * The old gate passed junk strings to is_approval_bypass_active_for_session and
 * hashed the return. Both sides answered false, so it read NON-DISCRIMINATING while
 * the port printed "yolo probe" and never enabled anything. Here the input drives a
 * SEQUENCE: "<action>|<key>" where action is enable/clear/none. Both sides perform
 * the action and then ask the membership question, so the registry itself is under
 * test.
 *
 * Ladder (must match twin.py exactly):
 *   B1: the key is non-empty
 *   B2: after the action, the key is enabled (true/false)
 *   V1: the membership answer — the only observable Python has
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

extern int  apr2_enable_session_yolo(const char *session_key);
extern int  apr2_clear_session(const char *session_key);
extern bool apr2_is_session_yolo_enabled(const char *session_key);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_yolo <action|key>...\n", argv[0]); return 2; }

    for (int i = 2; i < argc; i++) {
        char probe[256];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *bar = strchr(probe, '|');
        const char *action = probe;
        const char *key = "";
        if (bar) { *bar = '\0'; key = bar + 1; }

        WCUP_BEGIN("wc_yolo");
        WCUP_BRANCH(key[0] != '\0');            /* B1 */

        int rc = 0;
        if (strcmp(action, "enable") == 0)      rc = apr2_enable_session_yolo(key);
        else if (strcmp(action, "clear") == 0)  rc = apr2_clear_session(key);

        bool active = apr2_is_session_yolo_enabled(key);
        WCUP_BRANCH(active);                    /* B2 */
        /* Python's enable_session_yolo/clear_session return None: the registry's
         * membership IS the observable. Mixing the C return code would compare a
         * value Python never has. */
        (void)rc;
        WCUP_VALSTR(active ? "1" : "0");

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wc_yolo");
    }
    return 0;
}
