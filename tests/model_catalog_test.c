/*
 * model_catalog_test.c — real-behavior test for model_catalog.c.
 * Pure (no network): exercises the static catalog, normalization, grouping,
 * model-name parsing, provider detection, openrouter slug, fast-mode, cache.
 */
#include "model_catalog.h"
#include "port_models_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } else { g_fail++; printf("FAIL: %s\n", msg); } } while (0)

int main(void) {
    /* ── normalize_provider ──────────────────────────────────────── */
    CHECK(strcmp(model_normalize_provider("GLM"), "zai") == 0, "glm -> zai");
    CHECK(strcmp(model_normalize_provider("claude"), "anthropic") == 0, "claude -> anthropic");
    CHECK(strcmp(model_normalize_provider("auto"), "auto") == 0, "auto passes through");
    CHECK(strcmp(model_normalize_provider(""), "openrouter") == 0, "empty -> openrouter");
    CHECK(strcmp(model_normalize_provider("openrouter"), "openrouter") == 0, "openrouter stays");
    CHECK(strcmp(model_normalize_provider("github-copilot"), "copilot") == 0, "github-copilot -> copilot");

    /* ── provider_label ──────────────────────────────────────────── */
    CHECK(strcmp(model_provider_label("nous"), "Nous Portal") == 0, "nous label");
    CHECK(strcmp(model_provider_label("GLM"), "Z-AI") == 0, "glm label via alias");

    /* ── provider group ──────────────────────────────────────────── */
    CHECK(strcmp(model_provider_group_for_slug("kimi-coding"), "kimi") == 0, "kimi-coding group");
    CHECK(strcmp(model_provider_group_for_slug("openai-codex"), "openai") == 0, "openai-codex group");
    CHECK(strcmp(model_provider_group_for_slug("nous"), "") == 0, "nous ungrouped");

    /* ── default model (cost-safe) ───────────────────────────────── */
    CHECK(strcmp(model_default_model_for_provider("openai"), "gpt-5.4") == 0, "openai default");
    CHECK(strcmp(model_default_model_for_provider("nous"), "deepseek/deepseek-v4-flash") == 0,
          "nous silent default is low-cost, not flagship");
    CHECK(strcmp(model_default_model_for_provider("anthropic"), "claude-fable-5") == 0, "anthropic default");

    /* ── provider model counts ───────────────────────────────────── */
    CHECK(model_provider_model_count("nous") == 26, "nous has 26 curated models");
    CHECK(model_provider_model_count("nonexistent") == 0, "unknown provider -> 0");
    const char *m0 = model_provider_model_at("nous", 0);
    CHECK(m0 && strcmp(m0, "anthropic/claude-fable-5") == 0, "nous[0] is claude-fable-5");
    CHECK(model_provider_model_at("nous", 999) == NULL, "out-of-range -> NULL");

    /* ── provider count ──────────────────────────────────────────── */
    CHECK(model_catalog_provider_count() == 34, "34 providers in catalog");

    /* ── strip vendor prefix ─────────────────────────────────────── */
    char buf[256];
    model_strip_vendor_prefix("anthropic/claude-opus-4-6", buf, sizeof(buf));
    CHECK(strcmp(buf, "claude-opus-4-6") == 0, "strip vendor prefix");
    model_strip_vendor_prefix("claude-opus-4.6:fast", buf, sizeof(buf));
    CHECK(strcmp(buf, "claude-opus-4.6") == 0, "strip :variant suffix");

    /* ── parse model input ───────────────────────────────────────── */
    char pv[128], mv[256];
    model_parse_model_input("nous:hermes-3", "openrouter", pv, sizeof(pv), mv, sizeof(mv));
    CHECK(strcmp(pv, "nous") == 0 && strcmp(mv, "hermes-3") == 0, "provider:model parse");
    model_parse_model_input("gpt-5.4", "openrouter", pv, sizeof(pv), mv, sizeof(mv));
    CHECK(strcmp(pv, "openrouter") == 0 && strcmp(mv, "gpt-5.4") == 0, "bare model keeps current provider");
    model_parse_model_input("custom:local:qwen", "openrouter", pv, sizeof(pv), mv, sizeof(mv));
    CHECK(strcmp(pv, "custom:local") == 0 && strcmp(mv, "qwen") == 0, "custom:name:model triple");
    /* model name with colon that isn't a provider -> treated as model */
    model_parse_model_input("anthropic/claude-3.5-sonnet:beta", "openrouter", pv, sizeof(pv), mv, sizeof(mv));
    CHECK(strcmp(pv, "openrouter") == 0 && strcmp(mv, "anthropic/claude-3.5-sonnet:beta") == 0,
          "colon-in-model-name not split");

    /* ── detect static provider for model ────────────────────────── */
    CHECK(model_detect_static_provider_for_model("gpt-5.4", "openrouter", pv, sizeof(pv), mv, sizeof(mv)) == 1,
          "gpt-5.4 detects openai");
    CHECK(strcmp(pv, "openai") == 0, "detected provider is openai");
    CHECK(model_detect_static_provider_for_model("not-a-real-model", "openrouter", pv, sizeof(pv), mv, sizeof(mv)) == 0,
          "unknown model -> no match");

    /* ── openrouter slug ─────────────────────────────────────────── */
    char *slug = model_find_openrouter_slug("claude-opus-4.8");
    CHECK(slug && strcmp(slug, "anthropic/claude-opus-4.8") == 0, "OR slug by bare name");
    free(slug);
    slug = model_find_openrouter_slug("anthropic/claude-fable-5");
    CHECK(slug && strcmp(slug, "anthropic/claude-fable-5") == 0, "OR slug exact");
    free(slug);
    slug = model_find_openrouter_slug("totally-unknown");
    CHECK(slug == NULL, "OR slug unknown -> NULL");

    /* ── fast mode ──────────────────────────────────────────────── */
    CHECK(model_supports_fast_mode("anthropic/claude-opus-4-6") == 1, "opus-4-6 fast");
    CHECK(model_supports_fast_mode("anthropic/claude-opus-4-7") == 0, "opus-4-7 not fast param");
    CHECK(is_openai_fast_model("gpt-5.4") == 1, "gpt-5.4 openai fast");
    CHECK(is_openai_fast_model("gpt-5.3-codex") == 0, "codex excluded from fast");

    /* ── group rows (display grouping) ──────────────────────────── */
    char *packed = model_group_providers("nous;openai-codex;openai-api;kimi-coding;gemini;openai");
    CHECK(packed != NULL, "group packing succeeds");
    if (packed) {
        const char *cur = NULL, *kind, *slug2, *gid, *label, *desc, *mem;
        int singles = 0, groups = 0;
        while (model_group_next(packed, &cur, &kind, &slug2, &gid, &label, &desc, &mem)) {
            if (strcmp(kind, "single") == 0) singles++;
            else if (strcmp(kind, "group") == 0) { groups++; CHECK(strcmp(gid, "openai")==0, "openai group formed (both members present)"); }
        }
        CHECK(groups == 1, "exactly one group (openai) when both members present");
        /* kimi-coding alone -> single (only 1 of 2 kimi members present) */
        CHECK(singles >= 3, "nous + gemini + kimi-coding singles (ungrouped/degraded)");
        free(packed);
    }

    /* ── disk cache round-trip ──────────────────────────────────── */
    setenv("HERMES_HOME", "/tmp/model_catalog_test_home", 1);
    char *cpath = model_provider_models_cache_path();
    CHECK(strstr(cpath, "provider_models_cache.json") != NULL, "cache path has filename");
    free(cpath);
    char *ids = model_cached_provider_model_ids("openai", 0);
    CHECK(ids && ids[0] == '[' && strstr(ids, "gpt-5.4"), "cached openai ids returns static catalog");
    free(ids);
    /* second call should hit cache (no crash, still valid) */
    ids = model_cached_provider_model_ids("openai", 0);
    CHECK(ids && strstr(ids, "gpt-5.4"), "cached openai ids (2nd call)");
    model_clear_provider_models_cache("openai");
    model_clear_provider_models_cache(NULL);
    free(ids);

    /* ── provider_model_ids degrades to static ──────────────────── */
    char *pm = model_provider_model_ids("nous", 0);
    CHECK(pm && strstr(pm, "anthropic/claude-fable-5"), "provider_model_ids returns static nous");
    free(pm);

    /* ── parse_model_input (mirrors models.py:parse_model_input) ──── */
    {
        char prov[64], mod[256];
        model_parse_model_input("openrouter:anthropic/claude-sonnet-4.5", "nous", prov, sizeof(prov), mod, sizeof(mod));
        CHECK(strcmp(prov, "openrouter") == 0, "explicit provider switch");
        CHECK(strcmp(mod, "anthropic/claude-sonnet-4.5") == 0, "explicit model kept");

        model_parse_model_input("anthropic/claude-sonnet-4.5", "nous", prov, sizeof(prov), mod, sizeof(mod));
        CHECK(strcmp(prov, "nous") == 0, "no provider -> current_provider");
        CHECK(strcmp(mod, "anthropic/claude-sonnet-4.5") == 0, "model kept when no provider");

        model_parse_model_input("gpt-5.4", "openai", prov, sizeof(prov), mod, sizeof(mod));
        CHECK(strcmp(prov, "openai") == 0, "bare model -> current_provider");

        /* colon is NOT a delimiter when left side isn't a known provider */
        model_parse_model_input("anthropic/claude-3.5-sonnet:beta", "nous", prov, sizeof(prov), mod, sizeof(mod));
        CHECK(strcmp(prov, "nous") == 0, "model-with-colon not split");
        CHECK(strcmp(mod, "anthropic/claude-3.5-sonnet:beta") == 0, "full model with colon kept");

        /* custom triple syntax */
        model_parse_model_input("custom:local:qwen", "nous", prov, sizeof(prov), mod, sizeof(mod));
        CHECK(strcmp(prov, "custom:local") == 0, "custom triple provider");
        CHECK(strcmp(mod, "qwen") == 0, "custom triple model");
    }

    /* ── curated_models_for_provider (static-catalog fallback) ────── */
    {
        char prov[16][64]; char mods[16][256];
        int n = model_curated_models_for_provider("nous", prov, mods, 16);
        CHECK(n > 0, "nous has curated models");
        int found_fable = 0;
        for (int i = 0; i < n; i++) if (strcmp(mods[i], "anthropic/claude-fable-5") == 0) found_fable = 1;
        CHECK(found_fable, "nous curated list includes claude-fable-5");
        int n2 = model_curated_models_for_provider("unknownproviderxyz", prov, mods, 16);
        CHECK(n2 == 0, "unknown provider -> 0 models");
    }

    printf("\nmodel_catalog_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
