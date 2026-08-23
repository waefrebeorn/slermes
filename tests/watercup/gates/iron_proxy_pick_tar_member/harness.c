/* gate: iron_proxy_pick_tar_member — traced copy of
 * src/agent/iron_proxy.c::ip_pick_tar_member
 * (LIVE twin: agent/proxy_sources/iron_proxy.py::_pick_tar_member).
 *
 * Input encoding: "<binary_name>|<member>\t<member>\t..." (tab-separated;
 * all members regular files — the gate's sub-contract).
 *
 * Branch ladder per member (must match twin.py EXACTLY):
 *   B1: absolute path ('/' prefix)  -> skip
 *   B2: '..' traversal              -> skip
 *   B3: leaf name == binary_name    -> else skip
 *   B4: root-level (no '/')         -> immediate win, stop
 *   V1: chosen member or "NONE"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void wc_pick_tar_member(const char *enc) {
    WCUP_BEGIN("pick_tar_member");
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", enc);
    char *bar = strchr(buf, '|');
    if (!bar) {                              /* malformed — not probed */
        char cup[32];
        WCUP_VALSTR("NONE");
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pick_tar_member");
        return;
    }
    *bar = '\0';
    const char *binary_name = buf;
    const char *members = bar + 1;

    char chosen[512];
    chosen[0] = '\0';
    int have = 0;
    int root_hit = 0;
    int dir_kept = 0;
    const char *p = members;
    while (*p && !have) {
        const char *e = strchr(p, '\t');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char m[512];
        if (len >= sizeof m) len = sizeof m - 1;
        memcpy(m, p, len); m[len] = '\0';

        int absolute = (m[0] == '/');                 /* B1 */
        WCUP_BRANCH(absolute);
        if (!absolute) {
            int trav = (strstr(m, "..") != NULL);     /* B2 */
            WCUP_BRANCH(trav);
            if (!trav) {
                const char *leaf = strrchr(m, '/');
                leaf = leaf ? leaf + 1 : m;
                int match = (strcmp(leaf, binary_name) == 0);  /* B3 */
                WCUP_BRANCH(match);
                if (match) {
                    int root = (strchr(m, '/') == NULL);       /* B4 */
                    WCUP_BRANCH(root);
                    int *slot = root ? &root_hit : &dir_kept;
                    if (!*slot) {
                        snprintf(chosen, sizeof chosen, "%s", m);
                        *slot = 1;            /* first dir candidate kept;
                                                 any root hit wins immediately */
                        have = have || root;  /* only a root hit stops the scan */
                    }
                }
            }
        }
        p = e ? e + 1 : p + len;
    }
    {
        char cup[32];
        WCUP_VALSTR(chosen[0] ? chosen : "NONE");     /* V1 */
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pick_tar_member");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pick_tar_member <enc>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_pick_tar_member(argv[i]);
    return 0;
}
