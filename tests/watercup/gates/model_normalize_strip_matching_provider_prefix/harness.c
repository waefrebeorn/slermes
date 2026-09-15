/* gate: model_normalize_strip_matching_provider_prefix
 * Calls the REAL C port src/hermes_cli/model_normalize.c::model_normalize_strip_match_prefix
 * (linked from the repo's own object file) against LIVE Python
 * hermes_cli/model_normalize.py::_strip_matching_provider_prefix.
 *
 * The port's on-path dependency model_normalize_provider() (provider alias
 * resolution, normally in src/hermes_cli/model_catalog.c which drags an SSL/
 * HTTP dep chain into link) is embedded here VERBATIM — same table, same body.
 *
 * Trace ladder (both sides):
 *   b0: '/' in model_name?
 *   b1: prefix non-empty?
 *   b2: remainder non-empty?
 *   b3: normalized(prefix) == normalized(target)?
 *   V: result string
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

char *model_normalize_strip_match_prefix(const char *model_name, const char *target_provider);

/* ── VERBATIM from src/hermes_cli/model_catalog.c (on-path dependency) ── */
typedef struct { const char *alias; const char *canon; } alias_t;
static const alias_t PROVIDER_ALIASES[] = {
    { "glm", "zai" }, { "z-ai", "zai" }, { "z.ai", "zai" }, { "zhipu", "zai" },
    { "github", "copilot" }, { "github-copilot", "copilot" }, { "github-models", "copilot" },
    { "github-model", "copilot" }, { "github-copilot-acp", "copilot-acp" }, { "copilot-acp-agent", "copilot-acp" },
    { "google", "gemini" }, { "google-gemini", "gemini" }, { "google-ai-studio", "gemini" },
    { "google-vertex", "vertex" }, { "vertex-ai", "vertex" }, { "gcp-vertex", "vertex" },
    { "vertexai", "vertex" }, { "kimi", "kimi-coding" }, { "moonshot", "kimi-coding" },
    { "kimi-cn", "kimi-coding-cn" }, { "moonshot-cn", "kimi-coding-cn" },
    { "step", "stepfun" }, { "stepfun-coding-plan", "stepfun" },
    { "arcee-ai", "arcee" }, { "arceeai", "arcee" }, { "gmi-cloud", "gmi" }, { "gmicloud", "gmi" },
    { "minimax-china", "minimax-cn" }, { "minimax_cn", "minimax-cn" },
    { "minimax-portal", "minimax-oauth" }, { "minimax-global", "minimax-oauth" }, { "minimax_oauth", "minimax-oauth" },
    { "claude", "anthropic" }, { "claude-code", "anthropic" }, { "deep-seek", "deepseek" },
    { "opencode", "opencode-zen" }, { "zen", "opencode-zen" }, { "go", "opencode-go" }, { "opencode-go-sub", "opencode-go" },
    { "kilo", "kilocode" }, { "kilo-code", "kilocode" }, { "kilo-gateway", "kilocode" },
    { "dashscope", "alibaba" }, { "aliyun", "alibaba" }, { "qwen", "alibaba" }, { "alibaba-cloud", "alibaba" },
    { "qwen-portal", "qwen-oauth" }, { "hf", "huggingface" }, { "hugging-face", "huggingface" },
    { "huggingface-hub", "huggingface" }, { "novita-ai", "novita" }, { "novitaai", "novita" },
    { "mimo", "xiaomi" }, { "xiaomi-mimo", "xiaomi" },
    { "tencent", "tencent-tokenhub" }, { "tokenhub", "tencent-tokenhub" }, { "tencent-cloud", "tencent-tokenhub" },
    { "tencentmaas", "tencent-tokenhub" }, { "aws", "bedrock" }, { "aws-bedrock", "bedrock" },
    { "amazon-bedrock", "bedrock" }, { "amazon", "bedrock" }, { "grok", "xai" }, { "grok-oauth", "xai-oauth" },
    { "xai-oauth", "xai-oauth" }, { "x-ai-oauth", "xai-oauth" }, { "xai-grok-oauth", "xai-oauth" },
    { "x-ai", "xai" }, { "x.ai", "xai" }, { "nim", "nvidia" }, { "nvidia-nim", "nvidia" },
    { "build-nvidia", "nvidia" }, { "nemotron", "nvidia" }, { "lmstudio", "lmstudio" },
    { "lm-studio", "lmstudio" }, { "lm_studio", "lmstudio" }, { "ollama", "custom" },
    { "ollama_cloud", "ollama-cloud" },
};
#define N_ALIASES (int)(sizeof(PROVIDER_ALIASES)/sizeof(PROVIDER_ALIASES[0]))

const char *model_normalize_provider(const char *provider) {
    static const char *dflt = "openrouter";
    if (!provider || !*provider) return dflt;
    /* "auto" passes through unchanged. */
    if (strcmp(provider, "auto") == 0) return "auto";
    static char lower[128];
    size_t n = strlen(provider);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)provider[i]);
    lower[n] = '\0';
    for (int i = 0; i < N_ALIASES; i++) {
        if (strcmp(PROVIDER_ALIASES[i].alias, lower) == 0)
            return PROVIDER_ALIASES[i].canon;
    }
    return lower; /* alias not found: lowercased input */
}
/* ── end verbatim copy ── */

static void run_one(const char *input) {
    WCUP_BEGIN("strip_matching_provider_prefix");
    /* argv: "<model_name>\t<target_provider>" */
    char *tmp = strdup(input);
    char *tab = strchr(tmp, '\t');
    const char *model_name = tmp;
    const char *target = "";
    if (tab) { *tab = '\0'; target = tab + 1; }

    const char *slash = strchr(model_name, '/');
    WCUP_BRANCH(slash != NULL);                       /* b0: '/' present? */
    const char *result = model_name;
    int done = 0;
    if (!slash) done = 1;

    WCUP_BRANCH(slash != model_name);                 /* b1: prefix non-empty */
    if (slash == model_name && !done) done = 1;
    WCUP_BRANCH(slash && slash[1] != '\0');           /* b2: remainder non-empty */
    if (slash && slash[1] == '\0' && !done) done = 1;

    if (!done) {
        char *prefix = strndup(model_name, (size_t)(slash - model_name));
        char *np = strdup(model_normalize_provider(prefix));
        char *nt = strdup(model_normalize_provider(target));
        int match = strcmp(np, nt) == 0;
        WCUP_BRANCH(match);                           /* b3: aliases equal */
        free(np); free(nt); free(prefix);
        result = match ? slash + 1 : model_name;
    }
    WCUP_VALSTR(result);
    free(model_normalize_strip_match_prefix(model_name, target)); /* REAL C fn */
    free(tmp);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("strip_matching_provider_prefix");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s strip_matching_provider_prefix \"<name>\\t<provider>\"...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
