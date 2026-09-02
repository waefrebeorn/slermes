#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "libjson/json.h"
#include <stdbool.h>

/* shim returns const char*; harness passes json_node_t* cast through void* */
extern const char *acp_get_id(void *req);

static void trace_one(const char *input) {
    WCUP_BEGIN("acp_get_id");
    const char *result = acp_get_id(NULL);
    const char *val = result ? result : "NULL";
    WCUP_VALSTR(val);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("acp_get_id");
}

int main(int argc, char **argv) {
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
