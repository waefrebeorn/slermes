/*
 * model_normalize_test.c — behavioral tests for port_model_normalize.c.
 * Validates against the real Python hermes_cli/model_normalize.py (oracle).
 */
#include "model_normalize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int failures = 0, checks = 0;
#define CK(cond, msg) do { checks++; if(!(cond)){ printf("FAIL: %s\n", msg); failures++; } } while(0)

/* compare C output against python oracle text captured separately */
static void eq_(const char *got, const char *want, const char *label) {
    if (want == NULL) { CK(got == NULL, label); return; }
    CK(got && strcmp(got, want) == 0, label);
    if (!(got && strcmp(got, want) == 0)) printf("    got='%s' want='%s'\n", got ? got : "(null)", want);
}

int main(void) {
    /* ── normalize_model_for_provider (mirrors Python doctests) ── */
    {
        char *r = model_normalize_for_provider("claude-sonnet-4.6", "openrouter");
        eq_(r, "anthropic/claude-sonnet-4.6", "openrouter: prepend vendor"); free(r);
        r = model_normalize_for_provider("anthropic/claude-sonnet-4.6", "anthropic");
        eq_(r, "claude-sonnet-4-6", "anthropic: dots->hyphens"); free(r);
        r = model_normalize_for_provider("anthropic/claude-sonnet-4.6", "copilot");
        eq_(r, "claude-sonnet-4.6", "copilot: copilot resolver strips vendor/"); free(r);
        r = model_normalize_for_provider("openai/gpt-5.4", "copilot");
        eq_(r, "gpt-5.4", "copilot: openai/ stripped"); free(r);
        r = model_normalize_for_provider("claude-sonnet-4.6", "opencode-zen");
        eq_(r, "claude-sonnet-4-6", "opencode-zen: claude dots->hyphens"); free(r);
        r = model_normalize_for_provider("minimax-m2.5-free", "opencode-zen");
        eq_(r, "minimax-m2.5-free", "opencode-zen: minimax dots kept"); free(r);
        r = model_normalize_for_provider("deepseek-v3", "deepseek");
        eq_(r, "deepseek-v3", "deepseek: v3 kept (V-series, not folded to chat)"); free(r);
        r = model_normalize_for_provider("deepseek-r1", "deepseek");
        eq_(r, "deepseek-reasoner", "deepseek: r1 -> reasoner"); free(r);
        r = model_normalize_for_provider("my-model", "custom");
        eq_(r, "my-model", "custom: passthrough"); free(r);
        r = model_normalize_for_provider("claude-sonnet-4.6", "zai");
        eq_(r, "claude-sonnet-4.6", "zai: strip matching prefix (none here)"); free(r);
        r = model_normalize_for_provider("MiMo-V2.5-Pro", "xiaomi");
        eq_(r, "mimo-v2.5-pro", "xiaomi: lowercase model id"); free(r);
        /* V-series first-class + dated variant must NOT fold to chat */
        r = model_normalize_for_provider("deepseek-v4-pro", "deepseek");
        eq_(r, "deepseek-v4-pro", "deepseek: v4-pro kept"); free(r);
        r = model_normalize_for_provider("deepseek/deepseek-v4-flash", "deepseek");
        eq_(r, "deepseek-v4-flash", "deepseek: slash+dated kept"); free(r);
    }

    /* ── detect_vendor ── */
    {
        char *v = model_normalize_detect_vendor("claude-sonnet-4.6");
        eq_(v, "anthropic", "detect claude"); free(v);
        v = model_normalize_detect_vendor("gpt-5.4-mini");
        eq_(v, "openai", "detect gpt"); free(v);
        v = model_normalize_detect_vendor("anthropic/claude-sonnet-4.6");
        eq_(v, "anthropic", "detect prefix"); free(v);
        v = model_normalize_detect_vendor("qwen3.5-plus");
        eq_(v, "qwen", "detect qwen3.5 -> qwen (prefix startswith)"); free(v);
        v = model_normalize_detect_vendor("my-custom-model");
        CK(v == NULL, "detect custom -> NULL"); free(v);
    }

    /* ── strip / dots / prepend helpers ── */
    {
        char *s = model_normalize_strip_vendor("anthropic/claude-sonnet-4.6");
        eq_(s, "claude-sonnet-4.6", "strip vendor"); free(s);
        s = model_normalize_dots_to_hyphens("claude-sonnet-4.6");
        eq_(s, "claude-sonnet-4-6", "dots->hyphens"); free(s);
        s = model_normalize_prepend_vendor("claude-sonnet-4.6");
        eq_(s, "anthropic/claude-sonnet-4.6", "prepend vendor"); free(s);
        s = model_normalize_prepend_vendor("anthropic/claude-sonnet-4.6");
        eq_(s, "anthropic/claude-sonnet-4.6", "prepend: already prefixed"); free(s);
    }

    printf("model_normalize_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
