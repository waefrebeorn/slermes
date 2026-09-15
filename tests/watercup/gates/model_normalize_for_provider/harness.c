/* gate: model_normalize_for_provider — links the REAL C port
 * src/hermes_cli/model_normalize.c::model_normalize_for_provider (+ its
 * model_catalog.c dependency) and traces the dispatch ladder around it.
 * LIVE twin: hermes_cli/model_normalize.py::normalize_model_for_provider.
 *
 * Input argv format: "<provider><TAB><model_input>".
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: empty model after strip
 *   VSTR(resolved provider alias)
 *   B2: aggregator            B3: opencode family
 *     B4: slash present         B5: zen && claude-
 *   B6: anthropic               B7: bare still has slash
 *   B8: strip-vendor-only fam   B9: copilot/copilot-acp sub-arm
 *                               B10: unchanged && openai/ prefix
 *   B11: deepseek               B12: bare still has slash
 *   B13: matching-prefix-strip  B14: lowercase-model provider
 *   B15: authoritative native
 *   VSTR(final normalized id)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

char *model_normalize_for_provider(const char *model_input, const char *target_provider);
const char *model_normalize_provider(const char *provider);

/* Out-of-path externals owned by models_helpers.o (not on the normalize
 * path — only used by catalog cache/lookup helpers we never call). */
char *provider_models_cache_path(void) { return strdup("/dev/null"); }
char *credential_fingerprint(const char *p) { (void)p; return strdup("stub"); }

static bool is_aggregator(const char *p) {
    return !strcmp(p, "openrouter") || !strcmp(p, "nous") || !strcmp(p, "kilocode");
}
static bool is_matching_prefix_strip(const char *p) {
    static const char *set[] = {"zai","kimi-coding","kimi-coding-cn","minimax","minimax-oauth",
        "minimax-cn","alibaba","qwen-oauth","xiaomi","arcee","ollama-cloud","custom","gemini","xai",NULL};
    for (int i = 0; set[i]; i++) if (!strcmp(p, set[i])) return true;
    return false;
}
static bool is_lowercase_model_provider(const char *p) { return !strcmp(p, "xiaomi"); }
static bool is_strip_vendor_only(const char *p) {
    return !strcmp(p, "copilot") || !strcmp(p, "copilot-acp") || !strcmp(p, "openai-codex");
}
static bool is_authoritative_native(const char *p) { return !strcmp(p, "huggingface"); }

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *input) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);
    char *tab = strchr(buf, '\t');
    const char *provider = buf;
    const char *model = buf;
    if (tab) { *tab = '\0'; provider = buf; model = tab + 1; }

    WCUP_BEGIN("normalize_model_for_provider");

    /* trim (same as port entry) */
    char *name = strdup(model);
    char *s = name, *e = name + strlen(name);
    while (s < e && isspace((unsigned char)*s)) s++;
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';

    WCUP_BRANCH(!*s);                                        /* B1 */
    const char *pv = model_normalize_provider(provider);

    WCUP_BRANCH(is_aggregator(pv));                          /* B2 */
    bool oc = !strcmp(pv, "opencode-zen") || !strcmp(pv, "opencode-go");
    WCUP_BRANCH(oc);                                         /* B3 */
    if (oc) {
        const char *sl = strchr(s, '/');
        WCUP_BRANCH(sl != NULL && sl[1]);                    /* B4 */
        WCUP_BRANCH(!strcmp(pv, "opencode-zen") &&
                    strncasecmp(sl ? sl + 1 : s, "claude-", 7) == 0); /* B5 */
    } else {
        WCUP_BRANCH(!strcmp(pv, "anthropic"));               /* B6 */
        if (!strcmp(pv, "anthropic")) {
            const char *sl = strchr(s, '/');
            char pfx[256];
            bool match = false;
            if (sl && sl != s && sl[1]) {
                size_t pl = (size_t)(sl - s);
                memcpy(pfx, s, pl); pfx[pl] = '\0';
                const char *np = model_normalize_provider(pfx);
                match = np && !strcasecmp(np, pv);
            }
            WCUP_BRANCH(!match && strchr(s, '/'));           /* B7 */
        } else {
            WCUP_BRANCH(is_strip_vendor_only(pv));           /* B8 */
            if (is_strip_vendor_only(pv)) {
                WCUP_BRANCH(!strcmp(pv, "copilot") || !strcmp(pv, "copilot-acp")); /* B9 */
                const char *sl = strchr(s, '/');
                bool match = false;
                if (sl && sl != s && sl[1]) {
                    char pfx[256];
                    size_t pl = (size_t)(sl - s);
                    memcpy(pfx, s, pl); pfx[pl] = '\0';
                    const char *np = model_normalize_provider(pfx);
                    match = np && !strcasecmp(np, pv);
                }
                WCUP_BRANCH(!match && strncmp(s, "openai/", 7) == 0); /* B10 */
            } else {
                WCUP_BRANCH(!strcmp(pv, "deepseek"));        /* B11 */
                if (!strcmp(pv, "deepseek")) {
                    const char *sl = strchr(s, '/');
                    bool match = false;
                    if (sl && sl != s && sl[1]) {
                        char pfx[256];
                        size_t pl = (size_t)(sl - s);
                        memcpy(pfx, s, pl); pfx[pl] = '\0';
                        const char *np = model_normalize_provider(pfx);
                        match = np && !strcasecmp(np, pv);
                    }
                    WCUP_BRANCH(!match && sl != NULL);       /* B12 */
                } else {
                    WCUP_BRANCH(is_matching_prefix_strip(pv)); /* B13 */
                    if (is_matching_prefix_strip(pv))
                        WCUP_BRANCH(is_lowercase_model_provider(pv)); /* B14 */
                    else
                        WCUP_BRANCH(is_authoritative_native(pv)); /* B15 placeholder path */
                }
            }
        }
    }

    char *result = model_normalize_for_provider(model, provider);
    WCUP_VALSTR(result ? result : "");

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(name);
    free(result);
    WCUP_END("normalize_model_for_provider");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_model_for_provider <prov\\tmodel...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
