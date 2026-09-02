#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"

extern const char *account_usage_title_case_slug(const char *value);

static void trace_one(const char *input) {
    WCUP_BEGIN("account_usage_title_case_slug");
    WCUP_BRANCH(1);
    const char *result = account_usage_title_case_slug(input);
    const char *val = result ? result : "";
    WCUP_VALSTR(val);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("account_usage_title_case_slug");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <string_or_NULL>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
