/* gate: shell-hook allowlist — does approving a hook persist anything?
 *
 * Shk_record_approval used to print "approval recorded" and return 0, and revoke
 * printed a line, so the allowlist file never changed and a gateway restart forgot
 * every approval. Here the input drives a sequence against a private HERMES_HOME and
 * the cup carries the PERSISTED state read back from disk, not a return code.
 *
 * Ladder (matches twin.py):
 *   B1: command non-empty
 *   V1: the (event|command) pairs the allowlist holds, sorted, ';'-joined
 *   V2: entries removed by the last revoke
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include "watercup.h"

char *shk_load_allowlist(const char *hermes_home);
int   shk_record_approval(const char *event, const char *command);
long  shk_revoke(const char *command);

char *shk_allowlist_pairs(const char *json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_shellhook <action|event|command>...\n", argv[0]); return 2; }

    setenv("HERMES_HOME", ".testbin/shk_c", 1);
    /* no shell: cmd.exe mangles "&&", and a missing parent makes every write fail */
    mkdir(".testbin", 0777);
    mkdir(".testbin/shk_c", 0777);
    unlink(".testbin/shk_c/shell-hooks-allowlist.json");
    unlink(".testbin/shk_c/shell-hooks-allowlist.json.lock");

    for (int i = 2; i < argc; i++) {
        char probe[512];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *a1 = strchr(probe, '|');
        char *action = probe, *event = (char *)"", *command = (char *)"";
        if (a1) {
            *a1 = '\0'; event = a1 + 1;
            char *a2 = strchr(event, '|');
            if (a2) { *a2 = '\0'; command = a2 + 1; }
            else { command = event; event = (char *)""; }
        }

        WCUP_BEGIN("wc_shellhook");
        WCUP_BRANCH(command[0] != '\0');            /* B1 */

        long removed = 0;
        if (strcmp(action, "record") == 0) {
            int rc = shk_record_approval(event, command);
            if (getenv("SHK_DEBUG")) fprintf(stderr, "record rc=%d ev=%s cmd=%s\n", rc, event, command);
        }
        else if (strcmp(action, "revoke") == 0) removed = shk_revoke(command);

        char *json = shk_load_allowlist(".testbin/shk_c");
        char *joined = shk_allowlist_pairs(json);   /* sorted "event|command;..." */
        free(json);

        if (getenv("SHK_DEBUG")) fprintf(stderr, "V1=[%s] removed=%ld\n", joined, removed);
        WCUP_VALSTR(joined);                         /* V1: persisted state */
        WCUP_VALINT((int)removed);                   /* V2 */

        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("wc_shellhook");
        free(joined);
    }
    return 0;
}
