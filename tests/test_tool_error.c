/*
 * test_tool_error.c — Tests for tool_error() / tool_result_obj() helpers.
 * Port of Python tools/registry.py tool_error() and tool_result().
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hermes_tool_result.h"

static int pass = 0, fail = 0;

#define TEST(n) do { printf("  TEST: %s\n", n); } while(0)
#define PASS() do { printf("    PASS\n"); pass++; } while(0)
#define FAIL(msg) do { printf("    FAIL: %s\n", msg); fail++; } while(0)

static void test_basic_error(void) {
    TEST("tool_error: basic error string");
    char *r = tool_error("file not found", NULL);
    if (r && strcmp(r, "{\"error\":\"file not found\"}") == 0) { PASS(); }
    else { FAIL(r); }
    free(r);
}

static void test_error_with_extras(void) {
    TEST("tool_error: with extra key=value pairs");
    char *r = tool_error("bad input", "path", "/tmp/x", "code", "42", NULL);
    if (r && strstr(r, "\"error\":\"bad input\"") &&
        strstr(r, "\"path\":\"/tmp/x\"") &&
        strstr(r, "\"code\":\"42\"")) { PASS(); }
    else { FAIL(r); }
    free(r);
}

static void test_bare_result(void) {
    TEST("tool_result_obj: bare success");
    char *r = tool_result_obj(NULL);
    if (r && strcmp(r, "{\"success\":true}") == 0) { PASS(); }
    else { FAIL(r); }
    free(r);
}

static void test_result_with_fields(void) {
    TEST("tool_result_obj: with key=value fields");
    char *r = tool_result_obj("path", "/tmp/x", "n", "42", NULL);
    if (r && strstr(r, "\"path\":\"/tmp/x\"") &&
        strstr(r, "\"n\":\"42\"")) { PASS(); }
    else { FAIL(r); }
    free(r);
}

static void test_null_message(void) {
    TEST("tool_error: NULL message falls back to 'unknown error'");
    char *r = tool_error(NULL, NULL);
    if (r && strstr(r, "\"error\"")) { PASS(); }
    else { FAIL(r); }
    free(r);
}

int main(void) {
    printf("=== Tool Error/Result Helper Tests ===\n\n");
    test_basic_error();
    test_error_with_extras();
    test_bare_result();
    test_result_with_fields();
    test_null_message();
    printf("\nResults: %d passed, %d failed\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
