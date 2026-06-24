/*
 * tests/test_auxiliary_client.c — unit tests for auxiliary client
 *
 * Tests: provider normalization, model predicates, header builders,
 * URL helpers, error classification, health tracking, vision helpers,
 * task config resolution.
 */

#include "auxiliary_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int pass = 0, fail = 0;

#define TEST(name) do { \
    printf("  %s ... ", #name); \
    if (test_##name()) { \
        printf("PASS\n"); pass++; \
    } else { \
        printf("FAIL\n"); fail++; \
    } \
} while(0)

#define CHECK(cond, fmt, ...) do { \
    if (!(cond)) { \
        printf("    FAIL at %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        return 0; \
    } \
} while(0)

/* ─── Provider normalization tests ─── */

static int test_normalize_provider_google_alias(void) {
    const char *r = auxiliary_normalize_provider("google");
    CHECK(strcmp(r, "gemini") == 0, "google -> gemini, got '%s'", r);
    return 1;
}

static int test_normalize_provider_xai(void) {
    const char *r = auxiliary_normalize_provider("x-ai");
    CHECK(strcmp(r, "xai") == 0, "x-ai -> xai, got '%s'", r);
    return 1;
}

static int test_normalize_provider_claude(void) {
    const char *r = auxiliary_normalize_provider("claude");
    CHECK(strcmp(r, "anthropic") == 0, "claude -> anthropic, got '%s'", r);
    return 1;
}

static int test_normalize_provider_github_copilot(void) {
    const char *r = auxiliary_normalize_provider("github-copilot-acp");
    CHECK(strcmp(r, "copilot-acp") == 0, "github-copilot-acp -> copilot-acp, got '%s'", r);
    return 1;
}

static int test_normalize_provider_codex(void) {
    const char *r = auxiliary_normalize_provider("codex");
    CHECK(strcmp(r, "openai-codex") == 0, "codex -> openai-codex, got '%s'", r);
    return 1;
}

static int test_normalize_provider_custom_prefix(void) {
    const char *r = auxiliary_normalize_provider("custom:openai");
    CHECK(strcmp(r, "openai") == 0, "custom:openai -> openai, got '%s'", r);
    return 1;
}

static int test_normalize_provider_unknown_passthrough(void) {
    const char *r = auxiliary_normalize_provider("my-custom-provider");
    CHECK(strcmp(r, "my-custom-provider") == 0,
          "unknown -> passthrough, got '%s'", r);
    return 1;
}

static int test_normalize_provider_null_auto(void) {
    const char *r = auxiliary_normalize_provider(NULL);
    CHECK(strcmp(r, "auto") == 0, "NULL -> auto, got '%s'", r);
    return 1;
}

/* ─── Model predicate tests ─── */

static int test_is_kimi_model_kimi(void) {
    CHECK(auxiliary_is_kimi_model("kimi"), "bare 'kimi' should be true");
    return 1;
}

static int test_is_kimi_model_kimi_prefix(void) {
    CHECK(auxiliary_is_kimi_model("kimi-k2-turbo"), "kimi-k2-turbo should be true");
    return 1;
}

static int test_is_kimi_model_not_kimi(void) {
    CHECK(!auxiliary_is_kimi_model("gpt-4o"), "gpt-4o should be false");
    return 1;
}

static int test_is_arcee_trinity_thinking(void) {
    CHECK(auxiliary_is_arcee_trinity_thinking("trinity-large-thinking"),
          "trinity-large-thinking should be true");
    CHECK(!auxiliary_is_arcee_trinity_thinking("gpt-4o"),
          "gpt-4o should be false");
    return 1;
}

static int test_fixed_temperature_kimi(void) {
    float t = auxiliary_fixed_temperature_for_model("kimi-k2", NULL);
    CHECK(t == AUX_OMIT_TEMPERATURE, "kimi should get OMIT, got %.1f", t);
    return 1;
}

static int test_fixed_temperature_trinity(void) {
    float t = auxiliary_fixed_temperature_for_model("trinity-large-thinking", NULL);
    CHECK(t == 0.5f, "trinity should get 0.5, got %.1f", t);
    return 1;
}

static int test_fixed_temperature_default(void) {
    float t = auxiliary_fixed_temperature_for_model("gpt-4o", NULL);
    CHECK(t == 0.0f, "default should get 0.0, got %.1f", t);
    return 1;
}

static int test_compression_threshold_trinity(void) {
    float th = auxiliary_compression_threshold_for_model("trinity-large-thinking");
    CHECK(th == 0.75f, "trinity threshold 0.75, got %.2f", th);
    return 1;
}

static int test_compression_threshold_default(void) {
    float th = auxiliary_compression_threshold_for_model("gpt-4o");
    CHECK(th == 0.0f, "default threshold 0.0, got %.2f", th);
    return 1;
}

/* ─── Aux model lookup tests ─── */

static int test_get_aux_model_gemini(void) {
    const char *m = auxiliary_get_aux_model_for_provider("gemini");
    CHECK(m && m[0], "gemini should have an aux model");
    CHECK(strcmp(m, "gemini-3-flash-preview") == 0,
          "gemini aux model, got '%s'", m);
    return 1;
}

static int test_get_aux_model_anthropic(void) {
    const char *m = auxiliary_get_aux_model_for_provider("anthropic");
    CHECK(m && m[0], "anthropic should have an aux model");
    CHECK(strstr(m, "claude-haiku") != NULL,
          "anthropic aux model should contain claude-haiku, got '%s'", m);
    return 1;
}

static int test_get_aux_model_unknown(void) {
    const char *m = auxiliary_get_aux_model_for_provider("nonexistent");
    CHECK(m && m[0] == '\0', "unknown should return empty, got '%s'", m);
    return 1;
}

static int test_vision_model_zai(void) {
    const char *m = auxiliary_get_vision_model_for_provider("zai");
    CHECK(m && strcmp(m, "glm-5v-turbo") == 0,
          "zai vision model, got '%s'", m);
    return 1;
}

static int test_provider_without_vision(void) {
    CHECK(auxiliary_provider_without_vision("kimi-coding"),
          "kimi-coding should be without vision");
    CHECK(!auxiliary_provider_without_vision("openai"),
          "openai should have vision");
    return 1;
}

/* ─── Header builder tests ─── */

static int test_build_or_headers_basic(void) {
    char buf[512];
    auxiliary_build_or_headers(buf, sizeof(buf), false, 0);
    CHECK(strstr(buf, "Hermes Agent") != NULL,
          "OR headers should contain Hermes Agent, got '%s'", buf);
    CHECK(strstr(buf, "HTTP-Referer") != NULL,
          "OR headers should contain HTTP-Referer");
    return 1;
}

static int test_build_or_headers_cache_enabled(void) {
    char buf[512];
    auxiliary_build_or_headers(buf, sizeof(buf), true, 300);
    CHECK(strstr(buf, "X-OpenRouter-Cache") != NULL,
          "cache enabled should include cache header, got '%s'", buf);
    return 1;
}

static int test_build_nvidia_headers_cloud(void) {
    char buf[256];
    auxiliary_build_nvidia_nim_headers(buf, sizeof(buf),
        "https://integrate.api.nvidia.com/v1");
    CHECK(buf[0] != '\0', "nvidia cloud should get headers, got '%s'", buf);
    CHECK(strstr(buf, "HermesAgent") != NULL,
          "should contain HermesAgent");
    return 1;
}

static int test_build_nvidia_headers_local(void) {
    char buf[256];
    auxiliary_build_nvidia_nim_headers(buf, sizeof(buf),
        "https://localhost:8080/v1");
    CHECK(buf[0] == '\0', "local nvidia should get no headers, got '%s'", buf);
    return 1;
}

static int test_build_nous_extra_body(void) {
    char buf[256];
    auxiliary_build_nous_extra_body(buf, sizeof(buf));
    CHECK(strstr(buf, "tags") != NULL,
          "nous extra body should contain tags, got '%s'", buf);
    return 1;
}

static int test_build_codex_headers(void) {
    char buf[512];
    auxiliary_build_codex_headers(buf, sizeof(buf), "test-token");
    CHECK(strstr(buf, "codex_cli_rs") != NULL,
          "codex headers should contain codex_cli_rs, got '%s'", buf);
    CHECK(strstr(buf, "originator") != NULL,
          "codex headers should contain originator");
    return 1;
}

/* ─── URL helper tests ─── */

static int test_to_openai_base_url_anthropic_suffix(void) {
    char buf[256];
    auxiliary_to_openai_base_url(buf, sizeof(buf),
        "https://api.minimax.io/v1/anthropic");
    CHECK(strstr(buf, "/v1") != NULL && strstr(buf, "/anthropic") == NULL,
          "anthropic suffix should rewrite, got '%s'", buf);
    return 1;
}

static int test_to_openai_base_url_zai(void) {
    char buf[256];
    auxiliary_to_openai_base_url(buf, sizeof(buf),
        "https://open.bigmodel.cn/api/anthropic");
    CHECK(strstr(buf, "/paas/v4") != NULL,
          "zai should get /paas/v4, got '%s'", buf);
    return 1;
}

static int test_to_openai_base_url_kimi(void) {
    char buf[256];
    auxiliary_to_openai_base_url(buf, sizeof(buf),
        "https://api.kimi.com/coding");
    CHECK(strstr(buf, "/coding/v1") != NULL,
          "kimi should get /coding/v1, got '%s'", buf);
    return 1;
}

static int test_to_openai_base_url_normal(void) {
    char buf[256];
    auxiliary_to_openai_base_url(buf, sizeof(buf),
        "https://api.openai.com/v1");
    CHECK(strcmp(buf, "https://api.openai.com/v1") == 0,
          "normal url unchanged, got '%s'", buf);
    return 1;
}

static int test_endpoint_speaks_anthropic(void) {
    CHECK(auxiliary_endpoint_speaks_anthropic("https://api.anthropic.com"),
          "api.anthropic.com should be anthropic");
    CHECK(auxiliary_endpoint_speaks_anthropic("https://api.example.com/anthropic"),
          "/anthropic suffix should be anthropic");
    CHECK(!auxiliary_endpoint_speaks_anthropic("https://api.openai.com/v1"),
          "openai should NOT be anthropic");
    return 1;
}

static int test_is_anthropic_compat_endpoint(void) {
    CHECK(auxiliary_is_anthropic_compat_endpoint("anthropic", NULL),
          "explicit anthropic provider should match");
    CHECK(!auxiliary_is_anthropic_compat_endpoint("openai", "https://api.openai.com/v1"),
          "openai provider + openai url should NOT match");
    return 1;
}

static int test_validate_base_url_valid(void) {
    CHECK(auxiliary_validate_base_url("https://api.openai.com/v1"),
          "valid https url");
    CHECK(auxiliary_validate_base_url(""), "empty url = valid (not set)");
    return 1;
}

static int test_validate_base_url_invalid(void) {
    CHECK(!auxiliary_validate_base_url("not-a-url"),
          "url without scheme should be invalid");
    return 1;
}

/* ─── Error classification tests ─── */

static int test_is_payment_error_402(void) {
    CHECK(auxiliary_is_payment_error(402, ""), "HTTP 402 = payment error");
    return 1;
}

static int test_is_payment_error_text(void) {
    CHECK(auxiliary_is_payment_error(200, "insufficient_quota"),
          "insufficient_quota text = payment error");
    return 1;
}

static int test_is_rate_limit_429(void) {
    CHECK(auxiliary_is_rate_limit_error(429, ""), "HTTP 429 = rate limit");
    return 1;
}

static int test_is_rate_limit_text(void) {
    CHECK(auxiliary_is_rate_limit_error(200, "rate limit exceeded"),
          "rate limit text = rate limit");
    return 1;
}

static int test_is_connection_error(void) {
    CHECK(auxiliary_is_connection_error("Connection refused"),
          "connection refused");
    CHECK(auxiliary_is_connection_error("timeout"), "timeout = connection error");
    CHECK(!auxiliary_is_connection_error("success"), "success = not connection error");
    return 1;
}

static int test_is_auth_error_401(void) {
    CHECK(auxiliary_is_auth_error(401, ""), "HTTP 401 = auth error");
    CHECK(auxiliary_is_auth_error(403, ""), "HTTP 403 = auth error");
    return 1;
}

static int test_is_auth_error_text(void) {
    CHECK(auxiliary_is_auth_error(200, "unauthorized"),
          "unauthorized text = auth error");
    return 1;
}

static int test_is_model_not_found_404(void) {
    CHECK(auxiliary_is_model_not_found_error(404, ""),
          "HTTP 404 = model not found");
    return 1;
}

static int test_is_model_not_found_text(void) {
    CHECK(auxiliary_is_model_not_found_error(404, "model not found"),
          "model not found text");
    return 1;
}

static int test_is_unsupported_temperature(void) {
    CHECK(auxiliary_is_unsupported_temperature_error("temperature is not supported"),
          "unsupported temperature");
    CHECK(!auxiliary_is_unsupported_temperature_error("ok"),
          "ok = not unsupported temperature");
    return 1;
}

/* ─── Health tracking tests ─── */

static int test_health_mark_unhealthy(void) {
    auxiliary_reset_unhealthy_cache();
    CHECK(!auxiliary_is_provider_unhealthy("test-prov"),
          "fresh provider should be healthy");
    auxiliary_mark_provider_unhealthy("test-prov", 30);
    CHECK(auxiliary_is_provider_unhealthy("test-prov"),
          "marked provider should be unhealthy");
    return 1;
}

static int test_health_reset(void) {
    auxiliary_mark_provider_unhealthy("test-prov-2", 30);
    auxiliary_reset_unhealthy_cache();
    CHECK(!auxiliary_is_provider_unhealthy("test-prov-2"),
          "after reset, provider should be healthy");
    return 1;
}

/* ─── Vision helper tests ─── */

static int test_main_model_supports_vision(void) {
    CHECK(auxiliary_main_model_supports_vision("openai", "gpt-4o"),
          "openai + gpt-4o should support vision");
    CHECK(!auxiliary_main_model_supports_vision("unknown-provider", "gpt-4o"),
          "unknown provider should NOT support vision");
    return 1;
}

static int test_normalize_vision_provider(void) {
    const char *r = auxiliary_normalize_vision_provider("google");
    CHECK(strcmp(r, "gemini") == 0, "google -> gemini, got '%s'", r);
    return 1;
}

static int test_strict_vision_backend(void) {
    CHECK(auxiliary_strict_vision_backend_available("anthropic"),
          "anthropic should have strict vision");
    CHECK(!auxiliary_strict_vision_backend_available("xai"),
          "xai should not have strict vision");
    return 1;
}

static int test_get_available_vision_backends(void) {
    const char *b = auxiliary_get_available_vision_backends();
    CHECK(strstr(b, "openai") != NULL, "openai should be in backends");
    CHECK(strstr(b, "gemini") != NULL, "gemini should be in backends");
    return 1;
}

/* ─── Max tokens param tests ─── */

static int test_max_tokens_param(void) {
    char buf[64];
    auxiliary_max_tokens_param(buf, sizeof(buf), 4096);
    CHECK(strstr(buf, "4096") != NULL, "max_tokens 4096, got '%s'", buf);
    auxiliary_max_tokens_param(buf, sizeof(buf), 0);
    CHECK(buf[0] == '\0', "zero should give empty, got '%s'", buf);
    return 1;
}

/* ─── Chain label tests ─── */

static int test_normalize_chain_label(void) {
    const char *l = auxiliary_normalize_chain_label("openrouter");
    CHECK(strcmp(l, "OpenRouter") == 0, "openrouter -> OpenRouter, got '%s'", l);
    l = auxiliary_normalize_chain_label("nous");
    CHECK(strcmp(l, "Nous Portal") == 0, "nous -> Nous Portal, got '%s'", l);
    l = auxiliary_normalize_chain_label("unknown");
    CHECK(strcmp(l, "unknown") == 0, "unknown -> passthrough, got '%s'", l);
    return 1;
}

/* ─── Task label tests ─── */

static int test_task_label(void) {
    const char *l = auxiliary_task_label("compression");
    CHECK(strcmp(l, "context compression") == 0,
          "compression -> context compression, got '%s'", l);
    l = auxiliary_task_label("nonexistent");
    CHECK(strcmp(l, "nonexistent") == 0,
          "unknown -> passthrough, got '%s'", l);
    l = auxiliary_task_label(NULL);
    CHECK(strcmp(l, "auxiliary") == 0,
          "NULL -> auxiliary, got '%s'", l);
    return 1;
}

/* ─── Config resolution tests (from original) ─── */

static int test_resolve_auto_main_provider(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-test", sizeof(cfg.api_key) - 1);
    strncpy(cfg.base_url, "https://api.openai.com/v1", sizeof(cfg.base_url) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    strncpy(task.provider, "auto", sizeof(task.provider) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "resolve should succeed with auto provider");
    CHECK(strcmp(out.provider, "openai") == 0,
          "auto -> main provider 'openai', got '%s'", out.provider);
    CHECK(strcmp(out.model, "gpt-4o") == 0,
          "model should be main 'gpt-4o', got '%s'", out.model);

    return 1;
}

static int test_resolve_explicit_provider(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-main", sizeof(cfg.api_key) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    strncpy(task.provider, "anthropic", sizeof(task.provider) - 1);
    strncpy(task.model, "claude-sonnet-4", sizeof(task.model) - 1);
    strncpy(task.api_key, "sk-ant-test", sizeof(task.api_key) - 1);
    strncpy(task.base_url, "https://api.anthropic.com/v1", sizeof(task.base_url) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "resolve should succeed");
    CHECK(strcmp(out.provider, "anthropic") == 0,
          "explicit provider 'anthropic', got '%s'", out.provider);
    CHECK(strcmp(out.model, "claude-sonnet-4") == 0,
          "explicit model 'claude-sonnet-4', got '%s'", out.model);

    return 1;
}

static int test_resolve_blank_provider(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "deepseek", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "deepseek-chat", sizeof(cfg.model) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "blank provider should fallback to main");
    CHECK(strcmp(out.provider, "deepseek") == 0,
          "blank -> main provider 'deepseek', got '%s'", out.provider);

    return 1;
}

static int test_resolve_task_by_name(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-test", sizeof(cfg.api_key) - 1);

    strncpy(cfg.auxiliary.compression.provider, "anthropic",
            sizeof(cfg.auxiliary.compression.provider) - 1);
    strncpy(cfg.auxiliary.compression.model, "claude-sonnet-4",
            sizeof(cfg.auxiliary.compression.model) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_task(&cfg, "compression", &out);
    CHECK(ok != 0, "resolve compression should succeed");
    CHECK(strcmp(out.provider, "anthropic") == 0,
          "compression provider 'anthropic', got '%s'", out.provider);
    CHECK(strcmp(out.model, "claude-sonnet-4") == 0,
          "compression model 'claude-sonnet-4', got '%s'", out.model);

    return 1;
}

static int test_resolve_null_args(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    llm_config_t out;

    int ok = auxiliary_resolve_llm_config(NULL, &task, &out);
    CHECK(ok == 0, "NULL config should fail");

    ok = auxiliary_resolve_llm_config(&cfg, NULL, &out);
    CHECK(ok == 0, "NULL task should fail");

    ok = auxiliary_resolve_llm_config(&cfg, &task, NULL);
    CHECK(ok == 0, "NULL output should fail");

    ok = auxiliary_resolve_task(NULL, "compression", &out);
    CHECK(ok == 0, "NULL config should fail in resolve_task");

    ok = auxiliary_resolve_task(&cfg, NULL, &out);
    CHECK(ok == 0, "NULL task_name should fail");

    ok = auxiliary_resolve_task(&cfg, "compression", NULL);
    CHECK(ok == 0, "NULL output should fail in resolve_task");

    return 1;
}

static int test_resolve_auto_with_override_model(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "openai", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gpt-4o", sizeof(cfg.model) - 1);
    strncpy(cfg.api_key, "sk-main", sizeof(cfg.api_key) - 1);
    strncpy(cfg.base_url, "https://api.openai.com/v1", sizeof(cfg.base_url) - 1);

    auxiliary_task_config_t task;
    memset(&task, 0, sizeof(task));
    strncpy(task.provider, "auto", sizeof(task.provider) - 1);
    strncpy(task.model, "gpt-4o-mini", sizeof(task.model) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_llm_config(&cfg, &task, &out);
    CHECK(ok != 0, "resolve should succeed");
    CHECK(strcmp(out.provider, "openai") == 0,
          "provider should be main 'openai', got '%s'", out.provider);
    CHECK(strcmp(out.model, "gpt-4o-mini") == 0,
          "model should be task override 'gpt-4o-mini', got '%s'", out.model);

    return 1;
}

static int test_resolve_task_vision_auto(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.provider, "google", sizeof(cfg.provider) - 1);
    strncpy(cfg.model, "gemini-2.5-flash", sizeof(cfg.model) - 1);

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_task(&cfg, "vision", &out);
    CHECK(ok != 0, "resolve vision should succeed");
    CHECK(strcmp(out.provider, "google") == 0,
          "vision (auto) -> main 'google', got '%s'", out.provider);

    return 1;
}

static int test_resolve_task_unknown(void) {
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    llm_config_t out;
    memset(&out, 0, sizeof(out));

    int ok = auxiliary_resolve_task(&cfg, "nonexistent_task", &out);
    CHECK(ok == 0, "unknown task should fail");

    return 1;
}

int main(void) {
    printf("Auxiliary client tests:\n");

    /* Provider normalization */
    TEST(normalize_provider_google_alias);
    TEST(normalize_provider_xai);
    TEST(normalize_provider_claude);
    TEST(normalize_provider_github_copilot);
    TEST(normalize_provider_codex);
    TEST(normalize_provider_custom_prefix);
    TEST(normalize_provider_unknown_passthrough);
    TEST(normalize_provider_null_auto);

    /* Model predicates */
    TEST(is_kimi_model_kimi);
    TEST(is_kimi_model_kimi_prefix);
    TEST(is_kimi_model_not_kimi);
    TEST(is_arcee_trinity_thinking);
    TEST(fixed_temperature_kimi);
    TEST(fixed_temperature_trinity);
    TEST(fixed_temperature_default);
    TEST(compression_threshold_trinity);
    TEST(compression_threshold_default);

    /* Aux model lookup */
    TEST(get_aux_model_gemini);
    TEST(get_aux_model_anthropic);
    TEST(get_aux_model_unknown);
    TEST(vision_model_zai);
    TEST(provider_without_vision);

    /* Header builders */
    TEST(build_or_headers_basic);
    TEST(build_or_headers_cache_enabled);
    TEST(build_nvidia_headers_cloud);
    TEST(build_nvidia_headers_local);
    TEST(build_nous_extra_body);
    TEST(build_codex_headers);

    /* URL helpers */
    TEST(to_openai_base_url_anthropic_suffix);
    TEST(to_openai_base_url_zai);
    TEST(to_openai_base_url_kimi);
    TEST(to_openai_base_url_normal);
    TEST(endpoint_speaks_anthropic);
    TEST(is_anthropic_compat_endpoint);
    TEST(validate_base_url_valid);
    TEST(validate_base_url_invalid);

    /* Error classification */
    TEST(is_payment_error_402);
    TEST(is_payment_error_text);
    TEST(is_rate_limit_429);
    TEST(is_rate_limit_text);
    TEST(is_connection_error);
    TEST(is_auth_error_401);
    TEST(is_auth_error_text);
    TEST(is_model_not_found_404);
    TEST(is_model_not_found_text);
    TEST(is_unsupported_temperature);

    /* Health tracking */
    TEST(health_mark_unhealthy);
    TEST(health_reset);

    /* Vision helpers */
    TEST(main_model_supports_vision);
    TEST(normalize_vision_provider);
    TEST(strict_vision_backend);
    TEST(get_available_vision_backends);

    /* Max tokens */
    TEST(max_tokens_param);

    /* Chain labels */
    TEST(normalize_chain_label);

    /* Task labels */
    TEST(task_label);

    /* Config resolution */
    TEST(resolve_auto_main_provider);
    TEST(resolve_explicit_provider);
    TEST(resolve_blank_provider);
    TEST(resolve_task_by_name);
    TEST(resolve_null_args);
    TEST(resolve_auto_with_override_model);
    TEST(resolve_task_vision_auto);
    TEST(resolve_task_unknown);

    printf("  Results: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
