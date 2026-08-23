/* gate: models_opencode_model_api_mode — traced copy of
 * src/hermes_cli/models_helpers.c::opencode_model_api_mode (after the
 * grok-/muse-spark/qwen ladder fix that aligned it with LIVE).
 * LIVE twin: hermes_cli/models.py::opencode_model_api_mode.
 *
 * argv input format: "provider|model" (split on first '|').
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: normalized model id empty -> chat_completions
 *   B2: family == opencode-go?
 *     LOOP prefix chain [gpt-, grok-, muse-spark, minimax-, qwen]
 *   B3: family == opencode-zen?
 *     LOOP prefix chain [claude-, gpt-, grok-, muse-spark, qwen]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"

/* mirror of models_helpers.c normalize_provider_low: lowercase + trim,
 * NULL/empty provider -> "openrouter". */
static void wc_normalize_provider_low(char *out, size_t outsz, const char *provider)
{
    const char *p = provider ? provider : "openrouter";
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    if (n >= outsz) n = outsz - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
}

/* mirror of models_helpers.c normalize_opencode_model_id (unprefixed-model
 * subset used by this gate): strip "prov/" prefix when family is zen/go. */
static char *wc_normalize_model(const char *prov, const char *model)
{
    char *current = strdup(model ? model : "");
    if (!current[0] ||
        (strcmp(prov, "opencode-zen") != 0 && strcmp(prov, "opencode-go") != 0))
        return current;
    char prefix[160];
    snprintf(prefix, sizeof prefix, "%s/", prov);
    size_t pl = strlen(prefix);
    if (strncasecmp(current, prefix, pl) == 0) {
        char *r = strdup(current + pl);
        free(current);
        return r;
    }
    return current;
}

static bool starts_with(const char *s, const char *pre) {
    return strncmp(s, pre, strlen(pre)) == 0;
}

static const char *wc_opencode_model_api_mode(const char *provider,
                                              const char *model) {
    WCUP_BEGIN("opencode_model_api_mode");
    const char *result = "chat_completions";
    char prov[128];
    wc_normalize_provider_low(prov, sizeof prov, provider);
    const char *fam = prov;
    if (strcmp(fam, "opencode-free") == 0) fam = "opencode-zen";
    char *norm = wc_normalize_model(prov, model);
    char *low = strdup(norm);
    for (size_t i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);

    if (low[0] == '\0') {                       /* B1 */
        WCUP_BRANCH(1);
        result = "chat_completions";
    } else {
        WCUP_BRANCH(0);
        int is_go = (strcmp(fam, "opencode-go") == 0);
        WCUP_BRANCH(is_go);                     /* B2 */
        if (is_go) {
            static const char *const GO[5][2] = {
                {"gpt-", "codex_responses"}, {"grok-", "codex_responses"},
                {"muse-spark", "codex_responses"}, {"minimax-", "anthropic_messages"},
                {"qwen", "anthropic_messages"}};
            for (int i = 0; i < 5; i++) {
                WCUP_LOOP_TICK();
                bool m = starts_with(low, GO[i][0]);
                WCUP_BRANCH(m);
                if (m) { result = GO[i][1]; goto out; }
            }
            result = "chat_completions";
        } else {
            int is_zen = (strcmp(fam, "opencode-zen") == 0);
            WCUP_BRANCH(is_zen);                /* B3 */
            if (is_zen) {
                static const char *const ZEN[5][2] = {
                    {"claude-", "anthropic_messages"}, {"gpt-", "codex_responses"},
                    {"grok-", "codex_responses"}, {"muse-spark", "codex_responses"},
                    {"qwen", "anthropic_messages"}};
                for (int i = 0; i < 5; i++) {
                    WCUP_LOOP_TICK();
                    bool m = starts_with(low, ZEN[i][0]);
                    WCUP_BRANCH(m);
                    if (m) { result = ZEN[i][1]; goto out; }
                }
                result = "chat_completions";
            }
        }
    }
out:
    free(norm); free(low);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("opencode_model_api_mode");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s opencode_model_api_mode <provider|model>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *bar = strchr(arg, '|');
        const char *model = "";
        if (bar) { *bar = '\0'; model = bar + 1; }
        wc_opencode_model_api_mode(arg, model);
        free(arg);
    }
    return 0;
}
