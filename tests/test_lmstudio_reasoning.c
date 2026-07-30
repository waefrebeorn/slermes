/*
 * test_lmstudio_reasoning.c — Tests for LM Studio reasoning-effort resolution.
 *
 * Tests: valid effort check, alias mapping, full resolution with/without
 * enabled, edge cases for allowed_options clamping.
 */

#include "lmstudio_reasoning.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0, failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while(0)

#define TEST_STR_EQ(name, a, b) do { \
    const char *_a = (a); const char *_b = (b); \
    if (_a && _b && strcmp(_a, _b) == 0) { passed++; printf("  PASS: %s\n", name); } \
    else { \
        failed++; \
        printf("  FAIL: %s (line %d) — got \"%s\", expected \"%s\"\n", \
               name, __LINE__, _a ? _a : "(null)", _b ? _b : "(null)"); \
    } \
} while(0)

static void test_valid_effort(void) {
    printf("\n--- Valid effort check ---\n");
    TEST("NULL returns false", !lmstudio_is_valid_effort(NULL));
    TEST("empty returns false", !lmstudio_is_valid_effort(""));
    TEST("none is valid", lmstudio_is_valid_effort("none"));
    TEST("minimal is valid", lmstudio_is_valid_effort("minimal"));
    TEST("low is valid", lmstudio_is_valid_effort("low"));
    TEST("medium is valid", lmstudio_is_valid_effort("medium"));
    TEST("high is valid", lmstudio_is_valid_effort("high"));
    TEST("xhigh is valid", lmstudio_is_valid_effort("xhigh"));
    TEST("invalid effort returns false", !lmstudio_is_valid_effort("extreme"));
    TEST("case sensitive", !lmstudio_is_valid_effort("HIGH"));
}

static void test_alias_mapping(void) {
    printf("\n--- Alias mapping ---\n");
    TEST_STR_EQ("off -> none", lmstudio_map_effort_alias("off"), "none");
    TEST_STR_EQ("on -> medium", lmstudio_map_effort_alias("on"), "medium");
    TEST("NULL returns NULL", lmstudio_map_effort_alias(NULL) == NULL);
    TEST_STR_EQ("empty string returns empty", lmstudio_map_effort_alias(""), "");
    TEST_STR_EQ("low unchanged", lmstudio_map_effort_alias("low"), "low");
    TEST_STR_EQ("high unchanged", lmstudio_map_effort_alias("high"), "high");
    TEST_STR_EQ("unknown alias unchanged",
        lmstudio_map_effort_alias("turbo"), "turbo");
}

static void test_resolve_disabled(void) {
    printf("\n--- Reasoning disabled ---\n");
    /* Disabled should always resolve to "none" */
    lmstudio_reasoning_config_t cfg = { .enabled = 0, .effort = NULL, .allowed_options = NULL };
    TEST_STR_EQ("disabled, NULL config -> medium (default)",
        resolve_lmstudio_effort(NULL, NULL), "medium");
    TEST_STR_EQ("disabled, empty effort -> none",
        resolve_lmstudio_effort(&cfg, NULL), "none");

    /* Even with allowed_options, disabled = none */
    const char *opts[] = {"off", "on", NULL};
    TEST_STR_EQ("disabled with allowed opts",
        resolve_lmstudio_effort(&cfg, opts), "none");
}

static void test_resolve_enabled(void) {
    printf("\n--- Reasoning enabled ---\n");
    /* Enabled with NULL config -> default "medium" */
    TEST_STR_EQ("NULL config -> medium",
        resolve_lmstudio_effort(NULL, NULL), "medium");

    lmstudio_reasoning_config_t cfg = { .enabled = 1, .effort = NULL, .allowed_options = NULL };
    TEST_STR_EQ("enabled, NULL effort -> medium",
        resolve_lmstudio_effort(&cfg, NULL), "medium");
    TEST_STR_EQ("enabled, empty effort -> medium",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "", NULL, NULL}), "medium");

    /* Enabled with specific effort */
    TEST_STR_EQ("enabled, low -> low",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "low", NULL, NULL}, NULL), "low");
    TEST_STR_EQ("enabled, high -> high",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "high", NULL, NULL}, NULL), "high");

    /* Alias mapping */
    TEST_STR_EQ("enabled, off -> none",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "off", NULL, NULL}, NULL), "none");
    TEST_STR_EQ("enabled, on -> medium",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "on", NULL, NULL}, NULL), "medium");

    /* Invalid effort - should use default "medium" */
    TEST_STR_EQ("enabled, invalid effort -> medium",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "turbo", NULL, NULL}, NULL), "medium");
}

static void test_allowed_options_clamping(void) {
    printf("\n--- Allowed options clamping ---\n");
    const char *opts_valid[] = {"off", "on", NULL};

    /* Resolved effort matches an allowed option */
    TEST_STR_EQ("none in {off,on}",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "off", NULL, NULL}, opts_valid), "none");
    TEST_STR_EQ("medium (from on) in {off,on}",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "on", NULL, NULL}, opts_valid), "medium");

    /* Resolved effort NOT in allowed options -> returns NULL */
    const char *opts_limited[] = {"off", NULL};
    TEST("high not in {off} -> NULL",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "high", NULL, NULL}, opts_limited) == NULL);
    TEST("medium not in {off} -> NULL",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "medium", NULL, NULL}, opts_limited) == NULL);

    /* Allowed options with aliases: "on" maps to "medium" */
    const char *opts_with_alias[] = {"on", NULL};
    TEST("medium matches 'on' alias",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "medium", NULL, NULL}, opts_with_alias) != NULL);

    /* High not in {on} even after alias mapping */
    TEST("high not in {on} -> NULL",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "high", NULL, NULL}, opts_with_alias) == NULL);
}

static void test_case_insensitivity(void) {
    printf("\n--- Case insensitivity ---\n");
    TEST_STR_EQ("uppercase OFF -> none",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "OFF", NULL, NULL}, NULL), "none");
    TEST_STR_EQ("mixed Medium -> medium",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "Medium", NULL, NULL}, NULL), "medium");
    TEST_STR_EQ("uppercase ON -> medium",
        resolve_lmstudio_effort(&(lmstudio_reasoning_config_t){1, "ON", NULL, NULL}, NULL), "medium");
}

int main(void) {
    printf("=== LM Studio Reasoning Tests ===\n");

    test_valid_effort();
    test_alias_mapping();
    test_resolve_disabled();
    test_resolve_enabled();
    test_allowed_options_clamping();
    test_case_insensitivity();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}