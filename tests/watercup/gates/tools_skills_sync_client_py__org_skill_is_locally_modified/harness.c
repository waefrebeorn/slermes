/* gate: tools_skills_sync_client_py__org_skill_is_locally_modified — traced copy of ssc_org_skill_is_locally_modified */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ssc_org_skill_is_locally_modified(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ssc_org_skill_is_locally_modified");
    int result = ssc_org_skill_is_locally_modified(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ssc_org_skill_is_locally_modified");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ssc_org_skill_is_locally_modified <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
