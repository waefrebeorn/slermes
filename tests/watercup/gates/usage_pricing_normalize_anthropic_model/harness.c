/* gate: usage_pricing_normalize_anthropic_model — traced C copy of
 * src/agent/usage_pricing.c::usage_pricing_normalize_anthropic_model.
 * Faithful port of LIVE Python agent/usage_pricing.py::_normalize_anthropic_model_name.
 * WCUP_* records the SAME control structure so the cup matches iff the C port
 * executes the identical branch ladder as LIVE Python.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static void dots_to_dashes_versioned(char *out, size_t outsz, const char *in) {
    size_t o = 0, i = 0;
    while (in[i] && o < outsz - 1) {
        if (isdigit((unsigned char)in[i])) {
            while (isdigit((unsigned char)in[i])) { out[o++] = in[i++]; if (o >= outsz-1) break; }
            if (in[i] == '.' && isdigit((unsigned char)in[i+1]) && o < outsz - 1) {
                out[o++] = '-'; i++;
                while (isdigit((unsigned char)in[i])) { out[o++] = in[i++]; if (o >= outsz-1) break; }
            }
        } else { out[o++] = in[i++]; }
    }
    out[o] = '\0';
}

static void usage_pricing_normalize_anthropic_model(const char *model, char *out, size_t out_size) {
    WCUP_BEGIN("normalize_anthropic");
    if (!out || out_size == 0) { WCUP_BRANCH(1); WCUP_VALSTR(""); WCUP_VALSTR(""); char c[32]; WCUP_FINALIZE(c,sizeof c); printf("%s\n",c); WCUP_END("normalize_anthropic"); return; }
    if (!model) { WCUP_BRANCH(1); out[0]='\0'; WCUP_VALSTR("null"); WCUP_VALSTR(""); char c[32]; WCUP_FINALIZE(c,sizeof c); printf("%s\n",c); WCUP_END("normalize_anthropic"); return; }
    WCUP_BRANCH(0);
    const char *p = model;
    int had_prefix = 0;
    if (strncmp(p, "anthropic/", 10) == 0) { p += 10; had_prefix = 1; }
    WCUP_BRANCH(had_prefix);
    char lower[256];
    size_t i = 0;
    while (p[i] && i < sizeof(lower) - 1) { lower[i] = (char)tolower((unsigned char)p[i]); i++; }
    lower[i] = '\0';
    char norm[256];
    dots_to_dashes_versioned(norm, sizeof norm, lower);
    snprintf(out, out_size, "%s", norm);
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("normalize_anthropic");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_anthropic <model>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[256];
        usage_pricing_normalize_anthropic_model(argv[i], buf, sizeof buf);
    }
    return 0;
}
