/* gate: vision_supports_media_in_tool_results — traced copy of
 * src/tools/vision.c::vision_supports_media_in_tool_results
 * (LIVE twin: tools/vision_tools.py::_supports_media_in_tool_results).
 *
 * Input encoding: "<provider>|<model>" (split on first '|').
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B0: empty provider                     -> false
 *   B1: provider in AGGREGATORS            -> true
 *   B2: provider in ANTHROPIC set          -> true
 *   B3: provider in OPENAI set             -> true
 *   B4: provider in GEMINI set -> {
 *       B4a: empty model                   -> false
 *       B4b: gemini-3 family substring     -> true }
 *   B5: default                            -> false
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static bool in_set(const char *v, const char *const *set) {
    for (int i = 0; set[i]; i++)
        if (strcmp(v, set[i]) == 0) return true;
    return false;
}

static bool has_sub(const char *hay, const char *needle) {
    return strstr(hay, needle) != NULL;
}

static bool wc_supports_media(const char *provider, const char *model) {
    WCUP_BEGIN("supports_media_in_tool_results");
    static const char *const AGG[] = {"openrouter", "nous", "vertex", "bedrock",
        "anthropic-vertex", "google-vertex", NULL};
    static const char *const ANT[] = {"anthropic", "claude", "anthropic-direct", NULL};
    static const char *const OAI[] = {"openai", "openai-chat", "openai-codex",
        "azure-openai", NULL};
    static const char *const GEM[] = {"google", "gemini", "google-gemini",
        "google-vertex-gemini", NULL};
    bool result = false;
    if (!provider || !provider[0]) {                 /* B0 */
        WCUP_BRANCH(1);
        goto out;
    }
    WCUP_BRANCH(0);
    if (in_set(provider, AGG)) {                     /* B1 */
        WCUP_BRANCH(1);
        result = true;
        goto out;
    }
    WCUP_BRANCH(0);
    if (in_set(provider, ANT)) {                     /* B2 */
        WCUP_BRANCH(1);
        result = true;
        goto out;
    }
    WCUP_BRANCH(0);
    if (in_set(provider, OAI)) {                     /* B3 */
        WCUP_BRANCH(1);
        result = true;
        goto out;
    }
    WCUP_BRANCH(0);
    if (in_set(provider, GEM)) {                     /* B4 */
        WCUP_BRANCH(1);
        if (!model || !model[0]) {                   /* B4a */
            WCUP_BRANCH(1);
            result = false;
            goto out;
        }
        WCUP_BRANCH(0);
        result = has_sub(model, "gemini-3.") || has_sub(model, "gemini-3-") ||
                 strcmp(model, "gemini-3") == 0 ||
                 has_sub(model, "gemini-pro-3") || has_sub(model, "gemini-flash-3");
        WCUP_BRANCH(result);                         /* B4b */
        goto out;
    }
    WCUP_BRANCH(0);
    result = false;                                  /* B5 */
    WCUP_BRANCH(0);
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("supports_media_in_tool_results");
        return result;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_supports_media <prov|model>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[512];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *bar = strchr(buf, '|');
        const char *model = "";
        if (bar) { *bar = '\0'; model = bar + 1; }
        wc_supports_media(buf, model);
    }
    return 0;
}
