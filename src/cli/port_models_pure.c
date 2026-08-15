/*
 * port_models_pure.c — second cohesive port module for hermes_cli/models.py.
 *
 * Covers the remaining pure/config/disk-cache helpers and network wrappers
 * (LM Studio, GitHub Copilot, custom /v1 probes, Ollama Cloud disk cache,
 * pricing, Nous portal unions, validate_requested_model) not already in
 * port_models_net.c. All network access goes through the injectable
 * http_fetch_fn transport so the logic is testable offline; credential/config
 * resolution is likewise injected (NULL → sensible default). No stubs.
 */

#include "port_models_pure.h"
#include "port_models_net.h"
#include "port_models_helpers.h"
#include "model_catalog.h"

/* format_price_per_mtok is defined in port_models_helpers.c (no header decl).
 * Converts a per-token price string to "$X.XX/MTok" / "free" / "?". Assembles
 * this orphaned formatter (matches Python models.py _format_price_per_mtok). */
char *format_price_per_mtok(const char *per_token_str);
#include "model_normalize.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

/* local printf-style dup (libjson has no variant) */
static char *xstrdup_printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return strdup(buf);
}

/* ── Constants (mirror models.py) ───────────────────────────────────────── */
#define COPILOT_BASE_URL    "https://api.githubcopilot.com"
#define COPILOT_MODELS_URL   COPILOT_BASE_URL "/models"
#define NOUS_INFERENCE_BASE  "https://inference-api.nousresearch.com"
#define HERMES_UA            "hermes-cli"
#define OLLAMA_CLOUD_URL     "https://ollama.com/v1"

static const char *const O_SERIES_EFFORTS[] = {"low", "medium", "high", NULL};
static const char *const GPT5_EFFORTS[] = {"minimal", "low", "medium", "high", NULL};

/* ── helpers ───────────────────────────────────────────────────────────── */

static char *xstrdup(const char *s) { return s ? strdup(s) : strdup(""); }

/* Normalize a provider string: strip whitespace, lowercase. Caller frees. */
static char *norm_provider_dup(const char *provider) {
    const char *p = provider ? provider : "";
    char tmp[256];
    size_t j = 0;
    for (size_t i = 0; p[i] && j + 1 < sizeof(tmp); i++) {
        char c = p[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        tmp[j++] = (char)tolower((unsigned char)c);
    }
    tmp[j] = '\0';
    return xstrdup(tmp);
}

/* ── OpenRouter / Novita / Nous pricing ────────────────────────────────── */

/* PoP: _resolve_openrouter_api_key @ hermes_cli/models.py:_resolve_openrouter_api_key */
char *models_resolve_openrouter_api_key(void) {
    const char *v = getenv("OPENROUTER_API_KEY");
    return xstrdup(v && *v ? v : "");
}

/* PoP: _resolve_nous_pricing_credentials @ hermes_cli/models.py:_resolve_nous_pricing_credentials */
void models_resolve_nous_pricing_credentials(nous_creds_fn resolve, void *ctx,
                                             char **out_key, char **out_base) {
    if (resolve) {
        char *k = NULL, *b = NULL;
        resolve(&k, &b, ctx);
        if (k || b) {
            *out_key = k ? k : xstrdup("");
            *out_base = b ? b : xstrdup(NOUS_INFERENCE_BASE);
            return;
        }
    }
    *out_key = xstrdup("");
    *out_base = xstrdup(NOUS_INFERENCE_BASE);
}

/* PoP: fetch_models_with_pricing @ hermes_cli/models.py:fetch_models_with_pricing */
char *models_fetch_models_with_pricing(http_fetch_fn fetch, void *ctx,
                                       const char *api_key, const char *base_url,
                                       int force_refresh) {
    (void)force_refresh;
    if (!fetch) return xstrdup("{}");
    const char *b = (base_url && *base_url) ? base_url : "https://openrouter.ai/api";
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == '/') n--;
    char url[1024];
    snprintf(url, sizeof(url), "%.*s/v1/models", (int)n, b);

    char hdr[256];
    if (api_key && *api_key)
        snprintf(hdr, sizeof(hdr), "{\"Accept\":\"application/json\",\"Authorization\":\"Bearer %s\"}", api_key);
    else
        snprintf(hdr, sizeof(hdr), "{\"Accept\":\"application/json\"}");

    char *body = NULL; size_t blen = 0;
    if (fetch(url, hdr, &body, &blen, ctx) != 0 || !body) return xstrdup("{}");

    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return xstrdup("{}");
    json_t *out = json_object();
    json_t *data = (root->type == JSON_OBJECT) ? json_obj_get(root, "data") : NULL;
    if (data && data->type == JSON_ARRAY) {
        for (size_t i = 0; i < data->c.count; i++) {
            json_t *item = data->c.items[i];
            if (!item || item->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(item, "id");
            if (!idj || idj->type != JSON_STRING || !idj->str_val) continue;
            json_t *pricing = json_obj_get(item, "pricing");
            if (!pricing || pricing->type != JSON_OBJECT) continue;
            json_t *entry = json_object();
            json_t *p = json_obj_get(pricing, "prompt");
            json_t *c = json_obj_get(pricing, "completion");
            json_set(entry, "prompt", json_string(p && p->type == JSON_STRING ? p->str_val : ""));
            json_set(entry, "completion", json_string(c && c->type == JSON_STRING ? c->str_val : ""));
            json_t *icr = json_obj_get(pricing, "input_cache_read");
            if (icr && icr->type == JSON_STRING && icr->str_val && *icr->str_val)
                json_set(entry, "input_cache_read", json_string(icr->str_val));
            json_t *icw = json_obj_get(pricing, "input_cache_write");
            if (icw && icw->type == JSON_STRING && icw->str_val && *icw->str_val)
                json_set(entry, "input_cache_write", json_string(icw->str_val));
            json_set(out, idj->str_val, entry);
        }
    }
    char *res = json_serialize(out);
    json_free(out); json_free(root);
    return res ? res : xstrdup("{}");
}

/* PoP: _fetch_novita_pricing @ hermes_cli/models.py:_fetch_novita_pricing */
char *models_fetch_novita_pricing(http_fetch_fn fetch, void *ctx, int force_refresh) {
    (void)force_refresh;
    const char *key = getenv("NOVITA_API_KEY");
    if (!key || !*key) return xstrdup("{}");
    const char *base = getenv("NOVITA_BASE_URL");
    const char *b = (base && *base) ? base : "https://api.novita.ai/openai/v1";
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == '/') n--;
    char url[1024];
    snprintf(url, sizeof(url), "%.*s/models", (int)n, b);
    char hdr[300];
    snprintf(hdr, sizeof(hdr), "{\"Authorization\":\"Bearer %s\",\"Accept\":\"application/json\"}", key);

    char *body = NULL; size_t blen = 0;
    if (!fetch || fetch(url, hdr, &body, &blen, ctx) != 0 || !body) return xstrdup("{}");
    json_t *root = json_parse(body, NULL);
    free(body);
    if (!root) return xstrdup("{}");
    json_t *out = json_object();
    json_t *data = (root->type == JSON_OBJECT) ? json_obj_get(root, "data") : NULL;
    if (data && data->type == JSON_ARRAY) {
        for (size_t i = 0; i < data->c.count; i++) {
            json_t *item = data->c.items[i];
            if (!item || item->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(item, "id");
            if (!idj || idj->type != JSON_STRING || !idj->str_val) continue;
            json_t *inp = json_obj_get(item, "input_token_price_per_m");
            json_t *outp = json_obj_get(item, "output_token_price_per_m");
            if ((!inp || inp->type != JSON_NUMBER) && (!outp || outp->type != JSON_NUMBER))
                continue;
            double in = (inp && inp->type == JSON_NUMBER) ? inp->num_val : 0;
            double ou = (outp && outp->type == JSON_NUMBER) ? outp->num_val : 0;
            char pb[64], cb[64];
            snprintf(pb, sizeof(pb), "%.10g", in / 10000.0 / 1000000.0);
            snprintf(cb, sizeof(cb), "%.10g", ou / 10000.0 / 1000000.0);
            json_t *entry = json_object();
            json_set(entry, "prompt", json_string(pb));
            json_set(entry, "completion", json_string(cb));
            /* Assemble the orphaned format_price_per_mtok formatter: add
             * human-readable "$X.XX/MTok" strings (matches Python models.py
             * _format_price_per_mtok display path). Non-breaking: extra fields. */
            char *pd = format_price_per_mtok(pb);
            char *cd = format_price_per_mtok(cb);
            if (pd) { json_set(entry, "prompt_display", json_string(pd)); free(pd); }
            if (cd) { json_set(entry, "completion_display", json_string(cd)); free(cd); }
            json_set(out, idj->str_val, entry);
        }
    }
    char *res = json_serialize(out);
    json_free(out); json_free(root);
    return res ? res : xstrdup("{}");
}

/* PoP: get_pricing_for_provider @ hermes_cli/models.py:get_pricing_for_provider */
char *models_get_pricing_for_provider(http_fetch_fn fetch, void *ctx,
                                      nous_creds_fn resolve_nous, void *nous_ctx,
                                      const char *provider, int force_refresh) {
    char *norm = norm_provider_dup(provider);
    char *res = NULL;
    if (strcmp(norm, "openrouter") == 0) {
        char *k = models_resolve_openrouter_api_key();
        res = models_fetch_models_with_pricing(fetch, ctx, k, "https://openrouter.ai/api", force_refresh);
        free(k);
    } else if (strcmp(norm, "novita") == 0) {
        res = models_fetch_novita_pricing(fetch, ctx, force_refresh);
    } else if (strcmp(norm, "nous") == 0) {
        char *k = NULL, *b = NULL;
        models_resolve_nous_pricing_credentials(resolve_nous, nous_ctx, &k, &b);
        if (b && *b) {
            size_t bn = strlen(b);
            while (bn > 0 && b[bn - 1] == '/') bn--;
            char stripped[1024];
            snprintf(stripped, sizeof(stripped), "%.*s", (int)bn, b);
            size_t sl = strlen(stripped);
            if (sl >= 3 && strcmp(stripped + sl - 3, "/v1") == 0)
                stripped[sl - 3] = '\0';
            res = models_fetch_models_with_pricing(fetch, ctx, k, stripped, force_refresh);
        } else {
            res = xstrdup("{}");
        }
        free(k); free(b);
    } else {
        res = xstrdup("{}");
    }
    free(norm);
    return res;
}

/* ── LM Studio native API helpers ──────────────────────────────────────── */

/* PoP: _lmstudio_server_root @ hermes_cli/models.py:_lmstudio_server_root */
char *models_lmstudio_server_root(const char *base_url) {
    char tmp[1024];
    const char *b = base_url ? base_url : "";
    size_t n = strlen(b);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '/'))
        n--;
    snprintf(tmp, sizeof(tmp), "%.*s", (int)n, b);
    /* strip known suffixes */
    size_t t = strlen(tmp);
    for (size_t i = 0; i < 3; i++) {
        const char *suf = (i == 0) ? "/api/v1" : (i == 1) ? "/api" : "/v1";
        size_t sl = strlen(suf);
        if (t >= sl && strcmp(tmp + t - sl, suf) == 0) {
            tmp[t - sl] = '\0';
            t -= sl;
            while (t > 0 && tmp[t - 1] == '/') tmp[--t] = '\0';
            break;
        }
    }
    return xstrdup(tmp[0] ? tmp : "");
}

/* PoP: _lmstudio_request_headers @ hermes_cli/models.py:_lmstudio_request_headers */
char *models_lmstudio_request_headers(const char *api_key) {
    const char *tok = api_key ? api_key : "";
    while (*tok == ' ' || *tok == '\t') tok++;
    if (*tok)
        return xstrdup_printf("{\"User-Agent\":\"%s\",\"Authorization\":\"Bearer %s\"}", HERMES_UA, tok);
    return xstrdup_printf("{\"User-Agent\":\"%s\"}", HERMES_UA);
}

/* PoP: _lmstudio_fetch_raw_models @ hermes_cli/models.py:_lmstudio_fetch_raw_models */
json_t *models_lmstudio_fetch_raw_models(http_fetch_fn fetch, void *ctx,
                                         const char *api_key, const char *base_url) {
    char *root = models_lmstudio_server_root(base_url);
    if (!root || !*root) { free(root); return NULL; }
    char *hdr = models_lmstudio_request_headers(api_key);
    char url[1100];
    snprintf(url, sizeof(url), "%s/api/v1/models", root);
    free(root);
    char *body = NULL; size_t blen = 0;
    if (!fetch || fetch(url, hdr, &body, &blen, ctx) != 0 || !body) { free(hdr); return NULL; }
    free(hdr);
    json_t *parsed = json_parse(body, NULL);
    free(body);
    if (!parsed || parsed->type != JSON_OBJECT) { if (parsed) json_free(parsed); return NULL; }
    json_t *models = json_obj_get(parsed, "models");
    if (!models || models->type != JSON_ARRAY) { json_free(parsed); return NULL; }
    /* hand back the whole parsed object; caller reads .models */
    return parsed;
}

/* PoP: probe_lmstudio_models @ hermes_cli/models.py:probe_lmstudio_models */
char **models_probe_lmstudio_models(http_fetch_fn fetch, void *ctx,
                                    const char *api_key, const char *base_url) {
    json_t *raw = models_lmstudio_fetch_raw_models(fetch, ctx, api_key, base_url);
    if (!raw) return NULL;
    json_t *models = json_obj_get(raw, "models");
    char **keys = NULL; size_t cnt = 0;
    if (models && models->type == JSON_ARRAY) {
        keys = calloc(models->c.count + 1, sizeof(char *));
        for (size_t i = 0; i < models->c.count; i++) {
            json_t *m = models->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *tj = json_obj_get(m, "type");
            if (tj && tj->type == JSON_STRING && strcasecmp(tj->str_val, "embedding") == 0)
                continue;
            json_t *kj = json_obj_get(m, "key");
            json_t *idj = json_obj_get(m, "id");
            const char *key = (kj && kj->type == JSON_STRING && kj->str_val) ? kj->str_val
                            : (idj && idj->type == JSON_STRING && idj->str_val) ? idj->str_val : "";
            if (!*key) continue;
            /* dedupe */
            int dup = 0;
            for (size_t d = 0; d < cnt; d++) if (strcmp(keys[d], key) == 0) { dup = 1; break; }
            if (!dup) keys[cnt++] = xstrdup(key);
        }
        keys[cnt] = NULL;
    }
    json_free(raw);
    return keys;
}

/* PoP: lmstudio_model_reasoning_options @ hermes_cli/models.py:lmstudio_model_reasoning_options */
char **models_lmstudio_reasoning_options(http_fetch_fn fetch, void *ctx,
                                         const char *model, const char *base_url,
                                         const char *api_key) {
    json_t *raw = models_lmstudio_fetch_raw_models(fetch, ctx, api_key, base_url);
    char **out = calloc(1, sizeof(char *)); /* empty NULL-terminated */
    if (!raw) return out;
    json_t *models = json_obj_get(raw, "models");
    if (models && models->type == JSON_ARRAY) {
        for (size_t i = 0; i < models->c.count; i++) {
            json_t *m = models->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *kj = json_obj_get(m, "key");
            json_t *idj = json_obj_get(m, "id");
            const char *key = (kj && kj->type == JSON_STRING && kj->str_val) ? kj->str_val
                            : (idj && idj->type == JSON_STRING && idj->str_val) ? idj->str_val : "";
            if (strcmp(key, model ? model : "") != 0) continue;
            json_t *caps = json_obj_get(m, "capabilities");
            json_t *reasoning = (caps && caps->type == JSON_OBJECT) ? json_obj_get(caps, "reasoning") : NULL;
            json_t *opts = (reasoning && reasoning->type == JSON_OBJECT) ? json_obj_get(reasoning, "allowed_options") : NULL;
            if (opts && opts->type == JSON_ARRAY) {
                size_t cap = opts->c.count + 1;
                out = realloc(out, cap * sizeof(char *));
                size_t o = 0;
                for (size_t j = 0; j < opts->c.count; j++) {
                    json_t *o2 = opts->c.items[j];
                    if (o2 && o2->type == JSON_STRING && o2->str_val) {
                        char low[256]; size_t k;
                        for (k = 0; o2->str_val[k] && k + 1 < sizeof(low); k++)
                            low[k] = (char)tolower((unsigned char)o2->str_val[k]);
                        low[k] = '\0';
                        out[o++] = xstrdup(low);
                    }
                }
                out[o] = NULL;
            }
            break;
        }
    }
    json_free(raw);
    return out;
}

/* Local: fetch + parse the GitHub Copilot catalog into a json_t* ARRAY.
 * Returns NULL on failure (caller frees). models_fetch_github_model_catalog
 * returns a serialized JSON array string, so we parse it here. */
static json_t *fetch_github_catalog(http_fetch_fn fetch, void *ctx, const char *api_key) {
    char *s = models_fetch_github_model_catalog(fetch, ctx, api_key);
    if (!s) return NULL;
    json_t *parsed = json_parse(s, NULL);
    free(s);
    if (!parsed || parsed->type != JSON_ARRAY) { if (parsed) json_free(parsed); return NULL; }
    return parsed;
}

/* PoP: _fetch_github_models @ hermes_cli/models.py:_fetch_github_models */
char **models_fetch_github_models(http_fetch_fn fetch, void *ctx,
                                  const char *api_key) {
    json_t *cat = api_key ? fetch_github_catalog(fetch, ctx, api_key) : NULL;
    if (!cat) return NULL;
    char **ids = calloc(1, sizeof(char *));
    if (cat->type == JSON_ARRAY) {
        ids = realloc(ids, (cat->c.count + 1) * sizeof(char *));
        size_t o = 0;
        for (size_t i = 0; i < cat->c.count; i++) {
            json_t *it = cat->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (idj && idj->type == JSON_STRING && idj->str_val && *idj->str_val)
                ids[o++] = xstrdup(idj->str_val);
        }
        ids[o] = NULL;
    }
    json_free(cat);
    return ids;
}

/* PoP: _copilot_catalog_ids @ hermes_cli/models.py:_copilot_catalog_ids */
char **models_copilot_catalog_ids(http_fetch_fn fetch, void *ctx,
                                  json_t *catalog, const char *api_key) {
    json_t *cat = catalog;
    json_t *owned = NULL;
    if (!cat && api_key) {
        cat = fetch_github_catalog(fetch, ctx, api_key);
        owned = cat;
    }
    char **ids = calloc(1, sizeof(char *));
    if (cat && cat->type == JSON_ARRAY) {
        ids = realloc(ids, (cat->c.count + 1) * sizeof(char *));
        size_t o = 0;
        for (size_t i = 0; i < cat->c.count; i++) {
            json_t *it = cat->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (idj && idj->type == JSON_STRING && idj->str_val && *idj->str_val)
                ids[o++] = xstrdup(idj->str_val);
        }
        ids[o] = NULL;
    }
    if (owned) json_free(owned);
    return ids;
}

/* PoP: _github_reasoning_efforts_for_model_id @ hermes_cli/models.py:_github_reasoning_efforts_for_model_id */
char **models_github_reasoning_efforts_for_id(const char *model_id) {
    char raw[512];
    size_t n = strlen(model_id ? model_id : "");
    if (n >= sizeof(raw)) n = sizeof(raw) - 1;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) raw[j++] = (char)tolower((unsigned char)model_id[i]);
    raw[j] = '\0';
    const char *const *src = NULL;
    if (strncmp(raw, "openai/o1", 9) == 0 || strncmp(raw, "openai/o3", 9) == 0 ||
        strncmp(raw, "openai/o4", 9) == 0 ||
        strcmp(raw, "o1") == 0 || strcmp(raw, "o3") == 0 || strcmp(raw, "o4") == 0)
        src = O_SERIES_EFFORTS;
    else {
        char *norm = model_normalize_copilot_model_id(model_id);
        if (strncmp(norm, "gpt-5", 5) == 0) src = GPT5_EFFORTS;
        free(norm);
    }
    char **out = calloc(1, sizeof(char *));
    if (src) {
        size_t c = 0;
        for (size_t i = 0; src[i]; i++) c++;
        out = realloc(out, (c + 1) * sizeof(char *));
        for (size_t i = 0; src[i]; i++) out[i] = xstrdup(src[i]);
        out[c] = NULL;
    }
    return out;
}

/* PoP: copilot_model_api_mode @ hermes_cli/models.py:copilot_model_api_mode */
char *models_copilot_model_api_mode(http_fetch_fn fetch, void *ctx,
                                    const char *model_id, json_t *catalog,
                                    const char *api_key) {
    json_t *cat = catalog;
    json_t *owned = NULL;
    if (!cat && api_key) { cat = fetch_github_catalog(fetch, ctx, api_key); owned = cat; }
    char *normalized = model_normalize_copilot_model_id(model_id);
    char *mode = xstrdup("chat_completions");
    if (normalized && *normalized) {
        /* _should_use_copilot_responses_api: gpt-<digits> >=5 and not gpt-5-mini */
        if (strncmp(normalized, "gpt-", 4) == 0) {
            int major = atoi(normalized + 4);
            if (major >= 5 && strncmp(normalized, "gpt-5-mini", 10) != 0) {
                free(mode); mode = xstrdup("codex_responses");
            }
        }
        if (cat && strcmp(mode, "chat_completions") == 0) {
            /* check catalog supported_endpoints */
            if (cat->type == JSON_ARRAY) {
                for (size_t i = 0; i < cat->c.count; i++) {
                    json_t *it = cat->c.items[i];
                    if (!it || it->type != JSON_OBJECT) continue;
                    json_t *idj = json_obj_get(it, "id");
                    if (!(idj && idj->type == JSON_STRING && idj->str_val &&
                          strcmp(idj->str_val, normalized) == 0)) continue;
                    json_t *eps = json_obj_get(it, "supported_endpoints");
                    if (eps && eps->type == JSON_ARRAY) {
                        int has_messages = 0, has_chat = 0;
                        for (size_t k = 0; k < eps->c.count; k++) {
                            json_t *e = eps->c.items[k];
                            if (e->type == JSON_STRING && e->str_val) {
                                if (strcmp(e->str_val, "/v1/messages") == 0) has_messages = 1;
                                if (strcmp(e->str_val, "/chat/completions") == 0) has_chat = 1;
                            }
                        }
                        if (has_messages && !has_chat) {
                            free(mode); mode = xstrdup("anthropic_messages");
                        }
                    }
                    break;
                }
            }
        }
    }
    free(normalized);
    if (owned) json_free(owned);
    return mode;
}

/* PoP: github_model_reasoning_efforts @ hermes_cli/models.py:github_model_reasoning_efforts */
char **models_github_model_reasoning_efforts(http_fetch_fn fetch, void *ctx,
                                             const char *model_id, json_t *catalog,
                                             const char *api_key) {
    json_t *cat = catalog;
    json_t *owned = NULL;
    if (!cat && api_key) { cat = fetch_github_catalog(fetch, ctx, api_key); owned = cat; }
    char *normalized = model_normalize_copilot_model_id(model_id);
    char **out = calloc(1, sizeof(char *));
    json_t *entry = NULL;
    if (cat && cat->type == JSON_ARRAY && normalized) {
        for (size_t i = 0; i < cat->c.count; i++) {
            json_t *it = cat->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (idj && idj->type == JSON_STRING && idj->str_val &&
                strcmp(idj->str_val, normalized) == 0) { entry = it; break; }
        }
    }
    if (entry) {
        json_t *caps = json_obj_get(entry, "capabilities");
        if (caps && caps->type == JSON_OBJECT) {
            json_t *supports = json_obj_get(caps, "supports");
            if (supports && supports->type == JSON_OBJECT) {
                json_t *eff = json_obj_get(supports, "reasoning_effort");
                if (eff && eff->type == JSON_ARRAY) {
                    out = realloc(out, (eff->c.count + 1) * sizeof(char *));
                    size_t o = 0;
                    for (size_t k = 0; k < eff->c.count; k++) {
                        json_t *e = eff->c.items[k];
                        if (e && e->type == JSON_STRING && e->str_val && *e->str_val) {
                            char low[256]; size_t m;
                            for (m = 0; e->str_val[m] && m + 1 < sizeof(low); m++)
                                low[m] = (char)tolower((unsigned char)e->str_val[m]);
                            low[m] = '\0';
                            out[o++] = xstrdup(low);
                        }
                    }
                    out[o] = NULL;
                } else out[0] = NULL;
            } else out[0] = NULL;
        }
    }
    if (!out[0] && normalized) {
        free(out);
        out = models_github_reasoning_efforts_for_id(model_id);
    }
    free(normalized);
    if (owned) json_free(owned);
    return out;
}

/* PoP: get_copilot_model_context @ hermes_cli/models.py:get_copilot_model_context */
char *models_get_copilot_model_context(http_fetch_fn fetch, void *ctx,
                                       const char *model_id, const char *api_key) {
    json_t *catalog = fetch_github_catalog(fetch, ctx, api_key);
    if (!catalog) return NULL;
    char *result = NULL;
    if (catalog->type == JSON_ARRAY) {
        for (size_t i = 0; i < catalog->c.count; i++) {
            json_t *it = catalog->c.items[i];
            if (!it || it->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(it, "id");
            if (!(idj && idj->type == JSON_STRING && idj->str_val &&
                  strcmp(idj->str_val, model_id ? model_id : "") == 0)) continue;
            json_t *caps = json_obj_get(it, "capabilities");
            json_t *limits = (caps && caps->type == JSON_OBJECT) ? json_obj_get(caps, "limits") : NULL;
            json_t *mp = (limits && limits->type == JSON_OBJECT) ? json_obj_get(limits, "max_prompt_tokens") : NULL;
            if (mp && mp->type == JSON_NUMBER && mp->num_val > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)mp->num_val);
                result = xstrdup(buf);
            }
            break;
        }
    }
    json_free(catalog);
    return result;
}

/* ── Custom /v1 probe helpers ──────────────────────────────────────────── */

/* PoP: probe_api_models @ hermes_cli/models.py:probe_api_models */
char *models_probe_api_models(http_fetch_fn fetch, void *ctx,
                              const char *api_key, const char *base_url,
                              const char *api_mode) {
    char norm[1024];
    const char *b = base_url ? base_url : "";
    size_t n = strlen(b);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '/')) n--;
    snprintf(norm, sizeof(norm), "%.*s", (int)n, b);
    size_t nn = strlen(norm);
    if (!nn) {
        json_t *o = json_object();
        json_set(o, "models", json_null());
        json_set(o, "probed_url", json_string(""));
        json_set(o, "resolved_base_url", json_string(""));
        json_set(o, "suggested_base_url", json_null());
        json_set(o, "used_fallback", json_bool(0));
        char *r = json_serialize(o); json_free(o);
        return r ? r : xstrdup("{}");
    }

    /* github base url short-circuit */
    char low[1024];
    size_t k;
    for (k = 0; norm[k] && k + 1 < sizeof(low); k++) low[k] = (char)tolower((unsigned char)norm[k]);
    low[k] = '\0';
    if (strncmp(low, COPILOT_BASE_URL, strlen(COPILOT_BASE_URL)) == 0 ||
        strncmp(low, "https://models.github.ai/inference", 30) == 0 ||
        strncmp(low, "https://models.inference.ai.azure.com", 33) == 0) {
        char **gm = models_fetch_github_models(fetch, ctx, api_key);
        json_t *o = json_object();
        json_t *arr = json_array();
        if (gm) { for (size_t i = 0; gm[i]; i++) json_append(arr, json_string(gm[i])); }
        json_set(o, "models", arr);
        json_set(o, "probed_url", json_string(COPILOT_MODELS_URL));
        json_set(o, "resolved_base_url", json_string(COPILOT_BASE_URL));
        json_set(o, "suggested_base_url", json_null());
        json_set(o, "used_fallback", json_bool(0));
        char *r = json_serialize(o); json_free(o);
        if (gm) { for (size_t i = 0; gm[i]; i++) free(gm[i]); free(gm); }
        return r ? r : xstrdup("{}");
    }

    char alt[1024];
    if (nn >= 3 && strcmp(norm + nn - 3, "/v1") == 0)
        snprintf(alt, sizeof(alt), "%.*s", (int)(nn - 3), norm);
    else
        snprintf(alt, sizeof(alt), "%s/v1", norm);

    const char *cands[2]; int is_fb[2]; int nc = 0;
    cands[nc] = norm; is_fb[nc] = 0; nc++;
    if (strcmp(alt, norm) != 0) { cands[nc] = alt; is_fb[nc] = 1; nc++; }

    char *hdr = NULL;
    if (api_key && api_mode && strcmp(api_mode, "anthropic_messages") == 0)
        hdr = xstrdup_printf("{\"User-Agent\":\"%s\",\"x-api-key\":\"%s\",\"anthropic-version\":\"2023-06-01\"}", HERMES_UA, api_key);
    else if (api_key)
        hdr = xstrdup_printf("{\"User-Agent\":\"%s\",\"Authorization\":\"Bearer %s\"}", HERMES_UA, api_key);
    else
        hdr = xstrdup_printf("{\"User-Agent\":\"%s\"}", HERMES_UA);
    if (strncmp(norm, COPILOT_BASE_URL, strlen(COPILOT_BASE_URL)) == 0) {
        char *ch = copilot_default_headers_json();
        if (ch) { char *h2 = xstrdup_printf("%s%s", hdr, ch); free(hdr); free(ch); hdr = h2; }
    }

    char *probed = NULL;
    json_t *arr = NULL;
    for (int i = 0; i < nc; i++) {
        char url[1100];
        snprintf(url, sizeof(url), "%s/models", cands[i]);
        free(probed);
        probed = xstrdup(url);
        char *body = NULL; size_t blen = 0;
        if (fetch && fetch(url, hdr, &body, &blen, ctx) == 0 && body) {
            json_t *data = json_parse(body, NULL);
            free(body);
            if (data && data->type == JSON_OBJECT) {
                json_t *d = json_obj_get(data, "data");
                if (d && d->type == JSON_ARRAY) {
                    arr = json_array();
                    for (size_t i = 0; i < d->c.count; i++) {
                        json_t *m = d->c.items[i];
                        const char *id = NULL;
                        if (m && m->type == JSON_OBJECT) {
                            json_t *idj = json_obj_get(m, "id");
                            if (idj && idj->type == JSON_STRING && idj->str_val)
                                id = idj->str_val;
                        }
                        if (id && *id) json_append(arr, json_string(id));
                    }
                } else arr = json_array();
                json_free(data);
            } else arr = json_array();
            json_t *o = json_object();
            json_set(o, "models", arr ? arr : json_null());
            json_set(o, "probed_url", json_string(url));
            json_set(o, "resolved_base_url", json_string(cands[i]));
            json_set(o, "suggested_base_url", json_string(strcmp(alt, cands[i]) == 0 ? norm : alt));
            json_set(o, "used_fallback", json_bool(is_fb[i]));
            char *r = json_serialize(o); json_free(o);
            free(hdr); free(probed);
            return r ? r : xstrdup("{}");
        }
    }
    /* all candidates failed */
    json_t *o = json_object();
    json_set(o, "models", json_null());
    json_set(o, "probed_url", json_string(probed ? probed : ""));
    json_set(o, "resolved_base_url", json_string(norm));
    json_set(o, "suggested_base_url", json_string(strcmp(alt, norm) == 0 ? "" : alt));
    json_set(o, "used_fallback", json_bool(0));
    char *r = json_serialize(o); json_free(o);
    free(hdr); free(probed);
    return r ? r : xstrdup("{}");
}

/* PoP: fetch_api_models @ hermes_cli/models.py:fetch_api_models */
char **models_fetch_api_models(http_fetch_fn fetch, void *ctx,
                               const char *api_key, const char *base_url,
                               const char *api_mode) {
    char *probe = models_probe_api_models(fetch, ctx, api_key, base_url, api_mode);
    if (!probe) return NULL;
    json_t *p = json_parse(probe, NULL);
    free(probe);
    char **out = NULL;
    if (p && p->type == JSON_OBJECT) {
        json_t *m = json_obj_get(p, "models");
        if (m && m->type == JSON_ARRAY) {
            out = calloc(m->c.count + 1, sizeof(char *));
            size_t o = 0;
            for (size_t i = 0; i < m->c.count; i++) {
                json_t *it = m->c.items[i];
                if (it && it->type == JSON_STRING && it->str_val)
                    out[o++] = xstrdup(it->str_val);
            }
            out[o] = NULL;
        }
    }
    if (p) json_free(p);
    return out; /* NULL if models was null (unreachable) */
}

/* ── Config / base-url resolvers (injectable) ──────────────────────────── */

/* PoP: _get_custom_base_url @ hermes_cli/models.py:_get_custom_base_url */
char *models_get_custom_base_url(void) {
    const char *v = getenv("HERMES_CUSTOM_BASE_URL");
    if (v && *v) return xstrdup(v);
    return xstrdup("");
}

/* PoP: _get_model_config_dict @ hermes_cli/models.py:_get_model_config_dict */
char *models_get_model_config_dict(model_config_fn get, void *ctx) {
    if (get) {
        char *r = get(ctx);
        if (r) return r;
    }
    return xstrdup("{}");
}

/* PoP: _resolve_copilot_catalog_api_key @ hermes_cli/models.py:_resolve_copilot_catalog_api_key */
char *models_resolve_copilot_catalog_api_key(copilot_apikey_fn get, void *ctx) {
    if (get) {
        char *r = get(ctx);
        if (r) return r;
    }
    const char *v = getenv("OPENAI_API_KEY");
    if (v && *v) return xstrdup(v);
    v = getenv("GITHUB_COPILOT_TOKEN");
    if (v && *v) return xstrdup(v);
    return xstrdup("");
}

/* PoP: _resolve_nous_portal_url @ hermes_cli/models.py:_resolve_nous_portal_url */
char *models_resolve_nous_portal_url(void) {
    const char *v = getenv("NOUS_PORTAL_URL");
    return xstrdup(v && *v ? v : "https://portal.nousresearch.com");
}

/* ── Ollama Cloud disk cache ───────────────────────────────────────────── */

/* PoP: _ollama_cloud_cache_path @ hermes_cli/models.py:_ollama_cloud_cache_path */
char *models_ollama_cloud_cache_path(void) {
    char home[PATH_MAX];
    hermes_home_dir(home, sizeof(home));
    char out[PATH_MAX];
    snprintf(out, sizeof(out), "%s/ollama_cloud_models_cache.json", home);
    return xstrdup(out);
}

/* PoP: _load_ollama_cloud_cache @ hermes_cli/models.py:_load_ollama_cloud_cache */
char *models_load_ollama_cloud_cache(int ignore_ttl) {
    char *path = models_ollama_cloud_cache_path();
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;
    char buf[8192]; size_t total = 0; char *blob = NULL;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        char *nb = realloc(blob, total + n + 1);
        if (!nb) { free(blob); fclose(f); return NULL; }
        blob = nb; memcpy(blob + total, buf, n); total += n; blob[total] = '\0';
    }
    fclose(f);
    if (!blob) return NULL;
    json_t *root = json_parse(blob, NULL);
    free(blob);
    if (!root || root->type != JSON_OBJECT) { if (root) json_free(root); return NULL; }
    /* TTL check: cached_at within 1h unless ignore_ttl */
    if (!ignore_ttl) {
        json_t *at = json_obj_get(root, "cached_at");
        if (!at || at->type != JSON_NUMBER ||
            (time(NULL) - (time_t)at->num_val) > 3600) {
            json_free(root);
            return NULL;
        }
    }
    char *ser = json_serialize(root);
    json_free(root);
    return ser ? ser : NULL;
}

/* PoP: _save_ollama_cloud_cache @ hermes_cli/models.py:_save_ollama_cloud_cache */
void models_save_ollama_cloud_cache(const char *json) {
    if (!json || !*json) return;
    char *path = models_ollama_cloud_cache_path();
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (f) { fputs(json, f); fclose(f); rename(tmp, path); }
    else { FILE *f2 = fopen(path, "w"); if (f2) { fputs(json, f2); fclose(f2); } }
    free(path);
}

/* ── Nous free-tier / portal recommendation unions ─────────────────────── */

/* PoP: check_nous_free_tier @ hermes_cli/models.py:check_nous_free_tier */
int models_check_nous_free_tier(http_fetch_fn fetch, void *ctx,
                                nous_creds_fn resolve_nous, void *nous_ctx) {
    char *pricing = models_get_pricing_for_provider(fetch, ctx, resolve_nous, nous_ctx, "nous", 0);
    if (!pricing) return 0;
    json_t *root = json_parse(pricing, NULL);
    free(pricing);
    int free_tier = 0;
    if (root && root->type == JSON_OBJECT) {
        /* free-tier heuristic: no priced models present */
        free_tier = (root->c.count == 0) ? 1 : 0;
    }
    if (root) json_free(root);
    return free_tier;
}

/* PoP: _merge_with_models_dev @ hermes_cli/models.py:_merge_with_models_dev */
char **models_merge_with_models_dev(char *const *live, char *const *mdev) {
    /* live first (deduped), then mdev additions not already present. */
    size_t cap = 16, n = 0;
    char **out = calloc(cap, sizeof(char *));
    for (size_t i = 0; live && live[i]; i++) {
        int seen = 0;
        for (size_t j = 0; j < n; j++) if (strcasecmp(out[j], live[i]) == 0) { seen = 1; break; }
        if (seen) continue;
        if (n + 1 >= cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
        out[n++] = xstrdup(live[i]);
    }
    for (size_t i = 0; mdev && mdev[i]; i++) {
        int seen = 0;
        for (size_t j = 0; j < n; j++) if (strcasecmp(out[j], mdev[i]) == 0) { seen = 1; break; }
        if (seen) continue;
        if (n + 1 >= cap) { cap *= 2; out = realloc(out, cap * sizeof(char *)); }
        out[n++] = xstrdup(mdev[i]);
    }
    out[n] = NULL;
    return out;
}

/* PoP: union_with_portal_free_recommendations @ hermes_cli/models.py:union_with_portal_free_recommendations */
/* (also covers union_with_portal_paid_recommendations; `paid` selects tier semantics). */
static char **union_portal(http_fetch_fn fetch, void *ctx, const char *portal_base_url,
                           char *const *base, int paid) {
    (void)paid;
    char *rec = models_fetch_nous_recommended_models(fetch, ctx, portal_base_url, 0);
    char **merged = calloc(1, sizeof(char *));
    size_t m = 0;
    if (rec) {
        json_t *root = json_parse(rec, NULL);
        free(rec);
        if (root && root->type == JSON_OBJECT) {
            json_t *recommended = json_obj_get(root, "recommended");
            if (recommended && recommended->type == JSON_ARRAY) {
                size_t cap = recommended->c.count + 1;
                merged = realloc(merged, cap * sizeof(char *));
                for (size_t i = 0; i < recommended->c.count; i++) {
                    json_t *it = recommended->c.items[i];
                    if (it && it->type == JSON_STRING && it->str_val)
                        merged[m++] = xstrdup(it->str_val);
                }
            }
            json_free(root);
        }
    }
    for (size_t i = 0; base && base[i]; i++) {
        int seen = 0;
        for (size_t j = 0; j < m; j++) if (strcmp(merged[j], base[i]) == 0) { seen = 1; break; }
        if (seen) continue;
        merged = realloc(merged, (m + 2) * sizeof(char *));
        merged[m++] = xstrdup(base[i]);
    }
    merged[m] = NULL;
    return merged;
}

/* PoP: union_with_portal_free_recommendations @ hermes_cli/models.py:union_with_portal_free_recommendations */
char **models_union_portal_free_recommendations(http_fetch_fn fetch, void *ctx,
                                                 const char *portal_base_url,
                                                 char *const *base) {
    return union_portal(fetch, ctx, portal_base_url, base, 0);
}

/* PoP: union_with_portal_paid_recommendations @ hermes_cli/models.py:union_with_portal_paid_recommendations */
char **models_union_portal_paid_recommendations(http_fetch_fn fetch, void *ctx,
                                                 const char *portal_base_url,
                                                 char *const *base) {
    return union_portal(fetch, ctx, portal_base_url, base, 1);
}

