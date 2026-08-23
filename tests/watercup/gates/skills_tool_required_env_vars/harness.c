#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
/* gate: traced copy of src/tools/skills_tool.c::skills_tool_required_env_vars
 * (PoP: tools/skills_tool.py:_get_required_environment_variables).
 * Input: frontmatter JSON text. The C port is a minimal strstr-based extractor
 * of the required_environment_variables list (names only); inputs restricted to
 * the agreed subset (plain valid names, no dupes — see notes). */
static int extract_one(const char *fm) {
    char names[32][256]; int n = 0;
    WCUP_BEGIN("required_env_vars");
    const char *p = fm ? strstr(fm, "required_environment_variables") : NULL;
    if (!p) { WCUP_BRANCH(1); }
    else {
        WCUP_BRANCH(0);
        p = strchr(p, '[');
        if (!p) { WCUP_BRANCH(1); }
        else {
            WCUP_BRANCH(0);
            const char *end = strchr(p, ']');
            if (!end) { WCUP_BRANCH(1); }
            else {
                WCUP_BRANCH(0);
                char block[1024];
                size_t bl = (size_t)(end - p - 1);
                if (bl >= sizeof(block)) bl = sizeof(block) - 1;
                memcpy(block, p + 1, bl); block[bl] = '\0';
                WCUP_LOOP_TICK();
                char *tok = strtok(block, ",");
                while (tok) {
                    char *s = tok; while (*s && (*s==' '||*s=='\t')) s++;
                    char name[256]; name[0] = '\0';
                    const char *np = strstr(s, "\"name\"");
                    if (!np) np = strstr(s, "name");
                    if (np) {
                        WCUP_BRANCH(1);
                        const char *colon = strchr(np, ':');
                        if (colon) {
                            colon++; while (*colon && (*colon==' '||*colon=='\t')) colon++;
                            if (*colon == '"') {
                                WCUP_BRANCH(1);
                                colon++;
                                const char *q2 = strchr(colon, '"');
                                size_t l = q2 ? (size_t)(q2 - colon) : 0;
                                if (l < sizeof(name)) { memcpy(name, colon, l); name[l] = '\0'; }
                            } else {
                                WCUP_BRANCH(0);
                                size_t l = 0;
                                while (colon[l] && colon[l] != ' ' && colon[l] != ',' && colon[l] != '}') { name[l] = colon[l]; l++; }
                                name[l] = '\0';
                            }
                        }
                    } else {
                        WCUP_BRANCH(0);
                        if (*s == '"') {
                            WCUP_BRANCH(1);
                            s++;
                            const char *q2 = strchr(s, '"');
                            size_t l = q2 ? (size_t)(q2 - s) : 0;
                            if (l < sizeof(name)) { memcpy(name, s, l); name[l] = '\0'; }
                        } else {
                            WCUP_BRANCH(0);
                            size_t l = 0;
                            while (s[l] && s[l] != ',' && s[l] != '}' && s[l] != ' ') { name[l] = s[l]; l++; }
                            name[l] = '\0';
                        }
                    }
                    if (name[0] && n < 32) {
                        WCUP_BRANCH(1);
                        snprintf(names[n], sizeof names[0], "%s", name); n++;
                    } else WCUP_BRANCH(0);
                    tok = strtok(NULL, ",");
                }
            }
        }
    }
    /* value: comma-joined names */
    char joined[2048]; joined[0] = '\0';
    for (int i = 0; i < n; i++) { if (i) strcat(joined, ","); strcat(joined, names[i]); }
    WCUP_VALSTR(joined);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("required_env_vars");
    return n;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s required_env_vars <frontmatter>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) extract_one(argv[i]);
    return 0;
}
