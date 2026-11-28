/* gate: _fish_quote(value) -- fish single-quote escaping. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
char *w5_environments_407599_fish_quote(const char *value);
static void to_hex(const char *s, char *out, size_t cap) {
    size_t p = 0;
    for (const unsigned char *c = (const unsigned char *)s; *c && p + 3 < cap; c++)
        p += (size_t)snprintf(out + p, cap - p, "%02x", *c);
    out[p] = '\0';
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char hex[8192];
        WCUP_BEGIN("_fish_quote");
        char *q = w5_environments_407599_fish_quote(argv[i]);
        to_hex(q ? q : "(null)", hex, sizeof hex);
        WCUP_VALSTR(hex);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("_fish_quote");
        free(q);
    }
    return 0;
}
