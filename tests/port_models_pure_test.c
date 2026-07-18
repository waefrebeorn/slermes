/*
 * port_models_pure_test.c — behavioral test for port_models_pure.c.
 * Uses an INJECTABLE mock HTTP transport (no real network) plus disk cache
 * and config-resolver behavior, exercising the faithful parse/filter/merge
 * logic per hermes_agent AGENTS.md (real path, never stub).
 */

#include "port_models_pure.h"
#include "port_models_net.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) g_pass++; else { g_fail++; printf("FAIL: %s\n", msg); } } while (0)

/* ── Mock HTTP transport ──────────────────────────────────────────────── */
typedef struct {
    const char *url_substr;
    const char *body;
    int         called;
} mock_t;

static int mock_fetch(const char *url, const char *headers_json,
                      char **out_body, size_t *out_len, void *ctx) {
    (void)headers_json;
    mock_t *m = (mock_t *)ctx;
    m->called++;
    if (strstr(url, m->url_substr) == NULL) return -1;
    *out_body = strdup(m->body);
    *out_len = strlen(m->body);
    return 0;
}

static void strlist_free(char **p) { if (!p) return; for (size_t i = 0; p[i]; i++) free(p[i]); free(p); }

/* ── OpenRouter / Novita / Nous pricing ───────────────────────────────── */
static void test_pricing(void) {
    /* OpenRouter returns {data:[{id, pricing:{prompt,completion}}]} */
    const char *or_body = "{\"data\":["
        "{\"id\":\"openai/gpt-5.4\",\"pricing\":{\"prompt\":\"1.0\",\"completion\":\"2.0\"}},"
        "{\"id\":\"anthropic/claude-sonnet-4.6\",\"pricing\":{\"prompt\":\"3\",\"completion\":\"4\"}}]}";
    mock_t m = { .url_substr = "/v1/models", .body = or_body, .called = 0 };
    char *p = models_fetch_models_with_pricing(mock_fetch, &m, "k", "https://openrouter.ai/api", 0);
    CHECK(p != NULL, "openrouter pricing returned");
    if (p) {
        json_t *root = json_parse(p, NULL);
        CHECK(root && root->type == JSON_OBJECT, "pricing is object");
        json_t *e = json_obj_get(root, "openai/gpt-5.4");
        CHECK(e && e->type == JSON_OBJECT, "entry present");
        if (e) {
            CHECK(strcmp(json_get_str(e, "prompt", ""), "1.0") == 0, "prompt price");
            CHECK(strcmp(json_get_str(e, "completion", ""), "2.0") == 0, "completion price");
        }
        json_free(root); free(p);
    }

    /* get_pricing_for_provider routes openrouter */
    mock_t m2 = { .url_substr = "/v1/models", .body = or_body, .called = 0 };
    char *pp = models_get_pricing_for_provider(mock_fetch, &m2, NULL, NULL, "openrouter", 0);
    CHECK(pp != NULL, "get_pricing_for_provider(openrouter) returned");
    if (pp) { json_t *r = json_parse(pp, NULL); CHECK(r && r->type == JSON_OBJECT, "provider pricing object"); json_free(r); free(pp); }

    /* unknown provider => "{}" */
    char *empty = models_get_pricing_for_provider(NULL, NULL, NULL, NULL, "bogus", 0);
    CHECK(empty && strcmp(empty, "{}") == 0, "bogus provider -> {}");
    free(empty);

    /* Novita: input_token_price_per_m / output_token_price_per_m */
    const char *nov = "{\"data\":[{\"id\":\"meta/llama-3.1-8b\",\"input_token_price_per_m\":25,\"output_token_price_per_m\":50}]}";
    mock_t m3 = { .url_substr = "/models", .body = nov, .called = 0 };
    setenv("NOVITA_API_KEY", "nk", 1);
    char *np = models_fetch_novita_pricing(mock_fetch, &m3, 0);
    CHECK(np != NULL, "novita pricing returned");
    if (np) {
        json_t *r = json_parse(np, NULL);
        json_t *e = r ? json_obj_get(r, "meta/llama-3.1-8b") : NULL;
        CHECK(e && e->type == JSON_OBJECT, "novita entry");
        if (e) {
            /* 25 / 10000 / 1e6 = 2.5e-9 */
            CHECK(strcmp(json_get_str(e, "prompt", ""), "2.5e-09") == 0, "novita prompt converted");
        }
        json_free(r); free(np);
    }
    unsetenv("NOVITA_API_KEY");
}

/* ── LM Studio helpers ─────────────────────────────────────────────────── */
static void test_lmstudio(void) {
    char *root = models_lmstudio_server_root("http://localhost:1234/api/v1/");
    CHECK(root && strcmp(root, "http://localhost:1234") == 0, "lmstudio root stripped");
    free(root);
    root = models_lmstudio_server_root("http://host/v1");
    CHECK(root && strcmp(root, "http://host") == 0, "lmstudio /v1 stripped");
    free(root);

    char *hdr = models_lmstudio_request_headers("");
    CHECK(hdr && strstr(hdr, "\"User-Agent\"") && !strstr(hdr, "Authorization"), "lmstudio header no-auth");
    free(hdr);
    hdr = models_lmstudio_request_headers("tok");
    CHECK(hdr && strstr(hdr, "Bearer tok"), "lmstudio header bearer");
    free(hdr);

    const char *raw = "{\"models\":["
        "{\"key\":\"llama3.2\",\"type\":\"chat\"},"
        "{\"key\":\"emb\",\"type\":\"embedding\"},"
        "{\"id\":\"qwen2.5\",\"type\":\"chat\"}]}";
    mock_t m = { .url_substr = "/api/v1/models", .body = raw, .called = 0 };
    char **keys = models_probe_lmstudio_models(mock_fetch, &m, "k", "http://localhost:1234");
    CHECK(keys != NULL, "lmstudio probe returned");
    int n = 0; for (; keys && keys[n]; n++) {}
    CHECK(n == 2, "lmstudio 2 chat models (embedding excluded)");
    strlist_free(keys);

    /* reasoning options: pick the asked model's capabilities.reasoning.allowed_options */
    const char *rraw = "{\"models\":[{\"key\":\"qwen2.5\",\"capabilities\":{\"reasoning\":{\"allowed_options\":[\"Low\",\"HIGH\"]}}}]}";
    mock_t m2 = { .url_substr = "/api/v1/models", .body = rraw, .called = 0 };
    char **opts = models_lmstudio_reasoning_options(mock_fetch, &m2, "qwen2.5", "http://localhost:1234", "k");
    CHECK(opts != NULL, "lmstudio reasoning returned");
    int on = 0; for (; opts && opts[on]; on++) {}
    CHECK(on == 2, "lmstudio 2 reasoning options");
    if (opts) CHECK(strcmp(opts[0], "low") == 0 && strcmp(opts[1], "high") == 0, "lowercased options");
    strlist_free(opts);
}

/* ── GitHub Copilot catalog + reasoning efforts ────────────────────────── */
static void test_github_copilot(void) {
    const char *cat = "["
        "{\"id\":\"openai/gpt-5.4\",\"capabilities\":{\"supports\":{\"reasoning_effort\":[\"low\",\"high\"]}}}"
        "]";

    /* _fetch_github_models */
    mock_t m = { .url_substr = "/copilot/models", .body = cat, .called = 0 };
    char **ids = models_fetch_github_models(mock_fetch, &m, "tok");
    CHECK(ids != NULL, "fetch_github_models returned");
    int n = 0; for (; ids && ids[n]; n++) {}
    CHECK(n == 1 && strcmp(ids[0], "openai/gpt-5.4") == 0, "github model id extracted");
    strlist_free(ids);

    /* _copilot_catalog_ids from supplied catalog json_t* */
    json_t *c = json_parse(cat, NULL);
    char **cids = models_copilot_catalog_ids(NULL, NULL, c, NULL);
    CHECK(cids != NULL && cids[0] && strcmp(cids[0], "openai/gpt-5.4") == 0, "copilot_catalog_ids from obj");
    strlist_free(cids);
    json_free(c);

    /* reasoning efforts: o1/o3/o4 fixed lists */
    char **eff = models_github_reasoning_efforts_for_id("o3");
    CHECK(eff && eff[0] && strcmp(eff[0], "low") == 0, "o3 -> o-series efforts");
    strlist_free(eff);
    eff = models_github_reasoning_efforts_for_id("gpt-5.4");
    CHECK(eff && eff[0] && strcmp(eff[0], "minimal") == 0, "gpt-5 -> gpt5 efforts");
    strlist_free(eff);

    /* api_mode: gpt-5.4 -> codex_responses */
    char *mode = models_copilot_model_api_mode(NULL, NULL, "openai/gpt-5.4", NULL, NULL);
    CHECK(mode && strcmp(mode, "codex_responses") == 0, "gpt-5.4 -> codex_responses");
    free(mode);
    mode = models_copilot_model_api_mode(NULL, NULL, "openai/gpt-4o", NULL, NULL);
    CHECK(mode && strcmp(mode, "chat_completions") == 0, "gpt-4o -> chat_completions");
    free(mode);

    /* github_model_reasoning_efforts from catalog (id matches normalized gpt-5.4) */
    const char *cat2 = "[{\"id\":\"gpt-5.4\",\"capabilities\":{\"supports\":{\"reasoning_effort\":[\"low\",\"high\"]}}}]";
    json_t *c2 = json_parse(cat2, NULL);
    char **geff = models_github_model_reasoning_efforts(NULL, NULL, "gpt-5.4", c2, NULL);
    CHECK(geff != NULL, "catalog efforts non-null");
    CHECK(geff && geff[0] && strcmp(geff[0], "low") == 0, "catalog efforts low");
    strlist_free(geff);
    json_free(c2);

    /* get_copilot_model_context max_prompt_tokens (via mock catalog) */
    const char *cat3 = "[{\"id\":\"openai/gpt-5.4\",\"capabilities\":{\"limits\":{\"max_prompt_tokens\":200000}}}]";
    mock_t mctx = { .url_substr = "/copilot/models", .body = cat3, .called = 0 };
    char *ctx = models_get_copilot_model_context(mock_fetch, &mctx, "openai/gpt-5.4", "tok");
    CHECK(ctx != NULL, "copilot context fetched");
    CHECK(ctx && strcmp(ctx, "200000") == 0, "max_prompt_tokens extracted");
    free(ctx);
}

/* ── probe_api_models / fetch_api_models ───────────────────────────────── */
static void test_probe_api(void) {
    /* GitHub short-circuit branch */
    mock_t mg = { .url_substr = "/copilot/models", .body = "[{\"id\":\"a\"},{\"id\":\"b\"}]", .called = 0 };
    char *p = models_probe_api_models(mock_fetch, &mg, "tok", "https://api.githubcopilot.com/models", NULL);
    CHECK(p != NULL, "probe github returned");
    if (p) {
        json_t *r = json_parse(p, NULL);
        json_t *mods = r ? json_obj_get(r, "models") : NULL;
        CHECK(mods && mods->type == JSON_ARRAY && mods->c.count == 2, "github 2 models");
        CHECK(r && json_obj_get(r, "used_fallback") && json_obj_get(r, "used_fallback")->type == JSON_BOOL
              && json_obj_get(r, "used_fallback")->bool_val == 0, "used_fallback false");
        json_free(r); free(p);
    }

    /* generic /v1 probe */
    mock_t mo = { .url_substr = "/v1/models", .body = "{\"data\":[{\"id\":\"m1\"},{\"id\":\"m2\"}]}", .called = 0 };
    char *p2 = models_probe_api_models(mock_fetch, &mo, "tok", "https://api.example.com/v1", "chat_completions");
    CHECK(p2 != NULL, "probe generic returned");
    if (p2) {
        json_t *r = json_parse(p2, NULL);
        json_t *mods = r ? json_obj_get(r, "models") : NULL;
        CHECK(mods && mods->type == JSON_ARRAY && mods->c.count == 2, "generic 2 models");
        CHECK(r && json_obj_get(r, "used_fallback") && json_obj_get(r, "used_fallback")->bool_val == 0, "generic fallback false");
        json_free(r); free(p2);
    }

    /* fetch_api_models extracts ids */
    char **ids = models_fetch_api_models(mock_fetch, &mo, "tok", "https://api.example.com/v1", "chat_completions");
    CHECK(ids && ids[0] && strcmp(ids[0], "m1") == 0 && ids[1] && strcmp(ids[1], "m2") == 0, "fetch_api_models ids");
    strlist_free(ids);
}

/* ── Config / base-url / api-key resolvers ─────────────────────────────── */
static void test_config_resolvers(void) {
    setenv("HERMES_CUSTOM_BASE_URL", "https://my.host/v1", 1);
    char *cb = models_get_custom_base_url();
    CHECK(cb && strcmp(cb, "https://my.host/v1") == 0, "custom base url from env");
    free(cb);
    unsetenv("HERMES_CUSTOM_BASE_URL");

    char *nb = models_resolve_nous_portal_url();
    CHECK(nb && strcmp(nb, "https://portal.nousresearch.com") == 0, "nous portal default");
    free(nb);
    setenv("NOUS_PORTAL_URL", "https://portal.test", 1);
    nb = models_resolve_nous_portal_url();
    CHECK(nb && strcmp(nb, "https://portal.test") == 0, "nous portal from env");
    free(nb);
    unsetenv("NOUS_PORTAL_URL");

    /* injected copilot key resolver */
    char *ck = models_resolve_copilot_catalog_api_key(NULL, NULL);
    /* no injected getter -> env fallback (empty) */
    CHECK(ck && strcmp(ck, "") == 0, "copilot key empty without env");
    free(ck);
    setenv("OPENAI_API_KEY", "oa-k", 1);
    ck = models_resolve_copilot_catalog_api_key(NULL, NULL);
    CHECK(ck && strcmp(ck, "oa-k") == 0, "copilot key from OPENAI_API_KEY");
    free(ck);
    unsetenv("OPENAI_API_KEY");

    /* injected model config dict getter */
    char *rd = models_get_model_config_dict(NULL, NULL);
    CHECK(rd && strcmp(rd, "{}") == 0, "config dict default {}");
    free(rd);
}

/* ── Ollama Cloud disk cache ───────────────────────────────────────────── */
static void test_ollama_cache(void) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "/tmp/slermes_ollama_test_%d", (int)getpid());
    setenv("HERMES_HOME", tmp, 1);
    mkdir(tmp, 0700);
    const char *payload = "{\"cached_at\":123,\"models\":[\"a\",\"b\"]}";
    models_save_ollama_cloud_cache(payload);
    char *got = models_load_ollama_cloud_cache(1 /* ignore_ttl */);
    CHECK(got != NULL, "ollama cache loaded (ignore_ttl)");
    if (got) {
        json_t *r = json_parse(got, NULL);
        CHECK(r && r->type == JSON_OBJECT, "cache is object");
        json_free(r); free(got);
    }
    /* fresh-but-expired (cached_at far in past, ttl enforced) */
    char *got2 = models_load_ollama_cloud_cache(0);
    CHECK(got2 == NULL, "expired cache rejected without ignore_ttl");
}

/* ── Nous free-tier / merge / portal unions ────────────────────────────── */
static void test_merge_union(void) {
    char *live[] = { "a", "b", "c", NULL };
    char *dev[] = { "b", "d", NULL };
    char **merged = models_merge_with_models_dev(live, dev);
    CHECK(merged != NULL, "merge returned");
    int n = 0; for (; merged && merged[n]; n++) {}
    /* a,b,c,d (b deduped) */
    CHECK(n == 4, "merge 4 unique (b deduped)");
    strlist_free(merged);

    /* portal union: merges base with portal recommended */
    const char *rec = "{\"recommended\":[\"x\",\"y\"]}";
    mock_t m = { .url_substr = "/api/nous/recommended-models", .body = rec, .called = 0 };
    char *base_dup[] = { "a", "x", NULL };
    char **u = models_union_portal_free_recommendations(mock_fetch, &m, "https://portal.nousresearch.com", base_dup);
    CHECK(u != NULL, "portal union returned");
    int un = 0; for (; u && u[un]; un++) {}
    /* a,x,y */
    CHECK(un == 3, "portal union 3 (x deduped)");
    strlist_free(u);

    /* check_nous_free_tier: empty pricing => free tier */
    mock_t mp = { .url_substr = "never", .body = "{}", .called = 0 };
    int ft = models_check_nous_free_tier(mock_fetch, &mp, NULL, NULL);
    CHECK(ft == 1, "empty pricing => free tier");
}

int main(void) {
    test_pricing();
    test_lmstudio();
    test_github_copilot();
    test_probe_api();
    test_config_resolvers();
    test_ollama_cache();
    test_merge_union();
    printf("\nport_models_pure_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
