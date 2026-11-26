#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "watercup.h"
#include "libjson/json.h"

/* The generated harness declared `account_usage_fmt_usd_str(const char*, char*, size_t)` -- a symbol
 * the port has never had. The real port is account_usage_fmt_usd(double, char*, size_t), so the gate
 * compared against nothing. Call what exists. */
extern void account_usage_fmt_usd(double d, char *buf, size_t sz);

static void trace_one(const char *input) {
    WCUP_BEGIN("account_usage_fmt_usd");
    char buf[128];
    if (!input || strcmp(input, "NULL") == 0) {
        buf[0] = '\0';
    } else {
        account_usage_fmt_usd(strtod(input, NULL), buf, sizeof(buf));
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
