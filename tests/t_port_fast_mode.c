/* t_port_fast_mode.c — oracle harness for agent/fast_mode.py tier helpers. */
#include "port_fast_mode.h"

#include <stdio.h>
#include <stdlib.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        default: fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* parse_service_tier raw inputs: NULL (None), "", words, shorthands, junk,
     * mixed case, surrounding whitespace. */
    static const char *const psts[] = {
        NULL, "", "fast", "FAS T", "on", "priority", "ultrafast", "ultra",
        "auto", "cold", "normal", "default", "standard", "off", "none",
        "bogus", "  fast  ", " auto", "priority ",
    };
    for (size_t i = 0; i < sizeof(psts) / sizeof(psts[0]); i++) {
        char *out = fm_parse_service_tier(psts[i]);
        printf("{\"family\":\"pst\",\"case\":\"pst_%zu\",\"in\":", i);
        jstr(psts[i]);
        printf(",\"out\":");
        jstr(out);
        printf("}\n");
        free(out);
    }

    /* parse_exact_service_tier: "" pins normal, unknown raises ValueError. */
    static const char *const pests[] = {
        NULL, "", "fast", "FAS T", "on", "ultra", "auto", "normal", "none",
        "  standard  ", "bogus", "cold",
    };
    for (size_t i = 0; i < sizeof(pests) / sizeof(pests[0]); i++) {
        char *out = NULL;
        int ok = fm_parse_exact_service_tier(pests[i], &out);
        printf("{\"family\":\"pest\",\"case\":\"pest_%zu\",\"in\":", i);
        jstr(pests[i]);
        if (ok != 0) {
            printf(",\"err\":1}\n");
        } else {
            printf(",\"out\":");
            jstr(out);
            printf("}\n");
        }
        free(out);
    }

    /* service_tier_word: stored tiers -> user-facing words. */
    static const char *const tiers[] = {
        NULL, "", "priority", "ultrafast", "auto", "cold",
        "normal", "fast", "custom-x",
    };
    for (size_t i = 0; i < sizeof(tiers) / sizeof(tiers[0]); i++) {
        char *word = fm_service_tier_word(tiers[i]);
        printf("{\"family\":\"stw\",\"case\":\"stw_%zu\",\"in\":", i);
        jstr(tiers[i]);
        printf(",\"out\":");
        jstr(word);
        printf("}\n");
        free(word);
    }
    return 0;
}
