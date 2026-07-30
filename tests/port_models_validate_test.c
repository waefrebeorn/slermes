/*
 * port_models_validate_test.c — behavioral test for port_models_validate.c.
 * Uses injectable mock HTTP + injected MoA/catalog resolvers (no real
 * network), exercising validate_requested_model's real branches per
 * hermes_agent AGENTS.md (faithful logic, never stub).
 */

#include "port_models_validate.h"
#include "port_models_net.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) g_pass++; else { g_fail++; printf("FAIL: %s\n", msg); } } while (0)

typedef struct { const char *url_substr; const char *body; int called; } mock_t;
static int mock_fetch(const char *url, const char *h, char **ob, size_t *ol, void *ctx) {
    (void)h; mock_t *m = (mock_t*)ctx; m->called++;
    if (strstr(url, m->url_substr) == NULL) return -1;
    *ob = strdup(m->body); *ol = strlen(*ob); return 0;
}
/* POST mock for LM Studio load */
static int mock_post(const char *url, const char *h, const char *body, void *ctx) {
    (void)h; (void)body; mock_t *m = (mock_t*)ctx; m->called++;
    if (strstr(url, "/models/load") == NULL) return -1;
    return 0;
}

/* ── format checks ──────────────────────────────────────────────────────── */
static void test_format(void) {
    models_validate_result_t r = models_validate_requested_model(
        NULL, NULL, "", "openrouter", NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK(!r.accepted && !r.recognized && r.message && strstr(r.message, "empty"), "empty rejected");
    models_validate_result_free(&r);

    r = models_validate_requested_model(NULL, NULL, "gpt 5", "openrouter", NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    CHECK(!r.accepted && strstr(r.message, "spaces"), "spaces rejected");
    models_validate_result_free(&r);
}

/* ── lmstudio branch (probe) ────────────────────────────────────────────── */
static void test_lmstudio(void) {
    /* reachable, model present */
    mock_t m = { .url_substr = "/api/v1/models", .body =
        "{\"models\":[{\"key\":\"llama3.2\",\"type\":\"chat\"},{\"key\":\"emb\",\"type\":\"embedding\"}]}", .called=0 };
    models_validate_result_t r = models_validate_requested_model(
        mock_fetch, &m, "llama3.2", "lmstudio", "k", "http://localhost:1234", NULL, NULL, NULL, NULL, NULL);
    CHECK(r.accepted && r.recognized, "lmstudio present accepted");
    models_validate_result_free(&r);

    /* reachable, model missing -> rejected */
    models_validate_result_t r2 = models_validate_requested_model(
        mock_fetch, &m, "missing-model", "lmstudio", "k", "http://localhost:1234", NULL, NULL, NULL, NULL, NULL);
    CHECK(!r2.accepted, "lmstudio missing rejected");
    models_validate_result_free(&r2);

    /* unreachable -> rejected with warning */
    mock_t m3 = { .url_substr = "never", .body = "", .called = 0 };
    models_validate_result_t r3 = models_validate_requested_model(
        mock_fetch, &m3, "llama3.2", "lmstudio", "k", "http://localhost:1234", NULL, NULL, NULL, NULL, NULL);
    CHECK(!r3.accepted && strstr(r3.message ? r3.message : "", "reach"), "lmstudio unreachable rejected");
    models_validate_result_free(&r3);
}

/* ── custom branch: probe + autocorrect ─────────────────────────────────── */
static void test_custom(void) {
    /* model present in /models */
    mock_t m = { .url_substr = "/v1/models", .body = "{\"data\":[{\"id\":\"my-model\"},{\"id\":\"other\"}]}", .called=0 };
    models_validate_result_t r = models_validate_requested_model(
        mock_fetch, &m, "my-model", "custom", "k", "https://api.example.com/v1", NULL, NULL, NULL, NULL, NULL);
    CHECK(r.accepted && r.recognized, "custom present accepted");
    models_validate_result_free(&r);

    /* 1-char typo in a long-enough id -> autocorrect (cutoff 0.9).
     * "my-super-modex" vs "my-super-model": 1-char diff over 14 chars -> ratio 0.928. */
    mock_t m2 = { .url_substr = "/v1/models", .body = "{\"data\":[{\"id\":\"my-super-model\"}]}", .called=0 };
    models_validate_result_t r2 = models_validate_requested_model(
        mock_fetch, &m2, "my-super-modex", "custom", "k", "https://api.example.com/v1", NULL, NULL, NULL, NULL, NULL);
    CHECK(r2.accepted && r2.corrected_model && strcmp(r2.corrected_model, "my-super-model") == 0, "custom autocorrect");
    models_validate_result_free(&r2);
}

/* ── minimax branch: case-insensitive catalog lookup ────────────────────── */
static char *cat_resolver(void *ctx, const char *provider) {
    (void)ctx; (void)provider;
    return strdup("[\"MiniMax-M2.7\",\"MiniMax-M1\"]");
}
static char *cat_empty(void *ctx, const char *provider) {
    (void)ctx; (void)provider;
    return strdup("[]");
}
static char *cat_curated(void *ctx, const char *provider) {
    (void)ctx; (void)provider;
    return strdup("[\"hidden-model\"]");
}
static void test_minimax(void) {
    /* lower-case input matches mixed-case catalog */
    models_validate_result_t r = models_validate_requested_model(
        NULL, NULL, "minimax-m2.7", "minimax", NULL, NULL, NULL, NULL, NULL, cat_resolver, NULL);
    CHECK(r.accepted && r.recognized, "minimax case-insensitive accepted");
    models_validate_result_free(&r);

    /* not in catalog -> soft-accept warning */
    models_validate_result_t r2 = models_validate_requested_model(
        NULL, NULL, "totally-unknown", "minimax", NULL, NULL, NULL, NULL, NULL, cat_resolver, NULL);
    CHECK(r2.accepted && !r2.recognized && strstr(r2.message, "MiniMax"), "minimax unknown soft-accept");
    models_validate_result_free(&r2);
}

/* ── codex/oauth family-plausibility gate ──────────────────────────────── */
static void test_codex_family(void) {
    /* gpt- family plausible -> soft accept (catalog empty) */
    models_validate_result_t r = models_validate_requested_model(
        NULL, NULL, "gpt-5.4", "openai-codex", NULL, NULL, NULL, NULL, NULL, cat_empty, NULL);
    CHECK(r.accepted && !r.recognized, "codex gpt- plausible soft-accept");
    models_validate_result_free(&r);

    /* unrelated family -> rejected */
    models_validate_result_t r2 = models_validate_requested_model(
        NULL, NULL, "qwen3.5-4b", "openai-codex", NULL, NULL, NULL, NULL, NULL, cat_empty, NULL);
    CHECK(!r2.accepted && strstr(r2.message, "doesn't look like"), "codex unrelated rejected");
    models_validate_result_free(&r2);
}

/* ── generic probe: gemini prefix strip + curated fallback ──────────────── */
static void test_generic(void) {
    /* gemini: live /v1/models returns "models/..." prefixed ids */
    mock_t m = { .url_substr = "/v1/models", .body = "{\"data\":[{\"id\":\"models/gemini-2.5-flash\"}]}", .called=0 };
    models_validate_result_t r = models_validate_requested_model(
        mock_fetch, &m, "gemini-2.5-flash", "gemini", "k", "https://api.gemini.com/v1", NULL, NULL, NULL, NULL, NULL);
    CHECK(r.accepted && r.recognized, "gemini prefix-strip accepted");
    models_validate_result_free(&r);

    /* generic: not in live listing, but in curated catalog (via resolver) */
    mock_t m2 = { .url_substr = "/v1/models", .body = "{\"data\":[{\"id\":\"known-model\"}]}", .called=0 };
    models_validate_result_t r2 = models_validate_requested_model(
        mock_fetch, &m2, "hidden-model", "openrouter", "k", "https://openrouter.ai/api", NULL, NULL, NULL, cat_curated, NULL);
    CHECK(!r2.accepted, "generic not-in-live rejected"); /* curated fallback needs provider_model_ids which we stub empty here -> reject */
    models_validate_result_free(&r2);
}

/* fetch mock: returns raw models JSON for /api/v1/models */
static int lm_fetch(const char *url, const char *h, char **ob, size_t *ol, void *ctx) {
    (void)h; mock_t *m = (mock_t*)ctx;
    if (strstr(url, "/api/v1/models") != NULL) { *ob = strdup(m->body); *ol = strlen(*ob); return 0; }
    return -1;
}
/* post mock: accepts /models/load */
static int lm_post(const char *url, const char *h, const char *body, void *ctx) {
    (void)h; (void)body; mock_t *m = (mock_t*)ctx; m->called++;
    if (strstr(url, "/models/load") != NULL) return 0;
    return -1;
}

/* ── ensure_lmstudio_model_loaded ───────────────────────────────────────── */
static void test_ensure_loaded(void) {
    /* probe unreachable (fetch returns nothing) -> -1 */
    mock_t f0 = { .url_substr = "never", .body = "", .called = 0 };
    mock_t p0 = { .url_substr = "/models/load", .body = "", .called = 0 };
    int r0 = models_ensure_lmstudio_model_loaded(lm_fetch, &f0, lm_post, &p0, "llama3.2",
        "http://localhost:1234", "k", 4096, 120.0);
    CHECK(r0 == -1, "ensure_loaded probe-unreachable -> -1");

    /* POST load success -> returns target context */
    mock_t f = { .url_substr = "/api/v1/models", .body =
        "{\"models\":[{\"key\":\"llama3.2\",\"max_context_length\":8192,\"loaded_instances\":[]}]}", .called=0 };
    mock_t p = { .url_substr = "/models/load", .body = "", .called = 0 };
    int r1 = models_ensure_lmstudio_model_loaded(lm_fetch, &f, lm_post, &p, "llama3.2",
        "http://localhost:1234", "k", 4096, 120.0);
    CHECK(r1 == 4096, "ensure_loaded posts + returns target ctx");
    CHECK(p.called == 1, "load POST called once");

    /* already loaded with sufficient context -> no POST */
    mock_t f2 = { .url_substr = "/api/v1/models", .body =
        "{\"models\":[{\"key\":\"llama3.2\",\"max_context_length\":8192,"
        "\"loaded_instances\":[{\"config\":{\"context_length\":8192}}]}]}", .called=0 };
    mock_t p2 = { .url_substr = "/models/load", .body = "", .called = 0 };
    int r2 = models_ensure_lmstudio_model_loaded(lm_fetch, &f2, lm_post, &p2, "llama3.2",
        "http://localhost:1234", "k", 4096, 120.0);
    CHECK(r2 == 8192, "ensure_loaded reuses loaded instance");
    CHECK(p2.called == 0, "no POST when already loaded");

    /* clamp to max_context_length */
    mock_t f3 = { .url_substr = "/api/v1/models", .body =
        "{\"models\":[{\"key\":\"llama3.2\",\"max_context_length\":2048,\"loaded_instances\":[]}]}", .called=0 };
    mock_t p3 = { .url_substr = "/models/load", .body = "", .called = 0 };
    int r3 = models_ensure_lmstudio_model_loaded(lm_fetch, &f3, lm_post, &p3, "llama3.2",
        "http://localhost:1234", "k", 4096, 120.0);
    CHECK(r3 == 2048, "ensure_loaded clamps to max_context_length");
}

int main(void) {
    test_format();
    test_lmstudio();
    test_custom();
    test_minimax();
    test_codex_family();
    test_generic();
    test_ensure_loaded();
    printf("\nport_models_validate_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
