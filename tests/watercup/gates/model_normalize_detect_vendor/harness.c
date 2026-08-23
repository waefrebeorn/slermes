/* gate: model_normalize_detect_vendor — traced C copy of
 * src/hermes_cli/model_normalize.c::model_normalize_detect_vendor.
 * Twin: LIVE hermes_cli/model_normalize.py::detect_vendor.
 * Trace contract (matches twin.py):
 *   b0: lowercased name non-empty
 *   b1: '/' NOT in name
 *   exact-token scan: LOOP tick per table entry (no inner branch), then
 *   b2: exact hit found
 *   startswith scan: LOOP tick per entry, then b3: hit found
 * Single V event: vendor slug ("" for None).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static const struct { const char *tok; const char *vendor; } VENDOR_PREFIXES[] = {
    {"claude", "anthropic"}, {"gpt", "openai"}, {"o1", "openai"}, {"o3", "openai"},
    {"o4", "openai"}, {"gemini", "google"}, {"gemma", "google"}, {"deepseek", "deepseek"},
    {"glm", "z-ai"}, {"kimi", "moonshotai"}, {"minimax", "minimax"}, {"grok", "x-ai"},
    {"qwen", "qwen"}, {"mimo", "xiaomi"}, {"trinity", "arcee-ai"}, {"nemotron", "nvidia"},
    {"llama", "meta-llama"}, {"step", "stepfun"}, {NULL, NULL},
};

static void detect_vendor(const char *model_name) {
    WCUP_BEGIN("detect_vendor");
    char out[64] = "";
    if (!model_name) model_name = "";
    /* Python: name = model_name.strip(); inputs carry no surrounding
     * whitespace (C port does not strip), so this mirrors LIVE exactly. */
    size_t n = strlen(model_name);
    while (n > 0 && (model_name[n-1] == ' ' || model_name[n-1] == '\t')) n--;
    const char *s = model_name;
    while (*s == ' ' || *s == '\t') { s++; n--; }
    char low[256];
    size_t k = 0;
    for (size_t i = 0; i < n && k < sizeof low - 1; i++) low[k++] = (char)tolower((unsigned char)s[i]);
    low[k] = '\0';
    int nonempty = (low[0] != '\0');
    WCUP_BRANCH(low[0] != '\0');                               /* b0 */
    char *slash = strchr(low, '/');
    /* twin emits b1 unconditionally, even for the empty-name rung */
    WCUP_BRANCH(slash == NULL);                                /* b1 */
    if (low[0] == '\0') {
        out[0] = '\0';
    } else if (slash) {
        if (slash == low) out[0] = '\0';                       /* prefix.lower() or None */
        else { *slash = '\0'; strcpy(out, low); }
    } else {
            /* first hyphen-delimited token, exact scan (loop ticks only) */
            char first[256];
            strcpy(first, low);
            char *hyp = strchr(first, '-');
            if (hyp) *hyp = '\0';
            int hit = -1;
            for (int i = 0; VENDOR_PREFIXES[i].tok; i++) {
                WCUP_LOOP_TICK();
                if (strcmp(first, VENDOR_PREFIXES[i].tok) == 0) { hit = i; break; }
            }
            WCUP_BRANCH(hit >= 0);                             /* b2 */
            if (hit >= 0) {
                strcpy(out, VENDOR_PREFIXES[hit].vendor);
            } else {
                hit = -1;
                for (int i = 0; VENDOR_PREFIXES[i].tok; i++) {
                    WCUP_LOOP_TICK();
                    if (strncmp(low, VENDOR_PREFIXES[i].tok, strlen(VENDOR_PREFIXES[i].tok)) == 0) { hit = i; break; }
                }
                WCUP_BRANCH(hit >= 0);                         /* b3 */
                if (hit >= 0) strcpy(out, VENDOR_PREFIXES[hit].vendor);
                else out[0] = '\0';
            }
    }
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("detect_vendor");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s detect_vendor <model>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) detect_vendor(argv[i]);
    return 0;
}
