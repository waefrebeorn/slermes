/* gate: approval_scan_dollar_paren_end — traced copy of
 * src/tools/approval.c::scan_dollar_paren_end.
 * LIVE twin: tools/approval.py::_scan_dollar_paren_end.
 *
 * NOTE: the two implementations reach the same result by different internal
 * algorithms (LIVE: quote-aware scanner seeded depth=1 at '$(', C: paren
 * depth counter with quote/escape flags over the whole span). Only shared,
 * behaviorally-meaningful rungs are traced:
 *   B1 unmatched (no closing paren found)
 *   V normalized result ("None" | end offset)
 * Inputs carry trailing context so a successful scan never ends exactly at
 * buffer end (the C sentinel for unmatched).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static size_t wc_scan(const char *s, size_t len) {
    WCUP_BEGIN("scan_dollar_paren_end");
    /* traced copy of the FIXED port (mirrors LIVE _scan_dollar_paren_end):
     * depth seeded 1 at '$(', bare '(' ordinary, depth++ only on nested "$(" */
    int depth = 1;
    char quote = 0;
    size_t i = 2;
    size_t ret = len;
    while (i < len) {
        char c = s[i];
        if (quote) {
            if (c == '\\' && quote == '"' && i + 1 < len) { i += 2; continue; }
            if (c == quote) quote = 0;
            i++;
            continue;
        }
        if (c == '\'' || c == '"') { quote = c; i++; continue; }
        if (c == '\\' && i + 1 < len) { i += 2; continue; }
        if (c == '$' && i + 1 < len && s[i + 1] == '(') { depth++; i += 2; continue; }
        if (c == ')') {
            depth--;
            i++;
            if (depth == 0) { ret = i; break; }
            continue;
        }
        i++;
    }
    WCUP_BRANCH(ret == len);                                     /* B1 */
    {
        char vbuf[32];
        if (ret == len) snprintf(vbuf, sizeof vbuf, "None");
        else snprintf(vbuf, sizeof vbuf, "%zu", ret);
        WCUP_VALSTR(vbuf);
    }
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("scan_dollar_paren_end");
        return ret;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s scan_dollar_paren_end <cmd>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) wc_scan(argv[i], strlen(argv[i]));
    return 0;
}
