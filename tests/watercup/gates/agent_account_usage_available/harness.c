#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "watercup.h"
#include "libjson/json.h"

/* Minimal struct matching src/tools/account_usage.c */
typedef struct {
    int window_count;
    int detail_count;
    char unavailable_reason[256];
} account_usage_snapshot_t;

extern bool account_usage_available(const account_usage_snapshot_t *snap);

static void parse_input(const char *input, account_usage_snapshot_t *snap) {
    memset(snap, 0, sizeof(*snap));
    if (!input || !*input) return;
    /* Format: "windows,details,reason_or_NULL" */
    char buf[512];
    strncpy(buf, input, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    
    char *saveptr = NULL;
    char *tok = strtok_r(buf, ",", &saveptr);
    if (tok) snap->window_count = atoi(tok);
    
    tok = strtok_r(NULL, ",", &saveptr);
    if (tok) snap->detail_count = atoi(tok);
    
    tok = strtok_r(NULL, ",", &saveptr);
    if (tok && strcmp(tok, "NULL") != 0) {
        strncpy(snap->unavailable_reason, tok, sizeof(snap->unavailable_reason)-1);
    }
}

static void trace_one(const char *input) {
    WCUP_BEGIN("account_usage_available");
    account_usage_snapshot_t snap;
    parse_input(input, &snap);
    bool result = account_usage_available(&snap);
    const char *val = result ? "true" : "false";
    WCUP_VALSTR(val);
    char cup[64];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("account_usage_available");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <windows,details,reason>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
