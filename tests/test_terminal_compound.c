/*
 * test_terminal_compound.c — Tests for terminal_rewrite_compound_background() (B07 depth).
 * Tests: chain operator + background rewriting, edge cases.
 * Compile: gcc -O2 -Wall -Wextra -I../include test_terminal_compound.c
 *         ../src/tools/terminal.c -o /tmp/t_compound -lm
 *         -Wl,--unresolved-symbols=ignore-all
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Forward declaration */
char *terminal_rewrite_compound_background(const char *command);

static int failures = 0;
#define TEST(name, cond) do { \
    if (!(cond)) { fprintf(stderr, "  FAIL: %s\n", name); failures++; } \
    else printf("  PASS: %s\n", name); \
} while (0)

int main(void) {
    printf("=== Terminal Compound Background Rewrite Tests ===\n");

    /* Test 1: NULL command */
    {
        char *res = terminal_rewrite_compound_background(NULL);
        TEST("NULL returns empty", res && res[0] == '\0');
        free(res);
    }

    /* Test 2: Empty command */
    {
        char *res = terminal_rewrite_compound_background("");
        TEST("empty returns empty", res && res[0] == '\0');
        free(res);
    }

    /* Test 3: A && B & rewritten to A && { B & } */
    {
        char *res = terminal_rewrite_compound_background("A && B &");
        TEST("A && B & rewritten", res && strstr(res, "B  & }") != NULL);
        TEST("A && preserved", res && strstr(res, "A &&") != NULL);
        TEST("contains brace group", res && strstr(res, "{ ") != NULL);
        free(res);
    }

    /* Test 4: A || B & rewritten similarly */
    {
        char *res = terminal_rewrite_compound_background("A || B &");
        TEST("A || B & rewritten", res && strstr(res, "{ B  & }") != NULL);
        free(res);
    }

    /* Test 5: Simple cmd & left alone (no chain operator) */
    {
        char *res = terminal_rewrite_compound_background("cmd &");
        TEST("simple cmd & unchanged", res && strcmp(res, "cmd &") == 0);
        free(res);
    }

    /* Test 6: No background operator — unchanged */
    {
        char *res = terminal_rewrite_compound_background("A && B");
        TEST("no background unchanged", res && strcmp(res, "A && B") == 0);
        free(res);
    }

    /* Test 7: Deep chain: A && B && C & rewrites only last */
    {
        char *res = terminal_rewrite_compound_background("A && B && C &");
        TEST("deep chain rewritten", res && strstr(res, "{ C  & }") != NULL);
        TEST("deep chain preserves B &&", res && strstr(res, "B &&") != NULL);
        free(res);
    }

    /* Test 8: Redirect with &>: not a background operator */
    {
        char *res = terminal_rewrite_compound_background("cmd &> /dev/null");
        TEST("&> redirect unchanged", res && strcmp(res, "cmd &> /dev/null") == 0);
        free(res);
    }

    /* Test 9: Semicolon resets chain */
    {
        char *res = terminal_rewrite_compound_background("A; sudo B &");
        TEST("semicolon resets chain (no &&/|| before &)", res && strcmp(res, "A; sudo B &") == 0);
        free(res);
    }

    /* Test 10: Pipe resets chain */
    {
        char *res = terminal_rewrite_compound_background("A | sudo B &");
        TEST("pipe resets chain (no &&/|| before &)", res && strcmp(res, "A | sudo B &") == 0);
        free(res);
    }

    /* Test 11: Background with env var prefix */
    {
        char *res = terminal_rewrite_compound_background("KEY=val A && B &");
        TEST("env var + chain + bg rewritten", res != NULL);
        TEST("env var preserved", res && strstr(res, "KEY=val") != NULL);
        TEST("bg wrapped in braces", res && strstr(res, "{ B  & }") != NULL);
        free(res);
    }

    /* Test 12: Background with redirect: A && B > out & */
    {
        char *res = terminal_rewrite_compound_background("A && B > out &");
        TEST("redirect + bg rewritten", res != NULL);
        TEST("redirect preserved", res && strstr(res, "> out") != NULL);
        free(res);
    }

    /* Test 13: Triple chain: A && B || C & */
    {
        char *res = terminal_rewrite_compound_background("A && B || C &");
        TEST("triple chain rewritten", res != NULL);
        TEST("C wrapped in braces", res && strstr(res, "{ C  & }") != NULL);
        free(res);
    }

    /* Test 14: & is part of a token (not background): cmd arg&more */
    {
        char *res = terminal_rewrite_compound_background("echo foo&bar");
        TEST("& in token unchanged", res && strcmp(res, "echo foo&bar") == 0);
        free(res);
    }

    /* Test 15: Single-word command with & background: ls & */
    {
        char *res = terminal_rewrite_compound_background("ls &");
        TEST("simple ls & unchanged", res && strcmp(res, "ls &") == 0);
        free(res);
    }

    /* Summary */
    printf("\n%s\n", failures ? "SOME TESTS FAILED" : "All compound background rewrite tests PASSED");
    return failures ? 1 : 0;
}
