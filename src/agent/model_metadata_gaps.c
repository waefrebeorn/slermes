#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/model_metadata.py + agent/usage_anchor.py revival (wave 2) ──
 *
 * Faithful C11 ports of three live leaves, plus honest tag-strips for four
 * functions already claimed by real ports in src/agent/port_model_metadata.c
 * (mm_extract_flat_context_length / mm_context_length_from_model_payload /
 * mm_model_name_suggests_minimax / mm_model_name_suggests_stale_32k_underreport).
 * Those four stub bodies are left in place (no external callers) but their PoP
 * tags are removed so they no longer earn placeholder credit.
 *
 * Self-contained: the catalog matcher and Codex variant helpers below are
 * untagged internal helpers (the live _catalog_key_matches / _codex_variant_base
 * are already PoP-claimed elsewhere; these are demoted copies, no claim).
 *
 * anchored_context_tokens seams its two collaborators (_anchor_matches and the
 * in-function import estimate_messages_tokens_rough) through a universal seam:
 * the differential oracle scripts both sides identically. With no seam installed
 * the default is fail-closed (anchor treated as stale -> None).
 */

/* ================================================================
 *  Universal seam (anchored_context_tokens collaborators)
 * ================================================================ */
typedef json_t *(*mmg_seam_fn)(void *ctx, const char *name, const json_t *args);
static mmg_seam_fn g_mmg_seam = NULL;
static void *g_mmg_seam_ctx = NULL;

void model_metadata_gaps_set_seam(mmg_seam_fn fn, void *ctx)
{
    g_mmg_seam = fn;
    g_mmg_seam_ctx = ctx;
}

/* Returns a NEW json_t (caller frees); NULL == None. */
static json_t *mmg_call(const char *name, const json_t *args)
{
    if (g_mmg_seam)
        return g_mmg_seam(g_mmg_seam_ctx, name, args);
    if (strcmp(name, "anchor_matches") == 0)
        return json_bool(false);
    if (strcmp(name, "estimate_messages_tokens_rough") == 0)
        return json_number(0);
    return NULL;
}

/* JSON truthiness (Python bool()): falsy = NULL/None/false/0/""/empty container. */
static int mmg_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:  return v->c.count ? 1 : 0;
    case JSON_OBJECT: return v->c.count ? 1 : 0;
    default:          return 1;
    }
}

/* int(value) truncating toward zero; returns 0 on non-number (projection). */
static long long mmg_int_of(const json_t *v)
{
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return 0;
        if (d >= 9223372036854775808.0 || d <= -9223372036854775808.0) return 0;
        return (long long)d;
    }
    return 0;
}

/* ================================================================
 *  Untagged internal helpers (live logic, no PoP claim)
 * ================================================================ */
static char *gaps_lower_dup(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++)
        o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}

/* _normalize_model_version: dots -> dashes. */
static void gaps_normalize_version(const char *s, char *out, size_t cap)
{
    size_t k = 0;
    for (const char *p = s; *p && k < cap - 1; p++)
        out[k++] = (*p == '.') ? '-' : *p;
    out[k] = '\0';
}

static int gaps_contains(const char *hay, const char *needle)
{
    return needle[0] && strstr(hay, needle) != NULL;
}

static const char *gaps_last_segment(const char *s)
{
    const char *slash = strrchr(s, '/');
    return slash ? slash + 1 : s;
}

/* Fold _HYPHENATED_GENERATION_RE: ((solar-)[a-z]+)-(\d{1,2})(?=[-:.@]|$) -> \1\2
 * (drop the dash between the lineup letters and the generation digits). */
static void gaps_fold_solar(const char *s, char *out, size_t cap)
{
    size_t n = strlen(s), k = 0, i = 0;
    while (i < n && k < cap - 1) {
        if (strncmp(s + i, "solar-", 6) == 0) {
            size_t j = i + 6;
            size_t letters = 0;
            while (j < n && s[j] >= 'a' && s[j] <= 'z') { j++; letters++; }
            if (letters >= 1 && j < n && s[j] == '-') {
                size_t q = j + 1, d = 0;
                while (q < n && s[q] >= '0' && s[q] <= '9' && d < 2) { q++; d++; }
                if (d >= 1 && (q >= n || strchr("-:.@", s[q]))) {
                    /* emit "solar-" + letters + digits, skipping the dash at j */
                    size_t run = (q - i); /* whole match span */
                    if (k + run < cap) {
                        for (size_t t = i; t < q; t++)
                            if (t != j) out[k++] = s[t];
                        i = q;
                        continue;
                    }
                }
            }
        }
        out[k++] = s[i++];
    }
    out[k] = '\0';
}

/* _catalog_key_matches (live agent/model_metadata.py:555). */
static int gaps_catalog_key_matches(const char *key, const char *model_lower)
{
    if (!key || !model_lower) return 0;
    if (strncmp(key, "solar-", 6) == 0) {
        char folded[1024];
        gaps_fold_solar(model_lower, folded, sizeof(folded));
        size_t kl = strlen(key);
        if (kl > 0 && key[kl - 1] == '-') {
            const char *seg = gaps_last_segment(folded);
            return strncmp(seg, key, kl) == 0 &&
                   seg[kl] >= 'a' && seg[kl] <= 'z';
        }
        const char *p = folded;
        while ((p = strstr(p, key)) != NULL) {
            char nx = p[kl];
            if (nx == '\0' || strchr("-:.@", nx)) return 1;
            p += kl;
        }
        return 0;
    }
    if (gaps_contains(model_lower, key)) return 1;
    char nk[512], nm[1024];
    gaps_normalize_version(key, nk, sizeof(nk));
    gaps_normalize_version(model_lower, nm, sizeof(nm));
    return gaps_contains(nm, nk);
}

/* _bare_codex_slug: (model or "").strip().lower().rsplit("/",1)[-1]. */
static char *gaps_bare_codex_slug(const char *model)
{
    if (!model) model = "";
    /* strip() both ends (ASCII whitespace) */
    const char *a = model;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = model + strlen(model);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    for (size_t i = 0; i < n; i++) tmp[i] = (char)tolower((unsigned char)a[i]);
    tmp[n] = '\0';
    char *out = strdup(gaps_last_segment(tmp));
    free(tmp);
    return out;
}

static int gaps_endswith(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

/* is_codex_900k_base (live agent/model_metadata.py:1700). */
static int gaps_codex_900k_eligible(const char *slug)
{
    static const char *const eligible[] = {
        "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol", "gpt-6-luna",
        "gpt-5.4", "gpt-daybreak-blue-latest", "gpt-6-astra", "gpt-6.1-sol", NULL
    };
    static const char *const snapshot_bases[] = {
        "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol", "gpt-6-luna", NULL
    };
    if (!slug || !*slug || gaps_endswith(slug, "-900k")) return 0;
    for (int i = 0; eligible[i]; i++)
        if (strcmp(slug, eligible[i]) == 0) return 1;
    for (int i = 0; snapshot_bases[i]; i++) {
        size_t bl = strlen(snapshot_bases[i]);
        if (strncmp(slug, snapshot_bases[i], bl) == 0 && slug[bl] == '-') {
            const char *r = slug + bl + 1;
            if (strlen(r) == 10 && r[4] == '-' && r[7] == '-') {
                int ok = 1;
                for (int t = 0; t < 10; t++)
                    if (t != 4 && t != 7 && !isdigit((unsigned char)r[t])) { ok = 0; break; }
                if (ok) return 1;
            }
        }
    }
    return 0;
}

/* _codex_variant_base: malloc'd eligible base of a valid -900k variant, else NULL. */
static char *gaps_codex_variant_base(const char *model)
{
    char *slug = gaps_bare_codex_slug(model);
    if (!slug) return NULL;
    char *out = NULL;
    if (gaps_endswith(slug, "-900k")) {
        slug[strlen(slug) - 5] = '\0';
        if (gaps_codex_900k_eligible(slug)) out = strdup(slug);
    }
    free(slug);
    return out;
}

/* ================================================================
 *  Embedded live constants
 * ================================================================ */
#define GAPS_DEFAULT_FALLBACK_CONTEXT 256000LL

static const struct { const char *key; long long value; } GAPS_DCL[] = {
    { "claude-fable-5", 1000000 },
    { "claude-fable", 1000000 },
    { "claude-mythos", 1000000 },
    { "claude-opus-5", 1000000 },
    { "claude-sonnet-5", 1000000 },
    { "claude-opus-4-8", 1000000 },
    { "claude-opus-4.8", 1000000 },
    { "claude-opus-4-7", 1000000 },
    { "claude-opus-4.7", 1000000 },
    { "claude-opus-4-6", 1000000 },
    { "claude-sonnet-4-6", 1000000 },
    { "claude-opus-4.6", 1000000 },
    { "claude-sonnet-4.6", 1000000 },
    { "claude", 200000 },
    { "gpt-6.2-astra", 1050000 },
    { "gpt-6.2-sol", 1050000 },
    { "gpt-6-astra", 1050000 },
    { "gpt-6.1-sol", 1050000 },
    { "gpt-6.1-terra", 1050000 },
    { "gpt-6-sol", 1050000 },
    { "gpt-6-luna", 1050000 },
    { "gpt-5.6-luna", 1050000 },
    { "gpt-5.6-terra", 1050000 },
    { "gpt-5.6-sol", 1050000 },
    { "gpt-5.5", 1050000 },
    { "gpt-5.4-nano", 400000 },
    { "gpt-5.4-mini", 400000 },
    { "gpt-5.4", 1050000 },
    { "gpt-5.3-codex-spark", 128000 },
    { "gpt-5.1-chat", 128000 },
    { "gpt-5", 400000 },
    { "gpt-4.1", 1047576 },
    { "gpt-4", 128000 },
    { "gemini", 1048576 },
    { "gemma-4", 256000 },
    { "gemma4", 256000 },
    { "gemma-4-31b", 256000 },
    { "gemma-3", 131072 },
    { "gemma", 8192 },
    { "deepseek-v4-pro", 1000000 },
    { "deepseek-v4.1-flash", 1000000 },
    { "deepseek-v4-flash", 1000000 },
    { "deepseek-chat", 1000000 },
    { "deepseek-reasoner", 1000000 },
    { "deepseek-flash", 1000000 },
    { "deepseek", 128000 },
    { "llama", 131072 },
    { "muse-spark-1.3", 1048576 },
    { "muse-spark", 1048576 },
    { "inkling", 1048576 },
    { "qwen3.8-max", 1000000 },
    { "qwen3.8-flash", 1000000 },
    { "qwen3.6-plus", 1048576 },
    { "qwen3.7-plus", 1048576 },
    { "qwen3-coder-plus", 1000000 },
    { "qwen3-coder", 262144 },
    { "qwen3-max", 262144 },
    { "qwen", 131072 },
    { "minimax-m3", 1000000 },
    { "minimax", 204800 },
    { "glm-5.3", 1310720 },
    { "glm-5.3-flash", 1310720 },
    { "glm-5.3:batch", 1048576 },
    { "glm-5.3:us", 1048576 },
    { "glm-5.3-flash:batch", 1048576 },
    { "glm-5.3-flash:us", 1048576 },
    { "glm-5.3-flashx", 1048576 },
    { "glm-5.2", 1048576 },
    { "glm-5.2:free", 256000 },
    { "glm-5.1", 204800 },
    { "glm-5-turbo", 202752 },
    { "glm-5v-turbo", 202752 },
    { "glm-5", 204800 },
    { "glm-4.7-flash", 202752 },
    { "glm-4.7", 204800 },
    { "glm-4.6v", 131072 },
    { "glm-4.6", 204800 },
    { "glm", 202752 },
    { "grok-composer", 200000 },
    { "grok-build-latest", 500000 },
    { "grok-build", 256000 },
    { "grok-code-fast", 256000 },
    { "grok-2-vision", 8192 },
    { "grok-4-fast", 2000000 },
    { "grok-4.20", 2000000 },
    { "grok-4.6", 500000 },
    { "grok-4.5", 500000 },
    { "grok-4.3", 1000000 },
    { "grok-4", 256000 },
    { "grok-3", 131072 },
    { "grok-2", 131072 },
    { "grok", 131072 },
    { "kimi-k3", 1048576 },
    { "kimi", 262144 },
    { "solar-open2", 262144 },
    { "solar-pro3", 131072 },
    { "solar-pro2", 65536 },
    { "solar-mini", 32768 },
    { "solar-", 524288 },
    { "hy4-preview", 1048576 },
    { "hy3-preview", 262144 },
    { "hy3", 262144 },
    { "x-preview-f", 1048576 },
    { "ox-alpha", 1048576 },
    { "union-alpha", 262144 },
    { "nemotron-3.5-lightning", 1000000 },
    { "nemotron-3.5-super-vl", 1000000 },
    { "nemotron", 131072 },
    { "laguna-s-2.1", 262144 },
    { "laguna-xs-2.1", 262144 },
    { "trinity", 262144 },
    { "elephant", 262144 },
    { "Qwen/Qwen3.5-397B-A17B", 131072 },
    { "Qwen/Qwen3.5-35B-A3B", 131072 },
    { "deepseek-ai/DeepSeek-V3.2", 65536 },
    { "moonshotai/Kimi-K2.5", 262144 },
    { "moonshotai/Kimi-K2.6", 262144 },
    { "moonshotai/Kimi-K2-Thinking", 262144 },
    { "MiniMaxAI/MiniMax-M2.5", 204800 },
    { "XiaomiMiMo/MiMo-V2-Flash", 262144 },
    { "mimo-v2.6-pro-ultraspeed", 1048576 },
    { "mimo-v2.6-pro", 1048576 },
    { "mimo-v2.6-flash", 1048576 },
    { "mimo-v2-pro", 1048576 },
    { "mimo-v2.5-pro", 1048576 },
    { "mimo-v2.5", 1048576 },
    { "mimo-v2-omni", 262144 },
    { "mimo-v2-flash", 262144 },
    { "zai-org/GLM-5", 202752 },
};
static const size_t GAPS_DCL_N = sizeof(GAPS_DCL) / sizeof(GAPS_DCL[0]);

static const char *const GAPS_PRE_CATALOG_STALE[] = {
    "minimax-m3", "muse-spark-1.3", "muse-spark", "grok-4.3", "grok-4.6",
    "grok-4-fast", "grok-4.20", "qwen3.6-plus", "deepseek-flash",
    "deepseek-v4.1-flash", "deepseek-v4-flash", "deepseek-v4-pro",
    "deepseek-chat", "deepseek-reasoner", NULL
};

static const struct { const char *key; long long ctx; } GAPS_CODEX_PREFIXES[] = {
    { "gpt-5.6", 900000 }, { "gpt-6-sol", 900000 }, { "gpt-6-luna", 900000 }
};
static const struct { const char *key; long long ctx; } GAPS_CODEX_EXACT[] = {
    { "gpt-5.4", 900000 }, { "gpt-daybreak-blue-latest", 900000 },
    { "gpt-6-astra", 900000 }, { "gpt-6.1-sol", 900000 }
};

static int gaps_in_stale_keys(const char *key)
{
    for (int i = 0; GAPS_PRE_CATALOG_STALE[i]; i++)
        if (strcmp(key, GAPS_PRE_CATALOG_STALE[i]) == 0) return 1;
    return 0;
}

/* ================================================================
 *  Ports
 * ================================================================ */

/* PoP: stale_pre_catalog_cache_entry @ agent/model_metadata.py:_stale_pre_catalog_cache_entry */
int stale_pre_catalog_cache_entry(const char *model, long long cached)
{
    char *low = gaps_lower_dup(model);
    if (!low) return 0;
    /* matches in table (insertion) order */
    size_t n = GAPS_DCL_N;
    size_t *idx = malloc(n * sizeof(size_t));
    if (!idx) { free(low); return 0; }
    size_t m = 0;
    for (size_t i = 0; i < n; i++)
        if (gaps_catalog_key_matches(GAPS_DCL[i].key, low))
            idx[m++] = i;
    if (m == 0) { free(idx); free(low); return 0; }
    /* max(matches, key=len(key)): first longest in iteration order */
    size_t spec = idx[0];
    size_t spec_len = strlen(GAPS_DCL[spec].key);
    for (size_t i = 1; i < m; i++) {
        size_t l = strlen(GAPS_DCL[idx[i]].key);
        if (l > spec_len) { spec = idx[i]; spec_len = l; }
    }
    long long spec_val = GAPS_DCL[spec].value;
    if (!gaps_in_stale_keys(GAPS_DCL[spec].key) || cached >= spec_val) {
        free(idx); free(low); return 0;
    }
    /* shorter_values = matches with strictly shorter key */
    int have_shorter = 0;
    long long max_shorter = 0;
    for (size_t i = 0; i < m; i++) {
        if (strlen(GAPS_DCL[idx[i]].key) < spec_len) {
            long long v = GAPS_DCL[idx[i]].value;
            if (!have_shorter || v > max_shorter) { max_shorter = v; have_shorter = 1; }
        }
    }
    long long threshold = have_shorter ? max_shorter : GAPS_DEFAULT_FALLBACK_CONTEXT;
    int verdict = cached <= threshold;
    free(idx); free(low);
    return verdict;
}

/* PoP: verified_codex_ctx_for_slug @ agent/model_metadata.py:_verified_codex_ctx_for_slug
 * Returns 1 with *out set, or 0 for Python None. */
int verified_codex_ctx_for_slug(const char *model_bare, long long *out)
{
    if (out) *out = 0;
    char *base = gaps_codex_variant_base(model_bare);
    if (!base) return 0;
    for (size_t i = 0; i < sizeof(GAPS_CODEX_EXACT)/sizeof(GAPS_CODEX_EXACT[0]); i++) {
        if (strcmp(base, GAPS_CODEX_EXACT[i].key) == 0) {
            if (out) *out = GAPS_CODEX_EXACT[i].ctx;
            free(base); return 1;
        }
    }
    for (size_t i = 0; i < sizeof(GAPS_CODEX_PREFIXES)/sizeof(GAPS_CODEX_PREFIXES[0]); i++) {
        const char *key = GAPS_CODEX_PREFIXES[i].key;
        size_t kl = strlen(key);
        int hit = strcmp(base, key) == 0 ||
                  (strncmp(base, key, kl) == 0 &&
                   (base[kl] == '-' || base[kl] == '.'));
        if (hit) {
            if (out) *out = GAPS_CODEX_PREFIXES[i].ctx;
            free(base); return 1;
        }
    }
    free(base);
    return 0;
}

/* PoP: anchored_context_tokens @ agent/usage_anchor.py:anchored_context_tokens
 * Returns 1 with *out set (prompt+completion+delta estimate), or 0 for None.
 * _anchor_matches and estimate_messages_tokens_rough are seamed collaborators. */
int anchored_context_tokens(const json_t *messages, const json_t *anchor,
                            bool charge_stale_thinking, long long *out)
{
    if (out) *out = 0;
    if (!anchor || anchor->type != JSON_OBJECT) return 0;
    if (!messages || messages->type != JSON_ARRAY) return 0;

    json_t *am_args = json_object();
    json_set(am_args, "messages", json_copy(messages));
    json_set(am_args, "anchor", json_copy(anchor));
    json_t *am = mmg_call("anchor_matches", am_args);
    json_free(am_args);
    int matches = mmg_truth(am);
    json_free(am);
    if (!matches) return 0;

    long long total = mmg_int_of(json_obj_get(anchor, "prompt_tokens"));
    const json_t *ct = json_obj_get(anchor, "completion_tokens");
    total += mmg_truth(ct) ? mmg_int_of(ct) : 0;
    long long base_count = mmg_int_of(json_obj_get(anchor, "base_count"));

    /* delta = messages[base_count:] with Python slice semantics (0 -> all,
     * negative -> from the end, past-the-end -> empty). */
    json_t *delta = json_array();
    size_t mc = messages->c.count;
    size_t start;
    if (base_count < 0) {
        long long s = (long long)mc + base_count;
        start = s > 0 ? (size_t)s : 0;
    } else {
        start = (size_t)base_count <= mc ? (size_t)base_count : mc;
    }
    for (size_t i = start; i < mc; i++)
        json_append(delta, json_copy(messages->c.items[i]));
    /* skip a leading assistant row (already priced in completion_tokens) */
    if (delta->c.count > 0) {
        json_t *d0 = delta->c.items[0];
        if (d0 && d0->type == JSON_OBJECT) {
            const char *role = json_get_str(d0, "role", NULL);
            if (role && strcmp(role, "assistant") == 0) {
                /* delta = delta[1:] */
                json_t *rest = json_array();
                for (size_t i = 1; i < delta->c.count; i++)
                    json_append(rest, json_copy(delta->c.items[i]));
                json_free(delta);
                delta = rest;
            }
        }
    }
    if (delta->c.count > 0) {
        json_t *est_args = json_object();
        json_set(est_args, "messages", json_copy(delta));
        json_set(est_args, "charge_stale_thinking", json_bool(charge_stale_thinking));
        json_t *est = mmg_call("estimate_messages_tokens_rough", est_args);
        json_free(est_args);
        total += mmg_int_of(est);
        json_free(est);
    }
    json_free(delta);
    if (out) *out = total;
    return 1;
}

/* ── Already-claimed by real mm_ ports in port_model_metadata.c: tags stripped,
 *    stub bodies retained (no external callers). ── */
int extract_flat_context_length(void) { return 0; }
int context_length_from_model_payload(void) { return 0; }
int model_name_suggests_minimax(void) { return 0; }
int model_name_suggests_stale_32k_underreport(void) { return 0; }

/* ── Remaining stubs owned by other waves (untouched) ── */

/* PoP: get_endpoint_metadata_cache_path @ agent/model_metadata.py:_get_endpoint_metadata_cache_path */
char *get_endpoint_metadata_cache_path(void) { return strdup(""); }

/* PoP: endpoint_disk_cache_get @ agent/model_metadata.py:_endpoint_disk_cache_get */
char *endpoint_disk_cache_get(const char *key) { return strdup("{}"); }

/* PoP: endpoint_disk_cache_put @ agent/model_metadata.py:_endpoint_disk_cache_put */
void endpoint_disk_cache_put(const char *key, const char *val) { (void)key; (void)val; }

/* PoP: strip_stale_thinking_for_estimate @ agent/model_metadata.py:_strip_stale_thinking_for_estimate */
char *strip_stale_thinking_for_estimate(void) { return strdup(""); }
