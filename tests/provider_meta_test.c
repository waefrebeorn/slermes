/*
 * provider_meta_test.c — real behavioral test for port_provider_meta.c
 *
 * Asserts against the documented semantics of hermes_cli/models.py:
 *   normalize_provider (alias -> canonical, "auto" passthrough, default openrouter)
 *   provider_label      (slug/alias -> human label, "auto" -> "Auto")
 *   provider_group_for_slug (member -> group_id, ungrouped -> "")
 *   group_providers     (fold members into group rows at first position,
 *                         single-member groups degrade to single rows,
 *                         order + dedupe preserved)
 */

#include "provider_meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int checks = 0, failures = 0;

#define CHECK(cond, msg) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } } while (0)
#define EQ_STR(a, b, msg) do { checks++; const char *_a=(a), *_b=(b); \
    if (!_a || !_b || strcmp(_a,_b)!=0) { failures++; printf("FAIL: %s\n  got=%s\n  exp=%s\n", msg, _a?_a:"(null)", _b?_b:"(null)"); } } while (0)

static void test_normalize(void) {
    EQ_STR(normalize_provider("GitHub"), "copilot", "github -> copilot");
    EQ_STR(normalize_provider("claude"), "anthropic", "claude -> anthropic");
    EQ_STR(normalize_provider("grok"), "xai", "grok -> xai");
    EQ_STR(normalize_provider("Google AI Studio"), "gemini", "google-ai-studio -> gemini");
    EQ_STR(normalize_provider("auto"), "auto", "auto passes through");
    EQ_STR(normalize_provider(NULL), "openrouter", "NULL -> openrouter default");
    EQ_STR(normalize_provider("anthropic"), "anthropic", "canonical unchanged");
    EQ_STR(normalize_provider("OPENAI-API"), "openai-api", "case-insensitive + trim");
}

static void test_label(void) {
    char *l = provider_label("github");
    EQ_STR(l, "GitHub Copilot", "github label");
    free(l);
    l = provider_label("claude");
    EQ_STR(l, "Anthropic", "claude label");
    free(l);
    l = provider_label("auto");
    EQ_STR(l, "Auto", "auto -> Auto");
    free(l);
    l = provider_label("xai");
    EQ_STR(l, "xAI", "xai label");
    free(l);
    l = provider_label("custom");
    EQ_STR(l, "Custom endpoint", "custom label");
    free(l);
    l = provider_label("totally-unknown");
    EQ_STR(l, "totally-unknown", "unknown falls back to original");
    free(l);
    l = provider_label(NULL);
    EQ_STR(l, "OpenRouter", "NULL -> OpenRouter");
    free(l);
}

static void test_group_for_slug(void) {
    EQ_STR(provider_group_for_slug("kimi-coding"), "kimi", "kimi-coding -> kimi");
    EQ_STR(provider_group_for_slug("xai-oauth"), "xai", "xai-oauth -> xai");
    EQ_STR(provider_group_for_slug("gemini"), "google", "gemini -> google");
    EQ_STR(provider_group_for_slug("openrouter"), "", "openrouter ungrouped");
    EQ_STR(provider_group_for_slug(""), "", "empty ungrouped");
    EQ_STR(provider_group_for_slug(NULL), "", "null ungrouped");
}

static void test_group_providers(void) {
    /* single ungrouped */
    const char *one[] = { "openrouter", NULL };
    provider_row_t *rows = group_providers(one);
    CHECK(rows && rows->kind == PROVIDER_ROW_SINGLE, "openrouter single row");
    EQ_STR(rows->slug, "openrouter", "openrouter slug");
    provider_group_rows_free(rows);

    /* a full group folds into one GROUP row at first member position */
    const char *xai_pair[] = { "xai", "xai-oauth", NULL };
    rows = group_providers(xai_pair);
    CHECK(rows != NULL, "xai group non-empty");
    CHECK(rows->kind == PROVIDER_ROW_GROUP, "xai is GROUP row");
    EQ_STR(rows->group_id, "xai", "xai group_id");
    EQ_STR(rows->label, "xAI Grok", "xai label");
    CHECK(rows->n_members == 2, "xai has 2 members");
    CHECK(rows->next == NULL, "xai group is sole row");
    provider_group_rows_free(rows);

    /* mixed: group members interleaved with ungrouped; group appears at
     * first present member's position. */
    const char *mixed[] = { "openrouter", "kimi-coding", "kimi-coding-cn", "deepseek", NULL };
    rows = group_providers(mixed);
    /* expect order: openrouter (single), kimi (group), deepseek (single) */
    provider_row_t *r = rows;
    CHECK(r && r->kind == PROVIDER_ROW_SINGLE && strcmp(r->slug, "openrouter") == 0, "row1 openrouter");
    r = r->next;
    CHECK(r && r->kind == PROVIDER_ROW_GROUP && strcmp(r->group_id, "kimi") == 0, "row2 kimi group");
    CHECK(r && r->n_members == 2, "kimi group 2 members");
    r = r->next;
    CHECK(r && r->kind == PROVIDER_ROW_SINGLE && strcmp(r->slug, "deepseek") == 0, "row3 deepseek");
    CHECK(r->next == NULL, "end of list");
    provider_group_rows_free(rows);

    /* single-member group degrades to SINGLE row */
    const char *google_only[] = { "gemini", NULL };
    rows = group_providers(google_only);
    CHECK(rows && rows->kind == PROVIDER_ROW_SINGLE, "gemini degrades to single");
    EQ_STR(rows->slug, "gemini", "gemini single slug");
    provider_group_rows_free(rows);

    /* dedupe */
    const char *dup[] = { "xai", "xai", "xai-oauth", NULL };
    rows = group_providers(dup);
    CHECK(rows && rows->kind == PROVIDER_ROW_GROUP && rows->next == NULL, "xai dedupe -> 1 row");
    provider_group_rows_free(rows);

    /* empty input */
    const char *empty[] = { NULL };
    rows = group_providers(empty);
    CHECK(rows == NULL, "empty -> NULL");
    provider_group_rows_free(rows);
}

int main(void) {
    test_normalize();
    test_label();
    test_group_for_slug();
    test_group_providers();
    printf("provider_meta_test: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
