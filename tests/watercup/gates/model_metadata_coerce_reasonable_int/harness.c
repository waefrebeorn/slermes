/* gate: model_metadata_coerce_reasonable_int — traced copy of
 * src/agent/provider_metadata.c::coerce_reasonable_int.
 * LIVE twin: agent/model_metadata.py::_coerce_reasonable_int (defaults
 * minimum=1024, maximum=10_000_000).
 *
 * Input: one argv token per case; "NULL" encodes a null pointer.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: null -> -1/None
 *   LOOP leading space/tab ticks
 *   B2: empty after strip -> -1/None
 *   LOOP comma-removal ticks over the remaining chars
 *   parse integer; B3: trailing garbage -> -1/None
 *   B4: in [minimum, maximum] -> value else -1/None
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static int wc_coerce_reasonable_int(const char *value, int minimum, int maximum) {
    WCUP_BEGIN("coerce_reasonable_int");
    int result;
    if (!value) {                                    /* B1 */
        WCUP_BRANCH(1);
        result = -1;
        goto done;
    }
    WCUP_BRANCH(0);
    while (*value == ' ' || *value == '\t') { WCUP_LOOP_TICK(); value++; }
    if (!*value) {                                   /* B2 */
        WCUP_BRANCH(1);
        result = -1;
        goto done;
    }
    WCUP_BRANCH(0);
    {
        size_t len = strlen(value);
        char *buf = malloc(len + 1);
        size_t j = 0;
        for (size_t i = 0; i < len; i++) {           /* LOOP comma removal */
            WCUP_LOOP_TICK();
            if (value[i] != ',') buf[j++] = value[i];
        }
        buf[j] = '\0';
        char *endptr = NULL;
        long val = strtol(buf, &endptr, 10);
        bool endptr_ok = (endptr && (*endptr == '\0' || *endptr == ' ' || *endptr == '\t'));
        free(buf);
        if (!endptr_ok) {                            /* B3 */
            WCUP_BRANCH(1);
            result = -1;
            goto done;
        }
        WCUP_BRANCH(0);
        if (val < (long)minimum || val > (long)maximum) {  /* B4 */
            WCUP_BRANCH(1);
            result = -1;
        } else {
            WCUP_BRANCH(0);
            result = (int)val;
        }
    }
done: {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("coerce_reasonable_int");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coerce_reasonable_int <value|NULL>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_coerce_reasonable_int(strcmp(argv[i], "NULL") == 0 ? NULL : argv[i],
                                 1024, 10000000);
    return 0;
}
