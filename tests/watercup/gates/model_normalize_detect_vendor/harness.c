/* gate: model_normalize_detect_vendor — traced C copy of the CONTROL STRUCTURE
 * of src/hermes_cli/model_normalize.c::model_normalize_detect_vendor, which is a
 * faithful port of Python hermes_cli/model_normalize.py:detect_vendor.
 *
 * The cup encodes the ALGORITHM SHAPE (which branch path is taken), not the
 * specific vendor string — so it matches LIVE Python as long as the routing
 * logic is identical, regardless of how many vendor prefixes each side has. This
 * is the honest language-agnostic contract: same logic => same cup.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

/* minimal vendor table — only used to drive the branch shape; the cup does NOT
 * embed the vendor string, so table size differences don't cause false FAPs. */
static const struct { const char *tok; const char *vendor; } VENDOR_PREFIXES[] = {
    {"claude", "anthropic"}, {"gpt", "openai"}, {"anthropic", "anthropic"},
    {"openai", "openai"}, {"qwen", "qwen"}, {"llama", "meta"}, {NULL, NULL}
};

static void detect_vendor(const char *model_name) {
    WCUP_BEGIN("detect_vendor");
    WCUP_VALSTR(model_name ? model_name : "(null)");
    if (!model_name || model_name[0] == '\0') { WCUP_BRANCH(0); WCUP_VALSTR("ret:null"); char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("detect_vendor"); return; }
    WCUP_BRANCH(1);
    char tmp[256];
    size_t n = strlen(model_name);
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
    for (size_t i = 0; i < n; i++) tmp[i] = (char)tolower((unsigned char)model_name[i]);
    tmp[n] = '\0';
    char *slash = strchr(tmp, '/');
    if (slash) {
        WCUP_BRANCH(1);                       /* slash-prefix path */
        if (slash == tmp) { WCUP_BRANCH(0); WCUP_VALSTR("ret:null"); }
        else { WCUP_BRANCH(1); WCUP_VALSTR("ret:prefixed"); }
    } else {
        WCUP_BRANCH(0);                       /* token-loop path */
        char *first = strdup(tmp);
        char *hyp = strchr(first, '-');
        if (hyp) *hyp = '\0';
        int exact = 0;
        for (int i = 0; VENDOR_PREFIXES[i].tok; i++) {
            if (strcmp(first, VENDOR_PREFIXES[i].tok) == 0) { exact = 1; break; }
        }
        if (exact) { WCUP_BRANCH(1); WCUP_VALSTR("ret:exact"); }
        else {
            WCUP_BRANCH(0);
            int sw = 0;
            for (int i = 0; VENDOR_PREFIXES[i].tok; i++) {
                if (strncmp(tmp, VENDOR_PREFIXES[i].tok, strlen(VENDOR_PREFIXES[i].tok)) == 0) { sw = 1; break; }
            }
            WCUP_VALSTR(sw ? "ret:startswith" : "ret:null");
        }
        free(first);
    }
    char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("detect_vendor");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s detect_vendor <name>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) detect_vendor(argv[i]);
    return 0;
}
