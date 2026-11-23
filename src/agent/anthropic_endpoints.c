/*
 * anthropic_endpoints.c - C home for upstream Hermes agent/anthropic_endpoints.py.
 * PoP: w11_anthropice_3d1193_normalize_base_url_text @ agent/anthropic_endpoints.py:_normalize_base_url_text
 * PoP: w11_anthropice_3d1193_normalized_lower @ agent/anthropic_endpoints.py:_normalized_lower
 * PoP: w11_anthropice_3d1193_is_third_party_anthropic_endpoint @ agent/anthropic_endpoints.py:_is_third_party_anthropic_endpoint
 * PoP: w11_anthropice_3d1193_is_kimi_coding_endpoint @ agent/anthropic_endpoints.py:_is_kimi_coding_endpoint
 * PoP: w11_anthropice_3d1193_is_opencode_endpoint @ agent/anthropic_endpoints.py:_is_opencode_endpoint
 * PoP: w11_anthropice_3d1193_model_name_is_kimi_family @ agent/anthropic_endpoints.py:_model_name_is_kimi_family
 * PoP: w11_anthropice_3d1193_is_kimi_family_endpoint @ agent/anthropic_endpoints.py:_is_kimi_family_endpoint
 * PoP: w11_anthropice_3d1193_model_name_is_deepseek_thinking @ agent/anthropic_endpoints.py:_model_name_is_deepseek_thinking
 * PoP: w11_anthropice_3d1193_is_deepseek_anthropic_endpoint @ agent/anthropic_endpoints.py:_is_deepseek_anthropic_endpoint
 * PoP: w11_anthropice_3d1193_is_minimax_anthropic_endpoint @ agent/anthropic_endpoints.py:_is_minimax_anthropic_endpoint
 * PoP: w11_anthropice_3d1193_is_azure_anthropic_endpoint @ agent/anthropic_endpoints.py:_is_azure_anthropic_endpoint
 * PoP: w11_anthropice_3d1193_is_bedrock_runtime_endpoint @ agent/anthropic_endpoints.py:_is_bedrock_runtime_endpoint
 * PoP: w11_anthropice_3d1193_is_vertex_ai_endpoint @ agent/anthropic_endpoints.py:_is_vertex_ai_endpoint
 * PoP: w11_anthropice_3d1193_is_claude_platform_endpoint @ agent/anthropic_endpoints.py:_is_claude_platform_endpoint
 * PoP: w11_anthropice_3d1193_base_url_needs_context_1m_beta @ agent/anthropic_endpoints.py:_base_url_needs_context_1m_beta
 * PoP: w11_anthropice_3d1193_requires_bearer_auth @ agent/anthropic_endpoints.py:_requires_bearer_auth
 *
 * Module-level literals mirrored from the Python source (provenance):
 *   _MINIMAX_ANTHROPIC_PREFIXES = ("https://api.minimax.io/anthropic", "https://api.minimaxi.com/anthropic")
 *   _KIMI_FAMILY_MODEL_PREFIXES  = (kimi-, kimi_, moonshot-, moonshot_, k1., k1-, k2., k2-, k25, k2.5, k3., k3-)
 *   _KIMI_FAMILY_EXACT_SLUGS     = {"k3"}
 *   _DEEPSEEK_THINKING_MODEL_PREFIXES = (deepseek-r, deepseek-v4, deepseek_v4, deepseek-pro,
 *                                        deepseek_pro, deepseek-flash, deepseek_flash)
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_anthropice_3d1193.h"

static const char *const W11AE_MINIMAX_PREFIXES[] = {
    "https://api.minimax.io/anthropic", "https://api.minimaxi.com/anthropic"
};
static const char *const W11AE_KIMI_PREFIXES[] = {
    "kimi-", "kimi_", "moonshot-", "moonshot_", "k1.", "k1-",
    "k2.", "k2-", "k25", "k2.5", "k3.", "k3-"
};
static const char *const W11AE_DEEPSEEK_PREFIXES[] = {
    "deepseek-r", "deepseek-v4", "deepseek_v4", "deepseek-pro",
    "deepseek_pro", "deepseek-flash", "deepseek_flash"
};

static int w11ae_ws(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static void w11ae_lower(char *s) { for (; s && *s; s++) if (*s>='A' && *s<='Z') *s=(char)(*s+32); }
static char *w11ae_strip(const char *s) {
    if (!s) return (char *)calloc(1,1);
    size_t n=strlen(s), a=0, b=n;
    while (a<b && w11ae_ws((unsigned char)s[a])) a++;
    while (b>a && w11ae_ws((unsigned char)s[b-1])) b--;
    char *o=(char *)malloc(b-a+1); memcpy(o,s+a,b-a); o[b-a]=0; return o;
}
/* str(base_url).strip() if base_url else "" -- inputs are JSON strings or NULL. */
static char *w11ae_normalize(const char *b) {
    if (!b || !*b) return (char *)calloc(1,1);
    return w11ae_strip(b);
}
char *w11_anthropice_3d1193_normalize_base_url_text(const char *b) { return w11ae_normalize(b); }
char *w11_anthropice_3d1193_normalized_lower(const char *b) {
    char *t=w11ae_normalize(b); size_t n=strlen(t);
    while (n && t[n-1]=='/') t[--n]=0;
    w11ae_lower(t); return t;
}
static int w11ae_startswith_any(const char *s, const char *const *arr, size_t n) {
    if (!s) return 0;
    for (size_t i=0;i<n;i++) if (strncmp(s,arr[i],strlen(arr[i]))==0) return 1;
    return 0;
}
static int w11ae_contains(const char *h, const char *n) { return h && n && strstr(h,n)!=NULL; }
static int w11ae_endswith(const char *s, const char *suf) {
    if (!s||!suf) return 0; size_t a=strlen(s), b=strlen(suf);
    return a>=b && strcmp(s+a-b,suf)==0;
}
/* Mirrors utils/urls.py: _parse_base_url + _hostname_of + base_url_hostname.
 * Tolerates a bare host[:port][/path] (no scheme) via the implicit "//" prefix. */
static void w11ae_parse(const char *in, char **host_out, char **path_out) {
    char *raw=w11ae_strip(in);
    char *h=(char *)calloc(1,1), *p=(char *)calloc(1,1);
    if (raw && *raw) {
        const char *s=raw, *scheme=strstr(raw,"://");
        if (scheme) s=scheme+3;
        const char *e=s; while (*e && *e!='/' && *e!='?' && *e!='#') e++;
        size_t alen=(size_t)(e-s);
        char *auth=(char *)malloc(alen+1); memcpy(auth,s,alen); auth[alen]=0;
        const char *hp=auth, *at=strrchr(auth,'@'); if (at) hp=at+1;
        const char *hend=hp+strlen(hp);
        if (*hp=='[') { const char *rb=strchr(hp,']'); if (rb) hend=rb+1; }
        else { const char *col=strchr(hp,':'); if (col) hend=col; }
        size_t hlen=(size_t)(hend-hp);
        free(h); h=(char *)malloc(hlen+1); memcpy(h,hp,hlen); h[hlen]=0;
        w11ae_lower(h); while (hlen && h[hlen-1]=='.') h[--hlen]=0;
        const char *pe=e; while (*pe && *pe!='?' && *pe!='#') pe++;
        size_t plen=(size_t)(pe-e);
        free(p); p=(char *)malloc(plen+1); memcpy(p,e,plen); p[plen]=0;
        free(auth);
    }
    free(raw); *host_out=h; *path_out=p;
}
static char *w11ae_hostname(const char *b) { char *h,*p; w11ae_parse(b,&h,&p); free(p); return h; }
static int w11ae_host_matches(const char *b, const char *domain) {
    char *h=w11ae_hostname(b);
    char *d=w11ae_strip(domain); w11ae_lower(d);
    size_t dl=strlen(d); while (dl && d[dl-1]=='.') d[--dl]=0;
    int r = h[0] && d[0] && (strcmp(h,d)==0 ||
            (strlen(h)>strlen(d) && h[strlen(h)-strlen(d)-1]=='.' &&
             strcmp(h+strlen(h)-strlen(d),d)==0));
    free(h); free(d); return r;
}
static char *w11ae_vendor_leaf(const char *model) {
    char *m=w11ae_strip(model); w11ae_lower(m);
    const char *slash=strrchr(m,'/');
    char *leaf=strdup(slash?slash+1:m);
    free(m); return leaf;
}
int w11_anthropice_3d1193_is_third_party_anthropic_endpoint(const char *b) {
    char *n=w11_anthropice_3d1193_normalized_lower(b);
    int r = n[0] && !w11ae_contains(n,"anthropic.com"); free(n); return r;
}
int w11_anthropice_3d1193_is_kimi_coding_endpoint(const char *b) {
    char *n=w11_anthropice_3d1193_normalized_lower(b);
    int r = strncmp(n,"https://api.kimi.com/coding",strlen("https://api.kimi.com/coding"))==0;
    free(n); return r;
}
int w11_anthropice_3d1193_is_opencode_endpoint(const char *b) { return w11ae_host_matches(b?b:"","opencode.ai"); }
int w11_anthropice_3d1193_model_name_is_kimi_family(const char *model) {
    if (!model) return 0;
    char *leaf=w11ae_vendor_leaf(model);
    int r = leaf[0] && (strcmp(leaf,"k3")==0 ||
            w11ae_startswith_any(leaf,W11AE_KIMI_PREFIXES,sizeof(W11AE_KIMI_PREFIXES)/sizeof(W11AE_KIMI_PREFIXES[0])));
    free(leaf); return r;
}
int w11_anthropice_3d1193_is_kimi_family_endpoint(const char *b, const char *model) {
    if (w11_anthropice_3d1193_is_kimi_coding_endpoint(b)) return 1;
    const char *doms[]={"api.kimi.com","moonshot.ai","moonshot.cn"};
    for (int i=0;i<3;i++) if (w11ae_host_matches(b?b:"",doms[i])) return 1;
    return w11_anthropice_3d1193_model_name_is_kimi_family(model);
}
int w11_anthropice_3d1193_model_name_is_deepseek_thinking(const char *model) {
    if (!model) return 0;
    char *leaf=w11ae_vendor_leaf(model);
    int r = leaf[0] && w11ae_startswith_any(leaf,W11AE_DEEPSEEK_PREFIXES,
              sizeof(W11AE_DEEPSEEK_PREFIXES)/sizeof(W11AE_DEEPSEEK_PREFIXES[0]));
    free(leaf); return r;
}
int w11_anthropice_3d1193_is_deepseek_anthropic_endpoint(const char *b) {
    int hm=w11ae_host_matches(b?b:"","api.deepseek.com");
    char *n=w11_anthropice_3d1193_normalized_lower(b);
    int r=hm && w11ae_contains(n,"/anthropic"); free(n); return r;
}
int w11_anthropice_3d1193_is_minimax_anthropic_endpoint(const char *b) {
    char *n=w11_anthropice_3d1193_normalized_lower(b);
    int r=w11ae_startswith_any(n,W11AE_MINIMAX_PREFIXES,
            sizeof(W11AE_MINIMAX_PREFIXES)/sizeof(W11AE_MINIMAX_PREFIXES[0]));
    free(n); return r;
}
int w11_anthropice_3d1193_is_azure_anthropic_endpoint(const char *b) {
    char *norm=w11ae_normalize(b);
    if (!norm[0]) { free(norm); return 0; }
    char *host,*path; w11ae_parse(norm,&host,&path);
    size_t hp=strlen(host)+3;
    char *padded=(char *)malloc(hp+1);
    snprintf(padded,hp+1,".%s.",host);
    int azure = strstr(padded,".services.ai.azure.")!=NULL || strstr(padded,".openai.azure.")!=NULL;
    w11ae_lower(path);
    int r=azure && w11ae_contains(path,"/anthropic");
    free(padded); free(host); free(path); free(norm); return r;
}
int w11_anthropice_3d1193_is_bedrock_runtime_endpoint(const char *b) {
    char *host=w11ae_hostname(b?b:"");
    int labelled=0;
    char *copy=strdup(host), *tok=strtok(copy,".");
    while (tok) { if (strncmp(tok,"bedrock-runtime",15)==0) { labelled=1; break; } tok=strtok(NULL,"."); }
    int r=labelled && (w11ae_host_matches(host,"amazonaws.com") || w11ae_host_matches(host,"amazonaws.com.cn"));
    free(copy); free(host); return r;
}
int w11_anthropice_3d1193_is_vertex_ai_endpoint(const char *b) {
    char *host=w11ae_hostname(b?b:"");
    int r = strcmp(host,"aiplatform.googleapis.com")==0 || w11ae_endswith(host,"-aiplatform.googleapis.com");
    free(host); return r;
}
int w11_anthropice_3d1193_is_claude_platform_endpoint(const char *b) {
    return w11_anthropice_3d1193_is_bedrock_runtime_endpoint(b)
        || w11_anthropice_3d1193_is_vertex_ai_endpoint(b)
        || w11_anthropice_3d1193_is_azure_anthropic_endpoint(b);
}
int w11_anthropice_3d1193_base_url_needs_context_1m_beta(const char *b) {
    char *t=w11ae_normalize(b); w11ae_lower(t);
    int r=w11ae_contains(t,"azure.com"); free(t); return r;
}
/* nous_override is an injected seam for _is_nous_portal_endpoint (env driven). */
int w11_anthropice_3d1193_requires_bearer_auth(const char *b, int nous_override) {
    char *n=w11_anthropice_3d1193_normalized_lower(b);
    int r = nous_override
         || w11ae_startswith_any(n,W11AE_MINIMAX_PREFIXES,
                sizeof(W11AE_MINIMAX_PREFIXES)/sizeof(W11AE_MINIMAX_PREFIXES[0]))
         || w11ae_contains(n,"azure.com")
         || w11ae_host_matches(n,"palantirfoundry.com")
         || w11ae_host_matches(n,"api.commandcode.ai");
    free(n); return r;
}
