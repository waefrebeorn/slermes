/* gate: hermes_cli_plugins_cmd_resolve_git_url — links the FIXED
 * src/hermes_cli/plugins_cmd_wrappers.c::pcmd_resolve_git_url via extra_src.
 * LIVE twin: hermes_cli/plugins_cmd.py::_resolve_git_url.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: identifier is a URL (scheme prefix)
 *   B2: https://github.com/ + >=3 path parts + browser segment
 *   B3: segment == tree with subdir parts
 *   B4: '#' fragment present
 *   B5: '.git/' boundary present
 *   B6: owner/repo shorthand (>= 2 parts)
 * VSTR("<url>|<subdir>") — subdir "-" when None; "INVALID" mirrors LIVE's
 * ValueError (C returns NULL).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

char *pcmd_resolve_git_url(const char *identifier, char **out_subdir);

static void wc_case(const char *input) {
    WCUP_BEGIN("pcmd_resolve_git_url");
    char label[1024];

    int is_url = 0;
    static const char *const PREFS[] = {"https://", "http://", "git@", "ssh://", "file://", NULL};
    for (int i = 0; PREFS[i]; i++)
        if (strncmp(input, PREFS[i], strlen(PREFS[i])) == 0) { is_url = 1; break; }
    WCUP_BRANCH(is_url);                                        /* B1 */

    char *url = pcmd_resolve_git_url(input, &(char *){0});
    /* re-run capturing subdir through the ladder for the trace */
    int gh_hit = 0;
    int tree_sub = 0;
    if (is_url && strncmp(input, "https://github.com/", 19) == 0) {
        const char *path = input + 19;
        /* mirror LIVE: strip ?/# and slashes then count real segments */
        char tmp[512];
        snprintf(tmp, sizeof tmp, "%s", path);
        char *qm = strchr(tmp, '?'); if (qm) *qm = 0;
        char *hm = strchr(tmp, '#'); if (hm) *hm = 0;
        int nparts = 0;
        char *segs_pre[32]; int npre = 0;
        for (char *t = strtok(tmp, "/"); t && npre < 32; t = strtok(NULL, "/"))
            if (t[0]) { segs_pre[npre++] = t; nparts++; }
        static const char *const SEGS[] = {"actions","blob","commit","commits",
            "issues","pull","pulls","releases","tree","wiki",NULL};
        int in_seg = 0;
        if (nparts >= 3)
            for (int i = 0; SEGS[i]; i++)
                if (strcmp(segs_pre[2], SEGS[i]) == 0) { in_seg = 1; break; }
        gh_hit = nparts >= 3 && segs_pre[0][0] && segs_pre[1][0] && in_seg;
        WCUP_BRANCH(gh_hit);                                    /* B2 */
        if (gh_hit) {
            char tmp2[512];
            snprintf(tmp2, sizeof tmp2, "%s", path);
            char *h2 = strchr(tmp2, '#'); if (h2) *h2 = 0;
            char *segs[32]; int n = 0;
            for (char *t = strtok(tmp2, "/"); t && n < 32; t = strtok(NULL, "/"))
                if (t[0]) segs[n++] = t;
            tree_sub = n >= 3 && strcmp(segs[2], "tree") == 0 && n >= 5;
            WCUP_BRANCH(tree_sub);                              /* B3 */
        } else {
            WCUP_BRANCH(0);                                     /* B3 */
        }
    } else {
        WCUP_BRANCH(0);                                         /* B2 */
        WCUP_BRANCH(0);                                         /* B3 */
    }

    WCUP_BRANCH(strchr(input, '#') != NULL);                    /* B4 */
    WCUP_BRANCH(strstr(input, ".git/") != NULL);                /* B5 */

    /* B6: shorthand — nonempty slash-split parts >= 2 */
    {
        char tmp[512];
        snprintf(tmp, sizeof tmp, "%s", input);
        int nparts = 0;
        for (char *t = strtok(tmp, "/"); t; t = strtok(NULL, "/"))
            if (t[0]) nparts++;
        WCUP_BRANCH(!is_url && nparts >= 2);                    /* B6 */
    }

    char *sub = NULL;
    free(url);
    url = pcmd_resolve_git_url(input, &sub);
    if (!url)
        snprintf(label, sizeof label, "INVALID");
    else
        snprintf(label, sizeof label, "%s|%s", url, sub ? sub : "-");
    free(url);
    free(sub);

    WCUP_VALSTR(label);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pcmd_resolve_git_url");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s pcmd_resolve_git_url <identifier>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_case(argv[i]);
    return 0;
}
