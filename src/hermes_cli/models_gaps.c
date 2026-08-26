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
