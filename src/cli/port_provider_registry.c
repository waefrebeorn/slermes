/*
 * port_provider_registry.c — Faithful C11 port of the Hermes provider catalog
 * and runtime-provider resolution helpers. See port_provider_registry.h.
 */
#include "port_provider_registry.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ─── Default base-URL / OAuth constants (auth.py top-of-file) ─────────── */
#define D_NOUS_PORTAL   "https://portal.nousresearch.com"
#define D_NOUS_INFER    "https://inference-api.nousresearch.com/v1"
#define D_NOUS_CLIENT   "hermes-cli"
#define D_NOUS_SCOPE    "inference:invoke"
#define D_CODEX_BASE    "https://chatgpt.com/backend-api/codex"
#define D_XAI_OAUTH     "https://api.x.ai/v1"
#define D_QWEN_BASE     "https://portal.qwen.ai/v1"
#define D_GH_MODELS     "https://api.githubcopilot.com"
#define D_COPILOT_ACP   "acp://copilot"
#define D_OLLAMA_CLOUD  "https://ollama.com/v1"
#define D_STEPFUN_INTL  "https://api.stepfun.ai/step_plan/v1"
#define MM_CLIENT_ID    "78257093-7e40-4613-99e0-527b14b39113"
#define MM_SCOPE        "group_id profile model.completion"
#define MM_GLOBAL_BASE  "https://api.minimax.io"
#define MM_GLOBAL_INFER "https://api.minimax.io/anthropic"

/* NULL-terminated env-var arrays (static literals). */
static const char *ENV_OPENAI[]      = { "OPENAI_API_KEY", NULL };
static const char *ENV_LM[]          = { "LM_API_KEY", NULL };
static const char *ENV_COPILOT[]     = { "COPILOT_GITHUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN", NULL };
static const char *ENV_GEMINI[]      = { "GOOGLE_API_KEY", "GEMINI_API_KEY", NULL };
static const char *ENV_ZAI[]         = { "GLM_API_KEY", "ZAI_API_KEY", "Z_AI_API_KEY", NULL };
static const char *ENV_KIMI[]        = { "KIMI_API_KEY", "KIMI_CODING_API_KEY", NULL };
static const char *ENV_KIMI_CN[]     = { "KIMI_CN_API_KEY", NULL };
static const char *ENV_STEPFUN[]     = { "STEPFUN_API_KEY", NULL };
static const char *ENV_ARCEE[]       = { "ARCEEAI_API_KEY", NULL };
static const char *ENV_GMI[]         = { "GMI_API_KEY", NULL };
static const char *ENV_MINIMAX[]     = { "MINIMAX_API_KEY", NULL };
static const char *ENV_ANTHROPIC[]   = { "ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN", "CLAUDE_CODE_OAUTH_TOKEN", NULL };
static const char *ENV_ALIBABA[]     = { "DASHSCOPE_API_KEY", NULL };
static const char *ENV_ALIBABA_CP[]  = { "ALIBABA_CODING_PLAN_API_KEY", "DASHSCOPE_API_KEY", NULL };
static const char *ENV_MINIMAX_CN[]  = { "MINIMAX_CN_API_KEY", NULL };
static const char *ENV_DEEPSEEK[]    = { "DEEPSEEK_API_KEY", NULL };
static const char *ENV_XAI[]         = { "XAI_API_KEY", NULL };
static const char *ENV_NVIDIA[]      = { "NVIDIA_API_KEY", NULL };
static const char *ENV_OCZEN[]       = { "OPENCODE_ZEN_API_KEY", NULL };
static const char *ENV_OCGO[]        = { "OPENCODE_GO_API_KEY", NULL };
static const char *ENV_KILOCODE[]    = { "KILOCODE_API_KEY", NULL };
static const char *ENV_HF[]          = { "HF_TOKEN", NULL };
static const char *ENV_XIAOMI[]      = { "XIAOMI_API_KEY", NULL };
static const char *ENV_TOKENHUB[]    = { "TOKENHUB_API_KEY", NULL };
static const char *ENV_OLLAMA[]      = { "OLLAMA_API_KEY", NULL };
static const char *ENV_AZURE[]       = { "AZURE_FOUNDRY_API_KEY", NULL };
static const char *ENV_NONE[]        = { NULL };

/* PoP: PROVIDER_REGISTRY @ hermes_cli/auth.py:PROVIDER_REGISTRY */
static const provider_config_t CATALOG[] = {
    { "nous", "Nous Portal", "oauth_device_code", D_NOUS_PORTAL, D_NOUS_INFER, D_NOUS_CLIENT, D_NOUS_SCOPE, "", ENV_NONE },
    { "openai-codex", "OpenAI Codex", "oauth_external", "", D_CODEX_BASE, "", "", "", ENV_NONE },
    { "openai-api", "OpenAI API", "api_key", "", "https://api.openai.com/v1", "", "", "OPENAI_BASE_URL", ENV_OPENAI },
    { "xai-oauth", "xAI Grok OAuth (SuperGrok / Premium+)", "oauth_external", "", D_XAI_OAUTH, "", "", "", ENV_NONE },
    { "qwen-oauth", "Qwen OAuth", "oauth_external", "", D_QWEN_BASE, "", "", "", ENV_NONE },
    { "lmstudio", "LM Studio", "api_key", "", "http://127.0.0.1:1234/v1", "", "", "LM_BASE_URL", ENV_LM },
    { "copilot", "GitHub Copilot", "api_key", "", D_GH_MODELS, "", "", "COPILOT_API_BASE_URL", ENV_COPILOT },
    { "copilot-acp", "GitHub Copilot ACP", "external_process", "", D_COPILOT_ACP, "", "", "COPILOT_ACP_BASE_URL", ENV_NONE },
    { "gemini", "Google AI Studio", "api_key", "", "https://generativelanguage.googleapis.com/v1beta", "", "", "GEMINI_BASE_URL", ENV_GEMINI },
    { "zai", "Z.AI / GLM", "api_key", "", "https://api.z.ai/api/paas/v4", "", "", "GLM_BASE_URL", ENV_ZAI },
    { "kimi-coding", "Kimi / Moonshot", "api_key", "", "https://api.moonshot.ai/v1", "", "", "KIMI_BASE_URL", ENV_KIMI },
    { "kimi-coding-cn", "Kimi / Moonshot (China)", "api_key", "", "https://api.moonshot.cn/v1", "", "", "", ENV_KIMI_CN },
    { "stepfun", "StepFun Step Plan", "api_key", "", D_STEPFUN_INTL, "", "", "STEPFUN_BASE_URL", ENV_STEPFUN },
    { "arcee", "Arcee AI", "api_key", "", "https://api.arcee.ai/api/v1", "", "", "ARCEE_BASE_URL", ENV_ARCEE },
    { "gmi", "GMI Cloud", "api_key", "", "https://api.gmi-serving.com/v1", "", "", "GMI_BASE_URL", ENV_GMI },
    { "minimax", "MiniMax", "api_key", "", "https://api.minimax.io/anthropic", "", "", "MINIMAX_BASE_URL", ENV_MINIMAX },
    { "minimax-oauth", "MiniMax (OAuth \xc2\xb7 minimax.io)", "oauth_minimax", MM_GLOBAL_BASE, MM_GLOBAL_INFER, MM_CLIENT_ID, MM_SCOPE, "", ENV_NONE },
    { "anthropic", "Anthropic", "api_key", "", "https://api.anthropic.com", "", "", "ANTHROPIC_BASE_URL", ENV_ANTHROPIC },
    { "alibaba", "Qwen Cloud", "api_key", "", "https://dashscope-intl.aliyuncs.com/compatible-mode/v1", "", "", "DASHSCOPE_BASE_URL", ENV_ALIBABA },
    { "alibaba-coding-plan", "Alibaba Cloud (Coding Plan)", "api_key", "", "https://coding-intl.dashscope.aliyuncs.com/v1", "", "", "ALIBABA_CODING_PLAN_BASE_URL", ENV_ALIBABA_CP },
    { "minimax-cn", "MiniMax (China)", "api_key", "", "https://api.minimaxi.com/anthropic", "", "", "MINIMAX_CN_BASE_URL", ENV_MINIMAX_CN },
    { "deepseek", "DeepSeek", "api_key", "", "https://api.deepseek.com/v1", "", "", "DEEPSEEK_BASE_URL", ENV_DEEPSEEK },
    { "xai", "xAI", "api_key", "", "https://api.x.ai/v1", "", "", "XAI_BASE_URL", ENV_XAI },
    { "nvidia", "NVIDIA NIM", "api_key", "", "https://integrate.api.nvidia.com/v1", "", "", "NVIDIA_BASE_URL", ENV_NVIDIA },
    { "opencode-zen", "OpenCode Zen", "api_key", "", "https://opencode.ai/zen/v1", "", "", "OPENCODE_ZEN_BASE_URL", ENV_OCZEN },
    { "opencode-go", "OpenCode Go", "api_key", "", "https://opencode.ai/zen/go/v1", "", "", "OPENCODE_GO_BASE_URL", ENV_OCGO },
    { "kilocode", "Kilo Code", "api_key", "", "https://api.kilo.ai/api/gateway", "", "", "KILOCODE_BASE_URL", ENV_KILOCODE },
    { "huggingface", "Hugging Face", "api_key", "", "https://router.huggingface.co/v1", "", "", "HF_BASE_URL", ENV_HF },
    { "xiaomi", "Xiaomi MiMo", "api_key", "", "https://api.xiaomimimo.com/v1", "", "", "XIAOMI_BASE_URL", ENV_XIAOMI },
    { "tencent-tokenhub", "Tencent TokenHub", "api_key", "", "https://tokenhub.tencentmaas.com/v1", "", "", "TOKENHUB_BASE_URL", ENV_TOKENHUB },
    { "ollama-cloud", "Ollama Cloud", "api_key", "", D_OLLAMA_CLOUD, "", "", "OLLAMA_BASE_URL", ENV_OLLAMA },
    { "bedrock", "AWS Bedrock", "aws_sdk", "", "https://bedrock-runtime.us-east-1.amazonaws.com", "", "", "BEDROCK_BASE_URL", ENV_NONE },
    { "vertex", "Google Vertex AI", "vertex", "", "", "", "", "", ENV_NONE },
    { "azure-foundry", "Azure Foundry", "api_key", "", "", "", "", "AZURE_FOUNDRY_BASE_URL", ENV_AZURE },
};
static const int CATALOG_N = (int)(sizeof(CATALOG) / sizeof(CATALOG[0]));

const provider_config_t *provider_registry_get(const char *id) {
    if (!id || !*id) return NULL;
    for (int i = 0; i < CATALOG_N; i++)
        if (strcmp(CATALOG[i].id, id) == 0) return &CATALOG[i];
    return NULL;
}
int provider_registry_count(void) { return CATALOG_N; }
const provider_config_t *provider_registry_at(int i) {
    return (i >= 0 && i < CATALOG_N) ? &CATALOG[i] : NULL;
}

/* ─── small string helpers ────────────────────────────────────────────── */
static char *dup_or_empty(const char *s) {
    if (!s) { char *e = malloc(1); if (e) e[0] = '\0'; return e; }
    char *d = malloc(strlen(s) + 1); if (d) strcpy(d, s); return d;
}
static char *strip_lower(const char *s) {
    if (!s) return dup_or_empty("");
    const char *a = s; while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a); while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1); if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)a[i]);
    out[n] = '\0';
    return out;
}

/* ─── utils.base_url_hostname / base_url_host_matches ──────────────────── */
/* PoP: base_url_hostname @ utils.py:base_url_hostname */
char *provider_base_url_hostname(const char *base_url) {
    if (!base_url) return dup_or_empty("");
    /* strip leading/trailing ws */
    const char *p = base_url; while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return dup_or_empty("");
    /* skip scheme:// if present */
    const char *host = p;
    const char *scheme = strstr(p, "://");
    if (scheme) host = scheme + 3;
    /* host ends at '/', '?', '#', or end. userinfo '@' precedes host. */
    const char *at = NULL;
    for (const char *q = host; *q && *q != '/' && *q != '?' && *q != '#'; q++)
        if (*q == '@') at = q;
    if (at) host = at + 1;
    /* copy up to delimiter or port ':' */
    size_t n = 0;
    for (const char *q = host; q[n] && q[n] != '/' && q[n] != '?' &&
                               q[n] != '#' && q[n] != ':'; ) n++;
    /* IPv6 literal [::1] — urlparse returns the inner address (no brackets) */
    char *out;
    if (host[0] == '[') {
        const char *close = strchr(host, ']');
        if (close) {
            size_t inner = (size_t)(close - host) - 1;  /* between [ and ] */
            out = malloc(inner + 1); if (!out) return NULL;
            for (size_t i = 0; i < inner; i++)
                out[i] = (char)tolower((unsigned char)host[i + 1]);
            out[inner] = '\0';
            return out;
        }
    }
    out = malloc(n + 1); if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)host[i]);
    out[n] = '\0';
    /* rstrip trailing dot */
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '.') out[--len] = '\0';
    return out;
}

/* PoP: base_url_host_matches @ utils.py:base_url_host_matches */
bool provider_base_url_host_matches(const char *base_url, const char *domain) {
    char *host = provider_base_url_hostname(base_url);
    if (!host || !*host) { free(host); return false; }
    char *dom = strip_lower(domain);
    if (dom) { size_t dl = strlen(dom); while (dl > 0 && dom[dl-1]=='.') dom[--dl]='\0'; }
    bool ok = false;
    if (dom && *dom) {
        if (strcmp(host, dom) == 0) ok = true;
        else {
            size_t hl = strlen(host), dl = strlen(dom);
            if (hl > dl + 1 && host[hl - dl - 1] == '.' &&
                strcmp(host + hl - dl, dom) == 0) ok = true;
        }
    }
    free(host); free(dom);
    return ok;
}

/* ─── runtime_provider.py pure helpers ────────────────────────────────── */
/* PoP: _normalize_custom_provider_name @ hermes_cli/runtime_provider.py:_normalize_custom_provider_name */
char *provider_normalize_custom_name(const char *value) {
    char *low = strip_lower(value);
    if (!low) return NULL;
    for (char *c = low; *c; c++) if (*c == ' ') *c = '-';
    return low;
}

/* PoP: _loopback_hostname @ hermes_cli/runtime_provider.py:_loopback_hostname */
bool provider_loopback_hostname(const char *host) {
    char *h = strip_lower(host);
    if (!h) return false;
    size_t l = strlen(h); while (l > 0 && h[l-1]=='.') h[--l]='\0';
    bool ok = (strcmp(h,"localhost")==0 || strcmp(h,"127.0.0.1")==0 ||
               strcmp(h,"::1")==0 || strcmp(h,"0.0.0.0")==0);
    free(h);
    return ok;
}

/* PoP: _parse_api_mode @ hermes_cli/runtime_provider.py:_parse_api_mode */
const char *provider_parse_api_mode(const char *raw) {
    char *m = strip_lower(raw);
    if (!m) return NULL;
    const char *r = NULL;
    if      (strcmp(m,"chat_completions")==0)   r = "chat_completions";
    else if (strcmp(m,"codex_responses")==0)    r = "codex_responses";
    else if (strcmp(m,"anthropic_messages")==0) r = "anthropic_messages";
    else if (strcmp(m,"bedrock_converse")==0)   r = "bedrock_converse";
    else if (strcmp(m,"codex_app_server")==0)   r = "codex_app_server";
    free(m);
    return r;
}

/* PoP: _provider_supports_explicit_api_mode @ hermes_cli/runtime_provider.py:_provider_supports_explicit_api_mode */
bool provider_supports_explicit_api_mode(const char *provider,
                                         const char *configured_provider) {
    char *np = strip_lower(provider);
    char *nc = strip_lower(configured_provider);
    bool r;
    if (!nc || !*nc) r = true;
    else if (np && strcmp(np,"custom")==0)
        r = (strcmp(nc,"custom")==0) || (strncmp(nc,"custom:",7)==0);
    else r = (np && strcmp(nc,np)==0);
    free(np); free(nc);
    return r;
}

/* Return the lowercased URL path (after host), trailing '/' stripped.
 * Caller frees. */
static char *url_path_lower(const char *base_url) {
    if (!base_url) return dup_or_empty("");
    const char *p = base_url; while (*p && isspace((unsigned char)*p)) p++;
    const char *host = p;
    const char *scheme = strstr(p, "://");
    if (scheme) host = scheme + 3;
    const char *slash = host;
    while (*slash && *slash != '/' && *slash != '?' && *slash != '#') slash++;
    /* path begins at slash */
    size_t n = 0;
    while (slash[n] && slash[n] != '?' && slash[n] != '#') n++;
    char *out = malloc(n + 1); if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)slash[i]);
    out[n] = '\0';
    size_t len = strlen(out); while (len > 0 && out[len-1]=='/') out[--len]='\0';
    return out;
}
static bool ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* PoP: _detect_api_mode_for_url @ hermes_cli/runtime_provider.py:_detect_api_mode_for_url */
const char *provider_detect_api_mode_for_url(const char *base_url) {
    char *host = provider_base_url_hostname(base_url);
    const char *r = NULL;
    if (host && *host) {
        if (strcmp(host,"api.x.ai")==0)            r = "codex_responses";
        else if (strcmp(host,"api.openai.com")==0) r = "codex_responses";
        else if (strcmp(host,"api.anthropic.com")==0) r = "anthropic_messages";
    }
    if (!r) {
        char *path = url_path_lower(base_url);
        if (path && (ends_with(path,"/anthropic") || ends_with(path,"/anthropic/v1")))
            r = "anthropic_messages";
        free(path);
    }
    if (!r && host && strcmp(host,"api.kimi.com")==0) {
        /* "/coding" anywhere in normalized url */
        char *low = strip_lower(base_url);
        if (low) {
            size_t l = strlen(low); while (l>0 && low[l-1]=='/') low[--l]='\0';
            if (strstr(low, "/coding")) r = "anthropic_messages";
            free(low);
        }
    }
    free(host);
    return r;
}

/* PoP: _anthropic_base_url_override_ok @ hermes_cli/runtime_provider.py:_anthropic_base_url_override_ok */
bool provider_anthropic_base_url_override_ok(const char *base_url) {
    if (!base_url) return false;
    const char *p = base_url; while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return false;
    char *host = provider_base_url_hostname(base_url);
    if (!host || !*host) { free(host); return false; }
    bool ok = false;
    if (strcmp(host,"api.anthropic.com")==0 ||
        ends_with(host,".anthropic.com") ||
        ends_with(host,".claude.com") ||
        ends_with(host,".azure.com")) ok = true;
    free(host);
    if (ok) return true;
    const char *m = provider_detect_api_mode_for_url(base_url);
    return (m && strcmp(m,"anthropic_messages")==0);
}

/* PoP: _host_derived_api_key @ hermes_cli/runtime_provider.py:_host_derived_api_key */
char *provider_host_derived_api_key(const char *base_url) {
    char *host = provider_base_url_hostname(base_url);
    if (!host || !*host) { free(host); return dup_or_empty(""); }
    /* reject IP (last label starts with digit) / loopback / has ':' */
    if (strchr(host, ':')) { free(host); return dup_or_empty(""); }
    if (strcmp(host,"localhost")==0) { free(host); return dup_or_empty(""); }
    /* split labels */
    char *labels[32]; int nl = 0;
    char *tmp = dup_or_empty(host);
    for (char *tok = strtok(tmp, "."); tok && nl < 32; tok = strtok(NULL, "."))
        if (*tok) labels[nl++] = tok;
    /* last label numeric-leading → IP */
    if (nl > 0 && isdigit((unsigned char)labels[nl-1][0])) {
        free(tmp); free(host); return dup_or_empty("");
    }
    /* strip leading api/www */
    int start = 0;
    while (start < nl && (strcmp(labels[start],"api")==0 || strcmp(labels[start],"www")==0))
        start++;
    if (nl - start < 2) { free(tmp); free(host); return dup_or_empty(""); }
    const char *vendor = labels[nl - 2];  /* registrable label */
    /* sanitize to A-Z0-9_ upper */
    size_t vl = strlen(vendor);
    char *san = malloc(vl + 1); if (!san) { free(tmp); free(host); return NULL; }
    for (size_t i = 0; i < vl; i++) {
        char c = vendor[i];
        san[i] = isalnum((unsigned char)c) ? (char)toupper((unsigned char)c) : '_';
    }
    san[vl] = '\0';
    free(tmp); free(host);
    if (!*san || !isalpha((unsigned char)san[0])) { free(san); return dup_or_empty(""); }
    if (strcmp(san,"OPENAI")==0 || strcmp(san,"OPENROUTER")==0 || strcmp(san,"OLLAMA")==0) {
        free(san); return dup_or_empty("");
    }
    char envname[128];
    snprintf(envname, sizeof(envname), "%s_API_KEY", san);
    free(san);
    const char *v = getenv(envname);
    if (!v) return dup_or_empty("");
    /* strip */
    while (*v && isspace((unsigned char)*v)) v++;
    const char *e = v + strlen(v);
    while (e > v && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - v);
    char *out = malloc(n + 1); if (!out) return NULL;
    memcpy(out, v, n); out[n] = '\0';
    return out;
}

/* ─── auth.py:resolve_provider alias map (deterministic part) ──────────── */
struct alias_pair { const char *from; const char *to; };
/* PoP: resolve_provider @ hermes_cli/auth.py:resolve_provider (_PROVIDER_ALIASES) */
static const struct alias_pair ALIASES[] = {
    {"glm","zai"},{"z-ai","zai"},{"z.ai","zai"},{"zhipu","zai"},
    {"google","gemini"},{"google-gemini","gemini"},{"google-ai-studio","gemini"},
    {"x-ai","xai"},{"x.ai","xai"},{"grok","xai"},
    {"xai-oauth","xai-oauth"},{"x-ai-oauth","xai-oauth"},
    {"grok-oauth","xai-oauth"},{"xai-grok-oauth","xai-oauth"},
    {"kimi","kimi-coding"},{"kimi-for-coding","kimi-coding"},{"moonshot","kimi-coding"},
    {"kimi-cn","kimi-coding-cn"},{"moonshot-cn","kimi-coding-cn"},
    {"step","stepfun"},{"stepfun-coding-plan","stepfun"},
    {"arcee-ai","arcee"},{"arceeai","arcee"},
    {"gmi-cloud","gmi"},{"gmicloud","gmi"},
    {"minimax-china","minimax-cn"},{"minimax_cn","minimax-cn"},
    {"minimax-portal","minimax-oauth"},{"minimax-global","minimax-oauth"},{"minimax_oauth","minimax-oauth"},
    {"alibaba_coding","alibaba-coding-plan"},{"alibaba-coding","alibaba-coding-plan"},
    {"alibaba_coding_plan","alibaba-coding-plan"},
    {"claude","anthropic"},{"claude-code","anthropic"},
    {"github","copilot"},{"github-copilot","copilot"},
    {"github-models","copilot"},{"github-model","copilot"},
    {"github-copilot-acp","copilot-acp"},{"copilot-acp-agent","copilot-acp"},
    {"opencode","opencode-zen"},{"zen","opencode-zen"},
    {"qwen-portal","qwen-oauth"},{"qwen-cli","qwen-oauth"},{"qwen-oauth","qwen-oauth"},
    {"hf","huggingface"},{"hugging-face","huggingface"},{"huggingface-hub","huggingface"},
    {"mimo","xiaomi"},{"xiaomi-mimo","xiaomi"},
    {"tencent","tencent-tokenhub"},{"tokenhub","tencent-tokenhub"},
    {"tencent-cloud","tencent-tokenhub"},{"tencentmaas","tencent-tokenhub"},
    {"aws","bedrock"},{"aws-bedrock","bedrock"},{"amazon-bedrock","bedrock"},{"amazon","bedrock"},
    {"go","opencode-go"},{"opencode-go-sub","opencode-go"},
    {"kilo","kilocode"},{"kilo-code","kilocode"},{"kilo-gateway","kilocode"},
    {"lmstudio","lmstudio"},{"lm-studio","lmstudio"},{"lm_studio","lmstudio"},
    {"ollama","custom"},{"ollama_cloud","ollama-cloud"},
    {"vllm","custom"},{"llamacpp","custom"},
    {"llama.cpp","custom"},{"llama-cpp","custom"},
};
static const int ALIASES_N = (int)(sizeof(ALIASES)/sizeof(ALIASES[0]));

/* PoP: resolve_provider @ hermes_cli/auth.py:resolve_provider */
char *provider_resolve_alias(const char *requested) {
    char *norm = strip_lower((requested && *requested) ? requested : "auto");
    if (!norm) return NULL;
    if (!*norm) { free(norm); return dup_or_empty("auto"); }
    /* apply alias map */
    for (int i = 0; i < ALIASES_N; i++) {
        if (strcmp(norm, ALIASES[i].from) == 0) {
            char *r = dup_or_empty(ALIASES[i].to);
            free(norm);
            return r;
        }
    }
    if (strcmp(norm,"openrouter")==0 || strcmp(norm,"custom")==0) return norm;
    if (provider_registry_get(norm)) return norm;
    /* auto or unknown: return normalized as-is (caller resolves precedence) */
    return norm;
}
