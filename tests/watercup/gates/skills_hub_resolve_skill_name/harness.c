/* gate: skills_hub_resolve_skill_name — traced copy of
 * src/tools/skills_hub.c::skills_hub_resolve_skill_name (+ static
 * source_is_valid_skill_name). LIVE twin: GitHubSource._resolve_skill_name.
 *
 * Input format: "<fmname>|<url>" where fmname "-" = no frontmatter name
 * (fm absent). Branch ladder (twin.py mirrors EXACTLY):
 *   B1: fm name present & valid -> stripped name (V1)
 *   B2: empty/absent url -> NULL (V2 "None")
 *   B3: .../<name>/SKILL.md tail & valid -> that name
 *   B4: <last>.md strip & valid -> candidate
 *   V2: result | "None"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

static bool source_is_valid_skill_name(const char *name) {
    if (!name) return false;
    const char *s = name;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) len--;
    if (len == 0 || len >= 256) return false;
    char cand[256];
    for (size_t i = 0; i < len; i++) cand[i] = (char)tolower((unsigned char)s[i]);
    cand[len] = '\0';
    if (!strcmp(cand,"skill") || !strcmp(cand,"readme") ||
        !strcmp(cand,"index") || !strcmp(cand,"unnamed-skill")) return false;
    if (!(cand[0] >= 'a' && cand[0] <= 'z')) return false;
    for (size_t i = 1; i < len; i++) {
        char ch = cand[i];
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
              ch == '_' || ch == '-')) return false;
    }
    return true;
}

static void wc_resolve(const char *input) {
    WCUP_BEGIN("resolve_skill_name");
    /* split "<fmname>|<url>" */
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", input ? input : "");
    char *sep = strchr(buf, '|');
    const char *fmn = NULL;
    const char *u = buf;
    if (sep) {
        *sep = '\0';
        fmn = buf[0] ? buf : NULL;               /* "-" handled below */
        u = sep + 1;
    } else {
        u = buf;
        fmn = NULL;
    }
    if (fmn && strcmp(fmn, "-") == 0) fmn = NULL;

    char result[512];
    strcpy(result, "None");
    bool done = false;

    if (fmn && source_is_valid_skill_name(fmn)) {          /* B1 */
        WCUP_BRANCH(1);
        const char *s = fmn;
        while (*s == ' ' || *s == '\t') s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len-1]==' '||s[len-1]=='\t')) len--;
        memcpy(result, s, len);
        result[len] = '\0';
        goto emit;
    }
    WCUP_BRANCH(0);
    if (!u || !*u) {                                        /* B2 */
        WCUP_BRANCH(1);
        goto emit;
    }
    WCUP_BRANCH(0);
    {
        /* urlparse(url).path: skip scheme://host, cut at ? or # */
        const char *path = u;
        const char *scheme = strstr(u, "://");
        if (scheme) {
            /* LIVE urlparse().path is "" (never NULL) when no '/' follows;
             * keep the shared ladder instead of the port's early NULL. */
            path = strchr(scheme + 3, '/');
            if (!path) path = "";
        }
        size_t plen = strcspn(path, "?#");
        char pathbuf[512];
        if (plen >= sizeof pathbuf) plen = sizeof pathbuf - 1;
        memcpy(pathbuf, path, plen);
        pathbuf[plen] = '\0';
        char *parts[64];
        int nparts = 0;
        for (char *tok = strtok(pathbuf, "/"); tok && nparts < 64; tok = strtok(NULL, "/"))
            if (*tok) parts[nparts++] = tok;

        bool skillmd = (nparts >= 2 &&
                        strcasecmp(parts[nparts-1], "skill.md") == 0);
        if (skillmd) {                                      /* B3 */
            WCUP_BRANCH(1);
            if (source_is_valid_skill_name(parts[nparts-2])) {
                snprintf(result, sizeof result, "%s", parts[nparts-2]);
                done = true;
            }
        } else {
            WCUP_BRANCH(0);
        }
        if (!done && nparts >= 1) {
            char cand[512];
            snprintf(cand, sizeof cand, "%s", parts[nparts-1]);
            size_t clen = strlen(cand);
            bool ismd = (clen > 3 && strcasecmp(cand + clen - 3, ".md") == 0);
            if (ismd) {                                     /* B4 */
                WCUP_BRANCH(1);
                cand[clen-3] = '\0';
            } else {
                WCUP_BRANCH(0);
            }
            if (source_is_valid_skill_name(cand))
                snprintf(result, sizeof result, "%s", cand);
        }
    }
emit:
    WCUP_VALSTR(result);                                    /* V2 */
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_skill_name");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_skill_name <fm|url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_resolve(argv[i]);
    return 0;
}
