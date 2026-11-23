/*
 * models_fast_regions.c — C port of upstream Hermes hermes_cli/models_fast_regions.py.
 * PoP: fastregion_openai_fast_region_allows @ hermes_cli/models_fast_regions.py:openai_fast_region_allows
 *
 * The three lazy imports used by the Python function are inlined faithfully:
 *   utils.base_url_hostname,
 *   agent.model_metadata.strip_codex_context_variant_suffix (incl. -900k eligibility),
 *   hermes_cli.models._strip_vendor_prefix.
 */
#include "noc_fastregion.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dup_trim(const char *s) {
    size_t n = s ? strlen(s) : 0, a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (b > a) memcpy(out, s + a, b - a);
    out[b - a] = 0;
    return out;
}
static void lower_ascii(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}
static int authority_end(char c) {
    return c == '/' || c == '?' || c == '#';
}
static int in_set(const char *s, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

/* utils.base_url_hostname: lowercased hostname, trailing dots removed. */
static char *base_url_hostname(const char *base_url) {
    char *raw = dup_trim(base_url ? base_url : "");
    if (!*raw) { free(raw); return strdup(""); }
    const char *sep = strstr(raw, "://");
    const char *auth = sep ? sep + 3 : raw;
    const char *end = auth;
    while (*end && !authority_end(*end)) end++;
    size_t len = (size_t)(end - auth);
    char *host = (char *)malloc(len + 1);
    if (len) memcpy(host, auth, len);
    host[len] = 0;
    /* urlparse.hostname strips userinfo (everything through the last '@'). */
    char *at = strrchr(host, '@');
    if (at) memmove(host, at + 1, strlen(at + 1) + 1);
    char *h = host;
    if (*h == '[') {
        char *rb = strchr(h, ']');
        if (rb) { *(rb + 1) = 0; h++; }
    } else {
        char *colon = strchr(h, ':'); /* port */
        if (colon) *colon = 0;
    }
    lower_ascii(h);
    size_t hl = strlen(h);
    while (hl && h[hl - 1] == '.') h[--hl] = 0;
    char *res = strdup(h);
    free(host);
    free(raw);
    return res;
}

/* --- codex -900k context-variant eligibility (agent/model_metadata.py) --- */
static const char *const SNAPSHOT_BASES[] = {
    "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol", "gpt-6-luna"
};
static const char *const ELIGIBLE_BASES[] = {
    "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol", "gpt-6-luna",
    "gpt-5.4", "gpt-daybreak-blue-latest", "gpt-6-astra", "gpt-6.1-sol"
};
#define SUFFIX "-900k"
#define SUFFIX_LEN 5

static int date_suffix_ok(const char *s) { /* ^\d{4}-\d{2}-\d{2}$ */
    if (strlen(s) != 10) return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (s[i] != '-') return 0;
        } else if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}
static char *bare_codex_slug(const char *model) { /* strip, lower, after last '/' */
    char *t = dup_trim(model ? model : "");
    lower_ascii(t);
    char *slash = strrchr(t, '/');
    char *res = strdup(slash ? slash + 1 : t);
    free(t);
    return res;
}
static int codex_900k_eligible(const char *slug) {
    if (!*slug) return 0;
    size_t n = strlen(slug);
    if (n >= SUFFIX_LEN && !strcmp(slug + n - SUFFIX_LEN, SUFFIX)) return 0;
    if (in_set(slug, ELIGIBLE_BASES,
               sizeof(ELIGIBLE_BASES) / sizeof(ELIGIBLE_BASES[0]))) return 1;
    for (size_t i = 0; i < sizeof(SNAPSHOT_BASES) / sizeof(SNAPSHOT_BASES[0]); i++) {
        const char *base = SNAPSHOT_BASES[i];
        size_t bl = strlen(base);
        if (!strncmp(slug, base, bl) && slug[bl] == '-' && date_suffix_ok(slug + bl + 1))
            return 1;
    }
    return 0;
}
static char *strip_codex_context_variant_suffix(const char *model) {
    char *raw = dup_trim(model ? model : ""); /* case preserved, like Python */
    char *slug = bare_codex_slug(raw);
    size_t sl = strlen(slug);
    if (sl >= SUFFIX_LEN && !strcmp(slug + sl - SUFFIX_LEN, SUFFIX)) {
        char *base = (char *)malloc(sl - SUFFIX_LEN + 1);
        memcpy(base, slug, sl - SUFFIX_LEN);
        base[sl - SUFFIX_LEN] = 0;
        int ok = codex_900k_eligible(base);
        free(base);
        if (ok) {
            size_t rl = strlen(raw);
            raw[rl - SUFFIX_LEN] = 0; /* ASCII suffix => same byte length */
            free(slug);
            return raw;
        }
    }
    free(slug);
    return raw;
}

/* hermes_cli.models._strip_vendor_prefix: lower, strip first "vendor/" part. */
static char *strip_vendor_prefix(const char *s) {
    char *raw = dup_trim(s ? s : "");
    lower_ascii(raw);
    char *slash = strchr(raw, '/');
    if (slash) memmove(raw, slash + 1, strlen(slash + 1) + 1);
    return raw;
}

static const char *const EU_MODELS[] = {
    "gpt-6.2-astra", "gpt-6.2-sol", "gpt-6.1-sol", "gpt-6.1-terra", "gpt-6-luna"
};

int fastregion_openai_fast_region_allows(const char *base_url,
                                         const char *model_id,
                                         const char *tier)
{
    char *host = base_url_hostname(base_url ? base_url : "");
    int result = 0;
    if (!strcmp(host, "us.api.openai.com")) {
        /* _FAST_REGION_MODELS[us] is None: every model accepted. */
        result = 1;
    } else if (!strcmp(host, "eu.api.openai.com")) {
        char *stripped = strip_codex_context_variant_suffix(model_id ? model_id : "");
        char *base = strip_vendor_prefix(stripped);
        char *colon = strchr(base, ':'); /* .split(":")[0] */
        if (colon) *colon = 0;
        int ultrafast = tier && !strcmp(tier, "ultrafast");
        int gpt62 = !strcmp(base, "gpt-6.2-astra") || !strcmp(base, "gpt-6.2-sol");
        if (!(ultrafast && gpt62))
            result = in_set(base, EU_MODELS, sizeof(EU_MODELS) / sizeof(EU_MODELS[0]));
        free(base);
        free(stripped);
    }
    free(host);
    return result;
}
