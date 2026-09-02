#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "watercup.h"
#include "libjson/json.h"

extern void account_usage_fmt_usd_str(const char *s, char *buf, size_t sz);

static void trace_one(const char *input) {
    WCUP_BEGIN("account_usage_fmt_usd");
    char buf[128];
    if (!input || strcmp(input, "NULL") == 0) {
        snprintf(buf, sizeof(buf), "None");
    } else {
        account_usage_fmt_usd_str(input, buf, sizeof(buf));
    }
    WCUP_VALSTR(buf);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof(cup));
    printf("%s\n", cup);
    WCUP_END("account_usage_fmt_usd");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <string>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
