/* gate: skill_manager_tool_validate_category — traced copy of
 * src/tools/skill_manager_val.c::skill_val_validate_category
 * (PoP: tools/skill_manager_tool.py:_validate_category).
 *
 * Rung order: null? -> slash/backslash -> length>64 -> invalid chars -> ok.
 * Value events are CLASS CODES ("null"/"slash"/"len"/"chars"/"ok"), because
 * the human-readable message TEXTS differ between LIVE and C (documented
 * divergence); the branch ladder is the contract here.
 */
#include <stdio.h>
#include <string.h>
#include "watercup.h"

#define MAX_NAME_LENGTH 64

static int valid_name_chars(const char *s) {
    if (!s || !*s) return 0;
    for (size_t i = 0; s[i]; i++) {
        char c = s[i];
        int lo = (c >= 'a' && c <= 'z');
        int di = (c >= '0' && c <= '9');
        if (!lo && !di && c != '.' && c != '_' && c != '-') return 0;
    }
    return 1;
}

static void validate_category(const char *category) {
    WCUP_BEGIN("validate_category");
    const char *cls;
    if (!category) {
        WCUP_BRANCH(1);
        cls = "null";
    } else {
        WCUP_BRANCH(0);
        if (strchr(category, '/') || strchr(category, '\\')) {
            WCUP_BRANCH(1);
            cls = "slash";
        } else {
            WCUP_BRANCH(0);
            if (strlen(category) > MAX_NAME_LENGTH) {
                WCUP_BRANCH(1);
                cls = "len";
            } else {
                WCUP_BRANCH(0);
                if (!valid_name_chars(category)) {
                    WCUP_BRANCH(1);
                    cls = "chars";
                } else {
                    WCUP_BRANCH(0);
                    cls = "ok";
                }
            }
        }
    }
    WCUP_VALSTR(cls);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("validate_category");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s validate_category <cat>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) validate_category(argv[i]);
    return 0;
}
