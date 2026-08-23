/* gate: slash_access_can_run_ladder — traced copy of
 * src/gateway/slash_access.c::slash_policy_is_admin + slash_policy_can_run
 * (LIVE twin: gateway/slash_access.py::SlashAccessPolicy).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: gating disabled                    -> everyone can run
 *   B2: user is admin                      -> can run anything
 *   B3: canonical_cmd empty                -> deny
 *   B4: cmd in ALWAYS_ALLOWED {help,whoami} -> allow
 *   B5: cmd in user_allowed_commands       -> allow/deny
 *   V1: result bool as canonical string
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static const char *ALWAYS_ALLOWED[] = {"help", "whoami", NULL};

static bool id_in_list(const char *list, const char *id) {
    if (!list || !*list || !id || !*id) return false;
    char buf[4096];
    snprintf(buf, sizeof buf, "%s", list);
    char *save = NULL;
    char *lower = NULL;
    /* lowercase compare target */
    char lid[256]; size_t i = 0;
    for (; id[i] && i < sizeof lid - 1; i++) lid[i] = (char)tolower((unsigned char)id[i]);
    lid[i] = '\0';
    for (char *tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1]==' ' || end[-1]=='\t')) end--;
        *end = '\0';
        for (char *q = tok; *q; q++) *q = (char)tolower((unsigned char)*q);
        if (strcmp(tok, lid) == 0) return true;
        (void)lower;
    }
    return false;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s slash_policy_can_run <enabled|admins|allowed|user|cmd>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char probe[4096];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *f[5] = {probe, NULL, NULL, NULL, NULL};
        int nf = 0;
        for (char *p = probe; *p && nf < 4; p++)
            if (*p == '|') { *p = '\0'; f[++nf] = p + 1; }
        int enabled      = f[0] ? atoi(f[0]) : 1;
        const char *admins  = f[1] ? f[1] : "";
        const char *allowed = f[2] ? f[2] : "";
        const char *user    = f[3] ? f[3] : "";
        const char *cmd     = f[4] ? f[4] : "";

        WCUP_BEGIN("slash_policy_can_run");
        int result;
        if (!enabled) {                                   /* B1 */
            WCUP_BRANCH(1);
            result = 1;
            goto out;
        }
        WCUP_BRANCH(0);
        if (id_in_list(admins, user)) {                   /* B2 */
            WCUP_BRANCH(1);
            result = 1;
            goto out;
        }
        WCUP_BRANCH(0);
        if (!cmd[0]) {                                    /* B3 */
            WCUP_BRANCH(1);
            result = 0;
            goto out;
        }
        WCUP_BRANCH(0);
        {
            char lcmd[256]; size_t k = 0;
            for (; cmd[k] && k < sizeof lcmd - 1; k++) lcmd[k] = (cmd[k]=='/') ? ' ' : (char)tolower((unsigned char)cmd[k]);
            lcmd[k] = '\0';
            /* strip leading slashes then spaces */
            char *s = lcmd; while (*s == ' ') s++;
            memmove(lcmd, s, strlen(s) + 1);
            int always = 0;
            for (int a = 0; ALWAYS_ALLOWED[a]; a++)
                if (strcasecmp(lcmd, ALWAYS_ALLOWED[a]) == 0) always = 1;
            if (always) {                                 /* B4 */
                WCUP_BRANCH(1);
                result = 1;
                goto out;
            }
            WCUP_BRANCH(0);
            result = id_in_list(allowed, cmd);            /* B5 */
        }
out:
        {
            char vs[8];
            snprintf(vs, sizeof vs, "%s", result ? "True" : "False");
            WCUP_VALSTR(vs);
        }
        {
            char cup[32];
            WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup);
            WCUP_END("slash_policy_can_run");
        }
    }
    return 0;
}
