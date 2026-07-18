/*
 * port_models_net_test.c — behavioral test for port_models_net.c.
 * Uses an INJECTABLE mock HTTP transport (no real network), exercising the
 * faithful parse/filter/sort logic per hermes_agent AGENTS.md.
 */

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
    const char *url_substr;   /* only respond if url contains this */
    const char *body;          /* canned JSON response */
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

/* ── _copilot_catalog_item_is_text_model ───────────────────────────────── */
static void test_text_model(void) {
    const char *ok = "{\"id\":\"gpt-5.4\",\"model_picker_enabled\":true,\"capabilities\":{\"type\":\"chat\"}}";
    json_t *it = json_parse(ok, NULL);
    CHECK(models_copilot_item_is_text_model(it), "chat model is text model");
    json_free(it);

    const char *disabled = "{\"id\":\"x\",\"model_picker_enabled\":false}";
    it = json_parse(disabled, NULL);
    CHECK(!models_copilot_item_is_text_model(it), "picker-disabled excluded");
    json_free(it);

    const char *embedding = "{\"id\":\"y\",\"capabilities\":{\"type\":\"embedding\"}}";
    it = json_parse(embedding, NULL);
    CHECK(!models_copilot_item_is_text_model(it), "embedding type excluded");
    json_free(it);
}

/* ── fetch_github_model_catalog (mock) ─────────────────────────────────── */
static void test_github_catalog(void) {
    const char *payload = "{\"data\":["
        "{\"id\":\"gpt-5.4\",\"model_picker_enabled\":true,\"capabilities\":{\"type\":\"chat\"}},"
        "{\"id\":\"gpt-5.4\",\"model_picker_enabled\":true,\"capabilities\":{\"type\":\"chat\"}},"
        "{\"id\":\"emb-model\",\"capabilities\":{\"type\":\"embedding\"}},"
        "{\"id\":\"hidden\",\"model_picker_enabled\":false,\"capabilities\":{\"type\":\"chat\"}}"
        "]}";
    mock_t m = { .url_substr = "/copilot/models", .body = payload, .called = 0 };
    char *res = models_fetch_github_model_catalog(mock_fetch, &m, "fake-token");
    CHECK(m.called > 0, "fetch invoked");
    CHECK(res != NULL, "catalog returned");
    if (res) {
        json_t *a = json_parse(res, NULL);
        CHECK(a && a->type == JSON_ARRAY, "result is array");
        if (a && a->type == JSON_ARRAY) {
            CHECK(a->c.count == 1, "dedup + filter => 1 text model");
            json_t *e = a->c.items[0];
            CHECK(strcmp(json_get_str(e, "id", ""), "gpt-5.4") == 0, "correct id kept");
        }
        if (a) json_free(a);
        free(res);
    }
}

/* ── _fetch_anthropic_models (mock) ────────────────────────────────────── */
static void test_anthropic(void) {
    const char *payload = "{\"data\":[{\"id\":\"claude-haiku-4.5\"},{\"id\":\"claude-opus-4.8\"},{\"id\":\"claude-sonnet-5\"}]}";
    mock_t m = { .url_substr = "/v1/models", .body = payload, .called = 0 };
    char *res = models_fetch_anthropic_models(mock_fetch, &m, NULL, "tok");
    CHECK(res != NULL, "anthropic returned");
    if (res) {
        json_t *a = json_parse(res, NULL);
        CHECK(a && a->type == JSON_ARRAY && a->c.count == 3, "3 ids");
        if (a && a->type == JSON_ARRAY) {
            /* opus first */
            CHECK(strcmp(a->c.items[0]->str_val, "claude-opus-4.8") == 0, "opus sorted first");
            CHECK(strcmp(a->c.items[1]->str_val, "claude-sonnet-5") == 0, "sonnet second");
        }
        if (a) json_free(a);
        free(res);
    }
}

/* ── lmstudio + ollama (mock) ─────────────────────────────────────────── */
static void test_lmstudio_ollama(void) {
    const char *ls = "{\"data\":[{\"id\":\"llama3.2\"},{\"id\":\"qwen2.5\"}]}";
    mock_t m = { .url_substr = "/v1/models", .body = ls, .called = 0 };
    char *res = models_fetch_lmstudio_models(mock_fetch, &m, "http://localhost:1234");
    if (res) {
        json_t *a = json_parse(res, NULL);
        CHECK(a && a->type == JSON_ARRAY && a->c.count == 2, "lmstudio 2 models");
        json_free(a); free(res);
    } else CHECK(0, "lmstudio returned");

    const char *ol = "{\"models\":[{\"name\":\"llama3.2:latest\"},{\"name\":\"qwen2.5\"}]}";
    mock_t m2 = { .url_substr = "/api/tags", .body = ol, .called = 0 };
    res = models_fetch_ollama_cloud_models(mock_fetch, &m2, "https://api.ollama.com");
    if (res) {
        json_t *a = json_parse(res, NULL);
        CHECK(a && a->type == JSON_ARRAY && a->c.count == 2, "ollama 2 models");
        json_free(a); free(res);
    } else CHECK(0, "ollama returned");
}

/* ── pure helpers ─────────────────────────────────────────────────────── */
static void test_provider_keys(void) {
    char key[64], norm[64];
    models_provider_keys("Nous ", key, sizeof(key), norm, sizeof(norm));
    CHECK(strcmp(key, "nous") == 0, "key lowercased/trimmed");
    CHECK(strcmp(norm, "nous") == 0, "normalized nous");
    char *provs[] = { "nous", "openrouter", NULL };
    CHECK(models_model_in_provider_catalog("anthropic/claude-fable-5", provs), "in nous catalog");
    CHECK(!models_model_in_provider_catalog("totally-fake-model", provs), "not in catalog");
}

static void test_xai(void) {
    char *ids[] = { "grok-4.3", "grok-build-0.1", "grok-4.20-0309-reasoning", NULL };
    char *out[8] = {0};
    models_xai_promote_top(ids, out, 8);
    CHECK(strcmp(out[0], "grok-build-0.1") == 0, "top pinned first");
    /* free */
    for (int i = 0; out[i]; i++) free(out[i]);

    char *curated[8] = {0};
    char **c; size_t cn = 0;
    models_xai_curated_models(&c, &cn);
    size_t n = 0; for (; c[n]; n++) curated[n] = c[n];
    curated[n] = NULL;
    /* merge extras */
    models_xai_merge_curated_extras(curated, 8);
    int has_composer = 0;
    for (size_t i = 0; curated[i]; i++) if (strcmp(curated[i], "grok-composer-2.5-fast") == 0) has_composer = 1;
    CHECK(has_composer, "extra merged after headline");
    for (size_t i = 0; curated[i]; i++) free(curated[i]);
    free(c);
}

static void test_aux_model(void) {
    char *r = models_nous_recommended_aux_model("{\"aux_model\":\"claude-opus-4.8\"}");
    CHECK(r && strcmp(r, "claude-opus-4.8") == 0, "aux_model extracted");
    free(r);
    r = models_nous_recommended_aux_model("{\"models\":[\"gpt-5.4\",\"sonnet\"]}");
    CHECK(r && strcmp(r, "gpt-5.4") == 0, "first model fallback");
    free(r);
}

static void test_base_url(void) {
    CHECK(models_is_github_models_base_url("https://models.github.ai/inference"), "github inference");
    CHECK(!models_is_github_models_base_url("https://api.openai.com"), "openai not github");
}

/* ── get_curated_nous_model_ids / fetch_nous_recommended_models / disk ──── */
static void test_nous(void) {
    /* Isolate the disk cache under a temp HERMES_HOME. */
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "/tmp/slermes_nous_test_%d", (int)getpid());
    setenv("HERMES_HOME", tmp, 1);
    mkdir(tmp, 0700);
    char cache_dir[600];
    snprintf(cache_dir, sizeof(cache_dir), "%s/cache", tmp);
    mkdir(cache_dir, 0700);
    mock_t m = { .url_substr = "never-matches", .body = "", .called = 0 };
    char **ids; size_t in = 0;
    models_get_curated_nous_model_ids(mock_fetch, &m, &ids, &in);
    CHECK(ids != NULL, "nous curated non-null");
    int n = 0; for (; ids[n]; n++) free(ids[n]);
    CHECK(n > 0, "nous snapshot non-empty");
    free(ids);

    /* write + read disk cache round-trip */
    const char *payload = "{\"data\":{\"foo\":\"bar\"},\"ts\":1}";
    models_write_nous_recommended_disk("https://portal.nousresearch.com", payload);
    char *disk = read_nous_recommended_disk("https://portal.nousresearch.com");
    CHECK(disk != NULL, "disk cache written + read");
    if (disk) {
        json_t *root = json_parse(disk, NULL);
        CHECK(root && root->type == JSON_OBJECT, "disk payload is object");
        /* read_nous_recommended_disk returns the stored `data` payload. */
        json_t *inner = json_obj_get(root, "data");
        CHECK(inner && inner->type == JSON_OBJECT, "payload has data");
        json_t *foo = json_obj_get(inner, "foo");
        CHECK(foo != NULL && foo->type == JSON_STRING && strcmp(foo->str_val,"bar")==0, "data payload returned");
        json_free(root);
        free(disk);
    }

    /* fetch_nous_recommended_models: mock returns live JSON; caches + returns */
    const char *live = "{\"recommended\":[\"hermes-3\",\"nous-1\"],\"base\":\"ignored\"}";
    mock_t m2 = { .url_substr = "/api/nous/recommended-models", .body = live, .called = 0 };
    char *got = models_fetch_nous_recommended_models(mock_fetch, &m2,
                                                     "https://portal.nousresearch.com", 0);
    CHECK(got != NULL, "nous recommended fetched");
    if (got) {
        json_t *root = json_parse(got, NULL);
        CHECK(root && json_obj_get(root, "recommended") != NULL, "live payload returned");
        json_free(root);
        free(got);
    }
    CHECK(m2.called == 1, "fetch called once");
}

int main(void) {
    test_text_model();
    test_github_catalog();
    test_anthropic();
    test_lmstudio_ollama();
    test_provider_keys();
    test_xai();
    test_aux_model();
    test_base_url();
    test_nous();
    printf("\nport_models_net_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
