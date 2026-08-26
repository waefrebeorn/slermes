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

/* Exported from src/hermes_cli/moa_pure_helpers.c
 * (PoP: silent_default_for_provider @ hermes_cli/models.py:get_preferred_silent_default_model) */
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

/* PoP: models_xai_finalize_catalog @ hermes_cli/models.py:_xai_finalize_catalog */
char *models_xai_finalize_catalog(const char *ids_json) {
    json_t *arr = ids_json ? json_parse(ids_json, NULL) : NULL;
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); arr = json_array(); }
    if (!arr) return NULL;

    /* _xai_merge_curated_extras: append missing extras, slotting immediately
     * after the pinned headline model when it leads the list. */
    for (size_t e = 0; e < sizeof(MG_XAI_CURATED_EXTRAS)/sizeof(MG_XAI_CURATED_EXTRAS[0]); e++) {
        const char *extra = MG_XAI_CURATED_EXTRAS[e];
        int present = 0;
        for (size_t i = 0; i < arr->c.count && !present; i++) {
            json_t *it = arr->c.items[i];
            if (it->type == JSON_STRING && it->str_val && strcmp(it->str_val, extra) == 0)
                present = 1;
        }
        if (present) continue;
        size_t insert_at = arr->c.count;
        if (arr->c.count > 0) {
            json_t *first = arr->c.items[0];
            if (first->type == JSON_STRING && first->str_val &&
                strcmp(first->str_val, MG_XAI_TOP_MODEL) == 0)
                insert_at = 1;
        }
        /* insert at insert_at by rebuilding tail */
        json_t *tmp = json_array();
        if (!tmp) { json_free(arr); return NULL; }
        for (size_t i = 0; i < arr->c.count; i++) {
            if (i == insert_at) json_append(tmp, json_string(extra));
            json_append(tmp, json_copy(arr->c.items[i]));
        }
        if (insert_at >= arr->c.count) json_append(tmp, json_string(extra));
        json_free(arr);
        arr = tmp;
        if (!arr) return NULL;
    }

    /* _xai_promote_top: pin the headline model to the front. */
    int has_top = 0;
    for (size_t i = 0; i < arr->c.count && !has_top; i++) {
        json_t *it = arr->c.items[i];
        if (it->type == JSON_STRING && it->str_val && strcmp(it->str_val, MG_XAI_TOP_MODEL) == 0)
            has_top = 1;
    }
    if (has_top) {
        json_t *tmp = json_array();
        if (!tmp) { json_free(arr); return NULL; }
        json_append(tmp, json_string(MG_XAI_TOP_MODEL));
        for (size_t i = 0; i < arr->c.count; i++) {
            json_t *it = arr->c.items[i];
            if (it->type == JSON_STRING && it->str_val &&
                strcmp(it->str_val, MG_XAI_TOP_MODEL) == 0)
                continue;
            json_append(tmp, json_copy(it));
        }
        json_free(arr);
        arr = tmp;
    }
    char *out = json_serialize(arr);
    json_free(arr);
    return out;
}

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
