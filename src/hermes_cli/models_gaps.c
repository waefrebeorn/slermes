/*
 * models_gaps.c — REAL_GAP closure batch for hermes_cli/models.py.
 *
 * Faithful C11 ports; every function carries a single-line PoP annotation.
 * Conventions follow port_models_net.c / models_py_helpers.c:
 *   - JSON via libjson json_t
 *   - network via injectable http_fetch_fn (NULL -> models_live_http_fetch)
 *   - config.yaml via libyaml-backed config_py_* accessors
 */
#define _GNU_SOURCE

#include "models_gaps.h"
#include "port_config_py_helpers.h"
#include "port_auth_store.h"
#include "port_models_net.h"
#include "model_catalog.h"
#include "port_models_helpers.h"
#include "provider_meta.h"
#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>

/* Exported from src/hermes_cli/moa_pure_helpers.c (its PoP lives there). */
extern const char *silent_default_for_provider(const char *provider);

/* ── small string helpers ──────────────────────────────────────────────── */

static char *mg_strdup(const char *s) { return s ? strdup(s) : NULL; }

/* str.strip() into a malloc'd string ("" for NULL). */
static char *mg_strip_dup(const char *s) {
    if (!s) return strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r')) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static void mg_rstrip_slashes(char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n-1] == '/') s[--n] = '\0';
}

static int mg_endswith(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

static double mg_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* Python truthiness of a json node for `if x:` checks on dict/list. */
static bool mg_is_obj(const json_t *n) { return n && n->type == JSON_OBJECT; }

/* ── config plumbing ───────────────────────────────────────────────────── */

/* PoP: models_config_path @ hermes_cli/models.py:_config_path_via_config */
char *models_config_path(void) {
    char buf[HERMES_PATH_MAX];
    config_py_get_config_path(buf, sizeof buf);
    return strdup(buf);
}

/* PoP: models_load_config @ hermes_cli/models.py:_load_config_via_config */
json_t *models_load_config(void) {
    return config_py_load_config_impl(1); /* deep copy, like load_config() */
}

/* ── xAI catalog finalization ──────────────────────────────────────────── */

#define MG_XAI_TOP_MODEL "grok-4.6"
static const char *MG_XAI_CURATED_EXTRAS[] = {
    "grok-4.6",                 /* GA 2026-08 */
    "grok-4.5",                 /* GA 2026-07 */
    "grok-composer-2.5-fast",
};


/* ── default model per provider ────────────────────────────────────────── */

/* PoP: models_get_default_model_for_provider @ hermes_cli/models.py:get_default_model_for_provider */
char *models_get_default_model_for_provider(const char *provider) {
    const char *norm = normalize_provider(provider ? provider : "");
    int count = model_provider_model_count(norm);

    /* Metered aggregators: never silently default to the (most expensive)
     * first catalog entry — use the catalog-labeled low-cost default. */
    if ((strcmp(norm, "nous") == 0 || strcmp(norm, "openrouter") == 0)) {
        const char *preferred = silent_default_for_provider(norm);
        int in_models = 0;
        if (preferred) {
            for (int i = 0; i < count && !in_models; i++) {
                const char *m = model_provider_model_at(norm, i);
                if (m && strcmp(m, preferred) == 0) in_models = 1;
            }
        }
        /* Trust the preferred default even when the provider has no static
         * catalog (OpenRouter's picker list is fetched live). */
        if (preferred && (in_models || count == 0))
            return strdup(preferred);
    }
    if (count > 0) {
        const char *first = model_provider_model_at(norm, 0);
        if (first) return strdup(first);
    }
    return strdup("");
}

/* ── custom-provider TLS context ───────────────────────────────────────── */

/* PoP: models_custom_provider_ssl_context @ hermes_cli/models.py:_custom_provider_ssl_context */
models_tls_ctx_t *models_custom_provider_ssl_context(const char *base_url) {
    if (!base_url || !*base_url) return NULL;
    json_t *custom_providers = NULL;
    json_t *config = config_py_read_raw_config();
    if (config) {
        json_t *cp = json_obj_get(config, "custom_providers");
        custom_providers = cp ? json_copy(cp) : NULL;
        json_free(config);
    }
    json_t *tls = config_py_get_custom_provider_tls_settings(base_url, custom_providers);
    if (custom_providers) json_free(custom_providers);
    if (!tls) return NULL;

    models_tls_ctx_t *ctx = calloc(1, sizeof(models_tls_ctx_t));
    if (!ctx) { json_free(tls); return NULL; }
    ctx->ca_cert[0] = '\0';

    json_t *verify = json_obj_get(tls, "ssl_verify");
    if (verify && verify->type == JSON_BOOL && verify->bool_val == false) {
        ctx->verify_off = true;   /* CERT_NONE */
        json_free(tls);
        return ctx;
    }
    json_t *ca = json_obj_get(tls, "ssl_ca_cert");
    if (ca && ca->type == JSON_STRING && ca->str_val && ca->str_val[0]) {
        struct stat st;
        if (stat(ca->str_val, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(ctx->ca_cert, sizeof(ctx->ca_cert), "%s", ca->str_val);
            json_free(tls);
            return ctx;
        }
    }
    json_free(tls);
    free(ctx);
    return NULL; /* no per-provider TLS override applies */
}

/* ── config-dict lookups ───────────────────────────────────────────────── */

/* PoP: models_get_provider_config_dict @ hermes_cli/models.py:_get_provider_config_dict */
json_t *models_get_provider_config_dict(const char *provider) {
    char *key = mg_strip_dup(provider);
    if (!key || !*key) { free(key); return json_object(); }
    json_t *out = json_object();
    json_t *config = models_load_config();
    if (config) {
        json_t *providers_cfg = json_obj_get(config, "providers");
        if (mg_is_obj(providers_cfg)) {
            json_t *entry = json_obj_get(providers_cfg, key);
            if (!entry) {
                for (char *p = key; *p; p++) *p = (char)tolower((unsigned char)*p);
                entry = json_obj_get(providers_cfg, key);
            }
            if (mg_is_obj(entry)) {
                json_free(out);
                out = json_copy(entry);
            }
        }
        json_free(config);
    }
    free(key);
    return out;
}

/* PoP: models_get_model_config_dict_simple @ hermes_cli/models.py:_get_model_config_dict */
json_t *models_get_model_config_dict_simple(void) {
    json_t *out = json_object();
    json_t *config = models_load_config();
    if (config) {
        json_t *model_cfg = json_obj_get(config, "model");
        if (mg_is_obj(model_cfg)) {
            json_free(out);
            out = json_copy(model_cfg);
        }
        json_free(config);
    }
    return out;
}

/* PoP: models_get_custom_base_url @ hermes_cli/models.py:_get_custom_base_url */
char *models_get_custom_base_url(void) {
    json_t *model_cfg = models_get_model_config_dict_simple();
    char *val = mg_strip_dup(json_get_str(model_cfg, "base_url", ""));
    json_free(model_cfg);
    return val ? val : strdup("");
}

/* ── portal / key resolution ───────────────────────────────────────────── */

/* PoP: models_resolve_nous_portal_url @ hermes_cli/models.py:_resolve_nous_portal_url */
char *models_resolve_nous_portal_url(void) {
    json_t *state = get_provider_auth_state("nous");
    if (state) {
        const char *portal = json_get_str(state, "portal_base_url", "");
        char *stripped = mg_strip_dup(portal);
        json_free(state);
        if (stripped && *stripped) {
            mg_rstrip_slashes(stripped);
            return stripped;
        }
        free(stripped);
    }
    /* DEFAULT_NOUS_PORTAL_URL (include/hermes_auth.h), rstrip("/") */
    return strdup("https://portal.nousresearch.com");
}

/* PoP: models_resolve_openrouter_api_key @ hermes_cli/models.py:_resolve_openrouter_api_key */
char *models_resolve_openrouter_api_key(void) {
    const char *raw = getenv("OPENROUTER_API_KEY");
    char *key = mg_strip_dup(raw);
    return key ? key : strdup("");
}

/* ── ollama native API root normalization (pure) ───────────────────────── */

/* PoP: models_root_for_ollama_native_api @ hermes_cli/models.py:_root_for_ollama_native_api */
char *models_root_for_ollama_native_api(const char *base_url) {
    char *root = mg_strip_dup(base_url);
    if (!root) return NULL;
    mg_rstrip_slashes(root);
    if (root[0] == ':') {
        size_t n = strlen(root);
        char *joined = malloc(n + strlen("http://127.0.0.1") + 1);
        if (!joined) { free(root); return NULL; }
        snprintf(joined, n + 18, "http://127.0.0.1%s", root);
        free(root);
        root = joined;
    } else if (root[0] && !strstr(root, "://")) {
        size_t n = strlen(root);
        char *joined = malloc(n + 8);
        if (!joined) { free(root); return NULL; }
        snprintf(joined, n + 8, "http://%s", root);
        free(root);
        root = joined;
    }
    static const char *const suffixes[] = { "/api/tags", "/v1/models", "/api", "/v1" };
    for (size_t i = 0; i < sizeof(suffixes)/sizeof(suffixes[0]); i++) {
        if (mg_endswith(root, suffixes[i])) {
            size_t keep = strlen(root) - strlen(suffixes[i]);
            root[keep] = '\0';
            mg_rstrip_slashes(root);
            break;
        }
    }
    return root;
}

/* PoP: models_normalize_openai_base_url @ hermes_cli/models.py:_normalize_openai_base_url */
char *models_normalize_openai_base_url(const char *base_url) {
    char *value = mg_strip_dup(base_url);
    if (!value) return strdup("");
    if (value[0] == ':') {
        size_t n = strlen(value);
        char *joined = malloc(n + 18);
        if (!joined) { free(value); return strdup(""); }
        snprintf(joined, n + 18, "http://127.0.0.1%s", value);
        free(value);
        return joined;
    }
    if (value[0] && !strstr(value, "://")) {
        size_t n = strlen(value);
        char *joined = malloc(n + 8);
        if (!joined) { free(value); return strdup(""); }
        snprintf(joined, n + 8, "http://%s", value);
        free(value);
        return joined;
    }
    return value;
}

/* ── blake2b (RFC 7693) — 8-byte digest for _ollama_probe_cache_key ────── */

typedef struct { uint64_t h[8], t[2]; size_t buflen; uint8_t buf[128]; } mg_blake2b_t;

static const uint64_t MG_B2B_IV[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL,
    0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};
static const uint8_t MG_B2B_SIGMA[10][16] = {
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},{14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},
    {11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},{7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},
    {9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},{2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},
    {12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},{13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},
    {6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},{10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0},
};
#define MG_ROTR(x,n) (((x)>>(n))|((x)<<(64-(n))))
#define MG_G(a,b,c,d,x,y) do { \
    v[a]+=v[b]+m[x]; v[d]=MG_ROTR(v[d]^v[a],32); v[c]+=v[d]; v[b]=MG_ROTR(v[b]^v[c],24); \
    v[a]+=v[b]+m[y]; v[d]=MG_ROTR(v[d]^v[a],16); v[c]+=v[d]; v[b]=MG_ROTR(v[b]^v[c],63); \
} while (0)

static void mg_blake2b_compress(mg_blake2b_t *Sp, uint64_t t0, const uint8_t block[128], int is_final) {
    uint64_t m[16], v[16];
    int r;
    for (int i = 0; i < 16; i++)
        m[i] = (uint64_t)block[i*8] | ((uint64_t)block[i*8+1]<<8) |
               ((uint64_t)block[i*8+2]<<16) | ((uint64_t)block[i*8+3]<<24) |
               ((uint64_t)block[i*8+4]<<32) | ((uint64_t)block[i*8+5]<<40) |
               ((uint64_t)block[i*8+6]<<48) | ((uint64_t)block[i*8+7]<<56);
    for (int i = 0; i < 8; i++) { v[i] = Sp->h[i]; v[8+i] = MG_B2B_IV[i]; }
    v[12] ^= t0;
    if (is_final) v[14] ^= ~(uint64_t)0;
    for (r = 0; r < 12; r++) {
        const uint8_t *sg = MG_B2B_SIGMA[r % 10];
        MG_G(0,4,8,12,sg[0],sg[1]);
        MG_G(1,5,9,13,sg[2],sg[3]);
        MG_G(2,6,10,14,sg[4],sg[5]);
        MG_G(3,7,11,15,sg[6],sg[7]);
        MG_G(0,5,10,15,sg[8],sg[9]);
        MG_G(1,6,11,12,sg[10],sg[11]);
        MG_G(2,7,8,13,sg[12],sg[13]);
        MG_G(3,4,9,14,sg[14],sg[15]);
    }
    for (int i = 0; i < 8; i++) Sp->h[i] ^= v[i] ^ v[i+8];
}
#undef MG_G
#undef MG_ROTR

static void mg_blake2b_init(mg_blake2b_t *S, size_t outlen) {
    for (int i = 0; i < 8; i++) S->h[i] = MG_B2B_IV[i];
    S->h[0] ^= 0x01010000ULL | (uint64_t)outlen; /* no key, fanout=1, depth=1 */
    S->t[0] = S->t[1] = 0;
    S->buflen = 0;
    memset(S->buf, 0, sizeof S->buf);
}
static void mg_blake2b_update(mg_blake2b_t *S, const uint8_t *in, size_t len) {
    while (len > 0) {
        if (S->buflen == 128 && len > 0) {
            mg_blake2b_compress(S, S->t[0], S->buf, 0);
            S->buflen = 0;
        }
        size_t take = 128 - S->buflen;
        if (take > len) take = len;
        memcpy(S->buf + S->buflen, in, take);
        S->buflen += take; in += take; len -= take;
        S->t[0] += (uint64_t)take;
    }
}
static void mg_blake2b_final(mg_blake2b_t *S, uint8_t *out, size_t outlen) {
    memset(S->buf + S->buflen, 0, 128 - S->buflen);
    mg_blake2b_compress(S, S->t[0], S->buf, 1);
    for (size_t i = 0; i < outlen; i++) out[i] = (uint8_t)(S->h[i/8] >> (8*(i%8)));
}
/* PoP: models_ollama_probe_cache_key @ hermes_cli/models.py:_ollama_probe_cache_key */
char *models_ollama_probe_cache_key(const char *root, const char *headers_json) {
    if (!headers_json || !*headers_json) return mg_strdup(root ? root : "");
    json_t *headers = json_parse(headers_json, NULL);
    /* Python `if headers:` on a dict — an EMPTY dict is falsy, so no fingerprint. */
    if (!headers || headers->type != JSON_OBJECT || headers->c.count == 0) {
        if (headers) json_free(headers);
        return mg_strdup(root ? root : "");
    }

    /* normalized_headers = sorted((key.lower(), value)) */
    char *out = NULL;
    size_t n = headers->c.count;
    typedef struct { char *k; char *v; } kv_t;
    kv_t *pairs = malloc((n ? n : 1) * sizeof(kv_t));
    size_t np = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = headers->c.keys[i];
        json_t *v = headers->c.items[i];
        if (!k || !v) continue;
        char *kl = strdup(k);
        if (!kl) continue;
        for (char *p = kl; *p; p++) *p = (char)tolower((unsigned char)*p);
        char *vs = NULL;
        if (v->type == JSON_STRING) vs = strdup(v->str_val ? v->str_val : "");
        else { char *s = json_serialize(v); vs = s; }
        pairs[np].k = kl;
        pairs[np].v = vs ? vs : strdup("");
        np++;
    }
    json_free(headers);

    /* insertion sort by (key, value) */
    for (size_t i = 1; i < np; i++) {
        kv_t tmp = pairs[i];
        size_t j = i;
        while (j > 0 && (strcmp(pairs[j-1].k, tmp.k) > 0 ||
                         (strcmp(pairs[j-1].k, tmp.k) == 0 &&
                          strcmp(pairs[j-1].v, tmp.v) > 0))) {
            pairs[j] = pairs[j-1];
            j--;
        }
        pairs[j] = tmp;
    }

    /* header_blob = compact JSON of the sorted pair list, like
     * json.dumps(normalized_headers, ensure_ascii=False, separators=(",", ":")) */
    size_t cap = 16, blen = 0;
    char *blob = malloc(cap);
    if (!blob) { goto done; }
    blob[blen++] = '[';
    for (size_t i = 0; i < np; i++) {
        char *ek = json_escape_string(pairs[i].k);
        char *ev = json_escape_string(pairs[i].v);
        size_t need = blen + (ek?strlen(ek):2) + (ev?strlen(ev):2) + 12;
        if (need + 2 > cap) { while (cap <= need + 2) cap *= 2;
            blob = realloc(blob, cap); }
        if (!blob) goto done;
        blen += (size_t)snprintf(blob + blen, cap - blen, "%s[%s,%s]",
                                 i ? "," : "", ek ? ek : "\"\"", ev ? ev : "\"\"");
        free(ek); free(ev);
    }
    if (blen + 2 > cap) { cap += 4; blob = realloc(blob, cap); if (!blob) goto done; }
    blob[blen++] = ']';
    blob[blen] = '\0';

    mg_blake2b_t S;
    mg_blake2b_init(&S, 8);
    mg_blake2b_update(&S, (const uint8_t *)blob, blen);
    uint8_t digest[8];
    mg_blake2b_final(&S, digest, 8);

    char fp[17];
    for (int i = 0; i < 8; i++) snprintf(fp + i*2, 3, "%02x", digest[i]);
    size_t outn = strlen(root ? root : "") + strlen("|headers:") + 16 + 1;
    out = malloc(outn);
    if (out) snprintf(out, outn, "%s|headers:%s", root ? root : "", fp);
done:
    for (size_t i = 0; i < np; i++) { free(pairs[i].k); free(pairs[i].v); }
    free(pairs);
    free(blob);
    return out;
}


/* ════════════════════════════════════════════════════════════════════════
 * Batch 2 — pricing cache, novita pricing, provider dispatch, listing
 * ════════════════════════════════════════════════════════════════════════ */

#include "auth_helpers.h"
#include "port_models_py_helpers.h"

/* Extern ports declared in port_models_pure.h but with injectable-transport
 * signatures; re-declared here to avoid the conflicting novita prototype. */
extern char *models_fetch_models_with_pricing(void *fetch, void *ctx,
                                              const char *api_key, const char *base_url,
                                              int force_refresh);
extern char *fetch_ai_gateway_pricing(double timeout_sec, bool force_refresh); /* models_helpers.c */
/* port_models_net.c */
extern void models_resolve_nous_pricing_credentials(void *resolve, void *ctx,
                                                    char **out_key, char **out_base);

#define MG_FAILED_CATALOG_TTL_SECONDS 120.0

typedef struct mg_price_entry {
    char *key;
    json_t *value;              /* owned */
    double retry_after;         /* monotonic deadline, -1 when none */
    struct mg_price_entry *next;
} mg_price_entry_t;
static mg_price_entry_t *mg_pricing_cache = NULL;

static mg_price_entry_t *mg_price_find(const char *key) {
    for (mg_price_entry_t *e = mg_pricing_cache; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e;
    return NULL;
}

/* PoP: models_cached_catalog @ hermes_cli/models.py:_cached_catalog */
json_t *models_cached_catalog(const char *cache_key) {
    if (!cache_key) return NULL;
    mg_price_entry_t *e = mg_price_find(cache_key);
    if (!e) return NULL;
    if (e->retry_after >= 0 && mg_monotonic() >= e->retry_after) {
        /* expired failure marker: drop it and go fetch */
        json_free(e->value);
        free(e->key);
        if (mg_pricing_cache == e) mg_pricing_cache = e->next;
        else {
            for (mg_price_entry_t *p = mg_pricing_cache; p; p = p->next)
                if (p->next == e) { p->next = e->next; break; }
        }
        free(e);
        return NULL;
    }
    return e->value;
}

/* PoP: models_cache_catalog @ hermes_cli/models.py:_cache_catalog */
json_t *models_cache_catalog(const char *cache_key, json_t *result) {
    if (!cache_key) return result;
    mg_price_entry_t *e = mg_price_find(cache_key);
    if (e) { json_free(e->value); e->value = result; }
    else {
        e = calloc(1, sizeof(*e));
        if (!e) return result;
        e->key = strdup(cache_key);
        e->value = result;
        e->next = mg_pricing_cache;
        mg_pricing_cache = e;
    }
    /* An empty catalog gets an expiry so a dead gateway isn't remembered. */
    e->retry_after = (result && result->type == JSON_OBJECT && result->c.count > 0)
                         ? -1.0
                         : mg_monotonic() + MG_FAILED_CATALOG_TTL_SECONDS;
    return result;
}

/* PoP: models_fetch_novita_pricing @ hermes_cli/models.py:_fetch_novita_pricing */
char *models_fetch_novita_pricing(int force_refresh) {
    const char *raw_key = getenv("NOVITA_API_KEY");
    char *api_key = mg_strip_dup(raw_key);
    if (!api_key || !*api_key) { free(api_key); return strdup("{}"); }

    const char *raw_base = getenv("NOVITA_BASE_URL");
    char *base_url = (raw_base && *raw_base) ? mg_strip_dup(raw_base)
                                             : strdup("https://api.novita.ai/openai/v1");
    mg_rstrip_slashes(base_url);
    const char *cache_key = base_url;

    json_t *result = NULL;
    if (!force_refresh) {
        json_t *cached = models_cached_catalog(cache_key);
        if (cached) {
            char *out = json_serialize(cached);
            free(api_key); free(base_url);
            return out ? out : strdup("{}");
        }
    }

    size_t urln = strlen(cache_key) + strlen("/models") + 1;
    char *url = malloc(urln);
    if (!url) { free(api_key); free(base_url); return strdup("{}"); }
    snprintf(url, urln, "%s/models", cache_key);

    size_t hn = strlen(api_key) + 64;
    char *auth = malloc(hn);
    char *headers = NULL;
    if (auth) {
        snprintf(auth, hn, "{\"Authorization\":\"Bearer %s\",\"Accept\":\"application/json\","
                           "\"User-Agent\":\"hermes-cli/0.20.5-slermes\"}", api_key);
        headers = auth;
    }

    char *body = NULL; size_t blen2 = 0;
    int rc = headers ? models_live_http_fetch(url, headers, &body, &blen2, NULL) : -1;
    free(auth); free(url);
    if (rc != 0 || !body) {
        free(body);
        result = json_object();
        models_cache_catalog(cache_key, result);
        char *out = json_serialize(result);
        free(api_key); free(base_url);
        return out ? out : strdup("{}");
    }
    json_t *payload = json_parse(body, NULL);
    free(body);
    result = json_object();
    if (payload) {
        json_t *data = json_obj_get(payload, "data");
        if (data && data->type == JSON_ARRAY) {
            for (size_t i = 0; i < data->c.count; i++) {
                json_t *item = data->c.items[i];
                if (!item || item->type != JSON_OBJECT) continue;
                json_t *mid = json_obj_get(item, "id");
                if (!mid || mid->type != JSON_STRING || !mid->str_val) continue;
                json_t *inp = json_obj_get(item, "input_token_price_per_m");
                json_t *outp = json_obj_get(item, "output_token_price_per_m");
                if ((!inp || inp->type != JSON_NUMBER) && (!outp || outp->type != JSON_NUMBER))
                    continue;
                double in_v = (inp && inp->type == JSON_NUMBER) ? inp->num_val : 0.0;
                double out_v = (outp && outp->type == JSON_NUMBER) ? outp->num_val : 0.0;
                /* Novita prices are USD per million tokens in 1e-4 units;
                 * convert to per-token strings like Python's str(float(...)). */
                char pin[64], pout[64];
                snprintf(pin, sizeof pin, "%.17g", in_v / 10000.0 / 1000000.0);
                snprintf(pout, sizeof pout, "%.17g", out_v / 10000.0 / 1000000.0);
                json_t *row = json_object();
                json_set(row, "prompt", json_string(pin));
                json_set(row, "completion", json_string(pout));
                json_set(result, mid->str_val, row);
            }
        }
        json_free(payload);
    }
    models_cache_catalog(cache_key, result);
    char *out = json_serialize(result);
    free(api_key); free(base_url);
    return out ? out : strdup("{}");
}

/* PoP: models_get_pricing_for_provider @ hermes_cli/models.py:get_pricing_for_provider */
char *models_get_pricing_for_provider(const char *provider, int force_refresh) {
    const char *normalized = normalize_provider(provider ? provider : "");
    if (strcmp(normalized, "openrouter") == 0) {
        char *key = models_resolve_openrouter_api_key();
        char *out = models_fetch_models_with_pricing(NULL, NULL, key,
                                                     "https://openrouter.ai/api",
                                                     force_refresh);
        free(key);
        return out ? out : strdup("{}");
    }
    if (strcmp(normalized, "ai-gateway") == 0) {
        char *out = fetch_ai_gateway_pricing(8.0, force_refresh != 0);
        return out ? out : strdup("{}");
    }
    if (strcmp(normalized, "novita") == 0) {
        return models_fetch_novita_pricing(force_refresh);
    }
    if (strcmp(normalized, "deepinfra") == 0) {
        /* _fetch_deepinfra_pricing: chat-tagged catalog items -> picker
         * pricing strings ($/MTok -> per-token), memoized in _pricing_cache. */
        char *ck = NULL, *fu = NULL;
        models_deepinfra_catalog_url(&ck, &fu);
        if (!force_refresh && ck) {
            json_t *cached = models_cached_catalog(ck);
            if (cached) {
                char *out = json_serialize(cached);
                free(ck); free(fu);
                return out ? out : strdup("{}");
            }
        }
        json_t *catalog = models_fetch_deepinfra_catalog(NULL, NULL, force_refresh);
        json_t *items = models_deepinfra_models_by_tag(catalog, "chat");
        json_t *pricing = json_object();
        if (items) {
            for (size_t i = 0; i < items->c.count; i++) {
                json_t *item = items->c.items[i];
                if (!item || item->type != JSON_OBJECT) continue;
                const char *mid = json_get_str(item, "id", "");
                if (!*mid) continue;
                json_t *metadata = json_obj_get(item, "metadata");
                json_t *pricing_n = mg_is_obj(metadata) ? json_obj_get(metadata, "pricing") : NULL;
                if (!mg_is_obj(pricing_n)) continue;
                json_t *entry = json_object();
                json_t *inp = json_obj_get(pricing_n, "input_tokens");
                json_t *outp = json_obj_get(pricing_n, "output_tokens");
                json_t *cache_read = json_obj_get(pricing_n, "cache_read_tokens");
                char buf[64];
                if (inp && inp->type == JSON_NUMBER) {
                    snprintf(buf, sizeof buf, "%.17g", inp->num_val / 1000000.0);
                    json_set(entry, "prompt", json_string(buf));
                }
                if (outp && outp->type == JSON_NUMBER) {
                    snprintf(buf, sizeof buf, "%.17g", outp->num_val / 1000000.0);
                    json_set(entry, "completion", json_string(buf));
                }
                if (cache_read && cache_read->type == JSON_NUMBER) {
                    snprintf(buf, sizeof buf, "%.17g", cache_read->num_val / 1000000.0);
                    json_set(entry, "input_cache_read", json_string(buf));
                }
                if (entry->c.count > 0) json_set(pricing, mid, entry);
                else json_free(entry);
            }
            json_free(items);
        }
        if (catalog) json_free(catalog);
        models_cache_catalog(ck ? ck : "", pricing);
        char *out = json_serialize(pricing);
        free(ck); free(fu);
        return out ? out : strdup("{}");
    }
    if (strcmp(normalized, "fireworks") == 0) {
        /* The ported helper mirrors _fireworks_pricing_from_models_dev,
         * including its own _pricing_cache memoization. */
        json_t *fw = models_fireworks_pricing_from_models_dev(force_refresh);
        char *ser = fw ? json_serialize(fw) : NULL;
        if (fw) json_free(fw);
        return ser ? ser : strdup("{}");
    }
    if (strcmp(normalized, "nous") == 0) {
        char *key = NULL, *base = NULL;
        models_resolve_nous_pricing_credentials(NULL, NULL, &key, &base);
        char *out = NULL;
        if (base && *base)
            out = models_fetch_models_with_pricing(NULL, NULL, key, base, force_refresh);
        else
            out = strdup("{}");
        free(key); free(base);
        return out ? out : strdup("{}");
    }
    return strdup("{}");
}


/* PoP: models_configured_custom_provider_ids_json @ hermes_cli/models.py:_configured_custom_provider_ids */
static void mg_append_slug(json_t *arr, const char *name_or_key, int has_name) {
    /* custom_provider_slug: "custom:" + strip().lower() with spaces -> '-'.
     * Python slugifies (name or key) when name given, else name alone. */
    const char *src = (name_or_key && *name_or_key) ? name_or_key : "";
    char out[256];
    size_t o = 0;
    out[o++] = 'c'; out[o++] = 'u'; out[o++] = 's'; out[o++] = 't'; out[o++] = 'o'; out[o++] = 'm'; out[o++] = ':';
    const char *p2 = src;
    while (*p2 == ' ' || *p2 == '\t') p2++;
    size_t n = strlen(p2);
    while (n > 0 && (p2[n-1] == ' ' || p2[n-1] == '\t')) n--;
    for (size_t i = 0; i < n && o < sizeof(out) - 1; i++) {
        char c = p2[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c == ' ') c = '-';
        out[o++] = c;
    }
    out[o] = '\0';
    /* dedupe: skip an id already present */
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *e = arr->c.items[i];
        if (e->type == JSON_STRING && e->str_val && strcmp(e->str_val, out) == 0)
            return;
    }
    json_append(arr, json_string(out));
}

char *models_configured_custom_provider_ids_json(void) {
    json_t *ids = json_array();
    if (!ids) return NULL;
    json_append(ids, json_string("custom"));
    json_t *config = models_load_config();
    if (config) {
        json_t *providers = json_obj_get(config, "providers");
        if (mg_is_obj(providers)) {
            for (size_t i = 0; i < providers->c.count; i++) {
                json_t *entry = providers->c.items[i];
                const char *key = providers->c.keys[i];
                if (!mg_is_obj(entry)) continue;
                const char *name = json_get_str(entry, "name", "");
                mg_append_slug(ids, *name ? name : key, 1);
            }
        }
        json_t *legacy = json_obj_get(config, "custom_providers");
        if (legacy && legacy->type == JSON_ARRAY) {
            for (size_t i = 0; i < legacy->c.count; i++) {
                json_t *entry = legacy->c.items[i];
                if (!mg_is_obj(entry)) continue;
                mg_append_slug(ids, json_get_str(entry, "name", ""), 0);
            }
        }
        json_free(config);
    }
    char *out = json_serialize(ids);
    json_free(ids);
    return out;
}

/* PoP: models_list_available_providers_json @ hermes_cli/models.py:list_available_providers */
char *models_list_available_providers_json(void) {
    json_t *result = json_array();
    if (!result) return NULL;

    /* Derive display order from CANONICAL_PROVIDERS + "custom". */
    int n_providers = model_catalog_provider_count();

    /* Build reverse alias map once: canonical -> [aliases]. */
    int n_aliases = model_catalog_alias_count();
    json_t **alias_lists = calloc((size_t)(n_providers + 1), sizeof(json_t *));
    for (int i = 0; i < n_aliases; i++) {
        const char *alias = NULL, *canon = NULL;
        if (!model_catalog_alias_at(i, &alias, &canon)) continue;
        int slot = -1;
        for (int pidx = 0; pidx < n_providers; pidx++) {
            const char *pid = model_catalog_provider_at(pidx);
            if (pid && strcmp(pid, canon) == 0) { slot = pidx; break; }
        }
        if (slot < 0) continue;
        if (!alias_lists[slot]) alias_lists[slot] = json_array();
        json_append(alias_lists[slot], json_string(alias));
    }

    for (int pidx = 0; pidx <= n_providers; pidx++) {
        const char *pid;
        if (pidx < n_providers) pid = model_catalog_provider_at(pidx);
        else { static const char *custom_id = "custom"; pid = custom_id; }
        if (!pid) continue;

        char *label = provider_label(pid);

        int has_creds = 0;
        if (strcmp(pid, "custom") == 0) {
            char *custom_base = models_get_custom_base_url();
            has_creds = (custom_base && *custom_base);
            free(custom_base);
        } else if (strcmp(pid, "openrouter") == 0) {
            const char *env_key = getenv("OPENROUTER_API_KEY");
            has_creds = auth_has_usable_secret(env_key, 4);
        } else {
            /* get_auth_status derives from the provider's auth state. */
            json_t *state = get_provider_auth_state(pid);
            if (state) {
                json_t *li = json_obj_get(state, "logged_in");
                json_t *cfg = json_obj_get(state, "configured");
                has_creds = (li && li->type == JSON_BOOL && li->bool_val) ||
                            (cfg && cfg->type == JSON_BOOL && cfg->bool_val);
                json_free(state);
            }
        }

        json_t *row = json_object();
        json_set(row, "id", json_string(pid));
        json_set(row, "label", json_string(label ? label : pid));
        if (alias_lists[pidx]) {
            json_set(row, "aliases", alias_lists[pidx]);
        } else {
            json_set(row, "aliases", json_array());
        }
        json_set(row, "authenticated", json_bool(has_creds ? true : false));
        json_append(result, row);
        free(label);
    }
    free(alias_lists);

    char *out = json_serialize(result);
    json_free(result);
    return out;
}


/* ════════════════════════════════════════════════════════════════════════
 * Batch 3 — reasoning-capability caches (OpenRouter + Nous Portal)
 * ════════════════════════════════════════════════════════════════════════ */

#define MG_CAPS_DISK_TTL_SECONDS (24.0 * 3600.0)
#define MG_CAPS_FAIL_TTL_SECONDS 60.0
static const char MG_OPENROUTER_CATALOG_URL[] = "https://openrouter.ai/api/v1/models";
static const char MG_HERMES_USER_AGENT[] = "hermes-cli/0.20.5-slermes";

typedef struct {
    json_t *cache;              /* serialized caps map (owned) or NULL */
    double failed_at;           /* monotonic, -1 when none */
    int disk_checked;
    int warm_started;
} mg_caps_state_t;
static mg_caps_state_t g_mg_or_caps;
static mg_caps_state_t g_mg_nous_caps;

/* PoP: models_warm_reasoning_caps_async @ hermes_cli/models.py:_warm_reasoning_caps_async */
static void *mg_thread_trampoline(void *fn) {
    ((void (*)(void))(uintptr_t)fn)();
    return NULL;
}

void models_warm_reasoning_caps_async(void (*refresh)(void)) {
    /* Skipped under pytest: a mid-suite background fetch would make cache
     * state, and therefore test behavior, timing-dependent. */
    if (getenv("PYTEST_CURRENT_TEST")) return;
    pthread_t thr;
    if (pthread_create(&thr, NULL, mg_thread_trampoline, (void *)(uintptr_t)refresh) == 0)
        pthread_detach(thr);
}

/* Disk mirror read for *url*: {"url": {"ts": ..., "caps": {...}}}. Returns a
 * fresh json_t {"caps":..., "age": seconds} node (caller frees) or NULL. */
static json_t *mg_caps_disk_read(const char *url, double *out_age) {
    char *path = models_reasoning_caps_disk_path();
    if (!path) return NULL;
    char *err = NULL;
    json_t *doc = json_parse_file(path, &err);
    if (err) free(err);
    free(path);
    if (!doc || doc->type != JSON_OBJECT) { if (doc) json_free(doc); return NULL; }
    json_t *entry = json_obj_get(doc, url);
    json_t *caps = entry ? json_obj_get(entry, "caps") : NULL;
    json_t *result = NULL;
    if (entry && entry->type == JSON_OBJECT && caps && caps->type == JSON_OBJECT &&
        caps->c.count > 0) {
        json_t *ts = json_obj_get(entry, "ts");
        double saved = (ts && ts->type == JSON_NUMBER) ? ts->num_val : 0.0;
        double age = (double)time(NULL) - saved;
        if (!(age >= 0.0)) age = MG_CAPS_DISK_TTL_SECONDS; /* bad ts -> stale */
        result = json_object();
        json_set(result, "caps", json_copy(caps));
        json_set(result, "age", json_number(age));
    }
    json_free(doc);
    if (result && out_age) {
        json_t *ag = json_obj_get(result, "age");
        *out_age = ag ? ag->num_val : MG_CAPS_DISK_TTL_SECONDS;
    }
    return result;
}

/* Merge url's caps map into the shared disk mirror atomically (best-effort). */
static void mg_caps_disk_write(const char *url, const json_t *caps) {
    char *path = models_reasoning_caps_disk_path();
    if (!path || !url || !caps) { free(path); return; }
    json_t *doc = json_parse_file(path, NULL);
    if (!doc || doc->type != JSON_OBJECT) { if (doc) json_free(doc); doc = json_object(); }
    if (!doc) { free(path); return; }
    json_t *entry = json_object();
    if (entry) {
        json_set(entry, "ts", json_number((double)time(NULL)));
        json_set(entry, "caps", json_copy(caps));
        json_set(doc, url, entry);
    }
    char *ser = json_serialize(doc);
    json_free(doc);
    if (!ser) { free(path); return; }
    size_t plen = strlen(path);
    char dir[4096];
    snprintf(dir, sizeof(dir), "%.*s", (int)(plen - strlen("/reasoning_caps.json")), path);
    mkdir(dir, 0755); /* ignore EEXIST */
    char *tmp = malloc(plen + 8);
    int wrote = 0;
    if (tmp) {
        snprintf(tmp, plen + 8, "%s.tmp", path);
        FILE *f = fopen(tmp, "w");
        if (f) { fputs(ser, f); fclose(f);
                 if (rename(tmp, path) == 0) wrote = 1; else unlink(tmp); }
    }
    if (!wrote) {
        FILE *f2 = fopen(path, "w");
        if (f2) { fputs(ser, f2); fclose(f2); }
    }
    free(tmp);
    free(ser);
    free(path);
}

/* PoP: models_hydrate_reasoning_caps_from_disk @ hermes_cli/models.py:_hydrate_reasoning_caps_from_disk */
json_t *models_hydrate_reasoning_caps_from_disk(const char *url, void (*refresh)(void)) {
    double age = 0.0;
    json_t *disk = mg_caps_disk_read(url, &age);
    if (!disk) return NULL;
    json_t *caps = json_copy(json_obj_get(disk, "caps"));
    json_free(disk);
    if (!caps) return NULL;
    /* A copy past its TTL is still returned — a stale verdict beats no
       verdict — with a background refresh so the next run is current. */
    if (age >= MG_CAPS_DISK_TTL_SECONDS)
        models_warm_reasoning_caps_async(refresh);
    return caps;
}

/* Fetch one OpenRouter-shaped /v1/models catalog -> per-model caps map.
 * Returns a fresh json_t map, mirroring it to disk, or NULL. */
static json_t *mg_fetch_caps_catalog(const char *url, double timeout) {
    (void)timeout;
    if (!url) return NULL;
    char headers[256];
    snprintf(headers, sizeof headers,
             "{\"Accept\":\"application/json\",\"User-Agent\":\"%s\"}", MG_HERMES_USER_AGENT);
    char *body = NULL; size_t blen = 0;
    int rc = models_live_http_fetch(url, headers, &body, &blen, NULL);
    if (rc != 0 || !body) { if (body) free(body); return NULL; }
    json_t *payload = json_parse(body, NULL);
    free(body);
    if (!payload) return NULL;
    json_t *data = (payload->type == JSON_OBJECT) ? json_obj_get(payload, "data") : NULL;
    if (!data || data->type != JSON_ARRAY) { json_free(payload); return NULL; }

    json_t *map = json_object();
    if (!map) { json_free(payload); return NULL; }
    for (size_t i = 0; i < data->c.count; i++) {
        json_t *item = data->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        json_t *idn = json_obj_get(item, "id");
        if (!idn || idn->type != JSON_STRING) continue;
        char *mid = mg_strip_dup(idn->str_val);
        if (!mid || !*mid) { free(mid); continue; }
        /* parse_openrouter_reasoning_capabilities tri-state */
        json_t *caps = NULL;
        json_t *params = json_obj_get(item, "supported_parameters");
        if (params && params->type == JSON_ARRAY) {
            int has_reasoning = 0;
            for (size_t j = 0; j < params->c.count && !has_reasoning; j++) {
                json_t *e = params->c.items[j];
                if (e->type == JSON_STRING && e->str_val && strcmp(e->str_val, "reasoning") == 0)
                    has_reasoning = 1;
            }
            if (!has_reasoning) {
                caps = json_object();
                json_set(caps, "supports_reasoning", json_bool(false));
            } else {
                json_t *reasoning = json_obj_get(item, "reasoning");
                int mandatory = 0;
                json_t *efforts = NULL;
                if (reasoning && reasoning->type == JSON_OBJECT) {
                    json_t *mnd = json_obj_get(reasoning, "mandatory");
                    mandatory = mnd && mnd->type == JSON_BOOL && mnd->bool_val;
                    json_t *raw = json_obj_get(reasoning, "supported_efforts");
                    if (raw && raw->type == JSON_ARRAY) {
                        efforts = json_array();
                        for (size_t j = 0; efforts && j < raw->c.count; j++) {
                            json_t *e2 = raw->c.items[j];
                            if (!e2 || e2->type != JSON_STRING || !e2->str_val) continue;
                            char *s = mg_strip_dup(e2->str_val);
                            if (!s || !*s) { free(s); continue; }
                            for (char *pp = s; *pp; pp++) *pp = (char)tolower((unsigned char)*pp);
                            int dup = 0;
                            for (size_t k = 0; k < efforts->c.count && !dup; k++) {
                                json_t *xx = efforts->c.items[k];
                                if (xx->type == JSON_STRING && xx->str_val && strcmp(xx->str_val, s) == 0)
                                    dup = 1;
                            }
                            if (!dup) json_append(efforts, json_string(s));
                            free(s);
                        }
                    }
                }
                caps = json_object();
                json_set(caps, "supports_reasoning", json_bool(true));
                json_set(caps, "supported_efforts", efforts ? efforts : json_null());
                json_set(caps, "mandatory", json_bool(mandatory ? true : false));
            }
        }
        json_set(map, mid, caps); /* NULL value == unknown: stored as null */
        free(mid);
    }
    json_free(payload);
    if (map->c.count == 0) { json_free(map); return NULL; }
    mg_caps_disk_write(url, map);
    return map;
}

/* Shared fetch path with the 60-second failure backoff. */
static json_t *mg_caps_fetch(mg_caps_state_t *st, const char *url, double timeout, int force) {
    if (st->cache && !force) return st->cache;
    if (st->failed_at >= 0 && (mg_monotonic() - st->failed_at) < MG_CAPS_FAIL_TTL_SECONDS)
        return NULL;
    json_t *map = mg_fetch_caps_catalog(url, timeout);
    if (!map) { st->failed_at = mg_monotonic(); return NULL; }
    if (st->cache) json_free(st->cache);
    st->cache = map;
    st->failed_at = -1;
    return st->cache;
}

/* PoP: models_fetch_openrouter_reasoning_caps @ hermes_cli/models.py:_fetch_openrouter_reasoning_caps */
json_t *models_fetch_openrouter_reasoning_caps(http_fetch_fn fetch, void *ctx, double timeout, int force) {
    (void)fetch; (void)ctx; /* production transport is the live fetcher */
    return mg_caps_fetch(&g_mg_or_caps, MG_OPENROUTER_CATALOG_URL, timeout, force);
}

/* PoP: models_refresh_openrouter_reasoning_caps @ hermes_cli/models.py:_refresh_openrouter_reasoning_caps */
void models_refresh_openrouter_reasoning_caps(void) {
    models_fetch_openrouter_reasoning_caps(NULL, NULL, 6.0, 1);
}

/* PoP: models_openrouter_caps_cached @ hermes_cli/models.py:_openrouter_caps_cached */
json_t *models_openrouter_caps_cached(void) {
    if (!g_mg_or_caps.cache && !g_mg_or_caps.disk_checked) {
        g_mg_or_caps.disk_checked = 1;
        g_mg_or_caps.cache = models_hydrate_reasoning_caps_from_disk(
            MG_OPENROUTER_CATALOG_URL, models_refresh_openrouter_reasoning_caps);
    }
    return g_mg_or_caps.cache;
}

/* PoP: models_warm_openrouter_reasoning_caps_async @ hermes_cli/models.py:warm_openrouter_reasoning_caps_async */
void models_warm_openrouter_reasoning_caps_async(void) {
    if (g_mg_or_caps.warm_started || models_openrouter_caps_cached() != NULL) return;
    g_mg_or_caps.warm_started = 1;
    models_warm_reasoning_caps_async(models_refresh_openrouter_reasoning_caps);
}

/* PoP: models_fetch_nous_reasoning_caps @ hermes_cli/models.py:_fetch_nous_reasoning_caps */
json_t *models_fetch_nous_reasoning_caps(http_fetch_fn fetch, void *ctx, double timeout, int force) {
    (void)fetch; (void)ctx;
    char *url = models_nous_catalog_url();
    if (!url) return NULL;
    json_t *out = mg_caps_fetch(&g_mg_nous_caps, url, timeout, force);
    free(url);
    return out;
}

/* PoP: models_refresh_nous_reasoning_caps @ hermes_cli/models.py:_refresh_nous_reasoning_caps */
void models_refresh_nous_reasoning_caps(void) {
    models_fetch_nous_reasoning_caps(NULL, NULL, 6.0, 1);
}

/* PoP: models_nous_caps_cached @ hermes_cli/models.py:_nous_caps_cached */
json_t *models_nous_caps_cached(void) {
    if (!g_mg_nous_caps.cache && !g_mg_nous_caps.disk_checked) {
        /* One attempt per process: resolving the catalog URL may refresh a
         * token over the network — too expensive for per-turn callers. */
        g_mg_nous_caps.disk_checked = 1;
        char *url = models_nous_catalog_url();
        if (url) {
            g_mg_nous_caps.cache = models_hydrate_reasoning_caps_from_disk(
                url, models_refresh_nous_reasoning_caps);
            free(url);
        }
    }
    return g_mg_nous_caps.cache;
}

/* PoP: models_warm_nous_reasoning_caps_async @ hermes_cli/models.py:warm_nous_reasoning_caps_async */
void models_warm_nous_reasoning_caps_async(void) {
    if (g_mg_nous_caps.warm_started || models_nous_caps_cached() != NULL) return;
    g_mg_nous_caps.warm_started = 1;
    models_warm_reasoning_caps_async(models_refresh_nous_reasoning_caps);
}


/* ════════════════════════════════════════════════════════════════════════
 * Batch 4 — OpenRouter picker ids + ollama config/probing
 * ════════════════════════════════════════════════════════════════════════ */

#define MG_PREFERRED_SILENT_DEFAULT_MODEL "z-ai/glm-5.2"

/* Fallback OpenRouter snapshot used when the live catalog is unavailable
 * (models.py:OPENROUTER_MODELS). */
static const char *const MG_OPENROUTER_MODELS[][2] = {
    {"anthropic/claude-fable-5", ""}, {"anthropic/claude-opus-5", ""},
    {"anthropic/claude-opus-5-fast", "2x price, higher output speed"},
    {"anthropic/claude-opus-4.8", ""}, {"anthropic/claude-opus-4.8-fast", "2x price, higher output speed"},
    {"anthropic/claude-sonnet-5", ""}, {"anthropic/claude-haiku-4.5", ""},
    {"openai/gpt-5.6-sol", ""}, {"openai/gpt-5.6-sol-pro", ""}, {"openai/gpt-5.6-terra", ""},
    {"openai/gpt-5.6-terra-pro", ""}, {"openai/gpt-5.6-luna", ""}, {"openai/gpt-5.6-luna-pro", ""},
    {"openai/gpt-5.5", ""}, {"openai/gpt-5.5-pro", ""}, {"openai/gpt-5.4-mini", ""},
    {"google/gemini-3.1-pro-preview", ""}, {"google/gemini-3.7-flash", ""},
    {"x-ai/grok-4.6", ""}, {"deepseek/deepseek-v4-pro", ""},
    {"deepseek/deepseek-v4-pro-0813", "dated snapshot of v4-pro"},
    {"deepseek/deepseek-v4-flash", ""}, {"deepseek/deepseek-v4-flash-0731", "dated snapshot of v4-flash"},
    {"qwen/qwen3.8-max", ""}, {"moonshotai/kimi-k3", "recommended"}, {"minimax/minimax-m3", ""},
    {"z-ai/glm-5.3", ""}, {"z-ai/glm-5.2", "default"}, {"xiaomi/mimo-v2.5-pro", ""},
    {"tencent/hy3", ""}, {"stepfun/step-3.7-flash", ""},
    {"nvidia/nemotron-3-super-120b-a12b", ""}, {"meta/muse-spark-1.2", ""},
    {"sakana/fugu-ultra", ""},
    {"openrouter/pareto-code", "auto-routes to cheapest coder meeting openrouter.min_coding_score"},
    {"stealth/ox-alpha", "free"}, {"openrouter/elephant-alpha", "free"},
    {"z-ai/glm-5.2:free", "free"}, {"poolside/laguna-s-2.1:free", "free"},
    {"poolside/laguna-xs-2.1:free", "free"},
    {"nvidia/nemotron-3-super-120b-a12b:free", "free"},
    {"nvidia/nemotron-3-ultra-550b-a55b:free", "free"},
    {"nvidia/nemotron-3.5-lightning:free", "free"},
};

/* _openrouter_catalog_cache: list of (id, desc). */
typedef struct mg_or_entry { char *id; char *desc; } mg_or_entry_t;
static mg_or_entry_t *mg_or_catalog = NULL;
static size_t mg_or_catalog_n = 0;

/* _openrouter_model_supports_tools: permissive when supported_parameters is
 * missing/malformed; only hide explicit lists omitting "tools". */
static int mg_openrouter_supports_tools(const json_t *item) {
    json_t *params = item ? json_obj_get(item, "supported_parameters") : NULL;
    if (!params || params->type != JSON_ARRAY) return 1; /* unknown capability -> allow */
    for (size_t i = 0; i < params->c.count; i++) {
        json_t *e = params->c.items[i];
        if (e->type == JSON_STRING && e->str_val && strcmp(e->str_val, "tools") == 0)
            return 1;
    }
    return 0;
}

/* PoP: models_model_ids @ hermes_cli/models.py:model_ids */
char *models_model_ids(int force_refresh) {
    /* fetch_openrouter_models(force_refresh=force_refresh), then keep ids. */
    static int seeded = 0;
    if (!seeded) {
        seeded = 1;
        size_t n = sizeof(MG_OPENROUTER_MODELS)/sizeof(MG_OPENROUTER_MODELS[0]);
        mg_or_catalog = calloc(n, sizeof(mg_or_entry_t));
        if (mg_or_catalog) {
            for (size_t i = 0; i < n; i++) {
                mg_or_catalog[mg_or_catalog_n].id = strdup(MG_OPENROUTER_MODELS[i][0]);
                mg_or_catalog[mg_or_catalog_n].desc = strdup(MG_OPENROUTER_MODELS[i][1]);
                mg_or_catalog_n++;
            }
        }
    }
    if (!force_refresh && mg_or_catalog) {
        /* return cached ids */
        json_t *arr = json_array();
        if (!arr) return NULL;
        for (size_t i = 0; i < mg_or_catalog_n; i++)
            json_append(arr, json_string(mg_or_catalog[i].id));
        char *out = json_serialize(arr);
        json_free(arr);
        return out ? out : strdup("[]");
    }

    const char *silent_default = MG_PREFERRED_SILENT_DEFAULT_MODEL;

    /* Live /v1/models refresh; on failure fall back to the cached list. */
    char headers[128] = "{\"Accept\":\"application/json\"}";
    char *body = NULL; size_t blen = 0;
    int rc = models_live_http_fetch(MG_OPENROUTER_CATALOG_URL, headers, &body, &blen, NULL);
    json_t *payload = NULL;
    if (rc == 0 && body) payload = json_parse(body, NULL);
    free(body);
    if (!payload) {
        json_t *arr = json_array();
        if (!arr) return NULL;
        for (size_t i = 0; i < mg_or_catalog_n; i++)
            json_append(arr, json_string(mg_or_catalog[i].id));
        char *out = json_serialize(arr);
        json_free(arr);
        return out ? out : strdup("[]");
    }

    json_t *live_items = json_obj_get(payload, "data");
    if (!live_items || live_items->type != JSON_ARRAY) { json_free(payload);
        json_t *arr = json_array(); if (!arr) return NULL;
        for (size_t i = 0; i < mg_or_catalog_n; i++) json_append(arr, json_string(mg_or_catalog[i].id));
        char *out = json_serialize(arr); json_free(arr); return out ? out : strdup("[]"); }

    /* Free warm-up for the reasoning-caps cache: same payload, parsed once. */
    json_t *seeded_caps = mg_fetch_caps_catalog(MG_OPENROUTER_CATALOG_URL, 8.0);
    if (seeded_caps) json_free(seeded_caps);

    /* curated list over preferred ids present in the live catalog */
    typedef struct { char *id; char *desc; } entry_t;
    entry_t *curated = calloc(mg_or_catalog_n ? mg_or_catalog_n : 1, sizeof(entry_t));
    size_t n_curated = 0;
    if (!curated) { json_free(payload); return NULL; }

    for (size_t p = 0; p < mg_or_catalog_n; p++) {
        const char *preferred_id = mg_or_catalog[p].id;
        json_t *live_item = NULL;
        for (size_t i = 0; i < live_items->c.count && !live_item; i++) {
            json_t *item = live_items->c.items[i];
            if (!item || item->type != JSON_OBJECT) continue;
            const char *mid = json_get_str(item, "id", "");
            if (*mid && strcmp(mid, preferred_id) == 0) live_item = item;
        }
        if (!live_item) continue;
        if (!mg_openrouter_supports_tools(live_item)) continue;
        char *desc;
        if (strcmp(preferred_id, silent_default) == 0) {
            desc = strdup("default");
        } else {
            json_t *pricing = json_obj_get(live_item, "pricing");
            char *pj = pricing ? json_serialize(pricing) : NULL;
            desc = strdup((pj && openrouter_model_is_free(pj)) ? "free" : "");
            free(pj);
        }
        curated[n_curated].id = strdup(preferred_id);
        curated[n_curated].desc = desc ? desc : strdup("");
        n_curated++;
    }
    json_free(payload);

    if (n_curated == 0) {
        free(curated);
        json_t *arr = json_array();
        if (!arr) return NULL;
        for (size_t i = 0; i < mg_or_catalog_n; i++) json_append(arr, json_string(mg_or_catalog[i].id));
        char *out = json_serialize(arr); json_free(arr);
        return out ? out : strdup("[]");
    }

    /* First entry without a description becomes "recommended". */
    if (curated[0].desc && !*curated[0].desc) {
        free(curated[0].desc);
        curated[0].desc = strdup("recommended");
    }

    /* swap into module cache */
    for (size_t i = 0; i < mg_or_catalog_n; i++) { free(mg_or_catalog[i].id); free(mg_or_catalog[i].desc); }
    for (size_t i = 0; i < n_curated; i++) { mg_or_catalog[i].id = curated[i].id; mg_or_catalog[i].desc = curated[i].desc; }
    mg_or_catalog_n = n_curated;
    free(curated);

    json_t *arr = json_array();
    if (!arr) return NULL;
    for (size_t i = 0; i < mg_or_catalog_n; i++) json_append(arr, json_string(mg_or_catalog[i].id));
    char *out = json_serialize(arr);
    json_free(arr);
    return out ? out : strdup("[]");
}

/* ── ollama base URL / headers ─────────────────────────────────────────── */


/* PoP: models_get_ollama_base_url @ hermes_cli/models.py:_get_ollama_base_url */
char *models_get_ollama_base_url(void) {
    json_t *provider_cfg = models_get_provider_config_dict("ollama");
    const char *configured_raw =
        json_get_str(provider_cfg, "base_url", "");
    if (!*configured_raw) configured_raw = json_get_str(provider_cfg, "api", "");
    if (!*configured_raw) configured_raw = json_get_str(provider_cfg, "url", "");
    char *configured = mg_strip_dup(configured_raw);
    json_free(provider_cfg);
    if (configured && *configured)
        return configured;

    json_t *model_cfg = models_get_model_config_dict_simple();
    const char *mp = json_get_str(model_cfg, "provider", "");
    char model_provider[64] = "";
    snprintf(model_provider, sizeof model_provider, "%.*s",
             (int)(sizeof model_provider - 1),
             (*mp ? mp : ""));
    for (char *q = model_provider; *q; q++) *q = (char)tolower((unsigned char)*q);
    char *model_base = mg_strip_dup(json_get_str(model_cfg, "base_url", ""));
    json_free(model_cfg);

    if (strcmp(model_provider, "ollama") == 0 && model_base && *model_base) {
        free(configured);
        return model_base;
    }
    if (strcmp(model_provider, "custom") == 0 && model_base && *model_base) {
        /* Only reuse the active bare custom endpoint when it is actually
         * Ollama-compatible (probe its /api/tags). */
        if (models_should_use_ollama_native_catalog("custom", model_base, NULL)) {
            free(configured);
            return model_base;
        }
    }

    const char *raw_host = getenv("OLLAMA_HOST");
    char *env_host = mg_strip_dup(raw_host);
    if (env_host && *env_host) {
        if (env_host[0] == ':' && strncmp(env_host, "::", 2) != 0) {
            char *joined = malloc(strlen(env_host) + strlen("127.0.0.1") + 1);
            if (joined) { snprintf(joined, strlen(env_host) + 10, "127.0.0.1%s", env_host); free(env_host); env_host = joined; }
        } else if (env_host[0] == '[' && env_host[strlen(env_host)-1] == ']') {
            size_t n = strlen(env_host);
            char *joined = malloc(n + 16);
            if (joined) { snprintf(joined, n + 16, "%s:11434", env_host); free(env_host); env_host = joined; }
        } else if (strstr(env_host, "://")) {
            /* scheme://[user@]host[:port] — append :11434 when no port given. */
            const char *host_start = strstr(env_host, "://") + 3;
            const char *host_end = host_start + strcspn(host_start, "/?#");
            /* trim userinfo */
            const char *at = memchr(host_start, '@', (size_t)(host_end - host_start));
            if (at) host_start = at + 1;
            size_t hl = (size_t)(host_end - host_start);
            char hostpart[512];
            snprintf(hostpart, sizeof hostpart, "%.*s", (int)hl, host_start);
            int has_port = strchr(hostpart, ':') != NULL ||
                           (hostpart[0] == '[' && strchr(hostpart, ']') != NULL &&
                            strchr(strchr(hostpart, ']'), ':') != NULL);
            if (!has_port) {
                size_t n = strlen(env_host);
                char *joined = malloc(n + 16);
                if (joined) {
                    if (hostpart[0] == '[')
                        snprintf(joined, n + 16, "%.*s:11434%s",
                                 (int)(host_start - env_host) , env_host, host_end);
                    else
                        snprintf(joined, n + 16, "%.*s[%s]:11434%s",
                                 (int)(host_start - env_host), env_host,
                                 hostpart, host_end);
                    free(env_host); env_host = joined;
                }
            }
        } else if (strchr(env_host, ':') && env_host[0] != '[' ) {
            /* bare IPv6 or host:port — leave as-is unless multiple colons */
            int colons = 0;
            for (const char *c = env_host; *c; c++) colons += (*c == ':');
            if (colons > 1) {
                size_t n = strlen(env_host);
                char *joined = malloc(n + 16);
                if (joined) { snprintf(joined, n + 16, "[%s]:11434", env_host); free(env_host); env_host = joined; }
            }
        } else if (!strchr(env_host, ':')) {
            size_t n = strlen(env_host);
            char *joined = malloc(n + 16);
            if (joined) { snprintf(joined, n + 16, "%s:11434", env_host); free(env_host); env_host = joined; }
        }
        free(configured); free(model_base);
        return env_host;
    }
    free(configured); free(model_base); free(env_host);
    return strdup("http://localhost:11434");
}

/* PoP: models_get_ollama_request_headers @ hermes_cli/models.py:_get_ollama_request_headers */
char *models_get_ollama_request_headers(void) {
    json_t *entry = models_get_provider_config_dict("ollama");
    json_t *raw = json_obj_get(entry, "extra_headers");
    json_t *result = config_py_normalize_extra_headers(raw);

    const char *api_key_raw = json_get_str(entry, "api_key", "");
    char *api_key = mg_strip_dup(api_key_raw);
    char keybuf[1024] = "";
    if (api_key && *api_key) {
        snprintf(keybuf, sizeof keybuf, "%s", api_key);
    } else {
        /* no direct key: fall back to key_env / api_key_env environment lookup */
        const char *key_env = json_get_str(entry, "key_env", "");
        if (!*key_env) key_env = json_get_str(entry, "api_key_env", "");
        if (*key_env) {
            char *sv = mg_strip_dup(getenv(key_env));
            snprintf(keybuf, sizeof keybuf, "%s", sv ? sv : "");
            free(sv);
        }
    }
    free(api_key);
    if (*keybuf) {
        int has_auth = 0;
        if (result && result->type == JSON_OBJECT) {
            for (size_t i = 0; i < result->c.count && !has_auth; i++) {
                const char *k = result->c.keys[i];
                if (k && strcasecmp(k, "authorization") == 0) has_auth = 1;
            }
        }
        if (!has_auth) {
            char bearer[1152];
            snprintf(bearer, sizeof bearer, "Bearer %s", keybuf);
            json_set(result, "Authorization", json_string(bearer));
        }
    }
    json_free(entry);
    char *out = json_serialize(result);
    json_free(result);
    return out ? out : strdup("{}");
}

/* PoP: models_get_ollama_native_headers @ hermes_cli/models.py:_get_ollama_native_headers */
char *models_get_ollama_native_headers(const char *base_url, const char *api_key) {
    json_t *entry = models_get_provider_config_dict("ollama");
    const char *cb = json_get_str(entry, "base_url", "");
    if (!*cb) cb = json_get_str(entry, "api", "");
    if (!*cb) cb = json_get_str(entry, "url", "");
    char *configured_base = mg_strip_dup(cb);
    char *explicit_key = mg_strip_dup(api_key);
    int configured_matches = 0;
    if (configured_base && *configured_base && base_url && *base_url)
        configured_matches = models_same_ollama_native_root(base_url, configured_base);
    json_free(entry);
    free(configured_base);

    if (!configured_matches && !(explicit_key && *explicit_key)) {
        free(explicit_key);
        return strdup("{}");
    }
    char *headers = configured_matches ? models_get_ollama_request_headers() : strdup("{}");
    if (explicit_key && *explicit_key) {
        json_t *h = json_parse(headers, NULL);
        if (h && h->type == JSON_OBJECT) {
            /* drop any inherited Authorization variant first */
            for (;;) {
                int removed = 0;
                for (size_t i = 0; i < h->c.count && !removed; i++) {
                    const char *k = h->c.keys[i];
                    if (k && strcasecmp(k, "authorization") == 0) { json_obj_del(h, k); removed = 1; }
                }
                if (!removed) break;
            }
            char bearer[1152];
            snprintf(bearer, sizeof bearer, "Bearer %s", explicit_key);
            json_set(h, "Authorization", json_string(bearer));
        }
        free(headers);
        headers = h ? json_serialize(h) : strdup("{}");
        if (h) json_free(h);
    }
    free(explicit_key);
    return headers ? headers : strdup("{}");
}


/* ── ollama local-probe caches ─────────────────────────────────────────── */

#define MG_OLLAMA_LOCAL_MODELS_CACHE_TTL 300.0   /* 5 minutes */
#define MG_OLLAMA_LOCAL_PROBE_FAILURE_TTL 30
#define MG_OLLAMA_LOCAL_CACHE_MAX_ENTRIES 256

/* Cache selector used by models_remember_ollama_cache (mirrors the distinct
 * Python dicts): 0 = models, 1 = reachable, 2 = failure timestamps. */
typedef struct mg_oc_entry { char *key; void *value; double num; struct mg_oc_entry *next; } mg_oc_entry_t;
static mg_oc_entry_t *mg_oc_models = NULL, *mg_oc_reachable = NULL, *mg_oc_failure = NULL;

static size_t mg_oc_count(mg_oc_entry_t *head) {
    size_t n = 0;
    for (mg_oc_entry_t *e = head; e; e = e->next) n++;
    return n;
}

static void mg_oc_pop(mg_oc_entry_t **head, const char *key) {
    mg_oc_entry_t **pp = head;
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            mg_oc_entry_t *dead = *pp;
            *pp = dead->next;
            free(dead->key);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

static void mg_oc_put(mg_oc_entry_t **head, const char *key, void *value, double num) {
    for (mg_oc_entry_t *e = *head; e; e = e->next)
        if (strcmp(e->key, key) == 0) { e->value = value; e->num = num; return; }
    if (mg_oc_count(*head) >= MG_OLLAMA_LOCAL_CACHE_MAX_ENTRIES && *head) {
        /* evict oldest (list tail = insertion order) */
        mg_oc_entry_t **pp = head;
        while ((*pp)->next) pp = &(*pp)->next;
        mg_oc_entry_t *oldest = *pp;
        char *base_key = NULL;
        char *cut = strstr(oldest->key, "|timeout:");
        if (cut) { *cut = '\0'; base_key = strdup(oldest->key); *cut = '|'; }
        *pp = NULL;
        free(oldest->key); free(oldest);
        if (base_key) {
            mg_oc_pop(&mg_oc_models, base_key);
            mg_oc_pop(&mg_oc_reachable, base_key);
            /* failure keys are "<key>|timeout:<t>" */
            mg_oc_entry_t **pp2 = &mg_oc_failure;
            while (*pp2) {
                if (strcmp((*pp2)->key, base_key) == 0 ||
                    (strncmp((*pp2)->key, base_key, strlen(base_key)) == 0 &&
                     strncmp((*pp2)->key + strlen(base_key), "|timeout:", 9) == 0)) {
                    mg_oc_entry_t *dead = *pp2;
                    *pp2 = dead->next;
                    free(dead->key); free(dead);
                    continue;
                }
                pp2 = &(*pp2)->next;
            }
            free(base_key);
        }
    }
    mg_oc_entry_t *e = calloc(1, sizeof(*e));
    if (!e) return;
    e->key = strdup(key);
    e->value = value;
    e->num = num;
    e->next = *head;
    *head = e;
}

/* PoP: models_evict_related_ollama_cache_entries @ hermes_cli/models.py:_evict_related_ollama_cache_entries */
void models_evict_related_ollama_cache_entries(const char *key) {
    if (!key) return;
    mg_oc_pop(&mg_oc_models, key);
    mg_oc_pop(&mg_oc_reachable, key);
    char suffix[64];
    snprintf(suffix, sizeof suffix, "%s|timeout:", key);
    mg_oc_entry_t **pp = &mg_oc_failure;
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0 || strncmp((*pp)->key, suffix, strlen(suffix)) == 0) {
            mg_oc_entry_t *dead = *pp;
            *pp = dead->next;
            free(dead->key); free(dead);
            continue;
        }
        pp = &(*pp)->next;
    }
}

/* PoP: models_remember_ollama_cache @ hermes_cli/models.py:_remember_ollama_cache */
void models_remember_ollama_cache(int cache_id, const char *key, const char *value_json) {
    if (!key) return;
    mg_oc_entry_t **head =
        cache_id == 0 ? &mg_oc_models : cache_id == 1 ? &mg_oc_reachable : &mg_oc_failure;
    void *value = NULL;
    if (cache_id == 0 && value_json) value = json_parse(value_json, NULL);
    if (mg_oc_count(*head) < MG_OLLAMA_LOCAL_CACHE_MAX_ENTRIES ||
        (head != &mg_oc_models)) {
        /* capacity check + eviction applies to the owning cache */
        for (mg_oc_entry_t *e = *head; e; e = e->next)
            if (strcmp(e->key, key) == 0) { e->num = mg_monotonic(); return; }
        mg_oc_put(head, key, value, mg_monotonic());
        return;
    }
    /* at capacity and key is new: evict the oldest entry's related set */
    mg_oc_entry_t *tail = *head;
    while (tail->next) tail = tail->next;
    char *cut = strstr(tail->key, "|timeout:");
    char *base_key = NULL;
    if (cut) { size_t bl = (size_t)(cut - tail->key); base_key = malloc(bl + 1);
               memcpy(base_key, tail->key, bl); base_key[bl] = '\0'; }
    models_evict_related_ollama_cache_entries(base_key ? base_key : tail->key);
    free(base_key);
    mg_oc_put(head, key, value, mg_monotonic());
}

/* PoP: models_probe_ollama_local_models @ hermes_cli/models.py:probe_ollama_local_models */
char *models_probe_ollama_local_models(const char *base_url, double timeout,
                                       const char *headers_json) {
    char *default_base = NULL;
    if (!base_url || !*base_url) {
        default_base = models_get_ollama_base_url();
        base_url = default_base;
    }
    char *root = models_root_for_ollama_native_api(base_url);
    free(default_base);
    if (!root || !*root) { free(root); return NULL; }

    char *cache_key = models_ollama_probe_cache_key(root, headers_json);
    char failure_key[2048];
    snprintf(failure_key, sizeof failure_key, "%s|timeout:%.3f", cache_key, timeout);

    /* warm cache hit? */
    for (mg_oc_entry_t *e = mg_oc_models; e; e = e->next) {
        if (strcmp(e->key, cache_key) == 0) {
            if (mg_monotonic() - e->num < MG_OLLAMA_LOCAL_MODELS_CACHE_TTL) {
                json_t *arr = e->value;
                char *out = arr ? json_serialize(arr) : NULL;
                free(root); free(cache_key);
                return out;
            }
        }
    }
    /* recent failure? */
    for (mg_oc_entry_t *e = mg_oc_failure; e; e = e->next) {
        if (strcmp(e->key, failure_key) == 0) {
            if (mg_monotonic() - e->num < MG_OLLAMA_LOCAL_PROBE_FAILURE_TTL) {
                free(root); free(cache_key);
                return NULL;
            }
            mg_oc_pop(&mg_oc_failure, failure_key);
        }
    }

    size_t urln = strlen(root) + strlen("/api/tags") + 1;
    char *url = malloc(urln);
    if (!url) { free(root); free(cache_key); return NULL; }
    snprintf(url, urln, "%s/api/tags", root);

    /* request headers: User-Agent plus any caller-supplied headers */
    json_t *req_headers = json_object();
    json_set(req_headers, "User-Agent", json_string(MG_HERMES_USER_AGENT));
    if (headers_json && *headers_json) {
        json_t *extra = json_parse(headers_json, NULL);
        if (extra && extra->type == JSON_OBJECT) {
            for (size_t i = 0; i < extra->c.count; i++)
                json_set(req_headers, extra->c.keys[i], json_copy(extra->c.items[i]));
        }
        if (extra) json_free(extra);
    }
    char *hdrs = json_serialize(req_headers);
    json_free(req_headers);

    char *body = NULL; size_t blen = 0;
    int rc = hdrs ? models_live_http_fetch(url, hdrs, &body, &blen, NULL) : -1;
    free(hdrs); free(url);

    json_t *payload = (rc == 0 && body) ? json_parse(body, NULL) : NULL;
    free(body);
    json_t *raw_models = (payload && payload->type == JSON_OBJECT)
                             ? json_obj_get(payload, "models") : NULL;

    if (!raw_models || raw_models->type != JSON_ARRAY) {
        if (payload) json_free(payload);
        models_remember_ollama_cache(1, cache_key, NULL);           /* reachable=false */
        { char vbuf[64]; snprintf(vbuf, sizeof vbuf, "%.17g", mg_monotonic());
          models_remember_ollama_cache(2, failure_key, vbuf); }     /* failure ts */
        free(root); free(cache_key);
        return NULL;
    }

    json_t *models = json_array();
    int malformed = 0;
    for (size_t i = 0; i < raw_models->c.count && !malformed; i++) {
        json_t *item = raw_models->c.items[i];
        const char *model_id = NULL;
        if (item && item->type == JSON_OBJECT) {
            model_id = json_get_str(item, "model", "");
            if (!*model_id) model_id = json_get_str(item, "name", "");
        } else {
            malformed = 1;
            break;
        }
        if (!model_id || !*model_id) continue;
        int seen = 0;
        for (size_t k = 0; k < models->c.count && !seen; k++) {
            json_t *e2 = models->c.items[k];
            if (e2->type == JSON_STRING && strcmp(e2->str_val, model_id) == 0) seen = 1;
        }
        if (!seen) json_append(models, json_string(model_id));
    }
    if (malformed || (raw_models->c.count > 0 && models->c.count == 0)) {
        if (models) json_free(models);
        if (payload) json_free(payload);
        models_remember_ollama_cache(1, cache_key, NULL);
        { char vbuf[64]; snprintf(vbuf, sizeof vbuf, "%.17g", mg_monotonic());
          models_remember_ollama_cache(2, failure_key, vbuf); }
        free(root); free(cache_key);
        return NULL;
    }
    if (payload) json_free(payload);

    /* success: remember reachable=true, clear failure, cache the list */
    models_remember_ollama_cache(0, cache_key, json_serialize(models));
    mg_oc_pop(&mg_oc_failure, failure_key);
    mg_oc_put(&mg_oc_reachable, cache_key, NULL, 1.0);

    char *out = json_serialize(models);
    json_free(models);
    free(root); free(cache_key);
    return out ? out : strdup("[]");
}


/* PoP: models_fetch_models_with_pricing @ hermes_cli/models.py:fetch_models_with_pricing */
/* Declared (but never defined) in port_models_pure.h; the pricing dispatch
 * above is its only caller. Injectable transport; NULL -> live fetcher. */
char *models_fetch_models_with_pricing(void *fetch, void *ctx,
                                       const char *api_key, const char *base_url,
                                       int force_refresh) {
    http_fetch_fn dofetch = fetch ? (http_fetch_fn)(uintptr_t)fetch : models_live_http_fetch;
    char *cache_key = mg_strip_dup(base_url);
    if (!cache_key || !*cache_key) { free(cache_key); cache_key = strdup("https://openrouter.ai/api"); }
    mg_rstrip_slashes(cache_key);

    if (!force_refresh) {
        json_t *cached = models_cached_catalog(cache_key);
        if (cached) {
            char *out = json_serialize(cached);
            free(cache_key);
            return out ? out : strdup("{}");
        }
    }

    size_t urln = strlen(cache_key) + strlen("/v1/models") + 1;
    char *url = malloc(urln);
    if (!url) { free(cache_key); return strdup("{}"); }
    snprintf(url, urln, "%s/v1/models", cache_key);

    json_t *hdrs = json_object();
    json_set(hdrs, "Accept", json_string("application/json"));
    json_set(hdrs, "User-Agent", json_string(MG_HERMES_USER_AGENT));
    if (api_key && *api_key) {
        char bearer[1152];
        snprintf(bearer, sizeof bearer, "Bearer %s", api_key);
        json_set(hdrs, "Authorization", json_string(bearer));
    }
    char *hj = json_serialize(hdrs);
    json_free(hdrs);

    char *body = NULL; size_t blen = 0;
    int rc = hj ? dofetch(url, hj, &body, &blen, ctx) : -1;
    free(hj); free(url);
    if (rc != 0 || !body) {
        free(body);
        json_t *empty = json_object();
        models_cache_catalog(cache_key, empty);
        char *out = json_serialize(empty);
        free(cache_key);
        return out ? out : strdup("{}");
    }

    json_t *payload = json_parse(body, NULL);
    free(body);
    /* Same document the reasoning-capability fetch would pull — seed it. */
    if (payload) {
        json_t *seed_data = json_obj_get(payload, "data");
        json_t *caps_map = NULL;
        if (seed_data && seed_data->type == JSON_ARRAY) {
            /* reuse the shared seeding path via a temp fetch-free mirror */
            caps_map = json_object();
            for (size_t i = 0; caps_map && i < seed_data->c.count; i++) {
                json_t *item = seed_data->c.items[i];
                if (!item || item->type != JSON_OBJECT) continue;
                const char *mid0 = json_get_str(item, "id", "");
                if (!*mid0) continue;
                json_t *params = json_obj_get(item, "supported_parameters");
                json_t *caps = NULL;
                if (params && params->type == JSON_ARRAY) {
                    int has_reasoning = 0;
                    for (size_t j2 = 0; j2 < params->c.count && !has_reasoning; j2++) {
                        json_t *e = params->c.items[j2];
                        if (e->type == JSON_STRING && e->str_val && strcmp(e->str_val, "reasoning") == 0)
                            has_reasoning = 1;
                    }
                    if (has_reasoning) {
                        caps = json_object();
                        json_set(caps, "supports_reasoning", json_bool(true));
                        json_t *reasoning = json_obj_get(item, "reasoning");
                        json_t *efforts = NULL;
                        if (reasoning && reasoning->type == JSON_OBJECT) {
                            json_t *raw = json_obj_get(reasoning, "supported_efforts");
                            if (raw && raw->type == JSON_ARRAY) {
                                efforts = json_copy(raw);
                            }
                            json_t *mnd = json_obj_get(reasoning, "mandatory");
                            json_set(caps, "mandatory", json_bool(mnd && mnd->type == JSON_BOOL && mnd->bool_val ? true : false));
                        } else {
                            json_set(caps, "mandatory", json_bool(false));
                        }
                        json_set(caps, "supported_efforts", efforts ? efforts : json_null());
                    } else {
                        caps = json_object();
                        json_set(caps, "supports_reasoning", json_bool(false));
                    }
                }
                json_set(caps_map, mid0, caps);
            }
            if (caps_map->c.count > 0) mg_caps_disk_write(url, caps_map);
            json_free(caps_map);
        }
    }

    json_t *result = json_object();
    if (payload) {
        json_t *data = json_obj_get(payload, "data");
        if (data && data->type == JSON_ARRAY) {
            for (size_t i = 0; i < data->c.count; i++) {
                json_t *item = data->c.items[i];
                if (!item || item->type != JSON_OBJECT) continue;
                const char *mid = json_get_str(item, "id", "");
                json_t *pricing = json_obj_get(item, "pricing");
                if (!*mid || !mg_is_obj(pricing)) continue;
                json_t *entry = json_object();
                json_set(entry, "prompt",
                         json_string(json_get_str(pricing, "prompt", "")));
                json_set(entry, "completion",
                         json_string(json_get_str(pricing, "completion", "")));
                const char *icr = json_get_str(pricing, "input_cache_read", "");
                if (*icr) json_set(entry, "input_cache_read", json_string(icr));
                const char *icw = json_get_str(pricing, "input_cache_write", "");
                if (*icw) json_set(entry, "input_cache_write", json_string(icw));
                json_set(result, mid, entry);
            }
        }
        json_free(payload);
    }
    models_cache_catalog(cache_key, result);
    char *out = json_serialize(result);
    free(cache_key);
    return out ? out : strdup("{}");
}

/* PoP: models_same_ollama_native_root @ hermes_cli/models.py:_same_ollama_native_root */
int models_same_ollama_native_root(const char *left, const char *right)
{
    if (!left || !right) return 0;
    /* compare scheme+host+port, ignoring path/query (root normalization) */
    extern char *models_root_for_ollama_native_api(const char *base_url);
    char *rl = models_root_for_ollama_native_api(left);
    char *rr = models_root_for_ollama_native_api(right);
    int same = rl && rr && strcmp(rl, rr) == 0;
    free(rl); free(rr);
    return same;
}

/* PoP: models_should_use_ollama_native_catalog @ hermes_cli/models.py:should_use_ollama_native_catalog */
int models_should_use_ollama_native_catalog(const char *provider,
                                            const char *base_url,
                                            const char *headers_json)
{
    extern char *models_root_for_ollama_native_api(const char *base_url);
    extern char *models_probe_ollama_local_models(const char *base_url, double timeout, const char *headers_json);
    /* requested = str(provider or "").strip().lower() */
    const char *pb = provider ? provider : "";
    while (*pb==' '||*pb=='\t') pb++;
    size_t pn = strlen(pb);
    while (pn>0 && (pb[pn-1]==' '||pb[pn-1]=='\t')) pn--;
    char *requested = malloc(pn+1);
    if (!requested) return 0;
    for (size_t k=0;k<pn;k++) requested[k]=(char)((pb[k]>='A'&&pb[k]<='Z')?pb[k]+32:pb[k]);
    requested[pn]='\0';

    char *root = models_root_for_ollama_native_api(base_url ? base_url : "");
    int result = 0;

    if (root) {
        /* host check: ollama.com / *.ollama.com => False. Extract hostname crudely:
           skip scheme:// then take up to first '/' and strip port. */
        const char *h = strstr(root, "://");
        h = h ? h + 3 : root;
        char host[256]; size_t hi = 0;
        while (h[hi] && h[hi] != '/' && h[hi] != ':' && hi < sizeof(host)-1) { host[hi]=h[hi]; hi++; }
        host[hi]='\0';
        for (char *q=host; *q; q++) if (*q>='A'&&*q<='Z') *q+=32;
        if (strcmp(host,"ollama.com")==0 || (strlen(host)>11 && strcmp(host+strlen(host)-11,".ollama.com")==0)) {
            free(requested); free(root); return 0;
        }
    }

    static const char * const non_local[] = {
        "openrouter","nous","anthropic","openai","openai-codex","gemini","ollama-cloud", NULL
    };
    for (int i=0; non_local[i]; i++)
        if (strcmp(requested, non_local[i])==0) { free(requested); free(root); return 0; }

    /* configured ollama base from provider config dict */
    char *configured_base = NULL;
    {
        extern json_t *models_get_provider_config_dict(const char *provider);
        json_t *cfg = models_get_provider_config_dict("ollama");
        if (cfg) {
            json_t *v;
            if (((v=json_obj_get(cfg,"base_url")) && v->type==JSON_STRING) ||
                ((v=json_obj_get(cfg,"api")) && v->type==JSON_STRING) ||
                ((v=json_obj_get(cfg,"url")) && v->type==JSON_STRING))
                configured_base = strdup(v->str_val ? v->str_val : "");
            else configured_base = strdup("");
            json_free(cfg);
        } else configured_base = strdup("");
    }
    /* trim */
    {
        char *b=configured_base;
        while (*b==' '||*b=='\t') b++;
        size_t bn=strlen(b);
        while (bn>0 && (b[bn-1]==' '||b[bn-1]=='\t')) bn--;
        char *t=strdup(b); /* trimmed copy */ free(configured_base);
        configured_base=t;
        (void)bn;
    }

    if (strcmp(requested,"ollama")==0) {
        if (!root) { free(requested); free(root); free(configured_base); return 0; }
        if (configured_base && *configured_base &&
            !models_same_ollama_native_root(root, configured_base)) {
            char *probe = models_probe_ollama_local_models(root, 0.5, headers_json);
            result = probe != NULL;
            free(probe);
        } else result = 1;
        free(requested); free(root); free(configured_base);
        return result;
    }

    if (configured_base && *configured_base && root &&
        models_same_ollama_native_root(root, configured_base)) {
        free(requested); free(root); free(configured_base);
        return 1;
    }
    if (!root) { free(requested); free(root); free(configured_base); return 0; }

    static const char * const local_like[] = {
        "", "custom", "local", "llamacpp", "llama.cpp", "llama-cpp", "vllm", NULL
    };
    int is_local = 0;
    for (int i=0; local_like[i]; i++)
        if (strcmp(requested, local_like[i])==0) { is_local=1; break; }
    if (!is_local && strncmp(requested, "custom:", 7)!=0) {
        free(requested); free(root); free(configured_base); return 0;
    }
    if (strcmp(requested, "custom:ollama")==0 ||
        (pn>=7 && strcmp(requested+pn-7,"-ollama")==0)) {
        free(requested); free(root); free(configured_base); return 1;
    }
    /* port must be 11434 */
    {
        const char *h2 = strstr(root, "://");
        h2 = h2 ? h2 + 3 : root;
        const char *colon = strchr(h2, ':');
        const char *slash = strchr(h2, '/');
        if (!colon || (slash && colon > slash)) { free(requested); free(root); free(configured_base); return 0; }
        long port = strtol(colon+1, NULL, 10);
        if (port != 11434) { free(requested); free(root); free(configured_base); return 0; }
    }
    {
        char *probe = models_probe_ollama_local_models(root, 0.5, headers_json);
        result = probe != NULL;
        free(probe);
    }
    free(requested); free(root); free(configured_base);
    return result;
}
