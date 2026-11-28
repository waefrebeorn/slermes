/* gate: the coding-instructions block built from agent.coding_instructions.
 * Fixture is the raw config value; output compared as hex (it can contain newlines). */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
char *coding_context_join_instructions(const json_t *raw);
static void to_hex(const char *s, char *out, size_t cap) {
    size_t p = 0;
    for (const unsigned char *c = (const unsigned char *)(s ? s : ""); *c && p + 3 < cap; c++)
        p += (size_t)snprintf(out + p, cap - p, "%02x", *c);
    out[p] = '\0';
}
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    for (int i = 2; i < argc; i++) {
        char hex[4096];
        WCUP_BEGIN("join_instructions");
        json_t *raw = json_parse(argv[i], NULL);
        char *joined = coding_context_join_instructions(raw);
        to_hex(joined, hex, sizeof hex);
        WCUP_VALSTR(hex);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
        WCUP_END("join_instructions");
        if (joined) free(joined);
        if (raw) json_free(raw);
    }
    return 0;
}
