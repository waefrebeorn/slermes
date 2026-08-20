/* gate: usage_pricing_normalize_bedrock_model — traced C copy of
 * src/agent/usage_pricing.c::usage_pricing_normalize_bedrock_model.
 * Faithful port of LIVE Python agent/usage_pricing.py::_normalize_bedrock_model_name.
 * WCUP_* records the SAME control structure so the cup matches iff the C port
 * executes the identical branch ladder as LIVE Python.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static void dots_to_dashes_versioned(char *out, size_t outsz, const char *in) {
    /* simplified port: replace D.D version with D-D */
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < outsz; i++) {
        if (in[i] == '.' && in[i+1] >= '0' && in[i+1] <= '9' && i > 0 && in[i-1] >= '0' && in[i-1] <= '9') {
            out[j++] = '-';
        } else {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

static void usage_pricing_normalize_bedrock_model(const char *model, char *out, size_t out_size) {
    WCUP_BEGIN("normalize_bedrock");
    if (!out || out_size == 0) { WCUP_BRANCH(1); WCUP_VALSTR(""); WCUP_VALSTR(""); char c[32]; WCUP_FINALIZE(c,sizeof c); printf("%s\n",c); WCUP_END("normalize_bedrock"); return; }
    if (!model) { WCUP_BRANCH(1); out[0]='\0'; WCUP_VALSTR("null"); WCUP_VALSTR(""); char c[32]; WCUP_FINALIZE(c,sizeof c); printf("%s\n",c); WCUP_END("normalize_bedrock"); return; }
    WCUP_BRANCH(0);
    char lower[256];
    size_t i = 0;
    while (model[i] && i < sizeof(lower) - 1) { lower[i] = (char)tolower((unsigned char)model[i]); i++; }
    lower[i] = '\0';
    const char *prefixes[] = {"us.", "global.", "eu.", "apac.", "ap.", "au.", "jp.", "ca.", "sa.", "me.", "af."};
    int stripped = 0;
    for (int k = 0; k < 11; k++) {
        size_t pl = strlen(prefixes[k]);
        if (strncmp(lower, prefixes[k], pl) == 0) {
            memmove(lower, lower + pl, strlen(lower + pl) + 1);
            stripped = 1; break;
        }
    }
    WCUP_BRANCH(stripped);
    char norm[256];
    dots_to_dashes_versioned(norm, sizeof norm, lower);
    /* mirror fixed C: trailing-component stripping (":N", "-vN", "-DATE") */
    {
        size_t len = strlen(norm);
        char *colon = strrchr(norm, ':');
        if (colon && colon[1] && strchr(colon, ':') == colon) {
            const char *p = colon + 1; int all_digit = 1;
            for (; *p; p++) if (!isdigit((unsigned char)*p)) { all_digit = 0; break; }
            if (all_digit) { *colon = '\0'; len = colon - norm; }
        }
        char *vdash = NULL;
        if (len >= 2 && norm[len-2] == '-' && norm[len-3] == 'v' && isdigit((unsigned char)norm[len-1])) vdash = norm + len - 3;
        else if (len >= 3 && norm[len-3] == '-' && norm[len-2] == 'v' && isdigit((unsigned char)norm[len-1])) vdash = norm + len - 3;
        if (vdash) { *vdash = '\0'; len = vdash - norm; }
        if (len >= 9 && norm[len-9] == '-') {
            int date = 1;
            for (int d = 1; d <= 8; d++) if (!isdigit((unsigned char)norm[len-d])) { date = 0; break; }
            if (date) norm[len-9] = '\0';
        }
    }
    snprintf(out, out_size, "%s", norm);
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("normalize_bedrock");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_bedrock <model>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[256];
        usage_pricing_normalize_bedrock_model(argv[i], buf, sizeof buf);
    }
    return 0;
}
