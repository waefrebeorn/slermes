/* gate: cch_status_code_from_value — traced C copy of
 * src/agent/chat_completion_helpers_ports.c::cch_status_code_from_value.
 * Faithful port of LIVE Python agent/chat_completion_helpers.py::_status_code_from_value.
 * WCUP_* records the SAME control structure so the cup matches iff the C port
 * executes the identical branch ladder as LIVE Python.
 *
 * C returns -1 to represent LIVE's None (Optional[int]).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include "watercup.h"

static int cch_status_code_from_value(const char *arg) {
    WCUP_BEGIN("status_code_from_value");
    if (!arg || !*arg) { WCUP_BRANCH(1); WCUP_VALSTR("-1"); char c[32]; WCUP_FINALIZE(c,sizeof c); printf("%s\n",c); WCUP_END("status_code_from_value"); return -1; }
    WCUP_BRANCH(0);
    char *end = NULL;
    long iv = strtol(arg, &end, 10);
    if (*end == '\0') {
        if (iv >= 100 && iv < 600) { WCUP_BRANCH(1); char b[16]; snprintf(b,sizeof b,"%ld",iv); WCUP_VALSTR(b); char c[32]; WCUP_FINALIZE(c,sizeof c); printf("%s\n",c); WCUP_END("status_code_from_value"); return (int)iv; }
        WCUP_BRANCH(0); WCUP_VALSTR("-1"); char c0[32]; WCUP_FINALIZE(c0,sizeof c0); printf("%s\n",c0); WCUP_END("status_code_from_value"); return -1;
    }
    WCUP_BRANCH(0);
    const char *p = arg;
    int found = -1;
    while (*p) {
        if (strncasecmp(p, "HTTP_STATUS/", 11) == 0) p += 11;
        if (isdigit((unsigned char)p[0]) && p[0] >= '1' && p[0] <= '5' &&
            isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2])) {
            int code = (p[0]-'0')*100 + (p[1]-'0')*10 + (p[2]-'0');
            char nxt = p[3];
            if (!(isdigit((unsigned char)nxt) || (nxt >= 'a' && nxt <= 'z') || (nxt >= 'A' && nxt <= 'Z'))) {
                found = code; break;
            }
        }
        p++;
    }
    char fb[16]; snprintf(fb, sizeof fb, "%d", found); WCUP_VALSTR(fb);
    char c[32];
    WCUP_FINALIZE(c, sizeof c);
    printf("%s\n", c);
    WCUP_END("status_code_from_value");
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s status_code_from_value <value>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        cch_status_code_from_value(argv[i]);
    }
    return 0;
}
