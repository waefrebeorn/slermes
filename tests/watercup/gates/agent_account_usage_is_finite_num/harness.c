#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"
#include <stdbool.h>

/* The port takes a double: bool account_usage_is_finite_num(double). The generated harness declared
 * a char* prototype, so the pointer's bits were reinterpreted as a double and "hello" answered
 * true -- a fabricated PASS shape in the other direction. Parse the text instead, and treat text
 * that is not a number as "not a number" (Python: isinstance(v,(int,float)) is False). */
extern bool account_usage_is_finite_num(double v);

static void trace_one(const char *input) {
    WCUP_BEGIN("account_usage_is_finite_num");
    bool result;
    {
        char *end = NULL;
        double v = (input && input[0] && strcmp(input, "NULL") != 0) ? strtod(input, &end) : 0.0;
        bool numeric = end && end != input && *end == '\0';
        result = numeric ? account_usage_is_finite_num(v) : false;
    }
    const char *val = result ? "true" : "false";
    WCUP_VALSTR(val);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("account_usage_is_finite_num");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <string>...\\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *input = strcmp(argv[i], "NULL") == 0 ? NULL : argv[i];
        trace_one(input);
    }
    return 0;
}
