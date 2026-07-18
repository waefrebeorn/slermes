/*
 * test_skill_provenance.c — Faithful port of tools/skill_provenance.py.
 *
 * Proves the existing lib/libskillusage/skill_provenance.c port works:
 * default "foreground", set/reset token semantics, nesting, is_background_review.
 *
 * Build/run via `make test-skill-provenance`.
 */

#include "skill_provenance.h"
#include <stdio.h>
#include <string.h>

static int passed = 0, failed = 0;
#define TEST(name, expr) do { \
    if (expr) { passed++; printf("  PASS: %s\n", name); } \
    else { failed++; printf("  FAIL: %s (line %d)\n", name, __LINE__); } \
} while (0)
#define TEST_STR_EQ(name, a, b) TEST(name, (a) && (b) && strcmp((a), (b)) == 0)
#define TEST_INT_EQ(name, a, b) TEST(name, (a) == (b))

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Skill Provenance Test Suite ===\n");

    /* Default origin is "foreground" (mirrors Python ContextVar default). */
    TEST_STR_EQ("default origin = foreground",
                 skill_provenance_get(), "foreground");
    TEST_INT_EQ("not background by default",
                 skill_provenance_is_background_review(), 0);

    /* Set to background_review, get a token, check, then reset. */
    skill_provenance_token_t t1 = skill_provenance_set("background_review");
    TEST_STR_EQ("origin now background_review",
                 skill_provenance_get(), "background_review");
    TEST_INT_EQ("is_background_review true",
                 skill_provenance_is_background_review(), 1);

    skill_provenance_reset(t1);
    TEST_STR_EQ("reset restores foreground",
                 skill_provenance_get(), "foreground");
    TEST_INT_EQ("not background after reset",
                 skill_provenance_is_background_review(), 0);

    /* NULL/empty origin defaults to foreground (Python: origin or "foreground"). */
    skill_provenance_token_t t2 = skill_provenance_set(NULL);
    TEST_STR_EQ("null origin -> foreground",
                 skill_provenance_get(), "foreground");
    skill_provenance_reset(t2);

    /* Nesting: inner set overrides, inner reset restores outer, outer reset
     * restores the original default. Mirrors ContextVar nesting. */
    skill_provenance_token_t outer = skill_provenance_set("foreground");
    skill_provenance_token_t inner = skill_provenance_set("background_review");
    TEST_STR_EQ("inner origin = background_review",
                 skill_provenance_get(), "background_review");
    skill_provenance_reset(inner);
    TEST_STR_EQ("after inner reset = outer (foreground)",
                 skill_provenance_get(), "foreground");
    skill_provenance_reset(outer);
    TEST_STR_EQ("after outer reset = foreground (default)",
                 skill_provenance_get(), "foreground");

    /* Arbitrary origin string is preserved. */
    skill_provenance_token_t t3 = skill_provenance_set("assistant_tool");
    TEST_STR_EQ("custom origin preserved",
                 skill_provenance_get(), "assistant_tool");
    skill_provenance_reset(t3);
    TEST_STR_EQ("restored after custom",
                 skill_provenance_get(), "foreground");

    printf("\n%sSKILL-PROVENANCE TESTS: %d passed, %d failed%s\n",
           failed ? "FAIL " : "", passed, failed,
           failed ? "" : " — ALL PASSED");
    return failed ? 1 : 0;
}
