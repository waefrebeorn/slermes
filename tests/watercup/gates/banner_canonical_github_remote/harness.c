/* gate: banner_canonical_github_remote — traced C copy of
 * src/hermes_cli/banner_pure.c::banner_canonical_github_remote
 * (LIVE twin: hermes_cli/banner.py::_canonical_github_remote).
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: empty url
 *   V1: stripped value
 *   B2: git@github.com: prefix
 *   B3: ssh://git@github.com/ prefix
 *   B4 (else): urlparse netloc+path usable?
 *   V2: value after netloc/path rewrite
 *   V3: rstrip("/") (+ .git strip)
 *   V4: lowered result
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static char *banner_canonical_github_remote(const char *url) {
    WCUP_BEGIN("canonical_github_remote");
    char value[1024];
    if (!url || !*url) {
        WCUP_BRANCH(1);                                  /* B1 */
        WCUP_VALSTR("");                                 /* V4 */
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("canonical_github_remote");
        return NULL;
    }
    WCUP_BRANCH(0);
    /* strip */
    const char *s = url;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t slen = strlen(s);
    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t' || s[slen-1] == '\n' || s[slen-1] == '\r')) slen--;
    char stripped[1024];
    if (slen >= sizeof(stripped)) slen = sizeof(stripped) - 1;
    memcpy(stripped, s, slen); stripped[slen] = '\0';
    WCUP_VALSTR(stripped);                               /* V1 */
    const char *git_scp = "git@github.com:";
    const char *ssh_url = "ssh://git@github.com/";
    if (strncmp(stripped, git_scp, strlen(git_scp)) == 0) {
        WCUP_BRANCH(1);                                  /* B2 */
        snprintf(value, sizeof value, "github.com/%s", stripped + strlen(git_scp));
    } else {
        WCUP_BRANCH(0);
        if (strncmp(stripped, ssh_url, strlen(ssh_url)) == 0) {
            WCUP_BRANCH(1);                              /* B3 */
            snprintf(value, sizeof value, "github.com/%s", stripped + strlen(ssh_url));
        } else {
            WCUP_BRANCH(0);
            /* urlparse semantics: netloc only exists behind an explicit
             * "<scheme>://" prefix; otherwise everything is path. */
            const char *scheme_end = strstr(stripped, "://");
            size_t nl = 0;
            const char *path = "";
            const char *np = stripped;
            if (scheme_end) {
                np = scheme_end + 3;
                const char *slash = strchr(np, '/');
                nl = slash ? (size_t)(slash - np) : strlen(np);
                path = slash ? slash : "";
            }
            int usable = (nl > 0) && (*path != '\0');
            WCUP_BRANCH(usable);                         /* B4 */
            if (usable) snprintf(value, sizeof value, "%.*s%s", (int)nl, np, path);
            else snprintf(value, sizeof value, "%s", stripped);
        }
    }
    WCUP_VALSTR(value);                                  /* V2 */
    size_t vl = strlen(value);
    while (vl > 0 && value[vl-1] == '/') vl--;
    value[vl] = '\0';
    if (vl >= 4 && strcmp(value + vl - 4, ".git") == 0) { value[vl-4] = '\0'; vl -= 4; WCUP_BRANCH(1); }
    else WCUP_BRANCH(0);
    WCUP_VALSTR(value);                                  /* V3 */
    for (char *q = value; *q; q++) *q = (char)tolower((unsigned char)*q);
    WCUP_VALSTR(value);                                  /* V4 */
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("canonical_github_remote");
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s canonical_github_remote <url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) banner_canonical_github_remote(argv[i]);
    return 0;
}
