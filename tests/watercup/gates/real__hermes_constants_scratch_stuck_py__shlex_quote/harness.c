/* gate: shlex_quote(s) -- POSIX shlex.quote semantics, the contract this port claims.
 * Results are compared as HEX: a non-ascii fixture would otherwise be re-encoded by the
 * Windows argv/cup path and differ for reasons that have nothing to do with the port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
char *hcsstuck_shlex_quote(const char *s);
static void to_hex(const char *s, char *out, size_t cap) {
    size_t p = 0;
    for (const unsigned char *c = (const unsigned char *)s; *c && p + 3 < cap; c++)
        p += (size_t)snprintf(out + p, cap - p, "%02x", *c);
    out[p] = '\0';
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        const char *in = strcmp(argv[i], "<<CAFE>>") == 0 ? "caf\xc3\xa9" : argv[i];
        char hex[8192];
        WCUP_BEGIN("shlex_quote");
        char *q = hcsstuck_shlex_quote(in);
        to_hex(q ? q : "(null)", hex, sizeof hex);
        WCUP_VALSTR(hex);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("shlex_quote");
        free(q);
    }
    return 0;
}
