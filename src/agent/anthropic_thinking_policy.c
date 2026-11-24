/*
 * anthropic_thinking_policy.c - C11 port of upstream
 * agent/anthropic_thinking_policy.py (gate w24_anthropict_2a0525).
 *
 * PoP: w24_anthropict_2a0525_claude_family_version @ agent/anthropic_thinking_policy.py:claude_family_version
 * PoP: w24_anthropict_2a0525_model_preserves_prior_thinking @ agent/anthropic_thinking_policy.py:model_preserves_prior_thinking
 * PoP: w24_anthropict_2a0525_thinking_route @ agent/anthropic_thinking_policy.py:anthropic_thinking_route
 * PoP: w24_anthropict_2a0525_route_replays_prior_thinking @ agent/anthropic_thinking_policy.py:route_replays_prior_thinking
 * PoP: w24_anthropict_2a0525_native_preserves_prior_thinking @ agent/anthropic_thinking_policy.py:native_anthropic_preserves_prior_thinking
 * PoP: w24_anthropict_2a0525_constants @ agent/anthropic_thinking_policy.py:<literal tables>
 *
 * What is ported is the whole policy: the Claude family/version parse, the
 * keep-vs-last-turn-only verdict, the five replay routes and the two endpoint
 * predicates they are decided from (bedrock/vertex/azure platforms, kimi,
 * moonshot, deepseek, minimax, nous portal).  The endpoint predicates are pure
 * URL text; the ``NOUS_INFERENCE_BASE_URL`` host override is an operator env
 * seam and is modelled unset, and no request, credential or converter is
 * touched.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_anthropict_2a0525.h"

/* ---- small text helpers --------------------------------------------- */

static char *anp_dup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static char *anp_lower_dup(const char *s) {
    char *p = anp_dup(s);
    size_t i;
    if (!p) return NULL;
    for (i = 0; p[i]; i++) p[i] = (char)tolower((unsigned char)p[i]);
    return p;
}

/* str.strip() (ASCII whitespace) */
static char *anp_strip_dup(const char *s) {
    const char *b = s ? s : "";
    const char *e = b + strlen(b);
    char *out;
    size_t n;
    while (b < e && isspace((unsigned char)*b)) b++;
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* case-insensitive strstr */
static const char *anp_ifind(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    size_t i;
    if (!nl) return hay;
    for (; *hay; hay++) {
        for (i = 0; i < nl; i++)
            if (tolower((unsigned char)hay[i]) != tolower((unsigned char)needle[i])) break;
        if (i == nl) return hay;
    }
    return NULL;
}

static int anp_is_sep(char c) { return c == '-' || c == '_' || c == '.'; }

/* ---- base_url text: _normalize_base_url_text / _normalized_lower ---- */

static char *anp_normalized_lower(const char *raw) {
    char *s = anp_strip_dup(raw);
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    while (n > 0 && s[n - 1] == '/') s[--n] = '\0';
    out = anp_lower_dup(s);
    free(s);
    return out;
}

/* urlparse().hostname: netloc minus userinfo and port, lowercased, no trailing dot. */
static void anp_host_of(const char *raw, char *out, size_t cap) {
    const char *p = raw ? raw : "";
    const char *end, *q, *at = NULL;
    size_t n = 0;
    const char *scheme = strstr(p, "://");
    if (scheme) p = scheme + 3;
    end = p + strcspn(p, "/?#");
    for (q = p; q < end; q++)
        if (*q == '@') at = q;
    if (at) p = at + 1;
    if (p < end && *p == '[') {
        const char *rb = memchr(p, ']', (size_t)(end - p));
        const char *stop = rb ? rb + 1 : end;
        n = (size_t)(stop - p);
    } else {
        for (q = p; q < end && *q != ':'; q++) {}
        n = (size_t)(q - p);
    }
    if (n >= cap) n = cap - 1;
    for (q = p; q < p + n; q++) out[q - p] = (char)tolower((unsigned char)*q);
    out[n] = '\0';
    while (n > 0 && out[n - 1] == '.') out[--n] = '\0';
}

/* urlparse().path, lowercased. */
static void anp_path_of(const char *raw, char *out, size_t cap) {
    const char *p = raw ? raw : "";
    const char *scheme = strstr(p, "://");
    const char *start;
    size_t n;
    if (scheme) p = scheme + 3;
    p += strcspn(p, "/?#");
    start = p;
    n = strcspn(start, "?#");
    if (n >= cap) n = cap - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)start[i]);
    out[n] = '\0';
}

static int anp_host_matches(const char *raw, const char *domain) {
    char host[512], dom[256];
    size_t n;
    anp_host_of(raw, host, sizeof host);
    snprintf(dom, sizeof dom, "%s", domain ? domain : "");
    n = strlen(dom);
    while (n > 0 && isspace((unsigned char)dom[n - 1])) dom[--n] = '\0';
    while (n > 0 && dom[n - 1] == '.') dom[--n] = '\0';
    for (size_t i = 0; i < n; i++) dom[i] = (char)tolower((unsigned char)dom[i]);
    if (!host[0] || !dom[0]) return 0;
    if (strcmp(host, dom) == 0) return 1;
    {
        size_t hl = strlen(host), dl = strlen(dom);
        if (hl > dl && host[hl - dl - 1] == '.' && strcmp(host + hl - dl, dom) == 0) return 1;
    }
    return 0;
}

static int anp_str_ends(const char *s, const char *suffix) {
    size_t sl = strlen(s), tl = strlen(suffix);
    return sl >= tl && strcmp(s + sl - tl, suffix) == 0;
}

/* ---- endpoint predicates (anthropic_endpoints.py) -------------------- */

static int anp_third_party(const char *url) {
    char *norm = anp_normalized_lower(url);
    int r;
    if (!norm) return 0;
    r = norm[0] != '\0' && strstr(norm, "anthropic.com") == NULL;
    free(norm);
    return r;
}

static int anp_minimax(const char *url) {
    static const char *const pfx[] = {
        "https://api.minimax.io/anthropic", "https://api.minimaxi.com/anthropic",
    };
    char *norm = anp_normalized_lower(url);
    int hit = 0;
    if (!norm) return 0;
    for (size_t i = 0; i < sizeof pfx / sizeof pfx[0]; i++)
        if (strncmp(norm, pfx[i], strlen(pfx[i])) == 0) hit = 1;
    free(norm);
    return hit;
}

static int anp_nous_portal(const char *url) {
    return anp_host_matches(url ? url : "", "inference-api.nousresearch.com");
}

static int anp_kimi_coding(const char *url) {
    static const char pfx[] = "https://api.kimi.com/coding";
    char *norm = anp_normalized_lower(url);
    int hit;
    if (!norm) return 0;
    hit = strncmp(norm, pfx, sizeof pfx - 1) == 0;
    free(norm);
    return hit;
}

static int anp_model_kimi_family(const char *model) {
    static const char *const pfx[] = {
        "kimi-", "kimi_", "moonshot-", "moonshot_", "k1.", "k1-",
        "k2.", "k2-", "k25", "k2.5", "k3.", "k3-",
    };
    char *s, *name;
    int hit = 0;
    size_t n;
    if (!model) return 0;
    s = anp_lower_dup(model);
    if (!s) return 0;
    /* strip, then take the part after the last '/' */
    {
        char *t = anp_strip_dup(s);
        free(s);
        if (!t) return 0;
        s = t;
    }
    name = strrchr(s, '/');
    name = name ? name + 1 : s;
    n = strlen(name);
    if (n > 0) {
        if (strcmp(name, "k3") == 0) hit = 1;
        for (size_t i = 0; i < sizeof pfx / sizeof pfx[0]; i++)
            if (strncmp(name, pfx[i], strlen(pfx[i])) == 0) hit = 1;
    }
    free(s);
    return hit;
}

static int anp_model_deepseek_thinking(const char *model) {
    static const char *const pfx[] = {
        "deepseek-r", "deepseek-v4", "deepseek_v4", "deepseek-pro",
        "deepseek_pro", "deepseek-flash", "deepseek_flash",
    };
    char *s, *name;
    int hit = 0;
    if (!model) return 0;
    s = anp_lower_dup(model);
    if (!s) return 0;
    {
        char *t = anp_strip_dup(s);
        free(s);
        if (!t) return 0;
        s = t;
    }
    name = strrchr(s, '/');
    name = name ? name + 1 : s;
    if (name[0])
        for (size_t i = 0; i < sizeof pfx / sizeof pfx[0]; i++)
            if (strncmp(name, pfx[i], strlen(pfx[i])) == 0) hit = 1;
    free(s);
    return hit;
}

static int anp_kimi_family(const char *url, const char *model) {
    return anp_kimi_coding(url)
        || anp_host_matches(url ? url : "", "api.kimi.com")
        || anp_host_matches(url ? url : "", "moonshot.ai")
        || anp_host_matches(url ? url : "", "moonshot.cn")
        || anp_model_kimi_family(model);
}

static int anp_deepseek_endpoint(const char *url) {
    char *norm;
    int hit;
    if (!anp_host_matches(url ? url : "", "api.deepseek.com")) return 0;
    norm = anp_normalized_lower(url);
    if (!norm) return 0;
    hit = strstr(norm, "/anthropic") != NULL;
    free(norm);
    return hit;
}

static int anp_azure_anthropic(const char *url) {
    char host[512], path[1024], padded[600];
    char *text;
    int hit;
    if (!url) return 0;
    text = anp_strip_dup(url);
    if (!text) return 0;
    if (!text[0]) { free(text); return 0; }
    anp_host_of(text, host, sizeof host);
    anp_path_of(text, path, sizeof path);
    snprintf(padded, sizeof padded, ".%s.", host);
    hit = (strstr(padded, ".services.ai.azure.") != NULL
           || strstr(padded, ".openai.azure.") != NULL)
          && strstr(path, "/anthropic") != NULL;
    free(text);
    return hit;
}

static int anp_bedrock_runtime(const char *url) {
    char host[512], buf[512];
    const char *p;
    int label = 0;
    anp_host_of(url ? url : "", host, sizeof host);
    snprintf(buf, sizeof buf, "%s", host);
    for (p = buf; *p; ) {
        const char *dot = strchr(p, '.');
        size_t n = dot ? (size_t)(dot - p) : strlen(p);
        if (n >= 15 && strncmp(p, "bedrock-runtime", 15) == 0) label = 1;
        if (!dot) break;
        p = dot + 1;
    }
    return label && (anp_host_matches(host, "amazonaws.com")
                     || anp_host_matches(host, "amazonaws.com.cn"));
}

static int anp_vertex(const char *url) {
    char host[512];
    anp_host_of(url ? url : "", host, sizeof host);
    return strcmp(host, "aiplatform.googleapis.com") == 0
        || anp_str_ends(host, "-aiplatform.googleapis.com");
}

static int anp_claude_platform(const char *url) {
    return anp_bedrock_runtime(url) || anp_vertex(url) || anp_azure_anthropic(url);
}

static int anp_off_anthropic(const char *url) {
    return anp_third_party(url) && !anp_nous_portal(url);
}

/* ---- Claude id parsing ---------------------------------------------- */

static const char *anp_family_at(const char *p, size_t *len) {
    static const char *const fam[] = {"opus", "sonnet", "haiku", "fable", "mythos"};
    for (size_t i = 0; i < sizeof fam / sizeof fam[0]; i++) {
        size_t l = strlen(fam[i]);
        if (strncasecmp(p, fam[i], l) == 0) { *len = l; return fam[i]; }
    }
    return NULL;
}

/* claude[-_.](family)[-_.](\d+)(?:[-_.](\d{1,2})(?=$|[-_.]))? */
static int anp_parse_version(const char *model, char *fam_out, size_t fam_cap,
                             int *major, int *minor) {
    const char *p = model;
    while ((p = anp_ifind(p, "claude")) != NULL) {
        const char *q = p + 6;
        const char *fam;
        size_t flen = 0;
        int maj = 0, mn = 0;
        if (!anp_is_sep(*q)) { p++; continue; }
        q++;
        fam = anp_family_at(q, &flen);
        if (!fam) { p++; continue; }
        q += flen;
        if (!anp_is_sep(*q)) { p++; continue; }
        q++;
        if (!isdigit((unsigned char)*q)) { p++; continue; }
        while (isdigit((unsigned char)*q)) { maj = maj * 10 + (*q - '0'); q++; }
        if (anp_is_sep(*q)) {
            const char *d = q + 1;
            int n = 0, v2 = 0, v1 = 0;
            while (n < 2 && isdigit((unsigned char)d[n])) {
                if (n == 0) v1 = d[n] - '0';
                else v2 = v1 * 10 + (d[n] - '0');
                n++;
            }
            if (n == 2 && (d[2] == '\0' || anp_is_sep(d[2]))) mn = v2;
            else if (n >= 1 && (d[1] == '\0' || anp_is_sep(d[1]))) mn = v1;
        }
        snprintf(fam_out, fam_cap, "%s", fam);
        *major = maj;
        *minor = mn;
        return 1;
    }
    return 0;
}

static const char *anp_model_text(const json_t *model) {
    if (!model || model->type != JSON_STRING || !model->str_val) return NULL;
    return model->str_val;
}

/* claude[-_.]?3(?!\d) */
static int anp_last_turn_only(const char *model) {
    const char *p = model;
    while ((p = anp_ifind(p, "claude")) != NULL) {
        const char *q = p + 6;
        if (anp_is_sep(*q)) q++;
        if (*q == '3' && !isdigit((unsigned char)q[1])) return 1;
        p++;
    }
    return 0;
}

static int anp_model_preserves(const char *model) {
    static const struct { const char *family; int major, minor; } keep_from[] = {
        {"opus", 4, 5}, {"sonnet", 4, 6}, {"haiku", 5, 5},
    };
    char fam[16];
    int maj = 0, mn = 0, k0 = 5, k1 = 0;
    char *lower;
    int found;
    if (!model) return 0;
    lower = anp_lower_dup(model);
    if (!lower) return 0;
    if (!strstr(lower, "claude")) { free(lower); return 0; }
    free(lower);
    if (anp_last_turn_only(model)) return 0;
    found = anp_parse_version(model, fam, sizeof fam, &maj, &mn);
    if (!found) return 1;                       /* unknown/future Claude id defaults to keep */
    for (size_t i = 0; i < sizeof keep_from / sizeof keep_from[0]; i++)
        if (strcmp(fam, keep_from[i].family) == 0) {
            k0 = keep_from[i].major;
            k1 = keep_from[i].minor;
        }
    return maj > k0 || (maj == k0 && mn >= k1);
}

/* ---- exported port -------------------------------------------------- */

json_t *w24_anthropict_2a0525_claude_family_version(const json_t *model) {
    const char *text = anp_model_text(model);
    char fam[16];
    int maj = 0, mn = 0;
    char *stripped;
    json_t *out, *ver;
    if (!text) return json_null();
    stripped = anp_strip_dup(text);
    if (!stripped) return json_null();
    if (!anp_parse_version(stripped, fam, sizeof fam, &maj, &mn)) {
        free(stripped);
        return json_null();
    }
    free(stripped);
    out = json_array();
    ver = json_array();
    json_append(ver, json_number((double)maj));
    json_append(ver, json_number((double)mn));
    json_append(out, json_string(fam));
    json_append(out, ver);
    return out;
}

json_t *w24_anthropict_2a0525_model_preserves_prior_thinking(const json_t *model) {
    return json_bool(anp_model_preserves(anp_model_text(model)));
}

static const char *anp_route(const char *url, const char *model) {
    const char *u = url ? url : "";
    const char *m = model ? model : "";
    if (anp_kimi_family(u, m)) return "kimi";
    if (anp_deepseek_endpoint(u) || (anp_off_anthropic(u) && anp_model_deepseek_thinking(m)))
        return "deepseek";
    if (!anp_off_anthropic(u) || anp_claude_platform(u)) return "native";
    if (anp_minimax(u)) return "minimax";
    return "third_party";
}

json_t *w24_anthropict_2a0525_thinking_route(const json_t *base_url, const json_t *model) {
    const char *url = (base_url && base_url->type == JSON_STRING) ? base_url->str_val : NULL;
    const char *m = anp_model_text(model);
    return json_string(anp_route(url, m));
}

json_t *w24_anthropict_2a0525_route_replays_prior_thinking(const json_t *route,
                                                           const json_t *model) {
    const char *r = (route && route->type == JSON_STRING) ? route->str_val : "";
    if (r && strcmp(r, "minimax") == 0) return json_bool(1);
    if (r && strcmp(r, "native") == 0)
        return json_bool(anp_model_preserves(anp_model_text(model)));
    return json_bool(0);
}

json_t *w24_anthropict_2a0525_native_preserves_prior_thinking(const json_t *base_url,
                                                              const json_t *model) {
    const char *url = (base_url && base_url->type == JSON_STRING) ? base_url->str_val : NULL;
    const char *m = anp_model_text(model);
    const char *route = anp_route(url, m);
    if (strcmp(route, "minimax") == 0) return json_bool(1);
    if (strcmp(route, "native") == 0) return json_bool(anp_model_preserves(m));
    return json_bool(0);
}

json_t *w24_anthropict_2a0525_constants(void) {
    json_t *o = json_object();
    json_t *routes = json_array();
    json_t *keep = json_object();
    json_t *opus = json_array(), *sonnet = json_array(), *haiku = json_array();
    json_append(routes, json_string("minimax"));
    json_append(routes, json_string("native"));
    json_append(opus, json_number(4.0));
    json_append(opus, json_number(5.0));
    json_append(sonnet, json_number(4.0));
    json_append(sonnet, json_number(6.0));
    json_append(haiku, json_number(5.0));
    json_append(haiku, json_number(5.0));
    json_set(keep, "opus", opus);
    json_set(keep, "sonnet", sonnet);
    json_set(keep, "haiku", haiku);
    json_set(o, "signed_replay_routes", routes);
    json_set(o, "keep_all_from", keep);
    json_set(o, "claude_version_pattern",
             json_string("claude[-_.](opus|sonnet|haiku|fable|mythos)[-_.](\\d+)"
                         "(?:[-_.](\\d{1,2})(?=$|[-_.]))?"));
    json_set(o, "last_turn_only_pattern", json_string("claude[-_.]?3(?!\\d)"));
    return o;
}
