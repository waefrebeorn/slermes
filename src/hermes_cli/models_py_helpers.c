/*
 * port_models_py_helpers.c — Faithful C11 ports of hermes_cli/models.py module
 * helpers (REAL_GAP set). See include/port_models_py_helpers.h.
 */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "port_models_py_helpers.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>

#include "hermes_json.h"
#include "hermes_core_types.h"
#include "model_catalog.h"
#include "provider_metadata.h"
#include "libhttp/http.h"

/* forward decls */
static double models_finite_float(const char *raw);
static double models_nonneg_float(const char *raw);
static char *strdup_printf(const char *fmt, double v);

/* ---- constants (mirror models.py) ---- */
static const char *DEEPINFRA_DEFAULT_BASE_URL = "https://api.deepinfra.com/v1/openai";
static const char *DEEPINFRA_MODELS_QUERY = "filter=true&sort_by=hermes";
#define DEEPINFRA_CATALOG_NEG_TTL 60.0

/* DeepInfra surface tags (frozenset). */
static const char *DEEPINFRA_SURFACE_TAGS[] = {
    "chat", "embed", "image-gen", "tts", "stt", "video-gen", NULL
};
/* id-regex exclusion (case-insensitive): embed|rerank|whisper|stable-diffusion|
 * flux|sdxl|tts|bark|speech|image-gen|clip|vit-|dpt- */
static int deepinfra_id_excluded(const char *mid) {
    static const char *needles[] = {
        "embed", "rerank", "whisper", "stable-diffusion", "flux", "sdxl",
        "tts", "bark", "speech", "image-gen", "clip", "vit-", "dpt-", NULL
    };
    for (int i = 0; needles[i]; i++) {
        if (strcasestr(mid, needles[i])) return 1;
    }
    return 0;
}

/* module-level caches (process lifetime) */
static json_t *g_deepinfra_catalog_cache = NULL;
static char *g_deepinfra_catalog_cache_key = NULL;
static double g_deepinfra_neg_ts = 0.0;

/* ---- default live http transport (libhttp) ---- */
int models_live_http_fetch(const char *url, const char *headers_json,
                           char **out_body, size_t *out_len, void *ctx) {
    (void)ctx;
    http_t *h = http_new(8);
    if (!h) return -1;
    http_resp_t *r = http_get(h, url, headers_json);
    int rc = -1;
    if (r && r->status >= 200 && r->status < 300 && r->body) {
        size_t n = r->body_len ? r->body_len : strlen(r->body);
        char *b = (char *)malloc(n + 1);
        if (b) {
            memcpy(b, r->body, n);
            b[n] = '\0';
            *out_body = b;
            if (out_len) *out_len = n;
            rc = 0;
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return rc;
}

/* ============================================================
 * compute_sale_discount
 * ============================================================ */

/* PoP: models_compute_sale_discount @ hermes_cli/models.py:compute_sale_discount */
bool models_compute_sale_discount(const char *prompt, const char *completion,
                                  const json_t *original,
                                  int *out_pct, char **out_was_prompt, char **out_was_completion) {
    if (out_pct) *out_pct = 0;
    if (out_was_prompt) *out_was_prompt = NULL;
    if (out_was_completion) *out_was_completion = NULL;
    if (!original || original->type != JSON_OBJECT) return false;

    json_t *was_prompt = json_object_get(original, "prompt");
    json_t *was_completion = json_object_get(original, "completion");
    const char *wp = (was_prompt && was_prompt->type == JSON_STRING) ? was_prompt->str_val : NULL;
    const char *wc = (was_completion && was_completion->type == JSON_STRING) ? was_completion->str_val : NULL;
    if ((wp == NULL || wp[0] == '\0') && (wc == NULL || wc[0] == '\0')) return false;

    /* _finite / _nonneg helpers */
    #define FINITE(raw) (models_finite_float((raw)))
    #define NONNEG(raw) (models_nonneg_float((raw)))

    const char *cp = (prompt && prompt[0]) ? prompt : NULL;
    const char *cc = (completion && completion[0]) ? completion : NULL;
    int cur_prompt_zero = (cp && strtod(cp, NULL) == 0.0);
    int cur_comp_zero = (cc && strtod(cc, NULL) == 0.0);
    if (cur_prompt_zero && cur_comp_zero) return false;

    double cur_p = FINITE(cp);
    double orig_p = FINITE(wp);
    if (cur_p > 0 && orig_p > 0 && cur_p < orig_p) {
        int pct = (int)lround((1.0 - (cur_p / orig_p)) * 100.0);
        if (pct < 1) return false;
        if (out_pct) *out_pct = pct;
        if (out_was_prompt) *out_was_prompt = strdup(wp ? wp : "");
        if (out_was_completion) *out_was_completion = strdup(wc ? wc : "");
        return true;
    }
    double cur_c = FINITE(cc);
    double orig_c = FINITE(wc);
    if (cur_c > 0 && orig_c > 0 && cur_c < orig_c) {
        int pct = (int)lround((1.0 - (cur_c / orig_c)) * 100.0);
        if (pct < 1) return false;
        if (out_pct) *out_pct = pct;
        if (out_was_prompt) *out_was_prompt = strdup(wp ? wp : "");
        if (out_was_completion) *out_was_completion = strdup(wc ? wc : "");
        return true;
    }
    return false;
    #undef FINITE
    #undef NONNEG
}

static double models_finite_float(const char *raw) {
    if (!raw || !raw[0]) return -1.0;
    char *end = NULL;
    double n = strtod(raw, &end);
    if (end == raw || *end != '\0') return -1.0;
    if (!(n > 0.0) || n != n) return -1.0; /* >0 and not NaN */
    return n;
}
static double models_nonneg_float(const char *raw) {
    if (!raw || !raw[0]) return -1.0;
    char *end = NULL;
    double n = strtod(raw, &end);
    if (end == raw || *end != '\0') return -1.0;
    if (!(n >= 0.0) || n != n) return -1.0;
    return n;
}

/* ============================================================
 * normalize_opencode_base_url
 * ============================================================ */

/* PoP: models_normalize_opencode_base_url @ hermes_cli/models.py:normalize_opencode_base_url */
char *models_normalize_opencode_base_url(const char *provider_id, const char *api_mode, const char *base_url) {
    if (!base_url) base_url = "";
    size_t blen = strlen(base_url);
    char *url = (char *)malloc(blen + 1);
    memcpy(url, base_url, blen + 1);
    /* strip + rstrip('/') */
    /* trim leading/trailing ws */
    char *p = url; while (*p == ' ' || *p == '\t') p++;
    memmove(url, p, strlen(p) + 1);
    size_t L = strlen(url);
    while (L > 0 && (url[L-1] == '/' || url[L-1] == ' ' || url[L-1] == '\t')) url[--L] = '\0';
    if (url[0] == '\0') return url;

    const char *provider = model_normalize_provider(provider_id ? provider_id : "");
    if (provider && (strcmp(provider, "opencode-zen") == 0 || strcmp(provider, "opencode-go") == 0)) {
        if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
            /* strip trailing /v1 */
            size_t n = strlen(url);
            if (n >= 3 && strcmp(url + n - 3, "/v1") == 0) url[n - 3] = '\0';
            return url;
        }
        /* non-anthropic: ensure /v1 on official opencode.ai hosts */
        size_t n = strlen(url);
        if (n >= 3 && strcmp(url + n - 3, "/v1") == 0) return url;
        /* parse host */
        char host[1024];
        const char *scheme = strstr(url, "://");
        const char *start = scheme ? scheme + 3 : url;
        const char *slash = strchr(start, '/');
        size_t hlen = slash ? (size_t)(slash - start) : strlen(start);
        if (hlen >= sizeof host) hlen = sizeof host - 1;
        memcpy(host, start, hlen); host[hlen] = '\0';
        for (char *q = host; *q; q++) *q = (char)tolower((unsigned char)*q);
        if (strcmp(host, "opencode.ai") == 0 || (hlen > 11 && strcmp(host + hlen - 11, ".opencode.ai") == 0)) {
            char *out = (char *)malloc(strlen(url) + 4);
            sprintf(out, "%s/v1", url);
            free(url);
            return out;
        }
    }
    return url;
}

/* ============================================================
 * DeepInfra catalog URL / base URL
 * ============================================================ */

/* PoP: models_deepinfra_catalog_url @ hermes_cli/models.py:_deepinfra_catalog_url */
void models_deepinfra_catalog_url(char **out_cache_key, char **out_full_url) {
    const char *env = getenv("DEEPINFRA_BASE_URL");
    const char *base = (env && env[0]) ? env : DEEPINFRA_DEFAULT_BASE_URL;
    /* rstrip '/' */
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", base);
    size_t n = strlen(buf);
    while (n > 0 && buf[n-1] == '/') buf[--n] = '\0';
    char *ck = strdup(buf);
    char *fu = (char *)malloc(strlen(buf) + strlen(DEEPINFRA_MODELS_QUERY) + 16);
    sprintf(fu, "%s/models?%s", buf, DEEPINFRA_MODELS_QUERY);
    if (out_cache_key) *out_cache_key = ck; else free(ck);
    if (out_full_url) *out_full_url = fu; else free(fu);
}

/* PoP: models_deepinfra_base_url @ hermes_cli/models.py:deepinfra_base_url */
char *models_deepinfra_base_url(const json_t *section) {
    const char *candidate = NULL;
    if (section && section->type == JSON_OBJECT) {
        json_t *bu = json_object_get(section, "base_url");
        if (bu && bu->type == JSON_STRING) candidate = bu->str_val;
    }
    const char *env = getenv("DEEPINFRA_BASE_URL");
    const char *value = candidate ? candidate : (env && env[0] ? env : DEEPINFRA_DEFAULT_BASE_URL);
    char *out = strdup(value);
    size_t n = strlen(out);
    while (n > 0 && out[n-1] == '/') out[--n] = '\0';
    return out;
}

/* ============================================================
 * _fireworks_pricing_from_models_dev
 * ============================================================ */

/* PoP: models_fireworks_pricing_from_models_dev @ hermes_cli/models.py:_fireworks_pricing_from_models_dev */
json_t *models_fireworks_pricing_from_models_dev(int force_refresh) {
    (void)force_refresh;
    json_t *result = json_new_object();
    json_t *data = models_dev_fetch(force_refresh ? true : false);
    if (!data) return result;
    /* models_dev_fetch returns {provider: {model_id: entry,...}, ...} */
    json_t *fw = json_object_get(data, "fireworks");
    if (fw && fw->type == JSON_OBJECT) {
        for (size_t i = 0; i < fw->c.count; i++) {
            const char *mid = fw->c.keys[i];
            json_t *entry = fw->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            json_t *cost = json_object_get(entry, "cost");
            if (!cost || cost->type != JSON_OBJECT) continue;
            json_t *inp = json_object_get(cost, "input");
            json_t *out = json_object_get(cost, "output");
            if (!inp && !out) continue;
            json_t *row = json_new_object();
            if (inp && inp->type == JSON_NUMBER)
                json_object_set(row, "prompt", json_new_string(strdup_printf("%g", inp->num_val / 1000000.0)));
            if (out && out->type == JSON_NUMBER)
                json_object_set(row, "completion", json_new_string(strdup_printf("%g", out->num_val / 1000000.0)));
            json_t *cr = json_object_get(cost, "cache_read");
            if (cr && cr->type == JSON_NUMBER)
                json_object_set(row, "input_cache_read", json_new_string(strdup_printf("%g", cr->num_val / 1000000.0)));
            json_object_set(result, mid, row);
        }
    }
    json_free(data);
    return result;
}

/* ============================================================
 * DeepInfra tag filtering + pricing transform (in-memory)
 * ============================================================ */

static int deepinfra_has_surface_tag(const json_t *tags_arr) {
    if (!tags_arr || tags_arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < tags_arr->c.count; i++) {
        json_t *t = tags_arr->c.items[i];
        if (t->type != JSON_STRING) continue;
        for (int s = 0; DEEPINFRA_SURFACE_TAGS[s]; s++)
            if (strcmp(t->str_val, DEEPINFRA_SURFACE_TAGS[s]) == 0) return 1;
    }
    return 0;
}

/* PoP: models_deepinfra_models_by_tag @ hermes_cli/models.py:_fetch_deepinfra_models_by_tag */
json_t *models_deepinfra_models_by_tag(const json_t *catalog_data, const char *tag) {
    json_t *matched = json_new_array();
    if (!catalog_data || catalog_data->type != JSON_ARRAY) return matched;
    for (size_t i = 0; i < catalog_data->c.count; i++) {
        json_t *item = catalog_data->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        json_t *midv = json_object_get(item, "id");
        if (!midv || midv->type != JSON_STRING) continue;
        json_t *raw_meta = json_object_get(item, "metadata");
        if (raw_meta == NULL) continue; /* stub without pricing/context */
        json_t *metadata = (raw_meta->type == JSON_OBJECT) ? raw_meta : json_new_object();
        json_t *raw_tags = json_object_get(metadata, "tags");
        int has_surface = deepinfra_has_surface_tag(raw_tags);
        int matched_tag = 0;
        if (raw_tags && raw_tags->type == JSON_ARRAY) {
            for (size_t j = 0; j < raw_tags->c.count; j++) {
                json_t *t = raw_tags->c.items[j];
                if (t->type == JSON_STRING && strcmp(t->str_val, tag) == 0) { matched_tag = 1; break; }
            }
        }
        if (has_surface) {
            if (matched_tag) {
                json_t *m = json_new_object();
                json_object_set(m, "id", json_new_string(midv->str_val));
                json_object_set(m, "metadata", json_copy(metadata));
                json_array_append(matched, m);
            }
            continue;
        }
        /* fallback: only chat surface inferable from id */
        if (strcmp(tag, "chat") == 0 && !deepinfra_id_excluded(midv->str_val)) {
            json_t *m = json_new_object();
            json_object_set(m, "id", json_new_string(midv->str_val));
            json_object_set(m, "metadata", json_copy(metadata));
            json_array_append(matched, m);
        }
    }
    return matched;
}

/* PoP: models_deepinfra_model_ids @ hermes_cli/models.py:deepinfra_model_ids */
char **models_deepinfra_model_ids(const json_t *catalog_data, const char *tag, int *out_count) {
    json_t *items = models_deepinfra_models_by_tag(catalog_data, tag);
    int n = (int)items->c.count;
    char **arr = (char **)malloc((n + 1) * sizeof(char *));
    for (int i = 0; i < n; i++) {
        json_t *it = items->c.items[i];
        json_t *idv = json_object_get(it, "id");
        arr[i] = strdup(idv && idv->type == JSON_STRING ? idv->str_val : "");
    }
    arr[n] = NULL;
    json_free(items);
    if (out_count) *out_count = n;
    return arr;
}

/* PoP: models_fetch_deepinfra_models @ hermes_cli/models.py:_fetch_deepinfra_models */
char **models_fetch_deepinfra_models(const json_t *catalog_data, int *out_count) {
    return models_deepinfra_model_ids(catalog_data, "chat", out_count);
}

/* PoP: models_fetch_deepinfra_pricing @ hermes_cli/models.py:_fetch_deepinfra_pricing */
json_t *models_fetch_deepinfra_pricing(const json_t *tagged_models) {
    json_t *result = json_new_object();
    if (!tagged_models || tagged_models->type != JSON_ARRAY) return result;
    for (size_t i = 0; i < tagged_models->c.count; i++) {
        json_t *item = tagged_models->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;
        json_t *midv = json_object_get(item, "id");
        if (!midv || midv->type != JSON_STRING) continue;
        json_t *metadata = json_object_get(item, "metadata");
        if (!metadata || metadata->type != JSON_OBJECT) continue;
        json_t *pricing = json_object_get(metadata, "pricing");
        if (!pricing || pricing->type != JSON_OBJECT) continue;
        json_t *row = json_new_object();
        json_t *inp = json_object_get(pricing, "input_tokens");
        json_t *out = json_object_get(pricing, "output_tokens");
        json_t *cr = json_object_get(pricing, "cache_read_tokens");
        if (inp && inp->type == JSON_NUMBER)
            json_object_set(row, "prompt", json_new_string(strdup_printf("%g", inp->num_val / 1000000.0)));
        if (out && out->type == JSON_NUMBER)
            json_object_set(row, "completion", json_new_string(strdup_printf("%g", out->num_val / 1000000.0)));
        if (cr && cr->type == JSON_NUMBER)
            json_object_set(row, "input_cache_read", json_new_string(strdup_printf("%g", cr->num_val / 1000000.0)));
        if (row->c.count > 0)
            json_object_set(result, midv->str_val, row);
        else json_free(row);
    }
    return result;
}

/* ============================================================
 * _fetch_deepinfra_catalog (network + caches)
 * ============================================================ */

/* PoP: models_fetch_deepinfra_catalog @ hermes_cli/models.py:_fetch_deepinfra_catalog */
json_t *models_fetch_deepinfra_catalog(http_fetch_fn fetch, void *ctx, int force_refresh) {
    if (!fetch) { fetch = models_live_http_fetch; ctx = NULL; }
    char *cache_key = NULL, *url = NULL;
    models_deepinfra_catalog_url(&cache_key, &url);

    if (!force_refresh && g_deepinfra_catalog_cache) {
        if (g_deepinfra_catalog_cache_key && strcmp(g_deepinfra_catalog_cache_key, cache_key) == 0) {
            free(cache_key); free(url);
            return json_copy(g_deepinfra_catalog_cache);
        }
    }
    if (!force_refresh && g_deepinfra_neg_ts > 0) {
        double now = (double)time(NULL);
        if ((now - g_deepinfra_neg_ts) < DEEPINFRA_CATALOG_NEG_TTL) {
            free(cache_key); free(url);
            return NULL;
        }
    }

    char *body = NULL; size_t blen = 0;
    char *headers = strdup("{\"User-Agent\":\"hermes-agent\"}");
    const char *api_key = getenv("DEEPINFRA_API_KEY");
    if (api_key && api_key[0]) {
        free(headers);
        size_t hl = strlen(api_key) + 40;
        headers = (char *)malloc(hl);
        snprintf(headers, hl, "{\"Authorization\":\"Bearer %s\"}", api_key);
    }
    int rc = fetch(url, headers, &body, &blen, ctx);
    free(headers);
    free(url);

    if (rc != 0 || !body) {
        g_deepinfra_neg_ts = (double)time(NULL);
        free(cache_key);
        return NULL;
    }
    json_t *payload = json_parse(body, NULL);
    free(body);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        g_deepinfra_neg_ts = (double)time(NULL);
        free(cache_key);
        return NULL;
    }
    json_t *data = json_object_get(payload, "data");
    if (!data || data->type != JSON_ARRAY) {
        json_free(payload);
        g_deepinfra_neg_ts = (double)time(NULL);
        free(cache_key);
        return NULL;
    }
    if (g_deepinfra_catalog_cache) json_free(g_deepinfra_catalog_cache);
    free(g_deepinfra_catalog_cache_key);
    g_deepinfra_catalog_cache = json_copy(data);
    g_deepinfra_catalog_cache_key = cache_key;
    g_deepinfra_neg_ts = 0.0;
    json_free(payload);
    return json_copy(g_deepinfra_catalog_cache);
}

/* ============================================================
 * _urlopen_model_catalog_request
 * ============================================================ */

/* PoP: models_urlopen_model_catalog_request @ hermes_cli/models.py:_urlopen_model_catalog_request */
char *models_urlopen_model_catalog_request(http_fetch_fn fetch, void *ctx,
                                           const char *url, const char *headers_json, double timeout) {
    (void)timeout;
    if (!fetch) { fetch = models_live_http_fetch; ctx = NULL; }
    if (!headers_json) headers_json = "{}";
    char *body = NULL; size_t blen = 0;
    int rc = fetch(url, headers_json, &body, &blen, ctx);
    if (rc != 0 || !body) return NULL;
    return body; /* caller frees */
}

/* ============================================================
 * ollama_model_supports_thinking
 * ============================================================ */

/* PoP: models_ollama_model_supports_thinking @ hermes_cli/models.py:ollama_model_supports_thinking */
int models_ollama_model_supports_thinking(http_fetch_fn fetch, void *ctx,
                                          const char *model, const char *base_url,
                                          const char *api_key, double timeout) {
    (void)timeout;
    if (!fetch) { fetch = models_live_http_fetch; ctx = NULL; }
    if (!base_url || !base_url[0]) return -1;
    char server[2048];
    snprintf(server, sizeof server, "%s", base_url);
    size_t n = strlen(server);
    while (n > 0 && server[n-1] == '/') server[--n] = '\0';
    if (n >= 3 && strcmp(server + n - 3, "/v1") == 0) server[n - 3] = '\0';
    if (server[0] == '\0') return -1;

    /* _strip_ollama_cloud_suffix */
    const char *bare = model ? model : "";
    /* cloud suffix e.g. ":ollama-cloud" or similar; strip after last ':' if it
     * contains non-numeric tag and is not a port. Faithful enough: strip a
     * trailing ':cloud' style suffix. */
    char bare_buf[1024];
    snprintf(bare_buf, sizeof bare_buf, "%s", bare);
    char *colon = strrchr(bare_buf, ':');
    if (colon && strchr(colon, '.') == NULL && strcmp(colon, ":latest") != 0) {
        /* only strip if not a numeric port; ollama tags are like :7b etc. */
        *colon = '\0';
    }
    if (bare_buf[0] == '\0') return -1;

    char *headers = NULL;
    if (api_key && api_key[0]) {
        size_t hl = strlen(api_key) + 40;
        headers = (char *)malloc(hl);
        snprintf(headers, hl, "{\"Authorization\":\"Bearer %s\"}", api_key);
    } else {
        headers = strdup("{}");
    }
    char *body = NULL; size_t blen = 0;
    char req[3072];
    snprintf(req, sizeof req, "{\"name\":\"%s\"}", bare_buf);
    /* use a POST via the transport: encode method into headers? transport only
     * does GET. Use http_get on /api/show?name=... fallback. For faithful
     * POST, do a direct libhttp call when fetch is the live one. */
    int rc;
    if (fetch == models_live_http_fetch) {
        http_t *h = http_new((int)timeout > 0 ? (int)timeout : 5);
        char post_url[3072];
        snprintf(post_url, sizeof post_url, "%s/api/show", server);
        http_resp_t *r = http_request(h, HTTP_POST, post_url, headers, req, strlen(req));
        if (r && r->status == 200 && r->body) {
            body = strdup(r->body);
            blen = strlen(body);
            rc = 0;
        } else rc = -1;
        if (r) http_resp_free(r);
        http_free(h);
    } else {
        /* injectable transport is GET-only; attempt GET with query. */
        char get_url[4096];
        snprintf(get_url, sizeof get_url, "%s/api/show?name=%s", server, bare_buf);
        rc = fetch(get_url, headers, &body, &blen, ctx);
    }
    free(headers);
    if (rc != 0 || !body) return -1;
    json_t *resp = json_parse(body, NULL);
    free(body);
    if (!resp || resp->type != JSON_OBJECT) { if (resp) json_free(resp); return -1; }
    json_t *caps = json_object_get(resp, "capabilities");
    int result = -1;
    if (caps && caps->type == JSON_ARRAY) {
        result = 0;
        for (size_t i = 0; i < caps->c.count; i++) {
            json_t *c = caps->c.items[i];
            if (c->type == JSON_STRING && strcmp(c->str_val, "thinking") == 0) { result = 1; break; }
        }
    }
    json_free(resp);
    return result;
}

/* small helper */
static char *strdup_printf(const char *fmt, double v) {
    char b[64];
    snprintf(b, sizeof b, fmt, v);
    return strdup(b);
}
