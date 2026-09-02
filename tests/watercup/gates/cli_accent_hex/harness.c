#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"

extern char *cl2_accent_hex(void);

static void trace_one(void) {
    WCUP_BEGIN("cl2_accent_hex");
    char *result = cl2_accent_hex();
    const char *val = result ? result : "NULL";
    WCUP_VALSTR(val);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cl2_accent_hex");
    free((void*)result);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        (void)argv[i]; /* input ignored — function has no params */
        trace_one();
    }
    return 0;
}
